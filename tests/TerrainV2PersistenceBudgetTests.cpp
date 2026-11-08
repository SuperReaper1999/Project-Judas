#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "SceneSerialization.h"
#include "WorldPersistence.h"
#include "WorldStreaming.h"
#include "terrain/TerrainBrush.h"
#include "terrain/TerrainPersistence.h"
#include "terrain/TerrainSource.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
void Write(const std::filesystem::path &path, const std::string &bytes) {
    std::ofstream file(path, std::ios::binary);
    Check(bool(file.write(bytes.data(), bytes.size())), "write fixture");
}
template <class Done> void Pump(WorldStreaming &stream, Done done) {
    for (unsigned i = 0; i < 10000; ++i) {
        stream.Advance(false);
        for (const auto &region : stream.Regions())
            if (region.state == "failed")
                throw std::runtime_error(region.error);
        if (done())
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("regional continuation timed out");
}
void Region(const std::filesystem::path &root, bool read) {
    Project project;
    std::string error;
    if (!read) {
        Check(Project::CreateNew(root.string(), "Terrain budget", project, error), error);
        project.Settings().legacyGameplay = false;
        Check(project.Save(error), error);
        Scene source;
        source.CreateObject("Regional runtime-created landscape").body = SceneBodyComponent{};
        Check(SaveSceneToFile(source, project.Resolve("Scenes/region.judas"), error), error);
    } else
        Check(project.Load((root / "Terrain_budget.judasproj").string(), error), error);
    AssetDatabase assets;
    assets.Scan(project.RootDir(), project.AssetsDir());
    JobSystem jobs(2);
    ResourceManager resources(nullptr, &assets, &jobs);
    RuntimeWorld world;
    world.ConfigureProject(project.Settings());
    Check(world.Build(Scene{}, &resources, error), error);
    WorldManifest manifest;
    manifest.retainedBytes = 16 * 1024 * 1024;
    manifest.pendingBytes = manifest.liveBytes = 128 * 1024 * 1024;
    WorldRegion region;
    region.id = "ground";
    region.scene = "Scenes/region.judas";
    region.halfExtents = {512, 16, 512};
    manifest.regions.emplace(region.id, region);
    Check(ValidateWorldManifest(manifest, project, error), error);
    WorldStreaming stream(world, resources, project, manifest);
    if (read) {
        SaveArchive saved(Read(root / "regions.state"));
        stream.Persist(saved, 9);
        saved.Finish();
    }
    const auto token = stream.Request("ground", false, error);
    Check(token != 0, error);
    Pump(stream, [&] { return stream.Status(token)->state == "active"; });
    const auto id = stream.Resolve("ground", 1);
    if (!read) {
        auto source = TerrainV2::Create(513, 513, 2);
        std::fill(source.heights.begin(), source.heights.end(), 2);
        Check(world.LiveTerrain().CreateLandscape(id, source, error) &&
                  world.LiveTerrain().Wait(id, error),
              error);
        Check(world.LiveTerrain().Brush(id, {0, 1, 0}, {VolumeBrushKind::Remove, 3, 1}, error) &&
                  world.LiveTerrain().Wait(id, error),
              error);
    }
    std::string state;
    Check(world.CaptureRuntimeTerrain(id, state, error) && state.size() > 2 * 1024 * 1024,
          "regional fixture retains the real large runtime source");
    if (read) {
        Check(state == Read(root / "terrain.state"), "cold regional continuation is exact");
        const auto hit = world.Physics().Raycast({0, 8, 0}, {0, -1, 0}, 20);
        Check(hit.hit && hit.point.y < 0 && world.LiveTerrain().Status(id)->chunks == 1024,
              "cold region publishes real excavated support");
    } else
        Write(root / "terrain.state", state);
    Check(stream.Release(token) && stream.Unload("ground"), "unload runtime-created landscape");
    Pump(stream, [&] { return stream.Regions().front().state == "unloaded"; });
    Check(world.LiveTerrain().Owners().empty() && world.Physics().AliveBodyCount() == 0,
          "suspension retires the full large source and every generated collider");
    if (!read) {
        SaveArchive saved;
        stream.Persist(saved, 9);
        Write(root / "regions.state", saved.bytes);
    }
}
} // namespace
int main(int argc, char **argv) {
    try {
        namespace fs = std::filesystem;
        const bool read = argc == 3 && std::string(argv[1]) == "--read";
        const auto root =
            argc == 3
                ? fs::path(argv[2])
                : fs::temp_directory_path() /
                      ("terrain-save-budget-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!read) {
            Check(!fs::exists(root), "fresh fixture required");
            fs::create_directories(root / "Assets");
        }
        AssetDatabase assets;
        assets.Scan(root.string(), (root / "Assets").string());
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        Scene scene;
        auto &owner = scene.CreateObject("Runtime-created one-kilometre landscape");
        owner.body = SceneBodyComponent{};
        RuntimeWorld world;
        world.legacyGameplay = false;
        std::string error;
        Check(world.Build(scene, &resources, error), error);
        std::map<std::string, SaveChunk> saved;
        if (!read) {
            auto source = TerrainV2::Create(513, 513, 2);
            std::fill(source.heights.begin(), source.heights.end(), 2);
            Check(world.LiveTerrain().CreateLandscape(1, source, error) &&
                      world.LiveTerrain().Wait(1, error),
                  error);
            Check(world.LiveTerrain().Brush(1, {0, 1, 0}, {VolumeBrushKind::Remove, 3, 1}, error) &&
                      world.LiveTerrain().Wait(1, error),
                  error);
            std::string state;
            Check(world.CaptureRuntimeTerrain(1, state, error), error);
            std::cout << "METRIC complete_runtime_source_bytes=" << state.size() << '\n';
            Check(state.size() > 2 * 1024 * 1024 && state.size() <= 8 * 1024 * 1024,
                  "representative runtime-created source exercises the existing capture budget");
            Write(root / "terrain.state", state);
            Check(WorldPersistence::Capture(world, saved, error),
                  "capture real world save: " + error);
            SaveArchive archive;
            archive(saved);
            Write(root / "world.state", archive.bytes);
        } else {
            SaveArchive archive(Read(root / "world.state"));
            archive(saved);
            archive.Finish();
        }
        Scene restored;
        Check(WorldPersistence::SceneFrom(saved, restored, error), error);
        world.Destroy();
        Check(world.Build(restored, &resources, error, nullptr, nullptr, true), error);
        Check(WorldPersistence::Prepare(world, error), error);
        const bool restoredState = WorldPersistence::Restore(world, saved, error);
        Check(restoredState,
              "a successfully captured source must restore through the ordinary world path: " +
                  error);
        std::string after;
        Check(world.CaptureRuntimeTerrain(1, after, error) && after == Read(root / "terrain.state"),
              "cold restoration preserves all samples and the excavation journal exactly");
        const auto hit = world.Physics().Raycast({0, 8, 0}, {0, -1, 0}, 20);
        Check(hit.hit && hit.point.y < 0 && world.LiveTerrain().Status(1)->chunks == 1024,
              "restored source publishes real excavation and all physical chunks");
        const auto count = world.Physics().AliveBodyCount();
        Check(!world.LiveTerrain().Restore(1, std::string(TerrainV2::MaximumSnapshotBytes + 1, 'x'),
                                           error) &&
                  world.Physics().AliveBodyCount() == count,
              "oversized direct restore rejects before changing published support");
        const auto safeHit = world.Physics().Raycast({0, 8, 0}, {0, -1, 0}, 20);
        Check(safeHit.hit && safeHit.point == hit.point,
              "oversized continuation leaves exact current collision intact");
        world.Destroy();
        resources.Shutdown();
        Region(root / "regional", read);
        if (argc == 1)
            fs::remove_all(root);
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "FAIL " << exception.what() << " after " << checks << " checks\n";
        return 1;
    }
}
