#pragma once
#include "TerrainLod.h"
#include "TerrainSource.h"
#include <memory>

struct CollisionAsset;
namespace TerrainV2 {
struct PreparedChunk {
    Chunk chunk;
    std::shared_ptr<CollisionAsset> collision;
};
// Worker-only construction. Completed products contain no world, renderer or
// body handles. The coordinator moves them into a single runtime owner.
struct Preparation {
    std::shared_ptr<const Source> source;
    std::vector<PreparedChunk> chunks;
    double milliseconds = 0;
    std::string error;
    std::shared_ptr<const HeightLod> lod;
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
                  std::string &error, const std::function<bool()> &cancelled = {});
} // namespace TerrainV2
