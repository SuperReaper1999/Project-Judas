#pragma once
#include "CollisionAsset.h"
#include "TerrainCache.h"
#include "TerrainGeometrySpill.h"
#include "TerrainSource.h"

namespace TerrainV2 {
struct GeometryPage {
    Chunk chunk;
    CollisionAsset collision;
    size_t triangles = 0;
    size_t Bytes() const;
};
class GeometryCache : public PageCache<GeometryPage> {
  public:
    explicit GeometryCache(size_t budget)
        : PageCache<GeometryPage>(budget), spill(std::make_shared<GeometrySpillState>()) {}
    std::shared_ptr<GeometrySpillState> spill;
    size_t SpillBytes() const {
        std::lock_guard<std::mutex> lock(spill->mutex);
        return spill->bytes;
    }
};
// Immutable, local recipe. It retains only this chunk's edit neighbourhood,
// never a chain of full world revisions. Heavy products are leased from cache.
struct GeometryRecipe {
    uint64_t identity = 0;
    Source source;
    ChunkKey key;
    std::string fingerprint;
    std::shared_ptr<GeometryCache> cache;
    std::shared_ptr<const GeometrySpill> stored;
    std::shared_ptr<const GeometryPage> Acquire() const;
    std::shared_ptr<const GeometryPage> Build() const;
    size_t Bytes() const;
};
std::shared_ptr<GeometryRecipe> PrepareRecipe(const Source &, ChunkKey,
                                              std::shared_ptr<GeometryCache>, std::string &);
std::shared_ptr<CollisionAsset> CollisionPage(const std::shared_ptr<GeometryRecipe> &,
                                              const GeometryPage &);
} // namespace TerrainV2
