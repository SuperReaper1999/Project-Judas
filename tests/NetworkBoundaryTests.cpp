#include "AssetDatabase.h"
#include "GameSession.h"
#include "InputSystem.h"
#include "JobSystem.h"
#include "NetworkSession.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "ScriptSystem.h"
#include "Simulation.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
unsigned checks = 0;
constexpr float dt = 1.f / 60;
void Check(bool value, const std::string& message) {
    ++checks;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", message.c_str());
    std::fflush(stdout);
    if (!value) throw std::runtime_error(message);
}
bool Near(glm::vec3 a, glm::vec3 b) { return glm::distance(a, b) < 1e-6f; }
Json State(RuntimeWorld& world) {
    if (world.Scripts()) for (const auto& value : world.Scripts()->Capture())
        if (value.entity == 1) return Json::parse(value.json);
    return Json::object();
}
void Healthy(RuntimeWorld& world) {
    if (world.Scripts()) for (const auto& value : world.Scripts()->Diagnostics())
        throw std::runtime_error(value.asset + " " + value.callback + " " + value.message);
}
const char* Probe = R"JS(import {network, session, time} from 'judas';
export default class {
 constructor(){this.state={calls:0, joined:false, closed:false, timeSeen:[]};}
 fixedUpdate(){
  ++this.state.calls;
  this.state.timeSeen.push(time.elapsed);
  if(session.get('role')!=='client')return;
  if(!this.state.joined){
   network.join({address:'127.0.0.1',port:session.get('port'),name:'Fixed boundary client'});
   this.state.joined=true;
  }
  if(session.get('close')===true&&network.mode==='client'){
   network.close();this.state.closed=true;
  }
 }
})JS";
struct Instance {
    RuntimeWorld world;
    GameSession game;
    InputSystem input;
    std::shared_ptr<SceneSession> control;
    Instance(const Project& project, const Scene& scene, ResourceManager& resources, const char* role) {
        std::string error;
        control = std::make_shared<SceneSession>(project, project.StartupScenePath());
        Check(control->Set("role", Json(role).dump(), error), "ordinary session state selects fixture role");
        world.legacyGameplay = false;
        world.SetSceneControl(control);
        Check(world.Build(scene, &resources, error, &project.Settings().classification, &project.Settings().navigation),
              "ordinary public script world builds: " + error);
        Check(game.Begin(world, error) && input.SetMap(project.Settings().input, error), "session begins: " + error);
    }
    ~Instance() { world.EndScripts();game.End();world.Destroy(); }
    void Fixed() { input.BeginFixedStep();StepAuthoritativeWorld(game, input, dt);Healthy(world); }
};
} // namespace

int main(int argc, char** argv) {
    try {
        const auto root = fs::absolute(argc > 1 ? argv[1] : ".cache/m99/network-boundary");
        Check(!fs::exists(root), "fresh fixture preserves previous role-boundary evidence");
        fs::create_directories(root / "Assets");fs::create_directories(root / "Scenes");
        { std::ofstream file(root / "Assets/probe.js");file << Probe; }
        AssetDatabase assets;
        assets.Scan(root.string(), (root / "Assets").string());
        AssetRecord script;
        std::string error;
        Check(assets.Track((root / "Assets/probe.js").string(), script, error), "normal script asset registration");
        ProjectSettings settings;
        settings.name = "M99 fixed-boundary regression";
        settings.legacyGameplay = false;
        settings.startupScene = "Scenes/boundary.judas";
        settings.saveIdentity = std::string(64, 'b');
        { std::ofstream file(root / "boundary.judasproj");file << Project::SerializeToString(settings); }
        Project project;
        const bool loaded = project.Load((root / "boundary.judasproj").string(), error);
        Check(loaded, "normal project loads: " + error);
        Scene scene;
        SceneObject owner;
        owner.id = 1;owner.name = "Script boundary probe";owner.scripts.push_back({1, script.id, true, "{}"});
        scene.Objects().push_back(owner);
        SceneObject body;
        body.id = 2;body.name = "Known physical motion";body.transform.position = {0, 5, 0};
        body.body = SceneBodyComponent{};body.body->motion = SceneBodyMotion::Dynamic;
        body.body->initialLinearVelocity = {3, 0, 0};
        scene.Objects().push_back(body);
        SceneObject motor;
        motor.id = 3;motor.name = "Known motor motion";motor.characterMotor = CharacterMotorSettings{};
        motor.characterMotor->gravityScale = 0;motor.transform.position = {-3, 3, 0};
        scene.Objects().push_back(motor);
        SceneObject gravity;
        gravity.id = 4;gravity.name = "Ordinary gravity region";gravity.gravity = SceneGravityComponent{};
        gravity.gravity->kind = SceneGravityKind::Uniform;gravity.gravity->regionShape = SceneRegionShape::Box;
        gravity.gravity->regionHalfExtents = {100, 100, 100};scene.Objects().push_back(gravity);scene.SetNextId(10);
        Check(SaveSceneToFile(scene, project.StartupScenePath(), error), "authored source shared by independent worlds");
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        Instance host(project, scene, resources, "host"), client(project, scene, resources, "client");
        NetworkSession::Options options;
        options.address = "127.0.0.1";options.port = 0;
        Check(host.world.Network().Host(options, error), "actual host begins content validation");
        double now = 100;
        for (unsigned i = 0; i < 600 && host.world.Network().Status() != "listening"; ++i) {
            resources.Pump();host.world.PumpNetwork(now += dt);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(host.world.Network().Status() == "listening", "actual host publishes bound socket");
        Check(client.control->Set("port", std::to_string(host.world.Network().Port()), error), "runtime connection endpoint is ordinary project session data");
        client.world.RuntimeCharacter(3)->velocity = {2, 0, 0};
        const auto initialBody = client.world.Physics().GetTransform(client.world.RuntimeBody(2));
        const auto initialVelocity = client.world.Physics().GetLinearVelocity(client.world.RuntimeBody(2));
        const auto initialMotor = client.world.RuntimeCharacter(3)->position;
        const auto firstClock = client.world.SimulationTimeSeconds();
        client.Fixed();
        auto state = State(client.world);
        Check(client.world.NetworkClient() && state.value("joined", false) && state.value("calls", 0) == 1,
              "actual first fixedUpdate starts client session exactly once");
        Check(Near(client.world.Physics().GetTransform(client.world.RuntimeBody(2)).position, initialBody.position) &&
              Near(client.world.Physics().GetLinearVelocity(client.world.RuntimeBody(2)), initialVelocity) &&
              Near(client.world.RuntimeCharacter(3)->position, initialMotor),
              "join from fixedUpdate suppresses same-step duplicate rigid/gravity/motor simulation");
        Check(std::abs(client.world.SimulationTimeSeconds() - firstClock - dt) < 1e-8,
              "joining step still advances local fixed clock exactly once");
        std::string replicated;
        Check(host.world.Network().Replicate(2, "host", "null", replicated, error), "host registers real physical endpoint");
        for (unsigned i = 0; i < 600 &&
                (client.world.Network().Status() != "connected" || client.world.Network().Entries().empty()); ++i) {
            resources.Pump();client.world.PumpNetwork(now += dt);host.world.PumpNetwork(now);
            host.Fixed();client.Fixed();std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(client.world.Network().Status() == "connected" && !client.world.Network().Entries().empty(),
              "fixed-callback join completes real content-matched multi-world transport handshake");
        const auto pausedHostClock = host.world.SimulationTimeSeconds();
        const auto pausedClientClock = client.world.SimulationTimeSeconds();
        const auto pausedBody = host.world.Physics().GetTransform(host.world.RuntimeBody(2));
        const auto pausedCalls = State(client.world).at("calls").get<unsigned>();
        const auto pausedSnapshots = host.world.Network().Stats().snapshots;
        // InteractivePlay pumps networking before its gameplay/debug pause
        // boundary. Advance the real transport's supplied monotonic clock by
        // thirty seconds while deliberately withholding all fixed callbacks.
        for (unsigned i = 0; i < 300; ++i) {
            now += .1;
            host.world.PumpNetwork(now);
            client.world.PumpNetwork(now);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        const auto pausedPeers = host.world.Network().Peers();
        Check(client.world.Network().Status() == "connected" && !pausedPeers.empty() && pausedPeers.front().connected,
              "transport heartbeats survive thirty seconds of paused authoritative gameplay");
        Check(host.world.SimulationTimeSeconds() == pausedHostClock &&
              client.world.SimulationTimeSeconds() == pausedClientClock &&
              Near(host.world.Physics().GetTransform(host.world.RuntimeBody(2)).position, pausedBody.position) &&
              State(client.world).at("calls").get<unsigned>() == pausedCalls &&
              host.world.Network().Stats().snapshots == pausedSnapshots,
              "paused transport does not invent simulation, snapshots or script callbacks");
        const auto beforeCloseCalls = State(client.world).at("calls").get<unsigned>();
        const auto beforeCloseClock = client.world.SimulationTimeSeconds();
        Check(client.control->Set("close", "true", error), "ordinary project flag requests close from next fixed callback");
        client.Fixed();
        state = State(client.world);
        Check(!client.world.NetworkClient() && state.value("closed", false) &&
              state.at("calls").get<unsigned>() == beforeCloseCalls + 1,
              "close from client fixed callback executes once and ends connection");
        Check(Near(client.world.Physics().GetTransform(client.world.RuntimeBody(2)).position, initialBody.position) &&
              Near(client.world.Physics().GetLinearVelocity(client.world.RuntimeBody(2)), initialVelocity) &&
              Near(client.world.RuntimeCharacter(3)->position, initialMotor),
              "captured replica step does not integrate restored offline bodies midway through close");
        Check(std::abs(client.world.SimulationTimeSeconds() - beforeCloseClock - dt) < 1e-8,
              "closing replica step advances local clock once");
        client.Fixed();
        Check(std::abs(client.world.Physics().GetTransform(client.world.RuntimeBody(2)).position.x - 3 * dt) < 1e-6f &&
              std::abs(client.world.RuntimeCharacter(3)->position.x - initialMotor.x - 2 * dt) < 1e-6f,
              "offline physics resumes once on the next full fixed step with original motion");
        Check(State(client.world).at("calls").get<unsigned>() == beforeCloseCalls + 2 &&
              std::abs(client.world.SimulationTimeSeconds() - beforeCloseClock - 2 * dt) < 1e-8,
              "role changes never duplicate callbacks or fixed clock advancement");
        std::printf("SUMMARY %u checks 0 failures\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL %s after %u checks\n", error.what(), checks);
        return 1;
    }
}
