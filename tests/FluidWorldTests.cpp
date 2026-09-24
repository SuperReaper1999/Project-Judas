#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "FluidWorld.h"
#include "GravityField.h"
#include "WorldCoordinates.h"

namespace {

int failures = 0;

void Check(bool condition, const char* description) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", description);
    if (!condition) ++failures;
}

bool Near(const glm::vec3& a, const glm::vec3& b, float tolerance) {
    return glm::length(a - b) <= tolerance;
}

struct ConstantGravity final : GravityField {
    explicit ConstantGravity(const glm::vec3& acceleration) : acceleration(acceleration) {}
    glm::vec3 Sample(const glm::vec3&) const override { return acceleration; }
    glm::vec3 acceleration;
};

FluidBoxCollider Box(const glm::vec3& previousPosition, const glm::quat& previousRotation,
                     const glm::vec3& currentPosition, const glm::quat& currentRotation,
                     const glm::vec3& halfExtents) {
    FluidBoxCollider box;
    box.previousPose.position = previousPosition;
    box.previousPose.rotation = previousRotation;
    box.currentPose.position = currentPosition;
    box.currentPose.rotation = currentRotation;
    box.halfExtents = halfExtents;
    return box;
}

std::vector<FluidBoxCollider> OpenCupGeometry(const glm::vec3& previousOrigin,
                                               float previousAngle,
                                               const glm::vec3& currentOrigin,
                                               float currentAngle) {
    // Five ordinary box primitives, no container state or water amount.
    const glm::vec3 localCenters[5] = {
        {0.0f, 0.0f, 0.0f}, {-0.165f, 0.18f, 0.0f}, {0.165f, 0.18f, 0.0f},
        {0.0f, 0.18f, -0.165f}, {0.0f, 0.18f, 0.165f}};
    const glm::vec3 halfExtents[5] = {
        {0.18f, 0.015f, 0.18f}, {0.015f, 0.18f, 0.18f},
        {0.015f, 0.18f, 0.18f}, {0.18f, 0.18f, 0.015f},
        {0.18f, 0.18f, 0.015f}};
    const glm::quat previousRotation = glm::angleAxis(previousAngle, glm::vec3(0, 0, 1));
    const glm::quat currentRotation = glm::angleAxis(currentAngle, glm::vec3(0, 0, 1));
    std::vector<FluidBoxCollider> boxes;
    boxes.reserve(5);
    for (int i = 0; i < 5; ++i) {
        boxes.push_back(Box(previousOrigin + previousRotation * localCenters[i], previousRotation,
                            currentOrigin + currentRotation * localCenters[i], currentRotation,
                            halfExtents[i]));
    }
    return boxes;
}

void TestGravityAndZeroGravity() {
    std::printf("Fluid gravity and momentum\n");
    const float dt = 1.0f / 60.0f;
    const glm::vec3 direction = glm::normalize(glm::vec3(0.37f, -0.82f, 0.44f));
    ConstantGravity gravity(direction * 9.81f);
    FluidWorld fluid;
    fluid.AddParticle(glm::vec3(0.2f, -0.3f, 0.7f), glm::vec3(0.0f), 0.02f);
    for (int i = 0; i < 30; ++i) fluid.Step(dt, gravity, {});
    const FluidParticle& particle = fluid.Particles()[0];
    std::printf("    gravity speed %.6f, transverse %.6f\n",
                glm::dot(particle.velocity, direction),
                glm::length(glm::cross(particle.velocity, direction)));
    Check(glm::dot(particle.velocity, direction) > 4.8f &&
              glm::length(glm::cross(particle.velocity, direction)) < 1.0e-4f,
          "free liquid accelerates along supplied arbitrary gravity");

    ConstantGravity zero(glm::vec3(0.0f));
    FluidWorld freeFluid;
    const glm::vec3 initialVelocity(0.46f, -0.28f, 0.71f);
    freeFluid.AddParticle(glm::vec3(0.1f), initialVelocity, 0.02f);
    for (int i = 0; i < 120; ++i) freeFluid.Step(dt, zero, {});
    Check(Near(freeFluid.Particles()[0].velocity, initialVelocity, 2.0e-4f),
          "zero gravity creates no world-down velocity or hidden drag");

    const glm::quat tilted = glm::angleAxis(-1.2f, glm::vec3(0, 0, 1));
    const glm::vec3 cupPosition(0.3f, -0.4f, 0.2f);
    const glm::vec3 particleAtRest = cupPosition + tilted * glm::vec3(0.0f, 0.15f, 0.0f);
    FluidWorld tiltedCupFluid;
    tiltedCupFluid.AddParticle(particleAtRest, glm::vec3(0.0f), 0.02f);
    const std::vector<FluidBoxCollider> tiltedCup = OpenCupGeometry(
        cupPosition, -1.2f, cupPosition, -1.2f);
    for (int i = 0; i < 120; ++i) {
        tiltedCupFluid.Step(dt, zero, tiltedCup);
    }
    Check(Near(tiltedCupFluid.Particles()[0].position, particleAtRest, 1.0e-5f),
          "a tilted open cup invents no world-down water motion in zero gravity");
}

void TestRotatedUniverse() {
    std::printf("Rotated fluid scenario\n");
    const glm::quat turn = glm::angleAxis(0.94f, glm::normalize(glm::vec3(0.3f, 0.7f, -0.5f)));
    const glm::vec3 acceleration(0.3f, -9.1f, 0.2f);
    ConstantGravity gravity(acceleration);
    ConstantGravity rotatedGravity(turn * acceleration);
    FluidWorld first;
    FluidWorld second;
    const glm::vec3 initial(0.1f, 0.9f, -0.2f);
    const glm::vec3 initialVelocity(0.1f, -0.4f, 0.05f);
    first.AddParticle(initial, initialVelocity, 0.02f);
    second.AddParticle(turn * initial, turn * initialVelocity, 0.02f);
    const FluidBoxCollider floor = Box(glm::vec3(0), glm::quat(1, 0, 0, 0),
                                       glm::vec3(0), glm::quat(1, 0, 0, 0),
                                       glm::vec3(1.0f, 0.1f, 1.0f));
    const FluidBoxCollider rotatedFloor = Box(glm::vec3(0), turn, glm::vec3(0), turn,
                                              glm::vec3(1.0f, 0.1f, 1.0f));
    for (int i = 0; i < 65; ++i) {
        first.Step(1.0f / 60.0f, gravity, {floor});
        second.Step(1.0f / 60.0f, rotatedGravity, {rotatedFloor});
    }
    std::printf("    rotated position error %.6f, velocity error %.6f\n",
                glm::length(second.Particles()[0].position - turn * first.Particles()[0].position),
                glm::length(second.Particles()[0].velocity - turn * first.Particles()[0].velocity));
    Check(Near(second.Particles()[0].position, turn * first.Particles()[0].position, 0.002f),
          "rotated gravity and collider produce equivalent local position");
    Check(Near(second.Particles()[0].velocity, turn * first.Particles()[0].velocity, 0.002f),
          "rotated gravity and collider produce equivalent velocity");
}

void TestRotatedPressureCluster() {
    std::printf("Rotated 3D density-pressure cluster\n");
    const glm::quat turn = glm::angleAxis(0.83f,
        glm::normalize(glm::vec3(-0.37f, 0.61f, 0.48f)));
    const glm::vec3 acceleration(0.3f, -1.7f, 0.9f);
    ConstantGravity gravity(acceleration);
    ConstantGravity rotatedGravity(turn * acceleration);
    const glm::vec3 center(0.31f, 0.58f, -0.42f);
    const glm::vec3 offsets[] = {
        {0.000f, 0.020f, 0.010f}, {0.018f, -0.006f, 0.004f},
        {-0.015f, 0.009f, -0.013f}, {0.007f, 0.024f, -0.018f},
        {-0.010f, -0.020f, 0.019f}, {0.024f, 0.012f, 0.022f},
        {-0.021f, -0.014f, -0.007f}, {0.004f, -0.024f, -0.023f}};
    const glm::vec3 velocities[] = {
        {0.11f, -0.03f, 0.07f}, {-0.04f, 0.08f, -0.02f},
        {0.03f, -0.06f, 0.09f}, {-0.08f, 0.02f, -0.05f},
        {0.07f, 0.04f, 0.01f}, {-0.02f, -0.09f, 0.06f},
        {0.05f, 0.01f, -0.07f}, {-0.06f, 0.07f, 0.03f}};
    FluidWorld first;
    FluidWorld rotated;
    FluidSettings noPressureSettings;
    noPressureSettings.densityIterations = 0;
    FluidWorld withoutPressure(noPressureSettings);
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 position = center + offsets[i];
        first.AddParticle(position, velocities[i], 0.16f);
        rotated.AddParticle(turn * position, turn * velocities[i], 0.16f);
        withoutPressure.AddParticle(position, velocities[i], 0.16f);
    }

    constexpr float dt = 1.0f / 60.0f;
    first.Step(dt, gravity, {});
    rotated.Step(dt, rotatedGravity, {});
    withoutPressure.Step(dt, gravity, {});
    float pressureDisplacement = 0.0f;
    for (std::size_t i = 0; i < first.Particles().size(); ++i) {
        pressureDisplacement = std::max(pressureDisplacement,
            glm::distance(first.Particles()[i].position,
                          withoutPressure.Particles()[i].position));
    }
    Check(pressureDisplacement > 1.0e-4f,
          "cluster actually exercises density pressure rather than only free flight");

    for (int step = 1; step < 30; ++step) {
        first.Step(dt, gravity, {});
        rotated.Step(dt, rotatedGravity, {});
    }
    float maximumPositionError = 0.0f;
    float maximumVelocityError = 0.0f;
    for (std::size_t i = 0; i < first.Particles().size(); ++i) {
        maximumPositionError = std::max(maximumPositionError,
            glm::distance(rotated.Particles()[i].position,
                          turn * first.Particles()[i].position));
        maximumVelocityError = std::max(maximumVelocityError,
            glm::distance(rotated.Particles()[i].velocity,
                          turn * first.Particles()[i].velocity));
    }
    std::printf("    pressure displacement %.6f m, maximum rotation errors "
                "%.6f m / %.6f m/s\n", pressureDisplacement,
                maximumPositionError, maximumVelocityError);
    Check(maximumPositionError < 1.0e-3f && maximumVelocityError < 2.0e-3f,
          "non-symmetric 3D fluid pressure rotates equivalently over 30 fixed steps");
}

void TestStationaryAndMovingBox() {
    std::printf("Fluid/solid contact\n");
    ConstantGravity down(glm::vec3(0.0f, -9.81f, 0.0f));
    FluidWorld fluid;
    fluid.AddParticle(glm::vec3(0.0f, 0.7f, 0.0f), glm::vec3(0.0f), 0.02f);
    const FluidBoxCollider floor = Box(glm::vec3(0), glm::quat(1, 0, 0, 0),
                                       glm::vec3(0), glm::quat(1, 0, 0, 0),
                                       glm::vec3(1.0f, 0.5f, 1.0f));
    for (int i = 0; i < 120; ++i) fluid.Step(1.0f / 60.0f, down, {floor});
    Check(fluid.Particles()[0].position.y >= 0.517f,
          "stationary geometric bottom contains water without penetration");
    Check(std::abs(fluid.Particles()[0].velocity.y) < 0.15f,
          "water settles rather than bouncing indefinitely on a bottom");

    ConstantGravity zero(glm::vec3(0.0f));
    FluidWorld lifted;
    lifted.AddParticle(glm::vec3(0.0f, 0.518f, 0.0f), glm::vec3(0.0f), 0.02f);
    FluidBoxCollider movingFloor = Box(glm::vec3(0), glm::quat(1, 0, 0, 0),
                                       glm::vec3(0.0f, 0.06f, 0.0f),
                                       glm::quat(1, 0, 0, 0),
                                       glm::vec3(1.0f, 0.5f, 1.0f));
    movingFloor.owner.id = 42;
    std::vector<FluidContactImpulse> reactionImpulses;
    lifted.Step(1.0f / 60.0f, zero, {movingFloor}, &reactionImpulses);
    Check(lifted.Particles()[0].position.y >= 0.576f,
          "moving box physically displaces contacting water");
    Check(lifted.Particles()[0].velocity.y > 2.0f,
          "moving box imparts its point velocity to water in zero gravity");
    glm::vec3 solidReaction(0.0f);
    for (const FluidContactImpulse& impulse : reactionImpulses) {
        Check(impulse.owner.id == movingFloor.owner.id,
              "fluid reaction identifies the actual rigid-body owner");
        solidReaction += impulse.impulse;
    }
    Check(!reactionImpulses.empty() &&
              Near(solidReaction + lifted.Particles()[0].mass *
                   lifted.Particles()[0].velocity, glm::vec3(0.0f), 0.003f),
          "moving-wall momentum gain has an equal-and-opposite rigid-body reaction");

    FluidWorld spun;
    spun.AddParticle(glm::vec3(0.4f, 0.075f, 0.0f), glm::vec3(0.0f), 0.02f);
    const glm::quat rotation = glm::angleAxis(0.2f, glm::vec3(0.0f, 0.0f, 1.0f));
    const FluidBoxCollider turningFloor = Box(glm::vec3(0), glm::quat(1, 0, 0, 0),
                                              glm::vec3(0), rotation,
                                              glm::vec3(0.5f, 0.04f, 0.5f));
    spun.Step(1.0f / 60.0f, zero, {turningFloor});
    Check(spun.Particles()[0].position.y > 0.09f,
          "rotating box imparts motion at its off-centre contact point");
}

void TestSphereAndFarOrigin() {
    std::printf("Sphere geometry and distant origin\n");
    ConstantGravity down(glm::vec3(0.0f, -9.81f, 0.0f));
    FluidWorld fluid;
    fluid.AddParticle(glm::vec3(0.0f, 1.25f, 0.0f), glm::vec3(0.0f), 0.02f);
    FluidSphereCollider sphere;
    sphere.radius = 1.0f;
    for (int i = 0; i < 120; ++i) {
        fluid.Step(1.0f / 60.0f, down, {}, {sphere});
    }
    Check(fluid.Particles()[0].position.y >= 1.017f,
          "fluid cannot penetrate an ordinary spherical world surface");
    Check(std::abs(fluid.Particles()[0].velocity.y) < 0.15f,
          "water settles on a curved solid surface");

    const WorldCoordinates near(glm::dvec3(0.0));
    const WorldCoordinates far(glm::dvec3(1.0e9, -2.0e9, 3.0e9));
    const glm::quat turn = glm::angleAxis(0.79f,
        glm::normalize(glm::vec3(-0.4f, 0.2f, 0.7f)));
    const glm::vec3 start(0.13f, 0.82f, -0.17f);
    const glm::vec3 velocity(0.06f, -0.14f, 0.03f);
    FluidWorld first;
    FluidWorld second;
    first.AddParticle(near.ToLocal(near.ToGlobal(start)), velocity, 0.02f);
    second.AddParticle(far.ToLocal(far.ToGlobal(turn * start)), turn * velocity, 0.02f);
    ConstantGravity rotatedDown(turn * down.acceleration);
    const FluidBoxCollider floor = Box(glm::vec3(0), glm::quat(1, 0, 0, 0),
                                       glm::vec3(0), glm::quat(1, 0, 0, 0),
                                       glm::vec3(1.0f, 0.1f, 1.0f));
    const FluidBoxCollider farRotatedFloor = Box(
        far.ToLocal(far.ToGlobal(glm::vec3(0))), turn,
        far.ToLocal(far.ToGlobal(glm::vec3(0))), turn,
        glm::vec3(1.0f, 0.1f, 1.0f));
    for (int i = 0; i < 45; ++i) {
        first.Step(1.0f / 60.0f, down, {floor});
        second.Step(1.0f / 60.0f, rotatedDown, {farRotatedFloor});
    }
    Check(Near(second.Particles()[0].position,
               turn * first.Particles()[0].position, 0.002f),
          "rotating and translating the universe by billions of metres preserves fluid position");
    Check(Near(second.Particles()[0].velocity,
               turn * first.Particles()[0].velocity, 0.002f),
          "rotated far-origin fluid velocity remains locally equivalent");
}

void TestTwoCupPourAndPourBack() {
    std::printf("Two geometric cups, pour and pour back\n");
    ConstantGravity down(glm::vec3(0.0f, -9.81f, 0.0f));
    FluidWorld fluid;
    const glm::vec3 cupA(-0.15f, 0.9f, 0.0f);
    const glm::vec3 cupB(0.4f, 0.0f, 0.0f);
    for (int x = -2; x <= 2; ++x) {
        for (int y = 0; y < 5; ++y) {
            for (int z = -2; z <= 2; ++z) {
                fluid.AddParticle(cupA + glm::vec3(x * 0.04f, 0.05f + y * 0.04f,
                                                   z * 0.04f), glm::vec3(0.0f), 0.064f);
            }
        }
    }
    const float initialMass = fluid.GetDiagnostics().totalMass;
    auto angleA = [](int frame) {
        if (frame < 30) return 0.0f;
        if (frame >= 90) return -2.15f;
        return -2.15f * static_cast<float>(frame - 30) / 60.0f;
    };
    auto poseB = [](int frame) {
        const float lift = frame < 150 ? 0.0f : frame >= 180 ? 0.45f
            : 0.45f * static_cast<float>(frame - 150) / 30.0f;
        const float tilt = frame < 180 ? 0.0f : frame >= 240 ? -2.15f
            : -2.15f * static_cast<float>(frame - 180) / 60.0f;
        return std::pair<float, float>(lift, tilt);
    };
    std::vector<std::size_t> particlesInB;
    bool initiallyContained = false;
    int peakInB = 0;
    for (int frame = 0; frame < 270; ++frame) {
        std::vector<FluidBoxCollider> boxes = OpenCupGeometry(
            cupA, angleA(frame), cupA, angleA(frame + 1));
        const auto previousB = poseB(frame);
        const auto currentB = poseB(frame + 1);
        std::vector<FluidBoxCollider> bBoxes = OpenCupGeometry(
            cupB + glm::vec3(0, previousB.first, 0), previousB.second,
            cupB + glm::vec3(0, currentB.first, 0), currentB.second);
        boxes.insert(boxes.end(), bBoxes.begin(), bBoxes.end());
        fluid.Step(1.0f / 60.0f, down, boxes);

        int inB = 0;
        for (std::size_t i = 0; i < fluid.Particles().size(); ++i) {
            const glm::vec3 relative = fluid.Particles()[i].position - cupB;
            if (std::abs(relative.x) < 0.15f && std::abs(relative.z) < 0.15f &&
                relative.y > 0.03f && relative.y < 0.36f) {
                ++inB;
                if (frame == 149) particlesInB.push_back(i);
            }
        }
        peakInB = std::max(peakInB, inB);
        if (frame == 29) {
            initiallyContained = true;
            for (const FluidParticle& particle : fluid.Particles()) {
                const glm::vec3 relative = particle.position - cupA;
                if (std::abs(relative.x) >= 0.17f || std::abs(relative.z) >= 0.17f ||
                    relative.y < 0.017f || relative.y >= 0.36f) {
                    initiallyContained = false;
                }
            }
        }
    }
    Check(initiallyContained, "water remains inside upright Cup A before tilting");
    Check(peakInB >= 80 && particlesInB.size() >= 80,
          "water crosses Cup A rim, travels through space and enters Cup B");
    int sameWaterLeftB = 0;
    const auto finalB = poseB(270);
    const glm::quat inverseB = glm::inverse(glm::angleAxis(finalB.second,
        glm::vec3(0, 0, 1)));
    for (std::size_t i : particlesInB) {
        const glm::vec3 relative = inverseB *
            (fluid.Particles()[i].position - cupB - glm::vec3(0, finalB.first, 0));
        if (relative.y > 0.4f || std::abs(relative.x) > 0.19f ||
            std::abs(relative.z) > 0.19f) {
            ++sameWaterLeftB;
        }
    }
    Check(sameWaterLeftB >= 70,
          "the very same water particles leave Cup B when its geometry tips");
    Check(std::abs(fluid.GetDiagnostics().totalMass - initialMass) < 1.0e-5f,
          "fluid mass remains constant through both pours");
}

void TestDensityMassAndPresentation() {
    std::printf("Density, mass and presentation\n");
    ConstantGravity zero(glm::vec3(0.0f));
    FluidWorld fluid;
    fluid.AddParticle(glm::vec3(0.0f), glm::vec3(0.0f), 0.46f);
    fluid.AddParticle(glm::vec3(0.01f, 0.0f, 0.0f), glm::vec3(0.0f), 0.46f);
    const float massBefore = fluid.GetDiagnostics().totalMass;
    const glm::vec3 oldPosition = fluid.Particles()[0].position;
    fluid.Step(1.0f / 60.0f, zero, {});
    Check(glm::distance(fluid.Particles()[0].position, fluid.Particles()[1].position) > 0.01f,
          "overdense particles separate under density pressure");
    Check(Near(fluid.PresentedPosition(0, 0.5f),
               0.5f * (oldPosition + fluid.Particles()[0].position), 1.0e-6f),
          "presentation interpolates authoritative particle positions");
    for (int i = 0; i < 120; ++i) fluid.Step(1.0f / 60.0f, zero, {});
    const FluidDiagnostics diagnostics = fluid.GetDiagnostics();
    Check(diagnostics.particleCount == 2 && std::abs(diagnostics.totalMass - massBefore) < 1.0e-6f,
          "particle count and total fluid mass remain exact");
    Check(glm::length(diagnostics.totalMomentum) < 1.0e-4f,
          "symmetric pressure corrections preserve isolated pair momentum");
    Check(std::isfinite(diagnostics.centerOfMass.x) &&
              std::isfinite(diagnostics.kineticEnergy) &&
              std::isfinite(diagnostics.meanDensity) &&
              std::isfinite(diagnostics.maxPositiveDensityError),
          "diagnostics remain finite after pressure relaxation");
    fluid.Clear();
    Check(fluid.Particles().empty() && fluid.GetDiagnostics().totalMass == 0.0f,
          "reset removes authoritative fluid state cleanly");
}

}  // namespace

int main() {
    TestGravityAndZeroGravity();
    TestRotatedUniverse();
    TestRotatedPressureCluster();
    TestStationaryAndMovingBox();
    TestSphereAndFarOrigin();
    TestTwoCupPourAndPourBack();
    TestDensityMassAndPresentation();
    std::printf("FluidWorld: %s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
