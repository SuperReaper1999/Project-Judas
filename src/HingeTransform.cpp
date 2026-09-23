#include "HingeTransform.h"

void ComputeHingeTransform(const glm::vec3& baseCenter, const glm::quat& baseOrientation,
                            const glm::vec3& pivotWorld, const glm::vec3& hingeAxisWorld,
                            float angleRadians, glm::vec3& outPosition, glm::quat& outOrientation) {
    const glm::vec3 axis =
        glm::length(hingeAxisWorld) > 1.0e-6f ? glm::normalize(hingeAxisWorld) : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::quat swing = glm::angleAxis(angleRadians, axis);
    outOrientation = swing * baseOrientation;
    outPosition = pivotWorld + swing * (baseCenter - pivotWorld);
}
