#include "CollisionGeometry.h"
#include "Project.h"
#include "ProjectFeaturePolicy.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "fluid/GameWaterRuntime.h"
#include "particles/CollisionSnapshot.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainBrush.h"
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
double Capacity(const JudasWater::Grid &grid) {
    double sum = 0;
    for (const auto &cell : grid.Cells())
        sum += cell.staticCapacity;
    return sum;
}
} // namespace
int main() {
    try {
        namespace fs = std::filesystem;
        const auto root =
            fs::temp_directory_path() /
            ("terrain-v2-interactions-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root / "Assets");
        auto source = TerrainV2::Create(33, 33, .5f);
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
        Check(assets.Track(path.string(), record, error), "register native source: " + error);
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        Scene scene;
        auto &ground = scene.CreateObject("Source-owned solid");
        ground.body = SceneBodyComponent{};
        ground.landscape = SceneLandscapeSettings{record.id};
        auto &gravity = scene.CreateObject("Gravity");
        gravity.gravity = SceneGravityComponent{};
        gravity.gravity->kind = SceneGravityKind::Uniform;
        gravity.gravity->regionRadius = 10000;
        RuntimeWorld basic;
        basic.legacyGameplay = false;
        Check(basic.Build(scene, &resources, error), "ordinary terrain without advanced systems");
        Check(!basic.GridWater() && !basic.HasFluid(), "terrain never enables a fluid backend");
        auto initial = basic.LiveTerrain().Solids({-4, 0, -4}, {4, 4, 4});
        Check(initial.size() == 1 && initial[0].Contains({0, .25f, 0}) &&
                  !initial[0].Contains({0, 1.25f, 0}),
              "source query distinguishes interior from dry air");
        Check(!initial[0].Contains({50, 0, 0}) &&
                  basic.LiveTerrain().Solids({50, 0, 50}, {60, 4, 60}).empty(),
              "finite source boundary is never extended by clamped height lookup");
        const auto body = basic.OwnedCollisionBodies(1).front();
        Check(basic.EntityIdOfBody(body) == 1 &&
                  basic.LiveTerrain().OwnerOf({body.id, body.world + 1}) == 0,
              "generated ownership checks complete world identity");
        JudasParticles::CollisionSnapshot particles;
        const Aabb particleDomain{{-8, -4, -8}, {8, 4, 8}};
        const auto receiver = [&](BodyHandle h) { return basic.EntityIdOfBody(h); };
        Check(particles.Capture(basic.Physics(), particleDomain, 1. / 60, error, {}, receiver),
              "Particles V2 captures the actual terrain chunk colliders");
        const auto originalParticle = particles.SweepSphere({0, 3, 0}, {0, .25, 0}, .05);
        Check(originalParticle.status == JudasParticles::CollisionQueryStatus::Hit &&
                  originalParticle.entity == 1 && std::abs(originalParticle.point.y - 1) < 1e-4,
              "particle sweep hits real terrain with its logical receiver identity");
        TerrainV2::SurfaceBrush paint;
        paint.kind = TerrainV2::SurfaceBrush::Kind::Paint;
        paint.layer = 2;
        paint.strength = 1;
        Check(basic.LiveTerrain().Surface(1, {0, 1, 0}, paint, error) &&
                  basic.LiveTerrain().Wait(1, error),
              "paint source before querying solid epoch");
        auto painted = basic.LiveTerrain().Solids({-4, 0, -4}, {4, 4, 4});
        Check(initial[0].SameGeometry(painted[0]),
              "material publication does not invalidate solid geometry");
        Check(basic.LiveTerrain().Brush(1, {0, .5f, 0}, {VolumeBrushKind::Remove, 1, 1}, error),
              "queue real excavation");
        auto pending = basic.LiveTerrain().Solids({-4, 0, -4}, {4, 4, 4});
        Check(pending[0].SameGeometry(initial[0]) && pending[0].Contains({0, .25f, 0}),
              "solid queries retain the last complete revision during preparation");
        Check(basic.LiveTerrain().Wait(1, error), "publish actual excavation");
        auto excavated = basic.LiveTerrain().Solids({-4, 0, -4}, {4, 4, 4});
        Check(!excavated[0].SameGeometry(initial[0]) && !excavated[0].Contains({0, .25f, 0}) &&
                  initial[0].Contains({0, .25f, 0}),
              "independent immutable snapshots retain old solid and new excavation correctly");
        auto updatedParticles = particles;
        Check(updatedParticles.Capture(basic.Physics(), particleDomain, 1. / 60, error, {}, receiver),
              "particle snapshot refreshes after real terrain publication");
        Check(updatedParticles.SweepSphere({0, 3, 0}, {0, .25, 0}, .05).status ==
                  JudasParticles::CollisionQueryStatus::Miss &&
                  particles.SweepSphere({0, 3, 0}, {0, .25, 0}, .05).status ==
                  JudasParticles::CollisionQueryStatus::Hit,
              "particles pass through the excavated opening while the retained old snapshot stays immutable");
        auto pose = basic.RuntimeDefinition(1)->transform;
        pose.rotation = glm::angleAxis(.4f, glm::normalize(glm::vec3(1, 2, 3)));
        pose.position = {20, -5, 8};
        Check(basic.SetRuntimeTransform(1, pose), "move complete source rigidly");
        auto moved = basic.LiveTerrain().Solids({-100, -100, -100}, {100, 100, 100});
        Check(moved.size() == 1 &&
                  moved[0].Contains(pose.position + pose.rotation * glm::vec3(3, .25f, 0)) &&
                  !moved[0].Contains(pose.position + pose.rotation * glm::vec3(0, .25f, 0)),
              "solid and excavation use the same rotated source frame as collision");
        basic.HideRegion({1});
        Check(basic.LiveTerrain().Solids({-100, -100, -100}, {100, 100, 100}).empty(),
              "private region has no published solid ownership");
        basic.PublishRegion({1});
        Check(basic.LiveTerrain().Solids({-100, -100, -100}, {100, 100, 100}).size() == 1,
              "ordinary regional publication restores solid ownership");
        Check(basic.SetRuntimeTransform(1, SceneTransform{}),
              "restore source's original placement");
        basic.LiveTerrain().Remove(1);
        Check(basic.LiveTerrain().CreateAsset(1, record.id, error) &&
                  basic.LiveTerrain().Wait(1, error),
              "recreate same logical source through ordinary lifetime management");
        const auto reincarnated = basic.LiveTerrain().Solids({-4, 0, -4}, {4, 4, 4});
        Check(reincarnated.size() == 1 && reincarnated[0].revision == initial[0].revision &&
                  !reincarnated[0].SameGeometry(initial[0]),
              "a new incarnation cannot alias a previous source's numeric revision");
        if (AdvancedFluidDynamicsRestriction()) {
            std::cout << "NOT EXERCISED advanced-fluid integration: Alpha lock remains intact\n";
        } else {
            auto &water = scene.CreateObject("Source-aware water");
            water.fluidVolume = SceneFluidVolumeComponent{};
            water.fluidVolume->backend = SceneFluidBackend::Grid;
            water.fluidVolume->countX = 16;
            water.fluidVolume->countY = 8;
            water.fluidVolume->countZ = 16;
            water.fluidVolume->spacing = .5f;
            water.fluidVolume->fillHeight = 2;
            const auto waterId = water.id;
            ProjectSettings settings;
            settings.legacyGameplay = false;
            settings.advancedFluidDynamics = true;
            RuntimeWorld world;
            world.ConfigureProject(settings);
            const bool built = world.Build(scene, &resources, error);
            Check(built, "native terrain and grid water build: " + error);
            auto *system = world.GridWater();
            Check(system && system->Find(waterId), "explicit opt-in creates normal grid water");
            const auto *grid = system->Find(waterId);
            Check(std::abs(Capacity(*grid) - 192) < 1e-7,
                  "flat source displaces its analytic 64 cubic metres, not a thin surface shell");
            Check(std::abs(grid->Quantity() - 64) < 1e-7,
                  "filled water occupies only the known-answer above-ground volume");
            std::vector<PrimitivePose> oldBoundaries;
            for (auto body : world.OwnedCollisionBodies(1)) {
                Shape shape;
                BodyTransform pose;
                Check(world.Physics().GetBodyShape(body, shape, pose),
                      "retain actual old boundary input");
                RigidBody rigid;
                rigid.position = pose.position;
                rigid.orientation = pose.rotation;
                for (int i = 0; i < PrimitiveCount(shape); ++i)
                    oldBoundaries.push_back(PrimitiveAt(shape, rigid, i));
            }
            JudasWater::Grid shellReference;
            Check(shellReference.Initialize(grid->Configuration(), error),
                  "initialize unchanged shell oracle");
            shellReference.BakeStatic([&](glm::vec3 p) {
                for (const auto &primitive : oldBoundaries) {
                    const auto distance = PointGeometry(glm::dvec3(p), primitive, .0625);
                    if (distance.valid && (distance.contains || distance.gap <= .0625))
                        return true;
                }
                return false;
            });
            std::cout << "DIAGNOSTIC old_boundary_capacity_m3=" << Capacity(shellReference)
                      << " source_capacity_m3=" << Capacity(*grid) << '\n';
            Check(Capacity(shellReference) > Capacity(*grid) + 1 &&
                      std::abs(Capacity(*grid) - 192) < 1e-7,
                  "preserved old boundary-only interpretation exposes false underground fluid "
                  "capacity");
            const auto originalQuantity = grid->Quantity();
            const auto bakes = system->Stats(waterId).geometryBakes;
            Check(world.LiveTerrain().Surface(1, {0, 1, 0}, paint, error) &&
                      world.LiveTerrain().Wait(1, error),
                  "paint active water's source");
            Check(system->Step(1.f / 60, error) && system->Stats(waterId).geometryBakes == bakes,
                  "paint does not rebake physical water occupancy: " + error);
            Check(
                world.LiveTerrain().Brush(1, {0, .5f, 0}, {VolumeBrushKind::Remove, 1, 1}, error) &&
                    world.LiveTerrain().Wait(1, error),
                "excavate solid beneath actual water");
            Check(system->Step(1.f / 60, error),
                  "publish changed water capacity and advance: " + error);
            const double editPreparationMs = system->Stats(waterId).preparationMs;
            grid = system->Find(waterId);
            Check(system->Stats(waterId).geometryBakes == bakes + 1 && Capacity(*grid) > 194,
                  "excavation opens actual fluid capacity in the same revision");
            std::vector<double> steadyPreparation;
            for (unsigned i = 0; i < 30; ++i) {
                Check(system->Step(1.f / 60, error), "advance source-aware water: " + error);
                steadyPreparation.push_back(system->Stats(waterId).preparationMs);
            }
            std::sort(steadyPreparation.begin(), steadyPreparation.end());
            Check(std::abs(grid->Quantity() - originalQuantity) < 1e-7,
                  "geometry rebake and flow preserve physical water quantity");
            const auto beforeShiftCapacity = Capacity(*grid);
            Check(world.RequestOrigin({1000, 0, 0}, error) && world.CommitOrigin(),
                  "shift a real terrain/water world without changing source coordinates: " + error);
            Check(system->Step(1.f / 60, error) &&
                      std::abs(Capacity(*system->Find(waterId)) - beforeShiftCapacity) < 1e-7 &&
                      std::abs(system->Find(waterId)->Quantity() - originalQuantity) < 1e-7,
                  "origin shift preserves fluid capacity and water quantity");
            std::map<std::string, SaveChunk> saved;
            Check(WorldPersistence::Capture(world, saved, error),
                  "ordinary save captures terrain and actual water together: " + error);
            Scene savedScene;
            Check(WorldPersistence::SceneFrom(saved, savedScene, error), "read native saved scene");
            RuntimeWorld restored;
            restored.ConfigureProject(settings);
            const bool coldBuild =
                restored.Build(savedScene, &resources, error, nullptr, nullptr, true);
            Check(coldBuild, "build shifted snapshot scene: " + error);
            const bool coldRestore = WorldPersistence::Restore(restored, saved, error);
            Check(coldRestore, "restore coordinated source and water state: " + error);
            Check(restored.GridWater()->Step(1.f / 60, error) &&
                      std::abs(Capacity(*restored.GridWater()->Find(waterId)) -
                               beforeShiftCapacity) < 1e-7 &&
                      std::abs(restored.GridWater()->Find(waterId)->Quantity() - originalQuantity) <
                          1e-7,
                  "restored source geometry and fluid quantities agree after the first real step");
            auto radialScene = scene;
            const auto rotation = glm::angleAxis(.4f, glm::normalize(glm::vec3(1, 2, 3)));
            const auto location = rotation * glm::vec3(0, 1000, 0);
            for (auto id : {uint64_t(1), waterId}) {
                radialScene.Find(id)->transform.position = location;
                radialScene.Find(id)->transform.rotation = rotation;
            }
            radialScene.Find(2)->gravity->kind = SceneGravityKind::Radial;
            RuntimeWorld radial;
            radial.ConfigureProject(settings);
            const bool radialBuild = radial.Build(radialScene, &resources, error);
            Check(radialBuild,
                  "build native terrain/water in a rotated radial-gravity chart: " + error);
            Check(std::abs(Capacity(*radial.GridWater()->Find(waterId)) - 192) < 1e-7 &&
                      std::abs(radial.GridWater()->Find(waterId)->Quantity() - 64) < 1e-7,
                  "rotated radial chart preserves the independent volume answer");
            for (unsigned i = 0; i < 5; ++i)
                Check(radial.GridWater()->Step(1.f / 60, error),
                      "advance radial source-aware water: " + error);
            Check(std::abs(radial.GridWater()->Find(waterId)->Quantity() - 64) < 1e-7,
                  "radial-gravity flow preserves physical water quantity");
            std::cout << "METRIC native_terrain_water_quantity_m3=" << originalQuantity
                      << " initial_free_capacity_m3=192 old_boundary_capacity_m3="
                      << Capacity(shellReference)
                      << " excavation_free_capacity_m3=" << beforeShiftCapacity
                      << " edit_geometry_preparation_ms=" << editPreparationMs
                      << " steady_geometry_median_ms="
                      << (steadyPreparation[14] + steadyPreparation[15]) * .5
                      << " steady_geometry_p95_ms=" << steadyPreparation[28]
                      << " steady_geometry_worst_ms=" << steadyPreparation.back() << '\n';
        }
        basic.Destroy();
        resources.Shutdown();
        fs::remove_all(root);
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
