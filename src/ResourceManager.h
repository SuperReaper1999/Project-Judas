#pragma once

#include <map>
#include <memory>
#include <string>

#include "AssetDatabase.h"
#include "Renderer.h"

class RadialTerrain;

// Milestone 30: the one engine boundary that turns project assets into
// runtime resources. Replaces M28's RenderAssetCache.
//
// Semantics, per asset id:
//
//   request/load  GetMesh/GetTexture: on the first request the asset is
//                 resolved through the AssetDatabase, decoded by the
//                 engine loader and uploaded through Renderer — the only
//                 path on which a GL object is ever created.
//   ready         a valid handle is returned; further requests are cache
//                 hits and touch neither disk nor GPU.
//   cached        the handle stays valid until released or invalidated.
//   failed        an unknown id, a missing file or an undecodable file is
//                 remembered as Failed with its message; the request
//                 returns an invalid handle and does not retry until
//                 Invalidate/Reload.
//   released      Release(id) destroys the GPU resource; the next request
//                 loads again (a new handle — the old one is invalid).
//   reloaded      Invalidate(id) = release + forget the failure, so a
//                 re-imported or moved file is picked up on next request.
//                 ReleaseAll on shutdown.
//
// Authored scene data holds AssetIds only; handles exist here and in the
// runtime instance that asked for them. A null Renderer makes every
// request a headless no-op returning an invalid handle (tests, the CLI).
enum class ResourceState { Unloaded, Ready, Failed };

struct ResourceStats {
    std::size_t loadedMeshes = 0;
    std::size_t loadedTextures = 0;
    std::size_t loadedTerrainMeshes = 0;
    std::size_t failed = 0;
    unsigned long long hits = 0;
    unsigned long long misses = 0;  // loads performed
};

class ResourceManager {
public:
    ResourceManager(Renderer* renderer, const AssetDatabase* assets);
    ~ResourceManager();
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    void SetAssetDatabase(const AssetDatabase* assets) { m_assets = assets; }
    const AssetDatabase* Assets() const { return m_assets; }

    MeshHandle GetMesh(const AssetId& id, std::string& outError);
    TextureHandle GetTexture(const AssetId& id, std::string& outError);
    // Terrain surfaces are engine-constructed (TerrainLibrary), keyed by
    // identifier rather than asset id.
    MeshHandle GetTerrainMesh(const std::string& identifier, const RadialTerrain& surface);

    ResourceState StateOf(const AssetId& id) const;
    std::string ErrorOf(const AssetId& id) const;
    void Release(const AssetId& id);
    void Invalidate(const AssetId& id) { Release(id); }
    void ReleaseAll();
    const ResourceStats& Stats() const { return m_stats; }
    Renderer* GetRenderer() const { return m_renderer; }

private:
    struct Entry {
        ResourceState state = ResourceState::Unloaded;
        AssetType type = AssetType::Mesh;
        MeshHandle mesh;
        TextureHandle texture;
        std::string error;
    };
    Entry& Load(const AssetId& id, AssetType expected);

    Renderer* m_renderer = nullptr;
    const AssetDatabase* m_assets = nullptr;
    std::map<AssetId, Entry> m_entries;
    std::map<std::string, MeshHandle> m_terrainMeshes;
    ResourceStats m_stats;
};
