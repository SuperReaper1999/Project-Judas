#include "AssetDependencies.h"
#include "PhysicalMaterial.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "WorldPersistence.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainComponent.h"
#include "terrain/TerrainPersistence.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
}
void Write(const std::filesystem::path &path, const TerrainV2::Source &source) {
    std::vector<uint8_t> bytes;
    std::string error;
    if (!TerrainV2::EncodeAsset(source, bytes, error))
        throw std::runtime_error(error);
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    if (!file)
        throw std::runtime_error("write fixture");
}
void Wait(RuntimeWorld &world) {
    for (unsigned i = 0; i < 10000; ++i) {
        world.AdvanceRuntimeTerrain();
        if (!world.LiveTerrain().Status(1)->busy)
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("publication timeout");
}
} // namespace
int main() {
    try {
        namespace fs = std::filesystem;
        const auto root =
            fs::temp_directory_path() /
            ("judas-terrain-component-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root / "Assets");
        const auto path = root / "Assets/terrain.judasterrain";
        auto source = TerrainV2::Create(33, 33, .5f);
        std::fill(source.heights.begin(), source.heights.end(), 3);
        // An independent registered texture verifies the native export dependency,
        // including the material-only continuation and genuine world restore.
        const auto normalPath = root / "Assets/detail-normal.tga";
        {
            std::string image(22, '\0');
            image[2] = 2;
            image[12] = image[14] = 1;
            image[16] = 32;
            image[17] = 8;
            image[18] = char(255);
            image[19] = image[20] = char(128);
            image[21] = char(255);
            std::ofstream f(normalPath, std::ios::binary);
            f.write(image.data(), image.size());
        }
        Write(path, source);
        AssetDatabase assets;
        assets.Scan(root.string(), (root / "Assets").string());
        AssetRecord asset;
        std::string error;
        Check(assets.Track(path.string(), asset, error) && asset.type == AssetType::Terrain,
              "normal importer registers the native terrain kind: " + error);
        AssetRecord normal;
        Check(assets.Track(normalPath.string(), normal, error), "register normal detail texture");
        source.materialDetail.normalTexture = normal.id;
        source.materialDetail.tileMetres = {1, 2, 8, 16};
        source.materialDetail.roughness = {.3f, .5f, .8f, 1};
        Write(path, source);
        std::set<AssetId> dependencyIds;
        std::set<std::string> auxiliary;
        Check(CollectAssetDependencies(asset, dependencyIds, auxiliary, error) &&
                  dependencyIds.count(normal.id),
              "native terrain exports its normal detail dependency");
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        {
            Scene scene;
            auto &owner = scene.CreateObject("Native landscape");
            owner.body = SceneBodyComponent{};
            owner.render = SceneRenderComponent{};
            owner.landscape = SceneLandscapeSettings{asset.id};
            auto incompatible = owner;
            incompatible.body->compoundBoxes.emplace_back();
            Check(!TerrainV2::ValidateOwner(incompatible, &assets, error),
                  "native terrain cannot silently discard a competing compound collider");
            incompatible = owner;
            incompatible.body->fluidCavities.emplace_back();
            Check(!TerrainV2::ValidateOwner(incompatible, &assets, error),
                  "native terrain rejects competing body cavity ownership explicitly");
            std::string text;
            Scene roundtrip;
            Check(SaveSceneToString(scene, text) && LoadSceneFromString(text, roundtrip, error) &&
                      SceneObjectsEqual(owner, *roundtrip.Find(1)),
                  "native terrain component survives ordinary scene serialization");
            std::set<AssetId> references;
            CollectSceneAssetReferences(scene, references);
            Check(references.count(asset.id),
                  "ordinary scene/export dependency graph sees terrain source");
            RuntimeWorld world;
            world.legacyGameplay = false;
            Check(world.Build(scene, &resources, error),
                  "normal scene builds native terrain without a script bootstrap: " + error);
            Check(world.HasRuntimeTerrain(1) && world.LiveTerrain().Status(1)->ready,
                  "initial native collision is ready before scene gameplay starts");
            auto hit = world.Physics().Raycast({0, 5, 0}, {0, -1, 0}, 6);
            Check(hit.hit && std::abs(hit.point.y - 3) < .001f &&
                      world.EntityIdOfBody(hit.body) == 1,
                  "native source provides actual terrain collision and stable ownership");
            const PhysicalMaterial material{.27f, .12f};
            Check(world.SetBodyMaterial(1, "", &material, error),
                  "logical terrain owner accepts ordinary physical-material controls");
            bool materialMatches = true;
            for (auto body : world.OwnedCollisionBodies(1)) {
                std::string identity;
                float friction = 0, restitution = 0;
                materialMatches &=
                    world.Physics().GetPhysicalMaterial(body, identity, friction, restitution) &&
                    friction == material.friction && restitution == material.restitution;
            }
            Check(materialMatches, "material mutation updates actual generated collision");
            Check(world.LiveTerrain().Brush(1, {0, 2.5f, 0}, {VolumeBrushKind::Remove, 1.5f, 1},
                                            error),
                  "runtime excavation uses native source");
            Wait(world);
            hit = world.Physics().Raycast({0, 5, 0}, {0, -1, 0}, 6);
            Check(hit.hit && hit.point.y < 1.3f,
                  "excavation changes real collision rather than replacing presentation alone");
            std::string materialId;
            float friction = 0, restitution = 0;
            Check(
                world.Physics().GetPhysicalMaterial(hit.body, materialId, friction, restitution) &&
                    friction == material.friction && restitution == material.restitution,
                "replacement colliders retain the owner's current physical material");
            auto oversized = TerrainV2::Create(257, 257, .5f, 4);
            std::vector<uint8_t> oversizedBytes;
            Check(TerrainV2::EncodeAsset(oversized, oversizedBytes, error),
                  "larger on-disk terrain is valid independently of runtime residency");
            Check(!world.LiveTerrain().Restore(
                      1, std::string(oversizedBytes.begin(), oversizedBytes.end()), error),
                  "restoration cannot bypass the active collision residency budget");
            hit = world.Physics().Raycast({0, 5, 0}, {0, -1, 0}, 6);
            Check(hit.hit && hit.point.y < 1.3f && !world.LiveTerrain().Status(1)->busy,
                  "rejected over-budget restore retains current collision without queuing work");
            std::string snapshot;
            Check(world.CaptureRuntimeTerrain(1, snapshot, error) &&
                      TerrainV2::IsReferenceSnapshot(snapshot),
                  "asset-backed terrain saves immutable base identity plus mutable journal");
            Check(snapshot.size() < 8192,
                  "local excavation does not copy all source sample arrays into every save");
            std::map<std::string, SaveChunk> chunks;
            Check(WorldPersistence::Capture(world, chunks, error),
                  "ordinary world save captures terrain reference: " + error);
            RuntimeWorld cold;
            cold.legacyGameplay = false;
            Check(cold.Build(scene, &resources, error),
                  "independent world loads same native asset: " + error);
            Check(WorldPersistence::Restore(cold, chunks, error),
                  "ordinary cold world restore resolves base and journal: " + error);
            hit = cold.Physics().Raycast({0, 5, 0}, {0, -1, 0}, 6);
            Check(hit.hit && hit.point.y < 1.3f, "cold restore preserves native-source excavation");
            std::string restoredSnapshot;
            Check(cold.CaptureRuntimeTerrain(1, restoredSnapshot, error) &&
                      restoredSnapshot == snapshot,
                  "world restore preserves exact terrain appearance and journal metadata");
            auto changed = source;
            changed.heights[0] += .25f;
            Write(path, changed);
            Check(!world.LiveTerrain().Restore(1, snapshot, error),
                  "changed base cannot silently claim old save compatibility");
            hit = world.Physics().Raycast({0, 5, 0}, {0, -1, 0}, 6);
            Check(hit.hit && hit.point.y < 1.3f,
                  "failed base migration leaves live terrain intact");
            Write(path, source);
            Check(world.LiveTerrain().Restore(1, snapshot, error),
                  "original base permits explicit retry after content is restored");
            auto regional = owner;
            regional.id = kRuntimeEntityIdBase + 7;
            regional.transform.position = {40, 0, 0};
            Check(world.StageRegionObject(regional, regional, error),
                  "native terrain stages through ordinary streaming registration: " + error);
            Check(!world.Physics().Raycast({40, 8, 0}, {0, -1, 0}, 10).hit,
                  "private native terrain has no published physical support");
            EntityPhysicalState regionalState;
            Check(world.GetEntityState(regional.id, regionalState),
                  "staged source retains physical identity");
            regionalState.position = {45, 2, 0};
            Check(world.SetEntityState(regional.id, regionalState),
                  "internal regional restoration updates complete terrain placement");
            Check(!world.SetRuntimeTransform(regional.id, regional.transform),
                  "public transform writes cannot access a private staged owner");
            Check(!world.Physics().Raycast({45, 8, 0}, {0, -1, 0}, 10).hit,
                  "private placement cannot expose chunk collision early");
            world.PublishRegion({regional.id});
            auto regionalHit = world.Physics().Raycast({45, 8, 0}, {0, -1, 0}, 10);
            Check(regionalHit.hit && std::abs(regionalHit.point.y - 5) < .001f,
                  "region publication exposes correctly restored terrain placement");
            world.RemoveRegionObject(regional.id);
            Check(!world.HasRuntimeTerrain(regional.id),
                  "streamed terrain unregisters through ordinary cleanup");
            Check(!world.Physics().Raycast({45, 8, 0}, {0, -1, 0}, 10).hit,
                  "regional cleanup leaves no generated terrain behind");
            Check(world.DestroyEntity(1, &error) && !world.HasRuntimeTerrain(1),
                  "native component cleanup retires source and derived bodies");
            std::cout << "METRIC asset_backed_snapshot_bytes=" << snapshot.size()
                      << " raw_source_samples=" << source.heights.size() << '\n';
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
