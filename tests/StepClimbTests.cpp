// Milestone 10: standalone, headless tests for the step-up/step-down
// primitives (src/StepClimb.*) — no window, no GL context, just a real
// PhysicsWorld (see src/PhysicsWorld.cpp: no GL dependency at all) with a
// handful of static bodies. Same spirit as tests/PhysicsPrimitiveTests.cpp
// and tests/CollisionTests.cpp, including their signature technique: build
// a reference result, then rebuild the IDENTICAL scenario rotated by an
// arbitrary quaternion (every position AND every body's own orientation),
// and confirm the rotated-back result matches — if rotating the whole
// universe changes the outcome, a hidden world-axis assumption exists
// somewhere in the code under test. See docs/ARCHITECTURE.md, "Milestone
// 10," for the full design.
#include <cmath>
#include <cstdio>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "PhysicsWorld.h"
#include "StepClimb.h"

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

constexpr float kCapsuleRadius = 0.3f;
constexpr float kCapsuleHalfHeight = 0.6f;
constexpr float kGroundOffset = kCapsuleHalfHeight + kCapsuleRadius;  // capsule center above a floor it rests on
constexpr float kMaxStepHeight = 0.55f;
constexpr float kMinGroundDot = 0.643f;
constexpr float kSkinMargin = 0.02f;

// A small, self-contained scenario: a large flat floor plus one raised
// static box (the "step") a few meters ahead, both built at a canonical
// (+Y up, +Z forward) layout and then rotated by `rotation` — identity for
// the reference case, arbitrary for the "rotated universe" case. Returns
// everything TryStepMove/TryStepDown need, already in the rotated frame.
struct Scenario {
    std::unique_ptr<PhysicsWorld> physics;
    glm::vec3 localUp;
    glm::quat capsuleOrientation;
    glm::vec3 startPosition;
    glm::vec3 horizontalDisplacement;
};

Scenario BuildStepScenario(float stepHeight, const glm::quat& rotation) {
    Scenario scenario;
    scenario.physics = std::make_unique<PhysicsWorld>();
    scenario.physics->Init();
    scenario.physics->CreatePlayerShape(kCapsuleRadius, kCapsuleHalfHeight);

    const glm::vec3 floorHalfExtents(20.0f, 0.5f, 20.0f);
    const glm::vec3 floorLocalCenter(0.0f, -0.5f, 0.0f);
    scenario.physics->CreateStaticBox(rotation * floorLocalCenter, rotation, floorHalfExtents, 0.8f,
                                       0.0f);

    const glm::vec3 stepHalfExtents(2.0f, stepHeight * 0.5f, 1.0f);
    const glm::vec3 stepLocalCenter(0.0f, stepHeight * 0.5f, 4.0f);  // near face at local z=3
    scenario.physics->CreateStaticBox(rotation * stepLocalCenter, rotation, stepHalfExtents, 0.8f,
                                       0.0f);

    scenario.localUp = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    scenario.capsuleOrientation = rotation;
    scenario.startPosition = rotation * glm::vec3(0.0f, kGroundOffset, 0.0f);
    scenario.horizontalDisplacement = rotation * glm::vec3(0.0f, 0.0f, 3.0f);
    return scenario;
}

// A scenario with a single "ledge" the player starts standing right at the
// edge of, floor continuing `dropHeight` lower beyond it — for TryStepDown.
Scenario BuildLedgeScenario(float dropHeight, const glm::quat& rotation) {
    Scenario scenario;
    scenario.physics = std::make_unique<PhysicsWorld>();
    scenario.physics->Init();
    scenario.physics->CreatePlayerShape(kCapsuleRadius, kCapsuleHalfHeight);

    // Upper floor: local z in [-20, 0], top at local y=0.
    scenario.physics->CreateStaticBox(rotation * glm::vec3(0.0f, -0.5f, -10.0f), rotation,
                                       glm::vec3(20.0f, 0.5f, 10.0f), 0.8f, 0.0f);
    // Lower floor: local z in [0, 20], top at local y=-dropHeight.
    scenario.physics->CreateStaticBox(
        rotation * glm::vec3(0.0f, -dropHeight - 0.5f, 10.0f), rotation,
        glm::vec3(20.0f, 0.5f, 20.0f), 0.8f, 0.0f);

    scenario.localUp = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    scenario.capsuleOrientation = rotation;
    // Standing at local z=1 — cleanly past the upper floor's own edge
    // (z<=0, with enough clearance that the capsule's own radius doesn't
    // still graze it) and squarely over the lower floor's footprint, at
    // the UPPER floor's height — exactly the "just walked off the edge,
    // about to fall" moment TryStepDown exists to catch.
    scenario.startPosition = rotation * glm::vec3(0.0f, kGroundOffset, 1.0f);
    return scenario;
}

const glm::quat kArbitraryRotation =
    glm::normalize(glm::angleAxis(glm::radians(53.0f), glm::normalize(glm::vec3(0.4f, 0.8f, -0.3f))));

void TestStepUpSucceedsWithinMaxHeight() {
    std::printf("TryStepMove: a riser at exactly the max step height succeeds\n");
    Scenario scenario = BuildStepScenario(kMaxStepHeight - 0.05f, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    glm::vec3 newPosition;
    const bool stepped =
        TryStepMove(*scenario.physics, scenario.startPosition, scenario.capsuleOrientation,
                    scenario.localUp, scenario.horizontalDisplacement, kMaxStepHeight, kMinGroundDot,
                    kSkinMargin, newPosition);
    Check(stepped, "TryStepMove returns true");
    if (stepped) {
        CheckNear(newPosition.y, kGroundOffset + (kMaxStepHeight - 0.05f), 0.05f,
                  "landed at (roughly) the step's own top height");
        Check(newPosition.z > scenario.startPosition.z + 1.0f,
              "made real forward progress past where a flat sweep would have stopped");
    }
}

void TestStepUpFailsAboveMaxHeight() {
    std::printf("TryStepMove: a riser taller than the max step height fails (stays a wall)\n");
    Scenario scenario = BuildStepScenario(kMaxStepHeight + 0.3f, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    glm::vec3 newPosition(0.0f);
    const bool stepped =
        TryStepMove(*scenario.physics, scenario.startPosition, scenario.capsuleOrientation,
                    scenario.localUp, scenario.horizontalDisplacement, kMaxStepHeight, kMinGroundDot,
                    kSkinMargin, newPosition);
    Check(!stepped, "TryStepMove returns false — too tall to be a step");
}

void TestStepUpFailsWithNoObstruction() {
    std::printf("TryStepMove: an unobstructed flat sweep needs no stepping\n");
    Scenario scenario = BuildStepScenario(kMaxStepHeight - 0.05f, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    glm::vec3 newPosition(0.0f);
    // A short displacement that never reaches the step at all.
    const bool stepped =
        TryStepMove(*scenario.physics, scenario.startPosition, scenario.capsuleOrientation,
                    scenario.localUp, scenario.horizontalDisplacement * 0.2f, kMaxStepHeight,
                    kMinGroundDot, kSkinMargin, newPosition);
    Check(!stepped, "TryStepMove returns false — nothing was in the way");
}

void TestStepUpRotateTheUniverse() {
    std::printf("TryStepMove: rotating the entire scenario produces the identically-rotated result\n");
    const float stepHeight = kMaxStepHeight - 0.05f;

    Scenario reference = BuildStepScenario(stepHeight, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    glm::vec3 referencePosition(0.0f);
    const bool referenceStepped =
        TryStepMove(*reference.physics, reference.startPosition, reference.capsuleOrientation,
                    reference.localUp, reference.horizontalDisplacement, kMaxStepHeight,
                    kMinGroundDot, kSkinMargin, referencePosition);
    Check(referenceStepped, "reference (unrotated) scenario steps up successfully");

    Scenario rotated = BuildStepScenario(stepHeight, kArbitraryRotation);
    glm::vec3 rotatedPosition(0.0f);
    const bool rotatedStepped =
        TryStepMove(*rotated.physics, rotated.startPosition, rotated.capsuleOrientation,
                    rotated.localUp, rotated.horizontalDisplacement, kMaxStepHeight, kMinGroundDot,
                    kSkinMargin, rotatedPosition);
    Check(rotatedStepped, "rotated scenario ALSO steps up successfully");

    if (referenceStepped && rotatedStepped) {
        const glm::vec3 rotatedBack = glm::inverse(kArbitraryRotation) * rotatedPosition;
        CheckNear(glm::length(rotatedBack - referencePosition), 0.0f, 0.01f,
                  "rotated-back result matches the unrotated reference result");
    }
}

void TestStepDownWithinMaxHeight() {
    std::printf("TryStepDown: a drop within the max step height is caught\n");
    Scenario scenario = BuildLedgeScenario(kMaxStepHeight - 0.1f, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    glm::vec3 newPosition(0.0f);
    glm::vec3 normal(0.0f);
    BodyHandle hitBody;
    const bool stepped =
        TryStepDown(*scenario.physics, scenario.startPosition, scenario.capsuleOrientation,
                    scenario.localUp, kMaxStepHeight, kMinGroundDot, kSkinMargin, newPosition, normal,
                    hitBody);
    Check(stepped, "TryStepDown returns true");
    Check(hitBody.IsValid(), "identifies the body landed on");
    if (stepped) {
        CheckNear(newPosition.y, kGroundOffset - (kMaxStepHeight - 0.1f), 0.05f,
                   "landed at (roughly) the lower floor's own height");
    }
}

void TestStepDownFailsBeyondMaxHeight() {
    std::printf("TryStepDown: a drop taller than the max step height is left alone (free-fall)\n");
    Scenario scenario = BuildLedgeScenario(kMaxStepHeight + 1.0f, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    glm::vec3 newPosition(0.0f);
    glm::vec3 normal(0.0f);
    BodyHandle hitBody;
    const bool stepped =
        TryStepDown(*scenario.physics, scenario.startPosition, scenario.capsuleOrientation,
                    scenario.localUp, kMaxStepHeight, kMinGroundDot, kSkinMargin, newPosition, normal,
                    hitBody);
    Check(!stepped, "TryStepDown returns false — too far to catch, must free-fall");
}

void TestStepDownRotateTheUniverse() {
    std::printf("TryStepDown: rotating the entire scenario produces the identically-rotated result\n");
    const float dropHeight = kMaxStepHeight - 0.1f;

    Scenario reference = BuildLedgeScenario(dropHeight, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    glm::vec3 referencePosition(0.0f), referenceNormal(0.0f);
    BodyHandle referenceBody;
    const bool referenceStepped =
        TryStepDown(*reference.physics, reference.startPosition, reference.capsuleOrientation,
                    reference.localUp, kMaxStepHeight, kMinGroundDot, kSkinMargin, referencePosition,
                    referenceNormal, referenceBody);
    Check(referenceStepped, "reference (unrotated) scenario catches the step-down");

    Scenario rotated = BuildLedgeScenario(dropHeight, kArbitraryRotation);
    glm::vec3 rotatedPosition(0.0f), rotatedNormal(0.0f);
    BodyHandle rotatedBody;
    const bool rotatedStepped =
        TryStepDown(*rotated.physics, rotated.startPosition, rotated.capsuleOrientation,
                    rotated.localUp, kMaxStepHeight, kMinGroundDot, kSkinMargin, rotatedPosition,
                    rotatedNormal, rotatedBody);
    Check(rotatedStepped, "rotated scenario ALSO catches the step-down");

    if (referenceStepped && rotatedStepped) {
        const glm::vec3 rotatedBack = glm::inverse(kArbitraryRotation) * rotatedPosition;
        CheckNear(glm::length(rotatedBack - referencePosition), 0.0f, 0.01f,
                  "rotated-back result matches the unrotated reference result");
    }
}

}  // namespace

int main() {
    std::printf("=== Judas Step-Climb Tests ===\n\n");
    TestStepUpSucceedsWithinMaxHeight();
    TestStepUpFailsAboveMaxHeight();
    TestStepUpFailsWithNoObstruction();
    TestStepUpRotateTheUniverse();
    TestStepDownWithinMaxHeight();
    TestStepDownFailsBeyondMaxHeight();
    TestStepDownRotateTheUniverse();

    std::printf("\n");
    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d CHECK(S) FAILED\n", g_failures);
    return 1;
}
