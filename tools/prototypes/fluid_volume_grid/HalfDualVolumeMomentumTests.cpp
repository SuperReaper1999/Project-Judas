#include "HalfDualVolumeMomentum.h"
#include "GridPressureReaction.h"
#include "MovingVolumePressureInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace {

using namespace judas::volume_grid;

struct Checks {
    int total = 0, failures = 0;
    void Expect(bool pass, const std::string& label, double value = 0) {
        ++total;
        failures += !pass;
        std::cout << (pass ? "PASS " : "FAIL ") << label << " value=" << value << '\n';
    }
};

HalfDualVolumeState ZeroHalfState(const GridLayout& layout) {
    HalfDualVolumeState state;
    for (auto& field : state.velocity) field.resize(2 * layout.CellCount());
    return state;
}

MovingVolumeState Fill(const GridLayout& layout, const MovingGridGeometry& geometry) {
    MovingVolumeState state;
    state.water = geometry.startCapacity;
    for (int axis = 0; axis < 3; ++axis) state.velocity[axis].resize(layout.FaceCount(axis));
    return state;
}

struct Measured {
    GridVec momentum{};
    double kinetic = 0;
};

Measured Measure(const MovingGridGeometry& geometry, const std::vector<double>& water,
                 const HalfDualVolumeState& state, bool endpoint) {
    Measured result;
    const auto& capacity = endpoint ? geometry.endCapacity : geometry.startCapacity;
    for (std::size_t cell = 0; cell < water.size(); ++cell) {
        // Independent material-density calculation, then sum the two
        // separately owned impulses from this primal parcel.
        const double mass = (capacity[cell] - water[cell]) + 1000 * water[cell];
        for (int component = 0; component < 3; ++component) {
            const double first = state.velocity[component][2 * cell];
            const double second = state.velocity[component][2 * cell + 1];
            result.momentum[component] += .5 * mass * (first + second);
            result.kinetic += .25 * mass * (first * first + second * second);
        }
    }
    return result;
}

bool Build(Checks& checks, const GridLayout& layout, const std::vector<BoxMotion>& boxes,
           MovingGridGeometry& geometry) {
    std::string error;
    const bool ok = BuildMovingGridGeometry(layout, boxes, geometry, error);
    checks.Expect(ok, "half-dual geometry " + error);
    return ok;
}

bool Project(Checks& checks, const GridLayout& layout, const MovingGridGeometry& geometry,
             const MovingVolumeState& state, MovingVolumeStep& projected) {
    std::string error;
    const bool ok = AdvanceMovingVolume(layout, geometry, state, 1. / 30., {}, projected, error);
    checks.Expect(ok, "half-dual material projection " + error);
    return ok;
}

bool Transport(Checks& checks, const GridLayout& layout, const MovingGridGeometry& geometry,
               const MovingVolumeState& fluid, const MovingVolumeStep& projected,
               const HalfDualVolumeState& initial, HalfDualVolumeTransport& transported) {
    std::string error;
    const bool ok = AdvectHalfDualVolumeMomentum(layout, geometry, fluid, projected, initial, transported, error);
    checks.Expect(ok, "owned half-dual transport " + error);
    if (!ok) return false;
    const auto before = Measure(geometry, fluid.water, initial, false);
    const auto after = Measure(geometry, projected.state.water, transported.state, true);
    double residual = 0;
    for (int axis = 0; axis < 3; ++axis) {
        residual = std::max({residual,
            std::abs(after.momentum[axis] - before.momentum[axis] - transported.roundoffMomentum[axis]),
            std::abs(before.momentum[axis] - transported.beforeMomentum[axis]),
            std::abs(after.momentum[axis] - transported.afterMomentum[axis])});
    }
    checks.Expect(residual < 1e-8, "half-dual momentum closes with reported roundoff", residual);
    checks.Expect(after.kinetic <= before.kinetic + 1e-8 &&
        std::abs(before.kinetic - transported.beforeKineticEnergy) < 1e-8 &&
        std::abs(after.kinetic - transported.afterKineticEnergy) < 1e-8,
        "half-dual donor transport has independently bounded kinetic energy", after.kinetic - before.kinetic);
    checks.Expect(transported.maximumHalfMassResidual < 1e-8,
        "each half receives exactly its share of the primal mass change", transported.maximumHalfMassResidual);
    return true;
}

bool Merge(Checks& checks, const GridLayout& layout, const MovingGridGeometry& geometry,
           const std::vector<double>& water, const HalfDualVolumeState& initial,
           HalfDualFaceMerge& merged) {
    std::string error;
    const bool ok = MergeHalfDualVolumeFaces(layout, geometry, water, initial, merged, error);
    checks.Expect(ok, "half-dual face ownership mapping " + error);
    if (!ok) return false;
    const auto before = Measure(geometry, water, initial, true);
    const auto after = Measure(geometry, water, merged.state, true);
    double impulseError = 0;
    for (int axis = 0; axis < 3; ++axis) {
        impulseError = std::max(impulseError,
            std::abs(after.momentum[axis] - before.momentum[axis] - merged.stationaryWallImpulse[axis]));
    }
    const double loss = before.kinetic - after.kinetic;
    checks.Expect(impulseError < 1e-8, "open merge and wall impulse close independent momentum", impulseError);
    checks.Expect(merged.mergeKineticLoss >= 0 && merged.stationaryWallKineticLoss >= 0 &&
        std::abs(loss - merged.mergeKineticLoss - merged.stationaryWallKineticLoss) < 1e-8,
        "merge reports lost energy separately from stationary wall constraint", loss);
    return true;
}

void TestKnownMerge(Checks& checks) {
    const GridLayout layout{{2, 1, 1}, 1};
    MovingGridGeometry geometry;
    if (!Build(checks, layout, {}, geometry)) return;
    auto state = ZeroHalfState(layout);
    // Two 500 kg halves meet at the open face. The outer halves interact
    // with fixed tank walls and record a different impulse/energy change.
    state.velocity[0] = {.4, 1., -3., -.7};
    HalfDualFaceMerge merged;
    if (!Merge(checks, layout, geometry, geometry.endCapacity, state, merged)) return;
    checks.Expect(merged.faceVelocity[0] == std::vector<double>{0., -1., 0.} &&
        merged.state.velocity[0] == std::vector<double>{0., -1., -1., 0.},
        "known two-cell opening mixes the actual adjacent owners");
    checks.Expect(std::abs(merged.mergeKineticLoss - 2000.) < 1e-10,
        "opening two distinct velocities loses exactly the analytical mixing energy", merged.mergeKineticLoss);
    checks.Expect(std::abs(merged.stationaryWallImpulse[0] - 150.) < 1e-10 &&
        std::abs(merged.stationaryWallKineticLoss - 162.5) < 1e-10,
        "fixed exterior walls close the known half-cell impulse and work", merged.stationaryWallImpulse[0]);
}

void TestClosingAndOpening(Checks& checks) {
    const GridLayout layout{{3, 3, 3}, 1};
    BoxMotion box{42, {.4375, .875, .875}, {.9375, 2.125, 2.125}, {.125, 0, 0}};
    const auto face = layout.Face(0, 1, 1, 1);
    const auto leftHalf = 2 * layout.Cell(0, 1, 1) + 1;
    const auto rightHalf = 2 * layout.Cell(1, 1, 1);
    MovingGridGeometry geometry;
    if (!Build(checks, layout, {box}, geometry)) return;
    checks.Expect(geometry.startAperture[0][face] > 0 && geometry.endAperture[0][face] == 0,
        "closing witness changes a real cut face from open to closed");
    auto fluid = Fill(layout, geometry);
    MovingVolumeStep closing;
    if (!Project(checks, layout, geometry, fluid, closing)) return;
    auto state = ZeroHalfState(layout);
    state.velocity[0][leftHalf] = .8;
    state.velocity[0][rightHalf] = .8; // one shared starting velocity while open
    state.velocity[0][leftHalf - 1] = .2;
    state.velocity[0][rightHalf + 1] = -.5;
    HalfDualVolumeTransport transported;
    if (!Transport(checks, layout, geometry, fluid, closing, state, transported)) return;
    checks.Expect(std::abs(closing.materialMassFlux[0][face]) > 1e-6,
        "closing interval transports material before the aperture shuts", closing.materialMassFlux[0][face]);
    HalfDualFaceMerge closed;
    if (!Merge(checks, layout, geometry, closing.state.water, transported.state, closed)) return;
    const double left = closed.state.velocity[0][leftHalf];
    const double right = closed.state.velocity[0][rightHalf];
    checks.Expect(closed.faceVelocity[0][face] == 0 && left == transported.state.velocity[0][leftHalf] &&
        right == transported.state.velocity[0][rightHalf] && std::abs(left - right) > 1e-6,
        "closing face leaves distinct half momentum on both sides of the obstruction", std::abs(left - right));

    // Independently verify that an entirely blocked face has no material
    // exchange or automatic mixing in a subsequent stationary projection.
    box.lo[0] += box.translation[0];
    box.hi[0] += box.translation[0];
    box.translation = {};
    if (!Build(checks, layout, {box}, geometry)) return;
    fluid = Fill(layout, geometry);
    MovingVolumeStep stationary;
    if (!Project(checks, layout, geometry, fluid, stationary)) return;
    if (!Transport(checks, layout, geometry, fluid, stationary, closed.state, transported)) return;
    HalfDualFaceMerge retained;
    if (!Merge(checks, layout, geometry, stationary.state.water, transported.state, retained)) return;
    checks.Expect(stationary.materialMassFlux[0][face] == 0 &&
        std::abs(retained.state.velocity[0][leftHalf] - left) < 1e-14 &&
        std::abs(retained.state.velocity[0][rightHalf] - right) < 1e-14,
        "a stationary blocked face never transfers between its two owners");

    // Restore a genuine open face and mix the separately advected momenta.
    box.translation[0] = -.125;
    if (!Build(checks, layout, {box}, geometry)) return;
    checks.Expect(geometry.startAperture[0][face] == 0 && geometry.endAperture[0][face] > 0,
        "reopening witness restores a shared cut face");
    fluid = Fill(layout, geometry);
    fluid.velocity = retained.faceVelocity;
    MovingVolumeStep opening;
    if (!Project(checks, layout, geometry, fluid, opening)) return;
    if (!Transport(checks, layout, geometry, fluid, opening, retained.state, transported)) return;
    HalfDualFaceMerge opened;
    if (!Merge(checks, layout, geometry, opening.state.water, transported.state, opened)) return;
    const std::size_t lc = leftHalf / 2, rc = rightHalf / 2;
    const double lm = geometry.endCapacity[lc] + 999 * opening.state.water[lc];
    const double rm = geometry.endCapacity[rc] + 999 * opening.state.water[rc];
    const double expected = (lm * transported.state.velocity[0][leftHalf] +
                             rm * transported.state.velocity[0][rightHalf]) / (lm + rm);
    checks.Expect(std::abs(opened.faceVelocity[0][face] - expected) < 1e-12 &&
        opened.state.velocity[0][leftHalf] == opened.state.velocity[0][rightHalf],
        "newly open shared velocity uses the actual adjacent half masses", opened.faceVelocity[0][face]);

    const auto safe = transported;
    auto corrupt = opening;
    corrupt.materialMassFlux[0][layout.Face(0, 0, 1, 1)] = 1.;
    std::string error;
    checks.Expect(!AdvectHalfDualVolumeMomentum(layout, geometry, fluid, corrupt, retained.state, transported, error) &&
        error.find("obstructed") != std::string::npos && transported.state.velocity == safe.state.velocity &&
        transported.afterMomentum == safe.afterMomentum,
        "obstructed primal flux rejects without changing half ownership");
    auto nonfinite = transported.state;
    nonfinite.velocity[0][leftHalf] = std::numeric_limits<double>::quiet_NaN();
    const auto safeMerge = opened;
    checks.Expect(!MergeHalfDualVolumeFaces(layout, geometry, opening.state.water, nonfinite, opened, error) &&
        !error.empty() && opened.state.velocity == safeMerge.state.velocity &&
        opened.stationaryWallImpulse == safeMerge.stationaryWallImpulse,
        "invalid opening velocity rejects without publishing an impulse");
}

void TestPressureMapping(Checks& checks) {
    const GridLayout layout{{3, 3, 3}, 1};
    const BoxMotion box{42, {.5625, .875, .875}, {1.0625, 2.125, 2.125}, {-.125, 0, 0}};
    const auto face = layout.Face(0, 1, 1, 1);
    const auto leftCell = layout.Cell(0, 1, 1), rightCell = layout.Cell(1, 1, 1);
    const auto leftHalf = 2 * leftCell + 1, rightHalf = 2 * rightCell;
    MovingGridGeometry geometry;
    if (!Build(checks, layout, {box}, geometry)) return;
    checks.Expect(geometry.startAperture[0][face] == 0 && geometry.averageAperture[0][face] > 0 &&
        geometry.endAperture[0][face] > 0, "pressure ownership fixture opens during the step");
    auto state = ZeroHalfState(layout);
    state.velocity[0][leftHalf] = .8;
    state.velocity[0][rightHalf] = -.4;
    MovingVolumeState predictor;
    std::string error;
    const bool predicted = BuildHalfDualVolumePredictor(layout, geometry, geometry.startCapacity,
        state, predictor, error);
    checks.Expect(predicted, "half-dual weighted pressure predictor " + error);
    if (!predicted) return;
    const double leftVolume = geometry.startCapacity[leftCell], rightVolume = geometry.startCapacity[rightCell];
    const double expected = (.8 * leftVolume - .4 * rightVolume) / (leftVolume + rightVolume);
    checks.Expect(std::abs(predictor.velocity[0][face] - expected) < 1e-14 &&
        state.velocity[0][leftHalf] == .8 && state.velocity[0][rightHalf] == -.4,
        "pressure sees one weighted flux without merging its two physical owners", predictor.velocity[0][face]);
    constexpr double dt = 1. / 30.;
    MovingVolumeStep projected;
    const bool solved = detail::AdvanceVolumeWithOwnedDualMass(layout, geometry, predictor, dt, projected, error);
    checks.Expect(solved, "matched pressure metric for half owners " + error);
    if (!solved) return;
    HalfDualPressureUpdate corrected;
    const bool mapped = ApplyHalfDualVolumePressure(layout, geometry, predictor, projected, state,
        dt, corrected, error);
    checks.Expect(mapped, "shared pressure impulse maps to both owned halves " + error);
    if (!mapped) return;
    const double left = corrected.state.velocity[0][leftHalf], right = corrected.state.velocity[0][rightHalf];
    const double common = (left * leftVolume + right * rightVolume) / (leftVolume + rightVolume);
    const double fluxSpeed = projected.flux[0][face] / (dt * geometry.averageAperture[0][face]);
    checks.Expect(std::abs(common - fluxSpeed) < 1e-12 && std::abs((left - right) - 1.2) < 1e-12 &&
        std::abs(left - .8) > 1e-6, "pressure changes the true shared flow but retains relative velocity before opening",
        left - .8);
    const auto before = Measure(geometry, predictor.water, state, false);
    const auto after = Measure(geometry, predictor.water, corrected.state, false);
    double impulseError = 0;
    for (int axis = 0; axis < 3; ++axis) {
        impulseError = std::max(impulseError,
            std::abs(after.momentum[axis] - before.momentum[axis] - corrected.impulse[axis]));
    }
    checks.Expect(impulseError < 2e-9, "mapped pressure impulse closes independent owned momentum", impulseError);
    const double energyError = std::abs(after.kinetic - before.kinetic - projected.pressureKineticWork);
    checks.Expect(energyError < 2e-9 &&
        std::abs(projected.pressureFluidWork - corrected.kineticChange - corrected.correctionEnergy) < 2e-9,
        "mapped half pressure work matches the actual projection", energyError);
    const auto loads = AccumulateGridPressureReaction(geometry.averagePatches, projected.pressure);
    GridVec outerImpulse{};
    for (int k = 0; k < layout.cells[2]; ++k) {
        for (int j = 0; j < layout.cells[1]; ++j) {
            for (int i = 0; i < layout.cells[0]; ++i) {
                const std::array<int, 3> cell{i, j, k};
                const double pressure = projected.pressure[layout.Cell(i, j, k)];
                for (int axis = 0; axis < 3; ++axis) {
                    if (cell[axis] == 0) outerImpulse[axis] += dt * layout.h * layout.h * pressure;
                    if (cell[axis] + 1 == layout.cells[axis])
                        outerImpulse[axis] -= dt * layout.h * layout.h * pressure;
                }
            }
        }
    }
    double momentumClosure = std::numeric_limits<double>::infinity();
    if (loads.size() == 1 && loads.front().owner == box.owner) {
        momentumClosure = 0;
        for (int axis = 0; axis < 3; ++axis) {
            momentumClosure = std::max(momentumClosure,
                std::abs(corrected.impulse[axis] + dt * loads.front().force[axis] - outerImpulse[axis]));
        }
    }
    checks.Expect(momentumClosure < 2e-9,
        "actual pressure impulse closes with moving solid and independently integrated tank walls", momentumClosure);

    HalfDualVolumeTransport transported;
    if (!Transport(checks, layout, geometry, predictor, projected, corrected.state, transported)) return;
    HalfDualFaceMerge opened;
    if (!Merge(checks, layout, geometry, projected.state.water, transported.state, opened)) return;
    checks.Expect(opened.state.velocity[0][leftHalf] == opened.state.velocity[0][rightHalf],
        "end topology merges halves only after actual open-face material transport");

    const auto retained = corrected;
    MovingVolumeStep unmatched;
    const bool legacy = AdvanceMovingVolume(layout, geometry, predictor, dt, {}, unmatched, error);
    checks.Expect(legacy, "mismatched metric pressure control really solves " + error);
    if (!legacy) return;
    checks.Expect(!ApplyHalfDualVolumePressure(layout, geometry, predictor, unmatched, state, dt,
        corrected, error) && error.find("matched dual mass") != std::string::npos &&
        corrected.state.velocity == retained.state.velocity && corrected.impulse == retained.impulse,
        "mismatched kinetic metric rejects instead of inventing a pressure impulse");

    auto forged = predictor;
    forged.velocity[0][face] += .01;
    checks.Expect(!ApplyHalfDualVolumePressure(layout, geometry, forged, projected, state, dt,
        corrected, error) && error.find("owned face predictor") != std::string::npos &&
        corrected.state.velocity == retained.state.velocity && corrected.impulse == retained.impulse,
        "mismatched predictor rejects without publishing mapped pressure");

    auto leaked = projected;
    leaked.flux[0][layout.Face(0, 0, 1, 1)] = .01;
    checks.Expect(!ApplyHalfDualVolumePressure(layout, geometry, predictor, leaked, state, dt,
        corrected, error) && error.find("sealed face") != std::string::npos &&
        corrected.state.velocity == retained.state.velocity && corrected.impulse == retained.impulse,
        "mapped pressure rejects flux through a sealed face");
}

void TestEmptyOwners(Checks& checks) {
    const GridLayout layout{{5, 4, 4}, 1};
    const auto leftCell = layout.Cell(0, 1, 1), middleCell = layout.Cell(1, 1, 1);
    const auto nextCell = layout.Cell(2, 1, 1);
    const auto opening = layout.Face(0, 1, 1, 1), emptyFace = layout.Face(0, 2, 1, 1);
    auto state = ZeroHalfState(layout);
    state.velocity[0][2 * leftCell + 1] = .35;
    state.velocity[0][2 * middleCell] = 99.;
    MovingGridGeometry geometry;
    const BoxMotion oneEmpty{11, {.8125, .875, .875}, {2.3125, 2.125, 2.125}, {.3125, 0, 0}};
    if (!Build(checks, layout, {oneEmpty}, geometry)) return;
    checks.Expect(geometry.startCapacity[leftCell] > 0 && geometry.startCapacity[middleCell] == 0 &&
        geometry.averageAperture[0][opening] > 0, "opening cut face has one empty starting material owner");
    MovingVolumeState predictor;
    std::string error;
    const bool one = BuildHalfDualVolumePredictor(layout, geometry, geometry.startCapacity, state, predictor, error);
    checks.Expect(one && std::abs(predictor.velocity[0][opening] - .35) < 1e-14,
        "stale velocity in an empty half cannot move the actual material owner " + error);
    if (!one) return;
    const auto retained = predictor;
    const BoxMotion twoEmpty{12, {.8125, .875, .875}, {3.3125, 2.125, 2.125}, {1.25, 0, 0}};
    if (!Build(checks, layout, {twoEmpty}, geometry)) return;
    checks.Expect(geometry.startCapacity[middleCell] == 0 && geometry.startCapacity[nextCell] == 0 &&
        geometry.averageAperture[0][emptyFace] > 0, "a real opening can expose two previously empty owners");
    checks.Expect(!BuildHalfDualVolumePredictor(layout, geometry, geometry.startCapacity, state, predictor, error) &&
        error.find("needs start material") != std::string::npos &&
        predictor.water == retained.water && predictor.velocity == retained.velocity,
        "pressure refuses to invent inertia on a newly open face with no starting material");

    // A supplied conservative primal ledger may still move material through
    // an initially empty half before its incoming material exists. That donor
    // needs a temporal substep or a coupled remap, rather than a velocity guess.
    const GridLayout transitLayout{{2, 1, 1}, 1};
    MovingGridGeometry transit;
    transit.startCapacity = {1., 0.};
    transit.endCapacity = {.75, .25};
    for (int axis = 0; axis < 3; ++axis) transit.averageAperture[axis].resize(transitLayout.FaceCount(axis), 1.);
    MovingVolumeState transitBefore;
    transitBefore.water = transit.startCapacity;
    MovingVolumeStep flux;
    flux.state.water = transit.endCapacity;
    for (int axis = 0; axis < 3; ++axis) flux.materialMassFlux[axis].resize(transitLayout.FaceCount(axis));
    flux.materialMassFlux[0][transitLayout.Face(0, 1, 0, 0)] = 250.;
    HalfDualVolumeTransport unsupported;
    const auto transitState = ZeroHalfState(transitLayout);
    checks.Expect(!AdvectHalfDualVolumeMomentum(transitLayout, transit, transitBefore, flux,
        transitState, unsupported, error) && error.find("donor capacity") != std::string::npos &&
        unsupported.state.velocity[0].empty(), "same-step transit from an empty half donor rejects");
}

void TestTransverseCounterflow(Checks& checks) {
    const GridLayout layout{{2, 2, 1}, 1};
    MovingGridGeometry geometry;
    if (!Build(checks, layout, {}, geometry)) return;
    auto fluid = Fill(layout, geometry);
    // A real divergence-free circulation transports 100 kg along each leg.
    // Its two y-edges carry opposite mass across the same staggered interface.
    fluid.velocity[1][layout.Face(1, 0, 1, 0)] = 3.;
    fluid.velocity[1][layout.Face(1, 1, 1, 0)] = -3.;
    fluid.velocity[0][layout.Face(0, 1, 0, 0)] = -3.;
    fluid.velocity[0][layout.Face(0, 1, 1, 0)] = 3.;
    MovingVolumeStep projected;
    if (!Project(checks, layout, geometry, fluid, projected)) return;
    const double leftFlux = projected.materialMassFlux[1][layout.Face(1, 0, 1, 0)];
    const double rightFlux = projected.materialMassFlux[1][layout.Face(1, 1, 1, 0)];
    checks.Expect(std::abs(leftFlux - 100.) < 1e-12 && std::abs(rightFlux + 100.) < 1e-12,
        "opposed transverse half-fluxes cancel only their net mass");
    auto state = ZeroHalfState(layout);
    // Manufactured advected component, independent of the circulation's
    // pressure samples. Every half in a row starts with the same velocity.
    state.velocity[0] = {2., 2., 2., 2., -1., -1., -1., -1.};
    HalfDualVolumeTransport moved;
    if (!Transport(checks, layout, geometry, fluid, projected, state, moved)) return;
    HalfDualFaceMerge merged;
    if (!Merge(checks, layout, geometry, projected.state.water, moved.state, merged)) return;
    const double lower = merged.faceVelocity[0][layout.Face(0, 1, 0, 0)];
    const double upper = merged.faceVelocity[0][layout.Face(0, 1, 1, 0)];
    // Each half flow moves 50 kg. One carries +2 m/s, the other -1 m/s.
    // The resulting 150 Ns exchange moves both 1000 kg staggered averages.
    checks.Expect(std::abs(lower - 1.85) < 1e-12 && std::abs(upper + .85) < 1e-12,
        "opposed half-flows carry momentum even at zero net dual mass flux", lower);
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    Checks checks;
    TestKnownMerge(checks);
    TestClosingAndOpening(checks);
    TestPressureMapping(checks);
    TestEmptyOwners(checks);
    TestTransverseCounterflow(checks);
    std::cout << "SUMMARY checks=" << checks.total << " failures=" << checks.failures << '\n';
    return checks.failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
