#pragma once
#include "TerrainSource.h"

namespace TerrainV2 {
struct SurfaceBrush {
    enum class Kind { Raise, Lower, Flatten, Smooth, Paint };
    Kind kind = Kind::Raise;
    float radius = 2, strength = .25f, height = 0;
    unsigned layer = 0;
};
// One deterministic dab. Editor gestures schedule dabs by distance and own
// undo snapshots; gameplay owns timing. Returns only affected geometry chunks.
bool ApplySurfaceBrush(Source &, glm::vec3 point, const SurfaceBrush &,
                       std::vector<ChunkKey> &dirty, std::string &);
} // namespace TerrainV2
