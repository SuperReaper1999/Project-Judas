#include "FlyingPrimitiveControl.h"

#include <glm/gtc/quaternion.hpp>

#include "GravityField.h"
#include "Window.h"

namespace {
// Direct-commanded translation speed and yaw turn rate while controlled.
// Boring, explicit constants — no thrust/acceleration curve, matching the
// milestone brief's "boring controls are correct." Deliberately the same
// order of magnitude as the player's own kMoveSpeed (PlayerController.cpp)
// rather than something vehicle-fast.
constexpr float kFlightSpeed = 8.0f;              // m/s
constexpr float kYawRateRadiansPerSecond = 1.2f;  // ~69 degrees/second while A/D held
}  // namespace

void ApplyFlyingPrimitiveControl(FlyingPrimitiveControl& control, const Window& window,
                                  PhysicsWorld& physics, const GravityField& gravity) {
    if (!control.controlled) return;

    const BodyTransform transform = physics.GetTransform(control.handle);
    const glm::vec3 forward = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);

    // "Up" for vertical control purposes is the same concept every other
    // consumer in this engine uses for its own local up — whichever way the
    // active GravityField currently points away from, sampled at this
    // body's own position (see docs/ARCHITECTURE.md, "Orientation") — never
    // a hard-coded world axis. In genuinely unclaimed (zero-gravity) space,
    // degenerate-safe fallback to the primitive's own current body-space
    // +Y, mirroring PlayerController::ComputeLocalUp's identical fallback.
    const glm::vec3 sampledGravity = gravity.Sample(transform.position);
    const float gravityLength = glm::length(sampledGravity);
    const glm::vec3 up = gravityLength > 1.0e-6f
                              ? -(sampledGravity / gravityLength)
                              : (transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f));

    // Translation: W/S along the primitive's own current forward axis, Q/E
    // along local up/down. A/D are deliberately NOT strafe here — they
    // yaw the primitive instead (see below) — turning is sufficient
    // navigation per the brief, and reusing exactly two more keys (Q/E)
    // for vertical avoids inventing a third axis of held-key input.
    glm::vec3 desiredDirection(0.0f);
    if (window.IsActionActive(Action::MoveForward)) desiredDirection += forward;
    if (window.IsActionActive(Action::MoveBackward)) desiredDirection -= forward;
    if (window.IsActionActive(Action::MoveUp)) desiredDirection += up;
    if (window.IsActionActive(Action::MoveDown)) desiredDirection -= up;
    if (glm::length(desiredDirection) > 0.0f) {
        desiredDirection = glm::normalize(desiredDirection);
    }
    physics.SetLinearVelocity(control.handle, desiredDirection * kFlightSpeed);

    // Rotation: A/D yaw the primitive about its own current local-gravity
    // "up" axis (not its own body-space +Y, so turning stays level even if
    // the primitive is currently tilted) — reusing the StrafeLeft/
    // StrafeRight actions' physical keys for a different meaning while this
    // object, not the player, has control. This is ordinary commanded
    // angular velocity through the same RigidBody integration path every
    // other rotating body in this engine already uses (see
    // src/RigidBody.h) — not a special rotation mechanism.
    float yawRate = 0.0f;
    if (window.IsActionActive(Action::StrafeLeft)) yawRate += kYawRateRadiansPerSecond;
    if (window.IsActionActive(Action::StrafeRight)) yawRate -= kYawRateRadiansPerSecond;
    physics.SetAngularVelocity(control.handle, up * yawRate);
}
