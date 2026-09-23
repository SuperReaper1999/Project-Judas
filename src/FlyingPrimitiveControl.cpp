#include "FlyingPrimitiveControl.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "Window.h"

namespace {
// Milestone 12: control FORCE/TORQUE magnitudes, not speeds/rates — see
// docs/ARCHITECTURE.md, "Milestone 12," for the full derivation and
// measured evidence. Chosen relative to the spacecraft's own real mass
// (kFlyingPrimitiveMass = 80kg, Application.cpp) and real inertia tensor
// (SolidBoxInverseInertia over its 2.0 x 0.25 x 3.0 half-extents box
// collider — see src/RigidBody.cpp), not picked independently of them.
//
// kControlForceMagnitude: 1200N against the spacecraft's real 80kg mass
// gives a nominal acceleration of F/m = 15 m/s^2 along whichever direction
// is held. This number went through a real revision while validating this
// milestone: an initial, more "comfortable-looking" 320N (nominal 4 m/s^2,
// deliberately the same order of magnitude Milestone 11's old
// kFlightSpeed of 8 m/s was) turned out to be PHYSICALLY UNFLYABLE —
// resting on the plank under this demo's 9.81 m/s^2 gravity needs at
// least mass*g = 784.8N of upward thrust just to counter gravity, so 320N
// upward left the spacecraft permanently unable to lift off at all, ever
// (a*net = F/m - g would be negative), and separately, its own friction
// (0.8 coefficient against ~785N of normal force ≈ 628N of available
// grip) was enough to fully cancel 320N of horizontal thrust too — see
// docs/ARCHITECTURE.md, "Milestone 12, Remaining limitations," for the
// full measured diagnosis. 1200N clears both thresholds with real margin
// (net ~5.2 m/s^2 climb straight up against gravity; ~572N of net forward
// thrust after friction while still resting flat, per the same measured
// diagnosis) without being so far beyond them that flight feels
// artificially violent once actually airborne.
constexpr float kControlForceMagnitude = 1200.0f;  // N

// kControlTorqueMagnitude: one shared magnitude applied about whichever
// local axis (pitch/yaw/roll) is held — the spacecraft's own REAL inverse
// inertia tensor (not a per-axis constant chosen here) is what makes pitch,
// yaw, and roll actually accelerate at different rates, per the brief's
// explicit requirement. 450 N*m was chosen so the AXIS WITH THE MOST
// ROTATIONAL INERTIA (yaw, about the box's long/wide axes) reaches roughly
// the same ~1.3 rad/s angular speed in about a second that Milestone 11
// directly commanded, while roll (the least rotationally resistant axis
// for this flat, wide box) visibly spins up faster for the identical
// torque — see docs/ARCHITECTURE.md, "Milestone 12, Rotational inertia,"
// for the measured per-axis numbers. Unlike kControlForceMagnitude above,
// this was NOT increased to guarantee it can overcome the plank's own
// resting friction: measured directly, spinning in place while still
// flat on the ground needs upward of ~2500-3000 N*m to break static
// friction's torque at this coefficient, well past what free-flight
// responsiveness calls for. Rather than inflate this constant to a value
// that would feel twitchy once genuinely airborne just to make ground-
// resting rotation possible, this is documented honestly as a real,
// physically correct limitation instead — see "Remaining limitations":
// rotating in place while still resting flat is realistically stiff, the
// same way a parked aircraft doesn't pivot under its own control surfaces
// alone; ascending (which DOES work at this magnitude once genuinely
// clear of the ground — inertia does not fight friction once there is no
// more contact to generate it) resolves it immediately.
constexpr float kControlTorqueMagnitude = 450.0f;  // N*m

// Critically damped attitude hold in angular-acceleration space. Converting
// the commanded acceleration through the actual world inertia tensor keeps
// SAS equally responsive about the ship's high- and low-inertia axes.
constexpr float kSasProportionalGain = 25.0f;  // 1/s^2
constexpr float kSasDerivativeGain = 10.0f;    // 1/s

glm::vec3 ShortestRotationVector(const glm::quat& target, const glm::quat& current) {
    glm::quat error = glm::normalize(target * glm::inverse(current));
    if (error.w < 0.0f) error = -error;

    const glm::vec3 imaginary(error.x, error.y, error.z);
    const float sinHalfAngle = glm::length(imaginary);
    if (sinHalfAngle < 1.0e-6f) return imaginary * 2.0f;

    const float angle = 2.0f * std::atan2(sinHalfAngle,
                                          std::clamp(error.w, -1.0f, 1.0f));
    return imaginary * (angle / sinHalfAngle);
}
}  // namespace

void SetSpacecraftSasEnabled(FlyingPrimitiveControl& control, bool enabled,
                              const PhysicsWorld& physics) {
    control.sasEnabled = enabled;
    if (enabled && control.handle.IsValid()) {
        control.sasTargetOrientation = physics.GetTransform(control.handle).rotation;
    }
}

void ApplyFlyingPrimitiveControl(FlyingPrimitiveControl& control, const Window& window,
                                  PhysicsWorld& physics) {
    if (!control.controlled && !control.sasEnabled) return;

    const BodyTransform transform = physics.GetTransform(control.handle);

    // SAS is an active torque controller and remains active after pilot
    // release once enabled. It never applies a force or writes velocity.
    if (control.sasEnabled) {
        const glm::vec3 attitudeError =
            ShortestRotationVector(control.sasTargetOrientation, transform.rotation);
        const glm::vec3 angularVelocity = physics.GetAngularVelocity(control.handle);
        const glm::vec3 angularAcceleration = kSasProportionalGain * attitudeError -
                                              kSasDerivativeGain * angularVelocity;
        physics.ApplyTorque(control.handle,
                            physics.GetInertiaWorld(control.handle) * angularAcceleration);
    }

    if (!control.controlled) return;

    // Milestone 11/12: ALL three body axes come from the spacecraft's own
    // current orientation — never gravity, never a fixed world axis. See
    // docs/ARCHITECTURE.md, "Milestone 11"/"Milestone 12."
    const glm::vec3 forward = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 right = transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);

    // --- Translation: a single net local-space FORCE, not a commanded
    // velocity. Combining multiple held directions is normalized BEFORE
    // scaling by the force magnitude (unchanged from Milestone 11's own
    // diagonal-input handling) so holding two axes at once doesn't apply
    // more total force than holding one. PhysicsWorld::ApplyForce adds
    // this to the body's force accumulator; PhysicsWorld::Step turns it
    // into an actual velocity change via F=ma using the spacecraft's real
    // mass — see docs/ARCHITECTURE.md, "Milestone 12, Translational
    // physics." Calling this with a zero vector (nothing held) is a
    // harmless no-op — it does NOT zero the spacecraft's existing
    // velocity, unlike Milestone 11's unconditional SetLinearVelocity.
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
    physics.ApplyForce(control.handle, desiredDirection * kControlForceMagnitude);

    // --- Rotation: a single net local-space TORQUE, not a commanded
    // angular velocity. Each held axis contributes the SAME fixed
    // magnitude (kControlTorqueMagnitude) about its own current world-space
    // direction; the three contributions are summed, not normalized —
    // these are three independent physical rotation axes, not one input
    // direction the way translation's six keys are, so a diagonal
    // pitch+roll input genuinely should apply more net torque than either
    // alone, the same way two real RCS thrusters firing together would.
    // PhysicsWorld::ApplyTorque adds this to the body's torque accumulator;
    // PhysicsWorld::Step turns it into an angular velocity change via the
    // spacecraft's REAL inverse inertia tensor (not a scalar) — see
    // docs/ARCHITECTURE.md, "Milestone 12, Rotational inertia."
    // SAS owns attitude while enabled. Pilot translation remains available;
    // turn SAS off to apply the raw M12 rotational torque controls again.
    float pitchTorque = 0.0f;
    if (!control.sasEnabled && window.IsActionActive(Action::PitchUp)) pitchTorque += kControlTorqueMagnitude;
    if (!control.sasEnabled && window.IsActionActive(Action::PitchDown)) pitchTorque -= kControlTorqueMagnitude;

    float yawTorque = 0.0f;
    if (!control.sasEnabled && window.IsActionActive(Action::YawLeft)) yawTorque += kControlTorqueMagnitude;
    if (!control.sasEnabled && window.IsActionActive(Action::YawRight)) yawTorque -= kControlTorqueMagnitude;

    float rollTorque = 0.0f;
    if (!control.sasEnabled && window.IsActionActive(Action::RollRight)) rollTorque += kControlTorqueMagnitude;
    if (!control.sasEnabled && window.IsActionActive(Action::RollLeft)) rollTorque -= kControlTorqueMagnitude;

    const glm::vec3 torque = right * pitchTorque + up * yawTorque + forward * rollTorque;
    physics.ApplyTorque(control.handle, torque);
}
