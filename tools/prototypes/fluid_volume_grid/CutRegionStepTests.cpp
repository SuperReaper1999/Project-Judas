#include "CutRegionStep.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace {

using namespace judas::volume_grid;

struct Checks {
    unsigned total = 0, failed = 0;

    void Expect(bool pass, const std::string& label, double measure = 0.) {
        ++total;
        failed += !pass;
        std::cout << (pass ? "PASS " : "FAIL ") << label << ' ' << measure << '\n';
    }
};

double Sum(const std::vector<double>& values) {
    return std::accumulate(values.begin(), values.end(), 0.);
}

double Max(const GridVec& values) {
    return std::max({std::abs(values[0]), std::abs(values[1]), std::abs(values[2])});
}

CutRegionState FlatWater(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                         int up, double height) {
    CutRegionState state;
    for (const auto& region : chart.regions) {
        auto upper = region.hi;
        upper[up] = std::min(upper[up], height);
        state.water.push_back(CutRegionClippedVolume(region, geometry.motion, region.lo, upper));
    }
    state.velocity.resize(state.water.size());
    return state;
}

void PressureGraph(Checks& checks) {
    constexpr double transfer = 3e-5, tolerance = 1e-18;
    const std::vector<CutRegionPressureFace> faces{{0, 1, 1e-6, 0.}, {1, 2, 1e-29, 0., true}};
    const std::vector<double> source{transfer, -transfer + 1e-19, 0.};
    CutRegionPressure output;
    std::string error;
    const bool solved = ProjectCutRegionPressure(source, faces, .125 * .125 * .125, tolerance, output, error);
    checks.Expect(solved, "near-disconnected pressure graph " + error);
    if (!solved)
        return;
    const double expectedWork = transfer * transfer / faces[0].conductance;
    checks.Expect(output.pressureJump[1] == 0. && output.volumeFlux[1] == 0. &&
                  std::abs(output.pressureWork - expectedWork) < 1e-16,
                  "source-free leaf recovers exact pressure and work", output.pressureJump[1]);
    const auto retained = output.volumeFlux;
    checks.Expect(!ProjectCutRegionPressure({transfer, 0., 0.}, faces, .002, tolerance, output, error) &&
                  output.volumeFlux == retained,
                  "incompatible sealed graph rejects without publishing");
}

void StaticFloat(Checks& checks, int up) {
    const GridLayout layout{{8, 8, 8}, .125};
    constexpr double height = .45, gravity = -9.81;
    BoxMotion box{41, {.28, .28, .28}, {.58, .58, .58}, {}};
    box.lo[up] = height + .004;
    box.hi[up] = height + .304;
    CutRegionChart chart;
    CutRegionGeometry geometry;
    std::string error;
    CutRegionStep step;
    const bool prepared = PrepareCutRegionChart(layout, {}, box, {}, {}, chart, error) &&
        BuildCutRegionGeometry(chart, {}, geometry, error);
    checks.Expect(prepared, "stationary chart with a real 4 mm dry gap " + error);
    if (!prepared)
        return;
    const auto water = FlatWater(chart, geometry, up, height);
    const bool solved = AdvanceCutRegionStep(chart, geometry, water, 1. / 60.,
                                             {up, gravity, height}, step, error);
    checks.Expect(solved && step.loads.size() == 1, "hydrostatic cut-region step " + error);
    if (!solved || step.loads.size() != 1)
        return;
    const double gasBuoyancy = -gravity * .3 * .3 * .3;
    const auto& load = step.loads.front();
    checks.Expect(std::abs(load.force[up] - gasBuoyancy) < 1e-10 &&
                  std::abs(Sum(step.state.water) - Sum(water.water)) < 1e-12 &&
                  std::all_of(step.pressure.volumeFlux.begin(), step.pressure.volumeFlux.end(),
                              [](double flux) { return std::abs(flux) < 1e-13; }),
                  "dry body sees only gas buoyancy in level or rotated chart", load.force[up]);
}

void WetTranslation(Checks& checks) {
    const GridLayout layout{{8, 8, 8}, .125};
    constexpr double dt = 1. / 120., height = .72, travel = .001;
    const BoxMotion body{7, {.292, .251, .307}, {.521, .489, .569}, {}};
    CutRegionChart chart;
    CutRegionGeometry geometry;
    std::string error;
    if (!PrepareCutRegionChart(layout, {}, body, {}, {0., 12 * travel, 0.}, chart, error) ||
        !BuildCutRegionGeometry(chart, {}, geometry, error)) {
        checks.Expect(false, "wet translating chart " + error);
        return;
    }
    auto state = FlatWater(chart, geometry, 1, height);
    const double initialWater = Sum(state.water);
    double volume = 0., work = 0., momentum = 0., largestFlux = 0.;
    for (int i = 0; i < 12; ++i) {
        CutRegionStep next;
        if (!BuildCutRegionGeometry(chart, {0., i * travel, 0.}, {0., travel, 0.}, geometry, error) ||
            !AdvanceCutRegionStep(chart, geometry, state, dt, {1, -9.81, height}, next, error)) {
            checks.Expect(false, "wet translation stopped at " + std::to_string(i) + " " + error);
            return;
        }
        if (next.activeFaces.size() != next.phase.waterFlux.size() ||
            next.pressure.volumeFlux.size() != next.phase.waterFlux.size()) {
            checks.Expect(false, "shared pressure and phase face identity");
            return;
        }
        for (std::size_t face = 0; face < next.activeFaces.size(); ++face) {
            const double mixture = next.pressure.volumeFlux[face];
            const double water = next.phase.waterFlux[face], air = next.phase.airFlux[face];
            if (std::abs(mixture - water - air) > 1e-16) {
                checks.Expect(false, "one pressure flux is exactly partitioned by the two phases");
                return;
            }
            largestFlux = std::max(largestFlux, std::abs(mixture));
        }
        state = std::move(next.state);
        volume = std::max(volume, std::abs(Sum(state.water) - initialWater));
        work = std::max(work, std::abs(next.pressureWorkResidual));
        momentum = std::max(momentum, Max(next.transportMomentumResidual));
    }
    checks.Expect(largestFlux > 1e-6, "wet translating body drives real shared flux", largestFlux);
    checks.Expect(volume < 1e-11 && work < 1e-8 && momentum < 1e-9,
                  "repeated cut-region pressure, phase and momentum close", volume);
    std::cout << "MEASURE work_J=" << work << " momentum_kg_m_s=" << momentum << '\n';
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    Checks checks;
    PressureGraph(checks);
    StaticFloat(checks, 1);
    StaticFloat(checks, 2);
    WetTranslation(checks);
    std::cout << "SUMMARY cut-region step " << checks.total << " checks, " << checks.failed << " failures\n";
    return checks.failed ? 1 : 0;
}
