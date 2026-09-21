#include "FaithfulGravity.h"

glm::vec3 FaithfulGravity::Sample(const glm::vec3& worldPosition) const {
    // Constant field: this is this implementation's entire behavior.
    // worldPosition is unused here (a radial implementation would use it
    // instead) but is part of the shared GravityField interface, not
    // something FaithfulGravity itself needed.
    (void)worldPosition;
    return glm::vec3(0.0f, -9.81f, 0.0f);
}
