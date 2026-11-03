#include "MovingVolumeGrid.h"
#include "GridPressureReaction.h"
#include "CoupledVolumeBox.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

namespace {

using namespace judas::volume_grid;

constexpr double kDt = 1.0 / 30.0;
constexpr double kVolumeTolerance = 2e-11;
constexpr GridVec kZero{};

struct Checks {
    int total = 0;
    int failures = 0;

    void Expect(bool pass, const std::string& label, double value = 0.0) {
        ++total;
        failures += !pass;
        std::cout << (pass ? "PASS " : "FAIL ") << label << " value=" << value << '\n';
    }
};

template<class Function>
void ForEachCell(const GridLayout& layout, Function&& function) {
    for (int k = 0; k < layout.cells[2]; ++k) {
        for (int j = 0; j < layout.cells[1]; ++j) {
            for (int i = 0; i < layout.cells[0]; ++i) {
                function(std::array<int, 3>{i, j, k}, layout.Cell(i, j, k));
            }
        }
    }
}

template<class Function>
void ForEachFace(const GridLayout& layout, int axis, Function&& function) {
    auto counts = layout.cells;
    ++counts[axis];
    for (int k = 0; k < counts[2]; ++k) {
        for (int j = 0; j < counts[1]; ++j) {
            for (int i = 0; i < counts[0]; ++i) {
                function(std::array<int, 3>{i, j, k}, layout.Face(axis, i, j, k));
            }
        }
    }
}

double FluidCapacity(const GridLayout& layout, const std::array<int, 3>& cell,
                     const BoxMotion& box, double time) {
    // Independent endpoint AABB clipping. Do not use the production capacity,
    // aperture, patch or sweep arrays as a reference for this calculation.
    double solid = 1;
    for (int axis = 0; axis < 3; ++axis) {
        const double cellLow = cell[axis] * layout.h;
        const double cellHigh = (cell[axis] + 1) * layout.h;
        const double low = box.lo[axis] + time * box.translation[axis];
        const double high = box.hi[axis] + time * box.translation[axis];
        solid *= std::max(0.0, std::min(cellHigh, high) - std::max(cellLow, low));
    }
    return layout.h * layout.h * layout.h - solid;
}

double Sum(const std::vector<double>& values) {
    return std::accumulate(values.begin(), values.end(), 0.0);
}

double MaximumSpeed(const MovingVolumeState& state) {
    double maximum = 0;
    for (const auto& field : state.velocity) {
        for (double value : field) {
            maximum = std::max(maximum, std::abs(value));
        }
    }
    return maximum;
}

MovingVolumeState AtRest(const GridLayout& layout, const std::vector<double>& water) {
    MovingVolumeState state;
    state.water = water;
    for (int axis = 0; axis < 3; ++axis) {
        state.velocity[axis].resize(layout.FaceCount(axis));
    }
    return state;
}

BoxMotion InteriorBox() {
    return {17, {0.61, 0.56, 0.60}, {1.01, 0.74, 0.93}, {0.035, 0.0, 0.0}};
}

void Translate(BoxMotion& box) {
    for (int axis = 0; axis < 3; ++axis) {
        box.lo[axis] += box.translation[axis];
        box.hi[axis] += box.translation[axis];
    }
}

bool Build(Checks& checks, const std::string& label, const GridLayout& layout,
           const std::vector<BoxMotion>& motions, MovingGridGeometry& geometry) {
    std::string error;
    const bool ok = BuildMovingGridGeometry(layout, motions, geometry, error);
    checks.Expect(ok, label + " geometry admission" + (ok ? "" : " " + error));
    return ok;
}

bool Advance(Checks& checks, const std::string& label, const GridLayout& layout,
             const MovingGridGeometry& geometry, const MovingVolumeState& state,
             const GridVec& gravity, MovingVolumeStep& step) {
    std::string error;
    const bool ok = AdvanceMovingVolume(layout, geometry, state, kDt, gravity, step, error);
    checks.Expect(ok, label + " pressure and transport admission" + (ok ? "" : " " + error));
    return ok;
}

struct MotionErrors {
    double capacity = 0;
    double localVolume = 0;
    double water = 0;
    double boundaryFlux = 0;
    double cfl = 0;
    double flux = 0;
    double fluidWork = 0;
    double kineticIdentity = 0;
    double reactionIdentity = 0;
};

void CheckMotionStep(const GridLayout& layout, const BoxMotion& box,
                     const MovingGridGeometry& geometry, const MovingVolumeState& state,
                     const MovingVolumeStep& next, const GridVec& gravity,
                     double tracer, MotionErrors& errors) {
    double expectedWork = 0;
    ForEachCell(layout, [&](auto cell, std::size_t index) {
        const double before = FluidCapacity(layout, cell, box, 0);
        const double after = FluidCapacity(layout, cell, box, 1);
        errors.capacity = std::max({errors.capacity,
            std::abs(geometry.startCapacity[index] - before),
            std::abs(geometry.endCapacity[index] - after)});
        double outwardFlux = 0;
        for (int axis = 0; axis < 3; ++axis) {
            auto upper = cell;
            ++upper[axis];
            const std::size_t low = layout.Face(axis, cell[0], cell[1], cell[2]);
            const std::size_t high = layout.Face(axis, upper[0], upper[1], upper[2]);
            outwardFlux += next.flux[axis][high] - next.flux[axis][low];
        }
        errors.localVolume = std::max(errors.localVolume, std::abs(outwardFlux - (before - after)));
        if (tracer >= 0) {
            errors.water = std::max(errors.water, std::abs(next.state.water[index] - tracer * after));
        }
        expectedWork += next.pressure[index] * (before - after);
    });
    errors.fluidWork = std::max(errors.fluidWork, std::abs(next.pressureFluidWork - expectedWork));

    // Pressure projection work oracle at fixed average face metric. Recover
    // transport velocity from the actual reported flux, independent of the
    // state velocity published for the next step.
    double kineticChange = 0;
    double projectionDefect = 0;
    for (int axis = 0; axis < 3; ++axis) {
        ForEachFace(layout, axis, [&](auto face, std::size_t index) {
            const double flux = next.flux[axis][index];
            errors.flux = std::max(errors.flux, std::abs(flux));
            if (face[axis] == 0 || face[axis] == layout.cells[axis]) {
                errors.boundaryFlux = std::max(errors.boundaryFlux, std::abs(flux));
                return;
            }
            const double area = geometry.averageAperture[axis][index];
            if (area == 0) return;
            auto left = face;
            --left[axis];
            const auto leftIndex = layout.Cell(left[0], left[1], left[2]);
            const auto rightIndex = layout.Cell(face[0], face[1], face[2]);
            const double leftFraction = state.water[leftIndex] / FluidCapacity(layout, left, box, 0);
            const double rightFraction = state.water[rightIndex] / FluidCapacity(layout, face, box, 0);
            const double density = 1 + 999 * (leftFraction + rightFraction) * 0.5;
            const double after = flux / (area * kDt);
            const double before = state.velocity[axis][index] + kDt * gravity[axis];
            const double mass = density * area * layout.h;
            const double delta = after - before;
            kineticChange += 0.5 * mass * (after * after - before * before);
            projectionDefect += 0.5 * mass * delta * delta;
        });
    }
    errors.kineticIdentity = std::max(errors.kineticIdentity,
        std::abs(kineticChange + projectionDefect - expectedWork));
    const auto loads = AccumulateGridPressureReaction(geometry.averagePatches, next.pressure);
    const double bodyWork = loads.size() == 1 && loads.front().owner == box.owner
        ? std::inner_product(loads.front().force.begin(), loads.front().force.end(),
                             box.translation.begin(), 0.)
        : std::numeric_limits<double>::infinity();
    errors.reactionIdentity = std::max(errors.reactionIdentity, std::abs(bodyWork + expectedWork));
    errors.cfl = std::max(errors.cfl, next.maxCfl);
}

void TestUniformTransport(Checks& checks, const GridLayout& layout, double tracer) {
    const std::string label = "uniform tracer " + std::to_string(tracer);
    BoxMotion box = InteriorBox();
    MovingGridGeometry geometry;
    if (!Build(checks, label, layout, {box}, geometry)) return;
    auto state = AtRest(layout, geometry.startCapacity);
    for (double& cell : state.water) cell *= tracer;
    const double initialWater = Sum(state.water);
    double massError = 0;
    MotionErrors errors;
    for (int frame = 0; frame < 16; ++frame) {
        if (frame == 8) box.translation[0] = -box.translation[0];
        if (frame && !Build(checks, label, layout, {box}, geometry)) return;
        MovingVolumeStep next;
        if (!Advance(checks, label, layout, geometry, state, kZero, next)) return;
        CheckMotionStep(layout, box, geometry, state, next, kZero, tracer, errors);
        if (frame == 0) {
            checks.Expect(next.pressureFluidWork > 0 && next.pressureKineticWork > 0,
                          label + " body initially supplies kinetic energy", next.pressureFluidWork);
        }
        state = std::move(next.state);
        Translate(box);
        massError = std::max(massError, std::abs(Sum(state.water) - initialWater));
    }
    checks.Expect(errors.capacity < kVolumeTolerance, label + " exact endpoint capacity", errors.capacity);
    checks.Expect(errors.localVolume < kVolumeTolerance, label + " cell continuity", errors.localVolume);
    checks.Expect(errors.water < kVolumeTolerance, label + " constant concentration follows capacity", errors.water);
    checks.Expect(massError < kVolumeTolerance, label + " conserved global water volume", massError);
    checks.Expect(errors.boundaryFlux == 0, label + " impermeable outer wall", errors.boundaryFlux);
    checks.Expect(errors.flux > 1e-5, label + " moving solid produces actual fluid flux", errors.flux);
    checks.Expect(errors.fluidWork < 2e-8, label + " moving boundary work uses same pressure and volume", errors.fluidWork);
    checks.Expect(errors.kineticIdentity < 2e-7, label + " discrete pressure energy identity", errors.kineticIdentity);
    checks.Expect(errors.reactionIdentity < 2e-8, label + " body and fluid pressure work are opposite", errors.reactionIdentity);
    checks.Expect(errors.cfl > 0 && errors.cfl <= 1, label + " admitted conservative CFL", errors.cfl);
}

void TestLayeredMotion(Checks& checks, const GridLayout& layout) {
    const std::string label = "submerged translating box in layered water and air";
    const GridVec gravity{0.0, -9.81, 0.0};
    BoxMotion box = InteriorBox();
    MovingGridGeometry geometry;
    if (!Build(checks, label, layout, {box}, geometry)) return;
    auto state = AtRest(layout, geometry.startCapacity);
    ForEachCell(layout, [&](auto cell, std::size_t index) {
        if (cell[1] >= 5) state.water[index] = 0;
    });
    const double initialWater = Sum(state.water);
    double massError = 0;
    double transportedIntoAir = 0;
    double minimumHeadroom = std::numeric_limits<double>::infinity();
    MotionErrors errors;
    for (int frame = 0; frame < 16; ++frame) {
        if (frame == 8) box.translation[0] = -box.translation[0];
        if (frame && !Build(checks, label, layout, {box}, geometry)) return;
        MovingVolumeStep next;
        if (!Advance(checks, label, layout, geometry, state, gravity, next)) return;
        CheckMotionStep(layout, box, geometry, state, next, gravity, -1, errors);
        ForEachCell(layout, [&](auto cell, std::size_t index) {
            const double water = next.state.water[index];
            const double capacity = FluidCapacity(layout, cell, box, 1);
            minimumHeadroom = std::min({minimumHeadroom, water, capacity - water});
            if (cell[1] >= 5) transportedIntoAir = std::max(transportedIntoAir, water);
        });
        state = std::move(next.state);
        Translate(box);
        massError = std::max(massError, std::abs(Sum(state.water) - initialWater));
    }
    checks.Expect(errors.localVolume < kVolumeTolerance, label + " continuity", errors.localVolume);
    checks.Expect(massError < kVolumeTolerance, label + " conserves water volume", massError);
    checks.Expect(minimumHeadroom >= -kVolumeTolerance, label + " remains within wet and dry capacity", minimumHeadroom);
    checks.Expect(transportedIntoAir > 1e-8, label + " transports the free interface", transportedIntoAir);
    checks.Expect(errors.kineticIdentity < 2e-7, label + " discrete pressure energy identity", errors.kineticIdentity);
    checks.Expect(errors.reactionIdentity < 2e-8, label + " body and fluid pressure work are opposite", errors.reactionIdentity);
}

void TestObliqueRest(Checks& checks, const GridLayout& layout) {
    BoxMotion box = InteriorBox();
    box.translation = kZero;
    MovingGridGeometry geometry;
    if (!Build(checks, "oblique gravity", layout, {box}, geometry)) return;
    const auto state = AtRest(layout, geometry.startCapacity);
    const GridVec gravity{1.3, -8.9, 2.6};
    MovingVolumeStep next;
    if (!Advance(checks, "oblique gravity", layout, geometry, state, gravity, next)) return;
    double gradientError = 0;
    for (int axis = 0; axis < 3; ++axis) {
        ForEachFace(layout, axis, [&](auto face, std::size_t index) {
            if (face[axis] == 0 || face[axis] == layout.cells[axis] ||
                geometry.averageAperture[axis][index] == 0) return;
            auto left = face;
            --left[axis];
            const double leftPressure = next.pressure[layout.Cell(left[0], left[1], left[2])];
            const double rightPressure = next.pressure[layout.Cell(face[0], face[1], face[2])];
            gradientError = std::max(gradientError,
                std::abs(rightPressure - leftPressure - 1000 * gravity[axis] * layout.h));
        });
    }
    checks.Expect(gradientError < 1e-6, "oblique gravity pressure difference is hydrostatic", gradientError);
    checks.Expect(MaximumSpeed(next.state) < 2e-8, "oblique gravity creates no motion at equilibrium", MaximumSpeed(next.state));
}

void TestRestAndPerturbation(Checks& checks, const GridLayout& layout) {
    BoxMotion box = InteriorBox();
    box.translation = kZero;
    MovingGridGeometry geometry;
    if (!Build(checks, "hydrostatic pool", layout, {box}, geometry)) return;
    auto state = AtRest(layout, geometry.startCapacity);
    ForEachCell(layout, [&](auto cell, std::size_t index) {
        if (cell[1] >= 5) state.water[index] = 0;
    });
    const auto originalWater = state.water;
    const GridVec gravity{0.0, -9.81, 0.0};
    double maximumSpeed = 0;
    double waterChange = 0;
    for (int frame = 0; frame < 30; ++frame) {
        MovingVolumeStep next;
        if (!Advance(checks, "hydrostatic pool", layout, geometry, state, gravity, next)) return;
        state = std::move(next.state);
        maximumSpeed = std::max(maximumSpeed, MaximumSpeed(state));
        for (std::size_t cell = 0; cell < originalWater.size(); ++cell) {
            waterChange = std::max(waterChange, std::abs(state.water[cell] - originalWater[cell]));
        }
    }
    checks.Expect(maximumSpeed < 2e-8, "hydrostatic pressure leaves layered water still", maximumSpeed);
    checks.Expect(waterChange < kVolumeTolerance, "hydrostatic pressure leaves occupied volume unchanged", waterChange);

    // A divergence-free circulation well inside an uncut, filled region
    // exercises pressure projection without relying on geometric source terms.
    constexpr double speed = 0.15;
    state.velocity[0][layout.Face(0, 2, 1, 1)] = speed;
    state.velocity[1][layout.Face(1, 2, 2, 1)] = speed;
    state.velocity[0][layout.Face(0, 2, 2, 1)] = -speed;
    state.velocity[1][layout.Face(1, 1, 2, 1)] = -speed;
    MovingVolumeStep perturbed;
    if (!Advance(checks, "hydrostatic perturbation", layout, geometry, state, gravity, perturbed)) return;
    const double flux = perturbed.flux[0][layout.Face(0, 2, 1, 1)];
    checks.Expect(std::abs(flux - speed * layout.h * layout.h * kDt) < kVolumeTolerance,
                  "equilibrium pressure does not erase actual circulation", flux);
}

bool SameStep(const MovingVolumeStep& left, const MovingVolumeStep& right) {
    return left.state.water == right.state.water && left.state.velocity == right.state.velocity &&
        left.pressure == right.pressure && left.flux == right.flux &&
        left.materialMassFlux == right.materialMassFlux &&
        left.residual == right.residual && left.pressureFluidWork == right.pressureFluidWork &&
        left.pressureKineticWork == right.pressureKineticWork &&
        left.pressureCorrectionEnergy == right.pressureCorrectionEnergy &&
        left.waterRoundoffCorrection == right.waterRoundoffCorrection && left.maxCfl == right.maxCfl &&
        left.iterations == right.iterations;
}

bool SameCoupledStep(const CoupledBoxStep& left, const CoupledBoxStep& right) {
    const auto& a = left.transport;
    const auto& b = right.transport;
    return left.body.owner == right.body.owner && left.body.lo == right.body.lo &&
        left.body.hi == right.body.hi && left.body.velocity == right.body.velocity &&
        left.body.mass == right.body.mass && SameStep(left.fluid, right.fluid) &&
        left.pressureForce == right.pressureForce && left.rigidResidual == right.rigidResidual &&
        left.pressureEnergyResidual == right.pressureEnergyResidual &&
        left.geometryEvaluations == right.geometryEvaluations && a.velocity == b.velocity &&
        a.beforeMomentum == b.beforeMomentum && a.afterMomentum == b.afterMomentum &&
        a.beforeKineticEnergy == b.beforeKineticEnergy && a.afterKineticEnergy == b.afterKineticEnergy &&
        a.maximumPrimalMassResidual == b.maximumPrimalMassResidual &&
        a.maximumDualMassResidual == b.maximumDualMassResidual && a.maximumDonorCfl == b.maximumDonorCfl &&
        left.stationaryWallImpulse == right.stationaryWallImpulse &&
        left.stationaryWallKineticLoss == right.stationaryWallKineticLoss;
}

void TestRejection(Checks& checks, const GridLayout& layout) {
    const BoxMotion box = InteriorBox();
    MovingGridGeometry geometry;
    if (!Build(checks, "rollback fixture", layout, {box}, geometry)) return;
    const auto state = AtRest(layout, geometry.startCapacity);
    MovingVolumeStep next;
    if (!Advance(checks, "rollback fixture", layout, geometry, state, kZero, next)) return;
    const auto saved = next;

    std::string error;
    auto invalid = state;
    invalid.water[0] = std::numeric_limits<double>::quiet_NaN();
    checks.Expect(!AdvanceMovingVolume(layout, geometry, invalid, kDt, kZero, next, error) &&
        !error.empty() && SameStep(saved, next), "non-finite water rejects without publication");

    error.clear();
    invalid = state;
    invalid.water[0] += layout.h * layout.h * layout.h;
    checks.Expect(!AdvanceMovingVolume(layout, geometry, invalid, kDt, kZero, next, error) &&
        !error.empty() && SameStep(saved, next), "overfilled cell rejects without publication");

    error.clear();
    invalid = state;
    invalid.velocity[0].pop_back();
    checks.Expect(!AdvanceMovingVolume(layout, geometry, invalid, kDt, kZero, next, error) &&
        !error.empty() && SameStep(saved, next), "missing face state rejects without publication");

    error.clear();
    checks.Expect(!AdvanceMovingVolume(layout, geometry, state, 0, kZero, next, error) &&
        !error.empty() && SameStep(saved, next), "invalid timestep rejects without publication");

    error.clear();
    auto missingSweep = geometry;
    for (auto& field : missingSweep.solidSweep) std::fill(field.begin(), field.end(), 0.);
    checks.Expect(!AdvanceMovingVolume(layout, missingSweep, state, kDt, kZero, next, error) &&
        error.find("capacity") != std::string::npos && SameStep(saved, next),
        "changed solid capacity without swept volume rejects without publication");

    error.clear();
    invalid = state;
    // Divergence-free circulation in full uncut cells. Projection must not
    // erase this input merely to slip under the donor CFL restriction.
    constexpr double speed = 16;
    invalid.velocity[0][layout.Face(0, 2, 1, 1)] = speed;
    invalid.velocity[1][layout.Face(1, 2, 2, 1)] = speed;
    invalid.velocity[0][layout.Face(0, 2, 2, 1)] = -speed;
    invalid.velocity[1][layout.Face(1, 1, 2, 1)] = -speed;
    checks.Expect(!AdvanceMovingVolume(layout, geometry, invalid, kDt, kZero, next, error) &&
        error.find("donor capacity") != std::string::npos && SameStep(saved, next),
        "excess transport CFL rejects without publication");

    error.clear();
    checks.Expect(AdvanceMovingVolume(layout, geometry, state, kDt, kZero, next, error) &&
        SameStep(saved, next), "identical admitted inputs replay exactly");

    error.clear();
    auto leaving = box;
    leaving.translation[0] = 2;
    MovingGridGeometry retained = geometry;
    checks.Expect(!BuildMovingGridGeometry(layout, {leaving}, retained, error) && !error.empty() &&
        retained.startCapacity == geometry.startCapacity && retained.endCapacity == geometry.endCapacity &&
        retained.averageAperture == geometry.averageAperture && retained.solidSweep == geometry.solidSweep,
        "solid leaving basin rejects without replacing published geometry");
}

double Dot(const GridVec& first, const GridVec& second) {
    return std::inner_product(first.begin(), first.end(), second.begin(), 0.);
}

double SolidVolume(const CoupledBoxState& body) {
    return (body.hi[0] - body.lo[0]) * (body.hi[1] - body.lo[1]) * (body.hi[2] - body.lo[2]);
}

BoxMotion ResolvedBox() {
    // Every extent exceeds h, so motion can change a cut-cell capacity
    // along each axis. The cross section is deliberately non-cubic.
    return {17, {.625, .53125, .59375}, {1.0625, .84375, .96875}, {.035, 0, 0}};
}

MovingVolumeState FilledTank(Checks& checks, const GridLayout& layout, const CoupledBoxState& body) {
    MovingGridGeometry geometry;
    if (!Build(checks, "coupled tank", layout, {{body.owner, body.lo, body.hi, {}}}, geometry)) return {};
    return AtRest(layout, geometry.startCapacity);
}

bool AdvanceBody(Checks& checks, const std::string& label, const GridLayout& layout,
                 const CoupledBoxState& body, const MovingVolumeState& fluid,
                 const GridVec& gravity, CoupledBoxStep& next) {
    std::string error;
    const bool ok = AdvanceCoupledVolumeBox(layout, body, fluid, kDt, gravity, next, error);
    checks.Expect(ok, label + " coupled pressure admission" + (ok ? "" : " " + error));
    return ok;
}

struct CoupledErrors {
    double impulse = 0;
    double dynamicWork = 0;
    double energy = 0;
    double volume = 0;
    double displacement = 0;
};

void CheckCoupledStep(const CoupledBoxState& body, const MovingVolumeState& fluid,
                      const GridVec& gravity, const CoupledBoxStep& next, CoupledErrors& errors) {
    const double displacedMass = 1000 * SolidVolume(body);
    GridVec freeVelocity, correction, displacement, dynamicForce;
    for (int axis = 0; axis < 3; ++axis) {
        freeVelocity[axis] = body.velocity[axis] + kDt * gravity[axis];
        correction[axis] = next.body.velocity[axis] - freeVelocity[axis];
        displacement[axis] = next.body.lo[axis] - body.lo[axis];
        dynamicForce[axis] = next.pressureForce[axis] + displacedMass * gravity[axis];
        errors.impulse = std::max(errors.impulse,
            std::abs(body.mass * correction[axis] - kDt * next.pressureForce[axis]));
        errors.displacement = std::max(errors.displacement,
            std::abs(displacement[axis] - kDt * next.body.velocity[axis]));
    }
    const double rigidKineticChange = .5 * body.mass *
        (Dot(next.body.velocity, next.body.velocity) - Dot(freeVelocity, freeVelocity));
    const double replacedPotential = displacedMass * Dot(gravity, displacement);
    const double rigidCorrectionEnergy = .5 * body.mass * Dot(correction, correction);
    const double pressureEnergy = rigidKineticChange + next.fluid.pressureKineticWork + replacedPotential +
        rigidCorrectionEnergy + next.fluid.pressureCorrectionEnergy;
    errors.dynamicWork = std::max(errors.dynamicWork,
        std::abs(Dot(dynamicForce, displacement) + next.fluid.pressureFluidWork));
    errors.energy = std::max({errors.energy,
        std::abs(pressureEnergy), std::abs(next.pressureEnergyResidual - pressureEnergy)});
    errors.volume = std::max(errors.volume, std::abs(Sum(next.fluid.state.water) - Sum(fluid.water)));
}

void TestCoupledBuoyancy(Checks& checks, const GridLayout& layout) {
    const GridVec obliqueGravity{1.3, -8.9, 2.6};
    const auto box = ResolvedBox();
    CoupledBoxState body{box.owner, box.lo, box.hi, kZero, 0};
    const double displacedMass = 1000 * SolidVolume(body);
    body.mass = displacedMass;
    auto fluid = FilledTank(checks, layout, body);
    if (fluid.water.empty()) return;
    CoupledBoxStep next;
    if (!AdvanceBody(checks, "neutral oblique", layout, body, fluid, obliqueGravity, next)) return;
    checks.Expect(next.body.lo == body.lo && next.body.hi == body.hi &&
        next.body.velocity == kZero && next.fluid.state.water == fluid.water &&
        MaximumSpeed(next.fluid.state) == 0, "neutral box and water remain exactly at rest under oblique gravity");
    double hydrostaticForceError = 0;
    for (int axis = 0; axis < 3; ++axis) {
        hydrostaticForceError = std::max(hydrostaticForceError,
            std::abs(next.pressureForce[axis] + displacedMass * obliqueGravity[axis]));
    }
    checks.Expect(hydrostaticForceError < 1e-10, "neutral box receives exactly the displaced-weight pressure", hydrostaticForceError);

    const GridVec gravity{0, -9.81, 0};
    for (double densityRatio : {.5, 2.}) {
        const std::string label = densityRatio < 1 ? "light box" : "heavy box";
        body = {box.owner, box.lo, box.hi, kZero, displacedMass * densityRatio};
        fluid = FilledTank(checks, layout, body);
        if (fluid.water.empty()) return;
        const double initialY = body.lo[1];
        const double vacuumAcceleration = gravity[1] * (1. - 1. / densityRatio);
        CoupledErrors errors;
        for (int frame = 0; frame < 8; ++frame) {
            if (!AdvanceBody(checks, label, layout, body, fluid, gravity, next)) return;
            if (frame == 0) {
                const double measuredAcceleration = next.body.velocity[1] / kDt;
                checks.Expect(measuredAcceleration * vacuumAcceleration > 0 &&
                    std::abs(measuredAcceleration) < std::abs(vacuumAcceleration),
                    label + " accelerates in expected direction with pressure-added inertia", measuredAcceleration);
            }
            CheckCoupledStep(body, fluid, gravity, next, errors);
            body = next.body;
            fluid = std::move(next.fluid.state);
        }
        const double travel = body.lo[1] - initialY;
        checks.Expect(travel * vacuumAcceleration > 0, label + " follows an actual moving trajectory", travel);
        checks.Expect(errors.impulse < 5e-8, label + " pressure force closes the rigid impulse", errors.impulse);
        checks.Expect(errors.displacement < 1e-13, label + " published pose follows coupled velocity", errors.displacement);
        checks.Expect(errors.dynamicWork < 2e-8, label + " fluid reaction equals opposite rigid work", errors.dynamicWork);
        checks.Expect(errors.energy < 2e-7, label + " coupled projection energy closes", errors.energy);
        checks.Expect(errors.volume < kVolumeTolerance, label + " water volume remains conserved", errors.volume);
    }
}

void TestCoupledFlow(Checks& checks, const GridLayout& layout) {
    auto motion = ResolvedBox();
    MovingGridGeometry geometry;
    if (!Build(checks, "generated water motion", layout, {motion}, geometry)) return;
    auto fluid = AtRest(layout, geometry.startCapacity);
    MovingVolumeStep stirring;
    if (!Advance(checks, "generated water motion", layout, geometry, fluid, kZero, stirring)) return;
    Translate(motion);
    CoupledBoxState body{motion.owner, motion.lo, motion.hi, kZero, 0};
    body.mass = 1000 * SolidVolume(body);
    CoupledBoxStep next;
    // An external agent stopped the box. The fluid retains the state produced
    // by the genuine prescribed motion and transfers pressure to the release.
    if (!AdvanceBody(checks, "water-driven release", layout, body, stirring.state, kZero, next)) return;
    checks.Expect(next.body.velocity[0] > 1e-3 && next.body.lo[0] > body.lo[0],
        "moving water transfers pressure and moves the stopped box without gravity", next.body.velocity[0]);
    CoupledErrors errors;
    CheckCoupledStep(body, stirring.state, kZero, next, errors);
    checks.Expect(errors.impulse < 5e-8 && errors.dynamicWork < 2e-8 && errors.energy < 2e-7,
        "water-driven release closes the pressure impulse and work", errors.energy);
}

void TestCoupledRejection(Checks& checks, const GridLayout& layout) {
    const auto box = ResolvedBox();
    const CoupledBoxState body{box.owner, box.lo, box.hi, kZero, 20.};
    const auto fluid = FilledTank(checks, layout, body);
    if (fluid.water.empty()) return;
    CoupledBoxStep next;
    if (!AdvanceBody(checks, "coupled rollback", layout, body, fluid, kZero, next)) return;
    const auto saved = next;
    std::string error;
    auto mixed = fluid;
    mixed.water[0] *= .75;
    checks.Expect(!AdvanceCoupledVolumeBox(layout, body, mixed, kDt, kZero, next, error) &&
        !error.empty() && SameCoupledStep(saved, next), "unsupported mixed phase rejects coupled publication");
    error.clear();
    auto outside = body;
    outside.velocity[0] = 1e6;
    checks.Expect(!AdvanceCoupledVolumeBox(layout, outside, fluid, kDt, kZero, next, error) &&
        !error.empty() && SameCoupledStep(saved, next), "inadmissible moving trajectory leaves prior fluid and box intact");

    error.clear();
    const CoupledBoxState unresolved{19, {.52, .52, .52}, {.64, .64, .64}, {.05, 0, 0}, 1.};
    const auto unresolvedFluid = FilledTank(checks, layout, unresolved);
    if (unresolvedFluid.water.empty()) return;
    checks.Expect(!AdvanceCoupledVolumeBox(layout, unresolved, unresolvedFluid, kDt, kZero, next, error) &&
        !error.empty() && SameCoupledStep(saved, next), "unresolved subcell body rejects without inventing pressure response");
}

struct WallLedger {
    GridVec momentum{};
    double energy = 0;
};

WallLedger ObserveMomentum(const GridLayout& layout, const BoxMotion& box,
                           const std::array<std::vector<double>, 3>& velocity) {
    WallLedger ledger;
    ForEachCell(layout, [&](auto cell, std::size_t) {
        const double halfMass = 500 * FluidCapacity(layout, cell, box, 0);
        for (int axis = 0; axis < 3; ++axis) {
            auto high = cell;
            ++high[axis];
            const double low = velocity[axis][layout.Face(axis, cell[0], cell[1], cell[2])];
            const double upper = velocity[axis][layout.Face(axis, high[0], high[1], high[2])];
            ledger.momentum[axis] += halfMass * (low + upper);
            ledger.energy += .5 * halfMass * (low * low + upper * upper);
        }
    });
    return ledger;
}

void TestTransportedBox(Checks& checks, const GridLayout& layout) {
    CoupledBoxState body{21, {.59375, .578125, .578125}, {.90625, .890625, .921875}, kZero, 0};
    body.mass = 650 * SolidVolume(body);
    const double initialHeight = body.lo[1];
    auto fluid = FilledTank(checks, layout, body);
    if (fluid.water.empty()) return;
    const GridVec gravity{0, -9.81, 0};
    CoupledBoxStep next;
    CoupledErrors errors;
    double wallImpulseError = 0, wallEnergyError = 0, advectiveLoss = 0;
    double largestWallImpulse = 0, maximumWallNormalSpeed = 0;
    std::string error;
    for (int frame = 0; frame < 6; ++frame) {
        const bool ok = AdvanceTransportedVolumeBox(layout, body, fluid, kDt, gravity, next, error);
        checks.Expect(ok, "joined pressure, volume and momentum step " + error);
        if (!ok) return;
        CheckCoupledStep(body, fluid, gravity, next, errors);
        const BoxMotion endpoint{body.owner, next.body.lo, next.body.hi, kZero};
        const auto sealed = ObserveMomentum(layout, endpoint, next.fluid.state.velocity);
        const auto advected = ObserveMomentum(layout, endpoint, next.transport.velocity);
        for (int axis = 0; axis < 3; ++axis) {
            const double applied = sealed.momentum[axis] - advected.momentum[axis];
            wallImpulseError = std::max({wallImpulseError,
                std::abs(applied - next.stationaryWallImpulse[axis]),
                std::abs(sealed.momentum[axis] - next.transport.beforeMomentum[axis] -
                         next.stationaryWallImpulse[axis])});
            largestWallImpulse = std::max(largestWallImpulse, std::abs(applied));
            ForEachFace(layout, axis, [&](auto face, std::size_t index) {
                if (face[axis] == 0 || face[axis] == layout.cells[axis]) {
                    maximumWallNormalSpeed = std::max(maximumWallNormalSpeed,
                        std::abs(next.fluid.state.velocity[axis][index]));
                }
            });
        }
        wallEnergyError = std::max(wallEnergyError,
            std::abs(advected.energy - sealed.energy - next.stationaryWallKineticLoss));
        advectiveLoss = std::max(advectiveLoss,
            next.transport.beforeKineticEnergy - next.transport.afterKineticEnergy);
        body = next.body;
        fluid = next.fluid.state;
    }
    const double travel = body.lo[1] - initialHeight;
    checks.Expect(travel > .01, "joined trajectory moves the buoyant box", travel);
    checks.Expect(errors.volume < kVolumeTolerance, "joined trajectory conserves water", errors.volume);
    checks.Expect(errors.impulse < 5e-8, "joined trajectory rigid impulse closes", errors.impulse);
    checks.Expect(errors.energy < 2e-7, "joined trajectory pressure projection energy closes", errors.energy);
    checks.Expect(advectiveLoss > 1e-8, "joined trajectory actually advects nonuniform momentum", advectiveLoss);
    checks.Expect(largestWallImpulse > 1e-8 && maximumWallNormalSpeed == 0,
        "joined transport resolves nonzero wall-normal momentum before publication", largestWallImpulse);
    checks.Expect(wallImpulseError < 2e-9, "sealed boundary accounts for all transported wall impulse", wallImpulseError);
    checks.Expect(wallEnergyError < 2e-9, "sealed boundary accounts for kinetic loss", wallEnergyError);

    // A fully blocked interior face is supported by pressure-only transport.
    // It still has no dual momentum ownership in the composed path.
    CoupledBoxState occluder{22, {.60, .41, .61}, {1.03, .81, .94}, kZero, 0};
    occluder.mass = 1000 * SolidVolume(occluder);
    const auto occludedFluid = FilledTank(checks, layout, occluder);
    if (occludedFluid.water.empty()) return;
    CoupledBoxStep pressureOnly;
    if (!AdvanceBody(checks, "occluder pressure control", layout, occluder, occludedFluid, kZero, pressureOnly)) return;
    const auto retained = next;
    checks.Expect(!AdvanceTransportedVolumeBox(layout, occluder, occludedFluid, kDt, kZero, next, error) &&
        error.find("open interior") != std::string::npos && SameCoupledStep(next, retained),
        "unsupported closed face cannot partially publish a joined step");
}

void TestOwnedBoxTopology(Checks& checks, const GridLayout& layout) {
    CoupledBoxState body{23, {.36, .40, .41}, {.68, .84, .749}, {0, 0, .25}, 500.};
    auto fluid = FilledTank(checks, layout, body);
    if (fluid.water.empty()) return;
    HalfDualVolumeState ownership;
    for (auto& field : ownership.velocity)
        field.resize(2 * layout.CellCount());
    const auto face = layout.Face(0, 2, 2, 2);
    const auto left = layout.Cell(1, 2, 2);
    const auto right = layout.Cell(2, 2, 2);
    CoupledErrors errors;
    double largestClosedDifference = 0;
    double maxTransportError = 0;
    bool closed = false, reopened = false;
    std::string error;
    for (int frame = 0; frame < 10; ++frame) {
        // A known external impulse reverses the box, while both fluid
        // owners keep the velocities they actually carried at closure.
        if (frame == 3)
            body.velocity[2] -= .65;
        OwnedCoupledBoxStep next;
        const bool ok = AdvanceOwnedCoupledVolumeBox(layout, body, fluid, ownership, kDt, kZero,
                                                      next, error);
        checks.Expect(ok, "joined side-owned moving pressure step " + error);
        if (!ok) return;
        CheckCoupledStep(body, fluid, kZero, next.box, errors);
        GridVec displacement;
        for (int axis = 0; axis < 3; ++axis) {
            displacement[axis] = next.box.body.lo[axis] - body.lo[axis];
            maxTransportError = std::max(maxTransportError,
                                          std::abs(next.transportMomentumError[axis]));
        }
        MovingGridGeometry geometry;
        if (!Build(checks, "independent owned endpoint", layout,
                   {{body.owner, body.lo, body.hi, displacement}}, geometry))
            return;
        const auto& sides = next.halfVelocity.velocity[0];
        const double difference = std::abs(sides[2 * left + 1] - sides[2 * right]);
        if (geometry.endAperture[0][face] == 0) {
            closed = true;
            largestClosedDifference = std::max(largestClosedDifference, difference);
        } else if (closed) {
            reopened = true;
            checks.Expect(difference == 0, "reopened face has one shared velocity", difference);
        }
        body = next.box.body;
        fluid = std::move(next.box.fluid.state);
        ownership = std::move(next.halfVelocity);
    }
    checks.Expect(closed && reopened && largestClosedDifference > 1e-7,
        "actual moving box closes and reopens a face with distinct side-owned state",
        largestClosedDifference);
    checks.Expect(errors.volume < kVolumeTolerance, "owned topology conserves water", errors.volume);
    checks.Expect(errors.impulse < 5e-8, "owned topology rigid impulse closes", errors.impulse);
    checks.Expect(errors.energy < 2e-7, "owned topology pressure work closes", errors.energy);
    checks.Expect(maxTransportError < 2e-9, "owned topology advection conserves momentum",
        maxTransportError);
}

bool Benchmark(const GridLayout& layout) {
    constexpr int samples = 60;
    BoxMotion box = InteriorBox();
    MovingGridGeometry geometry;
    MovingVolumeState state;
    std::vector<double> milliseconds;
    milliseconds.reserve(samples);
    std::string error;
    for (int frame = 0; frame < samples; ++frame) {
        if (frame && frame % 8 == 0) box.translation[0] = -box.translation[0];
        const auto start = std::chrono::steady_clock::now();
        if (!BuildMovingGridGeometry(layout, {box}, geometry, error)) {
            std::cerr << error << '\n';
            return false;
        }
        if (frame == 0) state = AtRest(layout, geometry.startCapacity);
        MovingVolumeStep next;
        if (!AdvanceMovingVolume(layout, geometry, state, kDt, kZero, next, error)) {
            std::cerr << error << '\n';
            return false;
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        milliseconds.push_back(std::chrono::duration<double, std::milli>(elapsed).count());
        std::cout << frame << ',' << milliseconds.back() << ',' << next.iterations << ','
                  << next.residual << ',' << next.maxCfl << ',' << Sum(next.state.water) << '\n';
        state = std::move(next.state);
        Translate(box);
    }
    std::sort(milliseconds.begin(), milliseconds.end());
    std::cout << "BENCH cells=" << layout.CellCount() << " samples=" << samples
              << " mean_ms=" << Sum(milliseconds) / samples
              << " p95_ms=" << milliseconds[56] << " worst_ms=" << milliseconds.back() << '\n';
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::cout << std::setprecision(17);
    const GridLayout layout{{8, 7, 7}, 0.25};
    if (argc == 2 && std::string(argv[1]) == "bench") return Benchmark(layout) ? 0 : 1;
    if (argc != 1) return 2;

    Checks checks;
    for (double tracer : {0.0, 0.4, 1.0}) TestUniformTransport(checks, layout, tracer);
    TestLayeredMotion(checks, layout);
    TestRestAndPerturbation(checks, layout);
    TestObliqueRest(checks, layout);
    TestRejection(checks, layout);
    TestCoupledBuoyancy(checks, layout);
    TestCoupledFlow(checks, layout);
    TestCoupledRejection(checks, layout);
    TestTransportedBox(checks, layout);
    TestOwnedBoxTopology(checks, layout);
    std::cout << "SUMMARY checks=" << checks.total << " failures=" << checks.failures << '\n';
    return checks.failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
