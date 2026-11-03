#include "CutHydroacousticStep.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace judas::volume_grid {
namespace {

bool Fail(std::string& error, const char* message) {
    error = message;
    return false;
}

struct PortOwner {
    std::size_t index;
    bool vent;
};

} // namespace

bool AdvanceCutHydroacoustic(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                             const CutHydroacousticState& state, const std::vector<CutRegionGasVent>& vents,
                             int upAxis, double referenceHeight, const CutHydroacousticSettings& settings,
                             CutHydroacousticStep& output, std::string& error) {
    const auto count = chart.regions.size();
    const auto faceCount = geometry.faces.size();
    if (count == 0 || (vents.size() && count > CutHydroacousticAmbient / vents.size()) ||
        state.waterVolume.size() != count || state.gasMass.size() != count ||
        (!state.faceSpeed.empty() && state.faceSpeed.size() != faceCount) ||
        (!state.ventSpeed.empty() && state.ventSpeed.size() != count * vents.size()) ||
        !std::isfinite(referenceHeight))
        return Fail(error, "hydroacoustic state does not match cut-region geometry");

    std::vector<CutRegionWaterPlane> planes;
    if (!ReconstructCutRegionWaterPlanes(chart, geometry, upAxis, state.waterVolume,
                                         state.interfaces, planes, error))
        return false;
    CutRegionFixedPhaseGeometry fixed;
    if (!MeasureCutRegionFixedPhaseGeometry(chart, geometry, planes, fixed, error))
        return false;
    // Validate actual dry owners, including a pocket cut in two by a
    // translating solid. Positive geometric connectivity is never collapsed
    // to one pressure or identified with the exterior pressure datum.
    CutRegionGasTopology dry;
    if (!BuildCutRegionGasTopology(chart, geometry, planes, vents, dry, error))
        return false;

    const double h = chart.layout.h;
    const double geometricTolerance = 256. * std::numeric_limits<double>::epsilon() * h * h * h;
    std::vector<CutHydroacousticRegion> cells;
    cells.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& plane = planes[i];
        const auto& start = fixed.startVolumes[i];
        const auto& end = fixed.endVolumes[i];
        if (std::abs(start[0] - state.waterVolume[i]) > geometricTolerance)
            return Fail(error, "hydroacoustic phase trace disagrees with its starting inventory");
        const bool mixed = start[0] > 0. && start[1] > 0.;
        if (mixed && (plane.orientation.axis != upAxis || plane.orientation.wetAbove))
            return Fail(error, "hydroacoustic storage requires a stable gravity-aligned phase surface");
        const double area = mixed
            ? MeasureCutRegionInterfaceArea(chart.regions[i], geometry.endpoint, plane) : 0.;
        if (mixed && !(area > 0.))
            return Fail(error, "mixed cut has no supported hydroacoustic interface");
        cells.push_back({state.waterVolume[i], start[1], state.gasMass[i], end[0], end[1],
                         area, mixed ? plane.RelativeTo(referenceHeight) : 0.});
    }
    std::vector<CutHydroacousticPort> ports;
    std::vector<PortOwner> owners;
    for (std::size_t i = 0; i < faceCount; ++i) {
        const auto& face = geometry.faces[i];
        const auto& left = planes[face.left];
        const auto& right = planes[face.right];
        const auto wet = MeasureCutRegionSharedPhase(face, geometry.motion, left, right, true, true);
        const auto gas = MeasureCutRegionSharedPhase(face, geometry.motion, left, right, false, false);
        if (face.axis == upAxis && wet.mean == 0. && gas.mean == 0. && face.meanArea > 0.)
            return Fail(error, "hydroacoustic phase interface lies on a shared face");
        const std::array apertures{wet, gas};
        for (int material = 0; material < 2; ++material) {
            if (apertures[material].mean == 0.)
                continue;
            ports.push_back({face.left, face.right, material == 0 ? CutFluidPhase::Water : CutFluidPhase::Gas,
                             apertures[material].mean, face.length,
                             state.faceSpeed.empty() ? 0. : state.faceSpeed[i][material]});
            owners.push_back({i, false});
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto& region = chart.regions[i];
        for (std::size_t j = 0; j < vents.size(); ++j) {
            const auto& vent = vents[j];
            const double area = MeasureCutRegionGasVent(region, geometry.motion, vent, planes[i]).mean;
            if (area == 0.)
                continue;
            const double length = .5 * (region.hi[vent.axis] - region.lo[vent.axis]);
            const auto index = i * vents.size() + j;
            ports.push_back({i, CutHydroacousticAmbient, CutFluidPhase::Gas, area, length,
                             state.ventSpeed.empty() ? 0. : state.ventSpeed[index]});
            owners.push_back({index, true});
        }
    }

    CutHydroacousticStep next;
    if (!SolveCutHydroacousticPressure(cells, ports, settings, next.pressure, error))
        return false;
    next.state.waterVolume = next.pressure.waterVolume;
    next.state.gasMass = next.pressure.gasMass;
    next.state.interfaces = state.interfaces;
    next.state.faceSpeed.resize(faceCount);
    next.state.ventSpeed.resize(count * vents.size());
    std::vector<std::array<double, 2>> departures(count);
    for (std::size_t i = 0; i < ports.size(); ++i) {
        const auto& port = ports[i];
        const auto& owner = owners[i];
        const auto material = port.phase == CutFluidPhase::Water ? 0 : 1;
        const double volume = next.pressure.portVolume[i];
        const auto donor = volume >= 0. ? port.left : port.right;
        if (donor != CutHydroacousticAmbient)
            departures[donor][material] += std::abs(volume);
        if (owner.vent)
            next.state.ventSpeed[owner.index] = next.pressure.portSpeed[i];
        else
            next.state.faceSpeed[owner.index][material] = next.pressure.portSpeed[i];
    }
    const double roundoff = 128. * std::numeric_limits<double>::epsilon();
    for (std::size_t i = 0; i < count; ++i) {
        const auto& cell = cells[i];
        const std::array inventory{cell.waterVolume, cell.gasMass / settings.gasDensity};
        for (int material = 0; material < 2; ++material)
            if (!(departures[i][material] - inventory[material] <= roundoff * inventory[material]))
                return Fail(error, "hydroacoustic pressure spends an unsupported phase donor");

        const double heightChange = next.pressure.surfaceHead[i] - cell.surfaceHead;
        auto predicted = planes[i];
        predicted.remainder += heightChange;
        const auto& region = chart.regions[i];
        const auto represented = MeasureCutRegionPhaseVolumes(region, geometry.endpoint, region.lo, region.hi, predicted);
        if (!(std::abs(represented[0] - next.state.waterVolume[i]) <= geometricTolerance &&
              std::abs(represented[1] - next.pressure.gasVolume[i]) <= geometricTolerance))
            return Fail(error, "linear hydroacoustic storage crosses a cut geometry event");
    }
    next.wallSweepError = fixed.maximumSweepError;
    try {
        next.loads[0] = AccumulateGridPressureReaction(fixed.wallPatches[0], next.pressure.waterPressure);
        next.loads[1] = AccumulateGridPressureReaction(fixed.wallPatches[1], next.pressure.gasPressure);
    } catch (const std::invalid_argument& invalid) {
        error = invalid.what();
        return false;
    }
    double work = 0.;
    if (geometry.motion)
        for (const auto& phase : next.loads)
            for (const auto& load : phase)
                work += std::inner_product(load.force.begin(), load.force.end(), geometry.motion->translation.begin(), 0.);
    next.reactionWorkError = work + next.pressure.wallWork;
    if (!(std::abs(next.reactionWorkError) <= 1e-10 * (1. + std::abs(work) + std::abs(next.pressure.wallWork))))
        return Fail(error, "hydroacoustic wall reaction disagrees with pressure work");
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
