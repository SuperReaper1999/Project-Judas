#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "SceneSerialization.h"
#include "WorldStreaming.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainBrush.h"
#include "terrain/TerrainPlanetBinding.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <tuple>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
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
                throw std::runtime_error(region.id + ": " + region.error);
        if (done())
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("planet regional workflow timed out");
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
                      ("planet-terrain-region-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::string error;
        Project project;
        PlanetSettings planet;
        planet.radius = 1000;
        planet.collisionSpacing = 1;
        planet.collisionRadius = 12;
        planet.center = {1e12, -1e12, 1e12};
        planet.rotation = glm::angleAxis(.35, glm::normalize(glm::dvec3(1, 2, 3)));
        TerrainV2::Source source;
        TerrainV2::PlanetChart chart;
        Check(TerrainV2::CreatePlanetChart(PlanetSurface(planet), {2, 4, 8, 8}, 128, source, chart,
                                           error),
              error);
        source.version = 3;
        source.chart = chart;
        const auto anchor = planet.center + planet.rotation * chart.ModelOrigin();
        if (!read) {
            Check(!fs::exists(root), "fresh fixture required");
            Check(Project::CreateNew(root.string(), "Planet regional terrain", project, error),
                  error);

            Check(project.Save(error), error);
            std::vector<uint8_t> bytes;
            Check(TerrainV2::EncodeAsset(source, bytes, error), error);
            Write(root / "Assets/patch.judasterrain", {bytes.begin(), bytes.end()});
        } else
            Check(project.Load((root / "Planet_regional_terrain.judasproj").string(), error),
                  error);
        AssetDatabase assets;
        assets.Scan(project.RootDir(), project.AssetsDir());
        AssetRecord asset;
        if (const auto *known = assets.FindByRelativePath("Assets/patch.judasterrain"))
            asset = *known;
        else
            Check(assets.Track((root / "Assets/patch.judasterrain").string(), asset, error), error);
        if (!read) {
            Scene descriptor;
            descriptor.CreateObject("Absolute planetary producer").planet = planet;
            Check(SaveSceneToFile(descriptor, (root / "Scenes/planet.judas").string(), error),
                  error);
            Scene region;
            region.CreateObject("Qualified-reference placeholder");
            auto &owner = region.CreateObject("Regional geographic terrain");
            owner.body = SceneBodyComponent{};
            owner.landscape = SceneLandscapeSettings{asset.id, 1};
            owner.transform = TerrainV2::PlanetTerrainPlacement(planet, chart, anchor);
            Check(SaveSceneToFile(region, (root / "Scenes/patch.judas").string(), error), error);
        }
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        RuntimeWorld world;
        world.ConfigureProject(project.Settings());
        Scene bootstrap;
        bootstrap.Settings().worldOrigin = anchor;
        Check(world.Build(bootstrap, &resources, error), error);
        WorldManifest manifest;
        manifest.unitsPerFrame = 1;
        manifest.pendingBytes = manifest.liveBytes = 64 * 1024 * 1024;
        WorldRegion descriptor;
        descriptor.id = "planet";
        descriptor.scene = "Scenes/planet.judas";
        // Region placement must not transform an absolute planet descriptor.
        descriptor.origin = anchor;
        descriptor.rotation = glm::angleAxis(.2f, glm::vec3(0, 1, 0));
        manifest.regions.emplace(descriptor.id, descriptor);
        WorldRegion patch;
        patch.id = "patch";
        patch.scene = "Scenes/patch.judas";
        patch.origin = anchor;
        patch.halfExtents = {80, 32, 80};
        patch.dependencies = {"planet"};
        manifest.regions.emplace(patch.id, patch);
        manifest.references.push_back({"patch", "terrainPlanet", "planet", 2, 1, true});
        Check(ValidateWorldManifest(manifest, project, error), error);
        WorldStreaming stream(world, resources, project, manifest);
        if (read) {
            SaveArchive saved(Read(root / "regions.state"));
            stream.Persist(saved, 9);
            saved.Finish();
        }
        auto load = [&] {
            const auto token = stream.Request("patch", false, error);
            Check(token != 0, error);
            Pump(stream, [&] { return stream.Status(token)->state == "active"; });
            const auto id = stream.Resolve("patch", 2), parent = stream.Resolve("planet", 1);
            const auto *owner = world.RuntimeDefinition(id);
            Check(owner && owner->landscape && owner->landscape->planet == parent,
                  "regional source resolves its actual planetary producer");
            Check(world.RuntimeDefinition(parent)->transform.position == glm::vec3(0) &&
                      world.RuntimeDefinition(parent)->transform.rotation == glm::quat(1, 0, 0, 0),
                  "absolute planet ignores region placement and retains its descriptor frame");
            bool ready = false;
            for (unsigned i = 0; i < 128 && !ready; ++i)
                ready = world.PlanetCollision(parent, anchor, error);
            Check(ready, error);
            return std::make_pair(token, id);
        };
        auto height = [&] {
            auto point = [&](double y) {
                return glm::vec3(planet.center + planet.rotation * chart.ModelPoint({0, y, 0}) -
                                 world.Settings().worldOrigin);
            };
            const auto a = point(8), b = point(-8);
            auto hit = world.Physics().Raycast(a, glm::normalize(b - a), 16);
            Check(hit.hit, "regional native surface has real support");
            const auto absolute = glm::dvec3(hit.point) + world.Settings().worldOrigin;
            const auto model = glm::inverse(planet.rotation) * (absolute - planet.center);
            return glm::length(model) - planet.radius;
        };
        auto [token, id] = load();
        if (!read) {
            Check(std::abs(height()) < .003, "initial streamed ground matches planet");
            const auto center =
                glm::vec3(planet.center + planet.rotation * chart.ModelPoint({0, -.5, 0}) -
                          world.Settings().worldOrigin);
            Check(world.LiveTerrain().Brush(id, center, {TerrainV2::Edit::Kind::Remove, {}, 3}, error) &&
                      world.LiveTerrain().Wait(id, error),
                  error);
            Check(height() < -2.5, "streamed excavation removes the real base planet");
            Check(!world.ValidateEntityDestruction(stream.Resolve("planet", 1), error),
                  "external native patch protects producer lifetime");
            const auto oldOrigin = world.Settings().worldOrigin;
            Check(world.RequestOrigin(anchor + glm::dvec3(128, 0, 0), error) &&
                      world.CommitOrigin(),
                  error);
            // This harness owns the coordinator directly, rather than through
            // SceneControl; deliver the same notification as that owner.
            stream.ShiftOrigin(world.Settings().worldOrigin - oldOrigin);
            stream.Advance(false);
            Check(height() < -2.5, "regional geographic support survives origin transaction");
        } else
            Check(height() < -2.5, "cold regional collision retains excavation");
        std::string state;
        Check(world.CaptureRuntimeTerrain(id, state, error), error);
        if (read)
            Check(state == Read(root / "terrain.state"), "cold chart and edit state are exact");
        else
            Write(root / "terrain.state", state);
        Check(stream.Release(token) && stream.Unload("patch"), "release native patch region");
        Pump(stream, [&] {
            for (const auto &region : stream.Regions())
                if (region.id == "patch")
                    return region.state == "unloaded";
            return false;
        });
        Check(world.LiveTerrain().Owners().empty(), "all geographic sources retired");
        if (!read) {
            std::tie(token, id) = load();
            Check(height() < -2.5, "revisit restores physical excavation before publication");
            Check(world.CaptureRuntimeTerrain(id, state, error) &&
                      state == Read(root / "terrain.state"),
                  "revisit preserves exact native state");
            Check(stream.Release(token) && stream.Unload("patch"), "release revisited patch");
            // An inactive-region archive contains suspended content. Active
            // regions additionally require their world snapshot, covered by
            // PlanetComponentTests rather than this coordinator-only harness.
            Pump(stream, [&] {
                for (const auto &region : stream.Regions())
                    if (region.state != "unloaded")
                        return false;
                return stream.SaveReady() && world.LiveTerrain().Owners().empty();
            });
            SaveArchive saved;
            stream.Persist(saved, 9);
            Write(root / "regions.state", saved.bytes);
        }
        if (argc == 1)
            fs::remove_all(root);
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "FAIL " << exception.what() << " after " << checks << " checks\n";
        return 1;
    }
}
