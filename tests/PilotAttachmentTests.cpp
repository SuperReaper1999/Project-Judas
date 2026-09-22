// Milestone 11: standalone, headless tests for the secured-pilot
// attachment primitives (src/PilotAttachment.*) — pure CPU math, no
// PhysicsWorld simulation, no window/GL context. `BodyTransform` is just a
// position+rotation pair (see src/PhysicsWorld.h), so these are exercised
// directly with hand-crafted transforms, the same way
// tests/StepClimbTests.cpp exercises TryStepMove/TryStepDown directly
// against hand-crafted geometry. Uses the same "rotate the whole universe"
// technique those suites already established: build a reference result,
// rebuild the identical scenario with every position AND orientation
// rotated by an arbitrary quaternion, confirm the rotated-back result
// matches — if rotating everything changes the outcome, a hidden world-
// axis assumption exists somewhere in the code under test.
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "PilotAttachment.h"

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

// An arbitrary, deliberately non-axis-aligned rotation — same role as
// StepClimbTests.cpp's kArbitraryRotation and PhysicsPrimitiveTests.cpp's
// own equivalents: if any formula under test secretly assumed a world axis,
// applying this rotation to the whole scenario would expose it.
const glm::quat kArbitraryRotation =
    glm::angleAxis(glm::radians(53.0f), glm::normalize(glm::vec3(0.4f, 0.8f, -0.3f)));

}  // namespace

void TestBeginAndApplyRoundTrip() {
    std::printf("Section A: BeginPilotAttachment/ApplyPilotAttachment round trip\n");

    const BodyTransform shipTransform{glm::vec3(5.0f, -2.0f, 10.0f),
                                       glm::angleAxis(glm::radians(37.0f), glm::vec3(0.0f, 1.0f, 0.0f))};
    const glm::vec3 playerPosition = shipTransform.position + glm::vec3(1.2f, 0.9f, -0.4f);
    const glm::quat playerOrientation =
        glm::angleAxis(glm::radians(12.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * shipTransform.rotation;

    PilotAttachment attachment;
    BeginPilotAttachment(attachment, shipTransform, playerPosition, playerOrientation);
    Check(attachment.attached, "attachment reports attached after Begin");

    glm::vec3 outPosition;
    glm::quat outOrientation;
    ApplyPilotAttachment(attachment, shipTransform, outPosition, outOrientation);

    CheckVec3Near(outPosition, playerPosition, 1.0e-4f,
                  "applying immediately with the SAME ship transform reproduces the original position");
    CheckNear(glm::abs(glm::dot(outOrientation, playerOrientation)), 1.0f, 1.0e-4f,
              "applying immediately reproduces the original orientation (dot magnitude 1)");
}

void TestTranslationOnly() {
    std::printf("Section B: pure ship translation carries the attachment rigidly\n");

    const BodyTransform shipStart{glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f)};
    const glm::vec3 playerStart = shipStart.position + glm::vec3(2.0f, 0.3f, -1.5f);

    PilotAttachment attachment;
    BeginPilotAttachment(attachment, shipStart, playerStart, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    const glm::vec3 translation(10.0f, -3.0f, 4.0f);
    const BodyTransform shipMoved{shipStart.position + translation, shipStart.rotation};

    glm::vec3 outPosition;
    glm::quat outOrientation;
    ApplyPilotAttachment(attachment, shipMoved, outPosition, outOrientation);

    CheckVec3Near(outPosition, playerStart + translation, 1.0e-4f,
                  "a pure ship translation carries the player by the exact same displacement");
}

void TestOneEightyRollStaysSecured() {
    std::printf("Section C: 180-degree roll under gravity does not detach the pilot\n");

    const BodyTransform shipStart{glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f)};
    // A non-zero, non-trivial offset: standing a little off the ship's own
    // center, above its deck.
    const glm::vec3 playerStart = shipStart.position + glm::vec3(0.5f, 0.9f, -1.0f);

    PilotAttachment attachment;
    BeginPilotAttachment(attachment, shipStart, playerStart, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    // Roll 180 degrees about the ship's own forward (-Z) axis — the deck
    // is now facing straight down.
    const glm::quat rolled = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 0.0f, -1.0f));
    const BodyTransform shipRolled{shipStart.position, rolled};

    glm::vec3 outPosition;
    glm::quat outOrientation;
    ApplyPilotAttachment(attachment, shipRolled, outPosition, outOrientation);

    // A 180-degree rotation about the Z axis maps (x,y,z) -> (-x,-y,z): the
    // player's own offset above the deck flips in BOTH X and Y (not just
    // Y — the whole plane perpendicular to the roll axis inverts), while
    // its component ALONG the roll axis (Z) is unchanged. Still rigidly
    // attached, not detached or left behind.
    const glm::vec3 offset = playerStart - shipStart.position;
    CheckNear(outPosition.x, shipStart.position.x - offset.x, 1.0e-3f,
              "roll: X offset inverts (perpendicular to the roll axis)");
    CheckNear(outPosition.y, shipStart.position.y - offset.y, 1.0e-3f,
              "roll: the player's own offset above the deck inverts in world Y along with it");
    CheckNear(outPosition.z, shipStart.position.z + offset.z, 1.0e-3f,
              "roll: Z offset (along the roll axis) is unchanged");
    Check(attachment.attached, "attachment remains marked attached through the roll (never silently cleared)");
}

void TestRepeatedRotationsNoDrift() {
    std::printf("Section D: repeated rotations accumulate zero attachment drift\n");

    const BodyTransform shipStart{glm::vec3(1.0f, 2.0f, 3.0f),
                                   glm::angleAxis(glm::radians(15.0f), glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f)))};
    const glm::vec3 playerStart = shipStart.position + glm::vec3(0.8f, 0.6f, -1.1f);
    const glm::quat playerOrientationStart =
        glm::angleAxis(glm::radians(5.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * shipStart.rotation;

    PilotAttachment attachment;
    BeginPilotAttachment(attachment, shipStart, playerStart, playerOrientationStart);

    // Simulate many fixed steps' worth of small incremental ship rotation
    // (as continuous angular velocity would produce), applying the
    // attachment fresh each step exactly as PlayerController::FixedUpdateAttached
    // does — never accumulated incrementally.
    glm::quat currentRotation = shipStart.rotation;
    const glm::quat perStepRotation = glm::angleAxis(glm::radians(3.7f), glm::normalize(glm::vec3(0.1f, 1.0f, 0.4f)));
    glm::vec3 finalOutPosition(0.0f);
    glm::quat finalOutOrientation(1.0f, 0.0f, 0.0f, 0.0f);
    for (int step = 0; step < 500; ++step) {
        currentRotation = glm::normalize(perStepRotation * currentRotation);
        const BodyTransform shipNow{shipStart.position, currentRotation};
        ApplyPilotAttachment(attachment, shipNow, finalOutPosition, finalOutOrientation);
    }

    // The offset's WORLD-SPACE LENGTH from the ship's center must be
    // exactly preserved after any number of rotations (a rigid attachment
    // never stretches), and the orientation must remain a unit quaternion
    // — both would drift if the implementation were incremental rather
    // than recomputed fresh from the stored local pose each time.
    const float expectedOffsetLength = glm::length(playerStart - shipStart.position);
    const float actualOffsetLength = glm::length(finalOutPosition - shipStart.position);
    CheckNear(actualOffsetLength, expectedOffsetLength, 1.0e-3f,
              "after 500 incremental rotations, the world-space offset length is exactly preserved");
    CheckNear(glm::length(finalOutOrientation), 1.0f, 1.0e-4f,
              "after 500 incremental rotations, the orientation is still a unit quaternion");
}

void TestRotateTheUniverseEquivalence() {
    std::printf("Section E: rotate-the-universe equivalence\n");

    const BodyTransform shipTransform{glm::vec3(-4.0f, 6.0f, 2.0f),
                                       glm::angleAxis(glm::radians(70.0f), glm::vec3(0.0f, 1.0f, 0.0f))};
    const glm::vec3 playerPosition = shipTransform.position + glm::vec3(-0.3f, 0.8f, 1.6f);
    const glm::quat playerOrientation =
        glm::angleAxis(glm::radians(-20.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * shipTransform.rotation;

    PilotAttachment referenceAttachment;
    BeginPilotAttachment(referenceAttachment, shipTransform, playerPosition, playerOrientation);
    const BodyTransform shipMoved{shipTransform.position + glm::vec3(3.0f, -1.0f, 0.5f),
                                   glm::angleAxis(glm::radians(40.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
                                       shipTransform.rotation};
    glm::vec3 referenceOutPosition;
    glm::quat referenceOutOrientation;
    ApplyPilotAttachment(referenceAttachment, shipMoved, referenceOutPosition, referenceOutOrientation);

    // Rebuild the identical scenario with every position and orientation
    // rotated by kArbitraryRotation.
    const BodyTransform rotatedShipTransform{kArbitraryRotation * shipTransform.position,
                                              kArbitraryRotation * shipTransform.rotation};
    const glm::vec3 rotatedPlayerPosition = kArbitraryRotation * playerPosition;
    const glm::quat rotatedPlayerOrientation = kArbitraryRotation * playerOrientation;

    PilotAttachment rotatedAttachment;
    BeginPilotAttachment(rotatedAttachment, rotatedShipTransform, rotatedPlayerPosition,
                          rotatedPlayerOrientation);
    const BodyTransform rotatedShipMoved{kArbitraryRotation * shipMoved.position,
                                          kArbitraryRotation * shipMoved.rotation};
    glm::vec3 rotatedOutPosition;
    glm::quat rotatedOutOrientation;
    ApplyPilotAttachment(rotatedAttachment, rotatedShipMoved, rotatedOutPosition, rotatedOutOrientation);

    const glm::vec3 rotatedBackPosition = glm::inverse(kArbitraryRotation) * rotatedOutPosition;
    const glm::quat rotatedBackOrientation = glm::inverse(kArbitraryRotation) * rotatedOutOrientation;

    CheckVec3Near(rotatedBackPosition, referenceOutPosition, 1.0e-3f,
                  "rotated-back position matches the unrotated reference position");
    CheckNear(glm::abs(glm::dot(rotatedBackOrientation, referenceOutOrientation)), 1.0f, 1.0e-3f,
              "rotated-back orientation matches the unrotated reference orientation");
}

void TestReleaseVelocityPureTranslation() {
    std::printf("Section F: release velocity — pure translation, zero offset contribution\n");

    const BodyTransform shipTransform{glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f)};
    const glm::vec3 shipLinearVelocity(3.0f, 0.0f, -2.0f);
    const glm::vec3 shipAngularVelocity(0.0f);
    const glm::vec3 playerPosition = shipTransform.position;  // exactly at the ship's own center

    const glm::vec3 releaseVelocity =
        ComputePilotReleaseVelocity(shipTransform, shipLinearVelocity, shipAngularVelocity, playerPosition);
    CheckVec3Near(releaseVelocity, shipLinearVelocity, 1.0e-5f,
                  "with zero angular velocity, release velocity equals the ship's own linear velocity");
}

void TestReleaseVelocityAngularContribution() {
    std::printf("Section G: release velocity — angular contribution at a non-zero offset\n");

    const BodyTransform shipTransform{glm::vec3(1.0f, 2.0f, 3.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f)};
    const glm::vec3 shipLinearVelocity(0.0f, 0.0f, 0.0f);
    const glm::vec3 shipAngularVelocity(0.0f, 2.0f, 0.0f);  // spinning about its own local Y
    const glm::vec3 offset(3.0f, 0.0f, 0.0f);               // 3m out along local X
    const glm::vec3 playerPosition = shipTransform.position + offset;

    const glm::vec3 releaseVelocity =
        ComputePilotReleaseVelocity(shipTransform, shipLinearVelocity, shipAngularVelocity, playerPosition);
    const glm::vec3 expected = glm::cross(shipAngularVelocity, offset);  // omega x r
    CheckVec3Near(releaseVelocity, expected, 1.0e-5f,
                  "release velocity matches v + omega x r for a spinning ship with a non-zero offset");
    Check(glm::length(releaseVelocity) > 1.0e-3f,
          "the angular contribution is not silently dropped (non-zero result for a spinning ship)");
}

void TestReleaseVelocityCombined() {
    std::printf("Section H: release velocity — combined linear and angular motion\n");

    const BodyTransform shipTransform{glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f)};
    const glm::vec3 shipLinearVelocity(5.0f, 1.0f, 0.0f);
    const glm::vec3 shipAngularVelocity(0.0f, 0.0f, 1.5f);
    const glm::vec3 offset(0.0f, 2.0f, 0.0f);
    const glm::vec3 playerPosition = shipTransform.position + offset;

    const glm::vec3 releaseVelocity =
        ComputePilotReleaseVelocity(shipTransform, shipLinearVelocity, shipAngularVelocity, playerPosition);
    const glm::vec3 expected = shipLinearVelocity + glm::cross(shipAngularVelocity, offset);
    CheckVec3Near(releaseVelocity, expected, 1.0e-5f,
                  "release velocity correctly sums the ship's linear velocity and the point's own "
                  "angular contribution, not just one or the other");
}

int main() {
    TestBeginAndApplyRoundTrip();
    TestTranslationOnly();
    TestOneEightyRollStaysSecured();
    TestRepeatedRotationsNoDrift();
    TestRotateTheUniverseEquivalence();
    TestReleaseVelocityPureTranslation();
    TestReleaseVelocityAngularContribution();
    TestReleaseVelocityCombined();

    if (g_failures == 0) {
        std::printf("\nALL TESTS PASSED\n");
        return 0;
    }
    std::printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
