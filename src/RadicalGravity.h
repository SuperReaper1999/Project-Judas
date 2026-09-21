#pragma once

#include "GravityField.h"

// Judas's second canonical gravity implementation: constant-magnitude
// acceleration always directed toward a configured center point — enough
// to prove GravityField's implementations are genuinely interchangeable
// on a spherical test world. See docs/ARCHITECTURE.md, "Gravity."
//
// Deliberately not physically realistic (no inverse-square falloff, no
// gravitational mass, no orbital mechanics) — this milestone tests that
// gravity DIRECTION can change and everything downstream still works, not
// astrophysics. `magnitude` is explicit configuration, not a universal
// constant.
class RadicalGravity : public GravityField {
public:
    RadicalGravity(const glm::vec3& center, float magnitude);

    glm::vec3 Sample(const glm::vec3& worldPosition) const override;

private:
    glm::vec3 m_center;
    float m_magnitude;
};
