#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <stdexcept>

#include <glm/gtc/quaternion.hpp>

#include "AtmosphereField.h"
#include "RadialTerrain.h"
#include "WorldCoordinates.h"

namespace {
int failures = 0;

void Check(bool condition, const char* label) {
    if (condition) {
        std::printf("  OK   %s\n", label);
    } else {
        std::printf("  FAIL %s\n", label);
        ++failures;
    }
}

bool Finite(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool Close(float a, float b, float absoluteTolerance, float relativeTolerance = 0.0f) {
    return std::abs(a - b) <= absoluteTolerance +
           relativeTolerance * std::max(std::abs(a), std::abs(b));
}

glm::vec3 IndependentCross(const glm::vec3& a, const glm::vec3& b) {
    return glm::vec3(a.y * b.z - a.z * b.y,
                     a.z * b.x - a.x * b.z,
                     a.x * b.y - a.y * b.x);
}

void TestProfileAndHydrostatics(const AtmosphereField& gas) {
    std::printf("Section A: pressure, density and hydrostatic profile\n");
    const AtmosphereParameters& p = gas.Parameters();
    const float expectedReferencePressure = (p.polytropicExponent - 1.0f) /
        p.polytropicExponent * p.referenceDensity * p.gravitationalParameter *
        (1.0f / p.referenceRadius - 1.0f / p.topRadius);
    Check(Close(gas.DensityAtRadius(80.0f), 0.05f, 1.0e-7f) &&
          Close(gas.ReferencePressure(), 3.0576623f, 2.0e-5f) &&
          Close(gas.ReferencePressure(), expectedReferencePressure, 2.0e-5f),
          "reference gas has configured mass density and independently known pressure");

    const std::array<float, 5> radii{80.0f, 85.0f, 90.0f, 100.0f, 109.0f};
    bool monotonic = true;
    bool finiteNonnegative = true;
    bool equationOfState = true;
    float previousDensity = 1.0e9f;
    float previousPressure = 1.0e9f;
    const float equationOfStateConstant =
        gas.ReferencePressure() / std::pow(p.referenceDensity, p.polytropicExponent);
    for (float radius : radii) {
        const float density = gas.DensityAtRadius(radius);
        const float pressure = gas.PressureAtRadius(radius);
        monotonic &= density < previousDensity && pressure < previousPressure;
        finiteNonnegative &= std::isfinite(density) && std::isfinite(pressure) &&
                             density > 0.0f && pressure > 0.0f;
        equationOfState &= Close(pressure,
            equationOfStateConstant * std::pow(density, p.polytropicExponent),
            1.0e-7f, 1.0e-4f);
        previousDensity = density;
        previousPressure = pressure;
    }
    Check(monotonic && finiteNonnegative,
          "density and pressure decrease monotonically while remaining finite and positive");
    Check(equationOfState, "pressure and mass density obey P = K rho^gamma");
    Check(Close(gas.DensityAtRadius(90.0f), 0.01351638f, 2.0e-7f) &&
          Close(gas.PressureAtRadius(90.0f), 0.48981975f, 2.0e-5f),
          "10 m altitude matches an independent high-precision profile value");

    // Pressure is a physical gas field: its spatial gradient must balance
    // local gravity, not simply correlate with an altitude-based drag factor.
    float largestRelativeGradientError = 0.0f;
    constexpr float spacing = 0.025f;
    for (float radius : {82.0f, 90.0f, 100.0f, 108.0f}) {
        const float measuredGradient =
            (gas.PressureAtRadius(radius + spacing) -
             gas.PressureAtRadius(radius - spacing)) / (2.0f * spacing);
        const float expectedGradient = -gas.DensityAtRadius(radius) *
            p.gravitationalParameter / (radius * radius);
        const float relativeError = std::abs(measuredGradient - expectedGradient) /
                                    std::abs(expectedGradient);
        largestRelativeGradientError = std::max(largestRelativeGradientError, relativeError);
    }
    std::printf("  maximum |dP/dr + rho*mu/r^2| / |rho*mu/r^2|: %.6f\n",
                largestRelativeGradientError);
    Check(largestRelativeGradientError < 0.003f,
          "finite-difference pressure gradient balances inverse-square gravity within 0.3%");

    bool stable = true;
    const float referenceDensity = gas.DensityAtRadius(90.0f);
    const float referencePressure = gas.PressureAtRadius(90.0f);
    for (int step = 0; step < 600; ++step) {
        stable &= gas.DensityAtRadius(90.0f) == referenceDensity &&
                  gas.PressureAtRadius(90.0f) == referencePressure;
    }
    Check(stable, "prescribed equilibrium profile remains stable for 600 fixed-step samples");
}

void TestVacuumAndTerrain(const AtmosphereField& gas) {
    std::printf("Section B: vacuum and actual terrain exclusion\n");
    const float nearTop = gas.DensityAtRadius(109.9f);
    Check(nearTop > 0.0f && nearTop < gas.DensityAtRadius(109.0f) * 0.01f &&
          gas.PressureAtRadius(109.9f) < gas.PressureAtRadius(109.0f) * 0.001f &&
          gas.DensityAtRadius(110.0f) == 0.0f &&
          gas.PressureAtRadius(110.0f) == 0.0f &&
          gas.DensityAtRadius(111.0f) == 0.0f &&
          gas.PressureAtRadius(111.0f) == 0.0f,
          "density and pressure approach exact vacuum smoothly at the finite top");
    Check(gas.DensityAtRadius(0.0f) == 0.0f &&
          gas.PressureAtRadius(-1.0f) == 0.0f &&
          gas.DensityAtRadius(1.0e6f) == 0.0f,
          "invalid or distant radial samples cannot produce negative or nonfinite gas");

    // A deliberately simple terrain with a 3 m hill and 3 m valley. Both
    // points have the SAME radial gravitational potential, but only the
    // valley point is exposed to gas. The gas profile uses radius, never
    // local height above the undulating solid.
    const RadialTerrain terrain(80.0f,
        [](const glm::vec3& direction) { return 3.0f * direction.x; }, 3.0f);
    const AtmosphereField overTerrain(gas.Parameters(), &terrain);
    const ReferenceFrame frame;
    const AtmosphereSample hillInterior = overTerrain.Sample(glm::vec3(80.0f, 0.0f, 0.0f), frame);
    const AtmosphereSample valleyAir = overTerrain.Sample(glm::vec3(-80.0f, 0.0f, 0.0f), frame);
    const AtmosphereSample aboveHill = overTerrain.Sample(glm::vec3(84.0f, 0.0f, 0.0f), frame);
    Check(hillInterior.density == 0.0f && hillInterior.pressure == 0.0f &&
          valleyAir.density > 0.0f && valleyAir.pressure > 0.0f &&
          aboveHill.density > 0.0f,
          "ordinary hill/valley solid geometry excludes interior gas without a smooth-sphere shortcut");
    Check(Close(valleyAir.density, gas.DensityAtRadius(80.0f), 1.0e-7f),
          "surface relief does not invent nonhydrostatic height-based pressure layers");

    const glm::quat rotation = glm::normalize(glm::angleAxis(
        0.74f, glm::normalize(glm::vec3(1.0f, -2.0f, 3.0f))));
    const ReferenceFrame rotatedFrame{glm::vec3(317.0f, -24.0f, 41.0f), rotation,
                                      glm::vec3(0.0f), glm::vec3(0.0f)};
    const AtmosphereSample rotatedHill = overTerrain.Sample(
        PositionToWorld(rotatedFrame, glm::vec3(80.0f, 0.0f, 0.0f)), rotatedFrame);
    const AtmosphereSample rotatedValley = overTerrain.Sample(
        PositionToWorld(rotatedFrame, glm::vec3(-80.0f, 0.0f, 0.0f)), rotatedFrame);
    Check(rotatedHill.density == 0.0f &&
          Close(rotatedValley.density, valleyAir.density, 1.0e-7f),
          "terrain exclusion follows its rotated, translated physical geometry");
}

void TestFrames(const AtmosphereField& gas) {
    std::printf("Section C: motion, rotation and origin independence\n");
    const glm::vec3 axis = glm::normalize(glm::vec3(1.0f, -2.0f, 3.0f));
    const glm::quat orientation = glm::normalize(glm::angleAxis(0.68f, axis));
    const ReferenceFrame frame{glm::vec3(300.0f, -41.0f, 23.0f), orientation,
                               glm::vec3(42.0f, -7.0f, 13.0f),
                               glm::vec3(0.04f, 0.07f, -0.03f)};
    const glm::vec3 localPoint = glm::normalize(glm::vec3(1.0f, 2.0f, -3.0f)) * 95.0f;
    const glm::vec3 worldPoint = PositionToWorld(frame, localPoint);
    const AtmosphereSample sample = gas.Sample(worldPoint, frame);
    const glm::vec3 independentVelocity = frame.linearVelocity +
        IndependentCross(frame.angularVelocity, worldPoint - frame.originPosition);
    Check(sample.density > 0.0f && sample.pressure > 0.0f &&
          glm::length(sample.velocity - independentVelocity) < 2.0e-5f &&
          Finite(sample.velocity),
          "gas velocity includes planetary translation and omega cross local radius");
    Check(glm::length(sample.velocity - frame.linearVelocity) > 1.0f,
          "the rotating frame point velocity is not mistaken for its centre velocity");

    // Independently transform the complete universe, including the frame's
    // angular velocity. Scalar gas state must agree and vector state rotate.
    const glm::quat universeRotation = glm::normalize(glm::angleAxis(
        0.91f, glm::normalize(glm::vec3(-2.0f, 4.0f, 1.0f))));
    const glm::vec3 translation(-605.0f, 240.0f, 392.0f);
    const ReferenceFrame transformed{
        translation + universeRotation * frame.originPosition,
        universeRotation * frame.orientation,
        universeRotation * frame.linearVelocity,
        universeRotation * frame.angularVelocity};
    const glm::vec3 transformedPoint = translation + universeRotation * worldPoint;
    const AtmosphereSample rotated = gas.Sample(transformedPoint, transformed);
    const float densityError = std::abs(rotated.density - sample.density);
    const float pressureError = std::abs(rotated.pressure - sample.pressure);
    const float velocityError = glm::length(rotated.velocity - universeRotation * sample.velocity);
    std::printf("  rotate+translate errors: rho %.8f kg/m^3, P %.8f Pa, v %.6f m/s\n",
                densityError, pressureError, velocityError);
    Check(densityError < 2.0e-7f && pressureError < 1.0e-5f &&
          velocityError < 1.0e-4f,
          "rotate/translate the universe preserves gas scalar and vector fields");

    // M23's far absolute placement is represented by a double-precision
    // origin, while these local physical coordinates remain unchanged.
    const WorldCoordinates near(glm::dvec3(0.0));
    const WorldCoordinates far(glm::dvec3(1.0e9, -2.0e9, 3.0e9));
    const glm::vec3 nearLocal = near.ToLocal(near.ToGlobal(worldPoint));
    const glm::vec3 farLocal = far.ToLocal(far.ToGlobal(worldPoint));
    const AtmosphereSample nearGas = gas.Sample(nearLocal, frame);
    const AtmosphereSample farGas = gas.Sample(farLocal, frame);
    Check(nearLocal == farLocal && nearGas.density == farGas.density &&
          nearGas.pressure == farGas.pressure && nearGas.velocity == farGas.velocity,
          "M23 far absolute origin changes no local atmosphere state");

    const glm::vec3 vacuumPoint = PositionToWorld(frame, glm::vec3(150.0f, 0.0f, 0.0f));
    const AtmosphereSample vacuum = gas.Sample(vacuumPoint, frame);
    Check(vacuum.density == 0.0f && vacuum.pressure == 0.0f &&
          vacuum.velocity == glm::vec3(0.0f),
          "vacuum has no stale gas state from earlier dense samples");
}

void TestConfigurationAndCost(const AtmosphereField& gas) {
    std::printf("Section D: configuration guard and field sampling cost\n");
    AtmosphereParameters invalid = gas.Parameters();
    invalid.topRadius = invalid.referenceRadius;
    bool rejected = false;
    try {
        const AtmosphereField impossible(invalid);
        (void)impossible;
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Check(rejected, "nonphysical zero-thickness atmosphere is rejected");

    constexpr int samples = 100000;
    const ReferenceFrame frame;
    volatile float checksum = 0.0f;
    const auto start = std::chrono::steady_clock::now();
    for (int index = 0; index < samples; ++index) {
        const float radius = 80.0f + 30.0f * static_cast<float>(index % 997) / 997.0f;
        checksum = checksum + gas.Sample(glm::vec3(radius, 0.0f, 0.0f), frame).density;
    }
    const auto end = std::chrono::steady_clock::now();
    const double elapsedMilliseconds =
        std::chrono::duration<double, std::milli>(end - start).count();
    std::printf("  100000 gas samples: %.3f ms (%.4f us/sample); checksum %.3f\n",
                elapsedMilliseconds, elapsedMilliseconds * 1000.0 / samples, checksum);
    Check(std::isfinite(checksum) && checksum > 0.0f,
          "bounded profile samples remain finite under repeated use");
}
}  // namespace

int main() {
    const AtmosphereField gas(AtmosphereParameters{});
    TestProfileAndHydrostatics(gas);
    TestVacuumAndTerrain(gas);
    TestFrames(gas);
    TestConfigurationAndCost(gas);
    std::printf("Atmosphere tests: %s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
