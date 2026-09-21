#pragma once

#include <glm/glm.hpp>

// Judas's own gravity contract. Every gravity implementation — the uniform
// FaithfulGravity (see FaithfulGravity.h), and future radial/planetary or
// composite/multi-source implementations — shares this one interface.
//
// Everything outside the gravity subsystem (PhysicsWorld, Application, and
// anything else that needs an acceleration at a position) consumes gravity
// only through this interface and stays completely agnostic about which
// implementation is active. The physics middleware's built-in global
// gravity is explicitly disabled (see PhysicsWorld::Init) so that gravity
// is always something a GravityField implementation computes and Judas
// hands to physics — never an assumption baked into the physics engine, and
// never tied to any one implementation of this interface. See
// docs/ARCHITECTURE.md, "Ownership boundary."
class GravityField {
public:
    virtual ~GravityField() = default;

    virtual glm::vec3 Sample(const glm::vec3& worldPosition) const = 0;
};
