#include "Door.h"

#include <algorithm>
#include <cmath>

#include "HingeTransform.h"
#include "Renderer.h"

namespace {
// World units — generous enough to prompt from a normal walking approach
// distance without needing to stand right on top of the door; the facing
// cone (see src/InteractionSystem.cpp) is what keeps this from selecting
// the door from far off to the side.
constexpr float kInteractionRadius = 3.0f;

float MoveTowards(float current, float target, float maxDelta) {
    if (std::abs(target - current) <= maxDelta) return target;
    return current + (target > current ? maxDelta : -maxDelta);
}
}  // namespace

Door::Door(PhysicsWorld& physics, const glm::vec3& hingeWorldPosition, const glm::quat& baseOrientation,
           const glm::vec3& halfExtents, const glm::vec3& localHingeAxis, float openAngleRadians,
           float angularSpeedRadiansPerSecond, const glm::vec3& color)
    : m_baseOrientation(baseOrientation),
      m_halfExtents(halfExtents),
      m_pivotWorld(hingeWorldPosition),
      m_hingeAxisWorld(glm::normalize(baseOrientation * localHingeAxis)),
      m_color(color),
      m_openAngleRadians(openAngleRadians),
      m_angularSpeedRadiansPerSecond(angularSpeedRadiansPerSecond) {
    // The closed-pose CENTER sits one half-extent (along the door's own
    // local +X) away from the authored hinge edge — see this class's own
    // header comment on the "local -X face is always the hinge edge"
    // authoring convention.
    m_baseCenter = hingeWorldPosition + baseOrientation * glm::vec3(halfExtents.x, 0.0f, 0.0f);

    m_bodyHandle = physics.CreateStaticBox(m_baseCenter, m_baseOrientation, m_halfExtents,
                                            /*friction=*/0.8f, /*restitution=*/0.0f);
}

void Door::Destroy(PhysicsWorld& physics) {
    physics.DestroyBody(m_bodyHandle);
}

void Door::FixedUpdate(PhysicsWorld& physics, float fixedDeltaTime) {
    m_previousAngle = m_currentAngle;

    const float target = m_open ? m_openAngleRadians : 0.0f;
    const float maxDelta = m_angularSpeedRadiansPerSecond * fixedDeltaTime;
    m_currentAngle = MoveTowards(m_currentAngle, target, maxDelta);

    glm::vec3 position;
    glm::quat orientation;
    ComputeHingeTransform(m_baseCenter, m_baseOrientation, m_pivotWorld, m_hingeAxisWorld, m_currentAngle,
                          position, orientation);
    physics.ResetBody(m_bodyHandle, position, orientation);
}

void Door::Draw(Renderer& renderer, float presentationAlpha) const {
    const float angle = glm::mix(m_previousAngle, m_currentAngle, glm::clamp(presentationAlpha, 0.0f, 1.0f));
    glm::vec3 position;
    glm::quat orientation;
    ComputeHingeTransform(m_baseCenter, m_baseOrientation, m_pivotWorld, m_hingeAxisWorld, angle, position,
                          orientation);
    renderer.DrawBox(position, orientation, m_halfExtents, m_color);
}

glm::vec3 Door::GetInteractionPoint() const {
    glm::vec3 position;
    glm::quat orientation;
    ComputeHingeTransform(m_baseCenter, m_baseOrientation, m_pivotWorld, m_hingeAxisWorld, m_currentAngle,
                          position, orientation);
    return position;
}

float Door::GetInteractionRadius() const {
    return kInteractionRadius;
}

std::string Door::GetPromptText() const {
    return m_open ? "Press G to close door" : "Press G to open door";
}
