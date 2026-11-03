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

void TransportedInterface(Checks& checks, int up) {
    GridLayout layout{{2, 2, 2}, 1.};
    const int across = (up + 1) % 3;
    layout.cells[(up + 2) % 3] = 1;
    CutRegionChart chart;
    CutRegionGeometry geometry;
    std::string error;
    if (!PrepareCutRegionChart(layout, {}, std::nullopt, {}, {}, chart, error) ||
        !BuildCutRegionGeometry(chart, {}, geometry, error)) {
        checks.Expect(false, "circulation interface chart " + error);
        return;
    }
    CutRegionState state;
    state.water.resize(chart.regions.size());
    state.velocity.resize(chart.regions.size());
    std::size_t source = 0, receiver = 0;
    for (std::size_t i = 0; i < chart.regions.size(); ++i) {
        const auto& r = chart.regions[i];
        const bool high = r.lo[up] > 0., far = r.lo[across] > 0.;
        state.water[i] = (high || far) ? 1. : 0.;
        state.velocity[i][up] = far ? -1. : 1.;
        state.velocity[i][across] = high ? 1. : -1.;
        if (!far)
            (high ? receiver : source) = i;
    }
    const CutRegionSettings settings{up, 0., 0.};
    constexpr double dt = .05;
    CutRegionStep first;
    if (!AdvanceCutRegionStep(chart, geometry, state, dt, settings, first, error)) {
        checks.Expect(false, "circulation creates the new interface " + error);
        return;
    }
    const auto& placed = first.state.interfaces[receiver];
    checks.Expect(placed.axis == up && placed.wetAbove &&
                  std::abs(first.state.water[receiver] - .95) < 1e-12 &&
                  std::abs(Sum(first.state.water) - Sum(state.water)) < 1e-12,
                  "air entering from below remains at the lower inlet", first.state.water[receiver]);

    std::vector<CutRegionWaterPlane> planes;
    if (!ReconstructCutRegionWaterPlanes(chart, geometry, up, first.state.water,
                                         first.state.interfaces, planes, error)) {
        checks.Expect(false, "transported interface reconstructs " + error);
        return;
    }
    const auto inlet = std::find_if(geometry.faces.begin(), geometry.faces.end(), [&](const auto& face) {
        return face.left == source && face.right == receiver;
    });
    if (inlet == geometry.faces.end()) {
        checks.Expect(false, "physical inlet survived the reconstruction");
        return;
    }
    const auto phase = MeasureCutRegionFacePhase(*inlet, geometry.motion, planes[source], planes[receiver]);
    checks.Expect(phase.dry[1].start == inlet->startArea && phase.wet[1].start == 0.,
                  "reconstructed pressure and transport inlet has a dry receiving side");

    // Drive the return circulation without exactly prescribing its divergence.
    // That lets the changed aperture inertia participate in the projection.
    auto driven = first.state;
    for (std::size_t i = 0; i < state.velocity.size(); ++i)
        for (int axis = 0; axis < 3; ++axis)
            driven.velocity[i][axis] = -.5 * state.velocity[i][axis];
    driven.velocity[receiver][up] *= .8;
    auto reset = driven;
    reset.interfaces.clear();
    CutRegionStep returned, resetResult;
    const bool physical = AdvanceCutRegionStep(chart, geometry, driven, dt, settings, returned, error);
    const bool relocated = AdvanceCutRegionStep(chart, geometry, reset, dt, settings, resetResult, error);
    if (!physical || !relocated) {
        checks.Expect(false, "return circulation " + error);
        return;
    }
    const auto face = static_cast<std::size_t>(inlet - geometry.faces.begin());
    const auto position = std::find(returned.activeFaces.begin(), returned.activeFaces.end(), face);
    if (position == returned.activeFaces.end() || returned.activeFaces != resetResult.activeFaces) {
        checks.Expect(false, "return uses the same physical face ordering");
        return;
    }
    const auto i = static_cast<std::size_t>(position - returned.activeFaces.begin());
    checks.Expect(returned.phase.airFlux[i] < -.01 && returned.phase.waterFlux[i] == 0. &&
                  resetResult.phase.waterFlux[i] < -.01 && resetResult.phase.airFlux[i] == 0.,
                  "return transports the anchored air and catches a gravity-only reconstruction",
                  returned.phase.airFlux[i]);
    const double change = std::abs(returned.pressure.volumeFlux[i] - resetResult.pressure.volumeFlux[i]);
    checks.Expect(change > 1e-7 && std::abs(Sum(returned.state.water) - Sum(state.water)) < 1e-12,
                  "pressure also consumes the transported interface on the return", change);
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    Checks checks;
    PressureGraph(checks);
    StaticFloat(checks, 1);
    StaticFloat(checks, 2);
    WetTranslation(checks);
    TransportedInterface(checks, 1);
    TransportedInterface(checks, 2);
    std::cout << "SUMMARY cut-region step " << checks.total << " checks, " << checks.failed << " failures\n";
    return checks.failed ? 1 : 0;
}
