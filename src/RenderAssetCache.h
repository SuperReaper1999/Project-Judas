#pragma once

#include <map>
#include <memory>
#include <string>

#include "Renderer.h"

class RadialTerrain;

// Milestone 28: GPU resources that scene content references by stable
// path/identifier — imported meshes, textures, and tessellated terrain
// surfaces — created on first use and shared by every object that names
// the same asset. This is the minimum "asset awareness" the editor and
// runtime need to draw a scene; it is not an asset database, importer
// pipeline, or streaming system (see docs/ARCHITECTURE.md, "Milestone 28,
// Deliberately not implemented"). Owned by whoever owns the Renderer; a
// null Renderer (headless tests) makes every lookup return an invalid
// handle without touching the GPU.
class RenderAssetCache {
public:
    explicit RenderAssetCache(Renderer* renderer) : m_renderer(renderer) {}
    ~RenderAssetCache() { Clear(); }
    RenderAssetCache(const RenderAssetCache&) = delete;
    RenderAssetCache& operator=(const RenderAssetCache&) = delete;

    // Loads from disk on first request. On failure `outError` names the
    // path and an invalid handle is returned; the failure is remembered so
    // the same missing file is reported once per cache, not once per frame.
    MeshHandle GetMesh(const std::string& path, std::string& outError);
    TextureHandle GetTexture(const std::string& path, std::string& outError);
    // Terrain meshes are keyed by surface identifier (see TerrainLibrary.h)
    // and built from the supplied surface at a fixed demo tessellation.
    MeshHandle GetTerrainMesh(const std::string& identifier, const RadialTerrain& surface);

    void Clear();
    Renderer* GetRenderer() const { return m_renderer; }

private:
    struct MeshEntry { MeshHandle handle; bool failed = false; };
    struct TextureEntry { TextureHandle handle; bool failed = false; };

    Renderer* m_renderer = nullptr;
    std::map<std::string, MeshEntry> m_meshes;
    std::map<std::string, TextureEntry> m_textures;
    std::map<std::string, MeshHandle> m_terrainMeshes;
};
