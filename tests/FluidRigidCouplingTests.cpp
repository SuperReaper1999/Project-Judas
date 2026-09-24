// Integration checks across Judas's real compound rigid bodies and fluid
// solver. The five boxes are ordinary collision geometry; the solver is
// never given a container identity or a stored liquid quantity.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "FluidWorld.h"
#include "GravityField.h"
#include "ObjectManipulation.h"
#include "PhysicsWorld.h"

namespace {

constexpr float kDt = 1.0f / 60.0f;
int failures = 0;

void Check(bool condition, const char* description) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", description);
    if (!condition) ++failures;
}

struct ConstantGravity final : GravityField {
    explicit ConstantGravity(const glm::vec3& acceleration) : acceleration(acceleration) {}
    glm::vec3 Sample(const glm::vec3&) const override { return acceleration; }
    glm::vec3 acceleration;
};

struct CenterwardGravity final : GravityField {
    glm::vec3 Sample(const glm::vec3& position) const override {
        return glm::dot(position, position) > 1.0e-10f
            ? -9.81f * glm::normalize(position) : glm::vec3(0.0f);
    }
};

std::vector<CompoundBox> OpenBoxGeometry() {
    return {
        {{0.0f, -0.35f, 0.0f}, {0.45f, 0.04f, 0.45f}},
        {{-0.41f, 0.04f, 0.0f}, {0.04f, 0.39f, 0.45f}},
        {{ 0.41f, 0.04f, 0.0f}, {0.04f, 0.39f, 0.45f}},
        {{0.0f, 0.04f, -0.41f}, {0.37f, 0.39f, 0.04f}},
        {{0.0f, 0.04f,  0.41f}, {0.37f, 0.39f, 0.04f}},
    };
}

std::vector<CompoundBox> SmallOpenBoxGeometry() {
    return {
        {{0.0f, 0.0f, 0.0f}, {0.18f, 0.015f, 0.18f}},
        {{-0.165f, 0.18f, 0.0f}, {0.015f, 0.18f, 0.18f}},
        {{ 0.165f, 0.18f, 0.0f}, {0.015f, 0.18f, 0.18f}},
        {{0.0f, 0.18f, -0.165f}, {0.18f, 0.18f, 0.015f}},
        {{0.0f, 0.18f,  0.165f}, {0.18f, 0.18f, 0.015f}},
    };
}

std::vector<CompoundBox> DemoCupGeometry() {
    constexpr float centerOfMassShift = 0.153f;
    return {
        {{0.0f, -centerOfMassShift, 0.0f}, {0.18f, 0.015f, 0.18f}},
        {{-0.165f, 0.195f - centerOfMassShift, 0.0f}, {0.015f, 0.18f, 0.18f}},
        {{ 0.165f, 0.195f - centerOfMassShift, 0.0f}, {0.015f, 0.18f, 0.18f}},
        {{0.0f, 0.195f - centerOfMassShift, -0.165f}, {0.15f, 0.18f, 0.015f}},
        {{0.0f, 0.195f - centerOfMassShift,  0.165f}, {0.15f, 0.18f, 0.015f}},
    };
}

void AppendBoxes(const PhysicsWorld& physics, BodyHandle handle,
                 std::vector<FluidBoxCollider>& out) {
    const std::vector<BodyBox> previous = physics.GetPreviousBodyBoxes(handle);
    const std::vector<BodyBox> current = physics.GetBodyBoxes(handle);
    if (previous.size() != current.size()) {
        Check(false, "current/prior rigid box counts match");
        return;
    }
    for (std::size_t i = 0; i < current.size(); ++i) {
        out.push_back(FluidBoxCollider{
            handle,
            BodyTransform{previous[i].center, previous[i].rotation},
            BodyTransform{current[i].center, current[i].rotation},
            current[i].halfExtents});
    }
}

struct CoupledStepResult {
    std::size_t contactCount = 0;
    glm::vec3 totalSolidImpulse{0.0f};
};

CoupledStepResult StepCoupled(PhysicsWorld& physics, FluidWorld& fluid,
                              const GravityField& fluidGravity,
                              const std::vector<BodyHandle>& solidHandles) {
    physics.Step(kDt);
    std::vector<FluidBoxCollider> boxes;
    for (BodyHandle handle : solidHandles) AppendBoxes(physics, handle, boxes);
    std::vector<FluidContactImpulse> impulses;
    fluid.Step(kDt, fluidGravity, boxes, &impulses);
    glm::vec3 impulseSum(0.0f);
    for (const FluidContactImpulse& contact : impulses) {
        physics.ApplyImpulseAtPoint(contact.owner, contact.impulse, contact.point);
        impulseSum += contact.impulse;
    }
    return {impulses.size(), impulseSum};
}

void TestStationaryFilledCup() {
    std::printf("Filled compound body on ordinary ground\n");
    PhysicsWorld physics;
    physics.Init();
    const BodyHandle ground = physics.CreateStaticBox(glm::vec3(0.0f, -0.49f, 0.0f),
                                                       glm::vec3(2.0f, 0.10f, 2.0f), 0.8f, 0.0f);
    const BodyHandle vessel = physics.CreateDynamicCompoundBoxes(
        glm::vec3(0.0f), OpenBoxGeometry(), 120.0f, 0.8f, 0.0f);
    Check(vessel.IsValid() && physics.GetBodyBoxes(vessel).size() == 5,
          "one dynamic body has a bottom and four open walls");

    ConstantGravity down(glm::vec3(0.0f, -9.81f, 0.0f));
    FluidWorld fluid;
    constexpr float spacing = 0.07f;
    constexpr float particleMass = 1000.0f * spacing * spacing * spacing;
    for (int y = 0; y < 4; ++y) {
        for (int z = -2; z <= 2; ++z) {
            for (int x = -2; x <= 2; ++x) {
                fluid.AddParticle(glm::vec3(x * spacing, -0.27f + y * spacing,
                                            z * spacing), glm::vec3(0.0f), particleMass);
            }
        }
    }
    const float initialMass = fluid.GetDiagnostics().totalMass;
    const auto start = std::chrono::steady_clock::now();
    float highestCupDisplacement = 0.0f;
    std::size_t contacts = 0;
    for (int step = 0; step < 180; ++step) {
        physics.ApplyLinearAcceleration(vessel, down.acceleration, kDt);
        const CoupledStepResult result = StepCoupled(physics, fluid, down, {ground, vessel});
        contacts += result.contactCount;
        highestCupDisplacement = std::max(highestCupDisplacement,
            std::abs(physics.GetTransform(vessel).position.y));
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();

    int escaped = 0;
    float lowest = 1.0e9f;
    const BodyTransform pose = physics.GetTransform(vessel);
    for (const FluidParticle& particle : fluid.Particles()) {
        const glm::vec3 local = glm::conjugate(pose.rotation) * (particle.position - pose.position);
        if (std::abs(local.x) > 0.375f || std::abs(local.z) > 0.375f ||
            local.y < -0.31f || local.y > 0.45f) {
            ++escaped;
        }
        lowest = std::min(lowest, local.y);
    }
    std::printf("    100 particles, 180 steps: %.1f ms total, %.3f ms/step; "
                "contacts %zu; escaped %d; max cup displacement %.4f m; lowest %.4f m\n",
                elapsed, elapsed / 180.0, contacts, escaped, highestCupDisplacement, lowest);
    Check(escaped == 0, "filled fluid stays inside the upright geometric vessel");
    Check(lowest >= -0.312f, "fluid does not pass through dynamic bottom");
    Check(highestCupDisplacement < 0.08f, "loaded dynamic vessel stays on ground");
    Check(std::abs(fluid.GetDiagnostics().totalMass - initialMass) < 2.0e-4f,
          "coupled contact preserves fluid mass");
}

void MeasureRestingFluid(const char* label, float velocitySmoothing, bool moveCup,
                         bool radialScene = false, float friction = 0.8f,
                         float cupMass = 120.0f) {
    PhysicsWorld physics;
    physics.Init();
    const glm::vec3 bearing = glm::normalize(glm::vec3(0.11f, 1.0f, 0.11f));
    const glm::quat stationRotation = radialScene
        ? glm::angleAxis(std::acos(glm::dot(glm::vec3(0.0f, 1.0f, 0.0f), bearing)),
                         glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), bearing)))
        : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::vec3 tableCenter = radialScene
        ? bearing * 20.12f : glm::vec3(0.0f, -0.12f, 0.0f);
    const BodyHandle ground = physics.CreateStaticBox(
        tableCenter, stationRotation, glm::vec3(0.85f, 0.12f, 0.42f), friction, 0.0f);
    if (radialScene)
        physics.CreateStaticSphere(glm::vec3(0.0f), 20.0f, 0.8f, 0.0f);
    const glm::vec3 cupStart = tableCenter +
        stationRotation * glm::vec3(-0.34f, 0.288f, 0.0f);
    const glm::vec3 emptyCupStart = tableCenter +
        stationRotation * glm::vec3(0.34f, 0.288f, 0.0f);
    const BodyHandle cup = physics.CreateDynamicCompoundBoxes(
        cupStart, DemoCupGeometry(), cupMass, friction, 0.0f);
    const BodyHandle emptyCup = physics.CreateDynamicCompoundBoxes(
        emptyCupStart, DemoCupGeometry(), cupMass, friction, 0.0f);
    physics.ResetBody(cup, cupStart, stationRotation);
    physics.ResetBody(emptyCup, emptyCupStart, stationRotation);

    FluidSettings settings;
    settings.velocitySmoothing = velocitySmoothing;
    FluidWorld fluid(settings);
    constexpr float spacing = 0.05f;
    const float particleMass = settings.restDensity * spacing * spacing * spacing;
    for (int y = 0; y < 5; ++y) {
        for (int z = -2; z <= 2; ++z) {
            for (int x = -2; x <= 2; ++x) {
                fluid.AddParticle(cupStart + stationRotation * glm::vec3(x * spacing,
                    0.05f - 0.153f + y * spacing, z * spacing),
                    glm::vec3(0.0f), particleMass);
            }
        }
    }
    ConstantGravity down(glm::vec3(0.0f, -9.81f, 0.0f));
    CenterwardGravity centerward;
    const GravityField& gravity = radialScene
        ? static_cast<const GravityField&>(centerward)
        : static_cast<const GravityField&>(down);
    const glm::vec3 initialCenter = fluid.GetDiagnostics().centerOfMass;
    double finalSecondEnergy = 0.0;
    int finalSecondSamples = 0;
    for (int step = 0; step < 600; ++step) {
        if (moveCup) {
            physics.ApplyLinearAcceleration(cup,
                                            gravity.Sample(physics.GetTransform(cup).position), kDt);
            physics.ApplyLinearAcceleration(emptyCup,
                                            gravity.Sample(physics.GetTransform(emptyCup).position), kDt);
            StepCoupled(physics, fluid, gravity, {ground, cup, emptyCup});
        } else {
            std::vector<FluidBoxCollider> boxes;
            AppendBoxes(physics, ground, boxes);
            AppendBoxes(physics, cup, boxes);
            AppendBoxes(physics, emptyCup, boxes);
            fluid.Step(kDt, gravity, boxes);
        }
        const FluidDiagnostics state = fluid.GetDiagnostics();
        if (step >= 540) {
            finalSecondEnergy += state.kineticEnergy;
            ++finalSecondSamples;
        }
        if (step == 179 || step == 599) {
            float maximumSpeed = 0.0f;
            for (const FluidParticle& particle : fluid.Particles())
                maximumSpeed = std::max(maximumSpeed, glm::length(particle.velocity));
            const BodyTransform cupPose = physics.GetTransform(cup);
            const BodyTransform emptyPose = physics.GetTransform(emptyCup);
            const glm::vec3 cupLocalDrift = glm::conjugate(stationRotation) *
                (cupPose.position - cupStart);
            const glm::vec3 emptyLocalDrift = glm::conjugate(stationRotation) *
                (emptyPose.position - emptyCupStart);
            std::printf("    %s at %2d s: energy %.4f J, rms %.4f m/s, max %.4f m/s, "
                        "COM drift %.4f m, cup drift %.4f m (local x %.4f m), cup speed %.4f m/s, "
                        "cup spin %.4f rad/s, empty drift %.4f m, speed %.4f m/s, "
                        "spin %.4f rad/s (local x %.4f m), over-density %.2f%%\n",
                        label, (step + 1) / 60, state.kineticEnergy,
                        std::sqrt(2.0f * state.kineticEnergy / state.totalMass),
                        maximumSpeed, glm::distance(state.centerOfMass, initialCenter),
                        glm::distance(cupPose.position, cupStart), cupLocalDrift.x,
                        glm::length(physics.GetLinearVelocity(cup)),
                        glm::length(physics.GetAngularVelocity(cup)),
                        glm::distance(emptyPose.position, emptyCupStart),
                        glm::length(physics.GetLinearVelocity(emptyCup)),
                        glm::length(physics.GetAngularVelocity(emptyCup)),
                        emptyLocalDrift.x,
                        100.0f * state.meanPositiveDensityError);
        }
    }
    std::printf("    %s final-second mean energy %.4f J\n", label,
                finalSecondEnergy / finalSecondSamples);
    Check(fluid.Particles().size() == 125 &&
          std::abs(fluid.GetDiagnostics().totalMass - 15.625f) < 0.001f,
          "resting-fluid measurement preserves the live demo's 125 particle masses");
}

void MeasureRestingFluidVariants() {
    std::printf("Live-sized resting cup, 3 s and 10 s diagnostics (measurement only)\n");
    MeasureRestingFluid("moving cup, smoothing .015", 0.015f, true);
    MeasureRestingFluid("frozen cup, smoothing .015", 0.015f, false);
    MeasureRestingFluid("moving cup, smoothing .150", 0.150f, true);
    MeasureRestingFluid("radial cups, smoothing .150", 0.150f, true, true);
    MeasureRestingFluid("radial cups, friction 2.0", 0.150f, true, true, 2.0f);
    MeasureRestingFluid("radial cups, mass 240", 0.150f, true, true, 0.8f, 240.0f);
}

void TestFluidReactionImpulse() {
    std::printf("Fluid contact transfers impulse to the rigid body\n");
    PhysicsWorld physics;
    physics.Init();
    const BodyHandle vessel = physics.CreateDynamicCompoundBoxes(
        glm::vec3(0.0f), OpenBoxGeometry(), 10.0f, 0.6f, 0.0f);
    ConstantGravity zero(glm::vec3(0.0f));
    FluidWorld fluid;
    fluid.AddParticle(glm::vec3(0.31f, -0.1f, 0.0f), glm::vec3(6.0f, 0.0f, 0.0f), 0.5f);
    const CoupledStepResult result = StepCoupled(physics, fluid, zero, {vessel});
    const glm::vec3 rigidVelocity = physics.GetLinearVelocity(vessel);
    std::printf("    contacts %zu, impulse x %.5f Ns, body vx %.5f m/s, water vx %.5f m/s\n",
                result.contactCount, result.totalSolidImpulse.x,
                rigidVelocity.x, fluid.Particles()[0].velocity.x);
    Check(result.contactCount > 0 && result.totalSolidImpulse.x > 0.0f,
          "water striking a wall returns an outward reaction impulse");
    Check(rigidVelocity.x > 0.0f,
          "reaction impulse accelerates the ordinary dynamic body");
    Check(std::abs(rigidVelocity.x * physics.GetMass(vessel) - result.totalSolidImpulse.x) < 1.0e-4f,
          "rigid body's momentum change equals the applied contact impulse");
}

void TestForcedTranslationAndRotation() {
    std::printf("Force/torque-driven vessel moves fluid in zero gravity\n");
    PhysicsWorld physics;
    physics.Init();
    const BodyHandle vessel = physics.CreateDynamicCompoundBoxes(
        glm::vec3(0.0f), OpenBoxGeometry(), 120.0f, 0.6f, 0.0f);
    ConstantGravity zero(glm::vec3(0.0f));
    FluidWorld fluid;
    fluid.AddParticle(glm::vec3(-0.30f, -0.19f, 0.0f), glm::vec3(0.0f), 0.35f);
    const glm::vec3 start = fluid.Particles()[0].position;
    std::size_t contacts = 0;
    for (int step = 0; step < 8; ++step) {
        physics.ApplyForce(vessel, glm::vec3(physics.GetMass(vessel) * 40.0f, 0.0f, 0.0f));
        physics.ApplyTorque(vessel,
            physics.GetInertiaWorld(vessel) * glm::vec3(0.0f, 0.0f, 12.0f));
        contacts += StepCoupled(physics, fluid, zero, {vessel}).contactCount;
    }
    const BodyTransform pose = physics.GetTransform(vessel);
    const float angle = glm::angle(pose.rotation);
    const glm::vec3 particle = fluid.Particles()[0].position;
    std::printf("    vessel x %.4f m, angle %.4f rad; water displacement (%.4f, %.4f, %.4f) m; "
                "contacts %zu\n", pose.position.x, angle,
                particle.x - start.x, particle.y - start.y, particle.z - start.z, contacts);
    Check(pose.position.x > 0.35f && angle > 0.04f,
          "ordinary force and torque translate and rotate one compound body");
    Check(contacts > 0 && particle.x - start.x > 0.08f,
          "moving and rotating walls impart motion to initially resting fluid");
    Check(std::isfinite(particle.x) && std::isfinite(particle.y) &&
          std::isfinite(fluid.Particles()[0].velocity.x),
          "coupled moving-container state stays finite");

    PhysicsWorld turningPhysics;
    turningPhysics.Init();
    const BodyHandle turningVessel = turningPhysics.CreateDynamicCompoundBoxes(
        glm::vec3(0.0f), OpenBoxGeometry(), 120.0f, 0.6f, 0.0f);
    FluidWorld turningFluid;
    const glm::vec3 turningStart(0.34f, 0.25f, 0.0f);
    turningFluid.AddParticle(turningStart, glm::vec3(0.0f), 0.35f);
    std::size_t turningContacts = 0;
    for (int step = 0; step < 12; ++step) {
        turningPhysics.ApplyTorque(turningVessel,
            turningPhysics.GetInertiaWorld(turningVessel) * glm::vec3(0.0f, 0.0f, 20.0f));
        turningContacts += StepCoupled(turningPhysics, turningFluid, zero,
                                       {turningVessel}).contactCount;
    }
    const BodyTransform turnedPose = turningPhysics.GetTransform(turningVessel);
    const float rotationOnlyDisplacement = glm::distance(
        turningFluid.Particles()[0].position, turningStart);
    std::printf("    pure torque: vessel angle %.4f rad; water displacement %.4f m; "
                "contacts %zu\n", glm::angle(turnedPose.rotation),
                rotationOnlyDisplacement, turningContacts);
    Check(turningContacts > 0 && rotationOnlyDisplacement > 0.04f,
          "rotating walls alone impart motion without gravity or vessel translation force");
}

void MeasureDemoResolutionCost() {
    std::printf("Two-vessel demo-resolution timing (measurement only)\n");
    PhysicsWorld physics;
    physics.Init();
    const BodyHandle ground = physics.CreateStaticBox(glm::vec3(0.0f, -0.115f, 0.0f),
                                                       glm::vec3(2.0f, 0.10f, 2.0f), 0.8f, 0.0f);
    const BodyHandle a = physics.CreateDynamicCompoundBoxes(
        glm::vec3(-0.40f, 0.0f, 0.0f), SmallOpenBoxGeometry(), 24.0f, 0.8f, 0.0f);
    const BodyHandle b = physics.CreateDynamicCompoundBoxes(
        glm::vec3(0.40f, 0.0f, 0.0f), SmallOpenBoxGeometry(), 24.0f, 0.8f, 0.0f);
    ConstantGravity down(glm::vec3(0.0f, -9.81f, 0.0f));
    FluidWorld fluid;
    constexpr float spacing = 0.04f;
    constexpr float particleMass = 1000.0f * spacing * spacing * spacing;
    for (int y = 0; y < 5; ++y) {
        for (int z = -2; z <= 2; ++z) {
            for (int x = -2; x <= 2; ++x) {
                fluid.AddParticle(glm::vec3(-0.40f + x * spacing,
                                            0.05f + y * spacing,
                                            z * spacing), glm::vec3(0.0f), particleMass);
            }
        }
    }
    const auto start = std::chrono::steady_clock::now();
    for (int step = 0; step < 60; ++step) {
        physics.ApplyLinearAcceleration(a, down.acceleration, kDt);
        physics.ApplyLinearAcceleration(b, down.acceleration, kDt);
        StepCoupled(physics, fluid, down, {ground, a, b});
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("    %zu particles, 11 box colliders, 60 coupled steps: %.1f ms total, "
                "%.3f ms/step\n", fluid.Particles().size(), elapsed, elapsed / 60.0);
}

void TestTwoPhysicalCupsPourAndPourBack(float velocitySmoothing) {
    std::printf("Two force/torque-driven compound cups pour the same water "
                "(smoothing %.3f)\n", velocitySmoothing);
    PhysicsWorld physics;
    physics.Init();
    const BodyHandle ground = physics.CreateStaticBox(glm::vec3(0.0f, -0.115f, 0.0f),
                                                       glm::vec3(2.5f, 0.10f, 2.0f), 0.8f, 0.0f);
    const BodyHandle a = physics.CreateDynamicCompoundBoxes(
        glm::vec3(-0.15f, 0.90f, 0.0f), SmallOpenBoxGeometry(), 24.0f, 0.8f, 0.0f);
    const BodyHandle b = physics.CreateDynamicCompoundBoxes(
        glm::vec3(0.20f, 0.0f, 0.0f), SmallOpenBoxGeometry(), 24.0f, 0.8f, 0.0f);
    ObjectManipulation manipulation({a, b});
    Check(manipulation.TryPickUp(a, physics), "ordinary manipulation acquires Cup A");

    ConstantGravity down(glm::vec3(0.0f, -9.81f, 0.0f));
    FluidSettings settings;
    settings.velocitySmoothing = velocitySmoothing;
    FluidWorld fluid(settings);
    constexpr float spacing = 0.04f;
    constexpr float particleMass = 1000.0f * spacing * spacing * spacing;
    for (int y = 0; y < 5; ++y) {
        for (int z = -2; z <= 2; ++z) {
            for (int x = -2; x <= 2; ++x) {
                const glm::vec3 local(x * spacing,
                                      0.05f + y * spacing,
                                      z * spacing);
                fluid.AddParticle(physics.GetTransform(a).position + local,
                                  glm::vec3(0.0f), particleMass);
            }
        }
    }
    const float initialMass = fluid.GetDiagnostics().totalMass;
    std::vector<std::size_t> waterInB;
    std::vector<bool> crossedARim(fluid.Particles().size(), false);
    int maximumInB = 0;
    int crossedA = 0;
    int exitedB = 0;
    for (int step = 0; step < 350; ++step) {
        if (step == 190) {
            manipulation.Drop();
            Check(manipulation.TryPickUp(b, physics), "ordinary manipulation acquires Cup B");
        }
        if (manipulation.HeldBody().id == a.id) {
            const float tiltFraction = std::clamp((step - 25) / 60.0f, 0.0f, 1.0f);
            const float shiftFraction = std::clamp((step - 150) / 35.0f, 0.0f, 1.0f);
            const glm::vec3 target(-0.15f - shiftFraction * 0.75f, 0.9f, 0.0f);
            manipulation.ApplyCarryForce(physics, target, glm::vec3(0.0f));
            manipulation.ApplyCarryOrientationTorque(physics,
                glm::angleAxis(-2.15f * tiltFraction, glm::vec3(0.0f, 0.0f, 1.0f)));
        } else if (manipulation.HeldBody().id == b.id) {
            const float liftFraction = std::clamp((step - 190) / 40.0f, 0.0f, 1.0f);
            const float tiltFraction = std::clamp((step - 230) / 60.0f, 0.0f, 1.0f);
            const glm::vec3 target(0.20f, liftFraction * 0.50f, 0.0f);
            manipulation.ApplyCarryForce(physics, target, glm::vec3(0.0f));
            manipulation.ApplyCarryOrientationTorque(physics,
                glm::angleAxis(-2.15f * tiltFraction, glm::vec3(0.0f, 0.0f, 1.0f)));
        }
        physics.ApplyLinearAcceleration(a, down.acceleration, kDt);
        physics.ApplyLinearAcceleration(b, down.acceleration, kDt);
        StepCoupled(physics, fluid, down, {ground, a, b});

        int inB = 0;
        const BodyTransform poseA = physics.GetTransform(a);
        const BodyTransform poseB = physics.GetTransform(b);
        for (std::size_t i = 0; i < fluid.Particles().size(); ++i) {
            const glm::vec3 fromA = glm::conjugate(poseA.rotation) *
                (fluid.Particles()[i].position - poseA.position);
            const glm::vec3 fromB = glm::conjugate(poseB.rotation) *
                (fluid.Particles()[i].position - poseB.position);
            if (step < 150 && !crossedARim[i] && fromA.y > 0.36f) {
                crossedARim[i] = true;
                ++crossedA;
            }
            if (std::abs(fromB.x) < 0.15f && std::abs(fromB.z) < 0.15f &&
                fromB.y > 0.017f && fromB.y < 0.36f) {
                ++inB;
                if (step == 189) waterInB.push_back(i);
            }
        }
        maximumInB = std::max(maximumInB, inB);
        if (step % 30 == 29) {
            const glm::vec3 waterCenter = fluid.GetDiagnostics().centerOfMass;
            std::printf("    step %3d: A angle %.2f, A (%.2f,%.2f), B angle %.2f, "
                        "B y %.2f, water COM (%.2f,%.2f), in B %d\n", step + 1,
                        glm::angle(poseA.rotation), poseA.position.x, poseA.position.y,
                        glm::angle(poseB.rotation), poseB.position.y,
                        waterCenter.x, waterCenter.y, inB);
        }
    }
    const BodyTransform finalB = physics.GetTransform(b);
    for (std::size_t i : waterInB) {
        const glm::vec3 local = glm::conjugate(finalB.rotation) *
            (fluid.Particles()[i].position - finalB.position);
        if (std::abs(local.x) > 0.19f || std::abs(local.z) > 0.19f ||
            local.y > 0.40f || local.y < 0.0f) {
            ++exitedB;
        }
    }
    std::printf("    crossed A open rim %d particles; peak in B %d, "
                "in B before re-pour %zu, those leaving B %d\n",
                crossedA, maximumInB, waterInB.size(), exitedB);
    Check(crossedA >= 60 && maximumInB >= 60 && waterInB.size() >= 60,
          "physical A tip sends fluid across its open rim into physical B");
    Check(exitedB >= 50,
          "the same particles later leave torque-driven Cup B");
    Check(std::abs(fluid.GetDiagnostics().totalMass - initialMass) < 2.0e-4f,
          "two physical pours preserve fluid mass");
}

}  // namespace

int main() {
    TestStationaryFilledCup();
    MeasureRestingFluidVariants();
    TestFluidReactionImpulse();
    TestForcedTranslationAndRotation();
    TestTwoPhysicalCupsPourAndPourBack(0.015f);
    TestTwoPhysicalCupsPourAndPourBack(0.150f);
    MeasureDemoResolutionCost();
    std::printf("Fluid/rigid coupling: %s (%d failures)\n",
                failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
