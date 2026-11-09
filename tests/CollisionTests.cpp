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
