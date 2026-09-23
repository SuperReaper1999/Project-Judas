#include "LightTransforms.h"

glm::vec3 TransformLocalLightPosition(const glm::vec3& ownerPosition, const glm::quat& ownerOrientation,
                                       const glm::vec3& localOffset) {
    return ownerPosition + ownerOrientation * localOffset;
}

glm::vec3 TransformLocalLightDirection(const glm::quat& ownerOrientation,
                                        const glm::vec3& localDirection) {
    return glm::normalize(ownerOrientation * localDirection);
}

void ComputeTorchTransform(const glm::vec3& basePosition, const glm::quat& baseOrientation,
                            float yawDegrees, float pitchDegrees, float eyeHeight,
                            glm::vec3& outPosition, glm::vec3& outDirection) {
    const glm::vec3 localUp = baseOrientation * glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::quat lookOrientation =
        baseOrientation * glm::angleAxis(glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::angleAxis(glm::radians(pitchDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
    outDirection = glm::normalize(lookOrientation * glm::vec3(0.0f, 0.0f, -1.0f));
    outPosition = basePosition + localUp * eyeHeight;
}
