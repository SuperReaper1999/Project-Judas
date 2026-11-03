#include "CutHydroacousticPressure.h"
#include "CutHydroacousticGraph.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace judas::volume_grid {
namespace {

bool Fail(std::string& error, const char* message) {
    error = message;
    return false;
}

bool Positive(double v) { return v > 0. && std::isfinite(v); }
bool Nonnegative(double v) { return v >= 0. && std::isfinite(v); }

struct Sum {
    double value = 0., correction = 0.;

    void Add(double term) {
        const double next = value + term;
        correction += std::abs(value) >= std::abs(term) ? (value - next) + term : (term - next) + value;
        value = next;
    }

    double Value() const { return value + correction; }
};

struct RegionLinks {
    double gasHead = 0., gasCapacitance = 0.;
};

} // namespace

bool SolveCutHydroacousticPressure(const std::vector<CutHydroacousticRegion>& regions,
                                  const std::vector<CutHydroacousticPort>& ports,
                                  const CutHydroacousticSettings& settings,
                                  CutHydroacousticResult& output, std::string& error) {
    const auto count = regions.size();
    const double scale = settings.waterDensity * settings.gravity;
    const double bulk = settings.gasDensity * settings.soundSpeed * settings.soundSpeed;
    if (count == 0 || count > CutHydroacousticAmbient / 2 ||
        !Positive(settings.dt) || !Positive(settings.volumeTolerance) ||
        !Positive(settings.waterDensity) || !Positive(settings.gasDensity) ||
        !Positive(settings.gravity) || !Positive(settings.soundSpeed) ||
        !Positive(scale) || !Positive(bulk))
        return Fail(error, "invalid hydroacoustic dimensions or material parameters");

    const auto wetNode = [](std::size_t region) { return 2 * region; };
    const auto gasNode = [](std::size_t region) { return 2 * region + 1; };
    std::vector<double> sources(2 * count), groundStorage(2 * count);
    std::vector<Sum> rhsSum(2 * count);
    std::vector<CutHydroacousticGraphLink> edges;
    edges.reserve(ports.size() + count);
    std::vector<RegionLinks> links(count);
    double cellVolume = 0.;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& r = regions[i];
        if (!Nonnegative(r.waterVolume) || !Nonnegative(r.gasVolume) || !Nonnegative(r.gasMass) ||
            !Nonnegative(r.fixedWaterVolume) || !Nonnegative(r.fixedGasVolume) ||
            !Nonnegative(r.interfaceArea) || !std::isfinite(r.surfaceHead) ||
            (r.gasVolume == 0. && r.gasMass != 0.) ||
            (r.gasVolume > 0. && r.gasMass == 0.) ||
            (r.interfaceArea > 0. && !(r.waterVolume > 0. && r.gasVolume > 0.)))
            return Fail(error, "invalid hydroacoustic phase inventory or interface");
        const double waterSource = r.waterVolume - r.fixedWaterVolume;
        const double gasSource = r.gasVolume - r.fixedGasVolume;
        sources[wetNode(i)] = waterSource;
        sources[gasNode(i)] = gasSource;
        rhsSum[wetNode(i)].Add(waterSource);
        rhsSum[gasNode(i)].Add(gasSource);
        cellVolume = std::max({cellVolume, r.waterVolume + r.gasVolume,
                              r.fixedWaterVolume + r.fixedGasVolume});
        auto& owned = links[i];
        if (r.interfaceArea > 0.) {
            const double storage = 2. * r.interfaceArea;
            edges.push_back({wetNode(i), gasNode(i), storage});
            rhsSum[wetNode(i)].Add(storage * r.surfaceHead);
            rhsSum[gasNode(i)].Add(-storage * r.surfaceHead);
        }
        if (r.gasVolume > 0.) {
            const double excess = std::fma(-settings.gasDensity, r.gasVolume, r.gasMass) / settings.gasDensity;
            owned.gasCapacitance = scale * r.gasVolume / bulk;
            owned.gasHead = excess / owned.gasCapacitance;
            groundStorage[gasNode(i)] = 2. * owned.gasCapacitance;
            rhsSum[gasNode(i)].Add(2. * excess);
        }
    }
    for (const auto& port : ports) {
        const bool isWater = port.phase == CutFluidPhase::Water;
        const bool isGas = port.phase == CutFluidPhase::Gas;
        if ((!isWater && !isGas) || port.left >= count || port.left == port.right ||
            (port.right >= count && !(isGas && port.right == CutHydroacousticAmbient)) ||
            !Positive(port.area) || !Positive(port.length) || !std::isfinite(port.speed))
            return Fail(error, "invalid hydroacoustic phase port");
        const double density = isWater ? settings.waterDensity : settings.gasDensity;
        const double conductance = settings.dt * settings.dt * port.area * scale / (2. * density * port.length);
        const double predicted = settings.dt * port.area * port.speed;
        if (!Positive(conductance) || !std::isfinite(predicted))
            return Fail(error, "unrepresentable hydroacoustic port conductance");
        const auto left = isWater ? wetNode(port.left) : gasNode(port.left);
        rhsSum[left].Add(-predicted);
        if (port.right == CutHydroacousticAmbient) {
            groundStorage[left] += conductance;
        } else {
            const auto right = isWater ? wetNode(port.right) : gasNode(port.right);
            rhsSum[right].Add(predicted);
            edges.push_back({left, right, conductance});
        }
    }

    std::vector<double> rhs(rhsSum.size());
    for (std::size_t i = 0; i < rhs.size(); ++i)
        rhs[i] = rhsSum[i].Value();
    CutHydroacousticGraphResult pressure;
    if (!SolveCutHydroacousticGraph(groundStorage, edges, rhs, settings.volumeTolerance, pressure, error))
        return false;

    CutHydroacousticResult next;
    next.waterPressure.resize(count);
    next.gasPressure.resize(count);
    next.waterVolume.resize(count);
    next.gasVolume.resize(count);
    next.gasMass.resize(count);
    next.surfaceHead.resize(count);
    next.portVolume.resize(ports.size());
    next.portSpeed.resize(ports.size());
    next.pressureIterations = pressure.iterations;
    Sum kinetic, storage, wall, exterior, waterError, gasError;
    std::vector<Sum> waterOut(count), gasOut(count);
    for (std::size_t i = 0; i < ports.size(); ++i) {
        const auto& port = ports[i];
        const bool isWater = port.phase == CutFluidPhase::Water;
        const double density = isWater ? settings.waterDensity : settings.gasDensity;
        const double mass = density * port.area * port.length;
        const double left = pressure.pressure[isWater ? wetNode(port.left) : gasNode(port.left)];
        const double right = port.right == CutHydroacousticAmbient ? 0. :
            pressure.pressure[isWater ? wetNode(port.right) : gasNode(port.right)];
        const double impulse = settings.dt * port.area * scale * (left - right);
        const double velocityChange = impulse / mass;
        const double endSpeed = port.speed + velocityChange;
        const double volume = settings.dt * port.area * (port.speed + .5 * velocityChange);
        if (!Positive(mass) || !std::isfinite(endSpeed) || !std::isfinite(volume))
            return Fail(error, "nonfinite hydroacoustic port response");
        next.portVolume[i] = volume;
        next.portSpeed[i] = endSpeed;
        auto& phaseOut = isWater ? waterOut : gasOut;
        phaseOut[port.left].Add(volume);
        if (port.right == CutHydroacousticAmbient)
            exterior.Add(settings.gasDensity * volume);
        else
            phaseOut[port.right].Add(-volume);
        kinetic.Add(mass * velocityChange * (port.speed + .5 * velocityChange));
    }

    for (std::size_t i = 0; i < count; ++i) {
        const auto& r = regions[i];
        const auto& owned = links[i];
        const double waterHead = pressure.pressure[wetNode(i)];
        const double gasHead = pressure.pressure[gasNode(i)];
        const double surface = 2. * r.interfaceArea * ((waterHead - gasHead) - r.surfaceHead);
        const double compression = 2. * owned.gasCapacitance * (gasHead - owned.gasHead);
        next.waterPressure[i] = scale * waterHead;
        next.gasPressure[i] = scale * gasHead;
        next.waterVolume[i] = r.fixedWaterVolume + surface;
        next.gasVolume[i] = r.fixedGasVolume - surface;
        // Face transport, rather than a re-evaluation of a linearized
        // equation of state at a different volume, owns the gas inventory.
        next.gasMass[i] = std::fma(-settings.gasDensity, gasOut[i].Value(), r.gasMass);
        next.surfaceHead[i] = r.interfaceArea > 0. ? r.surfaceHead + surface / r.interfaceArea : r.surfaceHead;
        if (!Nonnegative(next.waterVolume[i]) || !Nonnegative(next.gasVolume[i]) ||
            !Nonnegative(next.gasMass[i]) || !std::isfinite(next.surfaceHead[i]) ||
            ((next.gasVolume[i] == 0.) != (next.gasMass[i] == 0.)))
            return Fail(error, "hydroacoustic response exceeds available phase volume or gas mass");

        const double volumeError = (surface - sources[wetNode(i)]) + waterOut[i].Value();
        const double massError = (compression - surface - sources[gasNode(i)]) + gasOut[i].Value();
        next.maximumVolumeError = std::max({next.maximumVolumeError, std::abs(volumeError), std::abs(massError)});
        waterError.Add(next.waterVolume[i] - r.waterVolume);
        gasError.Add(next.gasMass[i] - r.gasMass);
        if (r.interfaceArea > 0.)
            storage.Add(scale * surface * (r.surfaceHead + .5 * surface / r.interfaceArea));
        if (owned.gasCapacitance > 0.)
            storage.Add(scale * compression * (owned.gasHead + .5 * compression / owned.gasCapacitance));
        wall.Add(next.waterPressure[i] * sources[wetNode(i)]);
        wall.Add(next.gasPressure[i] * sources[gasNode(i)]);
    }
    next.exteriorGasMass = exterior.Value();
    gasError.Add(next.exteriorGasMass);
    next.waterError = waterError.Value();
    next.gasMassError = gasError.Value();
    next.energyChange = kinetic.Value() + storage.Value();
    next.wallWork = wall.Value();
    next.workError = next.energyChange - next.wallWork;
    const double volumeBudget = settings.volumeTolerance + 128. * std::numeric_limits<double>::epsilon() * cellVolume;
    const double workBudget = 1e-10 * (1. + std::abs(next.energyChange) + std::abs(next.wallWork));
    if (!(next.maximumVolumeError <= volumeBudget &&
          std::abs(next.waterError) <= volumeBudget * double(count) &&
          std::abs(next.gasMassError) <= volumeBudget * double(count) * settings.gasDensity &&
          std::abs(next.workError) <= workBudget))
        return Fail(error, "hydroacoustic phase, mass or pressure work ledger did not close");
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
