#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "FluidWorld.h"
#include "GravityField.h"
#include "RadicalGravity.h"
#include "RadialTerrain.h"
#include "TerrainDemo.h"
#include "WorldCoordinates.h"

namespace {
constexpr float kDt = 1.0f / 60.0f;
constexpr float kSpacing = TerrainDemo::kWaterSpacing;
constexpr float kMass = 1000.0f * kSpacing * kSpacing * kSpacing;
int failures = 0;

void Check(bool condition, const char* label) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", label);
    if (!condition) ++failures;
}

bool Finite(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

struct RadialTestGravity final : GravityField {
    explicit RadialTestGravity(glm::vec3 center) : center(center) {}
    glm::vec3 Sample(const glm::vec3& point) const override {
        const glm::vec3 inward = center - point;
        const float distance = glm::length(inward);
        return distance > 1.0e-6f ? 9.81f * inward / distance : glm::vec3(0.0f);
    }
    glm::vec3 center;
};

struct ZeroGravity final : GravityField {
    glm::vec3 Sample(const glm::vec3&) const override { return glm::vec3(0.0f); }
};

FluidSettings LakeSettings() {
    FluidSettings settings;
    // Keep mass, kernel, collision radius and pressure correction at one
    // coherent physical resolution; the M24 algorithm itself is unchanged.
    settings.particleRadius *= 10.0f;
    settings.smoothingRadius *= 10.0f;
    settings.maxDensityCorrection *= 10.0f;
    return settings;
}

FluidTerrainCollider Collider(const RadialTerrain& surface,
                              const glm::vec3& center = glm::vec3(0.0f),
                              const glm::quat& rotation = glm::quat(1, 0, 0, 0)) {
    FluidTerrainCollider result;
    result.previousPose = BodyTransform{center, rotation};
    result.currentPose = result.previousPose;
    result.surface = &surface;
    return result;
}

void SeedBasinA(FluidWorld& fluid, const RadialTerrain& terrain, int side, int layers) {
    for (int x = 0; x < side; ++x) {
        for (int y = 0; y < layers; ++y) {
            for (int z = 0; z < side; ++z) {
                const float tx = TerrainDemo::kBasinAX + (x - (side - 1) * 0.5f) * kSpacing;
                const float tz = TerrainDemo::kBasinZ + (z - (side - 1) * 0.5f) * kSpacing;
                fluid.AddParticle(TerrainDemo::LocalPointAbove(
                    terrain, tx, tz, 0.25f + y * kSpacing), glm::vec3(0.0f), kMass);
            }
        }
    }
}

bool StateFinite(const FluidWorld& fluid) {
    for (const FluidParticle& particle : fluid.Particles()) {
        if (!Finite(particle.position) || !Finite(particle.velocity) ||
            !std::isfinite(particle.mass)) return false;
    }
    const FluidDiagnostics state = fluid.GetDiagnostics();
    return Finite(state.centerOfMass) && Finite(state.totalMomentum) &&
           std::isfinite(state.totalMass) && std::isfinite(state.kineticEnergy) &&
           std::isfinite(state.maxPositiveDensityError);
}

void TestEquivalence(const RadialTerrain& terrain) {
    std::printf("Rotated, translated and far-origin terrain/fluid contact\n");
    const glm::quat rotation = glm::angleAxis(0.83f,
        glm::normalize(glm::vec3(0.31f, -0.54f, 0.72f)));
    const glm::vec3 center(24.0f, -33.0f, 51.0f);
    const WorldCoordinates far(glm::dvec3(1.0e9, -2.0e9, 3.0e9));
    const glm::vec3 farCenter = far.ToLocal(far.ToGlobal(center));
    const glm::vec3 start = TerrainDemo::LocalPointAbove(terrain, -2.0f, 3.0f, 0.35f);
    const glm::vec3 velocity(0.30f, 0.0f, 0.12f);
    FluidWorld nearFluid(LakeSettings()), rotatedFluid(LakeSettings()),
               farFluid(LakeSettings());
    nearFluid.AddParticle(start, velocity, kMass);
    rotatedFluid.AddParticle(center + rotation * start, rotation * velocity, kMass);
    farFluid.AddParticle(far.ToLocal(far.ToGlobal(center + rotation * start)),
                         rotation * velocity, kMass);
    RadialTestGravity nearGravity(glm::vec3(0.0f)), rotatedGravity(center),
                      farGravity(farCenter);
    const FluidTerrainCollider nearCollider = Collider(terrain);
    const FluidTerrainCollider rotatedCollider = Collider(terrain, center, rotation);
    const FluidTerrainCollider farCollider = Collider(terrain, farCenter, rotation);
    for (int i = 0; i < 120; ++i) {
        nearFluid.Step(kDt, nearGravity, {}, {}, {nearCollider});
        rotatedFluid.Step(kDt, rotatedGravity, {}, {}, {rotatedCollider});
        farFluid.Step(kDt, farGravity, {}, {}, {farCollider});
        if ((i + 1) % 30 == 0) {
            std::printf("    step %d local/rotated position difference %.6f m\n", i + 1,
                glm::distance(rotatedFluid.Particles()[0].position,
                              center + rotation * nearFluid.Particles()[0].position));
        }
    }
    const glm::vec3 expectedPosition = center + rotation * nearFluid.Particles()[0].position;
    const glm::vec3 expectedVelocity = rotation * nearFluid.Particles()[0].velocity;
    const float positionError = glm::distance(rotatedFluid.Particles()[0].position,
                                               expectedPosition);
    const float velocityError = glm::distance(rotatedFluid.Particles()[0].velocity,
                                               expectedVelocity);
    const float farPositionError = glm::distance(farFluid.Particles()[0].position,
                                                  rotatedFluid.Particles()[0].position);
    const float farVelocityError = glm::distance(farFluid.Particles()[0].velocity,
                                                  rotatedFluid.Particles()[0].velocity);
    std::printf("    rotated %.6f m / %.6f m/s; far %.6f m / %.6f m/s\n",
                positionError, velocityError, farPositionError, farVelocityError);
    Check(positionError < 0.015f && velocityError < 0.020f,
          "fluid contact follows rotated/translated terrain and gravity");
    Check(farPositionError < 0.003f && farVelocityError < 0.005f,
          "billion-metre absolute translation preserves local fluid trajectory");
    Check(StateFinite(nearFluid) && StateFinite(rotatedFluid) && StateFinite(farFluid),
          "transformed states remain finite");
}

void TestMovingTerrainAndZeroGravity(const RadialTerrain& terrain) {
    std::printf("Moving terrain and zero gravity\n");
    ZeroGravity zero;
    const glm::vec3 start = TerrainDemo::LocalPointAbove(terrain, -4.0f, 3.0f, 0.18f);
    const glm::vec3 outward = glm::normalize(start);
    FluidWorld moved(LakeSettings());
    moved.AddParticle(start, glm::vec3(0.0f), kMass);
    FluidTerrainCollider movingCollider = Collider(terrain);
    movingCollider.currentPose.position = outward * 0.06f;
    moved.Step(kDt, zero, {}, {}, {movingCollider});
    std::printf("    moving terrain outward displacement %.6f m, speed %.6f m/s\n",
                glm::dot(moved.Particles()[0].position - start, outward),
                glm::dot(moved.Particles()[0].velocity, outward));
    std::printf("    new position %.5f %.5f %.5f, start %.5f %.5f %.5f, relative clearance %.5f\n",
                moved.Particles()[0].position.x, moved.Particles()[0].position.y,
                moved.Particles()[0].position.z, start.x, start.y, start.z,
                terrain.Sample(moved.Particles()[0].position -
                    movingCollider.currentPose.position).signedDistance);
    Check(glm::dot(moved.Particles()[0].position - start, outward) > 0.052f &&
          glm::dot(moved.Particles()[0].velocity, outward) > 2.0f,
          "moving terrain geometry pushes water and imparts point velocity");

    FluidWorld free;
    const glm::vec3 velocity(0.28f, -0.19f, 0.13f);
    free.AddParticle(TerrainDemo::LocalPointAbove(terrain, 1.0f, 2.0f, 3.0f),
                     velocity, 0.125f);
    const FluidTerrainCollider stationaryCollider = Collider(terrain);
    for (int i = 0; i < 30; ++i) free.Step(kDt, zero, {}, {}, {stationaryCollider});
    Check(glm::distance(free.Particles()[0].velocity, velocity) < 1.0e-5f,
          "terrain does not invent world-down acceleration in zero gravity");
}

void TestRotatedPressureOnTerrain(const RadialTerrain& terrain) {
    std::printf("Rotated multi-particle pressure on terrain\n");
    const glm::quat rotation = glm::angleAxis(0.67f,
        glm::normalize(glm::vec3(-0.42f, 0.36f, 0.77f)));
    const glm::vec3 center = TerrainDemo::LocalPointAbove(terrain, -3.0f, 3.0f, 0.45f);
    FluidWorld first(LakeSettings()), rotated(LakeSettings());
    FluidSettings noPressureSettings = LakeSettings();
    noPressureSettings.densityIterations = 0;
    FluidWorld noPressure(noPressureSettings);
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                const glm::vec3 point = center + glm::vec3(
                    (x - 0.5f) * 0.20f, (y - 0.5f) * 0.20f,
                    (z - 0.5f) * 0.20f);
                first.AddParticle(point, glm::vec3(0.0f), kMass);
                rotated.AddParticle(rotation * point, glm::vec3(0.0f), kMass);
                noPressure.AddParticle(point, glm::vec3(0.0f), kMass);
            }
        }
    }
    RadialTestGravity gravity(glm::vec3(0.0f));
    const FluidTerrainCollider firstCollider = Collider(terrain);
    const FluidTerrainCollider rotatedCollider = Collider(terrain, glm::vec3(0.0f), rotation);
    first.Step(kDt, gravity, {}, {}, {firstCollider});
    rotated.Step(kDt, gravity, {}, {}, {rotatedCollider});
    noPressure.Step(kDt, gravity, {}, {}, {firstCollider});
    float pressureEffect = 0.0f;
    for (std::size_t i = 0; i < first.Particles().size(); ++i) {
        pressureEffect = std::max(pressureEffect,
            glm::distance(first.Particles()[i].position,
                          noPressure.Particles()[i].position));
    }
    for (int step = 1; step < 30; ++step) {
        first.Step(kDt, gravity, {}, {}, {firstCollider});
        rotated.Step(kDt, gravity, {}, {}, {rotatedCollider});
    }
    float positionError = 0.0f;
    float velocityError = 0.0f;
    for (std::size_t i = 0; i < first.Particles().size(); ++i) {
        positionError = std::max(positionError,
            glm::distance(rotated.Particles()[i].position,
                          rotation * first.Particles()[i].position));
        velocityError = std::max(velocityError,
            glm::distance(rotated.Particles()[i].velocity,
                          rotation * first.Particles()[i].velocity));
    }
    std::printf("    pressure displacement %.6f m, maximum rotated errors "
                "%.6f m / %.6f m/s\n", pressureEffect,
                positionError, velocityError);
    Check(pressureEffect > 1.0e-4f,
          "eight-particle scenario genuinely exercises fluid pressure");
    Check(positionError < 0.02f && velocityError < 0.05f,
          "terrain collision and liquid pressure rotate equivalently");
}

void TestSolidLakeDisturbance(const RadialTerrain& terrain) {
    std::printf("Ordinary moving solid disturbs terrain water\n");
    FluidWorld disturbed(LakeSettings()), undisturbed(LakeSettings());
    for (int x = -1; x <= 1; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = -1; z <= 1; ++z) {
                const glm::vec3 position = TerrainDemo::LocalPointAbove(
                    terrain, TerrainDemo::kBasinAX + x * kSpacing,
                    TerrainDemo::kBasinZ + z * kSpacing,
                    0.25f + y * kSpacing);
                disturbed.AddParticle(position, glm::vec3(0.0f), kMass);
                undisturbed.AddParticle(position, glm::vec3(0.0f), kMass);
            }
        }
    }
    RadialTestGravity gravity(glm::vec3(0.0f));
    const FluidTerrainCollider terrainCollider = Collider(terrain);
    const glm::vec3 waterCenter = TerrainDemo::LocalPointAbove(
        terrain, TerrainDemo::kBasinAX, TerrainDemo::kBasinZ, 0.45f);
    glm::vec3 boxCenter(TerrainDemo::kBasinAX - 1.25f,
                        waterCenter.y, TerrainDemo::kBasinZ);
    glm::vec3 solidReaction(0.0f);
    int contactCount = 0;
    bool correctOwner = true;
    for (int step = 0; step < 6; ++step) {
        FluidBoxCollider movingBox;
        movingBox.owner.id = 42;
        movingBox.previousPose.position = boxCenter;
        boxCenter.x += 0.12f;
        movingBox.currentPose.position = boxCenter;
        movingBox.halfExtents = glm::vec3(0.15f, 0.40f, 0.80f);
        std::vector<FluidContactImpulse> contacts;
        disturbed.Step(kDt, gravity, {movingBox}, {}, {terrainCollider}, &contacts);
        undisturbed.Step(kDt, gravity, {}, {}, {terrainCollider});
        for (const FluidContactImpulse& contact : contacts) {
            solidReaction += contact.impulse;
            correctOwner = correctOwner && contact.owner.id == movingBox.owner.id;
            ++contactCount;
        }
    }
    const glm::vec3 extraMomentum = disturbed.GetDiagnostics().totalMomentum -
                                    undisturbed.GetDiagnostics().totalMomentum;
    const glm::vec3 extraDisplacement = disturbed.GetDiagnostics().centerOfMass -
                                        undisturbed.GetDiagnostics().centerOfMass;
    const float minimumClearance = [&] {
        float result = 1.0e20f;
        for (const FluidParticle& particle : disturbed.Particles()) {
            result = std::min(result, terrain.Sample(particle.position).signedDistance);
        }
        return result;
    }();
    std::printf("    %d moving-box contacts; extra COM displacement %.3f m, "
                "water momentum %.3f kg m/s along box travel; "
                "solid reaction %.3f kg m/s; "
                "min terrain clearance %.3f m\n",
                contactCount, extraDisplacement.x, extraMomentum.x,
                solidReaction.x, minimumClearance);
    Check(contactCount > 0 && extraDisplacement.x > 0.01f &&
          extraMomentum.x > 5.0f,
          "ordinary moving box displaces basin water and imparts momentum");
    Check(correctOwner && solidReaction.x < -5.0f,
          "fluid returns opposite contact impulse to the actual solid owner");
    Check(minimumClearance >= LakeSettings().particleRadius - 0.02f &&
          StateFinite(disturbed),
          "disturbed water remains finite and outside terrain solid matter");
}

void TestCoupledLakeBody(const std::shared_ptr<const RadialTerrain>& terrain) {
    std::printf("M25-scale water reaction on a dense Judas rigid body\n");
    PhysicsWorld physics;
    Check(physics.Init(), "rigid world initializes for the coupled lake fixture");
    const BodyHandle ground = physics.CreateStaticTerrain(
        glm::vec3(0.0f), glm::quat(1, 0, 0, 0), terrain, 0.8f, 0.0f);
    FluidWorld fluid(LakeSettings());
    const glm::vec3 seedCenter = TerrainDemo::LocalPointAbove(
        *terrain, TerrainDemo::kBasinAX, TerrainDemo::kBasinZ, 0.65f);
    for (int y = 0; y < 5; ++y) {
        for (int z = -2; z <= 2; ++z) {
            for (int x = -2; x <= 2; ++x) {
                fluid.AddParticle(seedCenter + glm::vec3(
                    x * kSpacing, y * kSpacing, z * kSpacing),
                    glm::vec3(0.0f), kMass);
            }
        }
    }
    RadialTestGravity gravity(glm::vec3(0.0f));
    FluidTerrainCollider groundCollider = Collider(*terrain);
    groundCollider.owner = ground;
    // Allow the actual 125-particle lake to settle before contact. A clone
    // without the box separates impact energy from ordinary fluid motion.
    for (int step = 0; step < 120; ++step)
        fluid.Step(kDt, gravity, {}, {}, {groundCollider});
    FluidWorld control(LakeSettings());
    float minimumWaterX = 1.0e20f;
    for (const FluidParticle& particle : fluid.Particles()) {
        control.AddParticle(particle.position, particle.velocity, particle.mass);
        minimumWaterX = std::min(minimumWaterX, particle.position.x);
    }
    const glm::vec3 halfExtents(0.45f);
    // The authored prop is a dense rock-like cube: one coarse 125 kg water
    // element is much lighter than the solid it contacts. Use the ordinary
    // M18 throw speed so this also catches excessive fluid reaction kicks.
    constexpr float bodyMass = 2000.0f;
    constexpr float initialSpeed = 8.0f;
    glm::vec3 bodyStart = TerrainDemo::LocalPointAbove(
        *terrain, minimumWaterX - 0.75f, TerrainDemo::kBasinZ, 0.80f);
    const BodyHandle body = physics.CreateDynamicBox(
        bodyStart, halfExtents, bodyMass, 0.8f, 0.0f);
    physics.SetLinearVelocity(body, glm::vec3(initialSpeed, 0.0f, 0.0f));
    std::size_t bodyContacts = 0;
    float reactionAlongTravel = 0.0f;
    float measuredVelocityChange = 0.0f;
    float maximumMomentumError = 0.0f;
    float minimumClearance = 1.0e20f;
    float maximumBodySpeed = initialSpeed;
    for (int step = 0; step < 24; ++step) {
        physics.ApplyLinearAcceleration(body, gravity.Sample(physics.GetTransform(body).position),
                                        kDt);
        physics.Step(kDt);
        const BodyBox previous = physics.GetPreviousBodyBoxes(body).front();
        const BodyBox current = physics.GetBodyBoxes(body).front();
        const FluidBoxCollider box{body,
            BodyTransform{previous.center, previous.rotation},
            BodyTransform{current.center, current.rotation}, current.halfExtents};
        std::vector<FluidContactImpulse> contacts;
        fluid.Step(kDt, gravity, {box}, {}, {groundCollider}, &contacts);
        control.Step(kDt, gravity, {}, {}, {groundCollider});
        const glm::vec3 beforeReaction = physics.GetLinearVelocity(body);
        glm::vec3 stepImpulse(0.0f);
        for (const FluidContactImpulse& contact : contacts) {
            if (contact.owner.id != body.id) continue;
            stepImpulse += contact.impulse;
            ++bodyContacts;
            physics.ApplyImpulseAtPoint(contact.owner, contact.impulse, contact.point);
        }
        const glm::vec3 afterReaction = physics.GetLinearVelocity(body);
        maximumBodySpeed = std::max(maximumBodySpeed, glm::length(afterReaction));
        reactionAlongTravel += stepImpulse.x;
        measuredVelocityChange += afterReaction.x - beforeReaction.x;
        maximumMomentumError = std::max(maximumMomentumError,
            glm::length((afterReaction - beforeReaction) * bodyMass - stepImpulse));
        for (const FluidParticle& particle : fluid.Particles()) {
            minimumClearance = std::min(minimumClearance,
                terrain->Sample(particle.position).signedDistance);
        }
    }
    const float bodySpeed = glm::length(physics.GetLinearVelocity(body));
    const glm::vec3 bodyAngularVelocity = physics.GetAngularVelocity(body);
    const float bodyRotationalEnergy = 0.5f * glm::dot(bodyAngularVelocity,
        physics.GetInertiaWorld(body) * bodyAngularVelocity);
    const float finalKineticEnergy = fluid.GetDiagnostics().kineticEnergy +
        0.5f * bodyMass * bodySpeed * bodySpeed + bodyRotationalEnergy;
    const float initialKineticEnergy = 0.5f * bodyMass * initialSpeed * initialSpeed;
    double radialPotentialDelta = bodyMass * 9.81 *
        (static_cast<double>(glm::length(physics.GetTransform(body).position)) -
         static_cast<double>(glm::length(bodyStart)));
    for (std::size_t i = 0; i < fluid.Particles().size(); ++i) {
        radialPotentialDelta += fluid.Particles()[i].mass * 9.81 *
            (static_cast<double>(glm::length(fluid.Particles()[i].position)) -
             static_cast<double>(glm::length(control.Particles()[i].position)));
    }
    const double excessMechanicalEnergy =
        static_cast<double>(finalKineticEnergy) - initialKineticEnergy -
        control.GetDiagnostics().kineticEnergy + radialPotentialDelta;
    std::printf("    %zu rigid contacts; water reaction %.2f kg m/s, body dv %.3f m/s; "
                "momentum-transfer error %.4f kg m/s; max/final body speed "
                "%.3f/%.3f m/s; fluid/control KE %.1f/%.1f J, body rotational KE %.1f J; "
                "excess mechanical energy %.1f J (%.3f of box input); "
                "min terrain clearance %.3f m\n",
                bodyContacts, reactionAlongTravel, measuredVelocityChange,
                maximumMomentumError, maximumBodySpeed, bodySpeed,
                fluid.GetDiagnostics().kineticEnergy,
                control.GetDiagnostics().kineticEnergy,
                bodyRotationalEnergy,
                excessMechanicalEnergy,
                excessMechanicalEnergy / initialKineticEnergy, minimumClearance);
    Check(bodyContacts > 0 && reactionAlongTravel < -5.0f &&
          measuredVelocityChange < -0.002f,
          "lake water slows the moving ordinary rigid body through actual contact impulses");
    Check(maximumMomentumError < 0.01f &&
          physics.IsDynamicBody(body) &&
          Finite(physics.GetTransform(body).position) &&
          Finite(physics.GetLinearVelocity(body)) && StateFinite(fluid) &&
          StateFinite(control),
          "Judas physics receives the measured fluid reaction without invalid state");
    Check(maximumBodySpeed < 9.0f && bodySpeed < initialSpeed &&
          excessMechanicalEnergy < 0.25 * initialKineticEnergy,
          "dense thrown prop does not gain a nonphysical kick or excess kinetic energy");
    Check(minimumClearance >= LakeSettings().particleRadius - 0.02f,
          "two-way lake contact leaves water outside the terrain solid");
}

struct BasinResult {
    FluidDiagnostics initial;
    FluidDiagnostics final;
    float minClearance = 1.0e20f;
    int leftOfSaddle = 0;
    int reachedB = 0;
    int trackedViaSaddle = 0;
    double millisecondsPerStep = 0.0;
    bool finite = true;
};

BasinResult SimulateBasin(const RadialTerrain& terrain, int layers, int steps) {
    FluidWorld fluid(LakeSettings());
    SeedBasinA(fluid, terrain, 5, layers);
    BasinResult result;
    result.initial = fluid.GetDiagnostics();
    const FluidTerrainCollider collider = Collider(terrain);
    RadialTestGravity gravity(glm::vec3(0.0f));
    std::vector<bool> crossedSaddle(fluid.Particles().size(), false);
    const auto start = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) {
        fluid.Step(kDt, gravity, {}, {}, {collider});
        if (step % 10 == 0) {
            for (std::size_t i = 0; i < fluid.Particles().size(); ++i) {
                const glm::vec3 position = fluid.Particles()[i].position;
                const float clearance = terrain.Sample(position).signedDistance;
                result.minClearance = std::min(result.minClearance, clearance);
                if (std::abs(position.x) < 0.5f &&
                    std::abs(position.z - TerrainDemo::kBasinZ) < 2.0f &&
                    clearance < 1.5f) crossedSaddle[i] = true;
            }
        }
    }
    const auto stop = std::chrono::steady_clock::now();
    result.millisecondsPerStep =
        std::chrono::duration<double, std::milli>(stop - start).count() / steps;
    result.final = fluid.GetDiagnostics();
    result.finite = StateFinite(fluid);
    for (std::size_t i = 0; i < fluid.Particles().size(); ++i) {
        const glm::vec3 position = fluid.Particles()[i].position;
        if (position.x < 0.0f) ++result.leftOfSaddle;
        const float clearance = terrain.Sample(position).signedDistance;
        if (position.x > 3.0f && position.x < 7.0f &&
            std::abs(position.z - TerrainDemo::kBasinZ) < 3.0f &&
            clearance < 1.1f) {
            ++result.reachedB;
            if (crossedSaddle[i]) ++result.trackedViaSaddle;
        }
    }
    return result;
}

void TestBasinAndOverflow(const RadialTerrain& terrain) {
    std::printf("Basin settlement and physical overflow\n");
    const BasinResult basin = SimulateBasin(terrain, 5, 600);
    std::printf("    125 particles, 10 s: %.3f ms/step; COM radius %.3f -> %.3f m; "
                "%d on A side; min clearance %.3f m; KE %.1f J\n",
                basin.millisecondsPerStep, glm::length(basin.initial.centerOfMass),
                glm::length(basin.final.centerOfMass), basin.leftOfSaddle,
                basin.minClearance, basin.final.kineticEnergy);
    Check(glm::length(basin.final.centerOfMass) <
          glm::length(basin.initial.centerOfMass) - 0.3f,
          "supplied radial gravity lowers water into the real depression");
    Check(basin.leftOfSaddle >= 95 && basin.final.centerOfMass.x < -1.5f,
          "most ordinary fluid mass concentrates in Basin A");
    Check(basin.minClearance >= LakeSettings().particleRadius - 0.02f,
          "fluid remains outside terrain during ten seconds of settlement");
    Check(basin.final.particleCount == 125 &&
          std::abs(basin.final.totalMass - basin.initial.totalMass) < 1.0e-3f &&
          basin.finite, "settled fluid conserves count/mass and remains finite");

    // More initial water occupies the same geometric basin. No particle is
    // transferred or inserted during the run, and there is no lake state.
    const BasinResult overflow = SimulateBasin(terrain, 8, 360);
    std::printf("    200 particles, 6 s: %.3f ms/step; %d settled in B, %d tracked "
                "through low saddle; min clearance %.3f m; mass %.1f kg\n",
                overflow.millisecondsPerStep, overflow.reachedB,
                overflow.trackedViaSaddle, overflow.minClearance,
                overflow.final.totalMass);
    Check(overflow.reachedB >= 25 && overflow.trackedViaSaddle >= 10,
          "surplus water crosses the low saddle and settles in Basin B");
    Check(overflow.minClearance >= LakeSettings().particleRadius - 0.02f,
          "overflow does not pass through terrain solid matter");
    Check(overflow.final.particleCount == 200 &&
          std::abs(overflow.final.totalMass - overflow.initial.totalMass) < 1.0e-3f &&
          overflow.finite, "overflow carries the same finite 25,000 kg of fluid");
}

void SeedLiveReset(FluidWorld& fluid, const RadialTerrain& terrain,
                   const glm::vec3& planetCenter) {
    // Mirror Application's M25 reset path exactly: one radial centre point,
    // then a Cartesian 5x5x5 lattice at 0.5 m spacing above Basin A.
    const glm::vec3 localCenter = TerrainDemo::LocalPointAbove(
        terrain, TerrainDemo::kBasinAX, TerrainDemo::kBasinZ, 0.65f);
    for (int y = 0; y < 5; ++y) {
        for (int z = -2; z <= 2; ++z) {
            for (int x = -2; x <= 2; ++x) {
                const glm::vec3 local = localCenter +
                    glm::vec3(x * kSpacing, y * kSpacing, z * kSpacing);
                fluid.AddParticle(planetCenter + local, glm::vec3(0.0f), kMass);
            }
        }
    }
}

void TestLiveResetAndEmission(const RadialTerrain& terrain) {
    std::printf("Live reset and B-key water emission schedule\n");
    const glm::vec3 planetCenter(300.0f, 0.0f, 0.0f);
    RadicalGravity gravity(planetCenter, 9.81f);
    const FluidTerrainCollider collider = Collider(terrain, planetCenter);
    FluidWorld emitted(LakeSettings());
    FluidWorld control(LakeSettings());
    SeedLiveReset(emitted, terrain, planetCenter);
    SeedLiveReset(control, terrain, planetCenter);
    const std::vector<FluidParticle> originalParticles = emitted.Particles();
    std::vector<bool> passedSaddle(200, false);
    float minimumClearance = 1.0e20f;
    int emittedCount = 0;
    double emittingStepMilliseconds = 0.0;
    for (int frame = 0; frame < 480; ++frame) {
        // Two seconds of settling, then hold B for 75 consecutive fixed
        // steps. Application inserts exactly one particle per step until
        // the 200-particle cap, before advancing FluidWorld that step.
        if (frame >= 120 && frame < 195 && emitted.Particles().size() < 200) {
            const glm::vec3 source = TerrainDemo::LocalPointAbove(
                terrain, TerrainDemo::kBasinAX, TerrainDemo::kBasinZ, 2.8f);
            const int column = emittedCount % 9;
            const glm::vec3 spread(
                static_cast<float>(column % 3 - 1) * 0.3f, 0.0f,
                static_cast<float>(column / 3 - 1) * 0.3f);
            emitted.AddParticle(planetCenter + source + spread,
                                glm::vec3(0.0f), kMass);
            ++emittedCount;
        }
        const auto start = std::chrono::steady_clock::now();
        emitted.Step(kDt, gravity, {}, {}, {collider});
        emittingStepMilliseconds += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        control.Step(kDt, gravity, {}, {}, {collider});
        if (frame % 5 == 0) {
            for (std::size_t i = 0; i < emitted.Particles().size(); ++i) {
                const glm::vec3 local = emitted.Particles()[i].position - planetCenter;
                const float clearance = terrain.Sample(local).signedDistance;
                minimumClearance = std::min(minimumClearance, clearance);
                if (std::abs(local.x) < 0.5f &&
                    std::abs(local.z - TerrainDemo::kBasinZ) < 2.0f &&
                    clearance < 1.5f) passedSaddle[i] = true;
            }
        }
    }
    const auto countInB = [&](const FluidWorld& fluid, int* emittedInB,
                              int* trackedThroughSaddle) {
        int count = 0;
        if (emittedInB) *emittedInB = 0;
        if (trackedThroughSaddle) *trackedThroughSaddle = 0;
        for (std::size_t i = 0; i < fluid.Particles().size(); ++i) {
            const glm::vec3 local = fluid.Particles()[i].position - planetCenter;
            const float clearance = terrain.Sample(local).signedDistance;
            if (local.x > 3.0f && local.x < 7.0f &&
                std::abs(local.z - TerrainDemo::kBasinZ) < 3.0f &&
                clearance < 1.1f) {
                ++count;
                if (emittedInB && i >= originalParticles.size()) ++*emittedInB;
                if (trackedThroughSaddle && passedSaddle[i]) ++*trackedThroughSaddle;
            }
        }
        return count;
    };
    int emittedInB = 0;
    int trackedThroughSaddle = 0;
    const int actualB = countInB(emitted, &emittedInB, &trackedThroughSaddle);
    const int controlB = countInB(control, nullptr, nullptr);
    const FluidDiagnostics state = emitted.GetDiagnostics();
    std::printf("    B held t=2.00..3.25s: %d added, %d in B versus %d without B "
                "(%d added particles reached B, %d traversed saddle); "
                "clearance >=%.3f m; %.3f ms/emitting-world step\n",
                emittedCount, actualB, controlB, emittedInB,
                trackedThroughSaddle, minimumClearance,
                emittingStepMilliseconds / 480.0);
    Check(emittedCount == 75 && state.particleCount == 200 &&
          std::abs(state.totalMass - 25000.0f) < 1.0e-3f,
          "the live 75-step B hold adds exactly 75 real particles and 9,375 kg");
    Check(actualB >= 25 && actualB >= controlB + 15 &&
          trackedThroughSaddle >= 20,
          "live source increases physical saddle flow into Basin B");
    Check(minimumClearance >= LakeSettings().particleRadius - 0.02f &&
          StateFinite(emitted),
          "emitted water remains finite and outside terrain solid matter");

    emitted.Clear();
    SeedLiveReset(emitted, terrain, planetCenter);
    bool sameReset = emitted.Particles().size() == originalParticles.size();
    for (std::size_t i = 0; sameReset && i < originalParticles.size(); ++i) {
        sameReset = glm::distance(emitted.Particles()[i].position,
                                  originalParticles[i].position) < 1.0e-6f;
    }
    Check(sameReset && emitted.GetDiagnostics().totalMass == 15625.0f,
          "reset reconstructs the exact original fluid distribution and mass");
}

void TestLightSpacecraftLakeContainment(const std::shared_ptr<const RadialTerrain>& terrain) {
    std::printf("M26 light spacecraft remains a one-way lake collider\n");
    const glm::vec3 planetCenter(300.0f, 0.0f, 0.0f);
    constexpr float shipMass = 80.0f;
    constexpr float gravitationalParameter = 9.81f * 80.0f * 80.0f;
    const glm::vec3 shipHalfExtents(2.0f, 0.25f, 3.0f);
    RadicalGravity waterGravity(planetCenter, 9.81f);

    PhysicsWorld wetWorld, dryWorld;
    Check(wetWorld.Init() && dryWorld.Init(),
          "one-way lake fixture initializes both rigid worlds");
    const BodyHandle terrainBody = wetWorld.CreateStaticTerrain(
        planetCenter, glm::quat(1, 0, 0, 0), terrain, 0.8f, 0.1f);
    dryWorld.CreateStaticTerrain(planetCenter, glm::quat(1, 0, 0, 0),
                                 terrain, 0.8f, 0.1f);
    FluidTerrainCollider terrainCollider = Collider(*terrain, planetCenter);
    terrainCollider.owner = terrainBody;

    FluidWorld lake(LakeSettings());
    SeedLiveReset(lake, *terrain, planetCenter);
    for (int step = 0; step < 120; ++step)
        lake.Step(kDt, waterGravity, {}, {}, {terrainCollider});
    FluidWorld noShipLake(LakeSettings());
    for (const FluidParticle& particle : lake.Particles())
        noShipLake.AddParticle(particle.position, particle.velocity, particle.mass);

    // The 4 x 0.5 x 6 m M21 craft starts nose-down, clear of water. Gravity
    // brings it into the settled basin after about 30 ordinary fixed steps.
    // This is the light-solid case that made the M25 position-projection
    // reaction impulse numerically explosive when returned to an 80 kg body.
    const glm::vec3 localStart = TerrainDemo::LocalPointAbove(
        *terrain, TerrainDemo::kBasinAX, TerrainDemo::kBasinZ, 7.0f);
    const glm::vec3 start = planetCenter + localStart;
    const glm::quat noseDown = glm::angleAxis(
        glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 initialVelocity = -3.0f * glm::normalize(localStart);
    const BodyHandle wetShip = wetWorld.CreateDynamicBox(
        start, shipHalfExtents, shipMass, 0.8f, 0.1f);
    const BodyHandle dryShip = dryWorld.CreateDynamicBox(
        start, shipHalfExtents, shipMass, 0.8f, 0.1f);
    wetWorld.ResetBody(wetShip, start, noseDown);
    dryWorld.ResetBody(dryShip, start, noseDown);
    wetWorld.SetLinearVelocity(wetShip, initialVelocity);
    dryWorld.SetLinearVelocity(dryShip, initialVelocity);

    const auto applyPlanetaryForce = [&](PhysicsWorld& world, BodyHandle ship) {
        const glm::vec3 displacement = planetCenter - world.GetTransform(ship).position;
        const float distanceSquared = glm::dot(displacement, displacement);
        world.ApplyForce(ship, shipMass * displacement *
            (gravitationalParameter / (distanceSquared * std::sqrt(distanceSquared))));
    };
    int firstContactStep = -1;
    std::size_t shipWaterContacts = 0;
    float peakShipSpeed = 0.0f;
    float peakAngularSpeed = 0.0f;
    float maxTrajectoryError = 0.0f;
    float minimumWaterClearance = 1.0e20f;
    bool finite = true;
    for (int step = 0; step < 120; ++step) {
        applyPlanetaryForce(wetWorld, wetShip);
        applyPlanetaryForce(dryWorld, dryShip);
        wetWorld.Step(kDt);
        dryWorld.Step(kDt);
        const BodyBox previous = wetWorld.GetPreviousBodyBoxes(wetShip).front();
        const BodyBox current = wetWorld.GetBodyBoxes(wetShip).front();
        const FluidBoxCollider shipCollider{wetShip,
            BodyTransform{previous.center, previous.rotation},
            BodyTransform{current.center, current.rotation}, current.halfExtents};
        std::vector<FluidContactImpulse> reactions;
        lake.Step(kDt, waterGravity, {shipCollider}, {}, {terrainCollider}, &reactions);
        noShipLake.Step(kDt, waterGravity, {}, {}, {terrainCollider});
        for (const FluidContactImpulse& reaction : reactions) {
            if (reaction.owner.id != wetShip.id) continue;
            if (firstContactStep < 0) firstContactStep = step;
            ++shipWaterContacts;
            // M26's bounded one-way policy: the actual ship shape moves
            // water, but coarse lake projection impulses do not kick this
            // much lighter rigid body. Dense M25 props remain two-way.
        }

        const glm::vec3 wetPosition = wetWorld.GetTransform(wetShip).position;
        const glm::vec3 dryPosition = dryWorld.GetTransform(dryShip).position;
        const glm::vec3 wetVelocity = wetWorld.GetLinearVelocity(wetShip);
        const glm::vec3 dryVelocity = dryWorld.GetLinearVelocity(dryShip);
        const glm::vec3 wetAngular = wetWorld.GetAngularVelocity(wetShip);
        peakShipSpeed = std::max(peakShipSpeed, glm::length(wetVelocity));
        peakAngularSpeed = std::max(peakAngularSpeed, glm::length(wetAngular));
        maxTrajectoryError = std::max(maxTrajectoryError,
            glm::distance(wetPosition, dryPosition) +
            glm::distance(wetVelocity, dryVelocity));
        finite = finite && Finite(wetPosition) && Finite(wetVelocity) &&
                 Finite(wetAngular);
        for (const FluidParticle& particle : lake.Particles()) {
            minimumWaterClearance = std::min(minimumWaterClearance,
                terrain->Sample(particle.position - planetCenter).signedDistance);
            finite = finite && Finite(particle.position) && Finite(particle.velocity);
        }
    }

    float maxWaterDisplacement = 0.0f;
    float meanWaterDisplacement = 0.0f;
    for (std::size_t i = 0; i < lake.Particles().size(); ++i) {
        const float separation = glm::distance(lake.Particles()[i].position,
                                                noShipLake.Particles()[i].position);
        maxWaterDisplacement = std::max(maxWaterDisplacement, separation);
        meanWaterDisplacement += separation;
    }
    meanWaterDisplacement /= static_cast<float>(lake.Particles().size());
    const FluidDiagnostics lakeState = lake.GetDiagnostics();
    std::printf("    first contact step %d, %zu ship-water contacts; ship peak %.3f m/s, "
                "peak angular %.3f rad/s, dry-trajectory error %.6f; "
                "water displacement max/mean %.3f/%.3f m, clearance >=%.3f m, "
                "mass %.1f kg\n",
                firstContactStep, shipWaterContacts, peakShipSpeed, peakAngularSpeed,
                maxTrajectoryError, maxWaterDisplacement, meanWaterDisplacement,
                minimumWaterClearance, lakeState.totalMass);
    Check(firstContactStep > 0 && shipWaterContacts > 0,
          "nose-down craft enters the lake from a genuinely clear initial pose");
    Check(finite && peakShipSpeed < 20.0f && peakAngularSpeed < 10.0f &&
          maxTrajectoryError < 1.0e-3f,
          "one-way lake contact cannot launch or deflect the light spacecraft");
    Check(maxWaterDisplacement > 0.3f && meanWaterDisplacement > 0.05f,
          "the same physical ship collider still displaces the lake water");
    Check(StateFinite(lake) && StateFinite(noShipLake) &&
          lakeState.particleCount == 125 &&
          std::abs(lakeState.totalMass - 15625.0f) < 1.0e-3f &&
          minimumWaterClearance >= LakeSettings().particleRadius - 0.02f,
          "one-way encounter retains finite mass and keeps water outside terrain");
}
} // namespace

int main() {
    const std::shared_ptr<const RadialTerrain> terrain = TerrainDemo::CreateSurface();
    TestEquivalence(*terrain);
    TestMovingTerrainAndZeroGravity(*terrain);
    TestRotatedPressureOnTerrain(*terrain);
    TestSolidLakeDisturbance(*terrain);
    TestCoupledLakeBody(terrain);
    TestBasinAndOverflow(*terrain);
    TestLiveResetAndEmission(*terrain);
    TestLightSpacecraftLakeContainment(terrain);
    std::printf("TerrainFluid: %s (%d failures)\n", failures == 0 ? "PASS" : "FAIL",
                failures);
    return failures == 0 ? 0 : 1;
}
