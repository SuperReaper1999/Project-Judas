#include "CombustionWorld.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "AtmosphereField.h"
#include "ReferenceFrame.h"

namespace {
constexpr double kStefanBoltzmann = 5.670374419e-8; // W/(m^2 K^4)
constexpr double kFourPi = 12.566370614359172;
constexpr double kMinimumDistanceSquared = 1.0e-6;

bool FiniteNonNegative(float value) {
    return std::isfinite(value) && value >= 0.0f;
}

bool ValidMaterial(const CombustibleMaterial& material) {
    return std::isfinite(material.heatCapacityJPerK) &&
           material.heatCapacityJPerK > 0.0f &&
           FiniteNonNegative(material.initialFuelMassKg) &&
           std::isfinite(material.ignitionTemperatureK) &&
           material.ignitionTemperatureK > 0.0f &&
           std::isfinite(material.activationRangeK) &&
           material.activationRangeK > 0.0f &&
           FiniteNonNegative(material.maximumFuelRateKgPerSecond) &&
           FiniteNonNegative(material.heatReleaseJPerKg) &&
           std::isfinite(material.retainedCombustionHeatFraction) &&
           material.retainedCombustionHeatFraction >= 0.0f &&
           material.retainedCombustionHeatFraction <= 1.0f &&
           std::isfinite(material.oxygenRequiredKgPerKgFuel) &&
           material.oxygenRequiredKgPerKgFuel > 0.0f &&
           FiniteNonNegative(material.oxygenTransportSpeedMetersPerSecond) &&
           FiniteNonNegative(material.airflowTransportFactor) &&
           FiniteNonNegative(material.radiativeAreaSquareMeters) &&
           std::isfinite(material.emissivity) &&
           material.emissivity >= 0.0f && material.emissivity <= 1.0f &&
           FiniteNonNegative(material.convectionCoefficientWattsPerSquareMeterKelvin);
}

double FourthPower(double value) {
    const double squared = value * value;
    return squared * squared;
}

struct RadiativePair {
    std::size_t first = 0;
    std::size_t second = 0;
    double exchangeArea = 0.0; // reciprocal m^2 view-area approximation
};
} // namespace

void CombustionWorld::AddBody(BodyHandle body, const CombustibleMaterial& material,
                              float initialTemperatureKelvin) {
    if (!body.IsValid() || !ValidMaterial(material) ||
        !std::isfinite(initialTemperatureKelvin) || initialTemperatureKelvin <= 0.0f) {
        throw std::invalid_argument("CombustionWorld requires a valid body, material, and temperature");
    }
    if (State(body)) {
        throw std::invalid_argument("CombustionWorld cannot register the same body twice");
    }

    ThermalBodyState state;
    state.body = body;
    state.temperatureKelvin = initialTemperatureKelvin;
    state.previousTemperatureKelvin = initialTemperatureKelvin;
    state.remainingFuelMassKg = material.initialFuelMassKg;
    m_materials.push_back(material);
    m_states.push_back(state);
    m_initialTemperatures.push_back(initialTemperatureKelvin);
}

void CombustionWorld::Clear() {
    m_materials.clear();
    m_states.clear();
    m_initialTemperatures.clear();
}

void CombustionWorld::Reset() {
    for (std::size_t i = 0; i < m_states.size(); ++i) {
        const BodyHandle body = m_states[i].body;
        m_states[i] = ThermalBodyState{};
        m_states[i].body = body;
        m_states[i].temperatureKelvin = m_initialTemperatures[i];
        m_states[i].previousTemperatureKelvin = m_initialTemperatures[i];
        m_states[i].remainingFuelMassKg = m_materials[i].initialFuelMassKg;
    }
}

void CombustionWorld::Step(float fixedDeltaTime, const PhysicsWorld& physics,
                            const AtmosphereField& atmosphere,
                            const ReferenceFrame& planetFrame,
                            const RadiantHeater* heater) {
    if (!(fixedDeltaTime > 0.0f) || !std::isfinite(fixedDeltaTime)) {
        throw std::invalid_argument("CombustionWorld requires a finite positive fixed step");
    }
    if (heater && (!(heater->powerWatts >= 0.0f) || !std::isfinite(heater->powerWatts) ||
                   !std::isfinite(heater->worldPosition.x) ||
                   !std::isfinite(heater->worldPosition.y) ||
                   !std::isfinite(heater->worldPosition.z))) {
        throw std::invalid_argument("CombustionWorld heater must have finite position and power");
    }

    const std::size_t count = m_states.size();
    if (count == 0) return;
    std::vector<bool> active(count, false);
    std::vector<glm::vec3> positions(count);
    std::vector<AtmosphereSample> gasSamples(count);
    std::vector<double> thermalPower(count, 0.0);
    std::vector<double> viewedArea(count, 0.0);
    std::vector<double> rawViewedArea(count, 0.0);
    std::vector<RadiativePair> pairs;
    pairs.reserve(count * (count - 1) / 2);

    // Read every body and gas sample before changing any thermal state. The
    // gas's actual frame-point velocity, including rotation, supplies
    // relative airflow; vacuum supplies no world-rest reference velocity.
    for (std::size_t i = 0; i < count; ++i) {
        ThermalBodyState& state = m_states[i];
        state.previousTemperatureKelvin = state.temperatureKelvin;
        state.burnRateKgPerSecond = 0.0f;
        state.heatOutputWatts = 0.0f;
        state.heatToGasWatts = 0.0f;
        state.oxygenSupplyKgPerSecond = 0.0f;
        state.heaterWattsReceived = 0.0f;
        state.pairwiseHeatWattsReceived = 0.0f;
        state.environmentalHeatWattsReceived = 0.0f;
        state.relativeAirspeedMetersPerSecond = 0.0f;
        state.localOxidizerMassDensity = 0.0f;
        if (!physics.IsDynamicBody(state.body)) continue;

        active[i] = true;
        positions[i] = physics.GetTransform(state.body).position;
        gasSamples[i] = atmosphere.Sample(positions[i], planetFrame);
        const AtmosphereSample& gas = gasSamples[i];
        state.localOxidizerMassDensity = gas.oxidizerMassDensity;
        if (gas.density > 0.0f) {
            state.relativeAirspeedMetersPerSecond =
                glm::length(physics.GetLinearVelocity(state.body) - gas.velocity);
        }
    }

    // A symmetric exchange area makes Q_i->j = -Q_j->i. Its 1/r^2
    // dependence allows nearby hot bodies to heat cold ones without
    // adjacency flags or scripted spread. Bound near-field area by both
    // surfaces; then normalize if multiple receivers together would
    // intercept more than an emitter's total area.
    for (std::size_t i = 0; i < count; ++i) {
        if (!active[i]) continue;
        const double areaI = m_materials[i].radiativeAreaSquareMeters;
        if (areaI <= 0.0) continue;
        for (std::size_t j = i + 1; j < count; ++j) {
            if (!active[j]) continue;
            const double areaJ = m_materials[j].radiativeAreaSquareMeters;
            if (areaJ <= 0.0) continue;
            const glm::vec3 displacement = positions[j] - positions[i];
            const double distanceSquared = std::max(
                static_cast<double>(glm::dot(displacement, displacement)),
                kMinimumDistanceSquared);
            const double exchangeArea = std::min({areaI, areaJ,
                                                  areaI * areaJ / (kFourPi * distanceSquared)});
            if (exchangeArea <= 0.0) continue;
            pairs.push_back(RadiativePair{i, j, exchangeArea});
            rawViewedArea[i] += exchangeArea;
            rawViewedArea[j] += exchangeArea;
        }
    }
    for (RadiativePair& pair : pairs) {
        const double areaI = m_materials[pair.first].radiativeAreaSquareMeters;
        const double areaJ = m_materials[pair.second].radiativeAreaSquareMeters;
        const double scale = std::min({1.0,
            rawViewedArea[pair.first] > 0.0 ? areaI / rawViewedArea[pair.first] : 1.0,
            rawViewedArea[pair.second] > 0.0 ? areaJ / rawViewedArea[pair.second] : 1.0});
        pair.exchangeArea *= scale;
        viewedArea[pair.first] += pair.exchangeArea;
        viewedArea[pair.second] += pair.exchangeArea;

        const double emissivity = std::sqrt(
            static_cast<double>(m_materials[pair.first].emissivity) *
            static_cast<double>(m_materials[pair.second].emissivity));
        const double watts = emissivity * kStefanBoltzmann * pair.exchangeArea *
            (FourthPower(m_states[pair.first].temperatureKelvin) -
             FourthPower(m_states[pair.second].temperatureKelvin));
        thermalPower[pair.first] -= watts;
        thermalPower[pair.second] += watts;
        m_states[pair.first].pairwiseHeatWattsReceived -= static_cast<float>(watts);
        m_states[pair.second].pairwiseHeatWattsReceived += static_cast<float>(watts);
    }

    // The heater's finite input power is intercepted geometrically. If
    // several bodies overlap its point view, normalize the fractions so
    // their combined delivered energy never exceeds external input.
    if (heater && heater->powerWatts > 0.0f) {
        std::vector<double> interception(count, 0.0);
        double totalInterception = 0.0;
        for (std::size_t i = 0; i < count; ++i) {
            if (!active[i]) continue;
            const glm::vec3 displacement = positions[i] - heater->worldPosition;
            const double distanceSquared = std::max(
                static_cast<double>(glm::dot(displacement, displacement)),
                kMinimumDistanceSquared);
            interception[i] = std::min(1.0,
                static_cast<double>(m_materials[i].radiativeAreaSquareMeters) /
                    (kFourPi * distanceSquared));
            totalInterception += interception[i];
        }
        const double normalization = totalInterception > 1.0 ? 1.0 / totalInterception : 1.0;
        for (std::size_t i = 0; i < count; ++i) {
            const double watts = heater->powerWatts * interception[i] * normalization;
            thermalPower[i] += watts;
            m_states[i].heaterWattsReceived = static_cast<float>(watts);
        }
    }

    // Thermal chemistry uses the pre-step temperature. A smooth activation
    // above ignition temperature avoids a binary fire mode. Oxygen supply
    // comes from the sampled local gas density and relative flow; with no
    // oxidizer, the fuel rate and heat release are exactly zero.
    for (std::size_t i = 0; i < count; ++i) {
        if (!active[i]) continue;
        ThermalBodyState& state = m_states[i];
        const CombustibleMaterial& material = m_materials[i];
        const AtmosphereSample& gas = gasSamples[i];
        const double activation = std::clamp(
            (static_cast<double>(state.previousTemperatureKelvin) -
             material.ignitionTemperatureK) / material.activationRangeK,
            0.0, 1.0);
        const double oxygenSupply = gas.oxidizerMassDensity *
            material.radiativeAreaSquareMeters *
            (material.oxygenTransportSpeedMetersPerSecond +
             material.airflowTransportFactor * state.relativeAirspeedMetersPerSecond);
        state.oxygenSupplyKgPerSecond = static_cast<float>(oxygenSupply);
        const double kineticRate = material.maximumFuelRateKgPerSecond * activation;
        const double oxygenLimitedRate = oxygenSupply / material.oxygenRequiredKgPerKgFuel;
        const double fuelLimitedRate = state.remainingFuelMassKg / fixedDeltaTime;
        const double requestedBurnRate = std::max(0.0,
            std::min({kineticRate, oxygenLimitedRate, fuelLimitedRate}));
        const double previousFuel = state.remainingFuelMassKg;
        state.remainingFuelMassKg = std::max(
            0.0, previousFuel - requestedBurnRate * fixedDeltaTime);
        // The actual representable decrement is the sole chemical-energy
        // source. In particular, a thin-air rate may be below one float ULP
        // of a coating per step; heat must never be produced without fuel.
        const double burnRate =
            (previousFuel - state.remainingFuelMassKg) / fixedDeltaTime;
        state.burnRateKgPerSecond = static_cast<float>(burnRate);
        state.heatOutputWatts = static_cast<float>(burnRate * material.heatReleaseJPerKg);
        state.heatToGasWatts = state.heatOutputWatts *
            (1.0f - material.retainedCombustionHeatFraction);
        thermalPower[i] += state.heatOutputWatts *
            material.retainedCombustionHeatFraction;

        // Convective exchange grows with local gas density and relative
        // speed. Vacuum leaves only thermal radiation to cold surroundings.
        const double densityRatio = gas.density > 0.0f
            ? static_cast<double>(gas.density) / atmosphere.Parameters().referenceDensity
            : 0.0;
        const double convectionCoefficient =
            material.convectionCoefficientWattsPerSquareMeterKelvin * densityRatio *
            (1.0 + std::sqrt(static_cast<double>(state.relativeAirspeedMetersPerSecond)));
        const double convectiveWatts = convectionCoefficient *
            material.radiativeAreaSquareMeters *
            (static_cast<double>(gas.temperatureKelvin) - state.previousTemperatureKelvin);
        const double openArea = std::max(0.0,
            static_cast<double>(material.radiativeAreaSquareMeters) - viewedArea[i]);
        const double radiativeWatts = material.emissivity * kStefanBoltzmann * openArea *
            (FourthPower(gas.temperatureKelvin) -
             FourthPower(state.previousTemperatureKelvin));
        const double environmentalWatts = convectiveWatts + radiativeWatts;
        state.environmentalHeatWattsReceived = static_cast<float>(environmentalWatts);
        thermalPower[i] += environmentalWatts;
    }

    // A common constant exposed-layer heat capacity converts net energy
    // into temperature. Body mass/inertia and motion are untouched. The
    // floor is a numerical guard for extreme user-supplied coefficients,
    // not an ignition/extinguishing rule; ordinary calibrated steps stay
    // far from it.
    for (std::size_t i = 0; i < count; ++i) {
        if (!active[i]) continue;
        const double temperature = m_states[i].previousTemperatureKelvin +
            thermalPower[i] * fixedDeltaTime / m_materials[i].heatCapacityJPerK;
        m_states[i].temperatureKelvin = static_cast<float>(
            std::max(1.0, std::isfinite(temperature) ? temperature : 1.0));
    }
}

const ThermalBodyState* CombustionWorld::State(BodyHandle body) const {
    for (const ThermalBodyState& state : m_states) {
        if (state.body.id == body.id) return &state;
    }
    return nullptr;
}

float CombustionWorld::PresentedTemperature(BodyHandle body, float alpha) const {
    const ThermalBodyState* state = State(body);
    if (!state) return 0.0f;
    const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
    return state->previousTemperatureKelvin +
           (state->temperatureKelvin - state->previousTemperatureKelvin) * clampedAlpha;
}
