#pragma once
#include "TerrainSource.h"
namespace TerrainV2 {
// Validates a shared immutable source once for a preparation batch. Failure or
// cancellation never replaces the caller's previously complete chunk array.
bool BuildChunks(const Source &, const std::vector<ChunkKey> &, std::vector<Chunk> &, std::string &,
                 const std::function<bool()> &cancelled = {});
} // namespace TerrainV2
