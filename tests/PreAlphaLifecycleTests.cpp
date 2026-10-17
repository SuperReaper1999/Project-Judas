#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldPersistence.h"
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>

namespace {
unsigned checks = 0;
void Check(bool condition, const std::string& message) {
    ++checks;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message.c_str());
    if (!condition) throw std::runtime_error(message);
}
void Finish(RuntimeWorld& world) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < deadline) {
        world.AdvanceRuntimeTerrain();
        const auto* status = world.LiveTerrain().Status(1);
        if (status && !status->busy) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("bounded asynchronous terrain publication timed out");
}
Scene Source() {
    Scene scene;
    scene.SetNextId(10);
    SceneObject volume;
    volume.id = 1;
    volume.body = SceneBodyComponent{};
    volume.body->halfExtents = {5, 4, 5};
    volume.render = SceneRenderComponent{};
    scene.Objects().push_back(volume);
    return scene;
}
bool Solid(RuntimeWorld& world, glm::vec3 shift = {}) {
    const auto hit = world.Physics().Raycast(glm::vec3(0, 0, 6) - shift, {0, 0, -1}, 12);
    return hit.hit && hit.body.id == world.RuntimeBody(1).id;
}
bool RetainedSide(RuntimeWorld& world, glm::vec3 shift) {
    const auto hit = world.Physics().Raycast(glm::vec3(-6, 3, 0) - shift, {1, 0, 0}, 12);
    return hit.hit && hit.body.id == world.RuntimeBody(1).id;
}
}

int main() {
    try {
        AssetDatabase assets;
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        RuntimeWorld world;
        world.legacyGameplay = false;
        std::string error;
        Check(world.Build(Source(), &resources, error, nullptr, nullptr, true), "ordinary optional terrain world builds");
        auto source = CreateVolumeTerrain({21, 17, 21}, {10, 8, 10});
        Check(world.LiveTerrain().Create(1, source, true, 0, error), "private geometry/navigation preparation starts");
        std::map<std::string, SaveChunk> pending;
        pending["sentinel"] = {1, {'o', 'l', 'd'}};
        Check(!WorldPersistence::Capture(world, pending, error) && pending.count("sentinel"),
              "save refuses pending terrain without clearing caller's previous snapshot");
        const glm::vec3 delta(512, -512, 512);
        const auto body = world.RuntimeBody(1);
        Check(world.RequestOrigin(glm::dvec3(delta), error) && world.CommitOrigin(),
              "local immutable preparation can cross an origin shift before publication");
        Finish(world);
        Check(world.LiveTerrain().Status(1)->ready && Solid(world, delta),
              "asynchronous result publishes collision at the current owner frame");
        Check(!Solid(world), "negative query at the retired chart cannot hit the shifted owner");
        Check(bool(world.Navigation().Sample(glm::vec3(0, 4, 0) - delta, 2)),
              "navigation publishes at the shifted physical surface");
        Check(world.RuntimeBody(1).id == body.id, "publication preserves generation-safe body identity");

        // Remove a real through-tunnel, then prepare a deliberately invalid empty
        // product. Neither operation may publish geometry from an obsolete frame.
        for (float z : {4.5f, 2.25f, 0.f, -2.25f, -4.5f}) {
            Check(world.LiveTerrain().Brush(1, glm::vec3(0, 0, z) - delta,
                  {VolumeBrushKind::Remove, 2.5f, 1}, error), "world-space edit uses the translated local field");
            Finish(world);
            Check(world.LiveTerrain().Status(1)->error.empty(), "valid replacement commits successfully");
        }
        Check(!Solid(world, delta), "positive excavation control opens actual collision");
        Check(RetainedSide(world, delta), "independent side ray proves surrounding solid still exists");
        const auto revision = world.LiveTerrain().Status(1)->revision;
        Check(world.LiveTerrain().Brush(1, -delta, {VolumeBrushKind::Remove, 64, 1}, error),
              "empty-product failure is exercised through the public asynchronous path");
        Finish(world);
        Check(!world.LiveTerrain().Status(1)->error.empty() &&
              world.LiveTerrain().Status(1)->revision == revision && !Solid(world, delta) &&
              RetainedSide(world, delta),
              "failed replacement retains last-good revision and excavated collision");

        std::string field;
        Check(world.CaptureRuntimeTerrain(1, field, error) && !field.empty(), "committed field remains capturable after worker failure");
        std::map<std::string, SaveChunk> chunks;
        Check(WorldPersistence::Capture(world, chunks, error), "shifted edited world captures physical state");
        Scene restoredScene;
        Check(WorldPersistence::SceneFrom(chunks, restoredScene, error), "cold scene uses the saved precision chart");
        RuntimeWorld restored;
        restored.legacyGameplay = false;
        Check(restored.Build(restoredScene, &resources, error, nullptr, nullptr, true) &&
              WorldPersistence::Prepare(restored, error) && WorldPersistence::Restore(restored, chunks, error),
              "independent world reconstructs committed collision and navigation");
        std::string restoredField;
        Check(restored.CaptureRuntimeTerrain(1, restoredField, error) && restoredField == field &&
              !Solid(restored, delta) && restored.Settings().worldOrigin == glm::dvec3(delta),
              "restored physical endpoint and exact field agree, not merely cached status");
        Check(RetainedSide(restored, delta), "restored collider has real surrounding solid, not an empty negative-query false positive");
        Check(bool(restored.Navigation().Sample(glm::vec3(0, -3, 0) - delta, 2)),
              "restored tunnel floor is navigable at the translated endpoint");

        Check(world.LiveTerrain().Brush(1, -delta, {VolumeBrushKind::Add, 1.5f, 1}, error),
              "start replacement immediately before destruction");
        Check(world.DestroyEntity(1, &error), "destroy pending owner through ordinary lifecycle");
        jobs.WaitAll();
        world.AdvanceRuntimeTerrain();
        Check(!world.HasRuntimeTerrain(1) && !world.Physics().IsBodyEnabled(body),
              "cancelled worker cannot resurrect collision or its retired owner");

        Scene unsupported;
        auto& atmosphere = unsupported.CreateObject("optional atmosphere");
        atmosphere.body = SceneBodyComponent{};
        atmosphere.celestial = SceneCelestialComponent{};
        atmosphere.celestial->gravitationalParameter = 62784;
        atmosphere.atmosphere = SceneAtmosphereComponent{};
        RuntimeWorld atmosphereWorld;
        atmosphereWorld.legacyGameplay = false;
        const bool atmosphereBuilt = atmosphereWorld.Build(unsupported, nullptr, error);
        Check(atmosphereBuilt, "ordinary atmosphere ownership builds: " + error);
        atmosphereWorld.legacyGameplay = true;
        Check(!atmosphereWorld.RequestOrigin({512, 0, 0}, error) &&
              error.find("script-owned") != std::string::npos &&
              atmosphereWorld.Settings().worldOrigin == glm::dvec3(0),
              "legacy execution remains explicitly unsupported and leaves the chart unchanged");
        std::printf("SUMMARY %u checks 0 failures\n", checks);
        return 0;
    } catch (const std::exception& exception) {
        std::fprintf(stderr, "FAIL %s after %u checks\n", exception.what(), checks);
        return 1;
    }
}
