#pragma once

#include <glm/glm.hpp>

#include "GravityField.h"

// Milestone 28: a constant acceleration in an arbitrary configured
// direction. FaithfulGravity (law #2) stays the canonical fixed
// (0,-9.81,0) implementation and is untouched; this is the separate class
// that law reserves for "uniform but not world -Y" — previously a
// composition-root-only type inside Application.cpp (M24's rotated/zero
// fluid-station modes), now an ordinary engine field a scene's uniform
// gravity region can select when its object is rotated or its magnitude is
// not 9.81 m/s^2. A zero vector is a legitimate configuration (an explicit
// zero-gravity region), not an error.
class UniformGravity : public GravityField {
public:
    explicit UniformGravity(const glm::vec3& acceleration) : m_acceleration(acceleration) {}

    glm::vec3 Sample(const glm::vec3&) const override { return m_acceleration; }

private:
    glm::vec3 m_acceleration;
};
