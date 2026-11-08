#pragma once
#include "TerrainLod.h"
#include "TerrainGeometryPages.h"
#include "TerrainSource.h"
#include <memory>

struct CollisionAsset;
struct NavigationData;
struct PreparedNavigationSurface;
namespace TerrainV2 {
struct NavigationInput;
struct PreparedChunk {
    Chunk chunk;
    std::shared_ptr<CollisionAsset> collision;
    std::shared_ptr<GeometryRecipe> recipe;
    size_t triangles = 0;
    size_t PayloadBytes() const;
};
size_t SourcePayloadBytes(const Source &, const HeightLod *);
// Worker-only construction. Completed products contain no world, renderer or
// body handles. The coordinator moves them into a single runtime owner.
struct Preparation {
    std::shared_ptr<const Source> source;
    std::shared_ptr<GeometryCache> cache;
    std::vector<PreparedChunk> chunks;
    double milliseconds = 0;
    std::string error;
    std::shared_ptr<const HeightLod> lod;
    std::shared_ptr<const NavigationData> navigation;
    std::shared_ptr<PreparedNavigationSurface> navigationCache;
    size_t PayloadBytes() const;
};
bool PrepareGeometry(Preparation &, const std::vector<ChunkKey> &,
                     const std::function<bool()> &cancelled = {});
struct PreparedAsset {
    Preparation geometry;
    std::string asset, fingerprint;
    bool consumed = false;
};
bool PrepareAsset(const std::string &path, const std::string &identity, PreparedAsset &,
                  std::string &error, const std::function<bool()> &cancelled = {},
                  const NavigationInput *navigation = nullptr);
} // namespace TerrainV2
