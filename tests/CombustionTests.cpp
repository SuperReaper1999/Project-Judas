#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

#include <glm/gtc/quaternion.hpp>

#include "AtmosphereField.h"
#include "CelestialGravity.h"
#include "CombustionWorld.h"
#include "FirePresentation.h"
#include "FlyingPrimitiveControl.h"
#include "PhysicsWorld.h"
#include "RadialTerrain.h"
#include "RadicalGravity.h"
#include "ReferenceFrame.h"
#include "TerrainDemo.h"
#include "Window.h"
#include "WorldCoordinates.h"

namespace {
constexpr float kDt = 1.0f / 60.0f;
int failures = 0;

void Check(bool condition, const char* label) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", label);
    if (!condition) ++failures;
}

bool Near(float actual, float expected, float tolerance) {
    return std::isfinite(actual) && std::abs(actual - expected) <= tolerance;
}

bool Near(const glm::vec3& actual, const glm::vec3& expected, float tolerance) {
    return glm::length(actual - expected) <= tolerance;
}

bool Finite(const ThermalBodyState& state) {
    return std::isfinite(state.temperatureKelvin) &&
           std::isfinite(state.remainingFuelMassKg) &&
           std::isfinite(state.burnRateKgPerSecond) &&
           std::isfinite(state.heatOutputWatts) &&
           std::isfinite(state.oxygenSupplyKgPerSecond);
}

CombustibleMaterial IsolatedFuel() {
    CombustibleMaterial material;
    material.heatCapacityJPerK = 40.0f;
    material.initialFuelMassKg = 0.08f;
    material.ignitionTemperatureK = 400.0f;
    material.activationRangeK = 20.0f;
    material.maximumFuelRateKgPerSecond = 0.02f;
    material.heatReleaseJPerKg = 200000.0f;
    material.oxygenRequiredKgPerKgFuel = 1.0f;
    material.oxygenTransportSpeedMetersPerSecond = 100.0f;
    material.airflowTransportFactor = 0.2f;
    material.radiativeAreaSquareMeters = 0.5f;
    material.emissivity = 0.0f;
    material.convectionCoefficientWattsPerSquareMeterKelvin = 0.0f;
    return material;
}

void TestAtmosphereThermalAndOxidizerState() {
    std::printf("Section A: sampled gas composition and thermal equation of state\n");
    const AtmosphereParameters parameters;
    const AtmosphereField gas(parameters);
    const ReferenceFrame frame;
    bool correctMixture = true;
    bool correctEquationOfState = true;
    for (float radius : {80.0f, 90.0f, 100.0f}) {
        const AtmosphereSample sample = gas.Sample({radius, 0.0f, 0.0f}, frame);
        correctMixture &= sample.oxidizerMassDensity > 0.0f &&
                          Near(sample.oxidizerMassDensity,
                               sample.density * parameters.oxidizerMassFraction, 2.0e-7f);
        correctEquationOfState &= sample.temperatureKelvin > 0.0f &&
            Near(sample.pressure,
                 sample.density * gas.SpecificGasConstant() * sample.temperatureKelvin,
                 2.0e-5f);
    }
    Check(correctMixture, "oxidizer is explicit mass density derived from the gas mixture");
    Check(correctEquationOfState,
          "sampled gas temperature, density and pressure satisfy P = rho R T");
    const AtmosphereSample vacuum = gas.Sample({110.0f, 0.0f, 0.0f}, frame);
    Check(vacuum.density == 0.0f && vacuum.pressure == 0.0f &&
          vacuum.oxidizerMassDensity == 0.0f && vacuum.temperatureKelvin == 0.0f,
          "pressure, density, oxidizer and gas temperature all vanish at the finite vacuum limit");
}

struct Fixture {
    PhysicsWorld physics;
    AtmosphereField atmosphere;
    ReferenceFrame frame;
    CombustionWorld combustion;

    explicit Fixture(const AtmosphereParameters& parameters = AtmosphereParameters{})
        : atmosphere(parameters) {
        Check(physics.Init(), "physics world initialized");
    }
    ~Fixture() { physics.Shutdown(); }

    BodyHandle AddSphere(const glm::vec3& position, const CombustibleMaterial& material,
                         float temperatureKelvin, float radius = 0.2f) {
        const BodyHandle handle = physics.CreateDynamicSphere(position, radius, 2.0f, 0.4f, 0.0f);
        combustion.AddBody(handle, material, temperatureKelvin);
        return handle;
    }

    void ThermalStep(const RadiantHeater* heater = nullptr) {
        combustion.Step(kDt, physics, atmosphere, frame, heater);
    }
};

void TestIgnitionAndFiniteFuel() {
    std::printf("Section B: thermal ignition, fuel and independent energy budget\n");
    {
        Fixture fixture;
        const CombustibleMaterial fuel = IsolatedFuel();
        const BodyHandle cold = fixture.AddSphere({82.0f, 0.0f, 0.0f}, fuel, 300.0f);
        CombustibleMaterial inert = fuel;
        inert.initialFuelMassKg = 0.0f;
        const BodyHandle hotInert = fixture.AddSphere({82.0f, 6.0f, 0.0f}, inert, 700.0f);
        for (int step = 0; step < 240; ++step) fixture.ThermalStep();
        const ThermalBodyState& a = *fixture.combustion.State(cold);
        const ThermalBodyState& b = *fixture.combustion.State(hotInert);
        Check(Finite(a) && Near(a.remainingFuelMassKg, fuel.initialFuelMassKg, 1.0e-6f) &&
              a.burnRateKgPerSecond == 0.0f,
              "cold combustible material remains unburned in oxidizer-rich gas");
        Check(Finite(b) && b.remainingFuelMassKg == 0.0f &&
              b.burnRateKgPerSecond == 0.0f,
              "heat without combustible fuel cannot create fire");
    }
    {
        Fixture fixture;
        const CombustibleMaterial fuel = IsolatedFuel();
        const BodyHandle body = fixture.AddSphere({82.0f, 0.0f, 0.0f}, fuel, 300.0f);
        const RadiantHeater heater{{82.0f, 0.5f, 0.0f}, 20000.0f};
        int firstBurnStep = -1;
        float ignitionTemperature = 0.0f;
        float absorbedHeaterWatts = 0.0f;
        for (int step = 0; step < 240; ++step) {
            fixture.ThermalStep(&heater);
            const ThermalBodyState& state = *fixture.combustion.State(body);
            absorbedHeaterWatts = std::max(absorbedHeaterWatts, state.heaterWattsReceived);
            if (state.burnRateKgPerSecond > 0.0f) {
                firstBurnStep = step;
                ignitionTemperature = state.temperatureKelvin;
                break;
            }
        }
        std::printf("  heater ignition: step %d, T %.3f K, absorbed %.3f W\n",
                    firstBurnStep, ignitionTemperature, absorbedHeaterWatts);
        Check(absorbedHeaterWatts > 0.0f && firstBurnStep > 0 && firstBurnStep < 240 &&
              ignitionTemperature >= fuel.ignitionTemperatureK - 1.0f,
              "a nearby powered radiator heats fuel across its ignition condition");
    }
    {
        Fixture fixture;
        const CombustibleMaterial fuel = IsolatedFuel();
        const float initialTemperature = 450.0f;
        const BodyHandle body = fixture.AddSphere({82.0f, 0.0f, 0.0f}, fuel,
                                                  initialTemperature);
        float integratedHeatJoules = 0.0f;
        float lastFuel = fuel.initialFuelMassKg;
        bool monotonicFuel = true;
        bool finiteStates = true;
        int exhaustedAtStep = -1;
        for (int step = 0; step < 600; ++step) {
            fixture.ThermalStep();
            const ThermalBodyState& state = *fixture.combustion.State(body);
            finiteStates &= Finite(state);
            monotonicFuel &= state.remainingFuelMassKg <= lastFuel + 1.0e-7f &&
                             state.remainingFuelMassKg >= -1.0e-7f;
            lastFuel = state.remainingFuelMassKg;
            integratedHeatJoules += state.heatOutputWatts * kDt;
            if (state.remainingFuelMassKg <= 1.0e-6f && exhaustedAtStep < 0) {
                exhaustedAtStep = step;
            }
        }
        const ThermalBodyState& final = *fixture.combustion.State(body);
        const float burntMass = fuel.initialFuelMassKg - final.remainingFuelMassKg;
        const float expectedChemicalHeat = burntMass * fuel.heatReleaseJPerKg;
        const float sensibleHeatIncrease = fuel.heatCapacityJPerK *
            (final.temperatureKelvin - initialTemperature);
        std::printf("  burnout: step %d, fuel %.6f kg, T %.3f K, integrated heat %.3f J, "
                    "chemical %.3f J, sensible %.3f J\n",
                    exhaustedAtStep, final.remainingFuelMassKg, final.temperatureKelvin,
                    integratedHeatJoules, expectedChemicalHeat, sensibleHeatIncrease);
        Check(finiteStates && monotonicFuel && exhaustedAtStep >= 0 &&
              final.burnRateKgPerSecond == 0.0f && final.heatOutputWatts == 0.0f,
              "fuel decreases monotonically to exhaustion, then burning and heat release cease");
        Check(std::abs(integratedHeatJoules - expectedChemicalHeat) < 10.0f &&
              std::abs(sensibleHeatIncrease - expectedChemicalHeat) < 10.0f,
              "integrated reaction heat and temperature gain independently match consumed fuel energy");
    }
    {
        Fixture fixture;
        CombustibleMaterial fuel = IsolatedFuel();
        fuel.retainedCombustionHeatFraction = 0.75f;
        const BodyHandle body = fixture.AddSphere({82.0f, 0.0f, 0.0f}, fuel, 450.0f);
        fixture.ThermalStep();
        const ThermalBodyState& state = *fixture.combustion.State(body);
        const float chemicalJoules =
            (fuel.initialFuelMassKg - state.remainingFuelMassKg) * fuel.heatReleaseJPerKg;
        const float retainedSensibleJoules = fuel.heatCapacityJPerK *
            (state.temperatureKelvin - 450.0f);
        const float gasOutputJoules = state.heatToGasWatts * kDt;
        Check(chemicalJoules > 0.0f &&
              Near(retainedSensibleJoules, chemicalJoules * 0.75f, 0.01f) &&
              Near(gasOutputJoules, chemicalJoules * 0.25f, 0.01f) &&
              Near(retainedSensibleJoules + gasOutputJoules, chemicalJoules, 0.01f),
              "material and gas heat outputs partition consumed chemical energy without loss");
        const float previous = state.previousTemperatureKelvin;
        const float current = state.temperatureKelvin;
        Check(Near(fixture.combustion.PresentedTemperature(body, 0.0f), previous, 1.0e-6f) &&
              Near(fixture.combustion.PresentedTemperature(body, 0.5f),
                   0.5f * (previous + current), 1.0e-5f) &&
              Near(fixture.combustion.PresentedTemperature(body, 1.0f), current, 1.0e-6f) &&
              Near(fixture.combustion.State(body)->temperatureKelvin, current, 1.0e-6f),
              "presented temperature interpolates without changing authoritative thermal state");
    }
}

struct SpreadResult {
    int firstIgnitionStep = -1;
    float targetTemperature = 0.0f;
    float peakPairwiseWatts = 0.0f;
    float sourceFuelLeft = 0.0f;
};

SpreadResult RunSpread(float separation) {
    Fixture fixture;
    CombustibleMaterial source = IsolatedFuel();
    source.heatCapacityJPerK = 500.0f;
    source.initialFuelMassKg = 1.0f;
    source.ignitionTemperatureK = 340.0f;
    source.maximumFuelRateKgPerSecond = 0.02f;
    source.heatReleaseJPerKg = 1000000.0f;
    source.radiativeAreaSquareMeters = 1.0f;
    source.emissivity = 0.9f;
    const BodyHandle hot = fixture.AddSphere({82.0f, 0.0f, 0.0f}, source, 650.0f);
    CombustibleMaterial target = source;
    target.heatCapacityJPerK = 50.0f;
    target.initialFuelMassKg = 0.3f;
    target.maximumFuelRateKgPerSecond = 0.01f;
    const BodyHandle neighbour = fixture.AddSphere({82.0f, separation, 0.0f}, target, 300.0f);
    SpreadResult result;
    for (int step = 0; step < 1800; ++step) {
        fixture.ThermalStep();
        const ThermalBodyState& other = *fixture.combustion.State(neighbour);
        result.peakPairwiseWatts = std::max(result.peakPairwiseWatts,
                                            other.pairwiseHeatWattsReceived);
        if (other.burnRateKgPerSecond > 0.0f && result.firstIgnitionStep < 0) {
            result.firstIgnitionStep = step;
        }
    }
    result.targetTemperature = fixture.combustion.State(neighbour)->temperatureKelvin;
    result.sourceFuelLeft = fixture.combustion.State(hot)->remainingFuelMassKg;
    return result;
}

void TestThermalSpread() {
    std::printf("Section C: heat-transfer spread versus separation\n");
    const SpreadResult near = RunSpread(0.7f);
    const SpreadResult far = RunSpread(20.0f);
    std::printf("  near ignition %.3f s, far ignition step %d; peak neighbour heat "
                "near %.3f W, far %.3f W; final neighbour T %.3f / %.3f K\n",
                near.firstIgnitionStep * kDt, far.firstIgnitionStep,
                near.peakPairwiseWatts, far.peakPairwiseWatts,
                near.targetTemperature, far.targetTemperature);
    Check(near.firstIgnitionStep > 0 && near.firstIgnitionStep < 1800 &&
          near.peakPairwiseWatts > 0.0f,
          "nearby fuel heats and later ignites from actual radiative transfer");
    Check(far.firstIgnitionStep < 0 &&
          far.peakPairwiseWatts < near.peakPairwiseWatts * 0.1f,
          "a substantially separated body does not ignite in the same duration");
}

struct AuthoredSpreadResult {
    int heaterOffStep = -1;
    int firstIgnitionStepA = -1;
    int firstIgnitionStepB = -1;
    int firstIgnitionStepFar = -1;
    int fuelExhaustionStepA = -1;
    float initialOxidizerDensityA = 0.0f;
    float peakPairwiseHeatAtB = 0.0f;
    float peakTemperatureB = 0.0f;
    float finalTemperatureA = 0.0f;
    float finalTemperatureB = 0.0f;
    float finalTemperatureFar = 0.0f;
    float finalFuelA = 0.0f;
    float finalFuelB = 0.0f;
};

AuthoredSpreadResult RunAuthoredTerrainSpread(const glm::quat& universeRotation,
                                              const glm::vec3& translation,
                                              bool suppressSourceRadiation = false,
                                              int fixedHeaterOffStep = -1) {
    // Rebuild the actual M25 terrain and M27 spawn geometry. The authored
    // planet centre is a scene placement, not a physical universal origin.
    const auto terrain = TerrainDemo::CreateSurface();
    const AtmosphereField atmosphere(AtmosphereParameters{}, terrain.get());
    const glm::vec3 planetCentre = translation +
        universeRotation * glm::vec3(300.0f, 0.0f, 0.0f);
    const ReferenceFrame planetFrame{planetCentre, universeRotation,
                                      glm::vec3(0.0f), glm::vec3(0.0f)};
    const glm::vec3 localShipSurface =
        TerrainDemo::LocalPointAbove(*terrain, -3.0f, -3.0f, 0.5f);
    const glm::vec3 shipPosition = planetCentre + universeRotation * localShipSurface;
    const glm::vec3 localNormal = terrain->Sample(localShipSurface).outwardNormal;
    const glm::quat shipRotation = universeRotation *
        glm::quat(glm::vec3(0.0f, 1.0f, 0.0f), localNormal);
    PhysicsWorld physics;
    Check(physics.Init(), "authored terrain-spread physics world initialized");
    physics.CreateStaticTerrain(planetCentre, universeRotation, terrain, 0.8f, 0.0f);

    CombustibleMaterial fuel;
    fuel.heatCapacityJPerK = 150.0f;
    fuel.initialFuelMassKg = 0.12f;
    fuel.ignitionTemperatureK = 550.0f;
    fuel.maximumFuelRateKgPerSecond = 0.003f;
    fuel.radiativeAreaSquareMeters = 1.5f;
    fuel.retainedCombustionHeatFraction = 0.75f;
    const std::array<glm::vec3, 3> shipLocalOffsets{{
        {-1.1f, 0.53f, -1.2f}, {-1.1f, 0.53f, -0.65f}, {1.4f, 0.53f, 2.2f}}};
    std::array<BodyHandle, 3> blocks;
    std::array<glm::vec3, 3> positions;
    CombustionWorld combustion;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        positions[i] = shipPosition + shipRotation * shipLocalOffsets[i];
        blocks[i] = physics.CreateDynamicBox(
            positions[i], {0.25f, 0.25f, 0.25f}, 5.0f, 0.6f, 0.15f);
        physics.ResetBody(blocks[i], positions[i], shipRotation);
        const AtmosphereSample initialGas = atmosphere.Sample(positions[i], planetFrame);
        Check(initialGas.oxidizerMassDensity > 0.0f &&
              initialGas.temperatureKelvin > 0.0f,
              "authored combustible block starts in physical terrain atmosphere");
        CombustibleMaterial material = fuel;
        if (i == 0 && suppressSourceRadiation) material.emissivity = 0.0f;
        combustion.AddBody(blocks[i], material, initialGas.temperatureKelvin);
    }
    // Application creates the blocks before the ship so M18 pickup can
    // include them; preserve that body insertion order even in this
    // stationary thermal fixture.
    physics.CreateDynamicBox(shipPosition, {2.0f, 0.25f, 3.0f}, 80.0f, 0.8f, 0.0f);

    // The source is 0.35 m behind A along ship-local -Z, away from B.
    // It is isotropic and heats every registered object by geometric
    // interception, so this test still rules out target-directed ignition.
    const RadiantHeater heater{
        positions[0] + shipRotation * glm::vec3(0.0f, 0.0f, -0.35f), 18000.0f};
    AuthoredSpreadResult result;
    result.initialOxidizerDensityA =
        atmosphere.Sample(positions[0], planetFrame).oxidizerMassDensity;
    for (int step = 0; step < 3600; ++step) {
        const bool heaterOn = fixedHeaterOffStep >= 0
            ? step <= fixedHeaterOffStep : result.heaterOffStep < 0;
        combustion.Step(kDt, physics, atmosphere, planetFrame,
                        heaterOn ? &heater : nullptr);
        const ThermalBodyState& a = *combustion.State(blocks[0]);
        const ThermalBodyState& b = *combustion.State(blocks[1]);
        const ThermalBodyState& far = *combustion.State(blocks[2]);
        if (fixedHeaterOffStep >= 0 && step == fixedHeaterOffStep) {
            result.heaterOffStep = step;
        } else if (fixedHeaterOffStep < 0 &&
                   result.heaterOffStep < 0 && a.temperatureKelvin >= 700.0f) {
            result.heaterOffStep = step;
        }
        if (result.firstIgnitionStepA < 0 && a.burnRateKgPerSecond > 0.0f)
            result.firstIgnitionStepA = step;
        if (result.firstIgnitionStepB < 0 && b.burnRateKgPerSecond > 0.0f)
            result.firstIgnitionStepB = step;
        if (result.firstIgnitionStepFar < 0 && far.burnRateKgPerSecond > 0.0f)
            result.firstIgnitionStepFar = step;
        if (result.fuelExhaustionStepA < 0 && a.remainingFuelMassKg <= 1.0e-6f)
            result.fuelExhaustionStepA = step;
        result.peakPairwiseHeatAtB = std::max(result.peakPairwiseHeatAtB,
                                              b.pairwiseHeatWattsReceived);
        result.peakTemperatureB = std::max(result.peakTemperatureB,
                                            b.temperatureKelvin);
    }
    result.finalTemperatureA = combustion.State(blocks[0])->temperatureKelvin;
    result.finalTemperatureB = combustion.State(blocks[1])->temperatureKelvin;
    result.finalTemperatureFar = combustion.State(blocks[2])->temperatureKelvin;
    result.finalFuelA = combustion.State(blocks[0])->remainingFuelMassKg;
    result.finalFuelB = combustion.State(blocks[1])->remainingFuelMassKg;
    physics.Shutdown();
    return result;
}

void TestAuthoredTerrainSpread() {
    std::printf("Section D: exact terrain demo spread after the heater turns off\n");
    const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    const AuthoredSpreadResult ordinary =
        RunAuthoredTerrainSpread(identity, glm::vec3(0.0f));
    const AuthoredSpreadResult withoutTransfer =
        RunAuthoredTerrainSpread(identity, glm::vec3(0.0f), true,
                                 ordinary.heaterOffStep);
    const glm::quat rotation = glm::normalize(glm::angleAxis(
        glm::radians(47.0f), glm::normalize(glm::vec3(1.0f, 0.3f, 2.0f))));
    const AuthoredSpreadResult rotated = RunAuthoredTerrainSpread(
        rotation, glm::vec3(-500.0f, 270.0f, 180.0f));
    std::printf("  authored heater off %.3f s, A ignition %.3f s, B ignition %.3f s, "
                "A exhaustion %.3f s, far ignition %d, initial rhoO2 %.6f kg/m^3, "
                "peak B transfer %.3f W\n",
                ordinary.heaterOffStep * kDt,
                ordinary.firstIgnitionStepA * kDt,
                ordinary.firstIgnitionStepB * kDt,
                ordinary.fuelExhaustionStepA * kDt,
                ordinary.firstIgnitionStepFar,
                ordinary.initialOxidizerDensityA,
                ordinary.peakPairwiseHeatAtB);
    std::printf("  no-A-radiation control: heater off %.3f s, B ignition step %d, "
                "B peak/final T %.3f/%.3f K, peak B transfer %.6f W\n",
                withoutTransfer.heaterOffStep * kDt,
                withoutTransfer.firstIgnitionStepB,
                withoutTransfer.peakTemperatureB,
                withoutTransfer.finalTemperatureB,
                withoutTransfer.peakPairwiseHeatAtB);
    Check(ordinary.heaterOffStep >= 0 && ordinary.heaterOffStep < 450 &&
          ordinary.firstIgnitionStepA >= 0 &&
          ordinary.firstIgnitionStepB > ordinary.heaterOffStep &&
          ordinary.firstIgnitionStepB >= 600 &&
          ordinary.firstIgnitionStepB <= 1500 &&
          ordinary.peakPairwiseHeatAtB > 0.0f,
          "exact authored B ignites 10-25 s after start from A's transferred heat, after heater is off");
    Check(ordinary.firstIgnitionStepFar < 0 &&
          ordinary.fuelExhaustionStepA > ordinary.firstIgnitionStepB &&
          ordinary.fuelExhaustionStepA < 3600 &&
          ordinary.finalFuelA <= 1.0e-6f &&
          ordinary.finalFuelB < 0.12f,
          "distant demo block stays cold while finite A fuel is consumed in 60 seconds");
    Check(withoutTransfer.heaterOffStep == ordinary.heaterOffStep &&
          withoutTransfer.firstIgnitionStepA >= 0 &&
          withoutTransfer.firstIgnitionStepB < 0 &&
          withoutTransfer.peakTemperatureB < 550.0f &&
          withoutTransfer.peakPairwiseHeatAtB < ordinary.peakPairwiseHeatAtB * 0.001f,
          "with A's radiation removed, the same finite heater exposure cannot ignite B");
    Check(std::abs(rotated.firstIgnitionStepA - ordinary.firstIgnitionStepA) <= 1 &&
          std::abs(rotated.firstIgnitionStepB - ordinary.firstIgnitionStepB) <= 2 &&
          std::abs(rotated.fuelExhaustionStepA - ordinary.fuelExhaustionStepA) <= 2 &&
          rotated.firstIgnitionStepFar == ordinary.firstIgnitionStepFar &&
          Near(rotated.finalTemperatureA, ordinary.finalTemperatureA, 1.0f) &&
          Near(rotated.finalTemperatureB, ordinary.finalTemperatureB, 1.0f) &&
          Near(rotated.finalTemperatureFar, ordinary.finalTemperatureFar, 1.0f),
          "rotating and translating the exact terrain/fire setup preserves ignition times and temperatures");
}

void TestReciprocalHeatTransfer() {
    std::printf("Section E: pairwise thermal exchange conserves transferred energy\n");
    Fixture fixture;
    CombustibleMaterial inert = IsolatedFuel();
    inert.initialFuelMassKg = 0.0f;
    inert.heatCapacityJPerK = 100.0f;
    inert.radiativeAreaSquareMeters = 1.0f;
    inert.emissivity = 1.0f;
    const BodyHandle hot = fixture.AddSphere({82.0f, 0.0f, 0.0f}, inert, 600.0f);
    const BodyHandle cold = fixture.AddSphere({82.0f, 0.8f, 0.0f}, inert, 300.0f);
    fixture.ThermalStep();
    const ThermalBodyState& a = *fixture.combustion.State(hot);
    const ThermalBodyState& b = *fixture.combustion.State(cold);
    const float totalSensibleJoules = inert.heatCapacityJPerK *
        ((a.temperatureKelvin - 600.0f) + (b.temperatureKelvin - 300.0f));
    const float ambientExchangeJoules =
        (a.environmentalHeatWattsReceived + b.environmentalHeatWattsReceived) * kDt;
    Check(a.pairwiseHeatWattsReceived < 0.0f && b.pairwiseHeatWattsReceived > 0.0f &&
          Near(a.pairwiseHeatWattsReceived + b.pairwiseHeatWattsReceived, 0.0f, 1.0e-3f),
          "nearby hot and cold material exchange equal and opposite radiative heat");
    Check(std::abs(totalSensibleJoules - ambientExchangeJoules) < 0.02f,
          "two-body sensible energy changes only by independently measured ambient exchange");
}

void TestOxidizerVacuumAndReturn() {
    std::printf("Section F: oxidizer, vacuum extinction and thermal return\n");
    const CombustibleMaterial fuel = IsolatedFuel();
    {
        Fixture normal;
        AtmosphereParameters noOxygenParameters;
        noOxygenParameters.oxidizerMassFraction = 0.0f;
        Fixture noOxygen(noOxygenParameters);
        const BodyHandle air = normal.AddSphere({82.0f, 0.0f, 0.0f}, fuel, 450.0f);
        const BodyHandle inertGas = noOxygen.AddSphere({82.0f, 0.0f, 0.0f}, fuel, 450.0f);
        normal.ThermalStep();
        noOxygen.ThermalStep();
        const ThermalBodyState& a = *normal.combustion.State(air);
        const ThermalBodyState& b = *noOxygen.combustion.State(inertGas);
        Check(a.burnRateKgPerSecond > 0.0f && a.localOxidizerMassDensity > 0.0f &&
              b.burnRateKgPerSecond == 0.0f && b.localOxidizerMassDensity == 0.0f &&
              Near(b.remainingFuelMassKg, fuel.initialFuelMassKg, 1.0e-7f),
              "the same hot fuel burns with oxidizer but not in equally dense inert gas");
    }
    {
        Fixture fixture;
        const BodyHandle body = fixture.AddSphere({82.0f, 0.0f, 0.0f}, fuel, 450.0f);
        fixture.ThermalStep();
        const float fuelBeforeVacuum = fixture.combustion.State(body)->remainingFuelMassKg;
        fixture.physics.ResetBody(body, {140.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f});
        fixture.ThermalStep();
        const ThermalBodyState& vacuum = *fixture.combustion.State(body);
        Check(vacuum.localOxidizerMassDensity == 0.0f &&
              vacuum.burnRateKgPerSecond == 0.0f && vacuum.heatOutputWatts == 0.0f &&
              Near(vacuum.remainingFuelMassKg, fuelBeforeVacuum, 1.0e-7f),
              "moving burning fuel into vacuum stops the reaction without consuming more fuel");
        fixture.physics.ResetBody(body, {82.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f});
        fixture.ThermalStep();
        const ThermalBodyState& reentry = *fixture.combustion.State(body);
        Check(reentry.burnRateKgPerSecond > 0.0f &&
              reentry.remainingFuelMassKg < fuelBeforeVacuum,
              "hot fuel with remaining mass re-enters oxidizer and reacts from its actual state");
    }
    {
        Fixture fixture;
        CombustibleMaterial radiating = fuel;
        radiating.heatCapacityJPerK = 30.0f;
        radiating.radiativeAreaSquareMeters = 1.0f;
        radiating.emissivity = 1.0f;
        const BodyHandle body = fixture.AddSphere({140.0f, 0.0f, 0.0f}, radiating, 410.0f);
        for (int step = 0; step < 120; ++step) fixture.ThermalStep();
        const float cooledTemperature = fixture.combustion.State(body)->temperatureKelvin;
        fixture.physics.ResetBody(body, {82.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f});
        fixture.ThermalStep();
        Check(cooledTemperature < radiating.ignitionTemperatureK &&
              fixture.combustion.State(body)->burnRateKgPerSecond == 0.0f,
              "vacuum-cooled fuel does not reignite on return merely because it once burned");
    }
}

struct FlowResult {
    float burnRate = 0.0f;
    float oxygenSupply = 0.0f;
    float relativeAirspeed = 0.0f;
    float temperature = 0.0f;
    double fuel = 0.0;
};

FlowResult RunFlow(const glm::vec3& frameOrigin, const glm::quat& rotation,
                   const glm::vec3& bulkVelocity, const glm::vec3& relativeVelocity,
                   const WorldCoordinates& coordinates) {
    Fixture fixture;
    fixture.frame.originPosition = frameOrigin;
    fixture.frame.orientation = rotation;
    fixture.frame.linearVelocity = bulkVelocity;
    CombustibleMaterial fuel = IsolatedFuel();
    fuel.initialFuelMassKg = 1.0f;
    fuel.maximumFuelRateKgPerSecond = 0.1f;
    fuel.oxygenRequiredKgPerKgFuel = 3.0f;
    fuel.oxygenTransportSpeedMetersPerSecond = 0.1f;
    fuel.airflowTransportFactor = 1.0f;
    const glm::vec3 local = rotation * glm::vec3(82.0f, 0.0f, 0.0f);
    const glm::vec3 position = coordinates.ToLocal(coordinates.ToGlobal(frameOrigin + local));
    const BodyHandle body = fixture.AddSphere(position, fuel, 450.0f);
    fixture.physics.SetLinearVelocity(body, bulkVelocity + rotation * relativeVelocity);
    fixture.ThermalStep();
    const ThermalBodyState& state = *fixture.combustion.State(body);
    return {state.burnRateKgPerSecond, state.oxygenSupplyKgPerSecond,
            state.relativeAirspeedMetersPerSecond, state.temperatureKelvin,
            state.remainingFuelMassKg};
}

void TestFlowRotationAndFarOrigin() {
    std::printf("Section G: gas-relative flow, rotation and far-origin equivalence\n");
    const WorldCoordinates near;
    const WorldCoordinates far(glm::dvec3(1.0e9, -2.0e9, 3.0e9));
    const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    const FlowResult rest = RunFlow(glm::vec3(0.0f), identity,
                                    glm::vec3(0.0f), glm::vec3(0.0f), near);
    const FlowResult boosted = RunFlow({200.0f, -70.0f, 35.0f}, identity,
                                       {60.0f, -9.0f, 4.0f}, glm::vec3(0.0f), near);
    const FlowResult moving = RunFlow({200.0f, -70.0f, 35.0f}, identity,
                                      {60.0f, -9.0f, 4.0f}, {0.0f, 15.0f, 0.0f}, near);
    Check(Near(rest.relativeAirspeed, 0.0f, 1.0e-6f) &&
          Near(boosted.relativeAirspeed, 0.0f, 1.0e-5f) &&
          Near(rest.oxygenSupply, boosted.oxygenSupply, 1.0e-5f) &&
          Near(rest.burnRate, boosted.burnRate, 1.0e-5f),
          "shared gas/body frame velocity leaves oxygen supply and burning unchanged");
    Check(Near(moving.relativeAirspeed, 15.0f, 1.0e-4f) &&
          moving.oxygenSupply > boosted.oxygenSupply * 5.0f &&
          moving.burnRate > boosted.burnRate * 5.0f,
          "actual relative airflow increases oxygen delivery and reaction rate");

    const glm::quat turn = glm::normalize(glm::angleAxis(
        0.79f, glm::normalize(glm::vec3(1.0f, -2.0f, 3.0f))));
    const FlowResult turned = RunFlow({-250.0f, 80.0f, 125.0f}, turn,
                                      turn * glm::vec3(60.0f, -9.0f, 4.0f),
                                      {0.0f, 15.0f, 0.0f}, near);
    const FlowResult distant = RunFlow({200.0f, -70.0f, 35.0f}, identity,
                                       {60.0f, -9.0f, 4.0f},
                                       {0.0f, 15.0f, 0.0f}, far);
    Check(Near(turned.relativeAirspeed, moving.relativeAirspeed, 1.0e-4f) &&
          Near(turned.oxygenSupply, moving.oxygenSupply, 1.0e-5f) &&
          Near(turned.burnRate, moving.burnRate, 1.0e-5f),
          "rotating and translating the physical scenario preserves thermal scalars");
    Check(Near(distant.oxygenSupply, moving.oxygenSupply, 1.0e-7f) &&
          Near(distant.burnRate, moving.burnRate, 1.0e-7f) &&
          Near(distant.temperature, moving.temperature, 1.0e-6f) &&
          std::abs(distant.fuel - moving.fuel) < 1.0e-7 &&
          far.ToGlobal(glm::vec3(82.0f, 0.0f, 0.0f)).x > 1.0e9,
          "M23 double-origin translation preserves identical local combustion state");

    Fixture spin;
    CombustibleMaterial limited = IsolatedFuel();
    limited.initialFuelMassKg = 1.0f;
    limited.maximumFuelRateKgPerSecond = 0.1f;
    limited.oxygenTransportSpeedMetersPerSecond = 0.1f;
    limited.airflowTransportFactor = 1.0f;
    limited.oxygenRequiredKgPerKgFuel = 3.0f;
    spin.frame.angularVelocity = {0.0f, 0.0f, 0.2f};
    const BodyHandle body = spin.AddSphere({82.0f, 0.0f, 0.0f}, limited, 450.0f);
    spin.physics.SetLinearVelocity(body, {0.0f, 16.4f, 0.0f});
    spin.ThermalStep();
    const float coRotatingAirspeed =
        spin.combustion.State(body)->relativeAirspeedMetersPerSecond;
    const float coRotatingOxygenSupply =
        spin.combustion.State(body)->oxygenSupplyKgPerSecond;
    spin.physics.SetLinearVelocity(body, {0.0f, 0.0f, 0.0f});
    spin.ThermalStep();
    Check(Near(coRotatingAirspeed, 0.0f, 1.0e-4f) &&
          Near(spin.combustion.State(body)->relativeAirspeedMetersPerSecond,
               16.4f, 1.0e-4f) &&
          spin.combustion.State(body)->oxygenSupplyKgPerSecond >
              coRotatingOxygenSupply * 5.0f,
          "rotating atmosphere uses point velocity rather than only frame-centre velocity");
}

void TestMovingRigidBodyAndReset() {
    std::printf("Section H: moving rigid body carries combustion through gas into vacuum\n");
    Fixture fixture;
    CombustibleMaterial fuel = IsolatedFuel();
    fuel.initialFuelMassKg = 1.0f;
    const BodyHandle body = fixture.AddSphere({85.0f, 0.0f, 0.0f}, fuel, 450.0f);
    fixture.physics.SetLinearVelocity(body, {30.0f, 0.0f, 0.0f});
    bool burnedInGas = false;
    bool reachedVacuum = false;
    bool staleBurnInVacuum = false;
    float fuelAtExit = -1.0f;
    for (int step = 0; step < 90; ++step) {
        fixture.ThermalStep();
        const ThermalBodyState& state = *fixture.combustion.State(body);
        const float radius = glm::length(fixture.physics.GetTransform(body).position);
        if (radius < fixture.atmosphere.Parameters().topRadius &&
            state.burnRateKgPerSecond > 0.0f) burnedInGas = true;
        if (radius >= fixture.atmosphere.Parameters().topRadius) {
            reachedVacuum = true;
            if (fuelAtExit < 0.0f) fuelAtExit = state.remainingFuelMassKg;
            staleBurnInVacuum |= state.burnRateKgPerSecond > 0.0f ||
                                 state.heatOutputWatts > 0.0f;
        }
        fixture.physics.Step(kDt);
    }
    const ThermalBodyState& final = *fixture.combustion.State(body);
    const glm::vec3 finalPosition = fixture.physics.GetTransform(body).position;
    std::printf("  moving body final r %.3f m, fuel %.6f kg, T %.3f K\n",
                glm::length(finalPosition), final.remainingFuelMassKg, final.temperatureKelvin);
    Check(burnedInGas && reachedVacuum && !staleBurnInVacuum &&
          Near(final.remainingFuelMassKg, fuelAtExit, 1.0e-6f) &&
          glm::length(finalPosition) > fixture.atmosphere.Parameters().topRadius + 5.0f,
          "thermal material follows the actual moving PhysicsWorld body and extinguishes at vacuum");
    fixture.combustion.Reset();
    const ThermalBodyState& reset = *fixture.combustion.State(body);
    Check(Near(reset.remainingFuelMassKg, fuel.initialFuelMassKg, 1.0e-7f) &&
          Near(reset.temperatureKelvin, 450.0f, 1.0e-6f) &&
          reset.burnRateKgPerSecond == 0.0f,
          "thermal reset restores initial fuel and temperature without stale burn rate");
}

struct CargoAscentResult {
    std::array<int, 3> vacuumStep{{-1, -1, -1}};
    std::array<float, 3> finalCargoRadius{{0.0f, 0.0f, 0.0f}};
    float finalShipRadius = 0.0f;
    float fuelAtVacuum = -1.0f;
    float finalFuel = 0.0f;
    float finalOxidizerDensity = 0.0f;
    float largestSupportGap = -std::numeric_limits<float>::infinity();
    float largestCargoStep = 0.0f;
    float finalShipCargoSeparation = 0.0f;
    bool burnedInAir = false;
    bool staleReactionInVacuum = false;
};

CargoAscentResult RunShipCargoAscent(bool sasEnabled, bool allThreeBlocks) {
    Fixture fixture;
    constexpr float shipMass = 80.0f;
    constexpr float cargoMass = 5.0f;
    constexpr float shipThrustNewtons = 1200.0f;
    constexpr float localGravityRegionRadius = 103.0f;
    const glm::vec3 outward(1.0f, 0.0f, 0.0f);
    const glm::quat shipRotation = glm::angleAxis(
        glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::vec3 shipStart(82.0f, 0.0f, 0.0f);
    // The first position matches the real M27 demo block. The other two
    // reproduce the near-spread and distant negative-control placements.
    const std::array<glm::vec3, 3> localOffsets{{
        {-1.1f, 0.53f, -1.2f}, {-1.1f, 0.53f, -0.65f}, {1.4f, 0.53f, 2.2f}}};
    std::array<BodyHandle, 3> cargo;
    const int cargoCount = allThreeBlocks ? 3 : 1;
    for (int i = 0; i < cargoCount; ++i) {
        const glm::vec3 position = shipStart + shipRotation * localOffsets[i];
        cargo[i] = fixture.physics.CreateDynamicBox(
            position, {0.25f, 0.25f, 0.25f}, cargoMass, 0.8f, 0.0f);
        fixture.physics.ResetBody(cargo[i], position, shipRotation);
    }
    // Match Application's collision-body insertion order: the pickup
    // candidates exist before their supporting spacecraft body.
    const BodyHandle ship = fixture.physics.CreateDynamicBox(
        shipStart, {2.0f, 0.25f, 3.0f}, shipMass, 0.8f, 0.0f);
    fixture.physics.ResetBody(ship, shipStart, shipRotation);
    // Match the live demonstration coating while beginning after a genuine
    // ignition has already raised A above its threshold.
    CombustibleMaterial fuel;
    fuel.heatCapacityJPerK = 150.0f;
    fuel.initialFuelMassKg = 0.12f;
    fuel.ignitionTemperatureK = 550.0f;
    fuel.maximumFuelRateKgPerSecond = 0.003f;
    fuel.radiativeAreaSquareMeters = 1.5f;
    fuel.retainedCombustionHeatFraction = 0.75f;
    fixture.combustion.AddBody(cargo[0], fuel, 650.0f);
    const RadicalGravity propGravity(glm::vec3(0.0f), 9.81f);
    Window window;
    window.SetTestInputMode(true);
    FlyingPrimitiveControl control;
    control.handle = ship;
    SetSpacecraftSasEnabled(control, sasEnabled, fixture.physics);
    CargoAscentResult result;
    glm::vec3 previousCargo = fixture.physics.GetTransform(cargo[0]).position;
    int vacuumStepsForA = 0;
    for (int step = 0; step < 600; ++step) {
        const glm::vec3 shipPosition = fixture.physics.GetTransform(ship).position;
        fixture.physics.ApplyForce(ship, shipMass *
            CelestialGravity::AccelerationFromPointMass(
                glm::vec3(0.0f), fixture.atmosphere.Parameters().gravitationalParameter,
                shipPosition));
        fixture.physics.ApplyForce(ship, outward * shipThrustNewtons);
        ApplyFlyingPrimitiveControl(control, window, fixture.physics);
        for (int i = 0; i < cargoCount; ++i) {
            const glm::vec3 position = fixture.physics.GetTransform(cargo[i]).position;
            if (glm::length(position) < localGravityRegionRadius) {
                fixture.physics.ApplyLinearAcceleration(cargo[i],
                    propGravity.Sample(position), kDt);
            }
        }
        fixture.physics.Step(kDt);
        fixture.ThermalStep();
        const BodyTransform shipAfter = fixture.physics.GetTransform(ship);
        const glm::vec3 firstCargo = fixture.physics.GetTransform(cargo[0]).position;
        const ThermalBodyState& thermal = *fixture.combustion.State(cargo[0]);
        const float firstRadius = glm::length(firstCargo);
        const glm::vec3 firstInShip = glm::inverse(shipAfter.rotation) *
            (firstCargo - shipAfter.position);
        result.largestSupportGap = std::max(result.largestSupportGap,
                                             firstInShip.y - 0.50f);
        result.largestCargoStep = std::max(result.largestCargoStep,
                                            glm::length(firstCargo - previousCargo));
        previousCargo = firstCargo;
        if (firstRadius < fixture.atmosphere.Parameters().topRadius &&
            thermal.burnRateKgPerSecond > 0.0f) result.burnedInAir = true;
        for (int i = 0; i < cargoCount; ++i) {
            const float radius = glm::length(fixture.physics.GetTransform(cargo[i]).position);
            if (result.vacuumStep[i] < 0 &&
                radius >= fixture.atmosphere.Parameters().topRadius) {
                result.vacuumStep[i] = step;
            }
        }
        if (result.vacuumStep[0] >= 0) {
            if (result.fuelAtVacuum < 0.0f) result.fuelAtVacuum = thermal.remainingFuelMassKg;
            result.staleReactionInVacuum |= thermal.burnRateKgPerSecond > 0.0f ||
                                            thermal.heatOutputWatts > 0.0f;
            ++vacuumStepsForA;
        }
        if (sasEnabled && vacuumStepsForA >= 20) break;
    }
    const glm::vec3 finalShip = fixture.physics.GetTransform(ship).position;
    const glm::vec3 finalFirstCargo = fixture.physics.GetTransform(cargo[0]).position;
    result.finalShipRadius = glm::length(finalShip);
    result.finalShipCargoSeparation = glm::length(finalShip - finalFirstCargo);
    for (int i = 0; i < cargoCount; ++i) {
        result.finalCargoRadius[i] = glm::length(fixture.physics.GetTransform(cargo[i]).position);
    }
    result.finalFuel = fixture.combustion.State(cargo[0])->remainingFuelMassKg;
    result.finalOxidizerDensity = fixture.combustion.State(cargo[0])->localOxidizerMassDensity;
    return result;
}

void TestShipContactCarriesFuelToVacuum() {
    std::printf("Section I: SAS-held ship lifts physical burning cargo into vacuum\n");
    const CargoAscentResult stabilized = RunShipCargoAscent(true, false);
    const CargoAscentResult freeAttitude = RunShipCargoAscent(false, false);
    const CargoAscentResult threeBlocks = RunShipCargoAscent(true, true);
    std::printf("  SAS on/off A exit step %d/%d; SAS three-block A/B/C %d/%d/%d; "
                "ship/cargo r %.3f/%.3f m, gap %.4f m, step %.4f m\n",
                stabilized.vacuumStep[0], freeAttitude.vacuumStep[0],
                threeBlocks.vacuumStep[0], threeBlocks.vacuumStep[1],
                threeBlocks.vacuumStep[2],
                stabilized.finalShipRadius, stabilized.finalCargoRadius[0],
                stabilized.largestSupportGap, stabilized.largestCargoStep);
    Check(stabilized.burnedInAir && stabilized.vacuumStep[0] > 0 &&
          stabilized.finalShipRadius > 110.0f &&
          stabilized.finalCargoRadius[0] > 110.0f &&
          stabilized.finalShipCargoSeparation < 3.0f &&
          stabilized.largestSupportGap < 0.25f &&
          stabilized.largestCargoStep < 1.0f,
          "production SAS plus 1200 N thrust carries the off-centre block by ordinary rigid contact");
    Check(!stabilized.staleReactionInVacuum &&
          Near(stabilized.finalFuel, stabilized.fuelAtVacuum, 1.0e-6f) &&
          stabilized.finalOxidizerDensity == 0.0f,
          "the physically carried material extinguishes as its own position reaches vacuum");
    Check(freeAttitude.finalShipRadius > 110.0f &&
          freeAttitude.vacuumStep[0] < 0 && freeAttitude.finalCargoRadius[0] < 110.0f &&
          freeAttitude.finalShipCargoSeparation > 20.0f,
          "without attitude hold, the off-centre block can slide off during ascent");
    Check(threeBlocks.vacuumStep[0] > 0 && threeBlocks.vacuumStep[1] > 0 &&
          std::abs(threeBlocks.vacuumStep[0] - threeBlocks.vacuumStep[1]) < 20 &&
          threeBlocks.finalCargoRadius[0] > 110.0f &&
          threeBlocks.finalCargoRadius[1] > 110.0f,
          "the real near-spread A/B block pair rides the SAS-held ship into vacuum together");
}

glm::vec3 Centroid(const std::vector<FireVisualPrimitive>& visuals,
                   FireVisualKind kind) {
    glm::vec3 sum(0.0f);
    int count = 0;
    for (const FireVisualPrimitive& visual : visuals) {
        if (visual.kind != kind) continue;
        sum += visual.position;
        ++count;
    }
    return count > 0 ? sum / static_cast<float>(count) : glm::vec3(0.0f);
}

void TestProductPresentation() {
    std::printf("Section J: visual product motion has no hidden world up or ether\n");
    FirePresentationInput input;
    input.position = {4.0f, -3.0f, 2.0f};
    input.orientation = glm::normalize(glm::angleAxis(
        0.63f, glm::normalize(glm::vec3(0.3f, -0.8f, 0.4f))));
    input.burnRateKgPerSecond = 0.01f;
    input.temperatureKelvin = 750.0f;
    input.gasDensity = 0.04f;
    input.sourceRadius = 0.3f;
    const std::vector<FireVisualPrimitive> zeroG = BuildFirePresentation(input);
    Check(zeroG.size() == 13 &&
          Near(Centroid(zeroG, FireVisualKind::Smoke), input.position, 1.0e-5f),
          "zero gravity and zero relative flow yield a symmetric visual, not a +Y candle plume");
    input.burnRateKgPerSecond = 0.0f;
    Check(BuildFirePresentation(input).empty(),
          "presentation vanishes when authoritative combustion rate is zero");
    input.burnRateKgPerSecond = 0.01f;
    input.gravityAcceleration = glm::normalize(glm::vec3(-2.0f, 1.0f, 3.0f)) * 9.0f;
    input.gasVelocity = {0.0f, 2.0f, 0.0f};
    input.bodyVelocity = {1.0f, 1.0f, -1.0f};
    const std::vector<FireVisualPrimitive> original = BuildFirePresentation(input);
    const glm::quat turn = glm::normalize(glm::angleAxis(
        0.89f, glm::normalize(glm::vec3(-1.0f, 3.0f, 2.0f))));
    const glm::vec3 offset(200.0f, -90.0f, 65.0f);
    FirePresentationInput transformed = input;
    transformed.position = offset + turn * input.position;
    transformed.orientation = turn * input.orientation;
    transformed.bodyVelocity = turn * input.bodyVelocity;
    transformed.gasVelocity = turn * input.gasVelocity;
    transformed.gravityAcceleration = turn * input.gravityAcceleration;
    const std::vector<FireVisualPrimitive> rotated = BuildFirePresentation(transformed);
    bool equivalent = original.size() == rotated.size();
    if (equivalent) {
        for (std::size_t i = 0; i < original.size(); ++i) {
            equivalent &= original[i].kind == rotated[i].kind &&
                          Near(rotated[i].position, offset + turn * original[i].position, 5.0e-5f) &&
                          Near(rotated[i].radius, original[i].radius, 1.0e-6f) &&
                          Near(rotated[i].alpha, original[i].alpha, 1.0e-6f) &&
                          Near(rotated[i].color, original[i].color, 1.0e-6f);
        }
    }
    Check(equivalent, "rotated and translated universe preserves the whole flame/smoke geometry");
    FirePresentationInput boosted = input;
    boosted.bodyVelocity += glm::vec3(250.0f, -30.0f, 70.0f);
    boosted.gasVelocity += glm::vec3(250.0f, -30.0f, 70.0f);
    const std::vector<FireVisualPrimitive> comoving = BuildFirePresentation(boosted);
    bool sameUnderBoost = original.size() == comoving.size();
    if (sameUnderBoost) {
        for (std::size_t i = 0; i < original.size(); ++i) {
            sameUnderBoost &= Near(original[i].position, comoving[i].position, 1.0e-5f);
        }
    }
    Check(sameUnderBoost,
          "a shared body/gas velocity boost does not create visual wind");
    FirePresentationInput airflow = input;
    airflow.gasVelocity += glm::vec3(0.0f, 0.0f, 4.0f);
    const glm::vec3 originalSmoke = Centroid(original, FireVisualKind::Smoke);
    const glm::vec3 flowingSmoke = Centroid(BuildFirePresentation(airflow), FireVisualKind::Smoke);
    Check(glm::dot(flowingSmoke - originalSmoke, glm::vec3(0.0f, 0.0f, 1.0f)) > 0.05f,
          "real gas-relative flow carries visible products in its actual direction");
}

void TestThinAirFuelAccounting() {
    std::printf("Section K: thin-air heat consumes representable finite fuel\n");
    Fixture fixture;
    CombustibleMaterial fuel;
    fuel.heatCapacityJPerK = 150.0f;
    fuel.initialFuelMassKg = 0.12f;
    fuel.ignitionTemperatureK = 550.0f;
    fuel.maximumFuelRateKgPerSecond = 0.003f;
    fuel.radiativeAreaSquareMeters = 1.5f;
    fuel.retainedCombustionHeatFraction = 0.0f;
    fuel.emissivity = 0.0f;
    fuel.convectionCoefficientWattsPerSquareMeterKelvin = 0.0f;
    const glm::vec3 position(109.1f, 0.0f, 0.0f);
    const AtmosphereSample gas = fixture.atmosphere.Sample(position, fixture.frame);
    const BodyHandle body = fixture.AddSphere(position, fuel, 700.0f);
    const double initialFuel = fixture.combustion.State(body)->remainingFuelMassKg;
    fixture.ThermalStep();
    const ThermalBodyState first = *fixture.combustion.State(body);
    const double firstStepConsumed = initialFuel - first.remainingFuelMassKg;
    const double oldFloatUlp = static_cast<double>(
        std::nextafter(fuel.initialFuelMassKg, std::numeric_limits<float>::infinity()) -
        fuel.initialFuelMassKg);
    Check(gas.oxidizerMassDensity > 0.0f && first.heatOutputWatts > 0.0f &&
          firstStepConsumed > 0.0 && firstStepConsumed < 0.5 * oldFloatUlp,
          "thin gas burns less than half a float fuel ULP yet consumes real fuel");

    double integratedHeatJoules = static_cast<double>(first.heatOutputWatts) * kDt;
    constexpr int steps = 10000;
    for (int step = 1; step < steps; ++step) {
        fixture.ThermalStep();
        integratedHeatJoules +=
            static_cast<double>(fixture.combustion.State(body)->heatOutputWatts) * kDt;
    }
    const ThermalBodyState& final = *fixture.combustion.State(body);
    const double consumedFuel = initialFuel - final.remainingFuelMassKg;
    const double chemicalEnergyJoules = consumedFuel * fuel.heatReleaseJPerKg;
    std::printf("  r %.1f m, first loss %.3e kg, float ULP %.3e kg, "
                "total loss %.7f kg, heat %.3f / %.3f J\n",
                glm::length(position), firstStepConsumed, oldFloatUlp,
                consumedFuel, integratedHeatJoules, chemicalEnergyJoules);
    Check(consumedFuel > 1.0e-5 && final.temperatureKelvin == 700.0f &&
          std::abs(integratedHeatJoules - chemicalEnergyJoules) < 0.01,
          "thin-air chemical heat equals actual fuel mass lost over many steps");
}

void TestStepCost() {
    std::printf("Section L: bounded fixed-step cost measurement\n");
    Fixture fixture;
    CombustibleMaterial fuel = IsolatedFuel();
    fuel.initialFuelMassKg = 100.0f;
    fuel.maximumFuelRateKgPerSecond = 0.01f;
    fuel.emissivity = 0.8f;
    fuel.convectionCoefficientWattsPerSquareMeterKelvin = 8.0f;
    const BodyHandle body = fixture.AddSphere({82.0f, 0.0f, 0.0f}, fuel, 450.0f);
    constexpr int steps = 10000;
    const auto start = std::chrono::steady_clock::now();
    for (int step = 0; step < steps; ++step) fixture.ThermalStep();
    const auto end = std::chrono::steady_clock::now();
    const double totalMilliseconds =
        std::chrono::duration<double, std::milli>(end - start).count();
    const ThermalBodyState& state = *fixture.combustion.State(body);
    std::printf("  %d thermal steps: %.3f ms, %.5f ms/step, fuel %.6f kg, T %.3f K\n",
                steps, totalMilliseconds, totalMilliseconds / steps,
                state.remainingFuelMassKg, state.temperatureKelvin);
    Check(Finite(state) && totalMilliseconds >= 0.0,
          "long thermal run remains finite; measured cost is diagnostic, not a hardware threshold");
}
}  // namespace

int main() {
    TestAtmosphereThermalAndOxidizerState();
    TestIgnitionAndFiniteFuel();
    TestThermalSpread();
    TestAuthoredTerrainSpread();
    TestReciprocalHeatTransfer();
    TestOxidizerVacuumAndReturn();
    TestFlowRotationAndFarOrigin();
    TestMovingRigidBodyAndReset();
    TestShipContactCarriesFuelToVacuum();
    TestProductPresentation();
    TestThinAirFuelAccounting();
    TestStepCost();
    std::printf("Combustion tests: %s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
