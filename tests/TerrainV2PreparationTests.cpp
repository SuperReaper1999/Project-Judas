#include "CollisionAsset.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "WorldStreaming.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainPreparation.h"
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
} // namespace
int main() {
    try {
        namespace fs = std::filesystem;
        const auto root =
            fs::temp_directory_path() /
            ("terrain-v2-prepared-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root / "Assets");
        auto source = TerrainV2::Create(17, 17, 1);
        std::fill(source.heights.begin(), source.heights.end(), 1);
        std::string error;
        std::vector<uint8_t> bytes;
        Check(TerrainV2::EncodeAsset(source, bytes, error), "encode native source");
        const auto path = root / "Assets/ground.judasterrain";
        {
            std::ofstream file(path, std::ios::binary);
            file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        }
        AssetDatabase assets;
        assets.Scan(root.string(), (root / "Assets").string());
        AssetRecord record;
        Check(assets.Track(path.string(), record, error), "track native asset");
        JobSystem jobs(2);
        TerrainV2::PreparedAsset prepared;
        auto job = jobs.Submit(
            [&](JobContext &context) {
                if (!TerrainV2::PrepareAsset(path.string(), record.id, prepared, error,
                                             [&] { return context.CancelRequested(); }))
                    context.SetError(error);
            },
            JobPriority::Normal, "Native terrain handoff test");
        jobs.Wait(job);
        Check(jobs.StateOf(job) == JobState::Completed,
              "one real worker prepares source, mesh, BVH and LOD");
        jobs.Forget(job);
        Check(prepared.geometry.source->heights == source.heights &&
                  prepared.geometry.chunks.size() == 1 && prepared.geometry.chunks[0].collision &&
                  prepared.geometry.lod,
              "prepared product has complete immutable source and derived geometry");
        Check(prepared.geometry.PayloadBytes() >
                  source.heights.size() * 20 +
                      prepared.geometry.chunks[0].chunk.mesh.vertices.size() * sizeof(MeshVertex),
              "payload accounting includes geometry/BVH rather than only scene text");
        auto cancelled = prepared;
        Check(!TerrainV2::PrepareAsset(path.string(), record.id, cancelled, error,
                                       [] { return true; }) &&
                  cancelled.geometry.source == prepared.geometry.source &&
                  cancelled.geometry.chunks.size() == 1,
              "cancelled preparation preserves a previous complete product");
        ResourceManager resources(nullptr, &assets, &jobs);
        RuntimeWorld world;
        world.legacyGameplay = false;
        Check(world.Build(Scene{}, &resources, error), "build empty authoritative world");
        auto object = [&] {
            SceneObject owner;
            owner.id = world.AllocateRuntimeEntityId();
            owner.name = "Prepared landscape";
            owner.body = SceneBodyComponent{};
            owner.landscape = SceneLandscapeSettings{record.id};
            return owner;
        };
        auto malformed = prepared;
        malformed.geometry.chunks[0].chunk.key = {1, 0};
        auto bad = object();
        Check(!world.StageRegionObject(bad, bad, error, "bad coverage", &malformed) &&
                  !malformed.consumed && world.Physics().AliveBodyCount() == 0,
              "invalid precomputed coverage rejects without leaking an identity seed");
        auto wrong = prepared;
        wrong.asset = "00000000000000000000000000000000";
        bad = object();
        Check(!world.StageRegionObject(bad, bad, error, "wrong identity", &wrong) &&
                  !wrong.consumed && world.Physics().AliveBodyCount() == 0,
              "mismatched source identity cannot publish or consume the product");
        const auto fingerprint = prepared.geometry.chunks[0].collision->sourceFingerprint;
        auto owner = object();
        Check(world.StageRegionObject(owner, owner, error, "prepared terrain", &prepared),
              "ordinary regional staging consumes complete worker output: " + error);
        Check(prepared.consumed && !prepared.geometry.source && prepared.geometry.chunks.empty(),
              "handoff moves owned data instead of copying or retaining another terrain");
        Check(world.LiveTerrain().Status(owner.id)->ready &&
                  !world.Physics().Raycast({0, 5, 0}, {0, -1, 0}, 10).hit,
              "complete staged collision remains private until ordinary publication");
        world.PublishRegion({owner.id});
        auto hit = world.Physics().Raycast({0, 5, 0}, {0, -1, 0}, 10);
        Check(hit.hit && std::abs(hit.point.y - 1) < .001f &&
                  world.EntityIdOfBody(hit.body) == owner.id,
              "atomic publication exposes actual prepared collision and logical ownership");
        Shape shape;
        BodyTransform pose;
        Check(world.Physics().GetBodyShape(hit.body, shape, pose) && shape.asset &&
                  shape.asset->sourceFingerprint == fingerprint,
              "installed collider uses the exact prepared geometry certificate");
        auto duplicate = object();
        Check(!world.StageRegionObject(duplicate, duplicate, error, "reused terrain", &prepared) &&
                  world.Physics().AliveBodyCount() == 2,
              "double consumption rejects while preserving the first owner");
        world.RemoveRegionObject(owner.id);
        Check(world.Physics().AliveBodyCount() == 0 && world.LiveTerrain().Owners().empty(),
              "ordinary cleanup releases prepared bodies and source ownership");
        world.Destroy();
        // The same small native asset exercises real regional admission, not a
        // mocked byte counter. Both budgets must reject before any publication.
        Project project;
        Check(Project::CreateNew((root / "Project").string(), "Native budgets", project, error),
              "create regional budget project");
        const auto installed = fs::path(project.AssetsDir()) / "ground.judasterrain";
        fs::copy_file(path, installed);
        AssetRecord regionalAsset;
        assets.Scan(project.RootDir(), project.AssetsDir());
        Check(assets.Track(installed.string(), regionalAsset, error), "track regional terrain");
        Scene regional;
        auto &landscape = regional.CreateObject("Budget landscape");
        landscape.body = SceneBodyComponent{};
        landscape.landscape = SceneLandscapeSettings{regionalAsset.id};
        Check(SaveSceneToFile(regional, project.Resolve("Scenes/ground.judas"), error),
              "save actual regional source");
        for (bool pendingBudget : {true, false}) {
            RuntimeWorld budgetWorld;
            budgetWorld.legacyGameplay = false;
            Check(budgetWorld.Build(Scene{}, &resources, error), "build budget world");
            WorldManifest manifest;
            manifest.pendingBytes = pendingBudget ? 4096 : 16 * 1024 * 1024;
            manifest.liveBytes = pendingBudget ? 16 * 1024 * 1024 : 4096;
            WorldRegion region;
            region.id = "ground";
            region.scene = "Scenes/ground.judas";
            region.estimatedBytes = 4096;
            manifest.regions.emplace(region.id, region);
            {
                WorldStreaming stream(budgetWorld, resources, project, manifest);
                const auto token = stream.Request(region.id, false, error);
                Check(token != 0, "request actual budgeted terrain");
                bool rejected = false;
                for (unsigned step = 0; step < 5000; ++step) {
                    stream.Advance(false);
                    const auto status = stream.Status(token).value();
                    rejected =
                        pendingBudget
                            ? status.state == "failed" &&
                                  status.error == "prepared region exceeds pending byte cap"
                            : status.state == "prepared" && status.error == "live byte budget";
                    if (rejected)
                        break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                Check(rejected && budgetWorld.Physics().AliveBodyCount() == 0 &&
                          budgetWorld.LiveTerrain().Owners().empty() &&
                          stream.Stats().liveBytes == 0,
                      "actual native payload rejects admission without publishing colliders");
            }
            budgetWorld.Destroy();
        }
        resources.Shutdown();
        fs::remove_all(root);
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
