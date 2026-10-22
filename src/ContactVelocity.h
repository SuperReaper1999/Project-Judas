#pragma once
#include <glm/glm.hpp>

namespace contact_velocity {
// Direction follows the existing solver's unit-slip convention; the mass sum
// deliberately remains outside the angular quadratic. Do not renormalize here.
// Keeping r x d explicit preserves its exact represented angular null instead
// of expanding/cancelling large coefficients of a cached point-response matrix.
inline float FrictionInverseEffectiveMass(float inverseMassSum,
                                         const glm::vec3& rA, const glm::vec3& rB,
                                         const glm::mat3& inverseInertiaA,
                                         const glm::mat3& inverseInertiaB,
                                         const glm::vec3& direction) {
    const glm::vec3 jacobianA = glm::cross(rA, direction);
    const glm::vec3 jacobianB = glm::cross(rB, direction);
    return inverseMassSum + (glm::dot(jacobianA, inverseInertiaA * jacobianA) +
                             glm::dot(jacobianB, inverseInertiaB * jacobianB));
}
} // namespace contact_velocity
