#include "CharacterMotor.h"
#include "OriginLimits.h"
#include "PlanetTerrain.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "SceneSerialization.h"
#include "UniformGravity.h"
#include "Wheel.h"
#include "WorldStreaming.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainBrush.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <tuple>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
void Write(const std::filesystem::path &path, const std::string &bytes) {
    std::ofstream file(path, std::ios::binary);
    if (!file.write(bytes.data(), bytes.size()))
        throw std::runtime_error("fixture write failed");
}
std::string Read(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
template <class Done> void Pump(WorldStreaming &stream, Done done) {
    for (unsigned i = 0; i < 10000; ++i) {
        stream.Advance(false);
        for (const auto &region : stream.Regions())
            if (region.state == "failed")
                throw std::runtime_error("region failed: " + region.error);
        if (done())
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("regional terrain preparation timed out");
}
} // namespace
int main(int argc, char **argv) {
    try {
        namespace fs = std::filesystem;
        const bool residency = argc == 3 && std::string(argv[1]) == "--residency";
        const bool read = argc == 3 && std::string(argv[1]) == "--read";
        const fs::path root =
            argc == 3
                ? fs::path(argv[2])
                : fs::temp_directory_path() /
                      ("terrain-v2-streaming-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Project project;
        std::string error;
        if (!read) {
            Check(!fs::exists(root), "use a fresh project fixture");
            Check(Project::CreateNew(root.string(), "Native regional terrain", project, error),
                  "create ordinary project");

            Check(project.Save(error), "persist scripted project policy");
            auto source =
                residency ? TerrainV2::Create(1025, 1025, 1, 32) : TerrainV2::Create(513, 513, 2);
            std::fill(source.heights.begin(), source.heights.end(), 3);
            std::vector<uint8_t> bytes;
            Check(TerrainV2::EncodeAsset(source, bytes, error),
                  "encode one-kilometre native region");
            Write(root / "Assets/ground.judasterrain", {bytes.begin(), bytes.end()});
        } else
            Check(project.Load((root / "Native_regional_terrain.judasproj").string(), error),
                  "load cold project fixture: " + error);
        AssetDatabase assets;
        assets.Scan(project.RootDir(), project.AssetsDir());
        AssetRecord asset;
        if (const auto *known = assets.FindByRelativePath("Assets/ground.judasterrain"))
            asset = *known;
        else
            Check(assets.Track((root / "Assets/ground.judasterrain").string(), asset, error),
                  "track native source");
        Scene regionScene;
        auto &terrain = regionScene.CreateObject("Regional landscape");
        terrain.body = SceneBodyComponent{};
        terrain.landscape = SceneLandscapeSettings{asset.id};
        if (!read)
            Check(SaveSceneToFile(regionScene, (root / "Scenes/landscape.judas").string(), error),
                  "save ordinary source scene");
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        Scene bootstrap;
        bootstrap.Settings().worldOrigin = {OriginLimits::MaximumComponent,
                                            OriginLimits::MaximumComponent,
                                            -OriginLimits::MaximumComponent};
        RuntimeWorld world;
        world.ConfigureProject(project.Settings());
        Check(world.Build(bootstrap, &resources, error), "build root world");
        WorldManifest manifest;
        manifest.unitsPerFrame = 1;
        manifest.installMilliseconds = 2;
        manifest.pendingBytes = manifest.liveBytes = 128 * 1024 * 1024;
        manifest.retainedBytes = 8 * 1024 * 1024;
        WorldRegion region;
        region.id = "landscape";
        region.scene = "Scenes/landscape.judas";
        region.origin = bootstrap.Settings().worldOrigin;
        region.halfExtents = {512, 16, 512};
        manifest.regions.emplace(region.id, region);
        Check(ValidateWorldManifest(manifest, project, error),
              "validate native regional project: " + error);
        WorldStreaming stream(world, resources, project, manifest);
        if (read) {
            SaveArchive saved(Read(root / "streaming.state"));
            stream.Persist(saved, 9);
            saved.Finish();
        }
        auto load = [&] {
            const auto token = stream.Request("landscape", false, error);
            Check(token != 0, "request actual scene region");
            Pump(stream, [&] { return stream.Status(token)->state == "active"; });
            const auto id = stream.Resolve("landscape", 1);
            Check(id && world.HasRuntimeTerrain(id) && world.LiveTerrain().Status(id)->ready,
                  "region cannot publish before native collision is ready");
            Check(world.LiveTerrain().Status(id)->chunks == 1024 &&
                      world.Physics().AliveBodyCount() == 1025,
                  "all authored collision chunks belong to this single active region");
            return std::make_pair(token, id);
        };
        auto height = [&](float x) {
            const auto hit = world.Physics().Raycast({x, 12, 0}, {0, -1, 0}, 24);
            Check(hit.hit, "resident source has real collision");
            return hit.point.y;
        };
        auto unload = [&](uint64_t token) {
            Check(stream.Release(token) && stream.Unload("landscape"),
                  "release and unload actual region");
            Pump(stream, [&] { return stream.Regions().front().state == "unloaded"; });
            Check(world.Physics().AliveBodyCount() == 0 && world.LiveTerrain().Owners().empty(),
                  "unload retires every collider and runtime source");
            Check(stream.Stats().active == 0 && stream.Stats().liveBytes == 0,
                  "world residency accounting returns to zero");
        };
        auto [token, id] = load();
        if (residency) {
            auto &physics = world.Physics();
            const auto sleeper =
                physics.CreateDynamicBox({-400, 3.5f, -400}, glm::vec3(.5f), 1, .65f, 0);
            const auto chassis =
                physics.CreateDynamicBox({-400, 3.8f, 400}, {.7f, .15f, 1.1f}, 400, .5f, 0);
            std::vector<WheelInstance> wheels(4);
            for (unsigned i = 0; i < 4; ++i) {
                auto &wheel = wheels[i];
                wheel.settings.chassis = 1;
                wheel.settings.mount = {(i % 2 ? 1.f : -1.f) * .7f, -.1f,
                                        float(i / 2) * 1.8f - .9f};
                wheel.settings.radius = .35f;
                wheel.settings.restLength = .4f;
                wheel.settings.maxLength = .7f;
                wheel.settings.springStiffness = 14000;
                wheel.settings.damping = 1500;
                wheel.settings.rollingResistance = 0;
                wheel.state.length = .4f;
            }
            CharacterMotor motor;
            motor.Reset({-400, 3.94f, 0}, {1, 0, 0, 0});
            UniformGravity gravity({0, -9.81f, 0});
            PhysicsQueryFilter filter;
            filter.ignoredBodies = {sleeper, chassis};
            size_t cycleBytes = 0;
            unsigned supported = 0;
            const float initialVehicleZ = physics.GetTransform(chassis).position.z;
            for (unsigned pass = 0; pass < 3; ++pass) {
                if (pass == 2) {
                    Check(physics.IsSleeping(sleeper),
                          "support body sleeps before its terrain pages are revisited");
                    physics.SetLinearVelocity(sleeper, {0, .5f, 0});
                    Check(!physics.IsSleeping(sleeper),
                          "ordinary velocity command wakes the body after page churn");
                }
                for (unsigned z = 0; z < 32; ++z)
                    for (unsigned x = 0; x < 32; ++x) {
                        const auto hit = physics.Raycast({-496 + 32.f * x, 12, -496 + 32.f * z},
                                                         {0, -1, 0}, 24, filter);
                        Check(hit.hit && world.EntityIdOfBody(hit.body) == id &&
                                  std::abs(hit.point.y - 3) < 1e-4,
                              "independent remote query retains exact support through eviction");
                        const unsigned step = pass * 1024 + z * 32 + x;
                        physics.ApplyLinearAcceleration(sleeper, {0, -9.81f, 0}, 1.f / 60);
                        physics.ApplyLinearAcceleration(chassis, {0, -9.81f, 0}, 1.f / 60);
                        for (auto &wheel : wheels) {
                            wheel.command.driveTorque = step < 180 ? 80 : 0;
                            wheel.command.brakeTorque = step < 180 ? 0 : 180;
                            wheel.Step(physics, chassis, 1.f / 60);
                        }
                        physics.Step(1.f / 60);
                        for (auto &wheel : wheels)
                            wheel.Publish(physics, chassis);
                        motor.velocity.x = pass % 2 ? -1 : 1;
                        motor.Step(physics, gravity, 1.f / 60);
                        supported += motor.result.supported;
                        const auto *state = world.LiveTerrain().Status(id);
                        Check(state->sourcePageBytes <= state->sourcePageBudget &&
                                  state->collisionPageBytes <= state->collisionPageBudget,
                              "source and exact collision budgets hold throughout live physics");
                        Check(physics.GetTransform(sleeper).position.y > 3.45f &&
                                  physics.GetTransform(chassis).position.y > 3.4f &&
                                  motor.position.y > 3.9f,
                              "off-camera rigid body, vehicle and character stay physically "
                              "supported");
                    }
                const auto *state = world.LiveTerrain().Status(id);
                if (pass)
                    Check(state->bytes <= cycleBytes + 1024 * 1024,
                          "repeated traversal does not accumulate source/geometry revisions");
                cycleBytes = state->bytes;
                std::cout << "RESIDENCY cycle=" << pass << " bytes=" << state->bytes
                          << " source=" << state->sourcePageBytes
                          << " collision=" << state->collisionPageBytes
                          << " evictions=" << state->collisionEvictions << "\n";
            }
            Check(physics.IsSleeping(sleeper),
                  "off-camera physical body genuinely sleeps while terrain pages churn");
            Check(supported > 3000,
                  "character remains supported while crossing page/chunk boundaries");
            Check(physics.GetTransform(chassis).position.z < initialVehicleZ - 2,
                  "real wheel drive moves the vehicle on paged native collision");
            float load = 0;
            for (const auto &wheel : wheels)
                load += wheel.state.normalForce;
            Check(std::abs(load - 400 * 9.81f) < 80,
                  "paged terrain preserves independent wheel support mg load");
            physics.DestroyBody(sleeper);
            physics.DestroyBody(chassis);
        }
        if (read) {
            Check(height(0) < -1 && std::abs(height(-300) - 4) < .001f,
                  "cold regional reconstruction retains both excavation and sculpt");
            std::string snapshot;
            Check(world.CaptureRuntimeTerrain(id, snapshot, error) &&
                      snapshot == Read(root / "terrain.state"),
                  "cold region resolves exact base/raster/journal state");
            unload(token);
        } else {
            const auto initialPayload = world.LiveTerrain().LandscapePayloadBytes(id);
            Check(stream.Stats().liveBytes >= initialPayload && initialPayload > 32 * 1024 * 1024,
                  "regional admission accounts for native source, mesh, LOD and BVH payload");
            Check(std::abs(height(0) - 3) < .001f, "first activation uses authored native samples");
            TerrainV2::SurfaceBrush sculpt;
            sculpt.radius = 6;
            sculpt.strength = 1;
            Check(world.LiveTerrain().Surface(id, {-300, 3, 0}, sculpt, error) &&
                      world.LiveTerrain().Wait(id, error),
                  "sculpt an actual streamed owner");
            Check(
                world.LiveTerrain().Brush(id, {0, 3, 0}, {TerrainV2::Edit::Kind::Remove, {}, 6}, error) &&
                    world.LiveTerrain().Wait(id, error),
                "excavate an actual streamed owner");
            stream.Advance(false);
            Check(world.LiveTerrain().LandscapePayloadBytes(id) != initialPayload &&
                      stream.Stats().liveBytes >= world.LiveTerrain().LandscapePayloadBytes(id),
                  "resident edit payload refreshes regional memory accounting");
            std::string before;
            Check(world.CaptureRuntimeTerrain(id, before, error),
                  "capture native regional changes");
            Write(root / "terrain.state", before);
            auto oldId = id;
            for (unsigned visit = 0; visit < 5; ++visit) {
                const auto state = stream.Status(token).value();
                std::cout << "{\"case\":\"native-region\",\"visit\":" << visit
                          << ",\"load_ms\":" << state.loadMs
                          << ",\"largest_unit_ms\":" << state.largestUnitMs
                          << ",\"live_accounted_bytes\":" << stream.Stats().liveBytes
                          << ",\"terrain_geometry_estimate_bytes\":"
                          << world.LiveTerrain().Status(id)->bytes << "}\n";
                Check(height(0) < -1 && std::abs(height(-300) - 4) < .001f,
                      "resident sculpt and excavation retain real collision");
                unload(token);
                if (visit == 4)
                    break;
                std::tie(token, id) = load();
                Check(id != oldId, "revisit creates fresh runtime identity");
                oldId = id;
                std::string after;
                Check(world.CaptureRuntimeTerrain(id, after, error) && after == before,
                      "revisit restores exact native source state");
            }
            Check(stream.ArchiveVersion() == 9 && stream.SaveReady(),
                  "inactive native regions use the established terrain archive");
            SaveArchive saved;
            stream.Persist(saved, 9);
            Write(root / "streaming.state", saved.bytes);
        }
        if (argc == 1)
            fs::remove_all(root);
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
