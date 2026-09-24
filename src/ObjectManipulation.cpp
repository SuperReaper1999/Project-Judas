#include "ObjectManipulation.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

#include "DynamicBody.h"

namespace {
constexpr float kCarrySpring = 70.0f;
constexpr float kCarryDamping = 17.0f;
constexpr float kMaxCarryAcceleration = 70.0f;
constexpr float kCarryAngularFrequency = 9.0f;
constexpr float kMaxCarryAngularAcceleration = 100.0f;

glm::vec3 ClampMagnitude(const glm::vec3& value, float maximum) {
    const float length = glm::length(value);
    return length > maximum && length > 1.0e-6f ? value * (maximum / length) : value;
}
}  // namespace

ObjectManipulation::ObjectManipulation(std::vector<BodyHandle> eligibleBodies)
    : m_eligibleBodies(std::move(eligibleBodies)) {}

bool ObjectManipulation::CanPickUp(BodyHandle handle, const PhysicsWorld& physics) const {
    return !IsHolding() && std::find_if(m_eligibleBodies.begin(), m_eligibleBodies.end(),
                                        [handle](BodyHandle candidate) { return candidate.id == handle.id; }) !=
                               m_eligibleBodies.end() &&
           physics.IsDynamicBody(handle);
}

bool ObjectManipulation::TryPickUp(BodyHandle handle, const PhysicsWorld& physics) {
    if (!CanPickUp(handle, physics)) return false;
    m_held = handle;
    return true;
}

void ObjectManipulation::Drop() { m_held = BodyHandle{}; }

bool ObjectManipulation::Throw(PhysicsWorld& physics, const glm::vec3& lookDirection, float throwSpeed) {
    if (!IsHolding() || !physics.IsDynamicBody(m_held) || throwSpeed <= 0.0f) return false;
    const float lookLength = glm::length(lookDirection);
    if (lookLength <= 1.0e-6f) return false;
    const BodyHandle body = m_held;
    const glm::vec3 impulse = (lookDirection / lookLength) * physics.GetMass(body) * throwSpeed;
    Drop();
    physics.ApplyLinearImpulse(body, impulse);
    return true;
}

void ObjectManipulation::ApplyCarryForce(PhysicsWorld& physics, const glm::vec3& target,
                                         const glm::vec3& targetVelocity) const {
    if (!IsHolding() || !physics.IsDynamicBody(m_held)) return;
    const glm::vec3 position = physics.GetTransform(m_held).position;
    const glm::vec3 velocity = physics.GetLinearVelocity(m_held);
    const glm::vec3 acceleration = ClampMagnitude((target - position) * kCarrySpring -
                                                      (velocity - targetVelocity) * kCarryDamping,
                                                  kMaxCarryAcceleration);
    physics.ApplyForce(m_held, acceleration * physics.GetMass(m_held));
}

void ObjectManipulation::ApplyCarryOrientationTorque(PhysicsWorld& physics,
                                                       const glm::quat& target) const {
    if (!IsHolding() || !physics.IsDynamicBody(m_held)) return;
    const glm::quat current = glm::normalize(physics.GetTransform(m_held).rotation);
    glm::quat difference = glm::normalize(target * glm::conjugate(current));
    if (difference.w < 0.0f) difference = -difference;  // shortest rotation
    const glm::vec3 vector(difference.x, difference.y, difference.z);
    const float vectorLength = glm::length(vector);
    const float angle = 2.0f * std::atan2(vectorLength, difference.w);
    const glm::vec3 angularError = vectorLength > 1.0e-6f
                                        ? vector * (angle / vectorLength)
                                        : glm::vec3(0.0f);
    const glm::vec3 acceleration = ClampMagnitude(
        angularError * (kCarryAngularFrequency * kCarryAngularFrequency) -
            physics.GetAngularVelocity(m_held) * (2.0f * kCarryAngularFrequency),
        kMaxCarryAngularAcceleration);
    physics.ApplyTorque(m_held, physics.GetInertiaWorld(m_held) * acceleration);
}

glm::vec3 ComputeCarryTarget(const glm::vec3& playerPosition, const glm::quat& playerOrientation,
                             const glm::vec3& lookDirection, float eyeHeight, float carryDistance) {
    const glm::vec3 localUp = playerOrientation * glm::vec3(0.0f, 1.0f, 0.0f);
    const float lookLength = glm::length(lookDirection);
    const glm::vec3 look = lookLength > 1.0e-6f ? lookDirection / lookLength
                                                : playerOrientation * glm::vec3(0.0f, 0.0f, -1.0f);
    return playerPosition + localUp * eyeHeight + look * carryDistance;
}

glm::quat ComputeCarryOrientation(const glm::quat& playerOrientation,
                                   const glm::vec3& lookDirection) {
    const glm::vec3 baseUp = playerOrientation * glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 front = glm::length(lookDirection) > 1.0e-6f
                                ? glm::normalize(lookDirection)
                                : playerOrientation * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 right = glm::cross(front, baseUp);
    if (glm::length(right) < 1.0e-5f) {
        const glm::vec3 baseRight = playerOrientation * glm::vec3(1.0f, 0.0f, 0.0f);
        right = baseRight - front * glm::dot(baseRight, front);
    }
    right = glm::normalize(right);
    const glm::vec3 viewUp = glm::normalize(glm::cross(right, front));
    return glm::normalize(glm::quat_cast(glm::mat3(right, viewUp, -front)));
}

PickupInteractable::PickupInteractable(DynamicBody& body, ObjectManipulation& manipulation,
                                       const PhysicsWorld& physics)
    : m_body(body), m_manipulation(manipulation), m_physics(physics) {}

glm::vec3 PickupInteractable::GetInteractionPoint() const { return m_body.GetPosition(); }

std::string PickupInteractable::GetPromptText() const { return "G Pick up object"; }

bool PickupInteractable::CanInteract() const {
    return m_manipulation.CanPickUp(m_body.Handle(), m_physics);
}

void PickupInteractable::Interact() {
    m_manipulation.TryPickUp(m_body.Handle(), m_physics);
}
