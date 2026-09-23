#include "PlayerView.h"

#include <glm/gtc/constants.hpp>

PlayerCameraPose ComputePlayerCameraPose(const glm::vec3& position, const glm::quat& orientation,
                                         float yawDegrees, float pitchDegrees,
                                         PlayerViewMode mode, float eyeHeight,
                                         float followDistance, float thirdPersonHeightOffset) {
    PlayerCameraPose pose;
    const glm::vec3 localUp = orientation * glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::quat lookOrientation =
        orientation * glm::angleAxis(glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::angleAxis(glm::radians(pitchDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
    pose.front = glm::normalize(lookOrientation * glm::vec3(0.0f, 0.0f, -1.0f));
    pose.up = localUp;
    const glm::vec3 eye = position + localUp * eyeHeight;
    pose.position = mode == PlayerViewMode::FirstPerson
                        ? eye
                        : eye - pose.front * followDistance + localUp * thirdPersonHeightOffset;
    return pose;
}
