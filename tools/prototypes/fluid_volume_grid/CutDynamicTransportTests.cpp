#include "CutDynamicTransport.h"
#include "CutRectangularOwners.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>

using namespace judas::volume_grid;
namespace {
int checks = 0, failures = 0;
void Check(bool condition, const char* label) {
    ++checks;
    failures += !condition;
    std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
}
double Total(const std::vector<double>& v) {
    return static_cast<double>(std::accumulate(v.begin(), v.end(), 0.L));
}
bool Make(GridLayout layout, CutRegionChart& chart, CutRegionGeometry& geometry) {
    std::string error;
    return PrepareCutRegionChart(layout, {}, {}, {}, {}, chart, error) &&
           BuildCutRegionGeometry(chart, {}, geometry, error);
}
CutDynamicState Fill(const CutRegionGeometry& geometry, std::vector<double> water) {
    CutDynamicState state;
    state.water = std::move(water);
    for (std::size_t i = 0; i < state.water.size(); ++i)
        state.gasMass.push_back(1.2 * (geometry.startCapacity[i] - state.water[i]));
    return state;
}
void Audit(const CutRegionGeometry& geometry, const CutDynamicState& initial,
           const CutDynamicStep& step) {
    Check(std::abs(Total(step.state.water) - Total(initial.water)) <= 2e-14,
          "water inventory is conserved");
    Check(std::abs(Total(step.state.gasMass) + step.externalGasMass - Total(initial.gasMass)) <= 2e-14,
          "finite gas mass plus explicit boundary transfer is conserved");
    double closure = 0.;
    for (std::size_t i = 0; i < initial.water.size(); ++i) {
        const double density = 1.2 + step.state.pressure[i] / (343. * 343.);
        closure = std::max(closure, std::abs(geometry.endCapacity[i] -
            step.state.water[i] - step.state.gasMass[i] / density));
    }
    Check(closure <= 2e-14, "phase volumes and nonlinear gas EOS close independently");
}
} // namespace

int main() {
    std::cout << std::setprecision(17);
    std::string error;
    for (int axis = 0; axis < 3; ++axis) {
        for (bool inverted : {false, true}) {
            GridLayout layout{{1, 1, 1}, 1.};
            layout.cells[axis] = 2;
            CutRegionChart chart;
            CutRegionGeometry geometry;
            Check(Make(layout, chart, geometry), "build two-cell hydrostatic geometry");
            auto state = Fill(geometry, inverted ? std::vector<double>{0., 1.}
                                                 : std::vector<double>{1., 0.});
            CutDynamicSettings settings;
            settings.dt = 1. / 60.;
            settings.gravity = {};
            settings.gravity[axis] = -9.81;
            CutDynamicStep step;
            const bool ok = AdvanceCutDynamic(chart, geometry, state, settings, step, error);
            Check(ok, "shared-face hydrostatic column advances on each gravity axis");
            if (!ok) {
                std::cout << "DETAIL " << error << '\n';
                continue;
            }
            Check(std::abs(step.totalFlux[0]) < 1e-13, "hydrostatic column has no resolved flow");
            Check(std::abs(step.state.pressure[0] - step.state.pressure[1] - 4910.886) < 1e-7,
                  "pressure balances the independently integrated half-column weight");
            Audit(geometry, state, step);
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        for (bool wetAbove : {false, true}) {
            CutRegionChart chart;
            CutRegionGeometry geometry;
            Check(Make({{1, 1, 1}, 1.}, chart, geometry), "build independently owned minority phase");
            std::vector<CutRegionWaterPlane> planes;
            const std::vector<CutRegionInterface> orientation{{axis, wetAbove}};
            Check(ReconstructCutRegionPhasePlanes(chart, geometry, axis, {1.}, {1e-20},
                  orientation, planes, error), "retain gas smaller than one water ULP");
            const auto gas = MeasureCutRegionPhaseVolumes(chart.regions[0], {},
                chart.regions[0].lo, chart.regions[0].hi, planes[0]);
            Check(std::abs(gas[1] / 1e-20 - 1.) < 1e-12, "minority gas has actual reconstructed geometric volume");
            Check(ReconstructCutRegionPhasePlanes(chart, geometry, axis, {1e-20}, {1.},
                  orientation, planes, error), "retain water smaller than one gas ULP");
            const auto water = MeasureCutRegionPhaseVolumes(chart.regions[0], {},
                chart.regions[0].lo, chart.regions[0].hi, planes[0]);
            Check(std::abs(water[0] / 1e-20 - 1.) < 1e-12, "minority water has actual reconstructed geometric volume");
        }
    }
    {
        CutRegionChart chart;
        chart.layout = {{1, 1, 1}, 1.};
        chart.regions = {{{0., 0., 0.}, {.9017, 1., 1.}, 0}};
        CutRegionGeometry geometry;
        Check(BuildCutRegionGeometry(chart, {}, geometry, error), "build legacy arithmetic-boundary control");
        const double capacity = geometry.startCapacity[0];
        const double water = capacity + 128. * std::numeric_limits<double>::epsilon() * capacity;
        std::vector<CutRegionWaterPlane> planes;
        Check(ReconstructCutRegionWaterPlanes(chart, geometry, 1, {water}, {}, planes, error),
              "water-only entry point preserves its original rounded upper admission bound");
    }
    for (double origin : {0., 1000.}) {
        CutRegionChart chart;
        CutRegionGeometry geometry;
        Check(Make({{2, 2, 1}, 1.}, chart, geometry), "build closed circulation geometry");
        for (auto& region : chart.regions)
            for (int axis = 0; axis < 3; ++axis) {
                region.lo[axis] += origin;
                region.hi[axis] += origin;
            }
        for (auto& face : chart.faces)
            for (int axis = 0; axis < 3; ++axis) {
                face.lo[axis] += origin;
                face.hi[axis] += origin;
            }
        Check(BuildCutRegionGeometry(chart, {}, geometry, error), "build translated local-frame geometry");
        auto initial = Fill(geometry, {1., 0., 1., 0.});
        initial.faceSpeed.resize(geometry.faces.size());
        for (std::size_t f = 0; f < geometry.faces.size(); ++f) {
            const auto& face = geometry.faces[f];
            initial.faceSpeed[f] = face.axis == 0 ? (face.left == 0 ? .1 : -.1)
                                                 : (face.left == 0 ? -.1 : .1);
        }
        CutDynamicSettings settings;
        settings.gravity = {};
        settings.dt = 1. / 60.;
        CutDynamicStep step;
        const bool ok = AdvanceCutDynamic(chart, geometry, initial, settings, step, error);
        Check(ok, "a finite plug crosses a pure water-air shared face");
        if (ok) {
            Check(std::abs(step.state.water[1] - 1. / 600.) <= 1e-15,
                  "incoming water matches the analytic departure volume");
            Check(step.state.interfaces[1].axis == 0 && !step.state.interfaces[1].wetAbove,
                  "new mixed owner retains its actual incoming face orientation");
            Audit(geometry, initial, step);
        }
        for (double& velocity : initial.faceSpeed)
            velocity *= 1e-17;
        const bool tiny = AdvanceCutDynamic(chart, geometry, initial, settings, step, error);
        Check(tiny, "sub-coordinate-ULP departures use a local geometric frame");
        if (tiny)
            Check(step.state.water[1] > 0., "a tiny owned water arrival is retained");
    }
    for (bool water : {false, true}) {
        CutRegionChart chart;
        CutRegionGeometry geometry;
        Check(Make({{2, 2, 1}, 1.}, chart, geometry), "build connected pure-phase through-flow");
        auto state = Fill(geometry, std::vector<double>(4, water ? 1. : 0.));
        state.faceSpeed.resize(geometry.faces.size());
        for (std::size_t f = 0; f < geometry.faces.size(); ++f) {
            const auto& face = geometry.faces[f];
            state.faceSpeed[f] = face.axis == 0 ? (face.left == 0 ? 100. : -100.)
                                               : (face.left == 0 ? -100. : 100.);
        }
        CutDynamicSettings settings;
        settings.dt = 1. / 60.;
        settings.gravity = {};
        CutDynamicStep step;
        const bool ok = AdvanceCutDynamic(chart, geometry, state, settings, step, error);
        Check(ok, "pure phase transits more than one donor volume without an interface shortcut");
        if (ok) {
            Check(step.pureThroughFlowOwners == 4, "all transit owners are explicitly accounted for");
            Audit(geometry, state, step);
        }
        // Same speed, with real phase fronts: this permission must disappear.
        const auto mixedInventory = Fill(geometry, {1., 0., 1., 0.});
        state.water = mixedInventory.water;
        state.gasMass = mixedInventory.gasMass;
        step.state.water = {-7.};
        Check(!AdvanceCutDynamic(chart, geometry, state, settings, step, error) &&
              step.state.water == std::vector<double>{-7.},
              "mixed interface cannot borrow pure-phase transit permission");
    }
    {
        CutRegionChart chart;
        CutRegionGeometry geometry;
        Check(Make({{2, 1, 1}, 1.}, chart, geometry), "build finite-air compression control");
        const auto state = Fill(geometry, {0., 0.});
        auto moving = state;
        moving.faceSpeed = {.1};
        CutDynamicSettings settings;
        settings.dt = 1. / 60.;
        settings.gravity = {};
        CutDynamicStep step;
        const bool ok = AdvanceCutDynamic(chart, geometry, moving, settings, step, error);
        Check(ok, "finite-air acoustic pressure advances");
        if (ok) {
            Check(step.state.pressure[0] < 0. && step.state.pressure[1] > 0.,
                  "air loses and gains pressure according to actual mass transport");
            Check(step.state.faceSpeed[0] > 0. && step.state.faceSpeed[0] < .1,
                  "finite gas pressure opposes compression without reversing it");
            Audit(geometry, state, step);
        }
        const auto rejected = [&](const CutRegionGeometry& g, const CutDynamicState& s,
                                  const CutDynamicSettings& cfg, const char* label) {
            CutDynamicStep sentinel;
            sentinel.state.water = {-7.};
            sentinel.state.gasMass = {-9.};
            sentinel.totalFlux = {-11.};
            Check(!AdvanceCutDynamic(chart, g, s, cfg, sentinel, error) && !error.empty(), label);
            Check(sentinel.state.water == std::vector<double>{-7.} &&
                  sentinel.state.gasMass == std::vector<double>{-9.} &&
                  sentinel.totalFlux == std::vector<double>{-11.}, "rejection preserves published state");
        };
        auto invalid = state;
        invalid.water[0] = std::numeric_limits<double>::quiet_NaN();
        rejected(geometry, invalid, settings, "NaN water is rejected");
        invalid = state;
        invalid.gasMass[0] = 0.;
        rejected(geometry, invalid, settings, "unowned missing gas mass is rejected");
        auto badSettings = settings;
        badSettings.gravity[0] = std::numeric_limits<double>::quiet_NaN();
        rejected(geometry, state, badSettings, "nonfinite gravity is rejected");
        badSettings = settings;
        badSettings.dt = -1.;
        rejected(geometry, state, badSettings, "invalid interval is rejected");
        auto badGeometry = geometry;
        badGeometry.faces[0].left = 100;
        rejected(badGeometry, state, settings, "invalid face ownership is rejected");
        badGeometry = geometry;
        badGeometry.motion = BoxMotion{1, {}, {}, {.01, 0., 0.}};
        rejected(badGeometry, state, settings, "unsupported moving-solid remap is explicit");
    }
    {
        CutRegionChart chart;
        chart.layout = {{2, 1, 1}, 1.};
        chart.regions = {{{0., 0., 0.}, {1e-20, 1., 1.}, 0},
                         {{1e-20, 0., 0.}, {1., 1., 1.}, 1}};
        chart.faces = {{0, 1, 0, {1e-20, 0., 0.}, {1e-20, 1., 1.}, 1., 1., 1., 0., .5}};
        CutRegionGeometry geometry;
        Check(BuildCutRegionGeometry(chart, {}, geometry, error), "build tiny mixed owner pressure-oracle control");
        auto state = Fill(geometry, {5e-21, 0.});
        state.faceSpeed = {1e-18};
        CutDynamicSettings settings;
        settings.dt = .01;
        settings.gravity = {};
        CutDynamicStep sentinel;
        sentinel.state.water = {-7.};
        Check(!AdvanceCutDynamic(chart, geometry, state, settings, sentinel, error) &&
              error.find("cut-local EOS") != std::string::npos &&
              sentinel.state.water == std::vector<double>{-7.},
              "a small global residual cannot publish a locally empty gas owner");
    }
    {
        CutRegionChart chart;
        CutRegionGeometry geometry;
        Check(Make({{1, 1, 1}, 1.}, chart, geometry), "build explicit finite gas vent control");
        auto state = Fill(geometry, {0.});
        state.gasMass[0] *= 2.;
        state.pressure = {1.2 * 343. * 343.};
        CutDynamicSettings settings;
        settings.gravity = {};
        settings.dt = .001;
        const auto opening = FullCutRegionGasVent(chart, 0, 1);
        CutDynamicStep step;
        const bool ok = AdvanceCutDynamic(chart, geometry, state, settings, step, error, {opening});
        Check(ok, "finite ambient vent advances with an explicit mass ledger");
        if (ok) {
            Check(step.externalGasMass > 0. && step.state.pressure[0] > 0. &&
                  step.state.pressure[0] < state.pressure[0],
                  "vent releases pressure gradually rather than identifying the chamber with ambient");
            Audit(geometry, state, step);
        }
        step.state.water = {-7.};
        Check(!AdvanceCutDynamic(chart, geometry, state, settings, step, error, {opening, opening}) &&
              step.state.water == std::vector<double>{-7.}, "overlapping vents reject without double-counting a physical opening");
        auto invalidVent = opening;
        invalidVent.lo[1] = std::numeric_limits<double>::quiet_NaN();
        Check(!AdvanceCutDynamic(chart, geometry, state, settings, step, error, {invalidVent}) &&
              step.state.water == std::vector<double>{-7.}, "nonfinite vent geometry rejects transactionally");
        auto tiny = opening;
        tiny.hi[1] = 1e-9;
        tiny.hi[2] = 1e-9;
        const bool tinyOK = AdvanceCutDynamic(chart, geometry, state, settings, step, error, {tiny});
        Check(tinyOK, "microscopic gas vent retains finite inertance");
        if (tinyOK)
            Check(step.externalGasMass > 0. && step.state.pressure[0] >= state.pressure[0] * (1. - 1e-12),
                  "tiny aperture is neither closed nor instant atmosphere");
    }
    {
        CutRegionChart chart;
        CutRegionGeometry geometry;
        Check(Make({{2, 2, 1}, 1.}, chart, geometry), "build occluded mixed departure control");
        chart.moving = BoxMotion{1, {.5, 0., 0.}, {.75, .5, 1.}, {}};
        chart.movingRegions = {0};
        chart.movingFaces.resize(chart.faces.size());
        std::iota(chart.movingFaces.begin(), chart.movingFaces.end(), 0);
        Check(BuildCutRegionGeometry(chart, {}, geometry, error), "compile a partial solid behind the departure aperture");
        auto state = Fill(geometry, {.1875, .1875, .1875, .1875});
        state.faceSpeed.resize(geometry.faces.size());
        for (std::size_t f = 0; f < geometry.faces.size(); ++f) {
            const auto& face = geometry.faces[f];
            state.faceSpeed[f] = face.axis == 0 ? (face.left == 0 ? .6 : -.6)
                                               : (face.left == 0 ? -.6 : .6);
        }
        CutDynamicSettings settings;
        settings.dt = 1.;
        settings.gravity = {};
        CutDynamicStep sentinel;
        sentinel.state.water = {-7.};
        Check(!AdvanceCutDynamic(chart, geometry, state, settings, sentinel, error) &&
              error.find("owned finite prism") != std::string::npos &&
              sentinel.state.water == std::vector<double>{-7.},
              "a solid-occluded prism cannot be scaled into an invented material departure");
    }
    {
        CutRegionChart source;
        source.layout = {{2, 1, 1}, 1.};
        source.regions = {{{0., 0., 0.}, {.01, 1., 1.}, 0},
                          {{.01, 0., 0.}, {1., 1., 1.}, 1}};
        source.faces = {{0, 1, 0, {.01, 0., 0.}, {.01, 1., 1.}, 1., 1., 1., 0., .5}};
        CutRegionGeometry geometry;
        Check(BuildCutRegionGeometry(source, {}, geometry, error), "build exact thin rectangular cut");
        CutRectangularOwners owners;
        Check(BuildCutRectangularOwners(source, geometry, .125, owners, error), "agglomerate exact rectangular union");
        Check(owners.chart.regions.size() == 1 && owners.owner[0] == owners.owner[1],
              "a fully open internal boundary is removed");
        Check(owners.geometry.startCapacity[0] == geometry.startCapacity[0] + geometry.startCapacity[1],
              "owner capacity keeps the original geometric quantity ledger");
        Check(owners.chart.faces.empty(), "no internal boundary survives as an external flow port");
        geometry.faces[0].startArea = 0.;
        geometry.faces[0].endArea = 0.;
        geometry.faces[0].meanArea = 0.;
        Check(BuildCutRectangularOwners(source, geometry, .125, owners, error) &&
              owners.chart.regions.size() == 2 && owners.geometry.faces[0].meanArea == 0.,
              "closed wall prevents ownership merging and remains closed");
        geometry.faces[0].startArea = 1e-18;
        geometry.faces[0].endArea = 1e-18;
        geometry.faces[0].meanArea = 1e-18;
        Check(BuildCutRectangularOwners(source, geometry, .125, owners, error) &&
              owners.chart.regions.size() == 2 && owners.geometry.faces[0].meanArea == 1e-18,
              "microscopic aperture retains finite area and separate owners");
    }
    std::cout << "SUMMARY " << checks << " checks " << failures << " failures\n";
    return failures ? 1 : 0;
}
