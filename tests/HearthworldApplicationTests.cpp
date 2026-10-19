// Consumer-game verification through the ordinary application loop. Fixture
// placement controls the player and camera; tools, resources, bodies, joints,
// terrain and saves are changed exclusively by the shipped game or SaveService.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include "SaveService.h"
#include "SceneSession.h"
#include "ScreenshotWriter.h"
#include "WorldStreaming.h"
#include "../third_party/nlohmann/json.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>

using Json = nlohmann::json;
namespace {
constexpr EntityId kPlayer = 100;
constexpr EntityId kVolume = 200;
unsigned checks = 0;
unsigned failures = 0;

void Check(bool ok, const std::string& label) {
    ++checks;
    failures += !ok;
    std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl;
}

void Quit() {
    SDL_Event event{};
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}

Json ReadJson(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot read fixture " + path.string());
    return Json::parse(file);
}

void WriteJson(const std::filesystem::path& path, const Json& value) {
    std::ofstream file(path);
    file << value.dump(2) << '\n';
    if (!file) throw std::runtime_error("Cannot write observation " + path.string());
}

std::string ReadBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read observation " + path.string());
    return {std::istreambuf_iterator<char>(file), {}};
}

Json Vector(glm::vec3 value) { return {value.x, value.y, value.z}; }
glm::vec3 Vector(const Json& value) {
    return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

Json PlayerState(RuntimeWorld& world) {
    if (world.Scripts())
        for (const auto& record : world.Scripts()->Capture())
            if (record.entity == kPlayer && record.slot == 1) return Json::parse(record.json);
    return Json::object();
}

void FixtureState(RuntimeWorld& world, const std::function<void(Json&)>& edit) {
    auto records = world.Scripts()->Capture();
    for (auto& record : records) {
        if (record.entity != kPlayer || record.slot != 1) continue;
        auto state = Json::parse(record.json);
        edit(state);
        record.json = state.dump();
    }
    std::string error;
    if (!world.RestoreScriptState(records, error)) throw std::runtime_error(error);
}

// Surface-normal placement is a fixture operation, not a gameplay travel API.
// The test never changes a resource, terrain cell or target body this way.
void Aim(RuntimeWorld& world, glm::vec3 target, float distance = 2.0f) {
    auto transform = world.RuntimeDefinition(kPlayer)->transform;
    const auto up = glm::normalize(target);
    glm::vec3 backward{0, 0, 1};
    backward -= up * glm::dot(up, backward);
    if (glm::length(backward) < .01f) backward = {1, 0, 0};
    backward = glm::normalize(backward);
    transform.position = glm::normalize(target + backward * distance) * 120.06f;
    const auto localUp = glm::normalize(transform.position);
    const auto forward = glm::normalize(target - transform.position -
                                       localUp * glm::dot(target - transform.position, localUp));
    const auto right = glm::normalize(glm::cross(forward, localUp));
    transform.rotation = glm::normalize(glm::quat_cast(glm::mat3(right, localUp, -forward)));
    if (!world.SetRuntimeTransform(kPlayer, transform))
        throw std::runtime_error("Cannot position player fixture");
    world.RuntimeCharacter(kPlayer)->velocity = {};
    const auto direction = glm::normalize(target - transform.position - localUp * 1.6f);
    const float pitch = std::atan2(glm::dot(direction, localUp), glm::dot(direction, forward));
    FixtureState(world, [&](Json& state) { state["yaw"] = 0; state["pitch"] = pitch; });
}

Json PhysicalSnapshot(RuntimeWorld& world) {
    Json result = Json::object();
    for (const auto& object : world.ScriptObjects()) {
        const auto body = world.RuntimeBody(object.id);
        if (!body.IsValid() || !world.Physics().IsDynamicBody(body)) continue;
        const auto transform = world.Physics().GetTransform(body);
        const auto joint = world.RuntimeJoint(object.id);
        JointState jointState;
        const bool hasJoint = world.Physics().GetJoint(joint, jointState);
        result[std::to_string(object.id)] = {
            {"position", Vector(transform.position)},
            {"rotation", {transform.rotation.w, transform.rotation.x,
                          transform.rotation.y, transform.rotation.z}},
            {"velocity", Vector(world.Physics().GetLinearVelocity(body))},
            {"angularVelocity", Vector(world.Physics().GetAngularVelocity(body))},
            {"enabled", world.Physics().IsBodyEnabled(body)},
            {"joint", hasJoint},
            {"jointEnabled", hasJoint && jointState.settings.enabled},
        };
    }
    return result;
}

PhysicsCastHit Ray(RuntimeWorld& world, glm::vec3 origin, glm::vec3 direction, float maximum = 20) {
    PhysicsQueryFilter filter;
    filter.ignoredBodies.push_back(world.RuntimeBody(kPlayer));
    return world.Physics().Raycast(origin, direction, maximum, filter);
}

Json CharacterSnapshot(RuntimeWorld& world) {
    const auto* motor = world.RuntimeCharacter(kPlayer);
    const auto& transform = world.RuntimeDefinition(kPlayer)->transform;
    return {{"position", Vector(transform.position)}, {"velocity", Vector(motor->velocity)},
            {"orientation", {transform.rotation.w, transform.rotation.x,
                             transform.rotation.y, transform.rotation.z}}};
}

void ComparePhysical(RuntimeWorld& world, const Json& expected) {
    const auto actual = PhysicalSnapshot(world);
    Check(actual.size() == expected.size(), "cold restore retains every dynamic physical entity");
    for (const auto& [id, saved] : expected.items()) {
        const auto present = actual.find(id);
        Check(present != actual.end(), "restored dynamic entity " + id + " exists");
        if (present == actual.end()) continue;
        const auto& value = *present;
        const auto qa = value.at("rotation"), qb = saved.at("rotation");
        const glm::quat a{qa[0].get<float>(), qa[1].get<float>(),
                          qa[2].get<float>(), qa[3].get<float>()};
        const glm::quat b{qb[0].get<float>(), qb[1].get<float>(),
                          qb[2].get<float>(), qb[3].get<float>()};
        Check(glm::distance(Vector(value.at("position")), Vector(saved.at("position"))) < 1e-5f &&
              std::abs(glm::dot(a, b)) > .999999f &&
              glm::distance(Vector(value.at("velocity")), Vector(saved.at("velocity"))) < 1e-5f &&
              glm::distance(Vector(value.at("angularVelocity")),
                            Vector(saved.at("angularVelocity"))) < 1e-5f,
              "restored actual pose and momentum match the saved endpoint for " + id);
        Check(value.at("enabled") == saved.at("enabled") && value.at("joint") == saved.at("joint") &&
              value.at("jointEnabled") == saved.at("jointEnabled"),
              "restored collision and ordinary-joint ownership match for " + id);
    }
}

bool Healthy(RuntimeWorld& world) {
    if (!world.Scripts()) return false;
    const auto& diagnostics = world.Scripts()->Diagnostics();
    for (const auto& diagnostic : diagnostics)
        std::cout << "SCRIPT " << diagnostic.callback << ' ' << diagnostic.message << std::endl;
    return diagnostics.empty();
}

void Screenshot(EngineHost& host, const std::filesystem::path& path) {
    std::vector<unsigned char> pixels;
    host.GetRenderer().CaptureFrame(host.GetWindow().Width(), host.GetWindow().Height(), pixels);
    Check(WriteRgbPng(path.string(), host.GetWindow().Width(), host.GetWindow().Height(), pixels),
          "capture real rendered game frame");
}

double Percentile(std::vector<double> values, double fraction) {
    std::sort(values.begin(), values.end());
    return values.empty() ? 0 : values[std::min(values.size() - 1,
                                              std::size_t(fraction * values.size()))];
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: observer project.judasproj write|read|controls|performance output\n";
        return 2;
    }
    const std::string project = std::filesystem::absolute(argv[1]).string();
    const std::string mode = argv[2];
    const auto output = std::filesystem::absolute(argv[3]);
    std::filesystem::create_directories(output);
    if (mode != "write" && mode != "read" && mode != "controls" && mode != "performance") return 2;
    const auto authored = ReadJson(std::filesystem::path(project).parent_path() /
                                   "Authoring/world_manifest.json");
    std::ofstream timings(output / (mode + "-timings.csv"));
    timings << "frame,phase,frame_ms,physics_ms,bodies,dynamic_bodies,terrain_revision,"
               "terrain_busy,terrain_prepare_ms,terrain_publish_ms\n";

    // The scenario is filled below by ordinary input phases, never native game
    // callbacks. Definitions remain explicit so every physical assertion can be
    // compared with the editable project in a code review.
    unsigned frame = 0;
    unsigned phase = 0;
    unsigned at = 0;
    std::vector<std::string> pulse;
    std::vector<std::string> held;
    std::map<std::string, std::string> controls;
    std::uint64_t request = 0;
    RuntimeWorld* initialWorld = nullptr;
    Json expected;
    Json beforeInventory;
    unsigned toolIndex = 0;
    unsigned swings = 0;
    glm::vec3 heldStart{}, treeUp{}, digOrigin{}, digDirection{};
    glm::vec3 performanceStart{};
    glm::vec3 walkStart{}, farStart{};
    float initialDigDistance = 0;
    double digStartedAt = 0;
    std::string originalField;
    EntityId builtFloor = 0, builtWall = 0, builtStone = 0;
    glm::vec3 stonePositionBefore{}, stoneUpBefore{};
    unsigned excavationCut = 0;
    std::uint64_t editRevision = 0;
    std::size_t initialBuildCount = 0;
    unsigned woodSpent = 0, stoneSpent = 0;
    std::vector<double> frameTimes, physicsTimes, scriptTimes;
    std::uint64_t residentBytes = 0;
    double measuredFrameMs = 0;
    bool finished = false;
    bool welcomeClosed = false;
    bool captureObserved = false;
    bool gameSaveBegan = false;
    bool gameLoadBegan = false;
    bool saveQuitClicked = false;
    std::string firstHomeSave;
    auto next = [&](unsigned value) { phase = value; at = frame; };
    auto press = [&](const std::string& action) { pulse.push_back(action); };
    auto elapsed = [&] { return frame - at; };
    const auto tree = authored.at("trees").at(0);
    const auto treeId = std::stoull(tree.at("id").get<std::string>());
    const auto rockId = std::stoull(authored.at("rocks").at(0).at("id").get<std::string>());
    const auto propId = std::stoull(authored.at("props").at(0).at("id").get<std::string>());
    auto treeChopPoint = [&] {
        const auto base = Vector(tree.at("base"));
        return base + glm::normalize(base) * 1.2f;
    };
    auto rootPosition = [&](RuntimeWorld& world, EntityId id) {
        const auto* object = world.RuntimeDefinition(id);
        if (!object) throw std::runtime_error("Missing fixture entity " + std::to_string(id));
        return object->transform.position;
    };

    ApplicationControl control;
    control.hidden = true;
    control.hostReady = [&](EngineHost& host) {
        host.GetWindow().SetTestInputMode(true);
        SDL_GL_SetSwapInterval(0);
        std::string error;
        host.Audio().Init(error, true);
    };
    control.frameSeconds = [&](float actual) { measuredFrameMs = actual * 1000; return 1.f / 60; };
    control.beforeFrame = [&](EngineHost& host, RuntimeWorld& world, InteractivePlay&) {
        try {
            if (mode == "controls" && phase == 208 && &world != initialWorld) {
                Check(!world.SceneControl()->Saves(host.Resources())->Busy(),
                      "F9 publishes its validated world at the ordinary outer boundary");
                next(209);
            }
            if (mode == "write" && request && phase == 70) {
                const auto* status = world.SceneControl()->Saves(host.Resources())->Status(request);
                if (status && !captureObserved && status->state == "writing") {
                    const Json captured = {
                        {"frame", frame}, {"state", PlayerState(world)},
                        {"physical", PhysicalSnapshot(world)}, {"character", CharacterSnapshot(world)},
                        {"requestState", status->state},
                    };
                    WriteJson(output / "write-capture-boundary.json", captured);
                    // Saving may legitimately wait for content hashing or
                    // required resources while physics continues. This is the
                    // first boundary after capture, before another fixed step.
                    // Preserve the original request endpoint separately.
                    for (const char* key : {"state", "physical", "character"}) expected[key] = captured.at(key);
                    expected["captureFrame"] = frame;
                    WriteJson(output / "expected.json", expected);
                    captureObserved = true;
                }
            }
            if ((mode == "read" || mode == "performance") && request && phase == 90) {
                const auto* status = world.SceneControl()->Saves(host.Resources())->Status(request);
                if (status && (status->state == "completed" || status->state == "failed")) {
                    Check(status->state == "completed", "fresh-process cold restore completes: " + status->error);
                    Check(&world != initialWorld, "restore publishes a separate validated world");
                    if (status->state == "completed") {
                        WriteJson(output / (mode + "-restore-boundary.json"), {
                            {"frame", frame}, {"state", PlayerState(world)},
                            {"physical", PhysicalSnapshot(world)}, {"character", CharacterSnapshot(world)},
                            {"requestState", status->state},
                        });
                        // This hook runs before the restored world's first physics
                        // step: compare the real endpoint, not a cached game flag.
                        ComparePhysical(world, expected.at("physical"));
                        const auto character = CharacterSnapshot(world);
                        const auto savedCharacter = expected.at("character");
                        Check(glm::distance(Vector(character.at("position")),
                                            Vector(savedCharacter.at("position"))) < 1e-5f &&
                              glm::distance(Vector(character.at("velocity")),
                                            Vector(savedCharacter.at("velocity"))) < 1e-5f,
                              "cold restore reinstates the actual motor endpoint and velocity");
                        const auto transform = world.RuntimeDefinition(kPlayer)->transform;
                        const auto center = transform.position + transform.rotation * glm::vec3(0, .9f, 0);
                        const auto side = transform.rotation * glm::vec3(1, 0, 0);
                        const auto capsule = world.Physics().Raycast(center + side * 2.f, -side, 4);
                        Check(capsule.hit && capsule.body.id == world.RuntimeBody(kPlayer).id,
                              "cold motor restoration publishes its actual classified collision/query capsule");
                        // Instances are intentionally constructed at the next
                        // normal script lifecycle boundary. Inspect their state
                        // after that boundary, separately from physical state.
                        std::string field, error;
                        Check(world.CaptureRuntimeTerrain(kVolume, field, error) &&
                              field == ReadBytes(output / "expected-terrain.bin"),
                              "cold restore retains exact excavated density samples");
                        const auto ray = expected.at("digRay");
                        const auto hit = Ray(world, Vector(ray.at("origin")),
                                             Vector(ray.at("direction")));
                        Check(hit.hit && hit.body.id == world.RuntimeBody(kVolume).id &&
                              std::abs(hit.distance - ray.at("distance").get<float>()) < 1e-4f,
                              "cold restore reconstructs the actual excavated collision endpoint");
                        Check(Healthy(world), "restored game scripts remain healthy");
                    }
                    if (mode == "performance" && status->state == "completed") {
                        request = 0;
                        next(80);
                    } else if (status->state == "completed") {
                        request = 0;
                        next(91);
                    } else {
                        finished = true;
                        Quit();
                    }
                }
            }
            for (const auto& entry : host.GetWindow().Input().Map().entries) {
                for (const auto& binding : entry.bindings)
                    host.GetWindow().QueueTestPhysical(binding.control, 0);
                if (entry.bindings.empty()) continue;
                auto binding = std::find_if(entry.bindings.begin(), entry.bindings.end(),
                                            [](const auto& value) { return value.scale > 0; });
                if (binding == entry.bindings.end()) binding = entry.bindings.begin();
                controls[entry.name] = binding->control;
            }
            for (const auto& action : held)
                host.GetWindow().QueueTestPhysical(controls.at(action), 1);
            for (const auto& action : pulse)
                host.GetWindow().QueueTestPhysical(controls.at(action), 1);
            pulse.clear();
        } catch (const std::exception& error) {
            Check(false, std::string("fixture beforeFrame: ") + error.what());
            finished = true;
            Quit();
        }
    };
    control.afterFrame = [&](EngineHost& host, RuntimeWorld& world, InteractivePlay&) {
        ++frame;
        try {
            auto* terrain = world.LiveTerrain().Status(kVolume);
            auto* service = world.SceneControl()->Saves(host.Resources());
            const auto state = PlayerState(world);
            const auto physics = world.Physics().LastStepStats();
            timings << frame << ',' << phase << ',' << measuredFrameMs << ','
                    << physics.totalMilliseconds << ',' << physics.bodies << ','
                    << physics.dynamicBodies << ',' << (terrain ? terrain->revision : 0) << ','
                    << (terrain ? terrain->busy : false) << ','
                    << (terrain ? terrain->prepareMs : 0) << ','
                    << (terrain ? terrain->publishMs : 0) << '\n';
            if (!welcomeClosed && frame >= 3 && state.contains("inventory")) {
                Check(world.UI().Paused() && !world.pointerCapture,
                      "welcome menu owns input before ordinary play starts");
                press("pause");
                welcomeClosed = true;
            }
            if (!Healthy(world)) {
                Check(false, "actual project scripts execute without errors");
                finished = true;
                Quit();
                return;
            }
            if (phase == 0 && frame >= 12 && terrain && terrain->ready && !terrain->busy &&
                !service->Content().empty() && !service->Busy() && state.contains("inventory")) {
                Check(world.RuntimeCharacter(kPlayer) && world.view.has_value(),
                      "ordinary project starts with CharacterMotor and script-owned camera");
                Check(!world.UI().Paused() && world.pointerCapture,
                      "ordinary Escape navigation closes the welcome menu and captures gameplay input");
                Check(!world.legacyGameplay, "no legacy/native gameplay controller is used");
                initialWorld = &world;
                Screenshot(host, output / (mode + "-initial.png"));
                if (mode == "read" || mode == "performance") {
                    expected = ReadJson(output / "expected.json");
                    std::string error;
                    request = service->Request("load", "hearthworld-review", "", "{}", error);
                    Check(request != 0, "request existing cold progress through ordinary save service: " + error);
                    next(90);
                } else if (mode == "controls") next(200);
                else {
                    walkStart = rootPosition(world, kPlayer);
                    held = {"move_y"};
                    next(100);
                }
            }

            if (phase == 100 && elapsed() >= 24) {
                held.clear();
                Check(glm::distance(rootPosition(world, kPlayer), walkStart) > .4f &&
                      world.RuntimeCharacter(kPlayer)->result.supported,
                      "logical walking moves the actual supported CharacterMotor toward the home");
                next(1);
            } else if (phase == 1) {
                Check(state.at("inventory").at("wood") == 0 && state.at("inventory").at("stone") == 0,
                      "new game has no fabricated building resources");
                beforeInventory = state.at("inventory");
                initialBuildCount = state.at("built").size();
                Aim(world, {4, std::sqrt(120.f * 120.f - 16.f), 0});
                press("equip_build");
                next(2);
            } else if (phase == 2 && elapsed() >= 3) {
                press("primary");
                next(3);
            } else if (phase == 3 && elapsed() >= 4) {
                Check(state.at("built").size() == initialBuildCount && state.at("inventory") == beforeInventory,
                      "insufficient-resource construction creates no body or resource debt");
                const auto& tool = authored.at("tools").at(toolIndex);
                Aim(world, Vector(tool.at("position")), 1.5f);
                next(4);
            } else if (phase == 4 && elapsed() >= 3) {
                press("interact");
                next(5);
            } else if (phase == 5 && elapsed() >= 5) {
                const auto& tool = authored.at("tools").at(toolIndex);
                const auto kind = tool.at("kind").get<std::string>();
                Check(state.at("owned").at(kind).get<bool>(), "ordinary E interaction collects the " + kind);
                Check(!world.RuntimeDefinition(std::stoull(tool.at("id").get<std::string>())),
                      "collected tool leaves no duplicate world pickup");
                if (++toolIndex < authored.at("tools").size()) {
                    Aim(world, Vector(authored.at("tools").at(toolIndex).at("position")), 1.5f);
                    next(4);
                } else {
                    Check(state.at("inventory") == beforeInventory,
                          "tool collection does not manufacture wood or stone");
                    Aim(world, rootPosition(world, rockId));
                    press("equip_axe");
                    next(6);
                }
            } else if (phase == 6 && elapsed() >= 3) {
                press("primary");
                next(7);
            } else if (phase == 7 && elapsed() >= 30) {
                Check(state.at("inventory") == beforeInventory,
                      "axe impact on a rock grants no mining resources");
                Aim(world, treeChopPoint());
                treeUp = glm::normalize(Vector(tree.at("base")));
                swings = 0;
                next(8);
            } else if (phase == 8 && elapsed() >= 3 && elapsed() % 30 == 3) {
                Aim(world, treeChopPoint());
                press("primary");
                ++swings;
                next(9);
            } else if (phase == 9 && elapsed() >= 30) {
                const auto record = state.at("harvest").value(std::to_string(treeId), Json::object());
                if (!record.value("felled", false)) {
                    Check(world.Physics().GetMotionType(world.RuntimeBody(treeId)) == BodyMotionType::Static,
                          "partially chopped tree remains an inactive static physical assembly");
                    if (swings >= 6) throw std::runtime_error("real axe rays never felled the authored tree");
                    next(8);
                } else {
                    Check(swings == 3 && state.at("stats").at("treesFelled") == 1,
                          "three valid axe impacts fell one independently tracked tree");
                    Check(world.Physics().IsDynamicBody(world.RuntimeBody(treeId)),
                          "felling releases an actual finite-mass rigid body");
                    Check(state.at("inventory").at("wood") == 0,
                          "a falling tree grants no wood before physical processing and collection");
                    next(10);
                }
            } else if (phase == 10) {
                const auto transform = world.Physics().GetTransform(world.RuntimeBody(treeId));
                const float lean = glm::dot(transform.rotation * glm::vec3(0, 1, 0), treeUp);
                if (lean < .65f) {
                    Check(true, "the released trunk physically rotates and falls under radial gravity");
                    Screenshot(host, output / "tree-fallen.png");
                    Aim(world, transform.position);
                    press("primary");
                    swings = 1;
                    next(11);
                } else if (elapsed() > 300) throw std::runtime_error("released tree never physically falls");
            } else if (phase == 11 && elapsed() >= 31) {
                const auto record = state.at("harvest").at(std::to_string(treeId));
                if (!record.value("processed", false)) {
                    if (swings >= 4) throw std::runtime_error("fallen trunk processing rays never complete");
                    Aim(world, world.Physics().GetTransform(world.RuntimeBody(treeId)).position);
                    press("primary");
                    ++swings;
                    at = frame;
                } else {
                    Check(state.at("stats").at("treesProcessed") == 1 &&
                          !world.RuntimeDefinition(treeId),
                          "processing consumes the real fallen trunk exactly once");
                    Check(state.at("inventory").at("wood") == 0,
                          "processing creates collectable physical logs rather than free inventory");
                    const auto tag = world.SceneControl()->ProjectConfig().Settings().classification.tags.Find("wood");
                    Check(tag >= 0 && world.QueryEntities(CategoryBit(unsigned(tag))).size() == 3,
                          "one processed tree produces three independent physical log bodies");
                    next(12);
                }
            } else if (phase == 12 && elapsed() >= 8) {
                const auto tag = world.SceneControl()->ProjectConfig().Settings().classification.tags.Find("wood");
                const auto logs = world.QueryEntities(CategoryBit(unsigned(tag)));
                if (!logs.empty()) {
                    Aim(world, world.Physics().GetTransform(world.RuntimeBody(logs.front())).position, 1.7f);
                    next(13);
                } else {
                    Check(state.at("inventory").at("wood") == 9 && state.at("stats").at("woodCollected") == 9,
                          "collecting three actual logs accounts for exactly nine wood");
                    Aim(world, rootPosition(world, rockId));
                    press("equip_pickaxe");
                    swings = 0;
                    next(20);
                }
            } else if (phase == 13 && elapsed() >= 3) {
                press("interact");
                next(12);
            } else if (phase == 20 && elapsed() >= 3) {
                if (!world.RuntimeDefinition(rockId)) {
                    Check(state.at("inventory").at("stone") == 6 && state.at("stats").at("stoneMined") == 6,
                          "six real pickaxe hits conserve the finite stone deposit yield");
                    Check(state.at("harvest").at(std::to_string(rockId)).at("processed").get<bool>(),
                          "exhausted deposit is recorded for persistence");
                    Aim(world, rootPosition(world, propId), 2.4f);
                    press("equip_gravity");
                    next(30);
                } else {
                    if (swings >= 9) throw std::runtime_error("pickaxe rays never exhaust the finite rock");
                    Aim(world, rootPosition(world, rockId));
                    press("primary");
                    ++swings;
                    next(21);
                }
            } else if (phase == 21 && elapsed() >= 32) {
                next(20);
            } else if (phase == 30 && elapsed() >= 3) {
                heldStart = world.Physics().GetTransform(world.RuntimeBody(propId)).position;
                press("grab");
                next(31);
            } else if (phase == 31 && elapsed() >= 3) {
                Check(!state.at("heldRef").is_null() && state.at("stats").at("grabs").get<int>() >= 1,
                      "gravity gun selects a real dynamic prop through its public ray query");
                FixtureState(world, [](Json& value) { value["yaw"] = .55; value["pitch"] = 0; });
                next(32);
            } else if (phase == 32 && elapsed() >= 70) {
                const auto body = world.RuntimeBody(propId);
                Check(world.Physics().IsDynamicBody(body) &&
                      glm::distance(heldStart, world.Physics().GetTransform(body).position) > .5f,
                      "bounded grip forces move a finite-mass collidable prop without transform parenting");
                Check(!state.at("heldRef").is_null(), "physical grip remains attached while manipulating the prop");
                Screenshot(host, output / "gravity-held.png");
                press("primary");
                next(33);
            } else if (phase == 33 && elapsed() >= 2) {
                const auto body = world.RuntimeBody(propId);
                const auto direction = world.view->pose.rotation * glm::vec3(0, 0, -1);
                Check(state.at("heldRef").is_null() && state.at("stats").at("launched").get<int>() == 1 &&
                      glm::dot(world.Physics().GetLinearVelocity(body), direction) > 3,
                      "launch releases the grip and applies real forward physical momentum");
                Check(state.at("inventory").at("wood") == 9 && state.at("inventory").at("stone") == 6,
                      "gravity-gun manipulation and launch do not alter resource accounting");
                Aim(world, {120, 0, 0}, 2.5f);
                farStart = rootPosition(world, kPlayer);
                held = {"move_y"};
                next(34);
            } else if (phase == 34 && elapsed() >= 35) {
                held.clear();
                const auto* motor = world.RuntimeCharacter(kPlayer);
                const auto radial = glm::normalize(rootPosition(world, kPlayer));
                Check(glm::dot(motor->result.up, radial) > .99f &&
                      glm::dot(motor->result.gravity, radial) < -9 && motor->result.supported &&
                      glm::distance(rootPosition(world, kPlayer), farStart) > .4f,
                      "ordinary walking and support work on the planet's side under radial gravity");
                const auto planetHit = Ray(world, rootPosition(world, kPlayer) + radial * 3.f, -radial, 8);
                Check(planetHit.hit && planetHit.body.id == world.RuntimeBody(1).id,
                      "far-side support comes from the actual planetary collider");
                const auto crate = rootPosition(world, 901);
                const auto up = glm::normalize(crate);
                const auto side = glm::normalize(glm::vec3(1, 0, 0) - up * glm::dot(up, glm::vec3(1, 0, 0)));
                const auto blockedPoint = glm::normalize(crate + side * .8f) * 120.f;
                Aim(world, blockedPoint, 2.8f);
                press("equip_build");
                next(40);
            } else if (phase == 40 && elapsed() >= 4) {
                beforeInventory = state.at("inventory");
                press("primary");
                next(41);
            } else if (phase == 41 && elapsed() >= 32) {
                Check(state.at("built").empty() && state.at("inventory") == beforeInventory,
                      "an obstructed physical placement retains all resources and creates no piece");
                Aim(world, {4, std::sqrt(120.f * 120.f - 16.f), 0}, 2.7f);
                next(42);
            } else if (phase == 42 && elapsed() >= 4) {
                press("primary");
                next(43);
            } else if (phase == 43 && elapsed() >= 32) {
                Check(state.at("built").size() == 1 && state.at("inventory").at("wood") == 5 &&
                      state.at("inventory").at("stone") == 6,
                      "real prefab construction debits exactly the floor recipe");
                const auto reference = state.at("built").at(0).at("reference").get<std::string>();
                if (reference.rfind("entity:", 0) != 0) throw std::runtime_error("unexpected root construction reference");
                builtFloor = std::stoull(reference.substr(7));
                JointState joint;
                const bool hasJoint = world.Physics().GetJoint(world.RuntimeJoint(builtFloor), joint);
                Check(hasJoint && joint.settings.enabled && joint.settings.type == JointType::Fixed &&
                      joint.settings.bodyA.id == world.RuntimeBody(builtFloor).id &&
                      joint.settings.bodyB.id == world.RuntimeBody(kVolume).id,
                      "floor is an ordinary dynamic body with an actual fixed connection to the terrain");
                const auto floor = world.Physics().GetTransform(world.RuntimeBody(builtFloor));
                const auto up = floor.rotation * glm::vec3(0, 1, 0);
                const auto hit = Ray(world, floor.position + up * 3.f, -up, 6);
                Check(hit.hit && hit.body.id == world.RuntimeBody(builtFloor).id,
                      "constructed floor exposes genuine physical collision");
                Aim(world, floor.position + up * .08f, 2.8f);
                press("build_next");
                next(44);
            } else if (phase == 44 && elapsed() >= 4) {
                Check(state.at("selectedBuild") == "wall", "logical recipe input selects the wall");
                press("primary");
                next(45);
            } else if (phase == 45 && elapsed() >= 32) {
                Check(state.at("built").size() == 2 && state.at("inventory").at("wood") == 1 &&
                      state.at("inventory").at("stone") == 5,
                      "a second constructed piece debits wood and stone once");
                const auto& wall = state.at("built").at(1);
                const auto reference = wall.at("reference").get<std::string>();
                const auto id = std::stoull(reference.substr(7));
                builtWall = id;
                JointState joint;
                const bool hasJoint = world.Physics().GetJoint(world.RuntimeJoint(id), joint);
                Check(hasJoint && joint.settings.enabled &&
                      joint.settings.bodyB.id == world.RuntimeBody(builtFloor).id &&
                      wall.at("support") == state.at("built").at(0).at("reference"),
                      "wall connects physically to the constructed floor rather than a decorative parent");
                press("build_next");
                next(46);
            } else if (phase == 46 && elapsed() >= 2) {
                press("build_next");
                next(47);
            } else if (phase == 47 && elapsed() >= 2) {
                press("build_next");
                Aim(world, {-4, std::sqrt(120.f * 120.f - 16.f), 0}, 2.8f);
                next(48);
            } else if (phase == 48 && elapsed() >= 3) {
                Check(state.at("selectedBuild") == "stonebase", "recipe cycling reaches the stone foundation");
                press("primary");
                next(49);
            } else if (phase == 49 && elapsed() >= 32) {
                Check(state.at("built").size() == 3 && state.at("inventory").at("wood") == 1 &&
                      state.at("inventory").at("stone") == 0,
                      "three connected-world constructions use the exact gathered resources");
                woodSpent = state.at("stats").at("woodSpent").get<unsigned>();
                stoneSpent = state.at("stats").at("stoneSpent").get<unsigned>();
                Check(woodSpent == 8 && stoneSpent == 6 &&
                      state.at("inventory").at("wood").get<unsigned>() + woodSpent ==
                          state.at("stats").at("woodCollected").get<unsigned>() &&
                      state.at("inventory").at("stone").get<unsigned>() + stoneSpent ==
                          state.at("stats").at("stoneMined").get<unsigned>(),
                      "resource ledger balances collection, construction expenditure and remaining inventory");
                builtStone = std::stoull(state.at("built").at(2).at("reference").get<std::string>().substr(7));
                const auto stone = world.Physics().GetTransform(world.RuntimeBody(builtStone));
                stonePositionBefore = stone.position;
                stoneUpBefore = stone.rotation * glm::vec3(0, 1, 0);
                Aim(world, {0, 120, 0}, 3.1f);
                press("equip_shovel");
                next(60);
            } else if (phase == 60 && elapsed() >= 4) {
                digOrigin = world.view->pose.position;
                digDirection = world.view->pose.rotation * glm::vec3(0, 0, -1);
                const auto hit = Ray(world, digOrigin, digDirection);
                Check(hit.hit && hit.body.id == world.RuntimeBody(kVolume).id,
                      "shovel is aimed at the independent excavatable backyard collider");
                initialDigDistance = hit.distance;
                std::string error;
                Check(world.CaptureRuntimeTerrain(kVolume, originalField, error),
                      "capture original finite terrain field before gameplay excavation");
                digStartedAt = SDL_GetPerformanceCounter() / double(SDL_GetPerformanceFrequency());
                press("primary");
                next(61);
            } else if (phase == 61 && terrain && terrain->ready && !terrain->busy && terrain->revision >= 2) {
                std::string field, error;
                Check(world.CaptureRuntimeTerrain(kVolume, field, error) && field != originalField,
                      "actual shovel input modifies the authoritative volumetric density field");
                const auto hit = Ray(world, digOrigin, digDirection);
                Check(hit.hit && hit.body.id == world.RuntimeBody(kVolume).id &&
                      hit.distance > initialDigDistance + .15f,
                      "excavation publishes a deeper actual collision surface, not just a visual hole");
                Check(state.at("stats").at("dug").get<unsigned>() >= 1 &&
                      state.at("inventory").at("wood") == 1 && state.at("inventory").at("stone") == 0,
                      "excavation changes terrain without inventing building resources");
                std::cout << "EDIT_LATENCY_MS " <<
                    (SDL_GetPerformanceCounter() / double(SDL_GetPerformanceFrequency()) - digStartedAt) * 1000 <<
                    " prepare_ms=" << terrain->prepareMs << " publish_ms=" << terrain->publishMs << std::endl;
                Check(world.RuntimeJoint(builtFloor).IsValid() && world.RuntimeJoint(builtWall).IsValid(),
                      "unrelated excavation retains both floor and attached-wall physical connections");
                excavationCut = 0;
                next(62);
            } else if (phase == 62 && elapsed() >= 32) {
                // At most four ordinary shovel dabs beside the foundation
                // remove its actual support footprint; no target pose is set.
                const auto contact = state.at("built").at(2).at("contact");
                const float x = contact.at("x").get<float>() + (excavationCut < 2 ? -1.45f : 1.45f);
                const float z = contact.at("z").get<float>() + (excavationCut % 2 ? .9f : -.9f);
                Aim(world, {x, std::sqrt(120.f * 120.f - x * x - z * z), z}, 2.8f);
                editRevision = terrain->revision;
                next(63);
            } else if (phase == 63 && elapsed() >= 3) {
                press("primary");
                next(64);
            } else if (phase == 64 && terrain && terrain->ready && !terrain->busy && terrain->revision > editRevision) {
                Check(world.RuntimeJoint(builtFloor).IsValid() && world.RuntimeJoint(builtWall).IsValid(),
                      "foundation excavation does not release a separately supported structure");
                ++excavationCut;
                if (!world.RuntimeJoint(builtStone).IsValid() || excavationCut == 4) next(65);
                else next(62);
            } else if (phase == 64 && elapsed() > 120) {
                throw std::runtime_error("foundation shovel dab did not publish: " + state.at("lastAction").get<std::string>());
            } else if (phase == 65 && elapsed() >= 90) {
                const auto stone = world.Physics().GetTransform(world.RuntimeBody(builtStone));
                const bool fell = glm::dot(stone.position - stonePositionBefore, stoneUpBefore) < -.05f ||
                                  glm::dot(stone.rotation * glm::vec3(0, 1, 0), stoneUpBefore) < .98f;
                if (!fell && excavationCut < 4) { next(62); return; }
                Check(!world.RuntimeJoint(builtStone).IsValid(),
                      "removing real soil under the authored anchor releases the foundation joint");
                Check(fell,
                      "the unsupported foundation physically falls or tips without scripted pose changes");
                Check(world.RuntimeJoint(builtFloor).IsValid() && world.RuntimeJoint(builtWall).IsValid(),
                      "the independently supported floor and wall remain connected after support loss elsewhere");
                std::string field, error;
                Check(world.CaptureRuntimeTerrain(kVolume, field, error),
                      "capture final physically excavated world after support loss");
                const auto hit = Ray(world, digOrigin, digDirection);
                Check(hit.hit && hit.body.id == world.RuntimeBody(kVolume).id,
                      "the original backyard pit remains collision-enabled after further excavation");
                std::ofstream(output / "expected-terrain.bin", std::ios::binary) << field;
                expected = {{"state", state}, {"physical", PhysicalSnapshot(world)},
                            {"character", CharacterSnapshot(world)},
                            {"requestFrame", frame},
                            {"digRay", {{"origin", Vector(digOrigin)}, {"direction", Vector(digDirection)},
                                        {"distance", hit.distance}}}};
                Screenshot(host, output / "write-completed-gameplay.png");
                request = service->Request("save", "hearthworld-review", "Validated homestead", "{}", error);
                expected["contentIdentityReadyAtRequest"] = !service->Content().empty();
                WriteJson(output / "expected-pre-request.json", expected);
                Check(request != 0, "queue a durable ordinary cold save: " + error);
                next(70);
            } else if (phase == 70 && request) {
                const auto* status = service->Status(request);
                if (status && (status->state == "completed" || status->state == "failed")) {
                    Check(status->state == "completed", "gameplay cold save completes: " + status->error);
                    Check(captureObserved, "observe the actual save-capture boundary independently of request time");
                    Check(Healthy(world), "full game loop finishes without script errors");
                    std::cout << "FINAL_STATE " << PlayerState(world).dump() << std::endl;
                    finished = true;
                    Quit();
                }
            }

            // Independently exercise the game's own F5/F9 handlers and the
            // actual authored save-and-quit button. These do not use native
            // save requests or call a JavaScript callback directly.
            if (phase == 200) {
                Aim(world, Vector(authored.at("tools").at(0).at("position")), 1.5f);
                next(201);
            } else if (phase == 201 && elapsed() >= 3) {
                press("interact");
                next(202);
            } else if (phase == 202 && elapsed() >= 4) {
                Check(state.at("owned").at("axe").get<bool>(), "controls workflow makes genuine collectable progress");
                press("pause");
                next(203);
            } else if (phase == 203 && elapsed() >= 3) {
                Check(world.UI().Paused() && !world.pointerCapture,
                      "pause menu suspends the world and releases pointer capture");
                walkStart = rootPosition(world, kPlayer);
                beforeInventory = state.at("inventory");
                held = {"move_y", "primary", "grab"};
                next(204);
            } else if (phase == 204 && elapsed() >= 12) {
                held.clear();
                Check(rootPosition(world, kPlayer) == walkStart &&
                      state.at("inventory") == beforeInventory &&
                      state.at("stats").at("grabs") == 0 && state.at("stats").at("launched") == 0,
                      "paused movement, use and grab inputs cannot leak into gameplay");
                Screenshot(host, output / "controls-pause.png");
                press("save");
                next(205);
            } else if (phase == 205) {
                gameSaveBegan |= service->Busy();
                const auto& slots = service->List();
                const auto home = std::find_if(slots.begin(), slots.end(), [](const auto& slot) {
                    return slot.id == "home" && slot.status == "compatible";
                });
                if (gameSaveBegan && !service->Busy() && home != slots.end() && elapsed() >= 4) {
                    Check(true, "F5 invokes the shipped JudasJS handler and commits the real home slot while paused");
                    firstHomeSave = ReadBytes(std::filesystem::path(service->Root()) / "home.save");
                    press("pause");
                    next(206);
                } else if (elapsed() > 400) throw std::runtime_error("F5 did not commit the game home slot");
            } else if (phase == 206 && elapsed() >= 3) {
                Check(!world.UI().Paused() && world.pointerCapture,
                      "resume restores gameplay focus without using the paused fire input");
                Aim(world, Vector(authored.at("tools").at(1).at("position")), 1.5f);
                next(207);
            } else if (phase == 207 && elapsed() >= 3) {
                press("interact");
                next(217);
            } else if (phase == 217 && elapsed() >= 4) {
                Check(state.at("owned").at("shovel").get<bool>(),
                      "ordinary gameplay changes inventory after the saved checkpoint");
                press("load");
                initialWorld = &world;
                next(208);
            } else if (phase == 208) {
                gameLoadBegan |= service->Busy();
                if (elapsed() > 400) throw std::runtime_error("F9 never publishes the saved game world");
            } else if (phase == 209) {
                Check(gameLoadBegan && state.at("owned").at("axe").get<bool>() &&
                      !state.at("owned").at("shovel").get<bool>() &&
                      !world.RuntimeDefinition(300) && world.RuntimeDefinition(301),
                      "F9 restores both actual inventory and collected/uncollected world-tool entities");
                Check(!world.UI().Paused() && world.pointerCapture && Healthy(world),
                      "game restore resumes with clean script ownership and correct focus");
                press("inventory");
                next(210);
            } else if (phase == 210 && elapsed() >= 3) {
                Check(world.UI().Paused() && !world.pointerCapture,
                      "the actual inventory screen owns focus and pauses physical gameplay");
                Screenshot(host, output / "controls-inventory.png");
                press("inventory");
                next(211);
            } else if (phase == 211 && elapsed() >= 3) {
                press("pause");
                next(212);
            } else if (phase == 212 && elapsed() >= 3) {
                const auto document = world.UI().Find("hearthworld");
                const auto* layout = world.UI().LayoutOf(document, "quit");
                if (!layout || !layout->visible || !layout->enabled)
                    throw std::runtime_error("authored save-and-quit button is not usable");
                const auto point = layout->rect.position + layout->rect.size * .5f;
                host.GetWindow().SetTestPointer(int(point.x), int(point.y));
                press("ui_click");
                saveQuitClicked = true;
                next(213);
            } else if (phase == 213 && elapsed() > 400) {
                throw std::runtime_error("authored save-and-quit button did not safely finish");
            }

            if (phase == 80) {
                if (elapsed() == 1) {
                    for (const char* key : {"inventory", "owned", "harvest", "built", "stats"})
                        Check(state.at(key) == expected.at("state").at(key),
                              std::string("profiled cold world retains game ") + key);
                    Aim(world, {35, std::sqrt(120.f * 120.f - 35.f * 35.f - 25.f), 5}, 2);
                    performanceStart = rootPosition(world, kPlayer);
                }
                held = elapsed() < 201 ? std::vector<std::string>{"move_y"} :
                       elapsed() < 401 ? std::vector<std::string>{"move_x"} :
                                         std::vector<std::string>{};
                frameTimes.push_back(measuredFrameMs);
                physicsTimes.push_back(world.Physics().LastStepStats().totalMilliseconds);
                const auto history = PerformanceProfiler::Get().History();
                if (!history.empty()) {
                    const auto& previous = history.back();
                    double scripts = 0;
                    for (const auto& scope : previous.scopes)
                        if (std::string_view(scope.name).find("JavaScript ") == 0)
                            scripts += (scope.end - scope.start) / 1e6;
                    scriptTimes.push_back(scripts);
                    residentBytes = std::max(residentBytes, previous.processResidentBytes);
                }
                if (frameTimes.size() >= 600) {
                    const auto timing = [&](const std::vector<double>& values) {
                        return Json{{"medianMs", Percentile(values, .5)},
                                    {"p95Ms", Percentile(values, .95)},
                                    {"worstMs", Percentile(values, 1)}};
                    };
                    WriteJson(output / "PERFORMANCE.json", {
                        {"frames", frameTimes.size()}, {"fixedTimestep", 1.0 / 60},
                        {"frame", timing(frameTimes)}, {"physics", timing(physicsTimes)},
                        {"script", timing(scriptTimes)}, {"scriptProfileSamples", scriptTimes.size()},
                        {"processResidentBytes", residentBytes},
                        {"profilerEnabled", PerformanceProfiler::Get().Enabled()},
                        {"bodies", world.Physics().AliveBodyCount()},
                        {"note", "Controlled 60-Hz application clock; measured frame cost is not a displayed FPS claim"},
                    });
                    Screenshot(host, output / "performance.png");
                    Check(glm::distance(rootPosition(world, kPlayer), performanceStart) > 1,
                          "logical walking actually moves the player during the profiled consumer workload");
                    Check(Healthy(world), "representative gameplay remains healthy for ten simulated seconds");
                    finished = true;
                    Quit();
                }
            }
            if (phase == 91) {
                const auto state = PlayerState(world);
                for (const char* key : {"inventory", "owned", "harvest", "built", "stats"})
                    Check(state.at(key) == expected.at("state").at(key),
                          std::string("cold restore retains actual game ") + key);
                Screenshot(host, output / "read-restored-gameplay.png");
                Check(Healthy(world), "restored world remains healthy through its first actual rendered step");
                finished = true;
                Quit();
            }
            if (frame > 6000 && !finished) {
                Check(false, "bounded application watchdog at phase " + std::to_string(phase));
                std::cout << "LAST_STATE " << state.dump() << std::endl;
                finished = true;
                Quit();
            }
            if (mode != "performance") std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } catch (const std::exception& error) {
            Check(false, std::string("fixture afterFrame: ") + error.what());
            finished = true;
            Quit();
        }
    };
    control.beforeShutdown = [&](EngineHost& host, RuntimeWorld& world, InteractivePlay&) {
        if (mode != "controls") return;
        auto* service = world.SceneControl()->Saves(host.Resources());
        const bool durable = !firstHomeSave.empty() &&
            firstHomeSave != ReadBytes(std::filesystem::path(service->Root()) / "home.save");
        Check(saveQuitClicked && world.UI().QuitRequested() && !service->Busy() && durable,
              "actual save-and-quit click exits only after a new durable save completes");
        finished = saveQuitClicked && world.UI().QuitRequested() && !service->Busy() && durable;
    };
    Application application;
    char executable[] = "judas";
    char* arguments[] = {executable, const_cast<char*>(project.c_str())};
    Check(application.Run(2, arguments, &control) == 0, "ordinary application shuts down cleanly");
    std::string profileError;
    PerformanceProfiler::Get().Export((output / (mode + "-profile.json")).string(), profileError);
    Check(finished, "observer completed its requested workflow");
    std::cout << "SUMMARY " << checks << " checks " << failures << " failures\n";
    return failures ? 1 : 0;
}
