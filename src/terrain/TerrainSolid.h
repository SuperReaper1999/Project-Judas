#pragma once
#include "TerrainSource.h"
#include <glm/gtc/quaternion.hpp>
#include <memory>

namespace TerrainV2 {
// Source-owned game-resolution solid query, independent of visual LOD and
// transient triangle-body handles. Captures the last complete revision only.
// Callers retain the immutable source while preparing their own derived data.
struct SolidSnapshot {
    uint64_t owner = 0, revision = 0;
    std::shared_ptr<const Source> source;
    glm::vec3 position{0};
    glm::quat rotation{1, 0, 0, 0};
    std::shared_ptr<const void> epoch;
    bool Contains(glm::vec3 worldPoint) const;
    bool Intersects(glm::vec3 minimum, glm::vec3 maximum) const;
    bool SameGeometry(const SolidSnapshot &) const;
};
} // namespace TerrainV2
