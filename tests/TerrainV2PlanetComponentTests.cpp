#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "WorldPersistence.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainBrush.h"
#include "terrain/TerrainPlanetBinding.h"
#include "terrain/TerrainPreparation.h"
#include "terrain/TerrainSolid.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
}
std::string Read(const std::filesystem::path &p) {
    std::ifstream file(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
void Write(const std::filesystem::path &p, const std::string &bytes) {
    std::ofstream file(p, std::ios::binary);
    Check(bool(file.write(bytes.data(), bytes.size())), "write fixture");
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
                      ("terrain-planet-component-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        PlanetSettings planet;
        planet.radius = 1000;
        planet.collisionSpacing = 1;
        planet.collisionRadius = 12;
        planet.center = {1e12, -1e12, 1e12};
        planet.rotation = glm::angleAxis(.35, glm::normalize(glm::dvec3(1, 2, 3)));
        TerrainV2::Source source;
        TerrainV2::PlanetChart chart;
        std::string error;
        Check(TerrainV2::CreatePlanetChart(PlanetSurface(planet), {2, 4, 8, 8}, 128, source, chart,
                                           error),
              error);
        source.version = 3;
        source.chart = chart;
        if (!read) {
            Check(!fs::exists(root), "fixture fresh");
            fs::create_directories(root / "Assets");
            std::vector<uint8_t> bytes;
            Check(TerrainV2::EncodeAsset(source, bytes, error), error);
            Check(bytes[8] == 2, "projected envelope explicitly rejects older flat-only readers");
            TerrainV2::Source decoded;
            Check(
                TerrainV2::DecodeAsset({reinterpret_cast<const char *>(bytes.data()), bytes.size()},
                                       decoded, error) &&
                    TerrainV2::Serialize(decoded) == TerrainV2::Serialize(source),
                "native projected metadata/sample roundtrip");
            auto wrong = bytes;
            wrong[8] = 1;
            Check(!TerrainV2::DecodeAsset(
                      {reinterpret_cast<const char *>(wrong.data()), wrong.size()}, decoded, error),
                  "flat envelope cannot silently interpret projected geometry");
            auto bad = source;
            bad.heights[0] = 1;
            Check(!TerrainV2::Validate(bad, error), "manual edits cannot detach the protected rim");
            Write(root / "Assets/patch.judasterrain", {bytes.begin(), bytes.end()});
        }
        AssetDatabase assets;
        assets.Scan(root.string(), (root / "Assets").string());
        AssetRecord asset;
        if (const auto *known = assets.FindByRelativePath("Assets/patch.judasterrain"))
            asset = *known;
        else
            Check(assets.Track((root / "Assets/patch.judasterrain").string(), asset, error), error);
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        Scene scene;
        scene.Settings().worldOrigin = planet.center + planet.rotation * chart.ModelOrigin();
        auto &patch = scene.CreateObject("Geographic patch before its producer");
        patch.body = SceneBodyComponent{};
        patch.landscape = SceneLandscapeSettings{asset.id, 3};
        patch.transform =
            TerrainV2::PlanetTerrainPlacement(planet, chart, scene.Settings().worldOrigin);
        scene.CreateObject("Unused ordering control");
        auto &parent = scene.CreateObject("Absolute planet");
        parent.planet = planet;
        RuntimeWorld world;

        Check(world.Build(scene, &resources, error),
              "normal scene builds projected assets independent of object order: " + error);
        auto point = [&](glm::dvec3 value) {
            const auto pose = world.RuntimeDefinition(1)->transform;
            return pose.position + pose.rotation * glm::vec3(chart.LocalPoint(value));
        };
        auto height = [&] {
            auto a = point({0, 8, 0}), b = point({0, -8, 0});
            auto hit = world.Physics().Raycast(a, glm::normalize(b - a), 16);
            Check(hit.hit, "actual physical surface exists");
            const auto pose = world.RuntimeDefinition(1)->transform;
            return chart.ChartPoint(glm::inverse(pose.rotation) * (hit.point - pose.position)).y;
        };
        if (read) {
            SaveArchive archive(Read(root / "world.state"));
            std::map<std::string, SaveChunk> saved;
            archive(saved);
            archive.Finish();
            Scene restored;
            Check(WorldPersistence::SceneFrom(saved, restored, error), error);
            world.Destroy();
            Check(world.Build(restored, &resources, error, nullptr, nullptr, true),
                  "cold scene build: " + error);
            Check(WorldPersistence::Prepare(world, error), error);
            Check(WorldPersistence::Restore(world, saved, error),
                  "cold native/cutout reconstruction: " + error);
            std::string captured;
            Check(world.CaptureRuntimeTerrain(1, captured, error) &&
                      captured == Read(root / "terrain.state"),
                  "cold source samples, chart, paint and excavation are exact");
            Check(height() < -2.5, "cold collision exposes actual pit, not restored base ground");
        } else {
            bool ready = false;
            for (unsigned i = 0; i < 128 && !ready; ++i)
                ready = world.PlanetCollision(3, scene.Settings().worldOrigin, error);
            Check(ready, "base physical interest ready: " + error);
            Check(std::abs(height()) < .003, "initial curved surface matches geographic placement");
            Check(world.LiveTerrain().Brush(1, point({0, -.5, 0}), {TerrainV2::Edit::Kind::Remove, {}, 3},
                                            error) &&
                      world.LiveTerrain().Wait(1, error),
                  "normal public excavation with world-metric radius: " + error);
            Check(height() < -2.5, "new collision pit remains unblocked by the base planet");
            auto solids = world.LiveTerrain().Solids(point({-5, -5, -5}), point({5, 5, 5}));
            Check(!solids.empty() && !solids.front().Contains(point({0, -2, 0})) &&
                      solids.front().Contains(point({0, -4, 0})),
                  "curved source occupancy agrees with excavation");
            const auto bodies = world.LiveTerrain().Bodies(1);
            TerrainV2::SurfaceBrush paint;
            paint.kind = TerrainV2::SurfaceBrush::Kind::Paint;
            paint.radius = 3;
            paint.layer = 2;
            Check(
                world.LiveTerrain().Surface(1, point({10, 0, 10}), paint, error) &&
                    world.LiveTerrain().Wait(1, error) &&
                    [&] {
                        const auto after = world.LiveTerrain().Bodies(1);
                        return after.size() == bodies.size() &&
                               std::equal(after.begin(), after.end(), bodies.begin(),
                                          [](auto a, auto b) {
                                              return a.id == b.id && a.world == b.world;
                                          });
                    }(),
                "ordinary surface painting preserves physical identities");
            auto pose = world.RuntimeDefinition(1)->transform;
            pose.position.x += 1;
            Check(!world.SetRuntimeTransform(1, pose),
                  "geographic sources cannot silently detach from their planet");
            Check(!world.ValidateEntityDestruction(3, error),
                  "planet lifetime respects native patch dependencies");
            Check(
                world.RequestOrigin(world.Settings().worldOrigin + glm::dvec3(256, 0, 0), error) &&
                    world.CommitOrigin(),
                error);
            Check(height() < -2.5,
                  "origin transaction preserves both native and base replacement support");
            std::string captured;
            Check(world.CaptureRuntimeTerrain(1, captured, error), error);
            Write(root / "terrain.state", captured);
            std::map<std::string, SaveChunk> saved;
            Check(WorldPersistence::Capture(world, saved, error), error);
            SaveArchive archive;
            archive(saved);
            Write(root / "world.state", archive.bytes);
            Check(world.DestroyEntity(1, &error), "remove geographic source: " + error);
            auto a = glm::vec3(planet.center + planet.rotation * chart.ModelPoint({0, 8, 0}) -
                               world.Settings().worldOrigin);
            auto b = glm::vec3(planet.center + planet.rotation * chart.ModelPoint({0, -8, 0}) -
                               world.Settings().worldOrigin);
            auto hit = world.Physics().Raycast(a, glm::normalize(b - a), 16);
            Check(hit.hit && world.EntityIdOfBody(hit.body) == 3,
                  "source removal restores real planet ground");
            Check(world.ValidateEntityDestruction(3, error),
                  "removed patch releases its planet lifetime dependency");
            const auto baseCount = world.Physics().AliveBodyCount();
            SceneObject regional = scene.Objects().front();
            regional.id = world.AllocateRuntimeEntityId();
            regional.transform =
                TerrainV2::PlanetTerrainPlacement(planet, chart, world.Settings().worldOrigin);
            Check(world.StageRegionObject(regional, regional, error),
                  "projected terrain stages privately: " + error);
            auto baseHit = world.Physics().Raycast(a, glm::normalize(b - a), 16);
            Check(baseHit.hit && world.EntityIdOfBody(baseHit.body) == 3,
                  "private reservation keeps existing planet support visible");
            SceneObject overlap = regional;
            overlap.id = world.AllocateRuntimeEntityId();
            const auto stagedCount = world.Physics().AliveBodyCount();
            Check(!world.StageRegionObject(overlap, overlap, error) &&
                      !world.HasRuntimeTerrain(overlap.id) &&
                      world.Physics().AliveBodyCount() == stagedCount,
                  "overlapping private footprint rejects without leaking generated collision");
            Check(!world.ValidateEntityDestruction(3, error),
                  "private reservations protect their planetary producer");
            EntityPhysicalState detached;
            Check(world.GetEntityState(regional.id, detached), "private geographic state exists");
            detached.position.x += 1;
            Check(!world.SetEntityState(regional.id, detached),
                  "private restoration cannot detach geographic collision from its coverage");
            world.PublishRegion({regional.id});
            auto nativeHit = world.Physics().Raycast(a, glm::normalize(b - a), 16);
            Check(nativeHit.hit && world.EntityIdOfBody(nativeHit.body) == regional.id,
                  "publication switches support to the real projected source");
            world.HideRegion({regional.id});
            baseHit = world.Physics().Raycast(a, glm::normalize(b - a), 16);
            Check(baseHit.hit && world.EntityIdOfBody(baseHit.body) == 3,
                  "private suspension restores base coverage before source retirement");
            world.PublishRegion({regional.id});
            nativeHit = world.Physics().Raycast(a, glm::normalize(b - a), 16);
            Check(nativeHit.hit && world.EntityIdOfBody(nativeHit.body) == regional.id,
                  "republishing reinstates native support");
            world.RemoveRegionObject(regional.id);
            baseHit = world.Physics().Raycast(a, glm::normalize(b - a), 16);
            Check(baseHit.hit && world.EntityIdOfBody(baseHit.body) == 3 &&
                      world.Physics().AliveBodyCount() == baseCount &&
                      world.ValidateEntityDestruction(3, error),
                  "regional removal releases its coverage reservation, bodies and dependency");
        }
        world.Destroy();
        resources.Shutdown();
        if (argc == 1)
            fs::remove_all(root);
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "FAIL " << exception.what() << " after " << checks << " checks\n";
        return 1;
    }
}
