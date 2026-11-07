#include "RuntimeWorld.h"
#include "TerrainRuntime.h"
#include <algorithm>
#include <cmath>

namespace TerrainV2 {
bool Runtime::Surface(glm::vec3 point, const SurfaceBrush &brush, std::string &error) {
    if (!source || !Definition()) {
        error = "terrain not ready";
        return false;
    }
    if (pending) {
        error = "terrain edit pending; retry after publication";
        return false;
    }
    const auto &pose = Definition()->transform;
    const auto local = glm::conjugate(pose.rotation) * (point - pose.position);
    auto next = std::make_shared<Source>(*source);
    std::vector<ChunkKey> dirty;
    if (!ApplySurfaceBrush(*next, local, brush, dirty, error))
        return false;
    // Only asset-backed saves need sparse overrides. Procedural sources already
    // save all their native samples. Never accept an edit that cannot be saved.
    auto changes = raster;
    if (!baseAsset.empty()) {
        for (size_t i = 0; i < next->heights.size(); ++i)
            if (next->heights[i] != source->heights[i] || next->weights[i] != source->weights[i])
                changes[uint32_t(i)] = {next->heights[i], next->weights[i]};
        if (changes.size() > MaximumRasterChanges) {
            error = "landscape exceeds 65536 persistent changed samples; split editable regions";
            return false;
        }
    }
    if (!Submit(std::move(next), std::move(dirty), error))
        return false;
    pending->raster = std::move(changes);
    return true;
}
} // namespace TerrainV2
