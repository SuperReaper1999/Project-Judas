#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "WorldPersistence.h"
#include "particles/CollisionSnapshot.h"
#include "terrain/TerrainSource.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
void Restored(RuntimeWorld &world) {
    Check(world.LiveTerrain().Status(1)->chunks == 1024,
          "all kilometre terrain colliders remain resident");
    const auto &a = world.VisualEmitters()[0].pool.settings;
    const auto &b = world.VisualEmitters()[1].pool.settings;
    Check(a.collisionRegion && a.collisionRegionHalfExtents == glm::vec3(8) && b.collisionRegion &&
              b.collisionRegionOffset == glm::vec3(1, 0, 0) &&
              b.collisionRegionHalfExtents == glm::vec3(3, 8, 5),
          "runtime region overrides survive actual world restoration");
    auto saved = world.TakeParticleImpacts(2);
    Check(saved.size() == 1 && saved[0].receiver == 1 &&
              glm::distance(saved[0].position, glm::dvec3(0, 1, 0)) < 1e-4,
          "queued reliable terrain impact survives with absolute position and logical identity");
    Check(world.EmitParticleBurst(2, 1), "restored emitter emits normally");
    world.UpdateVisualParticles(.1f);
    auto impact = world.TakeParticleImpacts(2);
    Check(impact.size() == 1 && impact[0].receiver == 1 &&
              world.GetParticleRuntimeStats(2).snapshotBodies < 64,
          "cold region captures bounded geometry and still collides with real terrain");
}
void Benchmark(RuntimeWorld &world) {
    const auto originalContacts = world.GetParticleRuntimeStats(2).contacts;
    auto settings = world.RuntimeDefinition(2)->particleEmitter.value();
    settings.response = ParticleResponse::Stick;
    settings.impacts = ParticleImpactDelivery::Off;
    settings.lifetime = 3600;
    Check(world.SetParticleSettings(2, settings) && world.EmitParticleBurst(2, 1) &&
              world.ControlParticles(3, "clear") && world.ControlParticles(3, "stop"),
          "measure an active colliding effect, not the empty-emitter fast path");
    std::vector<double> local, whole, captures;
    using Clock = std::chrono::steady_clock;
    auto ms = [](auto begin) {
        return std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    };
    for (unsigned frame = 0; frame < 240; ++frame) {
        const auto begin = Clock::now();
        world.UpdateVisualParticles(1.f / 60);
        const auto elapsed = ms(begin);
        const auto stats = world.GetParticleRuntimeStats(2);
        if (frame >= 60) {
            local.push_back(elapsed);
            captures.push_back(stats.snapshotMs);
            std::cout << "{\"case\":\"local-runtime\",\"frame\":" << frame
                      << ",\"milliseconds\":" << elapsed << ",\"capture_ms\":" << stats.snapshotMs
                      << ",\"bodies\":" << stats.snapshotBodies
                      << ",\"triangles\":" << stats.snapshotTriangles << "}\n";
        }
    }
    Check(world.GetParticleRuntimeStats(2).live == 1 &&
              world.GetParticleRuntimeStats(2).contacts > originalContacts &&
              std::abs(world.VisualEmitters()[0].pool.Particles()[0].position.y -
                       1 - settings.collisionRadius) < .001f,
          "the benchmark retained a real stuck particle and live collision snapshots");
    JudasParticles::CollisionSnapshot reference;
    JudasParticles::CollisionCaptureLimits limits;
    limits.maxBodies = 2048;
    limits.maxTriangles = 1048576;
    std::string error;
    for (unsigned frame = 0; frame < 240; ++frame) {
        const auto begin = Clock::now();
        Check(reference.Capture(
                  world.Physics(), {{-8192, -8192, -8192}, {8192, 8192, 8192}}, double(1.f / 60),
                  error, {}, [&](BodyHandle body) { return world.EntityIdOfBody(body); }, limits),
              "complete TEST-ONLY reference capture");
        const auto elapsed = ms(begin);
        if (frame >= 60) {
            whole.push_back(elapsed);
            std::cout << "{\"case\":\"whole-reference\",\"frame\":" << frame
                      << ",\"milliseconds\":" << elapsed << "}\n";
        }
    }
    auto summary = [](const char *name, std::vector<double> &rows) {
        std::sort(rows.begin(), rows.end());
        std::cout << "METRIC " << name << " median_ms=" << (rows[89] + rows[90]) * .5
                  << " p95_ms=" << rows[170] << " worst_ms=" << rows.back() << '\n';
    };
    summary("runtime", local);
    summary("capture", captures);
    summary("whole-reference", whole);
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
                      ("terrain-particle-region-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!read) {
            Check(!fs::exists(root), "fresh evidence directory required");
            fs::create_directories(root);
        }
        JobSystem jobs(2);
        ResourceManager resources(nullptr, nullptr, &jobs);
        RuntimeWorld world;

        std::string error;
        std::map<std::string, SaveChunk> saved;
        if (!read) {
            Scene scene;
            scene.CreateObject("Kilometre native source").body = SceneBodyComponent{};
            for (unsigned i = 0; i < 2; ++i) {
                auto &object = scene.CreateObject("Local terrain effect");
                object.transform.position = {0, 4, 0};
                object.particleEmitter = ParticleEmitterSettings{};
                auto &settings = *object.particleEmitter;
                settings.collision = true;
                settings.rate = settings.burst = 0;
                settings.loop = false;
                settings.velocity = {0, -40, 0};
                settings.velocityVariation = settings.spread = glm::vec3(0);
                settings.lifetime = 10;
                settings.response = ParticleResponse::Remove;
                settings.impacts = ParticleImpactDelivery::Reliable;
                settings.maxImpactCells = 1;
            }
            Check(world.Build(scene, &resources, error), error);
            auto source = TerrainV2::Create(513, 513, 2);
            source.bottom = -16;
            source.top = 16;
            std::fill(source.heights.begin(), source.heights.end(), 1);
            Check(world.LiveTerrain().CreateLandscape(1, std::move(source), error) &&
                      world.LiveTerrain().Wait(1, error),
                  error);
            Check(world.EmitParticleBurst(2, 1) && world.EmitParticleBurst(3, 1),
                  "ordinary CPU emitters create actual droplets");
            world.UpdateVisualParticles(.1f);
            Check(world.GetParticleRuntimeStats(2).queryFailures == 1 &&
                      !world.VisualEmitters()[0].collision &&
                      world.VisualEmitters()[0].pool.Particles()[0].age == 0,
                  "default whole-chart capture still refuses over-budget coverage without moving "
                  "droplets");
            for (uint64_t id : {2, 3}) {
                auto settings = world.RuntimeDefinition(id)->particleEmitter.value();
                settings.collisionRegion = true;
                settings.collisionRegionHalfExtents = glm::vec3(8);
                Check(world.SetParticleSettings(id, settings), "enable explicit local coverage");
            }
            world.UpdateVisualParticles(.1f);
            const auto &a = world.VisualEmitters()[0];
            const auto &b = world.VisualEmitters()[1];
            Check(a.collision && a.collision == b.collision &&
                      a.collision->Stats().retainedBodies < 64 &&
                      a.collision->Stats().examinedBodies == 1024,
                  "compatible local regions share a conservative snapshot without deleting distant "
                  "ground");
            JudasParticles::CollisionSnapshot reference;
            JudasParticles::CollisionCaptureLimits referenceLimits;
            referenceLimits.maxBodies = 2048;
            referenceLimits.maxTriangles = 1048576;
            Check(reference.Capture(
                      world.Physics(), {{-8192, -8192, -8192}, {8192, 8192, 8192}}, .1, error, {},
                      [&](BodyHandle body) { return world.EntityIdOfBody(body); }, referenceLimits),
                  "independent full-world reference uses explicitly raised TEST-ONLY capacity");
            const auto complete = reference.SweepSphere({0, 4, 0}, {0, -1, 0}, .025);
            const auto bounded = a.collision->SweepSphere({0, 4, 0}, {0, -1, 0}, .025);
            Check(complete.status == JudasParticles::CollisionQueryStatus::Hit &&
                      bounded.status == complete.status && complete.entity == bounded.entity &&
                      glm::distance(complete.point, bounded.point) < 1e-6 &&
                      glm::distance(complete.normal, bounded.normal) < 1e-6,
                  "bounded runtime contact matches complete kilometre geometry");
            std::cout << "METRIC local_bodies=" << a.collision->Stats().retainedBodies
                      << " local_triangles=" << a.collision->Stats().uniqueTriangles
                      << " full_triangles=" << reference.Stats().uniqueTriangles << '\n';
            Check(a.pool.Particles().empty() && b.pool.Particles().empty(),
                  "successful reliable removals consume both colliding droplets");
            for (uint64_t id : {2, 3}) {
                auto impact = world.TakeParticleImpacts(id);
                Check(impact.size() == 1 && impact[0].receiver == 1 &&
                          std::abs(impact[0].position.y - 1) < 1e-4,
                      "local coverage delivers real terrain impacts to the logical receiver");
            }
            // Same history and filters, different declared coverage: sharing
            // would incorrectly turn this unsupported sweep into a floor hit.
            auto settings = b.pool.settings;
            settings.collisionRegionOffset.x = 40;
            Check(world.SetParticleSettings(3, settings) && world.EmitParticleBurst(2, 1) &&
                      world.EmitParticleBurst(3, 1),
                  "configure incompatible capture domains");
            world.UpdateVisualParticles(.1f);
            Check(a.collision != b.collision &&
                      world.GetParticleRuntimeStats(3).queryFailures > 0 &&
                      world.GetParticleRuntimeStats(3).diagnostic.find("domain") !=
                          std::string::npos &&
                      b.pool.Particles().size() == 1 && b.pool.Particles()[0].position.y == 4,
                  "incompatible regions cannot share; leaving coverage fails explicitly instead of "
                  "missing a wall");
            Check(world.TakeParticleImpacts(2).size() == 1,
                  "the independent compatible effect still delivers its contact");
            Check(world.ControlParticles(3, "clear"),
                  "clear the intentionally out-of-coverage visual control");
            settings.velocity = glm::vec3(0);
            settings.collisionRegionOffset = {1, 0, 0};
            settings.collisionRegionHalfExtents = {3, 8, 5};
            Check(world.SetParticleSettings(3, settings), "author transformed local coverage");
            SceneTransform pose;
            pose.position = {20, 4, 8};
            pose.rotation = glm::angleAxis(.7f, glm::vec3(0, 1, 0));
            pose.scale = {2, 1, 3};
            Check(world.SetRuntimeTransform(3, pose) && world.EmitParticleBurst(3, 1),
                  "move, rotate and scale the emitter through ordinary runtime placement");
            world.UpdateVisualParticles(.1f);
            glm::vec3 minimum(INFINITY), maximum(-INFINITY);
            for (int x : {-1, 1})
                for (int y : {-1, 1})
                    for (int z : {-1, 1}) {
                        const auto p = pose.position +
                                       pose.rotation *
                                           (pose.scale * (settings.collisionRegionOffset +
                                                          glm::vec3(x, y, z) *
                                                              settings.collisionRegionHalfExtents));
                        minimum = glm::min(minimum, p);
                        maximum = glm::max(maximum, p);
                    }
            Check(glm::distance(b.collision->Domain().min, minimum) < 1e-4 &&
                      glm::distance(b.collision->Domain().max, maximum) < 1e-4,
                  "captured AABB encloses all eight independent transformed box corners");
            const auto old = a.collision->Domain();
            Check(world.RequestOrigin({512, 0, 0}, error) && world.CommitOrigin(), error);
            Check(world.EmitParticleBurst(2, 1), "emit after normal origin transaction");
            world.UpdateVisualParticles(.1f);
            Check(glm::distance(a.collision->Domain().min + glm::vec3(512, 0, 0), old.min) < 1e-4 &&
                      world.GetParticleRuntimeStats(2).queuedImpacts == 1,
                  "origin shift preserves the absolute coverage and the pending terrain impact");
            auto changed = a.pool.settings;
            changed.collisionRegionHalfExtents = glm::vec3(9);
            Check(!world.SetParticleSettings(2, changed),
                  "pending reliable delivery prevents changing its capture contract");
            Check(WorldPersistence::Capture(world, saved, error), error);
            SaveArchive archive;
            archive(saved);
            std::ofstream file(root / "world.state", std::ios::binary);
            file.write(archive.bytes.data(), archive.bytes.size());
            Check(bool(file), "write real world continuation");
        } else {
            std::ifstream file(root / "world.state", std::ios::binary);
            SaveArchive archive(std::string{std::istreambuf_iterator<char>(file), {}});
            archive(saved);
            archive.Finish();
        }
        world.Destroy();
        Scene restored;
        Check(WorldPersistence::SceneFrom(saved, restored, error), error);
        Check(world.Build(restored, &resources, error, nullptr, nullptr, true), error);
        Check(WorldPersistence::Prepare(world, error), error);
        Check(WorldPersistence::Restore(world, saved, error), error);
        Restored(world);
        if (argc == 3 && std::string(argv[1]) == "--benchmark")
            Benchmark(world);
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
