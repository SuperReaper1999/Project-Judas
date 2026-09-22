#include "FlyingPrimitiveControl.h"

#include <glm/gtc/quaternion.hpp>

#include "Window.h"

namespace {
// Direct-commanded translation speed and attitude rate while controlled.
// Boring, explicit constants — no thrust/acceleration curve, matching the
// milestone brief's "boring controls are correct." Deliberately the same
// order of magnitude as the player's own kMoveSpeed (PlayerController.cpp)
// rather than something vehicle-fast; the attitude rate is chosen to make
// a deliberate 180-degree roll take a little under a second and a half —
// fast enough to feel responsive, slow enough that the "roll upside down
// and stay attached" human check (see docs/ARCHITECTURE.md, "Milestone
// 11") is easy to actually observe happening, not a instantaneous flip.
constexpr float kFlightSpeed = 8.0f;                    // m/s
constexpr float kAttitudeRateRadiansPerSecond = 1.3f;   // ~74.5 degrees/second per axis
}  // namespace

void ApplyFlyingPrimitiveControl(FlyingPrimitiveControl& control, const Window& window,
                                  PhysicsWorld& physics) {
    if (!control.controlled) return;

    const BodyTransform transform = physics.GetTransform(control.handle);

    // Milestone 11: ALL three body axes come from the spacecraft's own
    // current orientation — never gravity, never a fixed world axis (see
    // docs/ARCHITECTURE.md, "Milestone 11," for the brief's exact
    // requirement this satisfies). `bodyLocalUp` here is a model/body-axis
    // convention (this spacecraft's own "roof" direction), NOT the
    // gravity-derived local up PlayerController and Milestones 8/10's
    // vertical control used — the two are deliberately different concepts
    // now (see law #21 in Project_Persistent_Memory.md): after a 180-degree
    // roll, "local up" translation rolls WITH the spacecraft instead of
    // continuing to mean "away from whatever's pulling on it," exactly as
    // the brief requires.
    const glm::vec3 forward = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 right = transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);

    // --- Translation: full 3-axis, all body-local ---
    // W/S forward/back, A/D strafe left/right, Q/E local up/down — the same
    // six keys (and the same Action enum) Milestone 8 already used, but A/D
    // now mean an actual strafe again (Milestone 8 repurposed them as yaw;
    // Milestone 11 needs true independent yaw control, so it gets its own
    // keys below instead — see src/Window.h).
    glm::vec3 desiredDirection(0.0f);
    if (window.IsActionActive(Action::MoveForward)) desiredDirection += forward;
    if (window.IsActionActive(Action::MoveBackward)) desiredDirection -= forward;
    if (window.IsActionActive(Action::StrafeRight)) desiredDirection += right;
    if (window.IsActionActive(Action::StrafeLeft)) desiredDirection -= right;
    if (window.IsActionActive(Action::MoveUp)) desiredDirection += up;
    if (window.IsActionActive(Action::MoveDown)) desiredDirection -= up;
    if (glm::length(desiredDirection) > 0.0f) {
        desiredDirection = glm::normalize(desiredDirection);
    }
    physics.SetLinearVelocity(control.handle, desiredDirection * kFlightSpeed);

    // --- Rotation: independent pitch/yaw/roll, all body-local ---
    // Each is an ordinary commanded angular velocity about one of the
    // spacecraft's own current axes (right = pitch axis, up = yaw axis,
    // forward = roll axis) — the world-space angular velocity vector is
    // just the sum of each axis times its own rate; there is no gimbal
    // lock or Euler-angle state anywhere here, only the current quaternion
    // orientation and this step's commanded rate (see
    // docs/ARCHITECTURE.md, "Milestone 11," for the sign convention:
    // PitchUp noses the ship toward its own local up, YawLeft turns the
    // nose toward its own local -right (i.e. left, the ordinary sense),
    // RollRight banks the local right axis downward).
    float pitchRate = 0.0f;
    if (window.IsActionActive(Action::PitchUp)) pitchRate += kAttitudeRateRadiansPerSecond;
    if (window.IsActionActive(Action::PitchDown)) pitchRate -= kAttitudeRateRadiansPerSecond;

    float yawRate = 0.0f;
    if (window.IsActionActive(Action::YawLeft)) yawRate += kAttitudeRateRadiansPerSecond;
    if (window.IsActionActive(Action::YawRight)) yawRate -= kAttitudeRateRadiansPerSecond;

    float rollRate = 0.0f;
    if (window.IsActionActive(Action::RollRight)) rollRate += kAttitudeRateRadiansPerSecond;
    if (window.IsActionActive(Action::RollLeft)) rollRate -= kAttitudeRateRadiansPerSecond;

    const glm::vec3 angularVelocity = right * pitchRate + up * yawRate + forward * rollRate;
    physics.SetAngularVelocity(control.handle, angularVelocity);
}
