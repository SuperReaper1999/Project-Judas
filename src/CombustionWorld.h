#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "PhysicsWorld.h"

class AtmosphereField;
struct ReferenceFrame;

// A finite combustible coating on an ordinary rigid carrier. The carrier's
// PhysicsWorld mass/inertia and collision shape remain ordinary rigid-body
// state; thermal energy is tracked in the exposed material. A small fuel
// coating keeps neglecting burnt mass in the rigid inertia a bounded M27
// approximation rather than pretending material mass never changes.
struct CombustibleMaterial {
    float heatCapacityJPerK = 300.0f;
    float initialFuelMassKg = 0.0f;
    float ignitionTemperatureK = 600.0f;
    float activationRangeK = 80.0f;
    float maximumFuelRateKgPerSecond = 0.003f;
    float heatReleaseJPerKg = 1.6e7f;
    // Remaining reaction energy leaves with hot products into the
    // prescribed gas reservoir. The gas profile itself is not mutable;
    // exposing this power lets a bounded product presentation/simulation
    // use it without inventing extra heat in the solid.
    float retainedCombustionHeatFraction = 1.0f;
    float oxygenRequiredKgPerKgFuel = 3.0f;
    // Effective oxygen transport speed includes local diffusion/mixing at
    // rest; relative gas flow adds the second term. Both multiply local
    // oxidizer mass density and exposed area to produce kg O2/s.
    float oxygenTransportSpeedMetersPerSecond = 0.5f;
    float airflowTransportFactor = 0.2f;
    // Effective exchange/capture area, rather than the entire box area:
    // the same geometry drives pairwise radiation, heater absorption,
    // oxygen access, and convection. This can be supplied from any shape.
    float radiativeAreaSquareMeters = 0.5f;
    float emissivity = 0.8f;
    float convectionCoefficientWattsPerSquareMeterKelvin = 8.0f;
};

struct ThermalBodyState {
    BodyHandle body;
    float temperatureKelvin = 0.0f;
    float previousTemperatureKelvin = 0.0f;
    // Thin-air oxidation can consume less than one float ULP of a coating
    // per fixed step. Keep the authoritative finite reservoir in double so
    // every positive heat release has a corresponding fuel decrement.
    double remainingFuelMassKg = 0.0;
    float burnRateKgPerSecond = 0.0f;
    float heatOutputWatts = 0.0f; // total chemical energy release
    float heatToGasWatts = 0.0f;
    float oxygenSupplyKgPerSecond = 0.0f;
    float heaterWattsReceived = 0.0f;
    float pairwiseHeatWattsReceived = 0.0f;
    float environmentalHeatWattsReceived = 0.0f;
    float relativeAirspeedMetersPerSecond = 0.0f;
    float localOxidizerMassDensity = 0.0f;
};

// An externally powered, isotropic point radiator. The supplied energy
// reaches materials through ordinary geometric interception; there is no
// target handle and no Ignite command. It may be moved by the player's
// existing pose/look controls without becoming authoritative fire state.
struct RadiantHeater {
    glm::vec3 worldPosition{0.0f};
    float powerWatts = 0.0f;
};

class CombustionWorld {
public:
    // Registers a physical body and its thermal material. Repeated handles
    // or non-finite/negative material parameters are rejected.
    void AddBody(BodyHandle body, const CombustibleMaterial& material,
                 float initialTemperatureKelvin);
    void Clear();
    void Reset();

    // Integrates one fixed thermal step. The source body's CURRENT physical
    // pose/velocity and local M26 gas sample determine all heat and oxygen
    // exchange. Pairwise radiative exchange is reciprocal and is computed
    // from the same pre-step temperatures for every body, so registration
    // order cannot change spread. Thermal state never edits rigid velocity.
    void Step(float fixedDeltaTime, const PhysicsWorld& physics,
              const AtmosphereField& atmosphere, const ReferenceFrame& planetFrame,
              const RadiantHeater* heater = nullptr);

    const ThermalBodyState* State(BodyHandle body) const;
    const std::vector<ThermalBodyState>& States() const { return m_states; }
    float PresentedTemperature(BodyHandle body, float alpha) const;

private:
    std::vector<CombustibleMaterial> m_materials;
    std::vector<ThermalBodyState> m_states;
    std::vector<float> m_initialTemperatures;
};
