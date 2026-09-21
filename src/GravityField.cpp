#include "GravityField.h"

glm::vec3 GravityField::Sample(const glm::vec3& worldPosition) const {
    // Milestone 3: a constant field, uniform everywhere. worldPosition is
    // unused for now but is already part of the interface because a real
    // (radial, multi-source) gravity field needs it — see GravityField.h.
    (void)worldPosition;
    return glm::vec3(0.0f, -9.81f, 0.0f);
}
