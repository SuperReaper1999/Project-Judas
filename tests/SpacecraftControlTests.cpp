// Milestone 11: standalone, headless tests for the spacecraft's full
// 6-degree-of-freedom control (src/FlyingPrimitiveControl.*) — a real
// PhysicsWorld (no window/GL needed, see src/PhysicsWorld.cpp) holding one
// dynamic box, and a real Window constructed WITHOUT calling Init (so no
// SDL/GL call ever happens) but with SetTestInputMode(true) — the same
// scripted-input path src/TestHarness.cpp uses for the real game, here
// driven directly instead of through a script file. Because
// ApplyFlyingPrimitiveControl commands velocity directly (no integration,
// no fixedDeltaTime), these tests read the commanded linear/angular
// velocity straight back via PhysicsWorld::GetLinearVelocity/
// GetAngularVelocity without needing to Step() the world at all — exactly
// what's under test either way. Section C reuses the same "rotate the
// whole universe" technique tests/StepClimbTests.cpp and
// tests/PilotAttachmentTests.cpp already established.
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "FlyingPrimitiveControl.h"
#include "PhysicsWorld.h"
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

// Spawns one dynamic box (mass/friction/restitution values are irrelevant
// here — ApplyFlyingPrimitiveControl overrides velocity directly, nothing
// under test ever integrates or resolves a contact) at `orientation`, and
// returns its handle.
BodyHandle SpawnShip(PhysicsWorld& physics, const glm::quat& orientation) {
    const BodyHandle handle =
        physics.CreateDynamicBox(glm::vec3(0.0f), glm::vec3(1.0f), 10.0f, 0.5f, 0.1f);
    physics.ResetBody(handle, glm::vec3(0.0f), orientation);
    return handle;
}

}  // namespace

void TestTranslationIsBodyLocal() {
    std::printf("Section A: translation is body-local, not world-fixed\n");

    PhysicsWorld physics;
    physics.Init();

    const glm::quat shipOrientation =
        glm::angleAxis(glm::radians(60.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const BodyHandle handle = SpawnShip(physics, shipOrientation);

    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);

    FlyingPrimitiveControl control;
    control.handle = handle;
    control.controlled = true;
    ApplyFlyingPrimitiveControl(control, window, physics);

    const glm::vec3 commandedVelocity = physics.GetLinearVelocity(handle);
    const glm::vec3 expectedDirection = shipOrientation * glm::vec3(0.0f, 0.0f, -1.0f);
    CheckVec3Near(glm::normalize(commandedVelocity), expectedDirection, 1.0e-4f,
                  "holding forward commands velocity along the SHIP'S OWN current forward axis, "
                  "not world -Z");
    Check(glm::length(commandedVelocity) > 0.0f, "commanded velocity is non-zero while holding forward");

    physics.Shutdown();
}

void TestAllSixTranslationAxes() {
    std::printf("Section B: all six translation directions map to the correct local axis\n");

    PhysicsWorld physics;
    physics.Init();
    const glm::quat shipOrientation(1.0f, 0.0f, 0.0f, 0.0f);  // identity: local axes == world axes
    const BodyHandle handle = SpawnShip(physics, shipOrientation);

    Window window;
    window.SetTestInputMode(true);
    FlyingPrimitiveControl control;
    control.handle = handle;
    control.controlled = true;

    struct AxisCase {
        Action action;
        glm::vec3 expectedDirection;
        const char* description;
    };
    const AxisCase cases[] = {
        {Action::MoveForward, glm::vec3(0.0f, 0.0f, -1.0f), "forward -> local -Z"},
        {Action::MoveBackward, glm::vec3(0.0f, 0.0f, 1.0f), "backward -> local +Z"},
        {Action::StrafeRight, glm::vec3(1.0f, 0.0f, 0.0f), "strafe right -> local +X"},
        {Action::StrafeLeft, glm::vec3(-1.0f, 0.0f, 0.0f), "strafe left -> local -X"},
        {Action::MoveUp, glm::vec3(0.0f, 1.0f, 0.0f), "up -> local +Y"},
        {Action::MoveDown, glm::vec3(0.0f, -1.0f, 0.0f), "down -> local -Y"},
    };
    for (const AxisCase& testCase : cases) {
        window.SetTestActionState(testCase.action, true);
        ApplyFlyingPrimitiveControl(control, window, physics);
        window.SetTestActionState(testCase.action, false);

        const glm::vec3 commandedVelocity = physics.GetLinearVelocity(handle);
        CheckVec3Near(glm::normalize(commandedVelocity), testCase.expectedDirection, 1.0e-4f,
                      testCase.description);
    }

    physics.Shutdown();
}

void TestReleasingAllInputZeroesVelocity() {
    std::printf("Section C: releasing all translation input commands zero velocity (no drift)\n");

    PhysicsWorld physics;
    physics.Init();
    const BodyHandle handle = SpawnShip(physics, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    Window window;
    window.SetTestInputMode(true);
    FlyingPrimitiveControl control;
    control.handle = handle;
    control.controlled = true;

    window.SetTestActionState(Action::MoveForward, true);
    ApplyFlyingPrimitiveControl(control, window, physics);
    Check(glm::length(physics.GetLinearVelocity(handle)) > 0.0f, "moving while forward is held");

    window.SetTestActionState(Action::MoveForward, false);
    ApplyFlyingPrimitiveControl(control, window, physics);
    CheckVec3Near(physics.GetLinearVelocity(handle), glm::vec3(0.0f), 1.0e-6f,
                  "releasing the key commands exactly zero velocity, not leftover momentum");

    physics.Shutdown();
}

void TestPitchYawRollSigns() {
    std::printf("Section D: pitch/yaw/roll each rotate about the correct body-local axis\n");

    PhysicsWorld physics;
    physics.Init();
    const BodyHandle handle = SpawnShip(physics, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    Window window;
    window.SetTestInputMode(true);
    FlyingPrimitiveControl control;
    control.handle = handle;
    control.controlled = true;

    // Pitch up: commanded angular velocity should point along the ship's
    // own local right (+X at identity orientation) — the axis that, by the
    // right-hand rule, rotates forward (0,0,-1) toward +Y (nose rising).
    window.SetTestActionState(Action::PitchUp, true);
    ApplyFlyingPrimitiveControl(control, window, physics);
    window.SetTestActionState(Action::PitchUp, false);
    glm::vec3 angularVelocity = physics.GetAngularVelocity(handle);
    Check(angularVelocity.x > 0.0f, "PitchUp commands positive angular velocity about local +X");
    CheckNear(angularVelocity.y, 0.0f, 1.0e-5f, "PitchUp contributes nothing about Y");
    CheckNear(angularVelocity.z, 0.0f, 1.0e-5f, "PitchUp contributes nothing about Z");

    window.SetTestActionState(Action::PitchDown, true);
    ApplyFlyingPrimitiveControl(control, window, physics);
    window.SetTestActionState(Action::PitchDown, false);
    angularVelocity = physics.GetAngularVelocity(handle);
    Check(angularVelocity.x < 0.0f, "PitchDown commands negative angular velocity about local +X");

    // Yaw left: about the ship's own local up (+Y at identity).
    window.SetTestActionState(Action::YawLeft, true);
    ApplyFlyingPrimitiveControl(control, window, physics);
    window.SetTestActionState(Action::YawLeft, false);
    angularVelocity = physics.GetAngularVelocity(handle);
    Check(angularVelocity.y > 0.0f, "YawLeft commands positive angular velocity about local +Y");

    window.SetTestActionState(Action::YawRight, true);
    ApplyFlyingPrimitiveControl(control, window, physics);
    window.SetTestActionState(Action::YawRight, false);
    angularVelocity = physics.GetAngularVelocity(handle);
    Check(angularVelocity.y < 0.0f, "YawRight commands negative angular velocity about local +Y");

    // Roll: about the ship's own local forward (-Z at identity) — sign
    // convention is internal (see FlyingPrimitiveControl.cpp), so this only
    // checks that Left/Right are opposite and both act purely on the Z
    // component.
    window.SetTestActionState(Action::RollRight, true);
    ApplyFlyingPrimitiveControl(control, window, physics);
    window.SetTestActionState(Action::RollRight, false);
    const float rollRightZ = physics.GetAngularVelocity(handle).z;

    window.SetTestActionState(Action::RollLeft, true);
    ApplyFlyingPrimitiveControl(control, window, physics);
    window.SetTestActionState(Action::RollLeft, false);
    const float rollLeftZ = physics.GetAngularVelocity(handle).z;

    Check(rollRightZ * rollLeftZ < 0.0f, "RollLeft and RollRight command opposite-signed angular velocity");
    CheckNear(physics.GetAngularVelocity(handle).x, 0.0f, 1.0e-5f, "Roll contributes nothing about X");
    CheckNear(physics.GetAngularVelocity(handle).y, 0.0f, 1.0e-5f, "Roll contributes nothing about Y");

    physics.Shutdown();
}

void TestNoOpWhenNotControlled() {
    std::printf("Section E: a no-op when `controlled` is false\n");

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
    ApplyFlyingPrimitiveControl(control, window, physics);

    CheckVec3Near(physics.GetLinearVelocity(handle), glm::vec3(1.0f, 2.0f, 3.0f), 1.0e-6f,
                  "linear velocity is untouched while not controlled, even with input held");
    CheckVec3Near(physics.GetAngularVelocity(handle), glm::vec3(0.1f, 0.2f, 0.3f), 1.0e-6f,
                  "angular velocity is untouched while not controlled, even with input held");

    physics.Shutdown();
}

void TestRotateTheUniverseEquivalence() {
    std::printf("Section F: rotate-the-universe equivalence for translation control\n");

    PhysicsWorld physicsReference;
    physicsReference.Init();
    const glm::quat referenceShipOrientation =
        glm::angleAxis(glm::radians(25.0f), glm::normalize(glm::vec3(0.2f, 1.0f, 0.1f)));
    const BodyHandle referenceHandle = SpawnShip(physicsReference, referenceShipOrientation);

    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);
    window.SetTestActionState(Action::MoveUp, true);

    FlyingPrimitiveControl referenceControl;
    referenceControl.handle = referenceHandle;
    referenceControl.controlled = true;
    ApplyFlyingPrimitiveControl(referenceControl, window, physicsReference);
    const glm::vec3 referenceVelocity = physicsReference.GetLinearVelocity(referenceHandle);
    physicsReference.Shutdown();

    // Rebuild the identical scenario with the ship's own orientation
    // rotated by kArbitraryRotation — the input state (still "forward +
    // up") is unchanged, since it is expressed relative to the ship, not
    // the world.
    PhysicsWorld physicsRotated;
    physicsRotated.Init();
    const glm::quat rotatedShipOrientation = kArbitraryRotation * referenceShipOrientation;
    const BodyHandle rotatedHandle = SpawnShip(physicsRotated, rotatedShipOrientation);
    FlyingPrimitiveControl rotatedControl;
    rotatedControl.handle = rotatedHandle;
    rotatedControl.controlled = true;
    ApplyFlyingPrimitiveControl(rotatedControl, window, physicsRotated);
    const glm::vec3 rotatedVelocity = physicsRotated.GetLinearVelocity(rotatedHandle);
    physicsRotated.Shutdown();

    const glm::vec3 rotatedBackVelocity = glm::inverse(kArbitraryRotation) * rotatedVelocity;
    CheckVec3Near(rotatedBackVelocity, referenceVelocity, 1.0e-4f,
                  "rotated-back commanded velocity matches the unrotated reference — no hidden "
                  "world-axis assumption in translation control");
}

int main() {
    TestTranslationIsBodyLocal();
    TestAllSixTranslationAxes();
    TestReleasingAllInputZeroesVelocity();
    TestPitchYawRollSigns();
    TestNoOpWhenNotControlled();
    TestRotateTheUniverseEquivalence();

    if (g_failures == 0) {
        std::printf("\nALL TESTS PASSED\n");
        return 0;
    }
    std::printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
