#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "NetworkSession.h"
#include "SpriteAnimation.h"
#include "TextureLoader.h"
#include "authoring/Tilemap.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>

bool RuntimeWorld::EditTilemapCell(EntityId id, const std::string& layer, glm::ivec2 cell,
                                   const std::string& frame, bool flipX, bool flipY,
                                   std::string& error) {
    error.clear();
    if (auto* network = NetworkIfLoaded(); network && network->Active()) {
        error = "runtime tile editing is unavailable during an online session";
        return false;
    }
    const auto found = m_settings.authoringRecipes.find(TilemapKey(id));
    const auto* definition = RuntimeDefinition(id);
    const auto* assets = m_assets ? m_assets->Assets() : nullptr;
    if (!definition || found == m_settings.authoringRecipes.end() || !assets || IsTransientEntity(id)) {
        error = "runtime tile edit requires an authored tilemap and a project-owned world";
        return false;
    }
    // This copy is only the owner, never a snapshot of unrelated world objects.
    const auto root = *definition;
    if (root.parent || root.transform.scale != glm::vec3(1) ||
        root.transform.rotation != glm::quat(1, 0, 0, 0)) {
        error = "runtime tile edit requires an unparented, unrotated unit-scale grid";
        return false;
    }
    if (root.prefabRoot || !root.prefabAsset.empty() || root.body || root.characterMotor ||
        root.ragdoll || root.deformable || root.wheel || root.socket) {
        error = "runtime tilemap owner must be an authored static transform group";
        return false;
    }

    TilemapRecipe prior;
    if (!ParseTilemap(found->second, prior, error) || prior.root != id) {
        if (error.empty()) error = "tilemap recipe owner mismatch";
        return false;
    }
    auto next = prior;
    if (!PaintTilemap(next, layer, cell, cell, frame, frame.empty(), flipX, flipY, error)) return false;

    // Keep palette admission identical to authoring. This is bounded asset work,
    // not a scene bake, hierarchy flatten, texture upload or world serialization.
    const auto* palette = assets->Find(prior.sheet);
    SpriteSheet sheet;
    if (!palette || palette->missing || palette->type != AssetType::SpriteSheet) {
        error = "tracked sprite-sheet palette required";
        return false;
    }
    if (!LoadSpriteSheet(palette->path, sheet, error)) return false;
    const auto* texture = assets->Find(sheet.texture);
    if (!texture || texture->missing || texture->type != AssetType::Texture) {
        error = "palette image is missing";
        return false;
    }
    TextureData image;
    if (!LoadTextureFromFile(texture->path, image, error)) return false;
    if (image.width != sheet.width || image.height != sheet.height) {
        error = "palette image dimensions changed; explicitly re-slice the sheet";
        return false;
    }

    const TilemapEditScope scope{layer, cell};
    std::vector<CompiledTilemapObject> oldObjects, replacements;
    if (!CompileTilemapObjects(root, prior, sheet, scope, oldObjects, error) ||
        !CompileTilemapObjects(root, next, sheet, scope, replacements, error)) return false;

    std::map<std::string, TilemapGenerated> old;
    std::set<EntityId> affected;
    for (const auto& compiled : oldObjects) {
        const auto entry = std::find_if(prior.generated.begin(), prior.generated.end(),
            [&](const auto& g) { return g.key == compiled.key; });
        if (entry == prior.generated.end()) {
            error = "managed tile identity is missing";
            return false;
        }
        const auto* live = RuntimeDefinition(entry->entity);
        if (!live || live->parent != id) {
            error = "managed tile changed/removed outside painter";
            return false;
        }
        auto local = *live;
        if (const auto* hierarchy = m_hierarchy.Find(entry->entity)) local.transform = hierarchy->transform;
        if (TilemapObjectFingerprint(local) != entry->hash) {
            error = "managed tile changed outside painter; detach the grid before editing it";
            return false;
        }
        affected.insert(entry->entity);
        old.emplace(entry->key, *entry);
    }

    std::vector<SceneObject> additions;
    std::set<EntityId> retired;
    for (const auto& [key, entry] : old) retired.insert(entry.entity);
    auto nextId = m_nextRuntimeId;
    next.generated.erase(std::remove_if(next.generated.begin(), next.generated.end(),
        [&](const auto& entry) { return affected.count(entry.entity); }), next.generated.end());
    for (auto& compiled : replacements) {
        auto& object = compiled.object;
        auto previous = old.find(compiled.key);
        if (previous != old.end()) {
            object.id = previous->second.entity;
            if (TilemapObjectFingerprint(object) == previous->second.hash) {
                next.generated.push_back(previous->second);
                retired.erase(object.id);
                continue;
            }
        }
        if (nextId >= EntityId(std::numeric_limits<int64_t>::max())) {
            error = "tilemap edit exceeds runtime entity identity range";
            return false;
        }
        object.id = nextId++;
        const auto& local = object.transform.position;
        if ((root.transform.position + local) - root.transform.position != local) {
            error = "tile position cannot be restored exactly in this grid frame";
            return false;
        }
        if (!ValidateEntityCreation(object, error)) return false;
        next.generated.push_back({compiled.key, TilemapObjectFingerprint(object), object.id});
        additions.push_back(std::move(object));
    }
    // Read references without copying definitions. Unaffected children and all
    // unrelated bodies retain their identities, resources and contact state.
    size_t liveCount = 0;
    for (const auto& [other, object] : m_scriptDefinitions) {
        if (!RuntimeDefinition(other)) continue;
        ++liveCount;
        if (affected.count(object.parent)) {
            error = "managed tile has user children; detach grid before editing them";
            return false;
        }
        if (object.joint && (retired.count(object.joint->bodyA) || retired.count(object.joint->bodyB))) {
            error = "removed tile is a joint anchor; detach the grid before changing this assembly";
            return false;
        }
    }
    if (liveCount - retired.size() + additions.size() > 8192) {
        error = "tilemap edit would exceed the save entity limit";
        return false;
    }
    for (auto entity : retired) {
        // A script-free authored world keeps static tiles outside the dynamic
        // lifecycle registry. They still have normal definitions/resources.
        if (HasPendingParticleImpacts(entity)) {
            error = "consume reliable particle impacts before replacing a tile receiver";
            return false;
        }
        if (FindEntity(entity) && !ValidateEntityDestruction(entity, error)) return false;
    }
    if (!ValidateTilemap(next, error)) return false;
    auto encoded = EncodeTilemap(next);

    // Prepare all replacements before retiring bodies or publishing the recipe.
    // No script callback, physics step or renderer iteration runs in this scope.
    std::vector<EntityId> created;
    created.reserve(additions.size());
    const auto oldNextId = m_nextRuntimeId;
    for (const auto& object : additions) {
        const auto beforeAssets = m_referencedAssets.size();
        const auto createdId = CreateEntity(object, nullptr, &error);
        if (!createdId) {
            for (auto priorId : created) RemoveRegionObject(priorId);
            m_nextRuntimeId = oldNextId;
            return false;
        }
        // AppendSceneObjects acquires ordinary asset demand. Unlike a prop
        // retained until world teardown, generated tiles are physically removed
        // on replacement, so that demand must belong to the removable owner.
        m_regionAssets[createdId] = {m_referencedAssets.begin() + beforeAssets, m_referencedAssets.end()};
        created.push_back(createdId);
    }
    for (auto entity : retired) {
        // Initial authored tiles predate per-replacement ownership. Transfer
        // their one plain-atlas reference out of the world-lifetime list too.
        // Fingerprint admission above excludes user-added materials/playback.
        if (!m_regionAssets.count(entity)) {
            const auto* object = RuntimeDefinition(entity);
            if (object && object->render && !object->render->textureAsset.empty())
                m_regionAssets[entity] = {object->render->textureAsset};
        }
        RemoveRegionObject(entity);
    }
    found->second.swap(encoded);
    return true;
}
