#include "CutHydroacousticPressure.h"
#include "CutHydroacousticStep.h"
#include "CutRegionGeometry.h"

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace {

using namespace judas::volume_grid;
struct Checks {
    int count = 0, failures = 0;

    void Expect(bool condition, const char* label, double value) {
        ++count;
        failures += !condition;
        std::cout << (condition ? "PASS " : "FAIL ") << label << " value=" << value << '\n';
    }
};

void Pair(Checks& checks, int up, double dt, double dryPortFraction) {
    constexpr double g = 9.81, waterDensity = 1000., gasDensity = 1.2, soundSpeed = 343.;
    constexpr double area = 1., compression = .001, baseWater = .5;
    const double scale = waterDensity * g;
    GridLayout layout{{1, 1, 1}, 1.};
    layout.cells[(up + 1) % 3] = 2;
    CutRegionChart chart;
    CutRegionGeometry geometry;
    std::string error;
    if (!PrepareCutRegionChart(layout, {}, {}, {}, {}, chart, error) ||
        !BuildCutRegionGeometry(chart, {}, geometry, error) || geometry.faces.size() != 1) {
        checks.Expect(false, "two chambers share one actual face", 0.);
        return;
    }
    const auto& face = geometry.faces.front();
    const CutRegionWaterPlane water{{up, false}, .5};
    const double Aw = MeasureCutRegionSharedPhase(face, geometry.motion, water, water, true, true).start;
    // Manufactured impedance controls retain the same storage geometry.
    // The actual full port is the exact jointly dry portion of that face.
    const double Ag = dryPortFraction * MeasureCutRegionSharedPhase(face, geometry.motion, water, water, false, false).start;
    const double L = face.length;
    const double V = MeasureCutRegionPhaseVolumes(chart.regions[0], geometry.motion,
        chart.regions[0].lo, chart.regions[0].hi, water)[1];
    const double capacitance = scale * V / (gasDensity * soundSpeed * soundSpeed);
    const double Gw = dt * dt * Aw * g / (2. * L);
    const double Gg = dt * dt * Ag * scale / (2. * gasDensity * L);
    std::vector<CutHydroacousticRegion> cells{
        {baseWater, V, gasDensity * V, baseWater, V - compression, area, 0.},
        {baseWater, V, gasDensity * V, baseWater, V, area, 0.}};
    std::vector<CutHydroacousticPort> ports{{0, 1, CutFluidPhase::Water, Aw, L, 0.}};
    if (Ag > 0.)
        ports.push_back({0, 1, CutFluidPhase::Gas, Ag, L, 0.});
    CutHydroacousticResult result;
    if (!SolveCutHydroacousticPressure(cells, ports, {dt}, result, error)) {
        std::cerr << error << '\n';
        checks.Expect(false, "midpoint pressure solves the two chambers", 0.);
        return;
    }
    const double qw = result.portVolume[0], qg = Ag > 0. ? result.portVolume[1] : 0.;
    const double volumeLeft = result.waterVolume[0] - baseWater;
    const double volumeRight = result.waterVolume[1] - baseWater;
    const double massLeft = result.gasMass[0] - cells[0].gasMass;
    const double massRight = result.gasMass[1] - cells[1].gasMass;
    const double common = compression / (4. * capacitance);
    const double differential = compression / (4. * (capacitance + Gg + area * Gw / (area + Gw)));
    const double pressureError = std::max(std::abs(result.gasPressure[0] - scale * (common + differential)),
                                          std::abs(result.gasPressure[1] - scale * (common - differential)));
    const double waterError = std::max(std::abs(volumeLeft + qw), std::abs(volumeRight - qw));
    const double gasError = std::max(std::abs(massLeft + gasDensity * qg),
                                    std::abs(massRight - gasDensity * qg));
    const double waterVelocity = 2. * qw / (Aw * dt);
    const double gasVelocity = Ag > 0. ? 2. * qg / (Ag * dt) : 0.;
    const double waterPotential = .5 * scale * (volumeLeft*volumeLeft + volumeRight*volumeRight) / area;
    const double gasCompression = 2. * capacitance *
        (result.gasPressure[0] * result.gasPressure[0] + result.gasPressure[1] * result.gasPressure[1]) / scale;
    const double kinetic = .5 * L * (waterDensity * Aw * waterVelocity*waterVelocity +
                                    gasDensity * Ag * gasVelocity*gasVelocity);
    const double wallArea = .25, displacement = compression / wallArea;
    const double wallForce = result.gasPressure[0] * wallArea;
    const double work = wallForce * displacement;
    const double workResidual = (waterPotential + gasCompression + kinetic) - work;
    checks.Expect(Aw == .5 && V == .5 && pressureError < 2e-10,
                  "analytic symmetric and antisymmetric pressures agree", pressureError);
    checks.Expect(waterError < 1e-16 && gasError < 1e-16 && baseWater + volumeLeft > 0. &&
                  baseWater + volumeRight < 1.,
                  "paired water volume and gas mass close independently", std::max(waterError, gasError));
    checks.Expect(std::abs(workResidual) < 1e-13 && std::abs(workResidual - work) > 1e-3 &&
                  std::abs(result.workError) < 1e-13,
                  "midpoint wall reaction closes energy and endpoint traction fails the control", workResidual);
    std::cout << "MEASURE axis=" << up << " dt=" << dt << " dry_area=" << Ag << " gas_delta_Pa="
              << 2.*(result.gasPressure[0] - result.gasPressure[1]) << " water_transfer_m3=" << qw
              << " gas_transfer_kg=" << gasDensity*qg << " wall_work_J=" << work << '\n';
}

void VentContinuation(Checks& checks) {
    constexpr CutHydroacousticSettings settings{1. / 60.};
    constexpr double startingVolume = .5, squeeze = .001, area = .2, length = .1;
    const double scale = settings.waterDensity * settings.gravity;
    std::vector<CutHydroacousticRegion> cells{{0., startingVolume,
        startingVolume * settings.gasDensity, 0., startingVolume - squeeze, 0., 0.}};
    std::vector<CutHydroacousticPort> ports{{0, CutHydroacousticAmbient, CutFluidPhase::Gas, area, length, 0.}};
    CutHydroacousticResult result;
    std::string error;
    if (!SolveCutHydroacousticPressure(cells, ports, settings, result, error)) {
        std::cerr << error << '\n';
        checks.Expect(false, "compression in a vented chamber solves", 0.);
        return;
    }
    const double compliance = scale * startingVolume /
        (settings.gasDensity * settings.soundSpeed * settings.soundSpeed);
    const double conductance = settings.dt * settings.dt * area * scale / (2. * settings.gasDensity * length);
    const double expected = squeeze * scale / (2. * compliance + conductance);
    checks.Expect(std::abs(result.gasPressure[0] - expected) < 1e-12 &&
                  result.exteriorGasMass > 0. && std::abs(result.gasMassError) < 1e-16,
                  "an exterior opening transfers finite gas mass at ambient pressure", result.gasPressure[0]);
    const auto first = result;
    cells[0] = {0., first.gasVolume[0], first.gasMass[0], 0., first.gasVolume[0], 0., 0.};
    ports[0].speed = first.portSpeed[0];
    checks.Expect(SolveCutHydroacousticPressure(cells, ports, settings, result, error) &&
                  std::abs(result.gasMassError) < 1e-16 && std::abs(result.workError) < 1e-13 &&
                  result.wallWork == 0. && result.portVolume[0] != 0.,
                  "a subsequent interval carries gas mass and port momentum", result.portVolume[0]);

    const auto retained = result;
    cells[0] = {1., 0., 0., .99, 0., 0., 0.};
    checks.Expect(!SolveCutHydroacousticPressure(cells, {}, settings, result, error) &&
                  result.portVolume == retained.portVolume && result.waterVolume == retained.waterVolume,
                  "an incompressible sealed cut cannot discard water on compression", 0.);
    cells[0].waterVolume = std::numeric_limits<double>::infinity();
    checks.Expect(!SolveCutHydroacousticPressure(cells, {}, settings, result, error) &&
                  result.gasPressure == retained.gasPressure,
                  "invalid input preserves the previous pressure and state", 0.);
}

void MovingPhaseWall(Checks& checks, int up, bool wet) {
    const int travelAxis = (up + 1) % 3;
    GridLayout layout{{1, 1, 1}, 1.};
    layout.cells[travelAxis] = 2;
    BoxMotion body{7, {.25, .25, .25}, {.75, .75, .75}, {}};
    GridVec maximum{}, offset{}, travel{};
    maximum[travelAxis] = .0625;
    offset[travelAxis] = .03125;
    travel[travelAxis] = 1. / 4096.;
    CutRegionChart chart;
    CutRegionGeometry geometry;
    std::string error;
    if (!PrepareCutRegionChart(layout, {}, body, {}, maximum, chart, error) ||
        !BuildCutRegionGeometry(chart, offset, travel, geometry, error)) {
        std::cerr << error << '\n';
        checks.Expect(false, "moving two-phase wall chart builds", 0.);
        return;
    }
    constexpr CutHydroacousticSettings settings{1. / 240.};
    const double height = wet ? .5625 : 0.;
    const CutRegionWaterPlane plane{{up, false}, height};
    CutHydroacousticState state;
    for (const auto& region : chart.regions) {
        const auto [water, gas] = MeasureCutRegionPhaseVolumes(region, geometry.motion, region.lo, region.hi, plane);
        state.waterVolume.push_back(water);
        state.gasMass.push_back(settings.gasDensity * gas);
    }
    CutHydroacousticStep step;
    const bool solved = AdvanceCutHydroacoustic(chart, geometry, state, {}, up, height, settings, step, error);
    if (!solved) {
        std::cerr << error << '\n';
        checks.Expect(false, "moving wall with exact phase sources solves", 0.);
        return;
    }
    const auto& water = step.loads[0];
    const auto& gas = step.loads[1];
    const double pressureForce = (water.empty() ? 0. : water.front().force[travelAxis]) +
        (gas.empty() ? 0. : gas.front().force[travelAxis]);
    checks.Expect((wet ? water.size() == 1 : water.empty()) && gas.size() == 1 &&
                  pressureForce < 0. && step.wallSweepError < 1e-16,
                  "separate wet and dry wall patches recover an opposing physical pressure force", pressureForce);
    checks.Expect(std::abs(step.pressure.waterError) < 1e-14 &&
                  std::abs(step.pressure.gasMassError) < 1e-14 &&
                  std::abs(step.reactionWorkError) < 1e-13 && step.pressure.wallWork > 0.,
                  "moving geometry, material ledgers and pressure reaction close", step.reactionWorkError);

    const auto oldMass = step.state.gasMass;
    offset[travelAxis] += travel[travelAxis];
    if (!BuildCutRegionGeometry(chart, offset, travel, geometry, error)) {
        checks.Expect(false, "second geometric endpoint builds", 0.);
        return;
    }
    CutHydroacousticStep second;
    const bool continued = AdvanceCutHydroacoustic(chart, geometry, step.state, {}, up, height, settings,
                                                  second, error);
    if (!continued) std::cerr << error << '\n';
    checks.Expect(continued && std::abs(second.pressure.gasMassError) < 1e-14 &&
                  std::abs(second.reactionWorkError) < 1e-13 && second.state.gasMass != oldMass,
                  "next moving step carries physical pressure, phase and port state",
                  continued ? second.pressure.workError : 0.);
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    Checks checks;
    for (const int up : {1, 2})
        for (const double dt : {1. / 60., 1. / 240.})
            for (const double dry : {0., 1e-16, 1.})
                Pair(checks, up, dt, dry);
    VentContinuation(checks);
    MovingPhaseWall(checks, 1, false);
    MovingPhaseWall(checks, 1, true);
    MovingPhaseWall(checks, 2, true);
    std::cout << "SUMMARY " << checks.count << " checks " << checks.failures << " failures\n";
    return checks.failures ? 1 : 0;
}
