#pragma once

#include <glm/glm.hpp>

// Judas's own gravity abstraction. The physics middleware's built-in global
// gravity is explicitly disabled (see PhysicsWorld::Init) so that gravity is
// always something Judas computes and hands to physics — never an
// assumption baked into the physics engine itself.
//
// Milestone 3's Sample() returns a constant downward vector, but that
// constant is TEST DATA for this milestone only. It is NOT an engine-wide
// definition that gravity always points toward -Y — see
// docs/ARCHITECTURE.md, "Current gravity." Milestone 4 is expected to
// replace this with radial planetary gravity (accelerations that depend on
// `worldPosition` relative to one or more gravity sources) without
// requiring any change to how a physics body receives gravity — see
// PhysicsWorld::ApplyLinearAcceleration.
class GravityField {
public:
    glm::vec3 Sample(const glm::vec3& worldPosition) const;
};
