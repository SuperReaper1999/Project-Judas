#include "ExposureVolumePressure.h"
#include "GridHalfCellGeometry.h"
#include "GridPressureReaction.h"

#include <cstdio>
#include <numeric>

namespace {

using namespace judas::volume_grid;

struct Checks {
    int total = 0, failed = 0;
    void Expect(bool pass, const char* label, double value = 0) {
        ++total;
        if (!pass)
            ++failed;
        std::printf("%s %s %.17g\n", pass ? "PASS" : "FAIL", label, value);
    }
};

void PistonPressure(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, .25};
    const BoxMotion piston{1, {.25, .5, .25}, {.75, .875, .75}, {0., .04, 0.}};
    constexpr double dt = 1. / 60.;
    MovingGridGeometry geometry;
    GridHalfCellGeometry halves;
    std::string error;
    const bool built = BuildMovingGridGeometry(layout, {piston}, geometry, error) &&
        BuildGridHalfCellGeometry(layout, {piston}, 1, halves, error);
    checks.Expect(built, built ? "piston creates a thin water cavity" : error.c_str());
    if (!built)
        return;

    MovingVolumeState water;
    water.water = geometry.startCapacity;
    for (int axis = 0; axis < 3; ++axis)
        water.velocity[axis].resize(layout.FaceCount(axis));
    auto air = water;
    std::fill(air.water.begin(), air.water.end(), 0.);
    MovingVolumeStep wet, dry;
    const bool solved = AdvanceExposureVolumePressure(layout, geometry, water, dt, true, wet, error) &&
        AdvanceExposureVolumePressure(layout, geometry, air, dt, false, dry, error);
    checks.Expect(solved, solved ? "same geometry admits pressure with newly exposed water and air"
                                : error.c_str());
    if (!solved)
        return;

    const auto sum = [](const auto& values) { return std::accumulate(values.begin(), values.end(), 0.); };
    const auto loads = AccumulateGridPressureReaction(geometry.averagePatches, wet.pressure);
    const double workError = loads.size() == 1
        ? wet.pressureFluidWork + loads.front().force[1] * piston.translation[1] : INFINITY;
    const double metricError = wet.pressureFluidWork - wet.pressureKineticWork - wet.pressureCorrectionEnergy;
    checks.Expect(loads.size() == 1 && loads.front().owner == piston.owner &&
                  loads.front().force[1] < 0 && std::abs(workError) < 1e-10,
                  "agglomerated pressure reaction closes with the real swept boundary", workError);
    checks.Expect(wet.pressureFluidWork > .1 && std::abs(metricError) < 1e-9 &&
                  std::abs(wet.pressureFluidWork - 1000. * dry.pressureFluidWork) < 1e-9,
                  "fluid work uses the same pressure metric and actual phase density", metricError);
    const double volumeError = sum(wet.state.water) - sum(water.water);
    checks.Expect(std::abs(volumeError) < 1e-12 && sum(dry.state.water) == 0. && wet.residual < 1e-12,
                  "recovered transfers close every primal cell and retain both inventories", volumeError);

    double maximumMidpoint = 0., maximumOwnedError = 0., recoveredInlet = 0., sideInlet = 0.;
    for (int z = 1; z < 3; ++z) {
        for (int x = 1; x < 3; ++x) {
            const int y = 2;
            const auto cell = layout.Cell(x, y, z);
            const auto low = layout.Face(1, x, y, z), high = layout.Face(1, x, y+1, z);
            const auto left = layout.Face(0, x, y, z), right = layout.Face(0, x+1, y, z);
            const auto near = layout.Face(2, x, y, z), far = layout.Face(2, x, y, z+1);
            HalfCellInflow inflow{wet.flux[1][low], wet.flux[1][high], {}};
            // All transverse openings of these newborn cells lie below the
            // midpoint. The upper half is completely solid throughout.
            const bool lowerOnly = halves.transverseAperture[0][left][1] == 0. &&
                halves.transverseAperture[0][right][1] == 0. &&
                halves.transverseAperture[2][near][1] == 0. &&
                halves.transverseAperture[2][far][1] == 0.;
            inflow.netTransverse[0] = wet.flux[0][left] - wet.flux[0][right] +
                wet.flux[2][near] - wet.flux[2][far];
            double midpoint = 1.;
            const bool balanced = BalanceGridHalfCell(halves, cell, inflow, midpoint, error);
            checks.Expect(lowerOnly && balanced, balanced ? "pressure actually fills only the exposed half"
                                                          : error.c_str());
            maximumMidpoint = std::max(maximumMidpoint, std::abs(midpoint));
            maximumOwnedError = std::max(maximumOwnedError,
                std::abs(wet.state.water[cell] - halves.endCapacity[cell][0]));
            recoveredInlet += inflow.axialLow;
            sideInlet += inflow.netTransverse[0];
        }
    }
    constexpr double cavityVolume = .5 * .5 * .04;
    checks.Expect(maximumMidpoint == 0. && maximumOwnedError < 1e-13 && recoveredInlet > 0. &&
                  sideInlet > 0. && std::abs(recoveredInlet + sideInlet - cavityVolume) < 1e-13,
                  "conservative axial and lateral inlets supply the exact new cavity volume",
                  recoveredInlet + sideInlet);
    auto bad = water;
    bad.water[0] *= .5;
    const auto retained = wet.state.water;
    const bool accepted = AdvanceExposureVolumePressure(layout, geometry, bad, dt, true, wet, error);
    checks.Expect(!accepted && retained == wet.state.water,
                  "unsupported mixed inputs leave the last pressure result intact");
}

void RefinedPistonExposure(Checks& checks) {
    const GridLayout layout{{8, 8, 8}, .125};
    const BoxMotion piston{1, {.25, .5, .25}, {.75, .875, .75}, {0., .004, 0.}};
    constexpr double dt = 1. / 600.;
    MovingGridGeometry geometry;
    std::string error;
    if (!BuildMovingGridGeometry(layout, {piston}, geometry, error)) {
        checks.Expect(false, error.c_str());
        return;
    }
    MovingVolumeState water;
    water.water = geometry.startCapacity;
    for (int axis = 0; axis < 3; ++axis)
        water.velocity[axis].resize(layout.FaceCount(axis));
    MovingVolumeStep pressure;
    const bool old = AdvanceMovingVolume(layout, geometry, water, dt, {}, pressure, error);
    checks.Expect(!old && error.find("exceeds donor capacity") != std::string::npos,
                  "unmerged pressure asks an initially solid cell for outgoing material");
    const bool solved = AdvanceExposureVolumePressure(layout, geometry, water, dt, true, pressure, error);
    checks.Expect(solved, solved ? "grouped pressure supplies the new cells from their material owners"
                                : error.c_str());
    if (!solved)
        return;
    double opened = 0, errorVolume = 0;
    for (std::size_t cell = 0; cell < layout.CellCount(); ++cell) {
        if (geometry.startCapacity[cell] == 0.)
            opened += pressure.state.water[cell];
        errorVolume = std::max(errorVolume,
            std::abs(pressure.state.water[cell] - geometry.endCapacity[cell]));
    }
    constexpr double expected = .5 * .5 * .004;
    checks.Expect(std::abs(opened - expected) < 1e-13 && errorVolume < 1e-13 && pressure.residual < 1e-13,
                  "every newly opened cell gains its exact fully wet endpoint inventory", opened);
}

} // namespace

int main() {
    Checks checks;
    PistonPressure(checks);
    RefinedPistonExposure(checks);
    std::printf("SUMMARY exposed-volume pressure %d checks %d failures\n", checks.total, checks.failed);
    return checks.failed ? 1 : 0;
}
