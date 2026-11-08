#pragma once
#include "TerrainSource.h"
#include <limits>
namespace TerrainV2 {
// Internal immutable-page extraction after preparation validated source ownership.
bool ExtractValidatedChunk(const Source &, ChunkKey, Chunk &, std::string &,
                           const std::function<bool()> &cancelled = {});
// Validates a shared immutable source once for a preparation batch. Failure or
// cancellation never replaces the caller's previously complete chunk array.
bool BuildChunks(const Source &, const std::vector<ChunkKey> &, std::vector<Chunk> &, std::string &,
                 const std::function<bool()> &cancelled = {},
                 size_t maximumTriangles = std::numeric_limits<size_t>::max());
} // namespace TerrainV2
