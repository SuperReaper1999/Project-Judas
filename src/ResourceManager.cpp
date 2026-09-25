#include "ResourceManager.h"

#include "ModelLoader.h"
#include "RadialTerrain.h"
#include "TextureLoader.h"

ResourceManager::ResourceManager(Renderer* renderer, const AssetDatabase* assets)
    : m_renderer(renderer), m_assets(assets) {}

ResourceManager::~ResourceManager() {
    ReleaseAll();
}

ResourceManager::Entry& ResourceManager::Load(const AssetId& id, AssetType expected) {
    Entry& entry = m_entries[id];
    if (entry.state != ResourceState::Unloaded) {
        ++m_stats.hits;
        return entry;
    }
    ++m_stats.misses;
    entry.type = expected;
    const auto fail = [&](const std::string& message) -> Entry& {
        entry.state = ResourceState::Failed;
        entry.error = message;
        ++m_stats.failed;
        return entry;
    };
    if (!m_renderer) return fail("no renderer (headless)");
    if (!m_assets) return fail("no asset database");
    const AssetRecord* record = m_assets->Find(id);
    if (!record) return fail("unknown asset id " + id);
    if (record->missing) return fail("asset file is missing: " + record->relativePath);
    if (record->type != expected) {
        return fail("asset " + record->relativePath + " is a " + AssetTypeName(record->type) + ", not a " +
                    AssetTypeName(expected));
    }
    std::string error;
    if (expected == AssetType::Mesh) {
        MeshData data;
        if (!LoadObjMesh(record->path, data, error)) return fail(error);
        entry.mesh = m_renderer->CreateMesh(data);
        ++m_stats.loadedMeshes;
    } else if (expected == AssetType::Texture) {
        TextureData data;
        if (!LoadTextureFromFile(record->path, data, error)) return fail(error);
        entry.texture = m_renderer->CreateTexture(data);
        ++m_stats.loadedTextures;
    } else {
        return fail("fonts are engine-level resources, not scene resources");
    }
    entry.state = ResourceState::Ready;
    return entry;
}

MeshHandle ResourceManager::GetMesh(const AssetId& id, std::string& outError) {
    if (id.empty()) return MeshHandle{};
    const Entry& entry = Load(id, AssetType::Mesh);
    if (entry.state == ResourceState::Failed) outError = entry.error;
    return entry.mesh;
}

TextureHandle ResourceManager::GetTexture(const AssetId& id, std::string& outError) {
    if (id.empty()) return TextureHandle{};
    const Entry& entry = Load(id, AssetType::Texture);
    if (entry.state == ResourceState::Failed) outError = entry.error;
    return entry.texture;
}

MeshHandle ResourceManager::GetTerrainMesh(const std::string& identifier, const RadialTerrain& surface) {
    if (!m_renderer) return MeshHandle{};
    const auto it = m_terrainMeshes.find(identifier);
    if (it != m_terrainMeshes.end()) {
        ++m_stats.hits;
        return it->second;
    }
    ++m_stats.misses;
    // The same tessellation M25 accepted for its authored planet.
    const MeshHandle handle = m_renderer->CreateMesh(surface.BuildMesh(96, 128));
    m_terrainMeshes[identifier] = handle;
    ++m_stats.loadedTerrainMeshes;
    return handle;
}

ResourceState ResourceManager::StateOf(const AssetId& id) const {
    const auto it = m_entries.find(id);
    return it == m_entries.end() ? ResourceState::Unloaded : it->second.state;
}

std::string ResourceManager::ErrorOf(const AssetId& id) const {
    const auto it = m_entries.find(id);
    return it == m_entries.end() ? std::string() : it->second.error;
}

void ResourceManager::Release(const AssetId& id) {
    const auto it = m_entries.find(id);
    if (it == m_entries.end()) return;
    Entry& entry = it->second;
    if (m_renderer && entry.state == ResourceState::Ready) {
        if (entry.mesh.IsValid()) { m_renderer->DestroyMesh(entry.mesh); --m_stats.loadedMeshes; }
        if (entry.texture.IsValid()) { m_renderer->DestroyTexture(entry.texture); --m_stats.loadedTextures; }
    }
    if (entry.state == ResourceState::Failed && m_stats.failed > 0) --m_stats.failed;
    m_entries.erase(it);
}

void ResourceManager::ReleaseAll() {
    if (m_renderer) {
        for (auto& [id, entry] : m_entries) {
            if (entry.state != ResourceState::Ready) continue;
            if (entry.mesh.IsValid()) m_renderer->DestroyMesh(entry.mesh);
            if (entry.texture.IsValid()) m_renderer->DestroyTexture(entry.texture);
        }
        for (auto& [id, handle] : m_terrainMeshes) {
            if (handle.IsValid()) m_renderer->DestroyMesh(handle);
        }
    }
    m_entries.clear();
    m_terrainMeshes.clear();
    m_stats.loadedMeshes = m_stats.loadedTextures = m_stats.loadedTerrainMeshes = m_stats.failed = 0;
}
