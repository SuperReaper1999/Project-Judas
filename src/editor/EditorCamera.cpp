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
}  // namespace

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
    m_position = point - Forward() * distance;
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
