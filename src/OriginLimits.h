#pragma once

#include <cmath>
#include <glm/glm.hpp>

// Native physics and the world transaction must admit the same translation.
// An individually valid destination can still require an unsupported delta.
namespace OriginLimits {
inline constexpr double MaximumComponent = 1e12;

inline bool ValidTranslation(glm::dvec3 delta) {
    return std::isfinite(delta.x) && std::isfinite(delta.y) &&
           std::isfinite(delta.z) &&
           glm::all(glm::lessThanEqual(glm::abs(delta), glm::dvec3(MaximumComponent)));
}
}  // namespace OriginLimits
