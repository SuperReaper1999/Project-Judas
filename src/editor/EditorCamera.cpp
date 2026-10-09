#include "EditorCamera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "Window.h"

namespace {
constexpr float kLookSensitivityDegreesPerPixel = 0.15f;
constexpr float kNearPlane = 0.1f;
constexpr float kFarPlane = 5000.0f;
constexpr float kFieldOfViewDegrees = 60.0f;
constexpr double kDollyDistanceRatioPerStep = 0.8;
}  // namespace

void EditorCameraGestureState::Cancel() {
    m_active = EditorCameraGesture::None;
    m_waitForRelease = true;
}

void EditorCameraGestureState::Update(const EditorCameraGestureInput& input) {
    const bool rightPressed = input.rightDown && !m_rightWasDown;
    const bool middlePressed = input.middleDown && !m_middleWasDown;
    m_rightWasDown = input.rightDown;
    m_middleWasDown = input.middleDown;

    if (!input.focused || input.cancel) {
        Cancel();
        return;
    }
    if (m_waitForRelease) {
        if (!input.rightDown && !input.middleDown) m_waitForRelease = false;
        return;
    }
    if ((m_active == EditorCameraGesture::Look && !input.rightDown) ||
        (m_active == EditorCameraGesture::Pan && !input.middleDown)) {
        m_active = EditorCameraGesture::None;
        return;
    }
    if (m_active != EditorCameraGesture::None || !input.canStart || input.leftDown) return;
    // Simultaneous buttons are ambiguous: leave that press to editing.
    if (rightPressed && !input.middleDown) m_active = EditorCameraGesture::Look;
    else if (middlePressed && !input.rightDown) m_active = EditorCameraGesture::Pan;
}

bool EditorCameraCanDolly(const EditorCameraGestureInput& input, EditorCameraGesture active, bool editing) {
    return editing && input.focused && input.canStart && !input.cancel &&
        active == EditorCameraGesture::None && !input.leftDown && !input.rightDown && !input.middleDown;
}

glm::vec3 EditorCamera::Forward() const {
    const float yaw = glm::radians(m_yawDegrees);
    const float pitch = glm::radians(m_pitchDegrees);
    return glm::normalize(glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)));
}

glm::vec3 EditorCamera::Right() const {
    return glm::normalize(glm::cross(Forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 EditorCamera::Up() const {
    return glm::normalize(glm::cross(Right(), Forward()));
}

void EditorCamera::Update(const Window& window, float deltaSeconds, bool lookActive, int mouseDeltaX,
                          int mouseDeltaY, bool fast) {
    if (!lookActive) return;
    m_yawDegrees -= static_cast<float>(mouseDeltaX) * kLookSensitivityDegreesPerPixel;
    m_pitchDegrees -= static_cast<float>(mouseDeltaY) * kLookSensitivityDegreesPerPixel;
    m_pitchDegrees = std::clamp(m_pitchDegrees, -89.0f, 89.0f);

    glm::vec3 move(0.0f);
    if (window.IsActionActive(Action::MoveForward)) move += Forward();
    if (window.IsActionActive(Action::MoveBackward)) move -= Forward();
    if (window.IsActionActive(Action::StrafeRight)) move += Right();
    if (window.IsActionActive(Action::StrafeLeft)) move -= Right();
    if (window.IsActionActive(Action::MoveUp)) move += glm::vec3(0.0f, 1.0f, 0.0f);
    if (window.IsActionActive(Action::MoveDown)) move -= glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::length(move) > 0.0f) {
        m_position += glm::normalize(move) * m_moveSpeed * (fast ? 4.0f : 1.0f) * deltaSeconds;
    }
}

glm::mat4 EditorCamera::ViewMatrix() const {
    return glm::lookAt(m_position, m_position + Forward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 EditorCamera::ProjectionMatrix(float aspectRatio) const {
    return glm::perspective(glm::radians(kFieldOfViewDegrees), aspectRatio, kNearPlane, kFarPlane);
}

void EditorCamera::LookAt(const glm::vec3& point, float distance) {
    m_focusDistance = std::max(distance, kNearPlane);
    m_position = point - Forward() * m_focusDistance;
}

void EditorCamera::Pan(int mouseDeltaX, int mouseDeltaY, int viewportHeight) {
    if (viewportHeight <= 0) return;
    const float metresPerPixel = 2.0f * m_focusDistance *
        std::tan(glm::radians(kFieldOfViewDegrees) * 0.5f) / static_cast<float>(viewportHeight);
    m_position += metresPerPixel * (-Right() * static_cast<float>(mouseDeltaX) +
                                   Up() * static_cast<float>(mouseDeltaY));
}

void EditorCamera::Dolly(float wheelSteps) {
    if (!std::isfinite(wheelSteps) || wheelSteps == 0.0f ||
        !std::isfinite(m_focusDistance) || m_focusDistance <= 0.0f) return;
    const auto forward = Forward();
    if (!std::isfinite(forward.x) || !std::isfinite(forward.y) || !std::isfinite(forward.z) ||
        !std::isfinite(m_position.x) || !std::isfinite(m_position.y) || !std::isfinite(m_position.z)) return;

    const double current = m_focusDistance;
    // Clamp in log-distance space before exponentiating: precise fractional
    // wheels remain reversible away from the bounds, while huge finite deltas
    // cannot overflow or pass through the focus. Preserve an existing frame
    // beyond the far-plane limit until an inward step brings it into range.
    const double minimum = std::min(current, static_cast<double>(kNearPlane));
    const double maximum = std::max(current, static_cast<double>(kFarPlane));
    const double logarithm = std::clamp(std::log(current) +
        static_cast<double>(wheelSteps) * std::log(kDollyDistanceRatioPerStep),
        std::log(minimum), std::log(maximum));
    const float next = static_cast<float>(std::clamp(std::exp(logarithm), minimum, maximum));
    m_position += forward * (m_focusDistance - next);
    m_focusDistance = next;
}

void EditorCamera::SetPose(const glm::vec3& position, float yawDegrees, float pitchDegrees) {
    m_position = position;
    m_yawDegrees = yawDegrees;
    m_pitchDegrees = std::clamp(pitchDegrees, -89.0f, 89.0f);
}

void EditorCamera::PixelRay(int pixelX, int pixelY, int viewportWidth, int viewportHeight, glm::vec3& outOrigin,
                            glm::vec3& outDirection) const {
    const float aspect = static_cast<float>(std::max(viewportWidth, 1)) / static_cast<float>(std::max(viewportHeight, 1));
    const float ndcX = (2.0f * (static_cast<float>(pixelX) + 0.5f) / static_cast<float>(std::max(viewportWidth, 1))) - 1.0f;
    const float ndcY = 1.0f - (2.0f * (static_cast<float>(pixelY) + 0.5f) / static_cast<float>(std::max(viewportHeight, 1)));
    const float tanHalf = std::tan(glm::radians(kFieldOfViewDegrees) * 0.5f);
    outOrigin = m_position;
    outDirection = glm::normalize(Forward() + Right() * (ndcX * tanHalf * aspect) + Up() * (ndcY * tanHalf));
}
