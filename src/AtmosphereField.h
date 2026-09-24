#pragma once

#include <glm/glm.hpp>

#include "ReferenceFrame.h"

class RadialTerrain;

// A bounded, hydrostatic gas continuum around a planet. These are physical
// model parameters, not a drag-altitude lookup. The reference radius is an
// equipotential surface; actual terrain may rise above or fall below it.
struct AtmosphereParameters {
    float referenceRadius = 80.0f;           // m
    float topRadius = 110.0f;                // m, continuous vacuum above
    float gravitationalParameter = 62784.0f; // m^3/s^2 (9.81 m/s^2 at 80 m)
    float polytropicExponent = 1.4f;        // 1 < gamma < 2 in P = K rho^gamma
    float referenceDensity = 0.05f;         // kg/m^3 at referenceRadius
    // Prescribed mixture composition and thermal reference state. Trailing
    // defaults preserve the original five-parameter M26 aggregate setup.
    float oxidizerMassFraction = 0.21f;      // kg oxidizer / kg gas
    float referenceTemperatureKelvin = 300.0f;
};

struct AtmosphereSample {
    float density = 0.0f;  // kg/m^3
    float pressure = 0.0f; // Pa
    glm::vec3 velocity{0.0f}; // world-space gas velocity, m/s
    float oxidizerMassDensity = 0.0f; // kg oxidizer / m^3
    float temperatureKelvin = 0.0f;
};

// This is an analytic equilibrium reservoir, not a particle/grid CFD solver:
// it does not evolve wakes or accept reaction momentum from a passing body.
// Pressure and mass density are nevertheless linked by an equation of state
// and satisfy hydrostatic balance under the configured inverse-square gravity
// for a nonaccelerating, nonspinning planet. A translated or rotating frame
// still supplies correct point velocity for relative airflow; sustained spin
// or frame acceleration is not fed back into this prescribed pressure profile.
// A planet ReferenceFrame supplies current pose and motion at every sample;
// no world origin, axis, or universal state of rest is privileged.
class AtmosphereField {
public:
    explicit AtmosphereField(const AtmosphereParameters& parameters,
                             const RadialTerrain* solidTerrain = nullptr);

    const AtmosphereParameters& Parameters() const { return m_parameters; }
    float ReferencePressure() const { return m_referencePressure; }

    AtmosphereSample Sample(const glm::vec3& worldPosition,
                            const ReferenceFrame& planetFrame) const;

    // Radial diagnostics are independent of terrain. The full Sample() masks
    // gas inside solid terrain geometry before returning these quantities.
    float DensityAtRadius(float radius) const;
    float PressureAtRadius(float radius) const;
    float TemperatureAtRadius(float radius) const;
    float SpecificGasConstant() const { return m_specificGasConstant; }

    // Potential used to construct the hydrostatic gas profile. The actual
    // planetary force belongs to CelestialGravity, separate from gas.
    float SpecificPotentialAtRadius(float radius) const;

private:
    float EnthalpyFraction(float radius) const;

    AtmosphereParameters m_parameters;
    const RadialTerrain* m_solidTerrain = nullptr; // non-owning; must outlive field
    float m_referencePressure = 0.0f;
    float m_inversePotentialSpan = 0.0f;
    float m_specificGasConstant = 0.0f;
};
