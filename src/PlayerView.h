#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

enum class PlayerViewMode { ThirdPerson, FirstPerson };

// View switching is presentation state only. Keeping this transition as a
// tiny pure operation makes the menu ownership rule directly testable.
inline void ApplyPlayerViewToggle(PlayerViewMode& mode, bool requested, bool gameplayOwnsInput) {
    if (!requested || !gameplayOwnsInput) return;
    mode = mode == PlayerViewMode::ThirdPerson ? PlayerViewMode::FirstPerson
                                               : PlayerViewMode::ThirdPerson;
}

struct PlayerCameraPose {
    glm::vec3 position{0.0f};
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
};

// Computes the camera from the player's supplied pose and existing free-look
// angles. +Y is the player's local/model up axis, transformed by orientation.
PlayerCameraPose ComputePlayerCameraPose(const glm::vec3& position, const glm::quat& orientation,
                                         float yawDegrees, float pitchDegrees,
                                         PlayerViewMode mode, float eyeHeight,
                                         float followDistance, float thirdPersonHeightOffset);
