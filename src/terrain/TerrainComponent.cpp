#include "TerrainComponent.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SceneFingerprint.h"
#include "TerrainAsset.h"

namespace TerrainV2 {
bool ValidateOwner(const SceneObject &o, const AssetDatabase *assets, std::string &error) {
    if (!o.landscape)
        return true;
    if (!o.body || o.body->motion != SceneBodyMotion::Static || o.body->shape != SceneShape::Box ||
        o.body->managed || o.parent || o.planet || o.deformable || o.navigationSurface ||
        o.characterMotor || o.ragdoll || o.wheel || o.liquidBasin || o.liquidContainer ||
        o.fluidVolume || !o.body->fluidCavities.empty() || !o.body->compoundBoxes.empty() ||
        o.vehicle || o.door || o.lightSwitch || o.powertrain || o.transform.scale != glm::vec3(1) ||
        !o.body->collisionAsset.empty()) {
        error = "landscape owns root unit-scale static chunk collision; remove competing "
                "geometry/motion owners";
        return false;
    }
    const auto *asset = assets ? assets->Find(o.landscape->asset) : nullptr;
    if (!asset || asset->missing || asset->type != AssetType::Terrain) {
        error = "landscape requires a registered native terrain asset";
        return false;
    }
    AssetPages pages;
    if (!pages.Open(asset->path, error))
        return false;
    for (const auto &id : pages.Index().metadata.layerTextures)
        if (!id.empty()) {
            const auto *texture = assets->Find(id);
            if (!texture || texture->missing || texture->type != AssetType::Texture) {
                error = "landscape material requires a registered texture: " + id;
                return false;
            }
        }
    return true;
}
bool SourceFingerprint(const Scene &scene, const AssetDatabase *assets, std::string &result,
                       std::string &error) {
    std::string bytes = "Judas.TerrainSources.1";
    bool any = false;
    for (const auto &o : scene.Objects())
        if (o.landscape) {
            if (!ValidateOwner(o, assets, error))
                return false;
            AssetPages pages;
            if (!pages.Open(assets->Find(o.landscape->asset)->path, error))
                return false;
            bytes += o.landscape->asset + pages.Index().fingerprint;
            any = true;
        }
    result = any ? SceneFingerprintSha256(bytes) : std::string{};
    error.clear();
    return true;
}
bool BuildAuthored(RuntimeWorld &world, const SceneObject &o, std::string &error) {
    if (!o.landscape)
        return true;
    const auto *assets = world.Resources() ? world.Resources()->Assets() : nullptr;
    if (!ValidateOwner(o, assets, error))
        return false;
    // Initial collision is ready before a normal scene starts stepping. Runtime
    // edits retain immutable preparation and safe publication boundaries.
    return world.LiveTerrain().CreateAsset(o.id, o.landscape->asset, error) &&
           world.LiveTerrain().Wait(o.id, error);
}
} // namespace TerrainV2
