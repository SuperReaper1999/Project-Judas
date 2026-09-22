#include "PilotControl.h"

#include "PhysicsWorld.h"
#include "PlayerController.h"

void HandlePilotToggleRequest(FlyingPrimitiveControl& control, PilotAttachment& attachment,
                               PlayerController& player, PhysicsWorld& physics) {
    if (control.controlled) {
        // Release is always available — orientation, support, and gravity
        // context are irrelevant (see docs/ARCHITECTURE.md, "Milestone
        // 11," section on detachment). Compute the player's inherited
        // point velocity from the spacecraft's CURRENT authoritative state
        // before clearing the attachment, so nothing in between reads a
        // stale mid-transition value.
        const BodyTransform shipTransform = physics.GetTransform(control.handle);
        const glm::vec3 shipLinearVelocity = physics.GetLinearVelocity(control.handle);
        const glm::vec3 shipAngularVelocity = physics.GetAngularVelocity(control.handle);
        const glm::vec3 releaseVelocity = ComputePilotReleaseVelocity(
            shipTransform, shipLinearVelocity, shipAngularVelocity, player.GetPosition());
        player.SetVelocityAfterRelease(releaseVelocity);
        attachment.attached = false;
        control.controlled = false;
    } else if (player.IsGrounded() && player.GetSupportBodyHandle().id == control.handle.id) {
        // Same support-gated acquisition rule Milestone 8 established: F
        // from anywhere else is a no-op.
        const BodyTransform shipTransform = physics.GetTransform(control.handle);
        BeginPilotAttachment(attachment, shipTransform, player.GetPosition(), player.GetOrientation());
        control.controlled = true;
    }
}

void AdvancePlayerForPiloting(const FlyingPrimitiveControl& control, PilotAttachment& attachment,
                               PlayerController& player, PhysicsWorld& physics, const Window& window,
                               const GravityField& gravity, float fixedDeltaTime) {
    if (control.controlled && attachment.attached) {
        const BodyTransform shipTransform = physics.GetTransform(control.handle);
        glm::vec3 newPosition;
        glm::quat newOrientation;
        ApplyPilotAttachment(attachment, shipTransform, newPosition, newOrientation);
        player.FixedUpdateAttached(newPosition, newOrientation);
    } else {
        player.FixedUpdate(window, physics, gravity, fixedDeltaTime, true);
    }
}
