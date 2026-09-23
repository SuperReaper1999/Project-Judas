#include "CelestialGravity.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <glm/gtc/quaternion.hpp>

namespace {
void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

double Length(const glm::vec3& v) { return static_cast<double>(glm::length(glm::dvec3(v))); }

struct RunResult {
    glm::vec3 center{0.0f};
    glm::vec3 a{0.0f};
    glm::vec3 b{0.0f};
    glm::vec3 va{0.0f};
    glm::vec3 vb{0.0f};
    double radiusMin = 1.0e30;
    double radiusMax = 0.0;
    double maxMomentum = 0.0;
    double maxBarycentreDrift = 0.0;
    double maxEnergyError = 0.0;
    double maxAngularMomentumError = 0.0;
    double measuredPeriod = 0.0;
};

RunResult SimulateOrbit(const glm::quat& frame, float massA, float massB,
                        float separation, float dt, int revolutions) {
    const double totalMass = static_cast<double>(massA) + massB;
    const glm::vec3 axis = glm::normalize(frame * glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::vec3 normal = glm::normalize(frame * glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 tangent = glm::normalize(glm::cross(normal, axis));
    const float relativeSpeed = std::sqrt(CelestialGravity::kGravitationalConstant *
                                          static_cast<float>(totalMass) / separation);
    const glm::vec3 barycentre = frame * glm::vec3(0.0f, 23.0f, 115.0f);
    const float separationF = static_cast<float>(separation);
    const float fractionA = massA / static_cast<float>(totalMass);
    const float fractionB = massB / static_cast<float>(totalMass);
    const glm::vec3 positionA = barycentre - axis * (separationF * fractionB);
    const glm::vec3 positionB = barycentre + axis * (separationF * fractionA);
    const glm::vec3 velocityA = -tangent * (relativeSpeed * fractionB);
    const glm::vec3 velocityB = tangent * (relativeSpeed * fractionA);

    PhysicsWorld world;
    Check(world.Init(), "physics world initialized");
    const BodyHandle a = world.CreateDynamicSphere(positionA, 1.0f, massA, 0.0f, 0.0f);
    const BodyHandle b = world.CreateDynamicSphere(positionB, 1.0f, massB, 0.0f, 0.0f);
    world.SetLinearVelocity(a, velocityA);
    world.SetLinearVelocity(b, velocityB);
    CelestialGravity gravity({a, b});

    const double G = CelestialGravity::kGravitationalConstant;
    const double initialEnergy = 0.5 * (massA * Length(velocityA) * Length(velocityA) +
        massB * Length(velocityB) * Length(velocityB)) - G * massA * massB / separation;
    const glm::dvec3 initialMomentum = glm::dvec3(velocityA) * static_cast<double>(massA) +
                                      glm::dvec3(velocityB) * static_cast<double>(massB);
    const glm::dvec3 initialAngular = glm::cross(glm::dvec3(positionB - positionA),
        glm::dvec3(velocityB - velocityA)) *
        (static_cast<double>(massA) * massB / totalMass);
    const double initialAngularMagnitude = glm::length(initialAngular);
    const double omega = std::sqrt(G * totalMass / (separation * separation * separation));
    const double expectedPeriod = 2.0 * glm::pi<double>() / omega;

    RunResult result;
    result.center = barycentre;
    double accumulatedAngle = 0.0;
    glm::vec3 previousRelative = positionB - positionA;
    const int maxSteps = static_cast<int>(std::ceil(expectedPeriod * revolutions / dt)) + 4;
    for (int step = 0; step < maxSteps; ++step) {
        gravity.ApplyForces(world);
        world.Step(dt);
        const glm::vec3 currentA = world.GetTransform(a).position;
        const glm::vec3 currentB = world.GetTransform(b).position;
        const glm::vec3 currentVa = world.GetLinearVelocity(a);
        const glm::vec3 currentVb = world.GetLinearVelocity(b);
        const glm::vec3 relative = currentB - currentA;
        const double radius = Length(relative);
        result.radiusMin = std::min(result.radiusMin, radius);
        result.radiusMax = std::max(result.radiusMax, radius);
        accumulatedAngle += std::atan2(
            static_cast<double>(glm::dot(normal, glm::cross(previousRelative, relative))),
            static_cast<double>(glm::dot(previousRelative, relative)));
        if (result.measuredPeriod == 0.0 && accumulatedAngle >= 2.0 * glm::pi<double>()) {
            result.measuredPeriod = (step + 1) * static_cast<double>(dt);
        }
        previousRelative = relative;

        const glm::dvec3 momentum = glm::dvec3(currentVa) * static_cast<double>(massA) +
                                    glm::dvec3(currentVb) * static_cast<double>(massB);
        result.maxMomentum = std::max(result.maxMomentum,
            glm::length(momentum - initialMomentum) /
                (static_cast<double>(massA) * Length(velocityA) +
                 static_cast<double>(massB) * Length(velocityB)));
        const glm::dvec3 bary = (glm::dvec3(currentA) * static_cast<double>(massA) +
            glm::dvec3(currentB) * static_cast<double>(massB)) / totalMass;
        result.maxBarycentreDrift = std::max(result.maxBarycentreDrift,
            glm::length(bary - glm::dvec3(barycentre)));
        const double energy = 0.5 * (massA * Length(currentVa) * Length(currentVa) +
            massB * Length(currentVb) * Length(currentVb)) - G * massA * massB / radius;
        result.maxEnergyError = std::max(result.maxEnergyError,
                                         std::abs((energy - initialEnergy) / initialEnergy));
        const glm::dvec3 angular = glm::cross(glm::dvec3(relative),
            glm::dvec3(currentVb - currentVa)) *
            (static_cast<double>(massA) * massB / totalMass);
        result.maxAngularMomentumError = std::max(result.maxAngularMomentumError,
            std::abs(glm::length(angular) - initialAngularMagnitude) / initialAngularMagnitude);
        result.a = currentA;
        result.b = currentB;
        result.va = currentVa;
        result.vb = currentVb;
        if (accumulatedAngle >= 2.0 * glm::pi<double>() * revolutions) break;
    }
    world.Shutdown();
    return result;
}
} // namespace

int main() {
    // Independent inverse-square magnitude/direction check and singular input.
    const glm::vec3 force = CelestialGravity::ForceOnB(
        glm::vec3(0.0f), 2.0e4f, glm::vec3(3.0f, 4.0f, 0.0f), 5.0e4f);
    const double expectedMagnitude = CelestialGravity::kGravitationalConstant * 1.0e9 / 25.0;
    Check(std::abs(Length(force) - expectedMagnitude) < expectedMagnitude * 2.0e-6,
          "Newton force has inverse-square magnitude");
    Check(glm::dot(force, glm::vec3(-3.0f, -4.0f, 0.0f)) > 0.0f,
          "Newton force points toward the attracting body");
    const glm::vec3 finiteForce = CelestialGravity::ForceOnB(
        glm::vec3(0.0f), 1.0f, glm::vec3(1.0f, 0.0f, 0.0f), 1.0f);
    Check(std::isfinite(finiteForce.x) && std::isfinite(finiteForce.y) &&
          std::isfinite(finiteForce.z),
          "finite force at valid separation");
    Check(Length(CelestialGravity::ForceOnB(glm::vec3(0.0f), 1.0f,
              glm::vec3(0.0f), 1.0f)) == 0.0, "coincident point-mass singularity is finite");

    const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    const RunResult orbit = SimulateOrbit(identity, 1.0e14f, 1.0e14f,
                                           30.0f, 1.0f / 60.0f, 5);
    const RunResult halfStepOrbit = SimulateOrbit(identity, 1.0e14f, 1.0e14f,
                                                  30.0f, 1.0f / 120.0f, 5);
    const RunResult unequalOrbit = SimulateOrbit(identity, 1.0e14f, 2.0e14f,
                                                  80.0f, 1.0f / 60.0f, 5);
    constexpr double G = CelestialGravity::kGravitationalConstant;
    constexpr double massA = 1.0e14;
    constexpr double massB = 1.0e14;
    constexpr double separation = 30.0;
    const double expectedPeriod = 2.0 * glm::pi<double>() /
        std::sqrt(G * (massA + massB) / (separation * separation * separation));
    Check(std::abs(orbit.measuredPeriod - expectedPeriod) / expectedPeriod < 0.02,
          "simulated period agrees with analytical two-body period within 2%");
    Check((orbit.radiusMax - orbit.radiusMin) / separation < 0.025,
          "orbit radius remains bounded over five revolutions");
    Check(orbit.maxMomentum < 1.0e-5, "total linear momentum remains bounded");
    Check(orbit.maxBarycentreDrift < 1.0e-3, "shared barycentre drift remains below 1 mm");
    Check(orbit.maxEnergyError < 0.02, "energy error remains below 2% over five revolutions");
    Check(orbit.maxAngularMomentumError < 0.02,
          "angular momentum error remains below 2% over five revolutions");
    Check(halfStepOrbit.maxEnergyError < orbit.maxEnergyError,
          "halving the timestep reduces measured orbital energy error");

    // Unequal mass barycentric distances are inversely proportional to mass.
    const double actualRatio = Length(unequalOrbit.a - unequalOrbit.center) /
                               Length(unequalOrbit.b - unequalOrbit.center);
    Check(std::abs(actualRatio - 2.0) < 0.015,
          "unequal masses move in inverse mass proportion around their barycentre");
    Check(unequalOrbit.maxBarycentreDrift < 1.0e-3,
          "unequal-mass barycentre remains stable within 1 mm");
    Check(Length(unequalOrbit.a - unequalOrbit.center) > 1.0 &&
          Length(unequalOrbit.b - unequalOrbit.center) > 1.0,
          "both massive bodies have nonzero barycentric motion");

    // Rotating the complete initial value problem rotates its result only.
    const glm::quat rotation = glm::angleAxis(1.17f, glm::normalize(glm::vec3(2.0f, -1.0f, 4.0f)));
    const RunResult rotated = SimulateOrbit(rotation, 1.0e14f, 1.0e14f,
                                             30.0f, 1.0f / 60.0f, 5);
    const double rotationPositionError = std::max(
        Length(glm::inverse(rotation) * rotated.a - orbit.a),
        Length(glm::inverse(rotation) * rotated.b - orbit.b));
    const double rotationVelocityError = std::max(
        Length(glm::inverse(rotation) * rotated.va - orbit.va),
        Length(glm::inverse(rotation) * rotated.vb - orbit.vb));
    std::printf("Rotation errors: position %.6g m, velocity %.6g m/s\n",
                rotationPositionError, rotationVelocityError);
    Check(rotationPositionError < 0.02 && rotationVelocityError < 0.01,
          "rotate-the-universe equivalent orbit");

    // A real impulse changes the subsequent trajectory from its current state.
    PhysicsWorld perturbedWorld;
    Check(perturbedWorld.Init(), "perturbation world initialized");
    const glm::vec3 pA(-15.0f, 0.0f, 0.0f), pB(15.0f, 0.0f, 0.0f);
    const float mass = 1.0e14f;
    const BodyHandle a = perturbedWorld.CreateDynamicSphere(pA, 1.0f, mass, 0.0f, 0.0f);
    const BodyHandle b = perturbedWorld.CreateDynamicSphere(pB, 1.0f, mass, 0.0f, 0.0f);
    const float speed = std::sqrt(CelestialGravity::kGravitationalConstant * 2.0f * mass / 30.0f) * 0.5f;
    perturbedWorld.SetLinearVelocity(a, glm::vec3(0.0f, -speed, 0.0f));
    perturbedWorld.SetLinearVelocity(b, glm::vec3(0.0f, speed, 0.0f));
    CelestialGravity perturbGravity({a, b});
    perturbedWorld.ApplyLinearImpulse(a, glm::vec3(0.0f, 0.0f, mass * 5.0f));
    perturbGravity.ApplyForces(perturbedWorld);
    perturbedWorld.Step(1.0f / 60.0f);
    const glm::vec3 perturbedPosition = perturbedWorld.GetTransform(a).position;
    const glm::vec3 perturbedVelocity = perturbedWorld.GetLinearVelocity(a);
    Check(std::isfinite(perturbedPosition.x) && std::isfinite(perturbedPosition.y) &&
          std::isfinite(perturbedPosition.z) && std::isfinite(perturbedVelocity.x) &&
          std::isfinite(perturbedVelocity.y) && std::isfinite(perturbedVelocity.z) &&
          std::abs(perturbedVelocity.z) > 4.9f,
          "physical impulse changes existing orbit trajectory without path reassignment");
    perturbedWorld.Shutdown();

    // An impulse that raises relative specific energy above zero produces
    // an unbound trajectory; nothing restores a prescribed orbit.
    PhysicsWorld escapeWorld;
    Check(escapeWorld.Init(), "escape world initialized");
    const BodyHandle escapeA = escapeWorld.CreateDynamicSphere(pA, 1.0f, mass, 0.0f, 0.0f);
    const BodyHandle escapeB = escapeWorld.CreateDynamicSphere(pB, 1.0f, mass, 0.0f, 0.0f);
    escapeWorld.SetLinearVelocity(escapeA, glm::vec3(0.0f, -speed, 0.0f));
    escapeWorld.SetLinearVelocity(escapeB, glm::vec3(0.0f, speed, 0.0f));
    CelestialGravity escapeGravity({escapeA, escapeB});
    escapeWorld.ApplyLinearImpulse(escapeA, glm::vec3(0.0f, -mass * 20.0f, 0.0f));
    for (int i = 0; i < 600; ++i) {
        escapeGravity.ApplyForces(escapeWorld);
        escapeWorld.Step(1.0f / 60.0f);
    }
    const double escapedSeparation = Length(escapeWorld.GetTransform(escapeB).position -
                                             escapeWorld.GetTransform(escapeA).position);
    Check(escapedSeparation > 200.0, "positive-energy perturbation produces escape");
    escapeWorld.Shutdown();

    std::printf("Orbit metrics: radius %.5f..%.5f m, period %.5f s (analytic %.5f s), "
                "max momentum error %.3g, barycentre drift %.6g m, energy %.3g, angular momentum %.3g; "
                "half-step energy %.3g\n",
                orbit.radiusMin, orbit.radiusMax, orbit.measuredPeriod, expectedPeriod,
                orbit.maxMomentum, orbit.maxBarycentreDrift, orbit.maxEnergyError,
                orbit.maxAngularMomentumError, halfStepOrbit.maxEnergyError);
    std::printf("PASS: celestial gravity force, barycentre, orbit, conservation, perturbation, rotation.\n");
    return 0;
}
