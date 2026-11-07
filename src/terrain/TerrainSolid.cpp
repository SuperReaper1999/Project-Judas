#include "TerrainSolid.h"
#include <algorithm>
#include <cmath>

namespace TerrainV2 {
bool SolidSnapshot::Contains(glm::vec3 point) const {
    if (!source || !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
        return false;
    const auto local = glm::conjugate(rotation) * (point - position);
    const auto &s = *source;
    const float x = (s.samplesX - 1) * s.spacing * .5f, z = (s.samplesZ - 1) * s.spacing * .5f;
    return std::abs(local.x) <= x && std::abs(local.z) <= z && local.y >= s.bottom &&
           local.y <= s.top && Density(s, local) <= 0;
}
bool SolidSnapshot::Intersects(glm::vec3 minimum, glm::vec3 maximum) const {
    if (!source)
        return false;
    glm::vec3 lo(1e30f), hi(-1e30f);
    const auto &s = *source;
    const float x = (s.samplesX - 1) * s.spacing * .5f, z = (s.samplesZ - 1) * s.spacing * .5f;
    for (int i = 0; i < 8; ++i) {
        const auto p = position + rotation * glm::vec3(i & 1 ? x : -x, i & 2 ? s.top : s.bottom,
                                                       i & 4 ? z : -z);
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    return glm::all(glm::lessThanEqual(lo, maximum)) &&
           glm::all(glm::greaterThanEqual(hi, minimum));
}
bool SolidSnapshot::SameGeometry(const SolidSnapshot &other) const {
    return owner == other.owner && epoch == other.epoch && revision == other.revision &&
           position == other.position && rotation == other.rotation;
}
} // namespace TerrainV2
