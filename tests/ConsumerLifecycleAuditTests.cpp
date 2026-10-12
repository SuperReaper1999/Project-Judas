#include "Application.h"
#include "AssetDatabase.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SaveService.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "ScriptSystem.h"
#include "SimulationTiming.h"
#include "WorldPersistence.h"
#include "../third_party/nlohmann/json.hpp"

#include <SDL2/SDL.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <thread>

// An ordinary Application/QuickJS consumer, not a direct native-dispatch test.
// Only the detached invalid save fixture is edited by the auditor. Game commands
// below enter through project JS and the normal queued outer-boundary services.
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
int checks = 0, failures = 0;
Json results = Json::array(), observations = Json::object();

void Check(bool ok, const std::string& id, const std::string& label) {
    ++checks; failures += !ok;
    results.push_back({{"id", id}, {"passed", ok}, {"label", label}});
    std::cout << (ok ? "PASS " : "FAIL ") << id << " " << label << '\n';
    std::cout.flush();
}
void Quit() { SDL_Event event{}; event.type = SDL_QUIT; SDL_PushEvent(&event); }
bool Write(const fs::path& path, const std::string& bytes) {
    std::ofstream file(path, std::ios::binary); file << bytes; return bool(file);
}
std::string Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), {});
}
std::string StringField(const Json& value, const char* key) {
    if (!value.is_object() || !value.contains(key) || !value[key].is_string()) return {};
    return value[key].get<std::string>();
}
EntityId InstanceId(const Json& value, const char* key) {
    const auto text = StringField(value, key);
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos) return 0;
    try { return std::stoull(text); } catch (...) { return 0; }
}
Json Vec(glm::vec3 value) { return {{"x", value.x}, {"y", value.y}, {"z", value.z}}; }
Json State(RuntimeWorld& world, EntityId id) {
    if (world.Scripts()) for (const auto& record : world.Scripts()->Capture())
        if (record.entity == id && record.slot == 1) return Json::parse(record.json);
    return Json::object();
}
Json Physical(RuntimeWorld& world, EntityId id) {
    const auto body = world.RuntimeBody(id);
    if (!body.IsValid()) return {{"valid", false}};
    const auto pose = world.Physics().GetTransform(body);
    return {{"valid", true}, {"position", Vec(pose.position)},
        {"rotation", {{"w", pose.rotation.w}, {"x", pose.rotation.x}, {"y", pose.rotation.y}, {"z", pose.rotation.z}}},
        {"velocity", Vec(world.Physics().GetLinearVelocity(body))},
        {"angularVelocity", Vec(world.Physics().GetAngularVelocity(body))}};
}
Json WorldState(RuntimeWorld& world) {
    Json bodies = Json::object();
    for (const auto id : world.DefinitionIds()) if (world.RuntimeBody(id).IsValid())
        bodies[std::to_string(id)] = Physical(world, id);
    return {{"entities", world.DefinitionIds()}, {"bodies", bodies},
        {"bodyCount", world.Physics().AliveBodyCount()},
        {"scriptCount", world.Scripts() ? world.Scripts()->Capture().size() : 0},
        {"simulationTime", world.SimulationTimeSeconds()}};
}
Json Status(const SaveRequestStatus& status) {
    return {{"id", status.id}, {"operation", status.operation}, {"slot", status.slot},
        {"state", status.state}, {"error", status.error}, {"recovered", status.recovered},
        {"captureMs", status.captureMs}, {"workerMs", status.workerMs}, {"restoreMs", status.restoreMs}};
}

const char* itemScript = R"JS(
import {world} from 'judas';
export const properties = {
  kind: {type:'string', default:'child'},
  label: {type:'string', default:'authored'},
  external: {type:'entity', default:null},
  internal: {type:'entity', default:null}
};
export default class {
  constructor({entity,properties,initialState,restored}) {
    this.entity=entity; this.properties=properties;
    this.restoredConstruction=restored; this.initialWasNull=initialState===null;
    this.state={kind:properties.kind,label:properties.label,payload:initialState,
      parent:entity.parent?.id??null,external:properties.external?.id??null,
      internal:properties.internal?.id??null,started:0,restores:0,updates:0};
    if(properties.kind==='root') {
      this.state.constructedVelocity=entity.velocity;
      this.state.constructedAngularVelocity=entity.angularVelocity;
    }
  }
  start(){this.state.started++;}
  restore(){
    this.state.restores++;
    this.state.restoreSawRestored=this.restoredConstruction;
    this.state.restoreInitialNull=this.initialWasNull;
    this.state.restoredParent=this.entity.parent?.id??null;
    this.state.restoredExternal=this.properties.external?.valid===true;
    this.state.restoredInternal=this.properties.internal?.valid===true;
  }
  update(){this.state.updates++;}
}
)JS";

// PREFAB_ASSET is replaced with the tracked immutable asset identity, not a
// hidden engine fixture or native object name. Authored scene IDs are content.
const char* driverScript = R"JS(
import {world,session,saves} from 'judas';
const asset='PREFAB_ASSET';
export default class {
  constructor({restored}) {
    this.restoredConstruction=restored;this.pending=0;
    this.state={started:0,restores:0,updates:0,score:37,spawned:false};
  }
  start(){this.state.started++;session.set('audit.marker','saved-marker');}
  restore(){
    this.state.restores++;this.pending=0;
    this.state.restoreSawRestored=this.restoredConstruction;
    const recovered=saves.resolve(this.state.sentinelReference);
    this.state.resolvedReference=recovered?.id??null;
    this.state.resolvedValid=recovered?.valid===true;
  }
  update(){
    this.state.updates++;
    if(this.pending)this.state.lastStatus=saves.status(this.pending);
    const command=session.get('audit.command');
    if(!command)return;session.delete('audit.command');
    if(command==='spawn') {
      const spawn=(serial,x)=>world.spawnPrefab(asset,{position:{x,y:4,z:0}},{
        velocity:{x:serial,y:0,z:0},angularVelocity:{x:0,y:serial*.25,z:0},
        scripts:[
          {slot:1,properties:{kind:'root',label:`Instance ${serial}`,external:{entity:'3'}},state:{serial,phase:'initial'}},
          {source:'2',slot:1,state:{serial,phase:'child'}}
        ]});
      const a=spawn(1,12),b=spawn(2,20);
      this.state.instances=[{root:a.id,child:a.children[0]?.id??null},{root:b.id,child:b.children[0]?.id??null}];
      try {
        world.spawnPrefab(asset,{}, {scripts:[{slot:1,state:{serial:999}},{source:'2',slot:99,state:{}}]});
        this.state.invalidSlot={typeError:false,message:'accepted'};
      } catch(error) {this.state.invalidSlot={typeError:error instanceof TypeError,message:String(error)};}
      const stale=world.entity('4');stale.destroy();this.state.staleValid=stale.valid;
      this.state.staleReference=saves.reference(stale);
      try {
        world.spawnPrefab(asset,{}, {scripts:[{slot:1,properties:{kind:'root',external:{entity:stale.id}},state:{}}]});
        this.state.invalidReference={typeError:false,message:'accepted'};
      } catch(error) {this.state.invalidReference={typeError:error instanceof TypeError,message:String(error)};}
      this.state.spawned=true;
    } else if(command==='save') {
      this.state.sentinelReference=saves.reference(world.entity('2'));
      this.pending=saves.save('valid',{name:'Public lifecycle audit',metadata:{audit:'M78 continuation'}});
      this.state.request=this.pending;
    } else if(command==='reject') {
      const target=world.entity('2');
      target.transform={position:{x:7,y:9,z:-3},rotation:{w:1,x:0,y:0,z:0}};
      target.velocity={x:-4,y:2,z:3};target.angularVelocity={x:.25,y:-.5,z:.75};
      this.state.score=91;session.set('audit.marker','live-negative-control');
      this.pending=saves.load('invalid');this.state.request=this.pending;
    } else if(command==='load') {
      this.pending=saves.load('valid');this.state.request=this.pending;
    }
  }
}
)JS";
}

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: " << argv[0] << " <fresh-output-dir>\n"; return 2; }
    const auto out = fs::absolute(argv[1]), root = out / "project";
    if (fs::exists(root)) { std::cerr << "Use a fresh result directory\n"; return 2; }
    fs::create_directories(out);
#ifdef _WIN32
    _putenv_s("LOCALAPPDATA", (out / "userdata").string().c_str());
    _putenv_s("APPDATA", (out / "userdata").string().c_str());
#else
    setenv("XDG_DATA_HOME", (out / "userdata").string().c_str(), 1);
#endif
    Project project; std::string error;
    Check(Project::CreateNew(root.string(), "Public lifecycle audit", project, error), "FIXTURE.PROJECT", "ordinary new project: " + error);
    if (failures) return 1;
    project.Settings().legacyGameplay = false;
    project.Settings().startupScene = "Scenes/audit.judas";
    Check(project.Save(error), "FIXTURE.PROJECT_SAVE", "normal project registration: " + error);
    Check(Write(root / "Assets/item.js", itemScript), "FIXTURE.SCRIPT_WRITE", "original local item script written");
    AssetDatabase assets; assets.Scan(root.string(), project.AssetsDir()); AssetRecord item, prefab, driver;
    Check(assets.Track((root / "Assets/item.js").string(), item, error), "FIXTURE.SCRIPT_ASSET", "normal script asset: " + error);
    Scene prefabScene;
    auto& prefabRoot = prefabScene.CreateObject("Root");
    prefabRoot.body = SceneBodyComponent{}; prefabRoot.body->motion = SceneBodyMotion::Dynamic;
    prefabRoot.body->mass = 2; prefabRoot.scripts = {{1, item.id, true, R"({"kind":"root"})"}};
    auto& child = prefabScene.CreateObject("Child"); child.parent = 1;
    child.transform.position = {0, 2, 0};
    child.scripts = {{1, item.id, true, R"({"kind":"child","internal":{"entity":"1"}})"}};
    Check(SaveSceneToFile(prefabScene, (root / "Assets/pair.judasprefab").string(), error), "FIXTURE.PREFAB_WRITE", "normal authored hierarchy: " + error);
    Check(assets.Track((root / "Assets/pair.judasprefab").string(), prefab, error), "FIXTURE.PREFAB_ASSET", "normal prefab asset: " + error);
    std::string driverText = driverScript;
    driverText.replace(driverText.find("PREFAB_ASSET"), std::string("PREFAB_ASSET").size(), prefab.id);
    Check(Write(root / "Assets/driver.js", driverText), "FIXTURE.DRIVER_WRITE", "public-API controller script written");
    Check(assets.Track((root / "Assets/driver.js").string(), driver, error), "FIXTURE.DRIVER_ASSET", "normal controller asset: " + error);
    Scene authored; authored.Settings().name = "Public lifecycle audit";
    authored.CreateObject("Audit controller").scripts = {{1, driver.id, true, "{}"}};
    auto& sentinel = authored.CreateObject("Persistent dynamic actor");
    sentinel.transform.position = {0, 4, 0}; sentinel.body = SceneBodyComponent{};
    sentinel.body->motion = SceneBodyMotion::Dynamic; sentinel.body->initialLinearVelocity = {1, 0, 0};
    authored.CreateObject("External reference marker"); authored.CreateObject("Disposable reference marker");
    Check(SaveSceneToFile(authored, project.StartupScenePath(), error), "FIXTURE.SCENE", "normal registered startup scene: " + error);
    if (failures) return 1;
    const std::map<fs::path, std::string> immutable{{root / "Assets/item.js", Read(root / "Assets/item.js")},
        {root / "Assets/driver.js", Read(root / "Assets/driver.js")},
        {root / "Assets/pair.judasprefab", Read(root / "Assets/pair.judasprefab")},
        {root / "Assets/item.js.judasmeta", Read(root / "Assets/item.js.judasmeta")},
        {root / "Assets/driver.js.judasmeta", Read(root / "Assets/driver.js.judasmeta")},
        {root / "Assets/pair.judasprefab.judasmeta", Read(root / "Assets/pair.judasprefab.judasmeta")},
        {project.StartupScenePath(), Read(project.StartupScenePath())},
        {project.ProjectFile(), Read(project.ProjectFile())}};
    unsigned frame = 0, phase = 0, phaseFrame = 0;
    RuntimeWorld* initial = nullptr; Json instanceIds, savedWorld, rejectedWorld;
    std::string validBytes, invalidBytes, saveRoot;
    uint64_t saveRequest = 0, rejectRequest = 0, loadRequest = 0;
    int negativeUpdates = 0; bool oneResolvedStep = false;
    uint64_t priorFixedSteps = 0; double priorSimulationTime = 0;
    const auto started = std::chrono::steady_clock::now();
    auto command = [&](RuntimeWorld& world, const char* value) {
        std::string detail;
        Check(world.SceneControl()->Set("audit.command", Json(value).dump(), detail),
            "COMMAND." + std::string(value), "ordinary project session command: " + detail);
    };
    ApplicationControl control; control.hidden = true;
    control.hostReady = [](EngineHost& host) {
        host.GetWindow().SetTestInputMode(true); SDL_GL_SetSwapInterval(0);
        std::string detail; host.Audio().Init(detail, true);
    };
    // Frame callbacks/resources/services are real. Zero dt deliberately isolates
    // publication, construction and restoration from unrelated integration drift.
    control.frameSeconds = [&](float) {
        if (oneResolvedStep) { oneResolvedStep = false; return 1.f / 60.f; }
        return 0.f;
    };
    control.worldReady = [&](EngineHost&, RuntimeWorld& world, InteractivePlay&) { initial = &world; };
    control.afterFrame = [&](EngineHost& host, RuntimeWorld& world, InteractivePlay& play) {
        ++frame;
        if (world.Scripts() && !world.Scripts()->Diagnostics().empty()) {
            Json faults = Json::array(); for (const auto& fault : world.Scripts()->Diagnostics())
                faults.push_back({{"entity", fault.entity}, {"slot", fault.slot}, {"callback", fault.callback}, {"message", fault.message}});
            observations["faults"] = faults;
            Check(false, "APP.JS_HEALTH", "unexpected uncaught project callback fault"); Quit(); return;
        }
        auto state = State(world, 1); auto* service = world.SceneControl()->Saves(host.Resources());
        if (phase == 0 && state.value("started", 0) == 1) {
            const bool prepared = WorldPersistence::Prepare(world, error);
            if (!prepared && error != "loading") { Check(false, "APP.PREPARE", error); Quit(); return; }
            if (prepared) {
                Check(world.DefinitionIds().size() == 4 && world.Physics().AliveBodyCount() == 1,
                    "PREFAB.BASELINE", "four ordinary authored entities and one dynamic body");
                command(world, "spawn"); phase = 1; phaseFrame = frame;
            }
        } else if (phase == 1 && state.value("spawned", false) && frame > phaseFrame + 3) {
            instanceIds = state.at("instances"); observations["spawnController"] = state;
            if (!instanceIds.is_array() || instanceIds.size() != 2 || !InstanceId(instanceIds[0], "root") || !InstanceId(instanceIds[0], "child") ||
                !InstanceId(instanceIds[1], "root") || !InstanceId(instanceIds[1], "child")) {
                Check(false, "PREFAB.HIERARCHY_RESULT", "published prefab returned missing/malformed root or child identity"); Quit(); return;
            }
            Check(instanceIds.size() == 2 && instanceIds[0]["root"] != instanceIds[1]["root"] &&
                instanceIds[0]["child"] != instanceIds[1]["child"], "PREFAB.INDEPENDENT_IDS", "two separate published hierarchies");
            for (unsigned i = 0; i < instanceIds.size(); ++i) {
                const auto rootId = InstanceId(instanceIds[i], "root");
                const auto childId = InstanceId(instanceIds[i], "child");
                const auto a = State(world, rootId), b = State(world, childId);
                const auto serial = i + 1; const auto prefix = "PREFAB.INSTANCE_" + std::to_string(serial);
                observations[prefix] = {{"root", a}, {"child", b}, {"physical", Physical(world, rootId)}};
                Check(StringField(a, "label") == "Instance " + std::to_string(serial) && StringField(a, "kind") == "root",
                    prefix + ".PROPERTIES", "typed per-instance properties visible in constructor");
                Check(a.contains("payload") && a["payload"] == Json({{"serial", serial}, {"phase", "initial"}}) &&
                    b.contains("payload") && b["payload"] == Json({{"serial", serial}, {"phase", "child"}}),
                    prefix + ".INITIAL_STATE", "independent root/child constructor payloads");
                Check(StringField(a, "external") == "3" && StringField(b, "internal") == std::to_string(rootId) && StringField(b, "parent") == std::to_string(rootId),
                    prefix + ".REFERENCES", "published external reference and authored internal reference remapped");
                Check(a.contains("constructedVelocity") && a["constructedVelocity"] == Json({{"x", serial}, {"y", 0}, {"z", 0}}) &&
                    a.contains("constructedAngularVelocity") && a["constructedAngularVelocity"] == Json({{"x", 0}, {"y", serial * .25}, {"z", 0}}),
                    prefix + ".MOTION", "complete initial motion vectors visible before start/first simulation");
                Check(a.value("started", 0) == 1 && b.value("started", 0) == 1 && a.value("restores", 0) == 0 && b.value("restores", 0) == 0,
                    prefix + ".LIFECYCLE", "each newly published slot starts exactly once");
            }
            Check(state["invalidSlot"].value("typeError", false), "PREFAB.INVALID_SECOND_SLOT", "later invalid slot rejects after valid first initialization record");
            Check(state["invalidReference"].value("typeError", false) && !state.value("staleValid", true) && state["staleReference"].is_null(),
                "PREFAB.STALE_EXTERNAL", "destroyed external reference rejects and stable reference returns null");
            Check(world.DefinitionIds().size() == 7 && world.Physics().AliveBodyCount() == 3 && world.Scripts()->Capture().size() == 5,
                "PREFAB.ATOMIC_REJECTION", "both rejected attempts leave no published root/child/body/script");
            savedWorld = WorldState(world); observations["savedWorld"] = savedWorld;
            command(world, "save"); phase = 2; phaseFrame = frame;
        } else if (phase == 2) {
            saveRequest = state.value("request", uint64_t(0)); const auto* status = service->Status(saveRequest);
            if (status && status->state == "failed") { observations["saveFailure"] = Status(*status); Check(false, "SAVE.POSITIVE_WRITE", status->error); Quit(); return; }
            if (status && status->state == "completed" && !service->Busy()) {
                observations["saveStatus"] = Status(*status); saveRoot = service->Root();
                GameSnapshot snapshot; bool recovered = false; SaveStorage store(saveRoot);
                Check(store.Read("valid", snapshot, recovered, error) && !recovered, "SAVE.COMMITTED_SLOT", "public save creates readable checksum-valid ordinary slot: " + error);
                if (!snapshot.participants.count("motors")) { Check(false, "SAVE.REQUIRED_FIXTURE", "expected required motors participant absent"); Quit(); return; }
                snapshot.participants.at("motors").version = 99;
                Check(store.Write("invalid", snapshot, error), "SAVE.INVALID_FIXTURE", "detached slot has checksum-valid unsupported required participant: " + error);
                validBytes = Read(fs::path(saveRoot) / "valid.save"); invalidBytes = Read(fs::path(saveRoot) / "invalid.save");
                Check(!validBytes.empty() && !invalidBytes.empty(), "SAVE.SLOT_BYTES", "both isolated slot generations retained before rejected load");
                command(world, "reject"); phase = 3; phaseFrame = frame;
            }
        } else if (phase == 3) {
            rejectRequest = state.value("request", uint64_t(0));
            if (frame == phaseFrame + 1) { rejectedWorld = WorldState(world); observations["preRejectedLoadWorld"] = rejectedWorld; }
            const auto* status = service->Status(rejectRequest);
            if (status && status->state == "completed") { observations["unexpectedInvalidSuccess"] = Status(*status); Check(false, "SAVE.REJECTED_REQUIRED_PARTICIPANT", "invalid candidate unexpectedly published"); Quit(); return; }
            if (status && status->state == "failed" && state.contains("lastStatus") && state["lastStatus"]["state"] == "failed") {
                observations["rejectedLoadStatus"] = Status(*status); observations["postRejectedLoadWorld"] = WorldState(world);
                Check(status->error.find("required participant missing/newer: motors") != std::string::npos,
                    "SAVE.REJECTION_DIAGNOSTIC", "unsupported required participant reports its actual name/version");
                Check(&world == initial && WorldState(world) == rejectedWorld, "SAVE.LIVE_WORLD_UNCHANGED", "private candidate rejection preserves live identities, physical state, counts and clock");
                Check(state.value("score", 0) == 91 && state.value("started", 0) == 1 && state.value("restores", 0) == 0 &&
                    world.SceneControl()->Get("audit.marker") == Json("live-negative-control").dump(),
                    "SAVE.LIVE_GAME_UNCHANGED", "live JS state/session remain distinct from saved state after rejection");
                Check(Read(fs::path(saveRoot) / "valid.save") == validBytes && Read(fs::path(saveRoot) / "invalid.save") == invalidBytes,
                    "SAVE.SLOTS_UNCHANGED", "rejected load rewrites neither good nor invalid slot bytes");
                negativeUpdates = state.value("updates", 0); phase = 4; phaseFrame = frame;
            }
        } else if (phase == 4 && frame >= phaseFrame + 3) {
            Check(state.value("updates", 0) >= negativeUpdates + 3 && world.Scripts()->Diagnostics().empty(),
                "SAVE.LIVE_CALLBACKS_CONTINUE", "original live project executes ordinary update callbacks after rejection");
            priorFixedSteps = play.FixedStepsSinceReset(); priorSimulationTime = world.SimulationTimeSeconds();
            oneResolvedStep = true; phase = 7;
        } else if (phase == 7) {
            observations["postRejectionResolvedStep"] = WorldState(world);
            const auto body = world.RuntimeBody(2);
            const auto pose = world.Physics().GetTransform(body);
            Check(play.FixedStepsSinceReset() == priorFixedSteps + 1 && std::abs(world.SimulationTimeSeconds() - priorSimulationTime - SimulationTiming::kFixedTimestep) < 1e-7,
                "SAVE.ONE_FIXED_STEP", "normal loop executes exactly one scheduled authoritative step after rejection");
            Check(&world == initial && glm::length(pose.position - glm::vec3(7, 9, -3)) > .02f && world.SimulationTimeSeconds() > priorSimulationTime,
                "SAVE.LIVE_SIMULATION_CONTINUES", "rejected candidate leaves original dynamic body integrating through ordinary fixed step");
            command(world, "load"); phase = 5; phaseFrame = frame;
        } else if (phase == 5) {
            if (&world == initial) loadRequest = state.value("request", uint64_t(0));
            const auto* status = service->Status(loadRequest);
            if (status && status->state == "failed") { observations["positiveLoadFailure"] = Status(*status); Check(false, "SAVE.POSITIVE_LOAD", status->error); Quit(); return; }
            if (&world != initial && status && status->state == "completed" && state.value("restores", 0) == 1) {
                observations["loadStatus"] = Status(*status); observations["restoredWorld"] = WorldState(world); observations["restoredController"] = state;
                Check(WorldState(world) == savedWorld, "SAVE.RESTORED_PHYSICAL", "ordinary positive load reconstructs saved entity/body states and counts at paused boundary");
                Check(state.value("score", 0) == 37 && state.value("started", 0) == 1 && state.value("restores", 0) == 1 && state.value("restoreSawRestored", false),
                    "SAVE.RESTORE_NOT_START", "saved script state is installed before restore and start is not replayed");
                Check(world.SceneControl()->Get("audit.marker") == Json("saved-marker").dump(), "SAVE.RESTORED_SESSION", "saved session marker replaces later live mutation");
                Check(StringField(state, "resolvedReference") == "2" && state.value("resolvedValid", false), "SAVE.REFERENCE_REACQUIRED", "public stable reference resolves freshly in reconstructed world");
                for (unsigned i = 0; i < instanceIds.size(); ++i) {
                    const auto rootId = InstanceId(instanceIds[i], "root");
                    const auto childId = InstanceId(instanceIds[i], "child");
                    const auto a = State(world, rootId), b = State(world, childId); const auto prefix = "SAVE.INSTANCE_" + std::to_string(i + 1);
                    observations[prefix] = {{"root", a}, {"child", b}};
                    Check(a.value("started", 0) == 1 && b.value("started", 0) == 1 && a.value("restores", 0) == 1 && b.value("restores", 0) == 1 &&
                        a.value("restoreSawRestored", false) && b.value("restoreSawRestored", false) && a.value("restoreInitialNull", false) && b.value("restoreInitialNull", false),
                        prefix + ".LIFECYCLE", "restored prefab slots do not replay spawn payload or fresh start");
                    Check(a.value("restoredExternal", false) && b.value("restoredInternal", false) && StringField(b, "restoredParent") == std::to_string(rootId),
                        prefix + ".REFERENCES", "normal typed references resolve in restored hierarchy");
                }
                for (const auto& file : immutable) Check(Read(file.first) == file.second, "ASSET.UNCHANGED." + file.first.filename().string(), "runtime construction/load leaves original project asset bytes unchanged");
                phase = 6; Quit();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (frame > 6000 || std::chrono::steady_clock::now() - started > std::chrono::seconds(45)) {
            observations["deadline"] = {{"phase", phase}, {"frame", frame}, {"controller", state}, {"world", WorldState(world)}};
            if (const auto* status = service->Status(state.value("request", uint64_t(0)))) observations["deadline"]["request"] = Status(*status);
            Check(false, "APP.BOUNDED_DEADLINE", "ordinary resource/save services did not complete within bounded audit wait"); Quit();
        }
    };
    Application app; std::string projectPath = project.ProjectFile(); char name[] = "judas"; char* arguments[]{name, projectPath.data()};
    const int exitCode = app.Run(2, arguments, &control);
    Check(exitCode == 0 && phase == 6, "APP.COMPLETED", "ordinary Application startup/public commands/restore/shutdown");
    Json report{{"schema", 1}, {"cases", {"PUBLIC-SAVE-REJECT", "PUBLIC-PREFAB-INIT"}}, {"checks", checks}, {"failures", failures},
        {"frames", frame}, {"applicationExit", exitCode}, {"scope", "Linux/Application public VM lifecycle; paused publication snapshots, not trajectory, platform or whole-subsystem certification"},
        {"project", project.ProjectFile()}, {"saveRoot", saveRoot}, {"assertions", results}, {"observations", observations}};
    Write(out / "RESULT.json", report.dump(2) + "\n");
    std::cout << "SUMMARY " << checks << " checks " << failures << " failures; frames=" << frame << '\n';
    return failures ? 1 : 0;
}
