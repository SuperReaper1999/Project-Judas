#include "LightSwitch.h"

#include <algorithm>
#include <cmath>

#include "HingeTransform.h"
#include "Renderer.h"

namespace {
// Smaller than the door's own radius (kept in Door.cpp) — a switch is a
// small object meant to be interacted with at close range, not something
// a player aims at from across a room.
constexpr float kInteractionRadius = 1.5f;

float MoveTowards(float current, float target, float maxDelta) {
    if (std::abs(target - current) <= maxDelta) return target;
    return current + (target > current ? maxDelta : -maxDelta);
}
}  // namespace

LightSwitch::LightSwitch(const glm::vec3& hingeWorldPosition, const glm::quat& baseOrientation,
                          const glm::vec3& halfExtents, const glm::vec3& localHingeAxis,
                          float toggleAngleRadians, float angularSpeedRadiansPerSecond,
                          const glm::vec3& color, const glm::vec3& lampWorldPosition,
                          const glm::vec3& lampColor, float lampRange)
    : m_baseOrientation(baseOrientation),
      m_halfExtents(halfExtents),
      m_pivotWorld(hingeWorldPosition),
      m_hingeAxisWorld(glm::normalize(baseOrientation * localHingeAxis)),
      m_color(color),
      m_toggleAngleRadians(toggleAngleRadians),
      m_angularSpeedRadiansPerSecond(angularSpeedRadiansPerSecond),
      m_lampPosition(lampWorldPosition),
      m_lampColor(lampColor),
      m_lampRange(lampRange) {
    m_baseCenter = hingeWorldPosition + baseOrientation * glm::vec3(halfExtents.x, 0.0f, 0.0f);
}

void LightSwitch::FixedUpdate(float fixedDeltaTime) {
    m_previousAngle = m_currentAngle;
    const float target = m_lampOn ? m_toggleAngleRadians : 0.0f;
    const float maxDelta = m_angularSpeedRadiansPerSecond * fixedDeltaTime;
    m_currentAngle = MoveTowards(m_currentAngle, target, maxDelta);
}

void LightSwitch::Draw(Renderer& renderer, float presentationAlpha) const {
    const float angle = glm::mix(m_previousAngle, m_currentAngle, glm::clamp(presentationAlpha, 0.0f, 1.0f));
    glm::vec3 position;
    glm::quat orientation;
    ComputeHingeTransform(m_baseCenter, m_baseOrientation, m_pivotWorld, m_hingeAxisWorld, angle, position,
                          orientation);
    renderer.DrawBox(position, orientation, m_halfExtents, m_color);
}

glm::vec3 LightSwitch::GetInteractionPoint() const {
    return m_baseCenter;
}

float LightSwitch::GetInteractionRadius() const {
    return kInteractionRadius;
}

std::string LightSwitch::GetPromptText() const {
    return m_lampOn ? "Press G to turn off light" : "Press G to turn on light";
}
