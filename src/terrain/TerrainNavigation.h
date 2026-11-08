#pragma once
#include "NavigationAsset.h"
#include "TerrainSource.h"
#include <map>

struct CollisionAsset;
namespace TerrainV2 {
// Immutable references to the same cooked geometry used by MK2. Keeping these
// references across a job is safe even when the previous world revision retires.
struct NavigationChunk {
    glm::vec3 anchor{0};
    std::shared_ptr<const CollisionAsset> collision;
};
struct NavigationInput {
    NavigationSurfaceSettings settings;
    NavigationProfile profile;
    std::map<ChunkKey, NavigationChunk> chunks;
    std::shared_ptr<const NavigationData> previous;
    std::map<ChunkKey, NavigationChunk> previousChunks;
};
bool ValidateNavigationInput(const NavigationInput &, std::string &);
bool PrepareNavigation(const NavigationInput &, std::shared_ptr<const NavigationData> &,
                       std::string &, const std::function<bool()> &cancelled = {});
size_t NavigationPayloadBytes(const NavigationData *);
// Offline scene bakes also consume the real source, never its identity Box.
bool AppendNavigationSource(const Source &, const SceneTransform &owner,
                            const SceneTransform &frame, NavigationGeometry &, std::string &);
} // namespace TerrainV2
