#pragma once

#include <vector>
#include <utility>

#include <glm/glm.hpp>

#include "PhysicsWorld.h"

// Pairwise Newtonian gravitation between explicitly selected dynamic bodies.
// This is separate from GravityField: local gameplay gravity remains an
// acceleration sampled by consumers, while this applies mutual forces. The
// selected set may include low-mass test bodies such as the spacecraft.
class CelestialGravity {
public:
    static constexpr float kGravitationalConstant = 6.67430e-11f; // m^3 kg^-1 s^-2

    explicit CelestialGravity(std::vector<BodyHandle> bodies) : m_bodies(std::move(bodies)) {}

    // World-space force exerted on body B by A. Coincident bodies are
    // undefined in point-mass Newtonian gravity; return zero at singularity.
    static glm::vec3 ForceOnB(const glm::vec3& positionA, float massA,
                              const glm::vec3& positionB, float massB);

    // Accumulates equal-and-opposite forces for each valid dynamic pair.
    // Call once per authoritative fixed step before PhysicsWorld::Step.
    void ApplyForces(PhysicsWorld& physics) const;

private:
    std::vector<BodyHandle> m_bodies;
};
