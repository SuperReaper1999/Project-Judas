#include "AtmosphereField.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "RadialTerrain.h"

namespace {
bool PositiveFinite(float value) {
    return value > 0.0f && std::isfinite(value);
}
}  // namespace

AtmosphereField::AtmosphereField(const AtmosphereParameters& parameters,
                                 const RadialTerrain* solidTerrain)
    : m_parameters(parameters), m_solidTerrain(solidTerrain) {
    if (!PositiveFinite(parameters.referenceRadius) ||
        !PositiveFinite(parameters.topRadius) ||
        !(parameters.topRadius > parameters.referenceRadius) ||
        !PositiveFinite(parameters.gravitationalParameter) ||
        !PositiveFinite(parameters.referenceDensity) ||
        !std::isfinite(parameters.polytropicExponent) ||
        !(parameters.polytropicExponent > 1.0f) ||
        !(parameters.polytropicExponent < 2.0f) ||
        !std::isfinite(parameters.oxidizerMassFraction) ||
        parameters.oxidizerMassFraction < 0.0f ||
        parameters.oxidizerMassFraction > 1.0f ||
        !PositiveFinite(parameters.referenceTemperatureKelvin)) {
        throw std::invalid_argument("AtmosphereField requires finite positive radii, gravity, "
                                    "density, temperature, an oxidizer fraction in [0, 1], "
                                    "and polytropic exponent in (1, 2)");
    }

    // Integrating dP/dr = -rho*mu/r^2 with P=K*rho^gamma and the boundary
    // condition P(topRadius)=rho(topRadius)=0 gives this finite polytrope.
    // Unlike a clamped exponential, pressure, density, and their gradients
    // reach vacuum continuously at the chosen top radius.
    const float potentialSpan = parameters.gravitationalParameter *
        (1.0f / parameters.referenceRadius - 1.0f / parameters.topRadius);
    m_referencePressure = ((parameters.polytropicExponent - 1.0f) /
                           parameters.polytropicExponent) *
                          parameters.referenceDensity * potentialSpan;
    // An effective ideal-gas constant makes P = rho * R * T consistent
    // with the existing polytrope at its reference radius. This scaled
    // demonstration gas is not claimed to have Earth-air molecular mass.
    m_specificGasConstant = m_referencePressure /
        (parameters.referenceDensity * parameters.referenceTemperatureKelvin);
    m_inversePotentialSpan = 1.0f /
        (1.0f / parameters.referenceRadius - 1.0f / parameters.topRadius);
    if (!PositiveFinite(m_referencePressure) || !PositiveFinite(m_inversePotentialSpan) ||
        !PositiveFinite(m_specificGasConstant)) {
        throw std::invalid_argument("AtmosphereField derived pressure or radius span is invalid");
    }
}

float AtmosphereField::EnthalpyFraction(float radius) const {
    if (!(radius > 0.0f) || !std::isfinite(radius) ||
        radius >= m_parameters.topRadius) return 0.0f;
    return (1.0f / radius - 1.0f / m_parameters.topRadius) *
           m_inversePotentialSpan;
}

float AtmosphereField::DensityAtRadius(float radius) const {
    const float fraction = EnthalpyFraction(radius);
    if (!(fraction > 0.0f)) return 0.0f;
    return m_parameters.referenceDensity *
           std::pow(fraction, 1.0f / (m_parameters.polytropicExponent - 1.0f));
}

float AtmosphereField::PressureAtRadius(float radius) const {
    const float fraction = EnthalpyFraction(radius);
    if (!(fraction > 0.0f)) return 0.0f;
    return m_referencePressure *
           std::pow(fraction, m_parameters.polytropicExponent /
                                  (m_parameters.polytropicExponent - 1.0f));
}

float AtmosphereField::TemperatureAtRadius(float radius) const {
    // The M26 polytrope obeys rho ~ q^(1/(gamma-1)) and P ~
    // q^(gamma/(gamma-1)), so its ideal-gas temperature P/(rho R) is
    // exactly proportional to q. Evaluate q directly near vacuum to
    // avoid dividing two tiny floating-point quantities.
    return m_parameters.referenceTemperatureKelvin * EnthalpyFraction(radius);
}

AtmosphereSample AtmosphereField::Sample(const glm::vec3& worldPosition,
                                         const ReferenceFrame& planetFrame) const {
    AtmosphereSample gas;
    const glm::vec3 localPosition = PositionFromWorld(planetFrame, worldPosition);
    const float radius = glm::length(localPosition);
    if (!(radius > 0.0f) || radius >= m_parameters.topRadius ||
        !std::isfinite(radius)) return gas;
    if (m_solidTerrain && radius <= m_solidTerrain->BoundRadius() &&
        m_solidTerrain->Sample(localPosition).signedDistance < 0.0f) {
        return gas;
    }

    gas.density = DensityAtRadius(radius);
    gas.pressure = PressureAtRadius(radius);
    if (gas.density > 0.0f) {
        gas.velocity = FramePointVelocity(planetFrame, worldPosition);
        gas.oxidizerMassDensity = gas.density * m_parameters.oxidizerMassFraction;
        gas.temperatureKelvin = TemperatureAtRadius(radius);
    }
    return gas;
}

float AtmosphereField::SpecificPotentialAtRadius(float radius) const {
    if (!(radius > 0.0f) || !std::isfinite(radius)) {
        return -std::numeric_limits<float>::infinity();
    }
    return -m_parameters.gravitationalParameter / radius;
}
