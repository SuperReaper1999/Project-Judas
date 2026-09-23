// Milestone 14: standalone, headless tests for the dynamic-lighting MATH
// (src/LightTransforms.*, src/LightAttenuation.*) and the torch's input-
// ownership gating (src/PauseMenu.*) — pure CPU geometry/arithmetic and
// pause-state logic, no window, no GL context, no font loaded. Mirrors
// tests/UITests.cpp's own header comment: this suite deliberately NEVER
// calls a Renderer Draw method (PauseMenu.cpp/Renderer.cpp are linked only
// because the input-ownership section below constructs a real PauseMenu,
// whose own Draw method references Renderer symbols at compile time — see
// CMakeLists.txt). Actual GLSL shader correctness — does the fragment
// shader in Renderer.cpp actually produce the attenuation/cone shape
// src/LightAttenuation.cpp documents as the INTENDED formula — is verified
// by human visual validation instead, per this milestone's own brief; see
// docs/ARCHITECTURE.md, "Milestone 14, Automated evidence," for the
// offscreen-rendering spot-check performed before requesting that
// validation.
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "LightAttenuation.h"
#include "LightTransforms.h"
#include "PauseMenu.h"

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

// Same role as every other standalone suite's own kArbitraryRotation
// (see tests/PilotAttachmentTests.cpp, tests/StepClimbTests.cpp): a
// deliberately non-axis-aligned rotation, so a rotate-the-whole-scenario
// check here would expose any hidden world-axis assumption.
const glm::quat kArbitraryRotation =
    glm::angleAxis(glm::radians(53.0f), glm::normalize(glm::vec3(0.4f, 0.8f, -0.3f)));

}  // namespace

// --- Section A: player torch transform ---
void TestTorchTransformAtIdentity() {
    std::printf("Section A: player torch transform\n");

    const glm::vec3 basePosition(1.0f, 2.0f, 3.0f);
    const glm::quat baseOrientation(1.0f, 0.0f, 0.0f, 0.0f);  // identity

    glm::vec3 position, direction;
    ComputeTorchTransform(basePosition, baseOrientation, 0.0f, 0.0f, 0.7f, position, direction);

    CheckVec3Near(position, basePosition + glm::vec3(0.0f, 0.7f, 0.0f), 1.0e-5f,
                  "at zero yaw/pitch and identity orientation, torch origin is basePosition + eyeHeight*worldUp");
    CheckVec3Near(direction, glm::vec3(0.0f, 0.0f, -1.0f), 1.0e-5f,
                  "at zero yaw/pitch, torch direction is the base orientation's own -Z forward");
}

void TestTorchTransformFollowsYawAndPitch() {
    std::printf("Section A: torch direction follows free-look yaw/pitch\n");

    const glm::vec3 basePosition(0.0f);
    const glm::quat baseOrientation(1.0f, 0.0f, 0.0f, 0.0f);

    glm::vec3 position, direction;
    // 90 degree yaw: -Z forward should become -X (matching GetLookDirection's
    // own documented convention — increasing yaw rotates toward -X).
    ComputeTorchTransform(basePosition, baseOrientation, 90.0f, 0.0f, 0.0f, position, direction);
    CheckVec3Near(direction, glm::vec3(-1.0f, 0.0f, 0.0f), 1.0e-4f,
                  "a 90 degree yaw turns the torch direction from -Z toward -X");

    // 90 degree pitch UP should point the torch toward +Y (straight up).
    ComputeTorchTransform(basePosition, baseOrientation, 0.0f, 90.0f, 0.0f, position, direction);
    CheckVec3Near(direction, glm::vec3(0.0f, 1.0f, 0.0f), 1.0e-4f,
                  "a 90 degree pitch turns the torch direction from -Z toward +Y");
}

void TestTorchTransformRotateTheUniverse() {
    std::printf("Section C: torch transform rotate-the-universe invariance\n");

    const glm::vec3 basePosition(2.0f, -1.0f, 4.0f);
    const glm::quat baseOrientation = glm::angleAxis(glm::radians(20.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    constexpr float kYaw = 35.0f;
    constexpr float kPitch = -12.0f;
    constexpr float kEyeHeight = 0.7f;

    glm::vec3 referencePosition, referenceDirection;
    ComputeTorchTransform(basePosition, baseOrientation, kYaw, kPitch, kEyeHeight, referencePosition,
                          referenceDirection);

    const glm::vec3 rotatedBase = kArbitraryRotation * basePosition;
    const glm::quat rotatedOrientation = kArbitraryRotation * baseOrientation;
    glm::vec3 rotatedPosition, rotatedDirection;
    ComputeTorchTransform(rotatedBase, rotatedOrientation, kYaw, kPitch, kEyeHeight, rotatedPosition,
                          rotatedDirection);

    CheckVec3Near(rotatedPosition, kArbitraryRotation * referencePosition, 1.0e-4f,
                  "rotating the whole scenario rotates the torch's world position identically");
    CheckVec3Near(rotatedDirection, kArbitraryRotation * referenceDirection, 1.0e-4f,
                  "rotating the whole scenario rotates the torch's world direction identically — "
                  "no hidden world-axis assumption");
}

// --- Section B: spacecraft light attachment ---
void TestSpacecraftLightAttachment() {
    std::printf("Section B: spacecraft light attachment (position + direction)\n");

    const glm::vec3 shipPosition(10.0f, 5.0f, -3.0f);
    const glm::quat shipOrientation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 localOffset(0.0f, 0.0f, -3.0f);   // nose, matching Application.cpp's headlight offset
    const glm::vec3 localDirection(0.0f, 0.0f, -1.0f);

    const glm::vec3 worldPosition = TransformLocalLightPosition(shipPosition, shipOrientation, localOffset);
    const glm::vec3 worldDirection = TransformLocalLightDirection(shipOrientation, localDirection);

    // A 90 degree yaw turns local -Z into local -X (world -X here, since
    // the ship orientation IS the yaw): nose offset should now point along
    // world -X from the ship's position, and the headlight direction
    // should also point along world -X.
    CheckVec3Near(worldPosition, shipPosition + glm::vec3(-3.0f, 0.0f, 0.0f), 1.0e-4f,
                  "a 90 degree ship yaw rotates a local nose offset into the correct world position");
    CheckVec3Near(worldDirection, glm::vec3(-1.0f, 0.0f, 0.0f), 1.0e-4f,
                  "the same yaw rotates the headlight's local -Z direction into world -X");
}

void TestSpacecraftLightAttachmentArbitraryRotation() {
    std::printf("Section B: spacecraft light attachment under an arbitrary quaternion\n");

    const glm::vec3 shipPosition(-4.0f, 8.0f, 12.0f);
    const glm::quat shipOrientation = kArbitraryRotation;  // pitch+yaw+roll all at once
    const glm::vec3 localOffset(2.0f, 0.0f, 0.0f);  // starboard wingtip, matching Application.cpp

    const glm::vec3 worldPosition = TransformLocalLightPosition(shipPosition, shipOrientation, localOffset);
    CheckVec3Near(worldPosition, shipPosition + shipOrientation * localOffset, 1.0e-5f,
                  "wingtip world position matches shipPosition + shipOrientation * localOffset exactly, "
                  "for an arbitrary combined pitch/yaw/roll orientation");
}

void TestSpacecraftLightRotateTheUniverse() {
    std::printf("Section C: spacecraft light rotate-the-universe invariance\n");

    const glm::vec3 shipPosition(1.0f, 1.0f, 1.0f);
    const glm::quat shipOrientation = glm::angleAxis(glm::radians(64.0f), glm::normalize(glm::vec3(0.2f, 1.0f, 0.4f)));
    const glm::vec3 localOffset(0.0f, 0.0f, -3.0f);
    const glm::vec3 localDirection(0.0f, 0.0f, -1.0f);

    const glm::vec3 referencePosition = TransformLocalLightPosition(shipPosition, shipOrientation, localOffset);
    const glm::vec3 referenceDirection = TransformLocalLightDirection(shipOrientation, localDirection);

    const glm::vec3 rotatedShipPosition = kArbitraryRotation * shipPosition;
    const glm::quat rotatedShipOrientation = kArbitraryRotation * shipOrientation;
    const glm::vec3 rotatedPosition =
        TransformLocalLightPosition(rotatedShipPosition, rotatedShipOrientation, localOffset);
    const glm::vec3 rotatedDirection = TransformLocalLightDirection(rotatedShipOrientation, localDirection);

    CheckVec3Near(rotatedPosition, kArbitraryRotation * referencePosition, 1.0e-4f,
                  "rotating the whole scenario rotates the headlight's world position identically");
    CheckVec3Near(rotatedDirection, kArbitraryRotation * referenceDirection, 1.0e-4f,
                  "rotating the whole scenario rotates the headlight's world direction identically");
}

// --- Section D: attenuation ---
void TestAttenuationDecreasesWithDistance() {
    std::printf("Section D: distance attenuation\n");

    constexpr float kRange = 10.0f;
    const float near = ComputeDistanceAttenuation(1.0f, kRange);
    const float mid = ComputeDistanceAttenuation(5.0f, kRange);
    const float far = ComputeDistanceAttenuation(9.0f, kRange);
    Check(near > mid && mid > far, "attenuation strictly decreases as distance increases");

    Check(std::isfinite(ComputeDistanceAttenuation(0.0f, kRange)),
          "attenuation at exactly zero distance is finite (no 1/0 singularity)");
    Check(ComputeDistanceAttenuation(0.0f, kRange) > 0.0f, "attenuation at zero distance is positive, not zero");

    CheckNear(ComputeDistanceAttenuation(kRange, kRange), 0.0f, 1.0e-5f,
              "attenuation reaches exactly zero at the configured range");
    CheckNear(ComputeDistanceAttenuation(kRange * 2.0f, kRange), 0.0f, 1.0e-5f,
              "attenuation stays at zero beyond the configured range, doesn't reappear");
}

// --- Section E: spotlight cone ---
void TestSpotConeFactor() {
    std::printf("Section E: spotlight cone factor\n");

    const float innerCos = std::cos(glm::radians(15.0f));
    const float outerCos = std::cos(glm::radians(28.0f));
    const float midCos = std::cos(glm::radians(21.5f));  // roughly halfway between the two angles

    CheckNear(ComputeSpotConeFactor(1.0f, innerCos, outerCos), 1.0f, 1.0e-5f,
              "dead center of the cone (cosAngle = 1) gets full strength");
    CheckNear(ComputeSpotConeFactor(innerCos, innerCos, outerCos), 1.0f, 1.0e-5f,
              "exactly at the inner cone edge, contribution is still full strength");
    CheckNear(ComputeSpotConeFactor(outerCos, innerCos, outerCos), 0.0f, 1.0e-5f,
              "exactly at the outer cone edge, contribution is exactly zero");
    CheckNear(ComputeSpotConeFactor(0.0f, innerCos, outerCos), 0.0f, 1.0e-5f,
              "well outside the cone (90 degrees off-axis) contributes zero");

    const float mid = ComputeSpotConeFactor(midCos, innerCos, outerCos);
    Check(mid > 0.0f && mid < 1.0f, "midway between inner and outer, contribution is strictly between 0 and 1 "
                                     "(a smooth transition, not a binary edge)");

    // Monotonicity across the whole transition — verifies "smoothly falls
    // off" rather than any non-monotonic (e.g. overshooting) curve.
    float previous = 1.0f;
    bool monotonic = true;
    for (int i = 1; i <= 10; ++i) {
        const float angleDegrees = 15.0f + (28.0f - 15.0f) * (static_cast<float>(i) / 10.0f);
        const float cosAngle = std::cos(glm::radians(angleDegrees));
        const float factor = ComputeSpotConeFactor(cosAngle, innerCos, outerCos);
        if (factor > previous + 1.0e-5f) monotonic = false;
        previous = factor;
    }
    Check(monotonic, "the cone factor decreases monotonically from the inner edge to the outer edge");
}

// --- Section F: torch toggle / input ownership ---
//
// Mirrors tests/UITests.cpp's own Section G exactly — the actual boolean
// Application.cpp's real loop gates ACTING ON a drained torch-toggle
// request behind (see docs/ARCHITECTURE.md, "Milestone 14, Input
// ownership"): the request itself is always drained so it can never go
// stale, but it only flips `torchOn` while `!pauseMenu.IsOpen()`.
void TestTorchToggleInputOwnership() {
    std::printf("Section F: torch toggle obeys the same input-ownership boundary as UI input\n");

    PauseMenu menu;
    bool torchOn = false;

    auto handleTorchToggleRequest = [&](bool requested) {
        // Same shape as Application::Run's own gated block: drain always,
        // act only while gameplay owns input.
        if (requested && !menu.IsOpen()) torchOn = !torchOn;
    };

    handleTorchToggleRequest(true);
    Check(torchOn, "torch toggles normally during ordinary gameplay");

    menu.HandleBackRequest();  // open the pause menu
    Check(menu.IsOpen(), "menu is open for the rest of this check");
    handleTorchToggleRequest(true);
    Check(torchOn, "a toggle request while the menu owns input does not change torch state");

    menu.HandleBackRequest();  // close the menu (resume)
    Check(!menu.IsOpen(), "menu closed");
    Check(torchOn, "torch state is unchanged merely by closing the menu (no stale toggle fires on resume)");

    handleTorchToggleRequest(true);
    Check(!torchOn, "gameplay control of the torch is fully restored after resume");
}

int main() {
    TestTorchTransformAtIdentity();
    TestTorchTransformFollowsYawAndPitch();
    TestTorchTransformRotateTheUniverse();
    TestSpacecraftLightAttachment();
    TestSpacecraftLightAttachmentArbitraryRotation();
    TestSpacecraftLightRotateTheUniverse();
    TestAttenuationDecreasesWithDistance();
    TestSpotConeFactor();
    TestTorchToggleInputOwnership();

    if (g_failures == 0) {
        std::printf("\nALL TESTS PASSED\n");
        return 0;
    }
    std::printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
