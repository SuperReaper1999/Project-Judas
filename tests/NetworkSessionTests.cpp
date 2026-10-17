#include "AssetDatabase.h"
#include "GameSession.h"
#include "InputSystem.h"
#include "JobSystem.h"
#include "LocalMultiplayer.h"
#include "NetworkSession.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveService.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "Simulation.h"
#include "WorldState.h"
#include "../third_party/nlohmann/json.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
unsigned checks = 0;
void Check(bool ok, const std::string& message) {
    ++checks;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", message.c_str());
    std::fflush(stdout);
    if (!ok) throw std::runtime_error(message);
}

struct Content {
    Project project;
    Scene scene;
    AssetDatabase assets;
    JobSystem jobs{2};
    ResourceManager resources{nullptr, &assets, &jobs};
    explicit Content(const fs::path& root, bool changed = false, bool reuse = false) {
        if (reuse) {
            std::string error;
            Check(project.Load((root / "session.judasproj").string(), error), "child loads unchanged authored project: " + error);
            Check(LoadSceneFromFile(project.StartupScenePath(), scene, error), "child loads unchanged authored scene: " + error);
            assets.Scan(project.RootDir(), project.AssetsDir());
            return;
        }
        fs::create_directories(root / "Assets");
        fs::create_directories(root / "Scenes");
        fs::create_directories(root / "Saves");
        ProjectSettings settings;
        settings.name = "M99 native session fixture";
        settings.legacyGameplay = false;
        settings.startupScene = "Scenes/session.judas";
        settings.saveIdentity = std::string(64, '9');
        settings.input.Add("network_custom", true);
        { std::ofstream file(root / "session.judasproj"); file << Project::SerializeToString(settings); }
        SceneObject floor;
        floor.id = 1;
        floor.name = "Support";
        floor.transform.position = {0, -.5f, 0};
        floor.body = SceneBodyComponent{};
        floor.body->halfExtents = {100, .5f, 100};
        scene.Objects().push_back(floor);
        SceneObject box;
        box.id = 2;
        box.name = "Ordinary physical body";
        box.transform.position = {0, 1, 0};
        box.body = SceneBodyComponent{};
        box.body->motion = SceneBodyMotion::Dynamic;
        box.body->mass = 2;
        box.render = SceneRenderComponent{};
        box.render->color = changed ? glm::vec3(.9f, .1f, .1f) : glm::vec3(.2f, .6f, .9f);
        scene.Objects().push_back(box);
        SceneObject motor;
        motor.id = 3;
        motor.name = "Ordinary CharacterMotor";
        motor.transform.position = {-3, .92f, 0};
        motor.characterMotor = CharacterMotorSettings{};
        scene.Objects().push_back(motor);
        SceneObject nested;
        nested.id = 4;
        nested.name = "Unsupported nested replication";
        nested.parent = 2;
        nested.transform.position = {0, 2, 0};
        nested.render = SceneRenderComponent{};
        scene.Objects().push_back(nested);
        SceneObject gravity;
        gravity.id = 5;
        gravity.name = "Authored uniform gravity region";
        gravity.gravity = SceneGravityComponent{};
        gravity.gravity->kind = SceneGravityKind::Uniform;
        gravity.gravity->regionShape = SceneRegionShape::Box;
        gravity.gravity->regionHalfExtents = {200, 200, 200};
        scene.Objects().push_back(gravity);
        scene.SetNextId(10);
        std::string error;
        Check(SaveSceneToFile(scene, (root / "Scenes/session.judas").string(), error),
              "fixture scene writes: " + error);
        const bool loaded = project.Load((root / "session.judasproj").string(), error);
        Check(loaded, "fixture project loads: " + error);
        assets.Scan(project.RootDir(), project.AssetsDir());
    }
};

struct Instance {
    RuntimeWorld world;
    GameSession game;
    InputSystem input;
    std::shared_ptr<SceneSession> control;
    explicit Instance(Content& content) {
        std::string error;
        control = std::make_shared<SceneSession>(content.project, content.project.StartupScenePath());
        world.legacyGameplay = false;
        world.SetSceneControl(control);
        Check(world.Build(content.scene, &content.resources, error,
                          &content.project.Settings().classification, &content.project.Settings().navigation),
              "independent native world builds: " + error);
        Check(game.Begin(world, error) && input.SetMap(content.project.Settings().input, error),
              "ordinary script-owned session begins: " + error);
        Check(glm::distance(world.SampleEntityGravity(2, {0, 1, 0}), glm::vec3(0, -9.81f, 0)) < 1e-5f,
              "fixture supplies authored gravity instead of assuming a universal default");
    }
    ~Instance() { world.EndScripts(); game.End(); world.Destroy(); }
    NetworkSession& Net() { return world.Network(); }
    void Fixed() {
        input.BeginFixedStep();
        StepAuthoritativeWorld(game, input, 1.f / 60);
    }
};

struct Driver {
    Content& content;
    Instance& host;
    std::vector<Instance*> clients;
    double now = 100;
    void Pump(unsigned count = 1, bool physical = true) {
        for (unsigned tick = 0; tick < count; ++tick) {
            now += 1. / 60;
            content.resources.Pump();
            for (auto* client : clients) client->world.PumpNetwork(now);
            host.world.PumpNetwork(now);
            if (physical) {
                host.Fixed();
                for (auto* client : clients) client->Fixed();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    template<class Condition> bool Until(Condition condition, unsigned limit = 600) {
        for (unsigned i = 0; i < limit; ++i) {
            if (condition()) return true;
            Pump();
        }
        return condition();
    }
};

const NetworkSession::Entry* FindEntry(const std::vector<NetworkSession::Entry>& entries,
                                     const std::string& id) {
    for (const auto& entry : entries) if (entry.id == id) return &entry;
    return nullptr;
}

Json Vector(glm::vec3 value) { return Json::array({value.x, value.y, value.z}); }
double SteadySeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void WriteJson(const fs::path& path, const Json& value) {
    const auto temporary = path.string() + ".tmp";
    { std::ofstream file(temporary);file << value.dump(2) << '\n';if (!file) throw std::runtime_error("result write failed"); }
    fs::rename(temporary, path);
}

int ProcessHost(const fs::path& output, double seconds) {
    fs::create_directories(output);
    Content content(output / "project");
    Instance host(content);
    NetworkSession::Options options;
    options.address = "127.0.0.1";options.port = 0;options.maxPeers = 4;
    options.controls = {"move_x", "jump", "network_custom"};
    std::string error, boxId, motorId;
    Check(host.Net().Host(options, error), "independent-process host starts: " + error);
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();auto previous = start;
    bool ready = false;
    std::map<std::string, bool> held, connected;
    unsigned hits = 0, joins = 0, leaves = 0;
    std::vector<double> stepMicros;
    Json trace = Json::array();
    double accumulator = 0;
    std::string motorOwner;
    while (std::chrono::duration<double>(Clock::now() - start).count() < seconds) {
        const auto current = Clock::now();
        const double now = std::chrono::duration<double>(current - start).count();
        accumulator += std::min(.25, std::chrono::duration<double>(current - previous).count());previous = current;
        content.resources.Pump();host.world.PumpNetwork(now);
        if (!ready && host.Net().Status() == "listening") {
            Check(host.Net().Replicate(2, "host", "{\"fixture\":true}", boxId, error) &&
                  host.Net().Replicate(3, "", "{\"fixture\":true}", motorId, error), "host registers real shared body and motor");
            WriteJson(output / "ready.json", {{"port", host.Net().Port()}, {"project", (output / "project").string()}});
            ready = true;
        }
        for (const auto& peer : host.Net().Peers()) if (peer.id != "host") {
            if (peer.connected && !connected[peer.id]) ++joins;
            if (!peer.connected && connected[peer.id]) ++leaves;
            connected[peer.id] = peer.connected;
            if (peer.connected && motorOwner.empty()) {
                motorOwner = peer.id;Check(host.Net().SetOwner(motorId, peer.id, error), "ordinary host assigns physical motor identity");
            }
        }
        unsigned fixed = 0;
        while (accumulator >= 1. / 60 && fixed++ < 8) {
            if (auto* motor = host.world.RuntimeCharacter(3)) {
                auto intent = host.Net().Input(motorOwner);
                motor->velocity = {intent.count("move_x") ? intent.at("move_x") * 2.f : 0, motor->velocity.y, 0};
            }
            for (const auto& peer : host.Net().Peers()) if (peer.connected && peer.id != "host") {
                const auto intent = host.Net().Input(peer.id);
                const bool action = intent.count("jump") && intent.at("jump") == 1;
                if (action && !held[peer.id]) {
                    const auto body = host.world.RuntimeBody(2);
                    host.world.Physics().ApplyImpulseAtPoint(body, {8, 0, 0}, host.world.Physics().GetTransform(body).position);
                    ++hits;
                }
                held[peer.id] = action;
            }
            const auto stepStart = Clock::now();host.Fixed();
            stepMicros.push_back(std::chrono::duration<double, std::micro>(Clock::now() - stepStart).count());
            const auto body = host.world.RuntimeBody(2);
            trace.push_back({{"time", SteadySeconds()},
                {"position", Vector(host.world.Physics().GetTransform(body).position)},
                {"velocity", Vector(host.world.Physics().GetLinearVelocity(body))},
                {"motor", Vector(host.world.RuntimeCharacter(3)->position)},
                {"motorVelocity", Vector(host.world.RuntimeCharacter(3)->result.velocity)}});
            accumulator -= 1. / 60;
        }
        if (fixed >= 8) accumulator = 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto stats = host.Net().Stats();
    std::sort(stepMicros.begin(), stepMicros.end());
    const auto quantile = [&](double p) { return stepMicros.empty() ? 0 : stepMicros[size_t((stepMicros.size() - 1) * p)]; };
    WriteJson(output / "trace.json", trace);
    WriteJson(output / "result.json", {{"clock", "std::chrono::steady_clock seconds"},
        {"ready", ready}, {"hits", hits}, {"joins", joins}, {"leaves", leaves},
        {"box", Vector(host.world.Physics().GetTransform(host.world.RuntimeBody(2)).position)},
        {"motor", Vector(host.world.RuntimeCharacter(3)->position)}, {"steps", stepMicros.size()},
        {"fixedMedianUs", quantile(.5)}, {"fixedP95Us", quantile(.95)}, {"fixedWorstUs", quantile(1)},
        {"sentBytes", stats.transport.bytesSent}, {"receivedBytes", stats.transport.bytesReceived},
        {"snapshots", stats.snapshots}, {"rejected", stats.rejected}});
    Check(ready && hits >= 2 && joins >= 2, "independent clients exercised actual host gameplay and physics");
    return 0;
}

int ProcessClient(const fs::path& project, unsigned port, const fs::path& output, double seconds,
                  bool requireReconnect) {
    fs::create_directories(output);
    Content content(project, false, true);
    Instance client(content);
    NetworkSession::Options options;
    options.address = "127.0.0.1";options.port = port;options.name = requireReconnect ? "Impaired process" : "Direct process";
    std::string error;
    Check(client.Net().Join(options, error), "independent-process client starts: " + error);
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();auto previous = start;
    double accumulator = 0, firstConnected = -1, retryAt = 0;
    bool connected = false;
    std::vector<std::string> identities;
    unsigned reconnects = 0, inputPublications = 0;
    Json trace = Json::array();
    Json gaps = Json::array();
    double gapStart = -1;
    while (std::chrono::duration<double>(Clock::now() - start).count() < seconds) {
        const auto current = Clock::now();
        const double now = std::chrono::duration<double>(current - start).count();
        accumulator += std::min(.25, std::chrono::duration<double>(current - previous).count());previous = current;
        content.resources.Pump();client.world.PumpNetwork(now);
        const bool live = client.Net().Status() == "connected";
        if (live && !connected) {
            if (firstConnected < 0) firstConnected = now;else ++reconnects;
            identities.push_back(client.Net().PeerId());
            if (gapStart >= 0) {
                const auto end = SteadySeconds();
                gaps.push_back({{"start", gapStart}, {"end", end}, {"seconds", end - gapStart}});
                gapStart = -1;
            }
        }
        if (!live && connected) gapStart = SteadySeconds();
        connected = live;
        if (live) {
            const auto age = now - firstConnected;
            if (!client.Net().SendInput({{"move_x", age > .25 && age < 1.25 ? .5f : 0.f},
                                          {"jump", age > .6 && age < .85 ? 1.f : 0.f},
                                          {"network_custom", .25f}}, error))
                throw std::runtime_error("child input publication: " + error);
            ++inputPublications;
        } else if (firstConnected >= 0 && client.Net().Status() == "disconnected" && now >= retryAt) {
            client.Net().Join(options, error);retryAt = now + .2;
        }
        unsigned fixed = 0;
        while (accumulator >= 1. / 60 && fixed++ < 8) {
            client.Fixed();accumulator -= 1. / 60;
            if (live) {
                SceneTransform pose;
                if (client.Net().Presentation(2, pose)) {
                    SceneTransform motorPose;
                    const bool hasMotor = client.Net().Presentation(3, motorPose);
                    trace.push_back({{"time", SteadySeconds()}, {"position", Vector(pose.position)},
                                     {"motor", hasMotor ? Vector(motorPose.position) : Json()},
                                     {"snapshotAge", client.Net().Stats().snapshotAge}});
                }
            }
        }
        if (fixed >= 8) accumulator = 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto stats = client.Net().Stats();SceneTransform body, motor;
    const bool bodyReady = client.Net().Presentation(2, body), motorReady = client.Net().Presentation(3, motor);
    WriteJson(output / "result.json", {{"clock", "std::chrono::steady_clock seconds"},
        {"connectionGaps", gaps}, {"status", client.Net().Status()}, {"error", client.Net().Error()},
        {"identities", identities}, {"reconnects", reconnects}, {"boxReady", bodyReady}, {"motorReady", motorReady},
        {"box", Vector(body.position)}, {"motor", Vector(motor.position)}, {"sentBytes", stats.transport.bytesSent},
        {"receivedBytes", stats.transport.bytesReceived}, {"snapshots", stats.snapshots}, {"snapshotAge", stats.snapshotAge}});
    WriteJson(output / "trace.json", trace);
    Check(firstConnected >= 0 && bodyReady && motorReady && !identities.empty() && inputPublications > 0,
          "actual child sent independent controls and received physical endpoints");
    Check(!requireReconnect || (reconnects >= 1 && std::all_of(identities.begin(), identities.end(),
          [&](const std::string& id) { return id == identities.front(); })),
          "implicit reconnect retains reserved identity after a real connection failure");
    client.Net().Close();
    return 0;
}

void HandshakeDeadlineTests(Content& content) {
    // Independent framed TCP endpoint: it speaks valid pings but deliberately
    // withholds the session welcome. Socket activity must not renew the session
    // deadline. Synthetic monotonic time avoids a five-second test sleep.
    auto run = [&](bool delayedWelcome) {
        NetworkTransport service;
        NetworkTransport::Settings transportOptions;
        std::string error;
        Check(service.Listen("127.0.0.1", 0, transportOptions, error),
              "independent handshake-deadline TCP service listens: " + error);
        Instance client(content);
        NetworkSession::Options options;
        options.address = "127.0.0.1";
        options.port = service.BoundPort();
        Check(client.Net().Join(options, error), "deadline client begins a genuine connection: " + error);
        double now = 1000;
        uint32_t peer = 0;
        bool hello = false, controlsReceived = false;
        auto pump = [&](double step) {
            now += step;
            content.resources.Pump();
            service.Poll(now);
            for (const auto& event : service.Events()) {
                if (event.type == NetworkTransport::Event::Connected) peer = event.peer;
                if (event.type == NetworkTransport::Event::Message) {
                    const auto message = Json::parse(event.data);
                    if (message.value("t", "") == "hello") hello = true;
                    if (message.value("t", "") == "input" && message.contains("values")) {
                        const auto& values = message.at("values");
                        controlsReceived |= values.contains("network_custom") &&
                            values.at("network_custom").get<double>() == .5;
                    }
                }
            }
            if (peer) service.Send(peer, Json{{"t", "ping"}}.dump(), false, error);
            client.world.PumpNetwork(now);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        };
        for (unsigned i = 0; i < 600 && !hello; ++i) pump(.005);
        Check(hello && peer && client.Net().Status() == "connecting",
              "actual hello reaches independent service while client waits for welcome");
        const double observedHello = now;
        while (now - observedHello < 4.4) pump(.05);
        Check(client.Net().Status() == "connecting" && client.Net().Stats().transport.framesReceived >= 20,
              "repeated valid pings demonstrably arrive before the unchanged five-second handshake deadline");
        if (delayedWelcome) {
            Check(service.Send(peer, Json{{"t", "welcome"}, {"version", 1}, {"session", "deadline-positive"},
                    {"peer", "7"}, {"token", "deadline-token"},
                    {"controls", Json::array({"network_custom"})}}.dump(), false, error),
                  "independent service queues a genuine delayed but timely welcome");
            for (unsigned i = 0; i < 8 && client.Net().Status() != "connected"; ++i) pump(.01);
            Check(client.Net().Status() == "connected" && client.Net().PeerId() == "7",
                  "valid welcome before deadline completes the ordinary session contract");
            Check(client.Net().SendInput({{"network_custom", .5f}}, error),
                  "timely delayed handshake enables validated ordinary gameplay input: " + error);
            for (unsigned i = 0; i < 45; ++i) pump(.05);
            Check(client.Net().Status() == "connected" && controlsReceived,
                  "completed handshake survives its old deadline and delivers actual controls");
        } else {
            for (unsigned i = 0; i < 40 && client.Net().Status() != "disconnected"; ++i) pump(.05);
            Check(client.Net().Status() == "disconnected" &&
                  client.Net().Error().find("handshake") != std::string::npos && now - observedHello < 7,
                  "valid ping traffic cannot hold connecting open beyond the independent handshake deadline");
            Check(client.Net().PeerId().empty() && client.Net().Entries().empty(),
                  "expired handshake publishes no peer identity or partial authoritative state");
        }
        client.Net().Close();
        service.Stop();
    };
    run(false);
    run(true);
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string(argv[1]) == "--process-host")
            return ProcessHost(fs::absolute(argv[2]), std::stod(argv[3]));
        if (argc == 7 && std::string(argv[1]) == "--process-client")
            return ProcessClient(fs::absolute(argv[2]), unsigned(std::stoul(argv[3])),
                                 fs::absolute(argv[4]), std::stod(argv[5]), std::string(argv[6]) == "reconnect");
        const fs::path root = fs::absolute(argc > 1 ? argv[1] : ".cache/m99/session-native");
        Check(!fs::exists(root), "fresh evidence directory preserves previous failures");
        Content content(root / "current");
        HandshakeDeadlineTests(content);
        Instance host(content), client(content), other(content);
        std::string error, boxId, motorId;
        auto& server = host.Net();
        auto& remote = client.Net();
        Check(CanCaptureLegacyWorldState(host.world, error), "offline legacy-delta capture remains supported where previously valid");
        const auto offlineDelta = CaptureWorldState(host.world);
        Check(!server.Active() && !server.Client() && !host.control->networkActive,
              "optional networking starts inactive with no scene gate");
        NetworkSession::Options options;
        options.address = "127.0.0.1";
        options.port = 0;
        options.controls = {"move_x", "jump", "network_custom"};
        options.maxPeers = 4;
        Check(server.Host(options, error), "host begins asynchronous content validation: " + error);
        Driver driver{content, host, {&client, &other}};
        Check(driver.Until([&] { return server.Status() == "listening"; }),
              "actual listening socket appears after content validation: " + server.Error());
        Check(server.Port() != 0 && server.Active() && !server.Client(),
              "host publishes actual bound port and authoritative role");
        options.port = server.Port();
        options.name = "Independent client";
        EntityPhysicalState originalClientBody;
        Check(client.world.GetEntityState(2, originalClientBody), "capture real offline endpoint before joining");
        const auto originalClientAbsolute = client.world.Settings().worldOrigin + glm::dvec3(originalClientBody.position);
        Check(remote.Join(options, error) && other.Net().Join(options, error),
              "two separate client worlds begin real TCP joins: " + error);
        const double replicaTime=client.world.SimulationTimeSeconds();
        const auto replicaPosition=client.world.Physics().GetTransform(client.world.RuntimeBody(2)).position;
        client.Fixed();
        Check(client.world.SimulationTimeSeconds()>replicaTime&&
              client.world.Physics().GetTransform(client.world.RuntimeBody(2)).position==replicaPosition,
              "replica fixed callbacks advance local script time without duplicating physical integration");
        Check(driver.Until([&] { return remote.Status() == "connected" && other.Net().Status() == "connected"; }),
              "content-matched clients complete protocol handshake");
        Check(!remote.PeerId().empty() && remote.PeerId() != other.Net().PeerId() &&
              remote.PeerId() != server.PeerId(), "peer identities are distinct from host and each other");
        Check(server.Replicate(2, server.PeerId(), "{\"charge\":7}", boxId, error),
              "host opts ordinary body into replication: " + error);
        Check(server.Replicate(3, remote.PeerId(), "{\"ready\":true}", motorId, error),
              "host explicitly assigns motor ownership to one connection: " + error);
        std::string rejectedId;
        Check(!remote.Replicate(2, remote.PeerId(), "{}", rejectedId, error) &&
              !remote.SetOwner(boxId, remote.PeerId(), error) &&
              !remote.SetState(boxId, "{\"charge\":999}", error),
              "client cannot publish authoritative registration, ownership or replicated game state");
        Check(!server.Replicate(4, server.PeerId(), "{}", rejectedId, error),
              "unsupported hierarchy replication is rejected explicitly");
        Check(driver.Until([&] { return remote.Entries().size() == 2 && other.Net().Entries().size() == 2; }),
              "both independent clients receive server registrations");
        const auto before = host.world.Physics().GetTransform(host.world.RuntimeBody(2)).position;
        host.world.Physics().ApplyImpulseAtPoint(host.world.RuntimeBody(2), {8, 0, 0}, before);
        driver.Pump(240);
        const auto actual = host.world.Physics().GetTransform(host.world.RuntimeBody(2)).position;
        std::printf("PHYSICAL_ENDPOINT before=%.9g,%.9g,%.9g after=%.9g,%.9g,%.9g\n",
                    before.x, before.y, before.z, actual.x, actual.y, actual.z);
        SceneTransform presented;
        Check(actual.x > before.x + .5f && actual.y > .4f && actual.y < .7f,
              "real host rigid-body impulse/contact advances physical endpoint without client simulation");
        Check(remote.Presentation(2, presented) && glm::distance(presented.position, actual) < .1f,
              "client buffered presentation converges to actual resolved host physical endpoint");
        const auto definitions = remote.Entries();
        const auto* replicated = FindEntry(definitions, boxId);
        Check(replicated && replicated->state == "{\"charge\":7}",
              "client receives independently authored game state unchanged");
        Check(remote.SendInput({{"move_x", 1}, {"jump", 0}, {"network_custom", .25f}}, error),
              "client submits whitelisted bounded intent: " + error);
        Check(driver.Until([&] { auto values = server.Input(remote.PeerId());
              return values.count("move_x") && values.at("move_x") == 1; }),
              "server attributes validated intent to actual sender identity");
        std::uint64_t remotePlayer = 0;
        for (const auto& peer : server.Peers()) if (peer.id == remote.PeerId()) remotePlayer = peer.player;
        auto* mappedPlayer = host.world.PlayersIfLoaded() ? host.world.PlayersIfLoaded()->Find(remotePlayer) : nullptr;
        Check(mappedPlayer && mappedPlayer->input.Map().Find("network_custom") &&
              std::abs(mappedPlayer->input.Axis("network_custom") - .25f) < 1e-6f,
              "actual project input map and custom axis reach distinct authoritative player routing");
        Check(server.Input(other.Net().PeerId()).empty(), "other peer cannot inherit a different player's held input");
        const auto unchanged = server.Input(remote.PeerId());
        Check(!remote.SendInput({{"unregistered", 1}}, error) &&
              !remote.SendInput({{"move_x", 2}}, error) &&
              !remote.SendInput({{"move_x", std::numeric_limits<float>::quiet_NaN()}}, error),
              "unknown, out-of-range and nonfinite local input rejected before wire submission");
        driver.Pump(2);
        Check(server.Input(remote.PeerId()) == unchanged, "invalid input cannot partially replace held authoritative state");
        mappedPlayer->input.BeginFixedStep();
        Check(remote.SendInput({{"move_x", 1}, {"jump", 1}, {"network_custom", .25f}}, error) &&
              remote.SendInput({{"move_x", 1}, {"jump", 0}, {"network_custom", .25f}}, error),
              "short press and release queue before any transport pump");
        driver.Pump(6, false);
        mappedPlayer->input.BeginFixedStep();
        auto pulse = mappedPlayer->input.FixedAction("jump");
        Check(pulse.pressed && pulse.released && !pulse.held,
              "real wire delivery preserves both action edges within one authoritative observation");
        mappedPlayer->input.BeginFixedStep();
        pulse = mappedPlayer->input.FixedAction("jump");
        Check(!pulse.pressed && !pulse.released && !pulse.held, "short remote action is not replayed next fixed observation");
        auto* motor = host.world.RuntimeCharacter(3);
        const auto motorStart = motor->position;
        for (unsigned i = 0; i < 60; ++i) {
            const auto values = server.Input(remote.PeerId());
            motor->velocity = {values.count("move_x") ? values.at("move_x") * 2.f : 0, motor->velocity.y, 0};
            driver.Pump();
        }
        Check(motor->position.x > motorStart.x + 1.5f && motor->result.supported,
              "ordinary CharacterMotor resolves actual server motion from delivered control intent");
        Check(!host.control->Reload(error) && !host.control->Pending(),
              "online scene replacement rejected before queue mutation");
        auto* saves = host.control->Saves(content.resources);
        Check(!saves->Request("save", "online", "Online", "{}", error) &&
              !saves->Request("load", "online", "Online", "{}", error) && !saves->Busy(),
              "online cold save/load rejected without a partial service transaction");
        bool captureRejected = false;
        try { (void)CaptureWorldState(host.world); } catch (const std::logic_error&) { captureRejected = true; }
        const auto endpointBeforeDelta = host.world.Physics().GetTransform(host.world.RuntimeBody(2)).position;
        Check(!CanCaptureLegacyWorldState(host.world, error) && captureRejected &&
              !ApplyWorldState(host.world, offlineDelta, error) &&
              host.world.Physics().GetTransform(host.world.RuntimeBody(2)).position == endpointBeforeDelta,
              "editor/native legacy world-state save and apply cannot bypass active session persistence boundary");
        Check(server.SetOwner(motorId, other.Net().PeerId(), error), "host can deliberately transfer control ownership");
        Check(driver.Until([&] { auto entries = remote.Entries();auto* entry = FindEntry(entries, motorId);
              return entry && entry->owner == other.Net().PeerId(); }),
              "authority change reaches peers without replacing the physical entity");
        Check(!server.SetOwner(motorId, "retired-or-forged-peer", error),
              "unrecognized authority identity cannot be assigned");
        SceneTransform beforeOrigin;
        Check(remote.Presentation(2, beforeOrigin) && client.world.RequestOrigin({512, 0, 0}, error),
              "network client can request an ordinary local precision-chart transition");
        const auto absoluteBeforeOrigin = client.world.Settings().worldOrigin + glm::dvec3(beforeOrigin.position);
        client.Fixed();
        SceneTransform afterOrigin;
        Check(remote.Presentation(2, afterOrigin) &&
              glm::distance(client.world.Settings().worldOrigin + glm::dvec3(afterOrigin.position), absoluteBeforeOrigin) < .001,
              "buffered presentation preserves absolute placement through origin publication");
        const auto oldPeer = remote.PeerId();
        remote.Close();
        EntityPhysicalState restoredOffline;
        Check(client.world.GetEntityState(2, restoredOffline) &&
              glm::distance(client.world.Settings().worldOrigin + glm::dvec3(restoredOffline.position), originalClientAbsolute) < .001,
              "closing after rebase restores original offline absolute pose rather than stale chart coordinates");
        Check(driver.Until([&] {
              for (const auto& peer : server.Peers()) {
                  if (peer.id == oldPeer && peer.connected) return false;
              }
              return true;
              }),
              "server observes actual client disconnect");
        Check(server.Input(oldPeer).empty(), "disconnect neutralizes held remote controls");
        Check(remote.Join(options, error), "same client process can reconnect deliberately");
        Check(driver.Until([&] { return remote.Status() == "connected"; }), "reconnect completes a fresh handshake");
        Check(remote.PeerId() != oldPeer && server.Input(remote.PeerId()).empty(),
              "reconnect receives a new safe peer identity and no stale input");
        Check(driver.Until([&] { return remote.Entries().size() == 2; }), "reconnected client receives fresh baseline");
        Check(remote.SetInterest({1000, 0, 0}, 1, error), "client sets explicit bounded absolute interest");
        Check(driver.Until([&] { return remote.Entries().empty(); }), "out-of-interest replicas leave client relevance set");
        Check(remote.SetInterest({0, 0, 0}, 100, error), "client returns interest to physical scene");
        Check(driver.Until([&] { return remote.Entries().size() == 2; }), "relevance return reconstructs authoritative baseline");
        Check(server.Forget(boxId, error), "host retires a network identity explicitly");
        Check(driver.Until([&] { auto entries = remote.Entries();return !FindEntry(entries, boxId); }),
              "retired network identity is removed from client state");
        Check(!server.SetState(boxId, "{}", error), "retired identity cannot mutate a later registration");
        // A raw transport endpoint is deliberately independent of the trusted
        // session client. Public-API refusal alone cannot prove wire ownership.
        std::string fingerprint;
        Check(ComputeSaveContentFingerprint(content.project, content.assets, host.control->Scenes(),
                                            fingerprint, error), "independent adversarial client computes honest content identity");
        NetworkTransport adversary;
        NetworkTransport::Settings transportOptions;
        Check(adversary.Connect("127.0.0.1", server.Port(), transportOptions, error),
              "raw socket adversary connects without the public session wrapper");
        std::string adversaryPeer;
        auto pumpAdversary = [&] {
            adversary.Poll(driver.now);
            for (const auto& event : adversary.Events()) {
                if (event.type == NetworkTransport::Event::Connected)
                    adversary.Send(1, Json{{"t", "hello"}, {"version", 1}, {"content", fingerprint},
                                          {"name", "Wire adversary"}, {"token", ""}}.dump(), false, error);
                if (event.type == NetworkTransport::Event::Message) {
                    auto value = Json::parse(event.data);
                    if (value.value("t", "") == "welcome") adversaryPeer = value.at("peer").get<std::string>();
                }
            }
            driver.Pump();
        };
        for (unsigned tick = 0; tick < 180 && adversaryPeer.empty(); ++tick) pumpAdversary();
        Check(!adversaryPeer.empty() && adversaryPeer != remote.PeerId(),
              "raw endpoint receives its own actual connection identity");
        const auto victimInput = server.Input(remote.PeerId());
        Check(adversary.Send(1, Json{{"t", "input"}, {"sequence", 1u}, {"peer", remote.PeerId()},
                    {"values", {{"move_x", -.75f}}}}.dump(), false, error), "wire-level claimed-peer spoof is transmitted");
        for (unsigned tick = 0; tick < 30 && server.Input(adversaryPeer).empty(); ++tick) pumpAdversary();
        auto attributed = server.Input(adversaryPeer);
        Check(attributed.count("move_x") && attributed.at("move_x") == -.75f &&
              server.Input(remote.PeerId()) == victimInput,
              "wire input cannot impersonate claimed peer; actual socket owns attribution");
        const auto physicalBeforeAttack = host.world.Physics().GetTransform(host.world.RuntimeBody(2)).position;
        const auto rejectedBefore = server.Stats().rejected;
        Check(adversary.Send(1, Json{{"t", "snapshot"}, {"tick", 900000u},
                    {"entries", Json::array()}, {"peers", Json::array()}}.dump(), false, error) &&
              adversary.Send(1, Json{{"t", "input"}, {"sequence", 2u},
                    {"values", {{"move_x", 1.f}, {"jump", 1.f}}}}.dump(), false, error),
              "forged authoritative publication and valid-looking input queue in one hostile wire burst");
        for (unsigned tick = 0; tick < 30 && server.Stats().rejected == rejectedBefore; ++tick) pumpAdversary();
        Check(server.Stats().rejected > rejectedBefore && server.Status() == "listening" &&
              glm::distance(host.world.Physics().GetTransform(host.world.RuntimeBody(2)).position,
                            physicalBeforeAttack) < .02f,
              "host rejects forged state publication without corrupting actual physical endpoint");
        Check(server.Input(adversaryPeer).empty() && server.Input(remote.PeerId()) == victimInput,
              "rejected socket cannot publish later same-batch input or retain held controls");
        adversary.Stop();
        Content changed(root / "changed-content", true);
        Instance mismatch(changed);
        driver.clients.push_back(&mismatch);
        Check(mismatch.Net().Join(options, error), "changed-content client may request a diagnostic handshake");
        Check(driver.Until([&] { return mismatch.Net().Status() == "disconnected"; }),
              "real content mismatch is refused before client can enter authoritative world");
        Check(!mismatch.Net().Error().empty() && mismatch.Net().PeerId().empty() && mismatch.Net().Entries().empty(),
              "mismatch refusal retains useful diagnostic and publishes no partial replica state");
        mismatch.Net().Close();
        driver.clients.pop_back();
        const auto stats = server.Stats();
        Check(stats.snapshots > 0 && stats.transport.bytesSent > 0 && stats.transport.bytesReceived > 0,
              "measured session counters cover genuine socket traffic");
        std::printf("METRIC snapshots=%llu sent=%llu received=%llu peers=%zu entities=%zu\n",
                    static_cast<unsigned long long>(stats.snapshots),
                    static_cast<unsigned long long>(stats.transport.bytesSent),
                    static_cast<unsigned long long>(stats.transport.bytesReceived), stats.peers, stats.entities);
        remote.Close();other.Net().Close();server.Close();
        Check(!host.control->networkActive && !server.Active(), "close releases native lifecycle gate and sockets");
        Check(CanCaptureLegacyWorldState(host.world, error), "network close restores previously valid legacy-delta capture path");
        auto offlineSave = saves->Request("save", "offline", "Offline", "{}", error);
        Check(offlineSave != 0 && saves->Cancel(offlineSave),
              "offline save request remains valid and cancellable after network closure");
        Check(host.control->Reload(error) && host.control->Pending(), "offline scene request remains available after close");
        Check(!server.Host(options, error) && !server.Active() && host.control->Pending(),
              "pending scene transaction prevents opening a competing network session");
        const auto timeBefore = client.world.SimulationTimeSeconds();
        client.world.legacyGameplay = true;
        bool legacyRejected = false;
        try { client.Fixed(); } catch (const std::invalid_argument&) { legacyRejected = true; }
        client.world.legacyGameplay = false;
        Check(legacyRejected && client.world.SimulationTimeSeconds() == timeBefore,
              "headless fixed-step API rejects legacy execution before changing physical time");
        std::printf("SUMMARY %u checks 0 failures\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL %s after %u checks\n", error.what(), checks);
        return 1;
    }
}
