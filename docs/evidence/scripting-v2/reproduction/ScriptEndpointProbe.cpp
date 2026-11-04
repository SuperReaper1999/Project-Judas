// Qualification-only observer. No production thresholds, integration budgets or
// solver state are changed. The same source is linked against each native build.
#include "AssetDatabase.h"
#include "GameSession.h"
#include "InputSystem.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SceneFingerprint.h"
#include "Simulation.h"
#include "WorldPersistence.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {
constexpr float stepSeconds = 1.f / 60.f;

void Require(bool okay, const std::string& message) {
    if (!okay) throw std::runtime_error(message);
}

void Word(float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    std::printf(" %08x", unsigned(bits));
}

void Observe(RuntimeWorld& world, unsigned step) {
    for (const auto& object : world.ScriptObjects()) {
        const auto body = world.RuntimeBody(object.id);
        if (!body.IsValid() || !world.Physics().IsDynamicBody(body)) continue;
        const auto pose = world.Physics().GetTransform(body);
        const auto v = world.Physics().GetLinearVelocity(body);
        const auto w = world.Physics().GetAngularVelocity(body);
        std::printf("PHYS %u %llu", step, (unsigned long long)object.id);
        for (float value : {pose.position.x, pose.position.y, pose.position.z,
                           pose.rotation.w, pose.rotation.x, pose.rotation.y, pose.rotation.z,
                           v.x, v.y, v.z, w.x, w.y, w.z}) Word(value);
        std::puts("");
    }
    for (const auto& state : world.Scripts()->Capture())
        std::printf("SCRIPT %u %llu %llu %s\n", step, (unsigned long long)state.entity,
                    (unsigned long long)state.slot, state.json.c_str());
    for (const auto& agent : world.Scripts()->AIInspections())
        std::printf("AI %u %llu %llu %s\n", step, (unsigned long long)agent.entity,
                    (unsigned long long)agent.slot, agent.json.c_str());
    const auto& audio = world.AudioEmitters().front().settings;
    std::printf("NATIVE %u audio", step);
    Word(audio.volume); Word(audio.pitch);
    std::printf(" loop=%d camera=%d\n", audio.loop,
                world.PresentationCameras().front().settings.enabled);
}

Scene Fixture(const std::string& asset, bool freeflight) {
    Scene scene;
    auto& gravity = scene.CreateObject("Local gravity");
    gravity.gravity = SceneGravityComponent{};
    gravity.gravity->kind = SceneGravityKind::Uniform;
    gravity.gravity->regionRadius = 100;
    auto& floor = scene.CreateObject("Contact floor");
    floor.body = SceneBodyComponent{};
    floor.body->halfExtents = {20, .25f, 20};
    floor.transform.position = {0, freeflight ? -100.f : -.25f, 0};
    auto& body = scene.CreateObject("Utility-driven physical owner");
    body.transform.position = {0, 2, 0};
    body.body = SceneBodyComponent{};
    body.body->motion = SceneBodyMotion::Dynamic;
    body.body->mass = 2;
    body.body->halfExtents = {.25f, .4f, .3f};
    body.body->initialLinearVelocity = {.5f, 0, 0};
    body.body->initialAngularVelocity = {.2f, .4f, .3f};
    body.audioEmitter = SceneAudioEmitterComponent{};
    body.audioEmitter->playOnStart = false;
    body.renderCamera = SceneRenderCameraComponent{};
    body.scripts.push_back({1, asset, true, "{}"});
    return scene;
}
}

int main(int argc, char** argv) try {
    Require(argc == 3 || argc == 4, "usage: endpoint-probe full|read|negative <owned-fixture> [freeflight]");
    const bool freeflight = argc == 4;
    Require(!freeflight || std::string(argv[3]) == "freeflight", "known fixture variant");
    const std::string mode = argv[1];
    Require(mode == "full" || mode == "read" || mode == "negative", "known mode");
    const bool read = mode != "full";
    const auto root = fs::absolute(argv[2]);
    if (!read) {
        Require(!fs::exists(root), "producer output must be new");
        fs::create_directories(root / "Assets");
        std::ofstream(root / "Assets/agent.js") << R"JS(
import {UtilityAgent} from 'judas';
const definition={version:1,name:'physical-qualification',interval:.05,actions:[
 {id:'drive',considerations:[{fact:'desire'}]},
 {id:'coast',considerations:[{fact:'desire'}]}]};
export default class {
 constructor({entity}) { this.entity=entity; this.state={ticks:0,driven:0,starts:0,stops:0}; }
 make() {
  this.brain=new UtilityAgent(definition,{
   drive:{start:()=>this.state.starts++,stop:()=>this.state.stops++,
    update:()=>{this.state.driven++;this.entity.applyForce({x:3,y:0,z:1});
     this.entity.applyTorque({x:.03,y:.07,z:.02});}},
   coast:{start:()=>this.state.starts++,stop:()=>this.state.stops++}
  },'native physics brain');
 }
 start() {this.make();this.entity.setAudio({volume:.75,pitch:1.25,loop:true});
  this.entity.setCameraEnabled(false);}
 restore() {this.make();this.brain.restore(this.state.saved);}
 fixedUpdate(dt) {
  this.state.ticks++;
  const drive=this.state.ticks<=35 || this.state.ticks>85;
  this.brain.tick(dt,[{action:'drive',target:'east',facts:{desire:drive?.9:.1}},
   {action:'coast',target:'rest',facts:{desire:drive?.1:.9}}]);
  this.state.saved=this.brain.snapshot();
  this.state.velocity=this.entity.velocity;
  this.state.position=this.entity.transform.position;
 }
}
)JS";
    }
    AssetDatabase assets;
    assets.Scan(root.string(), (root / "Assets").string());
    AssetRecord script;
    std::string error;
    if (!read) Require(assets.Track((root / "Assets/agent.js").string(), script, error), error);
    else {
        const auto* existing = assets.FindByRelativePath("Assets/agent.js");
        Require(existing != nullptr, "cold process rediscovers exact asset");
        script = *existing;
    }
    ResourceManager resources(nullptr, &assets);
    RuntimeWorld world;
    ProjectSettings settings;
    settings.legacyGameplay = false;
    world.ConfigureProject(settings);
    Scene scene;
    if (read) {
        std::ifstream file(root / "save.snapshot", std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>(file), {}};
        GameSnapshot saved;
        Require(DecodeGameSnapshot(bytes, saved, error), "snapshot decoding: " + error);
        Require(WorldPersistence::SceneFrom(saved.participants, scene, error), "saved scene: " + error);
        Require(world.Build(scene, &resources, error, nullptr, nullptr, true), "saved world: " + error);
        Require(WorldPersistence::Prepare(world, error) &&
                WorldPersistence::Restore(world, saved.participants, error), "native restoration: " + error);
        WorldPersistence::Published(world);
    } else {
        scene = Fixture(script.id, freeflight);
        Require(world.Build(scene, &resources, error), "authored world: " + error);
    }
    GameSession session;
    Require(session.Begin(world, error), "authoritative session: " + error);
    InputSystem input;
    if (mode == "negative") {
        const auto body = world.RuntimeBody(3);
        world.Physics().ApplyImpulseAtPoint(body, {1, 0, 0}, world.Physics().GetCenterOfMass(body));
    }
    if (read) {
        // Resume callbacks without advancing physical time; normal restoration
        // is allowed to reenter action ownership, not to move the body.
        world.UpdateScripts(&input, 0);
        Observe(world, 60);
    }
    for (unsigned step = read ? 61 : 1; step <= 120; ++step) {
        input.BeginFixedStep();
        StepAuthoritativeWorld(session, input, stepSeconds);
        input.BeginFrame();
        Require(world.Scripts() && world.Scripts()->Diagnostics().empty(), "no unexpected script fault");
        Observe(world, step);
        if (!read && step == 60) {
            GameSnapshot saved;
            saved.project = "scripting-v2-endpoints";
            saved.scene = "native-fixture";
            saved.content = std::string(64, 'b');
            Require(WorldPersistence::Capture(world, saved.participants, error), "native capture: " + error);
            std::ofstream(root / "save.snapshot", std::ios::binary) << EncodeGameSnapshot(saved);
            for (const auto& [name, chunk] : saved.participants)
                std::printf("CHUNK %s %u %zu %s\n", name.c_str(), chunk.version, chunk.data.size(),
                            SceneFingerprintSha256(chunk.data).c_str());
        }
    }
    session.End();
    world.Destroy();
    std::puts("PROBE completed");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL %s\n", error.what());
    return 1;
}
