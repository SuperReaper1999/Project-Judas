#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Interactable.h"
#include "PhysicsWorld.h"

class DynamicBody;

// Small gameplay boundary for manipulating a deliberately supplied set of
// physics bodies. Bodies remain in PhysicsWorld and are carried by forces.
class ObjectManipulation {
public:
    explicit ObjectManipulation(std::vector<BodyHandle> eligibleBodies);

    bool CanPickUp(BodyHandle handle, const PhysicsWorld& physics) const;
    bool TryPickUp(BodyHandle handle, const PhysicsWorld& physics);
    bool IsHolding() const { return m_held.IsValid(); }
    BodyHandle HeldBody() const { return m_held; }
    void Drop();
    bool Throw(PhysicsWorld& physics, const glm::vec3& lookDirection, float throwSpeed);
    void ApplyCarryForce(PhysicsWorld& physics, const glm::vec3& target,
                         const glm::vec3& targetVelocity) const;
    // Optional attitude hold for a held rigid body. The target is supplied
    // by player/look presentation semantics; only torque enters physics.
    void ApplyCarryOrientationTorque(PhysicsWorld& physics, const glm::quat& target) const;

private:
    std::vector<BodyHandle> m_eligibleBodies;
    BodyHandle m_held;
};

glm::vec3 ComputeCarryTarget(const glm::vec3& playerPosition, const glm::quat& playerOrientation,
                             const glm::vec3& lookDirection, float eyeHeight, float carryDistance);
// Local +Y follows the view's up, so looking downward physically tips an
// ordinary held body. No world-axis or cup-specific state is involved.
glm::quat ComputeCarryOrientation(const glm::quat& playerOrientation,
                                   const glm::vec3& lookDirection);

class PickupInteractable final : public Interactable {
public:
    PickupInteractable(DynamicBody& body, ObjectManipulation& manipulation, const PhysicsWorld& physics);
    glm::vec3 GetInteractionPoint() const override;
    float GetInteractionRadius() const override { return 2.8f; }
    std::string GetPromptText() const override;
    bool CanInteract() const override;
    void Interact() override;

private:
    DynamicBody& m_body;
    ObjectManipulation& m_manipulation;
    const PhysicsWorld& m_physics;
};
