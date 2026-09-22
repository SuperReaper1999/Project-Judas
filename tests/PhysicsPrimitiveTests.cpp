// Deterministic, headless unit tests for the Judas-owned physics
// primitives (RigidBody state + integration + gravity application) — no
// window, no GL, no collision yet. Exercises exactly Milestone 7-Final's
// required validation sections A (uniform gravity in arbitrary
// orientations) and the integration-layer portion of D (rotate-the-whole-
// scenario invariance): the collision/contact portions of D are re-tested
// once collision exists (see docs/ARCHITECTURE.md's physics-migration
// notes for the running checklist). A separate, tiny test runner rather
// than routing through TestHarness/Application/Window — those exist to
// test GAMEPLAY behavior end-to-end; these test the physics primitives in
// isolation, which needs no game loop at all.
//
// No external test framework: plain pass/fail checks with a printed
// summary and a nonzero exit code on any failure, matching this project's
// existing preference (the gameplay test harness has no assertions of its
// own either — see docs/ARCHITECTURE.md, "Automated testing").

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "RadicalGravity.h"
#include "RigidBody.h"
#include "RigidBodyGravity.h"

namespace {

int g_failureCount = 0;

// Shortest-arc rotation taking unit vector `from` to unit vector `to` —
// the same self-contained helper shape used in PlayerController.cpp and
// GravityContextMap.cpp (no GLM_GTX_quaternion dependency needed for one
// small, well-understood piece of math).
glm::quat RotationBetweenUnitVectors(const glm::vec3& from, const glm::vec3& to) {
    const float d = std::clamp(glm::dot(from, to), -1.0f, 1.0f);
    if (d > 0.9999f) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (d < -0.9999f) {
        const glm::vec3 axis = std::abs(from.x) < 0.9f
                                    ? glm::cross(from, glm::vec3(1.0f, 0.0f, 0.0f))
                                    : glm::cross(from, glm::vec3(0.0f, 1.0f, 0.0f));
        return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
    }
    const glm::vec3 axis = glm::normalize(glm::cross(from, to));
    return glm::angleAxis(std::acos(d), axis);
}

void Check(bool condition, const std::string& description) {
    if (!condition) {
        std::printf("  FAIL: %s\n", description.c_str());
        ++g_failureCount;
    }
}

bool NearlyEqual(const glm::vec3& a, const glm::vec3& b, float tolerance) {
    return glm::length(a - b) <= tolerance;
}

bool NearlyEqual(float a, float b, float tolerance) { return std::abs(a - b) <= tolerance; }

// Runs `steps` fixed steps of "apply uniform gravity, integrate" on a
// single dynamic body starting at rest at the origin, and returns its
// final position/velocity.
struct FreeFallResult {
    glm::vec3 position;
    glm::vec3 velocity;
};

FreeFallResult SimulateFreeFall(const glm::vec3& gravityDirection, float magnitude, int steps,
                                 float fixedDeltaTime) {
    RigidBody body;
    body.inverseMass = 1.0f;  // ordinary dynamic body, mass = 1

    // Build a uniform field pointing exactly `gravityDirection` (unit) at
    // `magnitude` by using RadicalGravity's own math indirectly is
    // overkill for a uniform field — construct the acceleration directly
    // and apply it as a force each step, exactly mirroring what
    // ApplyGravity does with any GravityField::Sample result.
    const glm::vec3 acceleration = glm::normalize(gravityDirection) * magnitude;
    for (int i = 0; i < steps; ++i) {
        body.ApplyForce(acceleration / body.inverseMass);
        IntegrateRigidBody(body, fixedDeltaTime);
    }
    return FreeFallResult{body.position, body.linearVelocity};
}

// --- Section A / D (integration layer): uniform gravity in arbitrary
// orientations must be physically equivalent under rotation. For each
// candidate direction, build the rotation R that maps -Y onto it, run an
// identical free-fall scenario, then rotate the result BACK by R^-1 and
// compare against the reference (-Y) scenario's own result. Equal within
// tight tolerance proves nothing in the integration path secretly favors
// +Y/-Y or any other fixed axis.
void TestUniformGravityArbitraryOrientations() {
    std::printf("Section A/D (integration): uniform gravity in arbitrary orientations\n");

    constexpr int kSteps = 120;  // 2 simulated seconds at 60Hz
    constexpr float kDt = 1.0f / 60.0f;
    constexpr float kMagnitude = 9.81f;
    constexpr float kTolerance = 1.0e-4f;

    const FreeFallResult reference = SimulateFreeFall(glm::vec3(0.0f, -1.0f, 0.0f), kMagnitude,
                                                        kSteps, kDt);

    struct Candidate {
        const char* name;
        glm::vec3 direction;
    };
    const std::vector<Candidate> candidates = {
        {"+X down", glm::vec3(1.0f, 0.0f, 0.0f)},
        {"-X down", glm::vec3(-1.0f, 0.0f, 0.0f)},
        {"+Y down", glm::vec3(0.0f, 1.0f, 0.0f)},
        {"-Y down (reference)", glm::vec3(0.0f, -1.0f, 0.0f)},
        {"+Z down", glm::vec3(0.0f, 0.0f, 1.0f)},
        {"-Z down", glm::vec3(0.0f, 0.0f, -1.0f)},
        {"arbitrary normalized", glm::normalize(glm::vec3(0.3f, -0.5f, 0.81f))},
    };

    for (const Candidate& candidate : candidates) {
        const glm::quat rotation =
            RotationBetweenUnitVectors(glm::vec3(0.0f, -1.0f, 0.0f), candidate.direction);
        const FreeFallResult result =
            SimulateFreeFall(candidate.direction, kMagnitude, kSteps, kDt);

        const glm::quat inverseRotation = glm::inverse(rotation);
        const glm::vec3 unrotatedPosition = inverseRotation * result.position;
        const glm::vec3 unrotatedVelocity = inverseRotation * result.velocity;

        Check(NearlyEqual(unrotatedPosition, reference.position, kTolerance),
              std::string(candidate.name) + ": position does not match reference under inverse rotation");
        Check(NearlyEqual(unrotatedVelocity, reference.velocity, kTolerance),
              std::string(candidate.name) + ": velocity does not match reference under inverse rotation");
    }
    std::printf("  (7 directions checked against a common rotated reference)\n");
}

// --- Section D (full scenario rotation, integration layer): take a
// two-body scenario (different masses, different starting offsets) under
// a fixed gravity direction, rotate EVERYTHING (gravity direction, both
// starting positions) by an arbitrary rotation, re-run, then rotate the
// result back and compare. This is the same principle as the section
// above but with more than one body and non-origin starting positions —
// specifically to catch a bug that a single body starting at the origin
// might hide (e.g. a hidden translation-frame assumption).
void TestFullScenarioRotationInvariance() {
    std::printf("Section D: full scenario rigid rotation invariance (multi-body)\n");

    constexpr int kSteps = 90;
    constexpr float kDt = 1.0f / 60.0f;
    constexpr float kTolerance = 1.0e-4f;

    const glm::vec3 gravityDirection(0.0f, -1.0f, 0.0f);
    const glm::vec3 startA(2.0f, 5.0f, -1.0f);
    const glm::vec3 startB(-3.0f, 8.0f, 4.0f);
    const float magnitude = 9.81f;

    auto simulateTwoBody = [&](const glm::quat& sceneRotation) {
        RigidBody a;
        a.inverseMass = 1.0f;
        a.position = sceneRotation * startA;
        RigidBody b;
        b.inverseMass = 0.5f;  // different mass — gravity must still accelerate it identically
        b.position = sceneRotation * startB;

        const glm::vec3 acceleration = (sceneRotation * gravityDirection) * magnitude;
        for (int i = 0; i < kSteps; ++i) {
            a.ApplyForce(acceleration / a.inverseMass);
            b.ApplyForce(acceleration / b.inverseMass);
            IntegrateRigidBody(a, kDt);
            IntegrateRigidBody(b, kDt);
        }
        return std::make_pair(a.position, b.position);
    };

    const auto [referenceA, referenceB] = simulateTwoBody(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    const glm::quat sceneRotation =
        glm::angleAxis(glm::radians(53.0f), glm::normalize(glm::vec3(0.4f, 0.7f, -0.2f)));
    const auto [rotatedA, rotatedB] = simulateTwoBody(sceneRotation);
    const glm::quat inverseSceneRotation = glm::inverse(sceneRotation);

    Check(NearlyEqual(inverseSceneRotation * rotatedA, referenceA, kTolerance),
          "body A position mismatch after rotating the whole scenario and rotating back");
    Check(NearlyEqual(inverseSceneRotation * rotatedB, referenceB, kTolerance),
          "body B position mismatch after rotating the whole scenario and rotating back");
}

// --- Gravity-independent sanity: a static body (inverseMass == 0) must
// never move under any applied force/gravity, in any direction. Zero
// global-axis relevance, but a critical invariant the contact solver will
// depend on later (a planet's own sphere body is static).
void TestStaticBodiesNeverMove() {
    std::printf("Static-body invariant: infinite mass never accelerates\n");
    constexpr float kTolerance = 1.0e-6f;

    const std::vector<glm::vec3> directions = {
        glm::vec3(1, 0, 0),  glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),
        glm::vec3(0, -1, 0), glm::vec3(0, 0, 1),  glm::vec3(0, 0, -1),
        glm::normalize(glm::vec3(1, 1, 1)),
    };
    for (const glm::vec3& direction : directions) {
        RigidBody body;  // inverseMass defaults to 0 -> static
        body.position = glm::vec3(5.0f, -3.0f, 12.0f);
        const glm::vec3 startPosition = body.position;
        body.ApplyForce(direction * 1000.0f);
        for (int i = 0; i < 60; ++i) {
            IntegrateRigidBody(body, 1.0f / 60.0f);
        }
        Check(NearlyEqual(body.position, startPosition, kTolerance),
              "static body moved under an applied force");
        Check(NearlyEqual(body.linearVelocity, glm::vec3(0.0f), kTolerance),
              "static body gained velocity under an applied force");
    }
}

// --- ApplyGravity + a real GravityField (RadicalGravity): a body placed
// symmetrically around a RadicalGravity center in six different starting
// directions must fall toward that center identically (by symmetry), and
// a body dropped from directly "north," "east," or any other direction
// around a sphere must behave the same as any other, rotated — the same
// invariance as the uniform-gravity tests above, but exercising the actual
// GravityField contract and ApplyGravity's seam rather than a hand-built
// acceleration vector.
void TestRadialGravityNoPrivilegedAxis() {
    std::printf("Section B: radial gravity, no privileged axis\n");
    constexpr float kTolerance = 1.0e-4f;
    constexpr int kSteps = 90;
    constexpr float kDt = 1.0f / 60.0f;

    const glm::vec3 center(0.0f, 0.0f, 0.0f);
    RadicalGravity gravity(center, 9.81f);

    const std::vector<glm::vec3> startDirections = {
        glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),
        glm::vec3(0, -1, 0), glm::vec3(0, 0, 1), glm::vec3(0, 0, -1),
    };
    const float startDistance = 10.0f;

    // Every body starts the same distance from center along a different
    // axis; by the sphere's own symmetry, each should close the same
    // amount of distance toward the center after the same number of
    // steps — no direction should fall "faster" or "slower."
    std::vector<float> finalDistances;
    for (const glm::vec3& direction : startDirections) {
        RigidBody body;
        body.inverseMass = 1.0f;
        body.position = center + direction * startDistance;
        for (int i = 0; i < kSteps; ++i) {
            ApplyGravity(body, gravity);
            IntegrateRigidBody(body, kDt);
        }
        finalDistances.push_back(glm::length(body.position - center));
    }
    for (std::size_t i = 1; i < finalDistances.size(); ++i) {
        Check(NearlyEqual(finalDistances[i], finalDistances[0], kTolerance),
              "radial free-fall distance differs by starting direction (privileged axis present)");
    }
}

}  // namespace

int main() {
    std::printf("=== Judas Physics Primitive Tests ===\n\n");

    TestUniformGravityArbitraryOrientations();
    TestFullScenarioRotationInvariance();
    TestStaticBodiesNeverMove();
    TestRadialGravityNoPrivilegedAxis();

    std::printf("\n");
    if (g_failureCount == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d CHECK(S) FAILED\n", g_failureCount);
    return 1;
}
