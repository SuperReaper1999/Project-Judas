#include "RenderAssetCache.h"

#include "ModelLoader.h"
#include "RadialTerrain.h"
#include "TextureLoader.h"

MeshHandle RenderAssetCache::GetMesh(const std::string& path, std::string& outError) {
    if (!m_renderer || path.empty()) return MeshHandle{};
    const auto it = m_meshes.find(path);
    if (it != m_meshes.end()) {
        if (it->second.failed) outError = "mesh asset previously failed to load: " + path;
        return it->second.handle;
    }
    MeshData data;
    MeshEntry entry;
    if (!LoadObjMesh(path, data, outError)) {
        entry.failed = true;
        m_meshes[path] = entry;
        return MeshHandle{};
    }
    entry.handle = m_renderer->CreateMesh(data);
    m_meshes[path] = entry;
    return entry.handle;
}

TextureHandle RenderAssetCache::GetTexture(const std::string& path, std::string& outError) {
    if (!m_renderer || path.empty()) return TextureHandle{};
    const auto it = m_textures.find(path);
    if (it != m_textures.end()) {
        if (it->second.failed) outError = "texture asset previously failed to load: " + path;
        return it->second.handle;
    }
    TextureData data;
    TextureEntry entry;
    if (!LoadTextureFromFile(path, data, outError)) {
        entry.failed = true;
        m_textures[path] = entry;
        return TextureHandle{};
    }
    entry.handle = m_renderer->CreateTexture(data);
    m_textures[path] = entry;
    return entry.handle;
}

MeshHandle RenderAssetCache::GetTerrainMesh(const std::string& identifier, const RadialTerrain& surface) {
    if (!m_renderer) return MeshHandle{};
    const auto it = m_terrainMeshes.find(identifier);
    if (it != m_terrainMeshes.end()) return it->second;
    // The same tessellation M25 accepted for its authored planet.
    const MeshHandle handle = m_renderer->CreateMesh(surface.BuildMesh(96, 128));
    m_terrainMeshes[identifier] = handle;
    return handle;
}

void RenderAssetCache::Clear() {
    if (m_renderer) {
        for (auto& [path, entry] : m_meshes) {
            if (entry.handle.IsValid()) m_renderer->DestroyMesh(entry.handle);
        }
        for (auto& [path, entry] : m_textures) {
            if (entry.handle.IsValid()) m_renderer->DestroyTexture(entry.handle);
        }
        for (auto& [id, handle] : m_terrainMeshes) {
            if (handle.IsValid()) m_renderer->DestroyMesh(handle);
        }
    }
    m_meshes.clear();
    m_textures.clear();
    m_terrainMeshes.clear();
}
