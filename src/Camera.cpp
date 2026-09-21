#include "Camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "Window.h"

namespace {

// Temporary convention for this one demo camera only — see the note in
// Camera.h. Not an engine-wide "up".
const glm::vec3 kReferenceUp(0.0f, 1.0f, 0.0f);

constexpr float kMoveSpeed = 4.0f;          // world units per second
constexpr float kMouseSensitivity = 0.12f;  // degrees per pixel of mouse motion
constexpr float kFovDegrees = 70.0f;
constexpr float kNearPlane = 0.1f;
constexpr float kFarPlane = 500.0f;
constexpr float kMaxPitchDegrees = 89.0f;

}  // namespace

Camera::Camera(glm::vec3 position, float yawDegrees, float pitchDegrees)
    : m_position(position), m_yaw(yawDegrees), m_pitch(pitchDegrees) {}

glm::vec3 Camera::Front() const {
    const float yawRad = glm::radians(m_yaw);
    const float pitchRad = glm::radians(m_pitch);
    glm::vec3 front;
    front.x = std::cos(pitchRad) * std::cos(yawRad);
    front.y = std::sin(pitchRad);
    front.z = std::cos(pitchRad) * std::sin(yawRad);
    return glm::normalize(front);
}

void Camera::Update(const Window& window, float deltaTime) {
    int mouseDeltaX = 0;
    int mouseDeltaY = 0;
    window.GetMouseDelta(mouseDeltaX, mouseDeltaY);

    // Relative mouse deltas already represent motion accumulated since the
    // last poll, not a held-key rate — so, unlike keyboard movement below,
    // this must NOT be multiplied by deltaTime. Doing so would make look
    // sensitivity vary with frame rate instead of being independent of it.
    m_yaw += static_cast<float>(mouseDeltaX) * kMouseSensitivity;
    m_pitch -= static_cast<float>(mouseDeltaY) * kMouseSensitivity;
    m_pitch = std::clamp(m_pitch, -kMaxPitchDegrees, kMaxPitchDegrees);

    const glm::vec3 front = Front();
    const glm::vec3 right = glm::normalize(glm::cross(front, kReferenceUp));

    glm::vec3 moveDirection(0.0f);
    if (window.IsActionActive(Action::MoveForward)) moveDirection += front;
    if (window.IsActionActive(Action::MoveBackward)) moveDirection -= front;
    if (window.IsActionActive(Action::StrafeRight)) moveDirection += right;
    if (window.IsActionActive(Action::StrafeLeft)) moveDirection -= right;
    if (window.IsActionActive(Action::Ascend)) moveDirection += kReferenceUp;
    if (window.IsActionActive(Action::Descend)) moveDirection -= kReferenceUp;

    if (glm::length(moveDirection) > 0.0f) {
        moveDirection = glm::normalize(moveDirection);
    }

    m_position += moveDirection * kMoveSpeed * deltaTime;
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(m_position, m_position + Front(), kReferenceUp);
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(kFovDegrees), aspectRatio, kNearPlane, kFarPlane);
}
