// Private cold-process probe using ONLY public world/save/physics contracts.
// No warm caches are inspected, injected or serialized by this probe.
// Preparation only: this file has not yet been compiled or run.
#include "WorldPersistence.h"
#include "RuntimeWorld.h"
#include "SaveStorage.h"
#include "PhysicsWorld.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using Json = nlohmann::json;
namespace {
constexpr float dt = 1.f / 60;
constexpr unsigned warmSteps = 120, resumedSteps = 10;
constexpr unsigned count = 5;
void Require(bool value, const std::string& description) {
    if (!value) throw std::runtime_error(description);
}
Json Vec(glm::vec3 value) { return Json::array({value.x, value.y, value.z}); }
Json Quat(glm::quat value) { return Json::array({value.w, value.x, value.y, value.z}); }
Scene Fixture() {
    Scene scene;
    auto floor = scene.CreateObject("static physical support").id;
    auto* f = scene.Find(floor);
    f->body = SceneBodyComponent{};
    f->body->halfExtents = {8, .5f, 8};
    f->body->friction = .6f;
    f->transform.position = {0, -.5f, 0};
    for (unsigned index = 0; index < count; ++index) {
        auto id = scene.CreateObject("ordinary supported body").id;
        auto* object = scene.Find(id);
        object->body = SceneBodyComponent{};
        object->body->motion = SceneBodyMotion::Dynamic;
        object->body->managed = true;
        object->body->halfExtents = {.5f, .5f, .5f};
        object->body->mass = 2;
        object->body->friction = .6f;
        object->body->restitution = 0;
        object->transform.position = {0, .5f + float(index), 0};
    }
    return scene;
}
std::vector<EntityId> DynamicIds(RuntimeWorld& world) {
    std::vector<EntityId> ids;
    for (const auto& object : world.ScriptObjects()) {
        const auto body = world.RuntimeBody(object.id);
        if (body.IsValid() && world.Physics().IsDynamicBody(body)) ids.push_back(object.id);
    }
    std::sort(ids.begin(), ids.end());
    Require(ids.size() == count, "physical fixture dynamic population differs");
    return ids;
}
void Configure(RuntimeWorld& world) {
    // Diagnostic reference makes both branches exercise iterative support rather
    // than disappearing into sleep. It is applied equally, not a performance test.
    world.Physics().SetSleepingEnabled(false);
}
void Step(RuntimeWorld& world, const std::vector<EntityId>& ids) {
    for (auto id : ids) world.Physics().ApplyLinearAcceleration(world.RuntimeBody(id), {0, -9.81f, 0}, dt);
    world.Physics().Step(dt);
}
Json Physical(RuntimeWorld& world, const std::vector<EntityId>& ids) {
    Json rows = Json::array();
    for (auto id : ids) {
        auto body = world.RuntimeBody(id);
        auto pose = world.Physics().GetTransform(body);
        rows.push_back({{"entity", id}, {"position", Vec(pose.position)}, {"rotation", Quat(pose.rotation)},
                        {"linear", Vec(world.Physics().GetLinearVelocity(body))},
                        {"angular", Vec(world.Physics().GetAngularVelocity(body))},
                        {"mass", world.Physics().GetMass(body)}, {"sleeping", world.Physics().IsSleeping(body)}});
    }
    return rows;
}
void Continue(RuntimeWorld& world, const std::vector<EntityId>& ids, const std::filesystem::path& path) {
    Json states = Json::array();
    states.push_back({{"step", 0}, {"bodies", Physical(world, ids)}});
    for (unsigned i = 1; i <= resumedSteps; ++i) {
        Step(world, ids);
        states.push_back({{"step", i}, {"bodies", Physical(world, ids)}});
    }
    std::ofstream(path) << states.dump(2) << '\n';
}
bool Restore(RuntimeWorld& world, const GameSnapshot& snapshot, std::string& error) {
    Scene scene;
    world.legacyGameplay = false;
    if (!WorldPersistence::SceneFrom(snapshot.participants, scene, error) ||
        !world.Build(scene, nullptr, error, nullptr, nullptr, true) ||
        !WorldPersistence::Prepare(world, error) || !WorldPersistence::Restore(world, snapshot.participants, error)) return false;
    WorldPersistence::Published(world);
    Configure(world);
    return true;
}
Json ReadJson(const std::filesystem::path& path) {
    Json result;
    std::ifstream input(path);
    Require(bool(input), "oracle file unavailable");
    input >> result;
    return result;
}
} // namespace

int main(int argc, char** argv) {
    try {
        Require(argc == 4, "usage: cold-support OUTPUT write|read|read-repeat|missing-contact|truncated-contact calm|impacted");
        const std::filesystem::path output(argv[1]);
        const std::string mode(argv[2]), scenario(argv[3]);
        Require(scenario == "calm" || scenario == "impacted", "unknown scenario");
        const auto directory = output / scenario;
        std::string error;
        if (mode == "write") {
            Require(!std::filesystem::exists(directory), "fresh writer output required");
            std::filesystem::create_directories(directory / "slots");
            RuntimeWorld source;
            source.legacyGameplay = false;
            Require(source.Build(Fixture(), nullptr, error), "source Build: " + error);
            Configure(source);
            const auto ids = DynamicIds(source);
            for (unsigned i = 0; i < warmSteps; ++i) Step(source, ids);
            if (scenario == "impacted") {
                const auto body = source.RuntimeBody(ids.back());
                const auto pose = source.Physics().GetTransform(body);
                source.Physics().ApplyImpulseAtPoint(body, {.2f, 0, 0}, pose.position + glm::vec3(0, .25f, 0));
                for (unsigned i = 0; i < 4; ++i) Step(source, ids);
            }
            GameSnapshot snapshot;
            snapshot.project = "private-generic-cold-support";
            snapshot.scene = scenario;
            snapshot.content = "public-save-contract-v1";
            snapshot.displayName = "physical support continuation probe";
            Require(WorldPersistence::Capture(source, snapshot.participants, error), "public capture: " + error);
            const auto before = Physical(source, ids);
            std::ofstream(directory / "captured-boundary.json") << before.dump(2) << '\n';
            SaveStorage storage((directory / "slots").string());
            Require(storage.Write("support", snapshot, error, true), "actual modern slot write: " + error);
            Continue(source, ids, directory / "uninterrupted.json");
            std::cout << "writer passed; actual modern slot and uninterrupted endpoint rows saved\n";
            return 0;
        }
        SaveStorage storage((directory / "slots").string());
        GameSnapshot snapshot;
        bool recovered = false;
        Require(storage.Read("support", snapshot, recovered, error), "actual modern slot read: " + error);
        Require(!recovered, "unexpected recovery for untouched positive fixture");
        if (mode == "missing-contact" || mode == "truncated-contact") {
            Require(snapshot.participants.count("contacts") != 0, "negative control contact participant absent");
            if (mode == "missing-contact") snapshot.participants.erase("contacts");
            else {
                Require(!snapshot.participants.at("contacts").data.empty(), "contact fixture unexpectedly empty");
                snapshot.participants.at("contacts").data.pop_back();
            }
            RuntimeWorld rejected;
            const bool accepted = Restore(rejected, snapshot, error);
            std::cout << "negative " << mode << " accepted=" << accepted << " error=" << error << '\n';
            Require(!accepted, "malformed/missing required participant was accepted");
            return 0;
        }
        Require(mode == "read" || mode == "read-repeat", "unknown mode");
        RuntimeWorld restored;
        Require(Restore(restored, snapshot, error), "fresh public restoration: " + error);
        const auto ids = DynamicIds(restored);
        Require(Physical(restored, ids) == ReadJson(directory / "captured-boundary.json"),
                "physical save boundary differs before any resumed step");
        Continue(restored, ids, directory / (mode == "read" ? "restored.json" : "restored-repeat.json"));
        std::cout << "reader passed exact physical boundary; resumed differences are measured separately, not assumed defects\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
