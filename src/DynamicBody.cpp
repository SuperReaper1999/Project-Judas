#include "DynamicBody.h"

#include <algorithm>

#include "GravityField.h"

DynamicBody::DynamicBody(BodyHandle handle, const Visual& visual, const glm::vec3& spawnPosition,
                          const glm::quat& spawnRotation)
    : m_handle(handle),
      m_visual(visual),
      m_spawnPosition(spawnPosition),
      m_spawnRotation(spawnRotation),
      m_position(spawnPosition),
      m_orientation(spawnRotation),
      m_previousPosition(spawnPosition),
      m_previousOrientation(spawnRotation) {}

glm::vec3 DynamicBody::GetPresentedPosition(float alpha) const {
    return glm::mix(m_previousPosition, m_position, std::clamp(alpha, 0.0f, 1.0f));
}

glm::quat DynamicBody::GetPresentedOrientation(float alpha) const {
    return glm::slerp(m_previousOrientation, m_orientation, std::clamp(alpha, 0.0f, 1.0f));
}

void DynamicBody::SnapshotPrevious() {
    m_previousPosition = m_position;
    m_previousOrientation = m_orientation;
}

void DynamicBody::SyncFromPhysics(const PhysicsWorld& physics) {
    const BodyTransform transform = physics.GetTransform(m_handle);
    m_position = transform.position;
    m_orientation = transform.rotation;
}

void DynamicBody::ResetToSpawn(PhysicsWorld& physics) {
    physics.ResetBody(m_handle, m_spawnPosition, m_spawnRotation);
    m_position = m_spawnPosition;
    m_orientation = m_spawnRotation;
    m_previousPosition = m_spawnPosition;
    m_previousOrientation = m_spawnRotation;
}

void PrepareDynamicBodiesForStep(std::vector<DynamicBody>& bodies, const GravityField& gravity,
                                  PhysicsWorld& physics, float fixedDeltaTime) {
    for (DynamicBody& body : bodies) {
        body.SnapshotPrevious();
        const glm::vec3 acceleration = gravity.Sample(body.GetPosition());
        physics.ApplyLinearAcceleration(body.Handle(), acceleration, fixedDeltaTime);
    }
}

void SyncDynamicBodiesFromPhysics(std::vector<DynamicBody>& bodies, const PhysicsWorld& physics) {
    for (DynamicBody& body : bodies) {
        body.SyncFromPhysics(physics);
    }
}
