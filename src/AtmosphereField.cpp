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
        !(parameters.polytropicExponent < 2.0f)) {
        throw std::invalid_argument("AtmosphereField requires finite positive radii, gravity, "
                                    "density, and polytropic exponent in (1, 2)");
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
    m_inversePotentialSpan = 1.0f /
        (1.0f / parameters.referenceRadius - 1.0f / parameters.topRadius);
    if (!PositiveFinite(m_referencePressure) || !PositiveFinite(m_inversePotentialSpan)) {
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
    if (gas.density > 0.0f) gas.velocity = FramePointVelocity(planetFrame, worldPosition);
    return gas;
}

float AtmosphereField::SpecificPotentialAtRadius(float radius) const {
    if (!(radius > 0.0f) || !std::isfinite(radius)) {
        return -std::numeric_limits<float>::infinity();
    }
    return -m_parameters.gravitationalParameter / radius;
}
