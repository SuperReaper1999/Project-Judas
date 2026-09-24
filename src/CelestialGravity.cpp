#include "CelestialGravity.h"

#include <cmath>

glm::vec3 CelestialGravity::ForceOnB(const glm::vec3& positionA, float massA,
                                      const glm::vec3& positionB, float massB) {
    if (!(massA > 0.0f) || !(massB > 0.0f) ||
        !std::isfinite(massA) || !std::isfinite(massB)) return glm::vec3(0.0f);
    return massB * AccelerationFromPointMass(
        positionA, kGravitationalConstant * massA, positionB);
}

glm::vec3 CelestialGravity::AccelerationFromPointMass(
    const glm::vec3& sourcePosition, float gravitationalParameter,
    const glm::vec3& bodyPosition) {
    if (!(gravitationalParameter > 0.0f) || !std::isfinite(gravitationalParameter))
        return glm::vec3(0.0f);
    const glm::vec3 displacement = sourcePosition - bodyPosition;
    const float distanceSquared = glm::dot(displacement, displacement);
    if (!(distanceSquared > 0.0f) || !std::isfinite(distanceSquared)) {
        return glm::vec3(0.0f);
    }
    const float magnitude = gravitationalParameter / distanceSquared;
    return displacement * (magnitude / std::sqrt(distanceSquared));
}

void CelestialGravity::ApplyForces(PhysicsWorld& physics) const {
    for (std::size_t i = 0; i < m_bodies.size(); ++i) {
        const BodyHandle a = m_bodies[i];
        if (!physics.IsDynamicBody(a)) continue;
        const BodyTransform transformA = physics.GetTransform(a);
        const float massA = physics.GetMass(a);
        for (std::size_t j = i + 1; j < m_bodies.size(); ++j) {
            const BodyHandle b = m_bodies[j];
            if (!physics.IsDynamicBody(b)) continue;
            const BodyTransform transformB = physics.GetTransform(b);
            const glm::vec3 forceOnB = ForceOnB(transformA.position, massA,
                                                 transformB.position, physics.GetMass(b));
            physics.ApplyForce(a, -forceOnB);
            physics.ApplyForce(b, forceOnB);
        }
    }
}
