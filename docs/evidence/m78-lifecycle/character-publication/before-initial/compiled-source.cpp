#include "AuthoringJSON.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "WorldStreaming.h"
#include <glm/gtc/quaternion.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>

namespace {
namespace fs = std::filesystem;
struct Checks {
    nlohmann::json rows = nlohmann::json::array();
    unsigned failures = 0;
    void Check(bool ok, const std::string& name) {
        rows.push_back({{"name", name}, {"passed", ok}});
        failures += !ok;
        std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
    }
    void Require(bool ok, const std::string& name, const std::string& error = {}) {
        Check(ok, name);
        if (!ok) throw std::runtime_error(name + ": " + error);
    }
};
bool Pump(WorldStreaming& stream, const std::function<bool()>& done) {
    for (unsigned i = 0; i < 4000; ++i) {
        stream.Advance(false);
        if (done()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}
void Observe(Checks& c, RuntimeWorld& world, EntityId id, bool enabled,
             unsigned layer, CategoryMask tags, const std::string& phase) {
    auto* motor = world.RuntimeCharacter(id);
    c.Require(motor != nullptr, phase + " motor exists");
    const auto body = motor->observationBody;
    const auto expected = motor->position + motor->orientation * motor->settings.offset;
    const auto pose = world.Physics().GetTransform(body);
    c.Check(glm::length(pose.position - expected) < 1e-6f &&
            std::abs(glm::dot(pose.rotation, motor->orientation)) > 1 - 1e-6f,
            phase + " proxy follows root plus oblique offset");
    c.Check(world.Physics().IsBodyEnabled(body) == enabled,
            phase + " authored enabled state is already published");
    unsigned actualLayer = 99;
    CategoryMask mask = 0;
    const bool readable = world.Physics().GetCollisionFilter(body, actualLayer, mask);
    c.Check(readable && actualLayer == layer && mask == CategoryBit(layer),
            phase + " authored bilateral filter is already published");
    const auto direction = motor->orientation * glm::vec3(1, 0, 0);
    const auto cast = [&](PhysicsQueryFilter filter) {
        auto hit = world.Physics().Raycast(expected - direction * 2.f, direction, 4, filter);
        return hit.hit && hit.body.id == body.id;
    };
    PhysicsQueryFilter filter;
    filter.includeLayers = CategoryBit(layer);
    c.Check(cast(filter) == enabled, phase + " isolated layer query");
    filter = {};
    filter.requiredTags = tags;
    c.Check(cast(filter) == enabled, phase + " isolated tag query");
    filter.excludedTags = tags;
    c.Check(!cast(filter), phase + " excluded tag negative control");
    filter = {};
    filter.includeLayers = CategoryBit(layer + 1);
    c.Check(!cast(filter), phase + " unrelated layer negative control");
}
}

int main(int argc, char** argv) {
    Checks c;
    fs::path out;
    try {
        if (argc != 2) throw std::runtime_error("usage: character-publication fresh-output-directory");
        out = fs::absolute(fs::u8path(argv[1]));
        if (fs::exists(out)) throw std::runtime_error("refusing to overwrite existing fixture");
        fs::create_directories(out);
        std::string error;
        Project project;
        c.Require(Project::CreateNew((out / "project").string(), "Character publication", project, error),
                  "ordinary owned project", error);
        unsigned layer = 0, tag = 0;
        c.Require(project.Settings().classification.collision.Add("Actor", layer) &&
                  project.Settings().classification.tags.Add("Motor", tag),
                  "authored project classification");
        c.Require(project.Save(error), "project saved", error);
        Scene scene;
        auto& active = scene.CreateObject("active");
        const auto activeLocal = active.id;
        active.characterMotor = CharacterMotorSettings{};
        active.characterMotor->offset = {.8f, .2f, -.1f};
        active.characterMotor->collisionLayer = layer;
        active.characterMotor->collisionMask = CategoryBit(layer);
        active.tags = CategoryBit(tag);
        active.transform.position = {2, 3, 4};
        active.transform.rotation = glm::angleAxis(.73f, glm::normalize(glm::vec3(1, 2, -1)));
        auto disabled = active;
        disabled.id = scene.CreateObject("disabled").id;
        disabled.transform.position = {-4, 2, 1};
        disabled.characterMotor->enabled = false;
        *scene.Find(disabled.id) = disabled;
        const auto disabledLocal = disabled.id;

        // Lazy creation and settings changes must not require any simulation step.
        RuntimeWorld lazy;
        c.Require(lazy.Build(scene, nullptr, error, &project.Settings().classification), "ordinary lazy world builds", error);
        Observe(c, lazy, activeLocal, true, layer, CategoryBit(tag), "lazy creation");
        Observe(c, lazy, disabledLocal, false, layer, CategoryBit(tag), "lazy disabled creation");
        auto* motor = lazy.RuntimeCharacter(activeLocal);
        const auto originalBody = motor->observationBody;
        auto settings = motor->settings;
        settings.radius = .5f;
        settings.offset = {-.3f, .6f, .2f};
        c.Require(lazy.SetCharacterSettings(activeLocal, settings), "normal shape/offset configuration");
        Observe(c, lazy, activeLocal, true, layer, CategoryBit(tag), "shape replacement");
        c.Check(motor->observationBody.id != originalBody.id, "replacement has a fresh native generation");
        unsigned unusedLayer = 0; CategoryMask unusedMask = 0;
        c.Check(!lazy.Physics().GetCollisionFilter(originalBody, unusedLayer, unusedMask),
                "retired shape handle cannot alias replacement");
        auto teleport = lazy.RuntimeDefinition(activeLocal)->transform;
        teleport.position = {8, 6, 7};
        teleport.rotation = glm::angleAxis(-.54f, glm::normalize(glm::vec3(-2, 1, 3)));
        c.Require(lazy.SetRuntimeTransform(activeLocal, teleport), "ordinary oblique teleport");
        Observe(c, lazy, activeLocal, true, layer, CategoryBit(tag), "teleport before a step");
        c.Check(lazy.SimulationTimeSeconds() == 0, "all lazy observations use zero elapsed simulation");

        AssetDatabase db;
        db.Scan(project.RootDir(), project.AssetsDir());
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &db, &jobs);
        RuntimeWorld privateWorld;
        c.Require(privateWorld.Build(Scene{}, &resources, error, &project.Settings().classification), "private staging world builds", error);
        c.Require(privateWorld.StageRegionObject(*scene.Find(activeLocal), *scene.Find(activeLocal), error),
                  "normal private region staging", error);
        auto state = EntityPhysicalState{};
        state.position = scene.Find(activeLocal)->transform.position;
        state.rotation = scene.Find(activeLocal)->transform.rotation;
        state.linearVelocity = {1, -2, 3};
        c.Require(privateWorld.RestoreRegionObject(activeLocal, scene.Find(activeLocal)->transform, state, {}, error, false),
                  "normal private retained reconstruction", error);
        c.Check(!privateWorld.RuntimeDefinition(activeLocal) && !privateWorld.RuntimeBody(activeLocal).IsValid(),
                "staged actor has no public entity/body access");
        const auto center = state.position + state.rotation * scene.Find(activeLocal)->characterMotor->offset;
        const auto direction = state.rotation * glm::vec3(1, 0, 0);
        c.Check(!privateWorld.Physics().Raycast(center - direction * 2.f, direction, 4).hit,
                "private reconstructed capsule is absent from broadphase queries");
        privateWorld.PublishRegion({activeLocal});
        Observe(c, privateWorld, activeLocal, true, layer, CategoryBit(tag), "atomic publication");
        privateWorld.HideRegion({activeLocal});
        c.Check(!privateWorld.Physics().Raycast(center - direction * 2.f, direction, 4).hit,
                "hidden region is immediately query-invisible");
        privateWorld.PublishRegion({activeLocal});
        Observe(c, privateWorld, activeLocal, true, layer, CategoryBit(tag), "republished staged motor");

        c.Require(SaveSceneToFile(scene, (fs::path(project.ScenesDir()) / "motors.judas").string(), error),
                  "normal streaming source saved", error);
        RuntimeWorld world;
        c.Require(world.Build(Scene{}, &resources, error, &project.Settings().classification), "streamed world builds", error);
        WorldManifest manifest;
        manifest.unitsPerFrame = 1;
        WorldRegion region;
        region.id = "motors"; region.scene = "Scenes/motors.judas";
        manifest.regions.emplace(region.id, region);
        WorldStreaming stream(world, resources, project, manifest);
        auto request = [&] {
            const auto token = stream.Request("motors", false, error);
            c.Require(token != 0, "ordinary demand accepted", error);
            c.Require(Pump(stream, [&] { const auto s = stream.Status(token); return s && s->state == "active"; }),
                      "ordinary active publication");
            return token;
        };
        const auto time = world.SimulationTimeSeconds();
        auto token = request();
        auto first = stream.Resolve("motors", activeLocal);
        Observe(c, world, first, true, layer, CategoryBit(tag), "first streaming activation");
        Observe(c, world, stream.Resolve("motors", disabledLocal), false, layer, CategoryBit(tag), "disabled streaming activation");
        const auto oldHandle = world.RuntimeCharacter(first)->observationBody;
        EntityPhysicalState motion;
        c.Require(world.GetEntityState(first, motion), "ordinary root state reads");
        motion.linearVelocity = {1, -2, 3};
        c.Require(world.SetEntityState(first, motion), "ordinary root velocity write");
        c.Require(stream.Release(token), "release own region demand");
        c.Require(Pump(stream, [&] { return stream.Stats().active == 0 && stream.Stats().pending == 0; }),
                  "ordinary retained retirement");
        c.Check(world.Physics().AliveBodyCount() == 0, "retirement destroys all owned query capsules");
        c.Check(!world.Physics().GetCollisionFilter(oldHandle, unusedLayer, unusedMask),
                "retirement invalidates native body generation");
        token = request();
        auto next = stream.Resolve("motors", activeLocal);
        c.Check(next != first && world.RuntimeCharacter(next)->observationBody.id != oldHandle.id,
                "revisit creates fresh entity and body generations");
        c.Check(world.RuntimeCharacter(next)->velocity == motion.linearVelocity,
                "revisit retains actual authoritative velocity");
        Observe(c, world, next, true, layer, CategoryBit(tag), "retained streaming revisit");
        Observe(c, world, stream.Resolve("motors", disabledLocal), false, layer, CategoryBit(tag), "disabled retained revisit");
        c.Check(world.SimulationTimeSeconds() == time, "stage/retire/revisit does not advance physics");
        stream.Release(token);
        c.Require(Pump(stream, [&] { return stream.Stats().active == 0 && stream.Stats().pending == 0; }),
                  "final retirement completes");
        c.Check(world.Physics().AliveBodyCount() == 0, "repeated visits do not accumulate query bodies");
    } catch (const std::exception& e) {
        c.Check(false, std::string("fixture exception: ") + e.what());
    }
    if (!out.empty() && fs::exists(out)) {
        std::ofstream(out / "RESULT.json") << nlohmann::json{{"checks", c.rows}, {"failures", c.failures}}.dump(2) << '\n';
    }
    std::cout << "CHARACTER_PUBLICATION " << c.rows.size() << " checks " << c.failures << " failures\n";
    return c.failures ? 1 : 0;
}
