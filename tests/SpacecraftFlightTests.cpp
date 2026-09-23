#include "CelestialGravity.h"
#include "FlyingPrimitiveControl.h"
#include "PhysicsWorld.h"
#include "Window.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

namespace {
constexpr float kMass = 1.0e14f;
constexpr float kShipMass = 80.0f;
constexpr float kRadius = 30.0f;
constexpr float kDt = 1.0f / 60.0f;
const glm::vec3 kHalfExtents(2.0f, 0.25f, 3.0f);
int gFailures = 0;

void Check(bool condition, const char* message) {
    if (condition) std::printf("  OK   %s\n", message);
    else {
        std::fprintf(stderr, "  FAIL %s\n", message);
        ++gFailures;
    }
}

void CheckNear(float actual, float expected, float tolerance, const char* message) {
    Check(std::abs(actual - expected) <= tolerance, message);
}

glm::quat RotationFromTo(const glm::vec3& from, const glm::vec3& to);

struct OrbitSetup {
    BodyHandle source;
    BodyHandle ship;
    CelestialGravity gravity;
    float period;
    OrbitSetup(PhysicsWorld& world, const glm::quat& frame)
        : source(), ship(), gravity({}), period(0.0f) {
        const glm::vec3 radial = glm::normalize(frame * glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::vec3 tangent = glm::normalize(frame * glm::vec3(0.0f, 1.0f, 0.0f));
        const float totalMass = kMass + kShipMass;
        const float relativeSpeed = std::sqrt(CelestialGravity::kGravitationalConstant * totalMass /
                                              kRadius);
        const glm::vec3 sourcePosition = -radial * (kRadius * kShipMass / totalMass);
        const glm::vec3 shipPosition = radial * (kRadius * kMass / totalMass);
        const glm::vec3 sourceVelocity = -tangent * (relativeSpeed * kShipMass / totalMass);
        const glm::vec3 shipVelocity = tangent * (relativeSpeed * kMass / totalMass);
        source = world.CreateDynamicSphere(sourcePosition, 3.0f, kMass, 0.0f, 0.0f);
        ship = world.CreateDynamicBox(shipPosition, kHalfExtents, kShipMass, 0.0f, 0.0f);
        world.SetLinearVelocity(source, sourceVelocity);
        world.SetLinearVelocity(ship, shipVelocity);
        const glm::quat forwardAlongTangent = RotationFromTo(glm::vec3(0.0f, 0.0f, -1.0f),
                                                              glm::vec3(0.0f, 1.0f, 0.0f));
        world.ResetBody(ship, shipPosition, frame * forwardAlongTangent);
        // The body's local forward (-Z) is aligned with the initial tangent.
        world.SetLinearVelocity(ship, shipVelocity);
        gravity = CelestialGravity({source, ship});
        const float omega = std::sqrt(CelestialGravity::kGravitationalConstant * totalMass /
                                      (kRadius * kRadius * kRadius));
        period = glm::two_pi<float>() / omega;
    }
};

glm::vec3 RelativePosition(const PhysicsWorld& world, const OrbitSetup& orbit) {
    return world.GetTransform(orbit.ship).position - world.GetTransform(orbit.source).position;
}

glm::vec3 RelativeVelocity(const PhysicsWorld& world, const OrbitSetup& orbit) {
    return world.GetLinearVelocity(orbit.ship) - world.GetLinearVelocity(orbit.source);
}
float SpecificOrbitalEnergy(const PhysicsWorld& world, const OrbitSetup& orbit) {
    const float distance = glm::length(RelativePosition(world, orbit));
    const float speed = glm::length(RelativeVelocity(world, orbit));
    return 0.5f * speed * speed - CelestialGravity::kGravitationalConstant *
           (kMass + kShipMass) / distance;
}

glm::quat RotationFromTo(const glm::vec3& from, const glm::vec3& to) {
    const glm::vec3 a = glm::normalize(from);
    const glm::vec3 b = glm::normalize(to);
    const glm::vec3 cross = glm::cross(a, b);
    return glm::normalize(glm::quat(1.0f + glm::dot(a, b), cross.x, cross.y, cross.z));
}

void Advance(PhysicsWorld& world, OrbitSetup& orbit, FlyingPrimitiveControl& control,
             const Window& window, int steps) {
    for (int i = 0; i < steps; ++i) {
        orbit.gravity.ApplyForces(world);
        ApplyFlyingPrimitiveControl(control, window, world);
        world.Step(kDt);
    }
}

struct OrbitResult {
    glm::vec3 position;
    glm::vec3 velocity;
    float minRadius = std::numeric_limits<float>::max();
    float maxRadius = 0.0f;
    float simulatedTime = 0.0f;
    float analyticalPeriod = 0.0f;
};
OrbitResult SimulateUnpoweredOrbit(const glm::quat& frame, bool sasEnabled = false) {
    PhysicsWorld world;
    Check(world.Init(), "orbit world initialized");
    OrbitSetup orbit(world, frame);
    FlyingPrimitiveControl control;
    control.handle = orbit.ship;
    if (sasEnabled) {
        SetSpacecraftSasEnabled(control, true, world);
        world.SetAngularVelocity(orbit.ship, glm::vec3(0.8f, -0.4f, 0.6f));
    }
    Window window;
    window.SetTestInputMode(true);
    OrbitResult result;
    const int steps = static_cast<int>(std::ceil(orbit.period / kDt));
    result.analyticalPeriod = orbit.period;
    for (int i = 0; i < steps; ++i) {
        orbit.gravity.ApplyForces(world);
        ApplyFlyingPrimitiveControl(control, window, world);
        world.Step(kDt);
        const float radius = glm::length(RelativePosition(world, orbit));
        result.minRadius = std::min(result.minRadius, radius);
        result.maxRadius = std::max(result.maxRadius, radius);
    }
    result.position = RelativePosition(world, orbit);
    result.velocity = RelativeVelocity(world, orbit);
    result.simulatedTime = steps * kDt;
    world.Shutdown();
    return result;
}

void TestCelestialSpacecraftOrbitAndRotation() {
    std::printf("Section A: spacecraft is a celestial test body and coasts in orbit\n");
    const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    const OrbitResult reference = SimulateUnpoweredOrbit(identity);
    Check(std::abs(glm::length(reference.position) - kRadius) < 0.3f,
          "unpowered spacecraft remains on a curved, bounded trajectory for one orbit");
    Check(reference.maxRadius - reference.minRadius < 0.3f,
          "spacecraft orbital radius stays bounded throughout the revolution");
    Check(glm::dot(reference.position, glm::vec3(1.0f, 0.0f, 0.0f)) > kRadius * 0.9f,
          "spacecraft completes almost a full revolution without an orbit mode");
    const OrbitResult stabilized = SimulateUnpoweredOrbit(identity, true);
    Check(glm::length(stabilized.position - reference.position) < 1.0e-4f &&
              glm::length(stabilized.velocity - reference.velocity) < 1.0e-4f,
          "SAS does not measurably alter the unpowered celestial trajectory");

    const glm::quat rotation = glm::angleAxis(0.91f, glm::normalize(glm::vec3(2.0f, -1.0f, 3.0f)));
    const OrbitResult rotated = SimulateUnpoweredOrbit(rotation);
    Check(glm::length(glm::inverse(rotation) * rotated.position - reference.position) < 0.04f,
          "spacecraft celestial trajectory rotates with the complete scenario");
    Check(glm::length(glm::inverse(rotation) * rotated.velocity - reference.velocity) < 0.02f,
          "spacecraft celestial velocity has no preferred world axis");
    std::printf("Measured spacecraft orbit: T=%.4f s (analytic %.4f s), radius %.4f..%.4f m\n",
                reference.simulatedTime, reference.analyticalPeriod,
                reference.minRadius, reference.maxRadius);

    PhysicsWorld world;
    Check(world.Init(), "gravity measurement world initialized");
    const BodyHandle source = world.CreateDynamicSphere(glm::vec3(0.0f), 3.0f, kMass, 0.0f, 0.0f);
    const glm::vec3 start(30.0f, 0.0f, 0.0f);
    const BodyHandle ship = world.CreateDynamicBox(start, kHalfExtents, kShipMass, 0.0f, 0.0f);
    CelestialGravity gravity({source, ship});
    const glm::vec3 initialVelocity = world.GetLinearVelocity(ship);
    gravity.ApplyForces(world);
    world.Step(kDt);
    const glm::vec3 measuredAcceleration = (world.GetLinearVelocity(ship) - initialVelocity) / kDt;
    const float expectedMagnitude = CelestialGravity::kGravitationalConstant * kMass / (30.0f * 30.0f);
    CheckNear(glm::length(measuredAcceleration), expectedMagnitude, expectedMagnitude * 0.01f,
              "ship acceleration matches Newtonian source mass and separation");
    Check(glm::dot(measuredAcceleration, glm::vec3(-1.0f, 0.0f, 0.0f)) > 0.0f,
          "ship acceleration points toward the attracting body");
    world.Shutdown();
}

float RunThrustEnergyChange(float directionSign, int steps, bool radialThrust = false) {
    PhysicsWorld world;
    Check(world.Init(), "thrust orbit world initialized");
    OrbitSetup orbit(world, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    const float initialEnergy = SpecificOrbitalEnergy(world, orbit);
    FlyingPrimitiveControl control;
    control.handle = orbit.ship;
    control.controlled = true;
    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);
    if (directionSign < 0.0f || radialThrust) {
        const BodyTransform oldTransform = world.GetTransform(orbit.ship);
        const glm::vec3 oldVelocity = world.GetLinearVelocity(orbit.ship);
        const glm::quat thrustOrientation = radialThrust
            ? RotationFromTo(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f))
            : oldTransform.rotation * glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
        world.ResetBody(orbit.ship, oldTransform.position, thrustOrientation);
        world.SetLinearVelocity(orbit.ship, oldVelocity);
    }
    Advance(world, orbit, control, window, steps);
    const float delta = SpecificOrbitalEnergy(world, orbit) - initialEnergy;
    world.Shutdown();
    return delta;
}

void TestThrustChangesOrbitAndEscape() {
    std::printf("Section B: spacecraft thrust changes its physical orbit\n");
    const float progradeEnergyChange = RunThrustEnergyChange(1.0f, 30);
    const float retrogradeEnergyChange = RunThrustEnergyChange(-1.0f, 30);
    const float radialEnergyChange = RunThrustEnergyChange(1.0f, 30, true);
    Check(progradeEnergyChange > 0.0f, "prograde spacecraft thrust raises orbital energy");
    Check(retrogradeEnergyChange < 0.0f, "retrograde spacecraft thrust lowers orbital energy");
    Check(radialEnergyChange > 0.0f && radialEnergyChange < progradeEnergyChange,
          "radial thrust produces a distinct, physically appropriate energy change");

    PhysicsWorld world;
    Check(world.Init(), "escape orbit world initialized");
    OrbitSetup orbit(world, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    FlyingPrimitiveControl control;
    control.handle = orbit.ship;
    control.controlled = true;
    Window window;
    window.SetTestInputMode(true);
    window.SetTestActionState(Action::MoveForward, true);
    Advance(world, orbit, control, window, 180);
    Check(SpecificOrbitalEnergy(world, orbit) > 0.0f,
          "sustained real thrust can raise the trajectory above escape energy");
    const float escapeStart = glm::length(RelativePosition(world, orbit));
    window.SetTestActionState(Action::MoveForward, false);
    Advance(world, orbit, control, window, 120);
    Check(glm::length(RelativePosition(world, orbit)) > escapeStart + 20.0f,
          "unpowered positive-energy trajectory continues escaping");
    world.Shutdown();
}

void TestSasCounterTorqueAndTranslationIndependence() {
    std::printf("Section C: SAS applies torque, holds attitude, and leaves translation alone\n");
    PhysicsWorld world;
    Check(world.Init(), "SAS world initialized");
    const glm::quat startOrientation = glm::angleAxis(0.7f, glm::normalize(glm::vec3(1.0f, 2.0f, -1.0f)));
    const BodyHandle ship = world.CreateDynamicBox(glm::vec3(0.0f), kHalfExtents,
                                                    kShipMass, 0.0f, 0.0f);
    world.ResetBody(ship, glm::vec3(0.0f), startOrientation);
    world.SetLinearVelocity(ship, glm::vec3(12.0f, -3.0f, 5.0f));
    world.SetAngularVelocity(ship, glm::vec3(0.7f, -0.5f, 0.4f));
    const glm::vec3 initialAngularVelocity = world.GetAngularVelocity(ship);
    Window window;
    window.SetTestInputMode(true);
    FlyingPrimitiveControl control;
    control.handle = ship;
    control.controlled = true;

    for (int i = 0; i < 30; ++i) {
        ApplyFlyingPrimitiveControl(control, window, world);
        world.Step(kDt);
    }
    Check(glm::length(world.GetAngularVelocity(ship) - initialAngularVelocity) < 1.0e-5f,
          "SAS off preserves M12 angular inertia when no rotation input is held");

    SetSpacecraftSasEnabled(control, true, world);
    const glm::quat target = world.GetTransform(ship).rotation;
    world.SetAngularVelocity(ship, glm::vec3(4.0f, -6.0f, 8.0f));
    window.SetTestActionState(Action::YawLeft, true);
    for (int i = 0; i < 600; ++i) {
        ApplyFlyingPrimitiveControl(control, window, world);
        world.Step(kDt);
    }
    Check(glm::length(world.GetAngularVelocity(ship)) < 0.01f,
          "SAS stops a fast three-axis tumble even with a rotation key held");
    Check(std::abs(glm::dot(world.GetTransform(ship).rotation, target)) > 0.999f,
          "SAS returns to and holds the attitude captured when enabled");
    Check(glm::length(world.GetLinearVelocity(ship) - glm::vec3(12.0f, -3.0f, 5.0f)) < 1.0e-4f,
          "SAS torque does not change spacecraft linear velocity");
    window.SetTestActionState(Action::YawLeft, false);

    // Brief external disturbance: SAS corrects the angular rate and restores
    // the captured attitude through ordinary torque accumulation/integration.
    world.ApplyTorque(ship, glm::vec3(200.0f, -100.0f, 50.0f));
    ApplyFlyingPrimitiveControl(control, window, world);
    world.Step(kDt);
    for (int i = 0; i < 240; ++i) {
        ApplyFlyingPrimitiveControl(control, window, world);
        world.Step(kDt);
    }
    Check(glm::length(world.GetAngularVelocity(ship)) < 0.02f,
          "SAS damps a small angular disturbance");
    Check(std::abs(glm::dot(world.GetTransform(ship).rotation, target)) > 0.999f,
          "SAS resists the disturbance and restores the attitude target");

    SetSpacecraftSasEnabled(control, false, world);
    world.SetAngularVelocity(ship, glm::vec3(0.2f, 0.1f, -0.3f));
    ApplyFlyingPrimitiveControl(control, window, world);
    world.Step(kDt);
    Check(glm::length(world.GetAngularVelocity(ship) - glm::vec3(0.2f, 0.1f, -0.3f)) < 1.0e-5f,
          "disabling SAS restores torque-free M12 rotational coast");
    world.Shutdown();
}

void TestSasRequestDraining() {
    std::printf("Section D: SAS input request is one-shot and can be drained while UI owns input\n");
    Window window;
    window.SetTestInputMode(true);
    window.RequestTestSasToggle();
    const bool consumedWhileMenuOwnsInput = window.ConsumeSasToggleRequest();
    Check(consumedWhileMenuOwnsInput, "X request is drained independently of gameplay policy");
    Check(!window.ConsumeSasToggleRequest(), "drained X request cannot toggle later on resume");
}
}  // namespace

int main() {
    TestCelestialSpacecraftOrbitAndRotation();
    TestThrustChangesOrbitAndEscape();
    TestSasCounterTorqueAndTranslationIndependence();
    TestSasRequestDraining();
    if (gFailures == 0) {
        std::printf("\nALL TESTS PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "\n%d test(s) failed\n", gFailures);
    return 1;
}
