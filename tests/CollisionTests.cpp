// Deterministic, headless tests for collision/contact resolution (Section
// C) and for the redesigned GravityContextMap (Sections E/F/G) — see
// tests/PhysicsPrimitiveTests.cpp for the lower integration-layer tests
// (Sections A/B/D) this builds on. Same plain pass/fail convention, no
// external test framework.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "BoxVolume.h"
#include "ContactSolver.h"
#include "Contacts.h"
#include "FaithfulGravity.h"
#include "GravityContextMap.h"
#include "RadicalGravity.h"
#include "RigidBody.h"
#include "RigidBodyGravity.h"
#include "SphericalVolume.h"

namespace {

int g_failureCount = 0;

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

bool IsFinite(const glm::vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

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

RigidBody MakeStaticBox(const glm::vec3& position, const glm::quat& orientation) {
    RigidBody body;
    body.position = position;
    body.orientation = orientation;
    body.inverseMass = 0.0f;
    return body;
}

RigidBody MakeDynamicBox(const glm::vec3& position, const glm::vec3& halfExtents, float mass) {
    RigidBody body;
    body.position = position;
    body.inverseMass = 1.0f / mass;
    body.inverseInertiaLocal = SolidBoxInverseInertia(mass, halfExtents);
    return body;
}

RigidBody MakeDynamicSphere(const glm::vec3& position, float radius, float mass) {
    RigidBody body;
    body.position = position;
    body.inverseMass = 1.0f / mass;
    body.inverseInertiaLocal = SolidSphereInverseInertia(mass, radius);
    return body;
}

// One fixed step of "apply gravity, integrate, resolve contact(s)" for a
// single dynamic box resting/sliding on a single static box — the minimum
// loop needed to test resting/friction/restitution without pulling in all
// of PhysicsWorld.
void StepBoxOnGround(RigidBody& box, const glm::vec3& boxHalfExtents, RigidBody& ground,
                      const glm::vec3& groundHalfExtents, const glm::vec3& gravity, float friction,
                      float restitution, float dt) {
    box.ApplyForce(gravity / box.inverseMass);
    IntegrateRigidBody(box, dt);
    for (int i = 0; i < 4; ++i) {
        const ContactManifold manifold =
            BoxVsBoxManifold(box.position, box.orientation, boxHalfExtents, ground.position,
                              ground.orientation, groundHalfExtents);
        for (int p = 0; p < manifold.count; ++p) {
            ResolveContact(box, ground, manifold.points[p], friction, restitution);
        }
    }
}

// --- Section C: resting box on an arbitrarily-oriented plane ---
// A box on a plane rotated by `sceneRotation`, under gravity ALSO rotated
// by the same amount (so "down" still points into the plane, just not
// along any fixed world axis), must settle to the same resting gap above
// the surface as the unrotated (gravity = -Y, plane axis-aligned) case.
void TestRestingBoxArbitraryOrientation() {
    std::printf("Section C: resting box on an arbitrary-oriented plane\n");
    constexpr float kDt = 1.0f / 60.0f;
    constexpr int kSteps = 240;  // 4 simulated seconds -- ample settling time
    const glm::vec3 boxHalfExtents(0.5f);
    const glm::vec3 groundHalfExtents(10.0f, 0.5f, 10.0f);

    auto simulate = [&](const glm::quat& sceneRotation) {
        RigidBody ground = MakeStaticBox(glm::vec3(0.0f), sceneRotation);
        RigidBody box =
            MakeDynamicBox(sceneRotation * glm::vec3(0.0f, groundHalfExtents.y + boxHalfExtents.y + 1.0f, 0.0f),
                            boxHalfExtents, 2.0f);
        box.orientation = sceneRotation;
        const glm::vec3 gravity = sceneRotation * glm::vec3(0.0f, -9.81f, 0.0f);
        for (int i = 0; i < kSteps; ++i) {
            StepBoxOnGround(box, boxHalfExtents, ground, groundHalfExtents, gravity, 0.6f, 0.0f, kDt);
        }
        // Distance from the ground's own top surface along its own local +Y.
        const glm::vec3 localBoxPos = glm::conjugate(sceneRotation) * (box.position - ground.position);
        return localBoxPos.y - groundHalfExtents.y;
    };

    const float restingGapDefault = simulate(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    const glm::quat arbitraryRotation =
        glm::angleAxis(glm::radians(37.0f), glm::normalize(glm::vec3(0.5f, 1.0f, -0.3f)));
    const float restingGapRotated = simulate(arbitraryRotation);

    Check(NearlyEqual(restingGapDefault, boxHalfExtents.y, 0.02f),
          "box did not settle to its own half-height above an axis-aligned plane");
    Check(NearlyEqual(restingGapRotated, boxHalfExtents.y, 0.02f),
          "box did not settle to its own half-height above an arbitrarily-oriented plane");
    Check(NearlyEqual(restingGapDefault, restingGapRotated, 0.02f),
          "resting gap differs between axis-aligned and arbitrarily-oriented planes");
}

// --- Section C: friction on an arbitrarily-oriented plane ---
// A box given an initial tangential (along-surface) velocity while resting
// under high friction must decelerate the same amount whether the surface
// is axis-aligned or arbitrarily rotated (compared via the rotate-and-undo
// technique used throughout these suites).
void TestFrictionArbitraryOrientation() {
    std::printf("Section C: friction decelerates identically under rotation\n");
    constexpr float kDt = 1.0f / 60.0f;
    constexpr int kSettleSteps = 60;
    constexpr int kSlideSteps = 30;
    const glm::vec3 boxHalfExtents(0.5f);
    const glm::vec3 groundHalfExtents(10.0f, 0.5f, 10.0f);

    auto simulateFinalTangentialSpeed = [&](const glm::quat& sceneRotation) {
        RigidBody ground = MakeStaticBox(glm::vec3(0.0f), sceneRotation);
        RigidBody box = MakeDynamicBox(
            sceneRotation * glm::vec3(0.0f, groundHalfExtents.y + boxHalfExtents.y + 0.5f, 0.0f),
            boxHalfExtents, 2.0f);
        box.orientation = sceneRotation;
        const glm::vec3 gravity = sceneRotation * glm::vec3(0.0f, -9.81f, 0.0f);
        for (int i = 0; i < kSettleSteps; ++i) {
            StepBoxOnGround(box, boxHalfExtents, ground, groundHalfExtents, gravity, 0.9f, 0.0f, kDt);
        }
        box.linearVelocity += sceneRotation * glm::vec3(3.0f, 0.0f, 0.0f);
        for (int i = 0; i < kSlideSteps; ++i) {
            StepBoxOnGround(box, boxHalfExtents, ground, groundHalfExtents, gravity, 0.9f, 0.0f, kDt);
        }
        return glm::length(glm::conjugate(sceneRotation) * box.linearVelocity);
    };

    const float defaultSpeed = simulateFinalTangentialSpeed(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    const glm::quat arbitraryRotation =
        glm::angleAxis(glm::radians(62.0f), glm::normalize(glm::vec3(-0.2f, 0.6f, 0.9f)));
    const float rotatedSpeed = simulateFinalTangentialSpeed(arbitraryRotation);

    Check(defaultSpeed < 2.9f, "friction did not decelerate the box at all (axis-aligned case)");
    Check(NearlyEqual(defaultSpeed, rotatedSpeed, 0.05f),
          "friction deceleration differs between axis-aligned and arbitrarily-oriented planes");
}

// --- Section C: restitution/bounce independent of orientation ---
void TestRestitutionArbitraryOrientation() {
    std::printf("Section C: bounce restitution independent of orientation\n");
    constexpr float kDt = 1.0f / 60.0f;
    constexpr int kSteps = 90;
    const float sphereRadius = 0.5f;
    const glm::vec3 groundHalfExtents(10.0f, 0.5f, 10.0f);

    auto simulateBounceSpeed = [&](const glm::quat& sceneRotation) {
        RigidBody ground = MakeStaticBox(glm::vec3(0.0f), sceneRotation);
        RigidBody sphere = MakeDynamicSphere(
            sceneRotation * glm::vec3(0.0f, groundHalfExtents.y + sphereRadius + 3.0f, 0.0f),
            sphereRadius, 1.0f);
        const glm::vec3 gravity = sceneRotation * glm::vec3(0.0f, -9.81f, 0.0f);
        float maxUpwardSpeedAfterBounce = 0.0f;
        bool hasBounced = false;
        for (int i = 0; i < kSteps; ++i) {
            sphere.ApplyForce(gravity / sphere.inverseMass);
            IntegrateRigidBody(sphere, kDt);
            const Contact contact = SphereVsBox(sphere.position, sphereRadius, ground.position,
                                                 ground.orientation, groundHalfExtents);
            if (contact.hit) {
                ResolveContact(sphere, ground, contact, 0.1f, 0.6f);
            }
            const float localUpwardSpeed =
                glm::dot(glm::conjugate(sceneRotation) * sphere.linearVelocity, glm::vec3(0, 1, 0));
            if (hasBounced && localUpwardSpeed > maxUpwardSpeedAfterBounce) {
                maxUpwardSpeedAfterBounce = localUpwardSpeed;
            }
            if (contact.hit) hasBounced = true;
        }
        return maxUpwardSpeedAfterBounce;
    };

    const float defaultBounceSpeed = simulateBounceSpeed(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    const glm::quat arbitraryRotation =
        glm::angleAxis(glm::radians(84.0f), glm::normalize(glm::vec3(0.3f, -0.4f, 0.8f)));
    const float rotatedBounceSpeed = simulateBounceSpeed(arbitraryRotation);

    Check(defaultBounceSpeed > 1.0f, "sphere did not bounce upward at all (axis-aligned case)");
    Check(NearlyEqual(defaultBounceSpeed, rotatedBounceSpeed, 0.3f),
          "bounce speed differs between axis-aligned and arbitrarily-oriented planes");
}

// --- Section C: stacks under differently-oriented gravity ---
// Two boxes stacked, gravity pointing in an arbitrary (non -Y) direction:
// must settle to a bounded, finite, non-exploding configuration.
void TestStackArbitraryGravity() {
    std::printf("Section C: two-box stack under arbitrary gravity direction\n");
    constexpr float kDt = 1.0f / 60.0f;
    constexpr int kSteps = 300;
    const glm::vec3 boxHalfExtents(0.5f);
    const glm::vec3 groundHalfExtents(10.0f, 0.5f, 10.0f);
    const glm::vec3 gravityDirection = glm::normalize(glm::vec3(0.2f, -1.0f, 0.15f));
    const glm::quat groundOrientation = RotationBetweenUnitVectors(
        glm::vec3(0.0f, 1.0f, 0.0f), -gravityDirection);

    RigidBody ground = MakeStaticBox(glm::vec3(0.0f), groundOrientation);
    RigidBody lower = MakeDynamicBox(-gravityDirection * (groundHalfExtents.y + boxHalfExtents.y + 0.2f),
                                      boxHalfExtents, 2.0f);
    RigidBody upper = MakeDynamicBox(
        -gravityDirection * (groundHalfExtents.y + boxHalfExtents.y * 3.0f + 0.5f), boxHalfExtents,
        2.0f);
    lower.orientation = groundOrientation;
    upper.orientation = groundOrientation;
    const glm::vec3 gravity = gravityDirection * 9.81f;

    for (int i = 0; i < kSteps; ++i) {
        lower.ApplyForce(gravity / lower.inverseMass);
        upper.ApplyForce(gravity / upper.inverseMass);
        IntegrateRigidBody(lower, kDt);
        IntegrateRigidBody(upper, kDt);
        for (int iter = 0; iter < 4; ++iter) {
            const ContactManifold groundLower =
                BoxVsBoxManifold(lower.position, lower.orientation, boxHalfExtents, ground.position,
                                  ground.orientation, groundHalfExtents);
            for (int p = 0; p < groundLower.count; ++p) {
                ResolveContact(lower, ground, groundLower.points[p], 0.6f, 0.0f);
            }
            const ContactManifold lowerUpper =
                BoxVsBoxManifold(upper.position, upper.orientation, boxHalfExtents, lower.position,
                                  lower.orientation, boxHalfExtents);
            for (int p = 0; p < lowerUpper.count; ++p) {
                ResolveContact(upper, lower, lowerUpper.points[p], 0.6f, 0.0f);
            }
        }
    }

    Check(IsFinite(lower.position) && IsFinite(upper.position), "stack produced non-finite positions");
    Check(IsFinite(lower.linearVelocity) && IsFinite(upper.linearVelocity),
          "stack produced non-finite velocities");
    Check(glm::length(lower.linearVelocity) < 1.0f && glm::length(upper.linearVelocity) < 1.0f,
          "stack did not settle (still moving fast after 5 simulated seconds)");
    const float separation = glm::length(upper.position - lower.position);
    Check(separation > boxHalfExtents.y * 1.5f && separation < boxHalfExtents.y * 3.0f,
          "stacked boxes are not at a plausible separation (fell through or flew apart)");
}

// --- Section E: multiple simultaneous gravity contexts, no contamination ---
void TestMultipleSimultaneousContexts() {
    std::printf("Section E: multiple simultaneous gravity contexts do not contaminate\n");
    const glm::vec3 planetACenter(0.0f, 0.0f, 0.0f);
    const glm::vec3 planetBCenter(0.0f, 0.0f, 55.0f);
    const glm::vec3 plankCenter(0.0f, 17.0f, 27.5f);
    RadicalGravity planetA(planetACenter, 9.81f);
    RadicalGravity planetB(planetBCenter, 9.81f);
    FaithfulGravity plankField;
    const SphericalVolume planetARegion(planetACenter, 26.0f);
    const SphericalVolume planetBRegion(planetBCenter, 26.0f);
    const BoxVolume plankRegion(plankCenter, glm::vec3(8.0f, 7.0f, 22.0f));

    GravityContextMap context;
    context.AddRegion(plankField, plankRegion);
    context.AddRegion(planetA, planetARegion);
    context.AddRegion(planetB, planetBRegion);

    RigidBody onA;
    onA.inverseMass = 1.0f;
    onA.position = glm::vec3(0.0f, 20.0f, 0.0f);
    RigidBody onPlank;
    onPlank.inverseMass = 1.0f;
    onPlank.position = plankCenter;
    RigidBody onB;
    onB.inverseMass = 1.0f;
    onB.position = glm::vec3(0.0f, 20.0f, 55.0f);

    constexpr float kDt = 1.0f / 60.0f;
    for (int i = 0; i < 30; ++i) {
        ApplyGravity(onA, context);
        ApplyGravity(onPlank, context);
        ApplyGravity(onB, context);
        IntegrateRigidBody(onA, kDt);
        IntegrateRigidBody(onPlank, kDt);
        IntegrateRigidBody(onB, kDt);
    }

    // Body on Planet A must move toward Planet A's center, never Planet B's.
    Check(onA.position.y < 20.0f, "body on Planet A did not fall toward its own center");
    Check(NearlyEqual(onA.position.x, 0.0f, 1.0e-3f) && NearlyEqual(onA.position.z, 0.0f, 1.0e-3f),
          "body on Planet A drifted sideways -- contamination from another context");

    // Body on the plank must fall straight down (FaithfulGravity), with
    // zero horizontal drift regardless of either planet's existence.
    Check(onPlank.position.y < plankCenter.y, "body on the plank did not fall under its own gravity");
    Check(NearlyEqual(onPlank.position.x, plankCenter.x, 1.0e-3f) &&
              NearlyEqual(onPlank.position.z, plankCenter.z, 1.0e-3f),
          "body on the plank drifted sideways -- contamination from a planet's radial field");

    // Body on Planet B must move toward Planet B's center.
    Check(onB.position.y < 20.0f, "body on Planet B did not fall toward its own center");
    Check(NearlyEqual(onB.position.x, 0.0f, 1.0e-3f) &&
              NearlyEqual(onB.position.z, 55.0f, 1.0e-3f),
          "body on Planet B drifted sideways -- contamination from another context");
}

// --- Section F: zero gravity in genuinely unclaimed space ---
void TestZeroGravityUnclaimedSpace() {
    std::printf("Section F: zero gravity in unclaimed space (no invented +Y/-Y fallback)\n");
    RadicalGravity planetA(glm::vec3(0.0f), 9.81f);
    const SphericalVolume planetARegion(glm::vec3(0.0f), 26.0f);
    GravityContextMap context;
    context.AddRegion(planetA, planetARegion);

    const glm::vec3 farAway(500.0f, -300.0f, 900.0f);
    const glm::vec3 sample = context.Sample(farAway);
    Check(NearlyEqual(sample, glm::vec3(0.0f), 1.0e-6f),
          "unclaimed space did not return exactly zero gravity");

    RigidBody body;
    body.inverseMass = 1.0f;
    body.position = farAway;
    body.linearVelocity = glm::vec3(1.0f, 2.0f, -3.0f);  // pre-existing drift
    const glm::vec3 startVelocity = body.linearVelocity;
    const glm::vec3 startPosition = body.position;
    for (int i = 0; i < 60; ++i) {
        ApplyGravity(body, context);
        IntegrateRigidBody(body, 1.0f / 60.0f);
    }
    Check(NearlyEqual(body.linearVelocity, startVelocity, 1.0e-4f),
          "body's velocity changed in zero gravity (invented acceleration)");
    Check(NearlyEqual(body.position, startPosition + startVelocity, 0.02f),
          "body did not simply coast at constant velocity through zero gravity");
}

// --- Section G: context transitions, independent of PlayerController ---
// Sweep a straight line from deep inside Planet A's region, through the
// plank's, into Planet B's, and verify every sample matches EXACTLY one
// field's own raw output -- never a blend -- with magnitude always exactly
// 9.81 (no cliffs) throughout, and the plank's own stretch always exactly
// (0,-9.81,0).
void TestContextTransitionsNoBlending() {
    std::printf("Section G: context transitions never blend two fields\n");
    const glm::vec3 planetACenter(0.0f, 0.0f, 0.0f);
    const glm::vec3 planetBCenter(0.0f, 0.0f, 55.0f);
    const glm::vec3 plankCenter(0.0f, 17.0f, 27.5f);
    RadicalGravity planetA(planetACenter, 9.81f);
    RadicalGravity planetB(planetBCenter, 9.81f);
    FaithfulGravity plankField;
    const SphericalVolume planetARegion(planetACenter, 26.0f);
    const SphericalVolume planetBRegion(planetBCenter, 26.0f);
    const BoxVolume plankRegion(plankCenter, glm::vec3(8.0f, 7.0f, 22.0f));

    GravityContextMap context;
    context.AddRegion(plankField, plankRegion);
    context.AddRegion(planetA, planetARegion);
    context.AddRegion(planetB, planetBRegion);

    bool anyBlendedSampleFound = false;
    bool anyMagnitudeCliff = false;
    float previousMagnitude = -1.0f;
    for (float z = -10.0f; z <= 65.0f; z += 0.25f) {
        const glm::vec3 point(0.0f, 18.0f, z);
        const glm::vec3 sample = context.Sample(point);
        const float magnitude = glm::length(sample);

        if (magnitude > 1.0e-6f) {
            const bool matchesA = NearlyEqual(sample, planetA.Sample(point), 1.0e-4f);
            const bool matchesB = NearlyEqual(sample, planetB.Sample(point), 1.0e-4f);
            const bool matchesPlank = NearlyEqual(sample, plankField.Sample(point), 1.0e-4f);
            if (!matchesA && !matchesB && !matchesPlank) anyBlendedSampleFound = true;

            if (previousMagnitude >= 0.0f && std::abs(magnitude - previousMagnitude) > 1.0f) {
                anyMagnitudeCliff = true;
            }
        }
        previousMagnitude = magnitude;
    }

    Check(!anyBlendedSampleFound,
          "found a sample matching none of the three raw fields exactly -- blending occurred");
    Check(!anyMagnitudeCliff, "found a sudden magnitude jump crossing a region boundary");

    // The plank's own stretch (its region's z-span) must be EXACTLY
    // FaithfulGravity, everywhere, including right at its edges.
    for (float z = 6.0f; z <= 49.0f; z += 1.0f) {
        const glm::vec3 sample = context.Sample(glm::vec3(0.0f, 18.0f, z));
        Check(NearlyEqual(sample, glm::vec3(0.0f, -9.81f, 0.0f), 1.0e-4f),
              "plank region did not return exactly FaithfulGravity's own value");
    }
}

}  // namespace

int main() {
    std::printf("=== Judas Collision / Contact / Gravity-Context Tests ===\n\n");

    TestRestingBoxArbitraryOrientation();
    TestFrictionArbitraryOrientation();
    TestRestitutionArbitraryOrientation();
    TestStackArbitraryGravity();
    TestMultipleSimultaneousContexts();
    TestZeroGravityUnclaimedSpace();
    TestContextTransitionsNoBlending();

    std::printf("\n");
    if (g_failureCount == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d CHECK(S) FAILED\n", g_failureCount);
    return 1;
}
