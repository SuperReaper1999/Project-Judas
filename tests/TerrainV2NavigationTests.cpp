#include "ProjectExporter.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "SceneSerialization.h"
#include "WorldPersistence.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainBrush.h"
#include "terrain/TerrainNavigation.h"
#include "terrain/TerrainPreparation.h"
#include <algorithm>
#include <chrono>
#include <cmath>
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
    Check(bool(file.write(bytes.data(), bytes.size())), "write native test fixture");
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
                      ("terrain-native-nav-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::string error;
        if (!read) {
            Check(!fs::exists(root), "fixture is fresh");
            fs::create_directories(root / "Assets");
            auto source = TerrainV2::Create(33, 33, .5f);
            source.bottom = -4;
            source.top = 8;
            std::fill(source.heights.begin(), source.heights.end(), 2);
            std::vector<uint8_t> bytes;
            Check(TerrainV2::EncodeAsset(source, bytes, error), "encode native ground");
            Write(root / "Assets/ground.judasterrain", {bytes.begin(), bytes.end()});
        }
        AssetDatabase assets;
        assets.Scan(root.string(), (root / "Assets").string());
        AssetRecord asset;
        if (const auto *known = assets.FindByRelativePath("Assets/ground.judasterrain"))
            asset = *known;
        else
            Check(assets.Track((root / "Assets/ground.judasterrain").string(), asset, error),
                  "track source");
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        Scene scene;
        auto owner = scene.CreateObject("Navigable native landscape");
        owner.body = SceneBodyComponent{};
        owner.landscape = SceneLandscapeSettings{asset.id};
        owner.navigationSurface = NavigationSurfaceSettings{};
        owner.navigationSurface->halfExtents = {9, 8, 9};
        owner.navigationSurface->minRegion = 0;
        scene.Objects().front() = owner;
        auto &agent = scene.CreateObject("Route consumer");
        agent.navigationAgent = NavigationAgentSettings{};
        agent.transform.position = {-5, 2, 0};
        RuntimeWorld world;
        world.legacyGameplay = false;
        Check(world.Build(scene, &resources, error),
              "native navigation ready at normal build: " + error);
        const auto height = [&](glm::vec3 point) { return world.Navigation().Sample(point, .3f); };
        Check(world.LiveTerrain().Status(1)->navigation &&
                  world.Navigation().Statistics().surfaces == 1 && height({4, 2, 0}) &&
                  !height({4, .5f, 0}),
              "navigation uses real source height, not the half-metre identity Box");
        if (read) {
            SaveArchive archive(Read(root / "world.state"));
            std::map<std::string, SaveChunk> chunks;
            archive(chunks);
            archive.Finish();
            Scene saved;
            Check(WorldPersistence::SceneFrom(chunks, saved, error), "parse ordinary saved scene");
            world.Destroy();
            Check(world.Build(saved, &resources, error, nullptr, nullptr, true),
                  "fresh process builds native saved world");
            const bool prepared = WorldPersistence::Prepare(world, error);
            Check(prepared,
                  "normal restore preparation initializes required participants: " + error);
            const bool restored = WorldPersistence::Restore(world, chunks, error);
            Check(restored,
                  "ordinary world participants restore native terrain and navigation: " + error);
            const auto pose = world.RuntimeDefinition(1)->transform;
            Check(!height(pose.position + pose.rotation * glm::vec3(0, 2, 0)) &&
                      height(pose.position + pose.rotation * glm::vec3(5, 2, 0)),
                  "cold navigation retains excavation and its rotated/origin-shifted frame");
        } else {
            NavigationGeometry actual;
            Check(CollectNavigationGeometry(scene, owner, ProjectNavigation{}, actual, error,
                                            &assets) &&
                      actual.triangles.size() == 32 * 32 * 6,
                  "offline scene collection also uses the exact physical source triangles");
            world.Navigation().Update(world, 0);
            Check(world.Navigation().SetDestination(2, {5, 2, 0}),
                  "real scene agent accepts a destination");
            world.Navigation().Update(world, 0);
            const auto oldAgentDistance = world.Navigation().Agent(2)->path.distance;
            auto straight = world.Navigation().FindPath({-5, 2, 0}, {5, 2, 0});
            Check(straight.status == NavigationPath::Status::Complete && straight.distance < 10.1f,
                  "original planar route crosses the uncut source");
            Check(world.LiveTerrain().Brush(1, {0, 2, 0}, {VolumeBrushKind::Remove, 2, 1}, error),
                  "queue real excavation across all four chunk corners");
            Check(height({0, 2, 0}) && world.Physics().Raycast({0, 4, 0}, {0, -1, 0}, 3).hit,
                  "pending edit retains old complete collision and navigation");
            Check(world.LiveTerrain().Wait(1, error),
                  "publish new collision and navigation: " + error);
            const auto physical = world.Physics().Raycast({0, 4, 0}, {0, -1, 0}, 8);
            Check(physical.hit && physical.point.y < 1 && !height({0, 2, 0}),
                  "both real collision and navigation lose the removed upper surface");
            auto around = world.Navigation().FindPath({-5, 2, 0}, {5, 2, 0});
            Check(around.status == NavigationPath::Status::Complete &&
                      around.distance > straight.distance + .1f,
                  "new route genuinely goes around excavation");
            world.Navigation().Update(world, 0);
            Check(world.Navigation().Agent(2)->path.status == NavigationPath::Status::Complete &&
                      world.Navigation().Agent(2)->path.distance > oldAgentDistance + .1f,
                  "existing agent immediately replans against the published excavation");
            Check(world.SetColliderEnabled(1, false) && !height({4, 2, 0}) &&
                      world.Navigation().Statistics().surfaces == 0,
                  "disabling actual terrain also retires its navigation");
            Check(world.SetColliderEnabled(1, true) && height({4, 2, 0}) && !height({0, 2, 0}),
                  "reenabling source collision restores its matching navigation");
            Check(world.Navigation().Errors().empty(),
                  "normal navigation update needs no static asset");
            const auto revision = world.Navigation().Revision();
            TerrainV2::SurfaceBrush paint;
            paint.kind = TerrainV2::SurfaceBrush::Kind::Paint;
            paint.layer = 2;
            paint.strength = 1;
            Check(world.LiveTerrain().Surface(1, {4, 2, 0}, paint, error) &&
                      world.LiveTerrain().Wait(1, error) &&
                      world.Navigation().Revision() == revision,
                  "paint-only publication reuses navigation instead of rebaking it");
            auto pose = world.RuntimeDefinition(1)->transform;
            pose.position = {20, 5, 10};
            pose.rotation = glm::angleAxis(.4f, glm::normalize(glm::vec3(1, 2, 0)));
            Check(world.SetRuntimeTransform(1, pose) &&
                      height(pose.position + pose.rotation * glm::vec3(5, 2, 0)) &&
                      !height(pose.position + pose.rotation * glm::vec3(0, 2, 0)),
                  "native local navigation follows rigid terrain placement");
            Check(world.RequestOrigin({512, 0, 0}, error) && world.CommitOrigin(),
                  "normal world-origin transaction supports native navigation");
            pose = world.RuntimeDefinition(1)->transform;
            Check(height(pose.position + pose.rotation * glm::vec3(5, 2, 0)) &&
                      !height(pose.position + pose.rotation * glm::vec3(0, 2, 0)),
                  "origin shift preserves local path/collision relationship");
            std::map<std::string, SaveChunk> chunks;
            Check(WorldPersistence::Capture(world, chunks, error),
                  "capture actual world participants: " + error);
            SaveArchive saved;
            saved(chunks);
            Write(root / "world.state", saved.bytes);

            std::string excavated;
            Check(world.CaptureRuntimeTerrain(1, excavated, error),
                  "capture current source revision");
            Check(world.LiveTerrain().Brush(1, pose.position + pose.rotation * glm::vec3(0, 2, 0),
                                            {VolumeBrushKind::Remove, 32, 1}, error) &&
                      world.LiveTerrain().Wait(1, error),
                  "remove the entire native surface");
            Check(world.Navigation().Statistics().polygons == 0 &&
                      !height(pose.position + pose.rotation * glm::vec3(5, 2, 0)) &&
                      world.Physics().AliveBodyCount() == 1,
                  "wholly excavated terrain publishes empty collision and empty navigation");
            Check(world.LiveTerrain().Restore(1, excavated, error) &&
                      height(pose.position + pose.rotation * glm::vec3(5, 2, 0)),
                  "restoring removed material restores matching navigation");
            RuntimeWorld staged;
            staged.legacyGameplay = false;
            Check(staged.Build(Scene{}, &resources, error), "build independent regional owner");
            TerrainV2::PreparedAsset product;
            TerrainV2::NavigationInput nav;
            nav.settings = *owner.navigationSurface;
            Check(TerrainV2::PrepareAsset((root / "Assets/ground.judasterrain").string(), asset.id,
                                          product, error, {}, &nav),
                  "worker product contains complete source-owned navigation");
            Check(bool(product.geometry.navigationCache),
                  "native worker product includes its complete private Detour cache");
            NavigationSystem isolated;
            std::shared_ptr<PreparedNavigationSurface> cache;
            const auto coordinator = std::this_thread::get_id();
            std::thread::id preparationThread;
            auto cacheJob = jobs.Submit([&](JobContext &) {
                preparationThread = std::this_thread::get_id();
                cache = NavigationSystem::PrepareSurface(product.geometry.navigation, error);
            });
            jobs.Wait(cacheJob);
            jobs.Forget(cacheJob);
            Check(cache && preparationThread != coordinator && isolated.Statistics().surfaces == 0,
                  "actual worker constructs a private cache without world publication");
            auto sameBytes = std::make_shared<NavigationData>(*product.geometry.navigation);
            const auto beforeCacheRevision = isolated.Revision();
            Check(!isolated.LoadSurface(90, {}, sameBytes, error, true, cache.get()) &&
                      isolated.Revision() == beforeCacheRevision,
                  "cache from a different source identity rejects without changing live state");
            auto placed = SceneTransform{};
            placed.position = {50, 0, 0};
            Check(isolated.LoadSurface(90, placed, product.geometry.navigation, error, false,
                                       cache.get()) &&
                      !isolated.Sample({54, 2, 0}, .3f),
                  "single-consumed worker cache remains invisible while staged");
            isolated.PublishSurfaces({90});
            Check(isolated.Sample({54, 2, 0}, .3f) && !isolated.Sample({4, 2, 0}, .3f),
                  "cache installs in its final owner frame without rebuilding geometry");
            const auto publishedRevision = isolated.Revision();
            Check(!isolated.LoadSurface(90, {}, product.geometry.navigation, error, true,
                                        cache.get()) &&
                      isolated.Revision() == publishedRevision && isolated.Sample({54, 2, 0}, .3f),
                  "double consumption rejects and preserves the previous live routes");
            unsigned cancellationPoints = 0;
            Check(!NavigationSystem::PrepareSurface(product.geometry.navigation, error,
                                                    [&] { return ++cancellationPoints > 2; }) &&
                      cancellationPoints == 3 && isolated.Sample({54, 2, 0}, .3f),
                  "mid-cache cancellation cleans private ownership and leaves live routes intact");
            auto invalidCache = std::make_shared<NavigationData>(*product.geometry.navigation);
            invalidCache->layers.front()[0] ^= 1;
            Check(!NavigationSystem::PrepareSurface(invalidCache, error) &&
                      isolated.Sample({54, 2, 0}, .3f),
                  "invalid prepared layer rejects without replacing live routes");
            Check(!NavigationSystem::PrepareSurface({}, error),
                  "missing cache input rejects explicitly");
            auto regional = owner;
            auto mismatched = regional;
            mismatched.id = staged.AllocateRuntimeEntityId();
            mismatched.navigationSurface->cellSize *= 2;
            Check(!staged.StageRegionObject(mismatched, mismatched, error, "wrong nav", &product) &&
                      !product.consumed && staged.Physics().AliveBodyCount() == 0,
                  "precomputed navigation cannot use mismatched owner settings");
            regional.id = staged.AllocateRuntimeEntityId();
            Check(staged.StageRegionObject(regional, regional, error, "native navigation",
                                           &product) &&
                      staged.Navigation().Statistics().surfaces == 0,
                  "private regional staging exposes neither navigation nor collision");
            staged.PublishRegion({regional.id});
            Check(staged.Navigation().Sample({4, 2, 0}, .3f) &&
                      staged.Navigation().Statistics().surfaces == 1,
                  "ordinary regional publication exposes complete navigation");
            staged.RemoveRegionObject(regional.id);
            Check(staged.Navigation().Statistics().surfaces == 0 &&
                      staged.Physics().AliveBodyCount() == 0,
                  "regional removal retires all terrain and navigation ownership");

            TerrainV2::NavigationInput empty;
            empty.settings = nav.settings;
            std::shared_ptr<const NavigationData> data;
            Check(TerrainV2::PrepareNavigation(empty, data, error) && data->layers.empty(),
                  "wholly empty terrain is a valid empty navigation revision");
            const auto retained = data;
            Check(!TerrainV2::PrepareNavigation(nav, data, error, [] { return true; }) &&
                      data == retained,
                  "cancelled navigation preparation preserves the previous complete product");
            auto oversized = empty;
            oversized.settings.halfExtents = glm::vec3(1e30f);
            Check(!TerrainV2::PrepareNavigation(oversized, data, error) && data == retained,
                  "oversized coverage rejects before integer conversion or allocation");
            oversized.settings.cellSize = 0;
            Check(!TerrainV2::PrepareNavigation(oversized, data, error) && data == retained,
                  "invalid cell size rejects without replacing a complete product");
            auto bad = scene;
            bad.Find(1)->navigationSurface->includeDynamic = true;
            RuntimeWorld rejected;
            rejected.legacyGameplay = false;
            Check(!rejected.Build(bad, &resources, error),
                  "native ownership rejects misleading dynamic-source configuration");
            auto exportScene = scene;
            Check(ValidateSceneContent(exportScene, assets, ProjectClassification{},
                                       ProjectNavigation{}, error),
                  "export validation accepts native navigation without a phantom baked asset");
            exportScene.Find(1)->navigationSurface->profile = 63;
            Check(!ValidateSceneContent(exportScene, assets, ProjectClassification{},
                                        ProjectNavigation{}, error) &&
                      error.find("unknown project agent profile") != std::string::npos,
                  "export rejects an unknown native profile with an actionable error");
            exportScene = scene;
            exportScene.Find(1)->navigationSurface->halfExtents = glm::vec3(1e30f);
            Check(!ValidateSceneContent(exportScene, assets, ProjectClassification{},
                                        ProjectNavigation{}, error) &&
                      error.find("navigation") != std::string::npos,
                  "export preflights native navigation budgets before packaging");
            // A second authored source exercises a real through-tunnel, not
            // merely an open pit or navigation painted onto a flat floor.
            auto hillside = TerrainV2::Create(33, 33, .5f);
            hillside.bottom = -4;
            hillside.top = 8;
            for (unsigned z = 0; z < 33; ++z)
                for (unsigned x = 0; x < 33; ++x) {
                    const float pz = (float(z) - 16) * .5f;
                    hillside.heights[z * 33 + x] = .8f + 3 * std::exp(-pz * pz / 8);
                }
            std::vector<uint8_t> hillBytes;
            Check(TerrainV2::EncodeAsset(hillside, hillBytes, error),
                  "encode actual hillside source");
            Write(root / "Assets/hillside.judasterrain", {hillBytes.begin(), hillBytes.end()});
            AssetRecord hillAsset;
            Check(assets.Track((root / "Assets/hillside.judasterrain").string(), hillAsset, error),
                  "track independent authored hillside");
            auto hills = scene;
            hills.Find(1)->landscape->asset = hillAsset.id;
            RuntimeWorld tunnel;
            tunnel.legacyGameplay = false;
            Check(tunnel.Build(hills, &resources, error), "build hillside navigation");
            Check(!tunnel.Navigation().Sample({0, .5f, 0}, .3f),
                  "solid hill has no invented underground navigation");
            for (float z = 7; z >= -7; z -= 1) {
                Check(tunnel.LiveTerrain().Brush(1, {0, 1.9f, z},
                                                 {VolumeBrushKind::Remove, 1.5f, 1}, error) &&
                          tunnel.LiveTerrain().Wait(1, error),
                      "advance real cross-chunk tunnel and navigation together");
            }
            const auto floor = tunnel.Navigation().Sample({0, .5f, 0}, .3f);
            const auto roof = tunnel.Navigation().Sample({0, 3.8f, 0}, .3f);
            const auto route = tunnel.Navigation().FindPath({0, .5f, 6}, {0, .5f, -6});
            Check(floor && roof && roof->position.y - floor->position.y > 2.5f,
                  "cave floor and original hilltop coexist as distinct walkable layers");
            Check(route.status == NavigationPath::Status::Complete &&
                      std::all_of(route.corners.begin(), route.corners.end(),
                                  [](const auto &corner) { return corner.position.y < 1.1f; }) &&
                      !tunnel.Physics().Raycast({0, 1.9f, -7}, {0, 0, 1}, 14).hit,
                  "route genuinely traverses under the hill across chunk boundaries");
        }
        world.Destroy();
        resources.Shutdown();
        if (argc == 1)
            fs::remove_all(root);
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
