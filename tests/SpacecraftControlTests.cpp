// Milestone 12: standalone, headless tests for the spacecraft's force/
// torque-driven inertia (src/FlyingPrimitiveControl.*, and the generic
// PhysicsWorld::ApplyForce/ApplyTorque this milestone added). A real
// PhysicsWorld (no window/GL needed, see src/PhysicsWorld.cpp) holding one
// dynamic box, and a real Window constructed WITHOUT calling Init (so no
// SDL/GL call ever happens) but with SetTestInputMode(true) — the same
// scripted-input path src/TestHarness.cpp uses for the real game, here
// driven directly instead of through a script file.
//
// Milestone 11's version of this file asserted the OLD direct-velocity/
// direct-angular-velocity control semantics (ApplyFlyingPrimitiveControl
// immediately commanded PhysicsWorld::GetLinearVelocity to a known value,
// with no PhysicsWorld::Step() call needed). Milestone 12 deliberately
// replaces that control model with real F=ma physics — see
// docs/ARCHITECTURE.md, "Milestone 12" — so EVERY test below now calls
// PhysicsWorld::Step() after ApplyFlyingPrimitiveControl and reads back the
// resulting velocity/angular-velocity/position, the same way real gameplay
// actually experiences it. This is not a weakened test: it exercises the
// real control -> force/torque -> accumulator -> integration -> velocity
// pipeline end to end, not just the control layer's own intent.
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "FlyingPrimitiveControl.h"
#include "GravityField.h"
#include "PhysicsWorld.h"
#include "RigidBody.h"
#include "Window.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* description) {
    if (condition) {
        std::printf("  OK   %s\n", description);
    } else {
        std::printf("  FAIL %s\n", description);
        ++g_failures;
    }
}

void CheckNear(float a, float b, float tolerance, const char* description) {
    Check(std::abs(a - b) <= tolerance, description);
}

void CheckVec3Near(const glm::vec3& a, const glm::vec3& b, float tolerance, const char* description) {
    Check(glm::length(a - b) <= tolerance, description);
}

const glm::quat kArbitraryRotation =
    glm::angleAxis(glm::radians(53.0f), glm::normalize(glm::vec3(0.4f, 0.8f, -0.3f)));

// Matches Application.cpp's real spacecraft: mass 80kg, box collider
// halfExtents (2.0, 0.25, 3.0) — the same real inertia tensor the actual
// game uses, not a stand-in shape, so "Section H" below is measuring the
// real spacecraft's own rotational response.
constexpr float kShipMass = 80.0f;
const glm::vec3 kShipHalfExtents(2.0f, 0.25f, 3.0f);
constexpr float kFixedDeltaTime = 1.0f / 60.0f;

// Matches src/FlyingPrimitiveControl.cpp's own constants exactly — kept as
// a separate copy here (rather than #include-ing the anonymous-namespace
// constant, which isn't possible) so a future change to the production
// constants makes these tests visibly fail instead of silently drifting;
// see the "measured evidence" sections below, which derive their own
// expected values from mass/inertia + these, not from independently
// guessed numbers.
constexpr float kControlForceMagnitude = 1200.0f;  // N
constexpr float kControlTorqueMagnitude = 450.0f;  // N*m

BodyHandle SpawnShip(PhysicsWorld& physics, const glm::quat& orientation) {
    const BodyHandle handle = physics.CreateDynamicBox(glm::vec3(0.0f), kShipHalfExtents, kShipMass,
                                                         0.5f, 0.1f);
    physics.ResetBody(handle, glm::vec3(0.0f), orientation);
    return handle;
}

// A trivial constant-acceleration GravityField, local to this test file —
// exactly what PrepareDynamicBodiesForStep would sample and hand to
// PhysicsWorld::ApplyLinearAcceleration for a real dynamic body each fixed
// step; reproduced here by hand (rather than pulling in FaithfulGravity)
// since this file only needs one fixed value, and to keep this test's
// dependency footprint to exactly PhysicsWorld/FlyingPrimitiveControl.
struct ConstantGravity : public GravityField {
    glm::vec3 acceleration{0.0f};
    glm::vec3 Sample(const glm::vec3&) const override { return acceleration; }
};

}  // namespace

void TestForceProducesMeasuredAcceleration() {
    std::printf("Section A: sustained thrust produces velocity matching F/m*t (not a speed snap)\n");

    PhysicsWorld physics;
    physics.Init();
    const BodyHandle handle = SpawnShip(physics, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);

    FlyingPrimitiveControl control;
    control.handle = handle;
    control.controlled = true;

    const int kSteps = 60;  // 1 second of held thrust
    for (int i = 0; i < kSteps; ++i) {
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }

    const glm::vec3 velocity = physics.GetLinearVelocity(handle);
    const float expectedAcceleration = kControlForceMagnitude / kShipMass;
    const float expectedSpeed = expectedAcceleration * (kSteps * kFixedDeltaTime);
    const glm::vec3 expectedDirection(0.0f, 0.0f, -1.0f);  // world -Z at identity orientation

    CheckNear(glm::length(velocity), expectedSpeed, expectedSpeed * 0.02f,
              "measured speed after 1s of thrust matches (F/m)*t within 2%");
    CheckVec3Near(glm::normalize(velocity), expectedDirection, 1.0e-3f,
                  "velocity direction matches the ship's forward axis (world -Z at identity)");
    Check(glm::length(velocity) > 0.5f,
          "velocity actually built up gradually rather than snapping instantly to a fixed speed "
          "(single-step delta would be far smaller than the 1s total)");
}

void TestLinearCoastAfterReleasingThrust() {
    std::printf("Section B: releasing all translational input does not brake — velocity persists\n");

    PhysicsWorld physics;
    physics.Init();
    const BodyHandle handle = SpawnShip(physics, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);
    FlyingPrimitiveControl control;
    control.handle = handle;
    control.controlled = true;

    for (int i = 0; i < 30; ++i) {
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }
    const glm::vec3 velocityAtRelease = physics.GetLinearVelocity(handle);
    Check(glm::length(velocityAtRelease) > 0.0f, "built up real velocity before releasing thrust");

    // Release: no translation key held. ApplyFlyingPrimitiveControl is
    // still called every step (as the real game loop would), but with
    // nothing held it contributes zero force.
    window.SetTestActionState(Action::MoveForward, false);
    for (int i = 0; i < 120; ++i) {  // 2 full seconds of coasting
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }
    const glm::vec3 velocityAfterCoast = physics.GetLinearVelocity(handle);

    CheckVec3Near(velocityAfterCoast, velocityAtRelease, glm::length(velocityAtRelease) * 0.01f,
                  "velocity is unchanged (within 1%) after 2 full seconds of coasting with no input — "
                  "no hidden damping/drag/braking");
}

void TestOrientationDoesNotRotateExistingVelocity() {
    std::printf("Section C: rotating the spacecraft does not rotate its existing linear velocity\n");

    PhysicsWorld physics;
    physics.Init();
    const BodyHandle handle = SpawnShip(physics, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);
    FlyingPrimitiveControl control;
    control.handle = handle;
    control.controlled = true;
    for (int i = 0; i < 60; ++i) {
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }
    window.SetTestActionState(Action::MoveForward, false);
    const glm::vec3 velocityBeforeTurn = physics.GetLinearVelocity(handle);

    // Rotate the ship 180 degrees about world Y, preserving its existing
    // linear velocity — ResetBody (the only orientation-setting API) also
    // zeroes velocity, so it's captured and restored explicitly. This is a
    // test-only shortcut for an instantaneous turn; real gameplay reaches
    // any orientation gradually via torque (see Section G/Section H), never
    // through this path.
    const glm::quat turned = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    physics.ResetBody(handle, physics.GetTransform(handle).position, turned);
    physics.SetLinearVelocity(handle, velocityBeforeTurn);

    // No thrust applied — just let it coast through the rotation.
    for (int i = 0; i < 10; ++i) {
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }
    const glm::vec3 velocityAfterTurn = physics.GetLinearVelocity(handle);

    CheckVec3Near(velocityAfterTurn, velocityBeforeTurn, glm::length(velocityBeforeTurn) * 0.01f,
                  "linear velocity is UNCHANGED by a 180-degree orientation change with no thrust "
                  "applied — momentum belongs to the world, not the ship's nose");
}

void TestCounterThrustSlowsStopsAndReverses() {
    std::printf("Section D: counter-thrust (facing backwards) slows, stops, then reverses velocity\n");

    PhysicsWorld physics;
    physics.Init();
    const BodyHandle handle = SpawnShip(physics, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);
    FlyingPrimitiveControl control;
    control.handle = handle;
    control.controlled = true;
    for (int i = 0; i < 60; ++i) {
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }
    const glm::vec3 velocityBeforeTurn = physics.GetLinearVelocity(handle);
    const float speedBeforeTurn = glm::length(velocityBeforeTurn);
    Check(speedBeforeTurn > 0.0f, "built up real forward velocity before turning around");

    // Turn 180 degrees (same test-only shortcut as Section C), still
    // holding MoveForward — since the ship's own "forward" is now the
    // opposite world direction, continuing to hold forward now means
    // thrusting directly against the existing velocity.
    const glm::quat turned = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    physics.ResetBody(handle, physics.GetTransform(handle).position, turned);
    physics.SetLinearVelocity(handle, velocityBeforeTurn);

    // Enough steps for F/m*t to exceed the original speed and reverse it:
    // a = 4 m/s^2, need > speedBeforeTurn/a seconds; give it generous
    // headroom.
    const float secondsNeeded = (speedBeforeTurn / (kControlForceMagnitude / kShipMass)) * 1.5f + 0.5f;
    const int steps = static_cast<int>(secondsNeeded / kFixedDeltaTime) + 1;

    bool crossedNearZero = false;
    bool monotonicWhileDecelerating = true;
    float previousSpeedAlongOriginal = speedBeforeTurn;
    const glm::vec3 originalDirection = glm::normalize(velocityBeforeTurn);
    for (int i = 0; i < steps; ++i) {
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
        const float speedAlongOriginal = glm::dot(physics.GetLinearVelocity(handle), originalDirection);
        if (std::abs(speedAlongOriginal) < 0.05f) crossedNearZero = true;
        // Speed along the original direction must be monotonically
        // decreasing (never bouncing back up) until it reverses sign —
        // continuous deceleration, not a sudden brake.
        if (speedAlongOriginal >= 0.0f && speedAlongOriginal > previousSpeedAlongOriginal + 1.0e-4f) {
            monotonicWhileDecelerating = false;
        }
        previousSpeedAlongOriginal = speedAlongOriginal;
    }
    Check(monotonicWhileDecelerating,
          "speed along the original direction decreases monotonically across all steps while "
          "decelerating (never bounces back up)");

    const glm::vec3 finalVelocity = physics.GetLinearVelocity(handle);
    Check(crossedNearZero, "velocity along the original direction passed through approximately zero");
    Check(glm::dot(finalVelocity, originalDirection) < -0.5f,
          "after sustained counter-thrust, velocity has reversed to the opposite direction");
}

void TestPerpendicularThrustAddsNewComponent() {
    std::printf("Section E: perpendicular thrust adds a new velocity component without erasing the old one\n");

    PhysicsWorld physics;
    physics.Init();
    const BodyHandle handle = SpawnShip(physics, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);
    FlyingPrimitiveControl control;
    control.handle = handle;
    control.controlled = true;
    for (int i = 0; i < 60; ++i) {
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }
    const glm::vec3 originalVelocity = physics.GetLinearVelocity(handle);
    window.SetTestActionState(Action::MoveForward, false);

    // Rotate 90 degrees about world Y (test-only shortcut, see Section C),
    // preserving velocity, then thrust in the NEW forward direction —
    // perpendicular to the original travel direction.
    const glm::quat turned = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    physics.ResetBody(handle, physics.GetTransform(handle).position, turned);
    physics.SetLinearVelocity(handle, originalVelocity);

    window.SetTestActionState(Action::MoveForward, true);
    for (int i = 0; i < 30; ++i) {
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }
    const glm::vec3 finalVelocity = physics.GetLinearVelocity(handle);

    const glm::vec3 originalDirection = glm::normalize(originalVelocity);
    const glm::vec3 newThrustDirection = turned * glm::vec3(0.0f, 0.0f, -1.0f);
    Check(std::abs(glm::dot(originalDirection, newThrustDirection)) < 1.0e-3f,
          "sanity: the new thrust direction is genuinely perpendicular to the original velocity");

    const float originalComponentBefore = glm::dot(originalVelocity, originalDirection);
    const float originalComponentAfter = glm::dot(finalVelocity, originalDirection);
    const float newComponentAfter = glm::dot(finalVelocity, newThrustDirection);

    CheckNear(originalComponentAfter, originalComponentBefore, originalComponentBefore * 0.02f,
              "the ORIGINAL velocity component is preserved (within 2%), not erased by new thrust");
    Check(newComponentAfter > 0.5f,
          "a NEW velocity component has grown along the new thrust direction");
    Check(glm::length(finalVelocity) > glm::length(originalVelocity),
          "the resulting speed is the vector sum, not a replacement — trajectory bends, it doesn't snap");
}

void TestMassResponseFollowsFOverM() {
    std::printf("Section F: identical force produces acceleration proportional to 1/mass\n");

    PhysicsWorld physics;
    physics.Init();
    const BodyHandle lightHandle =
        physics.CreateDynamicBox(glm::vec3(-10.0f, 0.0f, 0.0f), kShipHalfExtents, 20.0f, 0.5f, 0.1f);
    const BodyHandle heavyHandle =
        physics.CreateDynamicBox(glm::vec3(10.0f, 0.0f, 0.0f), kShipHalfExtents, 80.0f, 0.5f, 0.1f);

    const glm::vec3 force(0.0f, 0.0f, -240.0f);
    const int kSteps = 30;
    for (int i = 0; i < kSteps; ++i) {
        physics.ApplyForce(lightHandle, force);
        physics.ApplyForce(heavyHandle, force);
        physics.Step(kFixedDeltaTime);
    }

    const float elapsed = kSteps * kFixedDeltaTime;
    const float lightSpeed = glm::length(physics.GetLinearVelocity(lightHandle));
    const float heavySpeed = glm::length(physics.GetLinearVelocity(heavyHandle));
    const float expectedLightSpeed = (glm::length(force) / 20.0f) * elapsed;
    const float expectedHeavySpeed = (glm::length(force) / 80.0f) * elapsed;

    CheckNear(lightSpeed, expectedLightSpeed, expectedLightSpeed * 0.02f,
              "20kg body's speed matches F/m*t within 2%");
    CheckNear(heavySpeed, expectedHeavySpeed, expectedHeavySpeed * 0.02f,
              "80kg body's speed matches F/m*t within 2%");
    CheckNear(lightSpeed / heavySpeed, 4.0f, 0.1f,
              "the 20kg body (1/4 the mass) accelerates to ~4x the speed of the 80kg body under "
              "identical force — F=ma, not a fixed speed cap");
}

void TestAngularCoastAfterReleasingTorque() {
    std::printf("Section G: releasing all rotational input does not stop rotation — angular velocity persists\n");

    PhysicsWorld physics;
    physics.Init();
    const BodyHandle handle = SpawnShip(physics, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::YawLeft, true);
    FlyingPrimitiveControl control;
    control.handle = handle;
    control.controlled = true;
    for (int i = 0; i < 60; ++i) {
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }
    const glm::vec3 angularVelocityAtRelease = physics.GetAngularVelocity(handle);
    Check(glm::length(angularVelocityAtRelease) > 0.0f, "built up real angular velocity before releasing torque");

    window.SetTestActionState(Action::YawLeft, false);
    for (int i = 0; i < 120; ++i) {
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }
    const glm::vec3 angularVelocityAfterCoast = physics.GetAngularVelocity(handle);

    CheckVec3Near(angularVelocityAfterCoast, angularVelocityAtRelease,
                  glm::length(angularVelocityAtRelease) * 0.02f,
                  "angular velocity is unchanged (within 2%) after 2 seconds with no rotational input — "
                  "no auto-level/attitude-hold/damping");
}

void TestCounterTorqueSlowsStopsAndReversesRotation() {
    std::printf("Section H: counter-torque slows, stops, then reverses angular velocity\n");

    PhysicsWorld physics;
    physics.Init();
    const BodyHandle handle = SpawnShip(physics, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::YawLeft, true);
    FlyingPrimitiveControl control;
    control.handle = handle;
    control.controlled = true;
    for (int i = 0; i < 60; ++i) {
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }
    const float angularSpeedBefore = glm::length(physics.GetAngularVelocity(handle));
    Check(angularSpeedBefore > 0.0f, "built up real angular velocity before applying counter-torque");

    window.SetTestActionState(Action::YawLeft, false);
    window.SetTestActionState(Action::YawRight, true);  // opposing torque about the same (world) axis here
    for (int i = 0; i < 180; ++i) {  // generous headroom to guarantee a reversal
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }
    const glm::vec3 finalAngularVelocity = physics.GetAngularVelocity(handle);

    Check(finalAngularVelocity.y < -0.05f,
          "sustained counter-torque reversed the angular velocity's sign about the yaw axis");
}

void TestInertiaTensorProducesAxisDependentResponse() {
    std::printf("Section I: torque about different body axes produces DIFFERENT angular acceleration, "
                "matching the real inverse inertia tensor\n");

    // Same closed-form the production RigidBody uses (src/RigidBody.cpp) —
    // reused directly, not re-derived independently, so this test measures
    // "does the engine use its own real tensor" rather than re-asserting
    // the formula against itself.
    const glm::mat3 expectedInverseInertia = SolidBoxInverseInertia(kShipMass, kShipHalfExtents);

    struct AxisCase {
        Action action;
        int inertiaComponentIndex;  // 0=Ixx (pitch, about local X), 1=Iyy (yaw, about local Y), 2=Izz (roll, about local Z)
        const char* name;
    };
    const AxisCase cases[] = {
        {Action::PitchUp, 0, "pitch (about local X)"},
        {Action::YawLeft, 1, "yaw (about local Y)"},
        {Action::RollRight, 2, "roll (about local Z)"},
    };

    float measuredAngularSpeed[3] = {0.0f, 0.0f, 0.0f};
    for (int c = 0; c < 3; ++c) {
        PhysicsWorld physics;
        physics.Init();
        const BodyHandle handle = SpawnShip(physics, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        Window window;
        window.SetTestInputMode(true);
        window.SetTestActionState(cases[c].action, true);
        FlyingPrimitiveControl control;
        control.handle = handle;
        control.controlled = true;

        const int kSteps = 30;
        for (int i = 0; i < kSteps; ++i) {
            ApplyFlyingPrimitiveControl(control, window, physics);
            physics.Step(kFixedDeltaTime);
        }
        measuredAngularSpeed[c] = glm::length(physics.GetAngularVelocity(handle));

        const float expectedInverseInertiaComponent =
            expectedInverseInertia[cases[c].inertiaComponentIndex][cases[c].inertiaComponentIndex];
        const float expectedAngularSpeed =
            kControlTorqueMagnitude * expectedInverseInertiaComponent * (kSteps * kFixedDeltaTime);
        char description[256];
        std::snprintf(description, sizeof(description),
                      "%s angular speed after 0.5s matches torque * inverseInertia * t within 2%%",
                      cases[c].name);
        CheckNear(measuredAngularSpeed[c], expectedAngularSpeed, expectedAngularSpeed * 0.02f, description);
        physics.Shutdown();
    }

    // The whole point: these three should NOT be equal — a flat, wide box
    // (2.0 x 0.25 x 3.0 half-extents) genuinely resists rotation about its
    // three axes by different amounts, and the same fixed torque must
    // produce visibly different angular speeds as a result.
    Check(std::abs(measuredAngularSpeed[0] - measuredAngularSpeed[1]) > 0.01f &&
              std::abs(measuredAngularSpeed[1] - measuredAngularSpeed[2]) > 0.01f &&
              std::abs(measuredAngularSpeed[0] - measuredAngularSpeed[2]) > 0.01f,
          "pitch/yaw/roll all produce MEASURABLY DIFFERENT angular speeds under identical torque — "
          "real tensor, not a uniform scalar");
}

void TestGravityAndThrustCompose() {
    std::printf("Section J: gravity and control thrust compose additively, neither replaces the other\n");

    // Reference: gravity alone (no thrust), matches ordinary
    // PrepareDynamicBodiesForStep + PhysicsWorld::Step behavior exactly.
    PhysicsWorld gravityOnly;
    gravityOnly.Init();
    const BodyHandle gravityOnlyHandle = SpawnShip(gravityOnly, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    ConstantGravity gravity;
    gravity.acceleration = glm::vec3(0.0f, -9.81f, 0.0f);
    const int kSteps = 30;
    for (int i = 0; i < kSteps; ++i) {
        gravityOnly.ApplyLinearAcceleration(gravityOnlyHandle, gravity.acceleration, kFixedDeltaTime);
        gravityOnly.Step(kFixedDeltaTime);
    }
    const glm::vec3 gravityOnlyVelocity = gravityOnly.GetLinearVelocity(gravityOnlyHandle);
    gravityOnly.Shutdown();

    // Thrust alone (no gravity) — ship already faces world -Z, thrust
    // pushes along that axis.
    PhysicsWorld thrustOnly;
    thrustOnly.Init();
    const BodyHandle thrustOnlyHandle = SpawnShip(thrustOnly, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);
    FlyingPrimitiveControl thrustControl;
    thrustControl.handle = thrustOnlyHandle;
    thrustControl.controlled = true;
    for (int i = 0; i < kSteps; ++i) {
        ApplyFlyingPrimitiveControl(thrustControl, window, thrustOnly);
        thrustOnly.Step(kFixedDeltaTime);
    }
    const glm::vec3 thrustOnlyVelocity = thrustOnly.GetLinearVelocity(thrustOnlyHandle);
    thrustOnly.Shutdown();

    // Combined: both act on the same body, same order real gameplay uses
    // (gravity applied via ApplyLinearAcceleration BEFORE the control's
    // ApplyForce, both integrated together by the same Step() call).
    PhysicsWorld combined;
    combined.Init();
    const BodyHandle combinedHandle = SpawnShip(combined, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    FlyingPrimitiveControl combinedControl;
    combinedControl.handle = combinedHandle;
    combinedControl.controlled = true;
    for (int i = 0; i < kSteps; ++i) {
        combined.ApplyLinearAcceleration(combinedHandle, gravity.acceleration, kFixedDeltaTime);
        ApplyFlyingPrimitiveControl(combinedControl, window, combined);
        combined.Step(kFixedDeltaTime);
    }
    const glm::vec3 combinedVelocity = combined.GetLinearVelocity(combinedHandle);
    combined.Shutdown();

    const glm::vec3 expectedCombined = gravityOnlyVelocity + thrustOnlyVelocity;
    CheckVec3Near(combinedVelocity, expectedCombined, glm::length(expectedCombined) * 0.02f,
                  "combined gravity+thrust velocity equals the sum of each effect measured alone "
                  "(within 2%) — composition, not replacement");
    Check(glm::length(combinedVelocity) > glm::length(thrustOnlyVelocity) - 1.0e-3f ||
              glm::dot(combinedVelocity, thrustOnlyVelocity) > 0.0f,
          "thrust's own contribution is still present in the combined result — gravity did not "
          "override control");
}

void TestNoOpWhenNotControlled() {
    std::printf("Section K: a no-op when `controlled` is false — no force, no torque, nothing to undo\n");

    PhysicsWorld physics;
    physics.Init();
    const BodyHandle handle = SpawnShip(physics, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    physics.SetLinearVelocity(handle, glm::vec3(1.0f, 2.0f, 3.0f));
    physics.SetAngularVelocity(handle, glm::vec3(0.1f, 0.2f, 0.3f));

    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);
    window.SetTestActionState(Action::PitchUp, true);

    FlyingPrimitiveControl control;
    control.handle = handle;
    control.controlled = false;
    for (int i = 0; i < 10; ++i) {
        ApplyFlyingPrimitiveControl(control, window, physics);
        physics.Step(kFixedDeltaTime);
    }

    CheckVec3Near(physics.GetLinearVelocity(handle), glm::vec3(1.0f, 2.0f, 3.0f), 1.0e-5f,
                  "linear velocity is untouched while not controlled, even with input held for 10 steps");
    CheckVec3Near(physics.GetAngularVelocity(handle), glm::vec3(0.1f, 0.2f, 0.3f), 1.0e-5f,
                  "angular velocity is untouched while not controlled, even with input held for 10 steps");
}

void TestRotateTheUniverseEquivalence() {
    std::printf("Section L: rotate-the-universe equivalence for force/torque-driven control\n");

    PhysicsWorld physicsReference;
    physicsReference.Init();
    const glm::quat referenceShipOrientation =
        glm::angleAxis(glm::radians(25.0f), glm::normalize(glm::vec3(0.2f, 1.0f, 0.1f)));
    const BodyHandle referenceHandle = SpawnShip(physicsReference, referenceShipOrientation);

    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);
    window.SetTestActionState(Action::MoveUp, true);
    window.SetTestActionState(Action::YawLeft, true);

    FlyingPrimitiveControl referenceControl;
    referenceControl.handle = referenceHandle;
    referenceControl.controlled = true;
    const int kSteps = 30;
    for (int i = 0; i < kSteps; ++i) {
        ApplyFlyingPrimitiveControl(referenceControl, window, physicsReference);
        physicsReference.Step(kFixedDeltaTime);
    }
    const glm::vec3 referenceVelocity = physicsReference.GetLinearVelocity(referenceHandle);
    const glm::vec3 referenceAngularVelocity = physicsReference.GetAngularVelocity(referenceHandle);
    const glm::quat referenceOrientation = physicsReference.GetTransform(referenceHandle).rotation;
    physicsReference.Shutdown();

    PhysicsWorld physicsRotated;
    physicsRotated.Init();
    const glm::quat rotatedShipOrientation = kArbitraryRotation * referenceShipOrientation;
    const BodyHandle rotatedHandle = SpawnShip(physicsRotated, rotatedShipOrientation);
    FlyingPrimitiveControl rotatedControl;
    rotatedControl.handle = rotatedHandle;
    rotatedControl.controlled = true;
    for (int i = 0; i < kSteps; ++i) {
        ApplyFlyingPrimitiveControl(rotatedControl, window, physicsRotated);
        physicsRotated.Step(kFixedDeltaTime);
    }
    const glm::vec3 rotatedVelocity = physicsRotated.GetLinearVelocity(rotatedHandle);
    const glm::vec3 rotatedAngularVelocity = physicsRotated.GetAngularVelocity(rotatedHandle);
    const glm::quat rotatedOrientation = physicsRotated.GetTransform(rotatedHandle).rotation;
    physicsRotated.Shutdown();

    const glm::vec3 rotatedBackVelocity = glm::inverse(kArbitraryRotation) * rotatedVelocity;
    const glm::vec3 rotatedBackAngularVelocity = glm::inverse(kArbitraryRotation) * rotatedAngularVelocity;
    const glm::quat rotatedBackOrientation = glm::inverse(kArbitraryRotation) * rotatedOrientation;

    CheckVec3Near(rotatedBackVelocity, referenceVelocity, glm::length(referenceVelocity) * 0.02f,
                  "rotated-back linear velocity matches the unrotated reference");
    CheckVec3Near(rotatedBackAngularVelocity, referenceAngularVelocity,
                  glm::length(referenceAngularVelocity) * 0.02f,
                  "rotated-back angular velocity matches the unrotated reference");
    CheckNear(glm::abs(glm::dot(rotatedBackOrientation, referenceOrientation)), 1.0f, 1.0e-2f,
              "rotated-back orientation matches the unrotated reference — no hidden world-axis "
              "assumption in force/torque-driven spacecraft control");
}

int main() {
    TestForceProducesMeasuredAcceleration();
    TestLinearCoastAfterReleasingThrust();
    TestOrientationDoesNotRotateExistingVelocity();
    TestCounterThrustSlowsStopsAndReverses();
    TestPerpendicularThrustAddsNewComponent();
    TestMassResponseFollowsFOverM();
    TestAngularCoastAfterReleasingTorque();
    TestCounterTorqueSlowsStopsAndReversesRotation();
    TestInertiaTensorProducesAxisDependentResponse();
    TestGravityAndThrustCompose();
    TestNoOpWhenNotControlled();
    TestRotateTheUniverseEquivalence();

    if (g_failures == 0) {
        std::printf("\nALL TESTS PASSED\n");
        return 0;
    }
    std::printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
