#include "RigidBody.h"
#include "PhysicsWorld.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>

glm::vec3 LegacyTransportAngularVelocity(const glm::mat3&, const glm::quat&,
                                         const glm::quat&, const glm::vec3&);
glm::vec3 LegacyAngularDriftGenerator(const glm::mat3&, const glm::quat&,
                                      const glm::vec3&, float);

namespace {
int checks = 0, failures = 0;
void Check(bool value, const std::string& label) {
    ++checks;
    if (!value) { if (failures < 30) std::cerr << "FAIL " << label << '\n'; ++failures; }
}
bool Bits(glm::vec3 a, glm::vec3 b) {
    return std::memcmp(&a.x, &b.x, sizeof(float)) == 0 &&
           std::memcmp(&a.y, &b.y, sizeof(float)) == 0 &&
           std::memcmp(&a.z, &b.z, sizeof(float)) == 0;
}
glm::dvec3 Momentum(const glm::mat3& inverse, const glm::quat& pose, const glm::vec3& omega) {
    const auto rotation = glm::mat3_cast(glm::normalize(glm::dquat(pose)));
    return rotation * glm::inverse(glm::dmat3(inverse)) * glm::transpose(rotation) * glm::dvec3(omega);
}
double Energy(const glm::mat3& inverse, const glm::quat& pose, const glm::vec3& omega) {
    return .5 * glm::dot(Momentum(inverse, pose, omega), glm::dvec3(omega));
}
struct Result { glm::vec3 value{0}; std::string error; };
template<class Function> Result Observe(Function&& function) {
    Result result;
    try { result.value = function(); }
    catch (const std::exception& error) { result.error = error.what(); }
    return result;
}
bool SameResult(const Result& a, const Result& b) {
    if (!a.error.empty() || !b.error.empty()) return a.error == b.error;
    for (int i = 0; i < 3; ++i)
        if (std::isnan(a.value[i]) != std::isnan(b.value[i]) ||
            (!std::isnan(a.value[i]) && std::memcmp(&a.value[i], &b.value[i], sizeof(float)))) return false;
    return true;
}
}

int main() {
    std::mt19937 random(773311);
    std::uniform_real_distribution<float> angle(-3, 3), spin(-100, 100);
    double largestLegacyDriftDifference = 0, largestLegacyTransportDifference = 0;
    unsigned legacyDriftDifferent = 0, legacyTransportDifferent = 0;
    for (int sample = 0; sample < 256; ++sample) {
        auto rotation = glm::angleAxis(angle(random), glm::normalize(glm::vec3(1, 2, -3)));
        if (sample % 9 == 0) rotation *= .25f; // The legacy helpers normalize valid nonunit input.
        const auto next = glm::angleAxis(angle(random), glm::normalize(glm::vec3(-2, 3, 1)));
        glm::vec3 omega(spin(random), spin(random), spin(random));
        if (sample % 4 == 1) omega = {0, spin(random), 0};
        if (sample % 4 == 2) omega.z = 0;
        if (sample % 4 == 3) omega.x = 1e-30f;
        const float duration = sample % 2 ? 1.f / 60 : .35f;
        const float scalar = sample % 4 == 0 ? .0001f : sample % 4 == 1 ? .5f : sample % 4 == 2 ? 16.f : 1000000.f;
        const glm::mat3 inverse(scalar);
        const auto drift = AngularDriftGenerator(inverse, rotation, omega, duration);
        const auto transported = TransportAngularVelocity(inverse, rotation, next, omega);
        Check(Bits(drift, omega), "exact isotropic drift retains world angular velocity bits");
        Check(Bits(transported, omega), "exact isotropic transport retains world angular velocity bits");
        const auto oldDrift = LegacyAngularDriftGenerator(inverse, rotation, omega, duration);
        const auto oldTransport = LegacyTransportAngularVelocity(inverse, rotation, next, omega);
        legacyDriftDifferent += !Bits(oldDrift, drift);
        legacyTransportDifferent += !Bits(oldTransport, transported);
        largestLegacyDriftDifference = std::max(largestLegacyDriftDifference,
                                               glm::length(glm::dvec3(oldDrift) - glm::dvec3(drift)));
        largestLegacyTransportDifference = std::max(largestLegacyTransportDifference,
                                                   glm::length(glm::dvec3(oldTransport) - glm::dvec3(transported)));
        const auto before = Momentum(inverse, rotation, omega), after = Momentum(inverse, next, transported);
        Check(glm::length(after - before) <= 1e-12 * std::max(1., glm::length(before)),
              "isotropic transport preserves independent world angular momentum");
        Check(std::abs(Energy(inverse, next, transported) - Energy(inverse, rotation, omega)) <=
                  1e-12 * std::max(1., std::abs(Energy(inverse, rotation, omega))),
              "isotropic transport preserves independent kinetic energy");

        // These must use the unchanged tensor path, including a one-ULP
        // diagonal perturbation and the smallest nonzero off-diagonal term.
        for (int kind = 0; kind < 4; ++kind) {
            glm::mat3 tensor(inverse);
            if (kind == 0) tensor[2][2] = std::nextafter(scalar, std::numeric_limits<float>::infinity());
            if (kind == 1) tensor[0][1] = tensor[1][0] = std::numeric_limits<float>::denorm_min();
            if (kind == 2) tensor = SolidBoxInverseInertia(3, {.2f, .5f, .8f});
            if (kind == 3) {
                const auto matrix = glm::mat3_cast(next);
                tensor = matrix * SolidBoxInverseInertia(2, {.3f, .4f, .7f}) * glm::transpose(matrix);
            }
            Check(SameResult(Observe([&] { return AngularDriftGenerator(tensor, rotation, omega, duration); }),
                             Observe([&] { return LegacyAngularDriftGenerator(tensor, rotation, omega, duration); })),
                  "near-isotropic and anisotropic drift remain bitwise legacy");
            Check(SameResult(Observe([&] { return TransportAngularVelocity(tensor, rotation, next, omega); }),
                             Observe([&] { return LegacyTransportAngularVelocity(tensor, rotation, next, omega); })),
                  "near-isotropic and anisotropic transport remain bitwise legacy");
        }
    }

    const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
    for (const glm::quat invalid : {glm::quat(0, 0, 0, 0), glm::quat(nan, 0, 0, 0), glm::quat(inf, 1, 0, 0)}) {
        Check(SameResult(Observe([&] { return AngularDriftGenerator(glm::mat3(1), invalid, {1, 2, 3}, .02f); }),
                         Observe([&] { return LegacyAngularDriftGenerator(glm::mat3(1), invalid, {1, 2, 3}, .02f); })),
              "invalid quaternion drift retains original failure/result");
        Check(SameResult(Observe([&] { return TransportAngularVelocity(glm::mat3(1), invalid, glm::quat(1, 0, 0, 0), {1, 2, 3}); }),
                         Observe([&] { return LegacyTransportAngularVelocity(glm::mat3(1), invalid, glm::quat(1, 0, 0, 0), {1, 2, 3}); })),
              "invalid quaternion transport retains original failure/result");
    }
    for (float invalid : {nan, inf})
        Check(SameResult(Observe([&] { return AngularDriftGenerator(glm::mat3(1), glm::quat(1, 0, 0, 0), {1, 2, 3}, invalid); }),
                         Observe([&] { return LegacyAngularDriftGenerator(glm::mat3(1), glm::quat(1, 0, 0, 0), {1, 2, 3}, invalid); })),
              "invalid duration retains original failure/result");

    double maxMomentumRelative = 0, maxEnergyRelative = 0;
    for (bool sphere : {false, true}) for (const glm::vec3 spinAxis :
            {glm::vec3(2, -3, 1), glm::vec3(-31, 67, 11), glm::vec3(0, 0, .125f)}) {
        PhysicsWorld world; world.Init();
        const auto body = sphere ? world.CreateDynamicSphere({2, 3, -4}, .3f, 2, .5f, 0)
                                 : world.CreateDynamicBox({2, 3, -4}, {.3f, .3f, .3f}, 2, .5f, 0);
        const glm::mat3 inverse = sphere ? SolidSphereInverseInertia(2, .3f)
                                        : SolidBoxInverseInertia(2, {.3f, .3f, .3f});
        world.SetAngularVelocity(body, spinAxis);
        const auto firstPose = world.GetTransform(body).rotation;
        const auto initialMomentum = Momentum(inverse, firstPose, spinAxis);
        const auto initialEnergy = Energy(inverse, firstPose, spinAxis);
        for (int step = 0; step < 256; ++step) {
            world.Step(1.f / 60);
            const auto actual = world.GetAngularVelocity(body);
            const auto pose = world.GetTransform(body).rotation;
            Check(Bits(actual, spinAxis), "actual PhysicsWorld exact-isotropic free spin remains constant");
            const double momentum = glm::length(Momentum(inverse, pose, actual) - initialMomentum) /
                                     std::max(1., glm::length(initialMomentum));
            const double energy = std::abs(Energy(inverse, pose, actual) - initialEnergy) /
                                   std::max(1., std::abs(initialEnergy));
            maxMomentumRelative = std::max(maxMomentumRelative, momentum);
            maxEnergyRelative = std::max(maxEnergyRelative, energy);
            Check(momentum < 2e-12 && energy < 2e-12, "actual isotropic world preserves closed angular invariants");
        }
    }
    std::cout << std::setprecision(17)
              << "{\"checks\":" << checks << ",\"failures\":" << failures
              << ",\"legacyDriftDifferentCases\":" << legacyDriftDifferent
              << ",\"legacyTransportDifferentCases\":" << legacyTransportDifferent
              << ",\"largestLegacyDriftDifference\":" << largestLegacyDriftDifference
              << ",\"largestLegacyTransportDifference\":" << largestLegacyTransportDifference
              << ",\"actualWorldMaximumRelativeMomentumError\":" << maxMomentumRelative
              << ",\"actualWorldMaximumRelativeEnergyError\":" << maxEnergyRelative << "}\n";
    return failures ? 1 : 0;
}
