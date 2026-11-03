#include "GridGeometricMomentum.h"

#include "ExposureVolumePressure.h"
#include "OwnedEvolvingVolume.h"

#include <cstdio>
#include <limits>
#include <numeric>

namespace {

using namespace judas::volume_grid;

struct Checks {
    int total = 0, failed = 0;
    void Expect(bool pass, const char* label, double value = 0.) {
        ++total;
        failed += !pass;
        std::printf("%s %s %.17g\n", pass ? "PASS" : "FAIL", label, value);
    }
};

HalfDualVolumeState Owned(const GridLayout& layout, GridVec speed = {}) {
    HalfDualVolumeState state;
    for (int axis = 0; axis < 3; ++axis)
        state.velocity[axis].assign(2 * layout.CellCount(), speed[axis]);
    return state;
}

MovingVolumeStep EmptyProjection(const GridLayout& layout) {
    MovingVolumeStep result;
    for (int axis = 0; axis < 3; ++axis) {
        const auto count = layout.FaceCount(axis);
        result.flux[axis].resize(count);
        result.materialMassFlux[axis].resize(count);
        result.waterFlux[axis].resize(count);
        result.gravityVelocityIncrement[axis].resize(count);
        result.pressureInertialFace[axis].resize(count);
    }
    return result;
}

void PressureWork(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, 1.};
    const BoxMotion block{1, {.625, 1.125, 1.125}, {.875, 1.375, 1.625}, {}};
    MovingVolumeState material;
    material.water.resize(layout.CellCount());
    std::vector<double> planes(layout.CellCount());
    for (int z = 0; z < 4; ++z) {
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                const auto cell = layout.Cell(x, y, z);
                material.water[cell] = .25;
                planes[cell] = y + .25;
            }
        }
    }
    // A 1/64 m³ wet portion of the interior block cuts the x-face's
    // left half. The face itself stays entirely open.
    material.water[layout.Cell(0, 1, 1)] -= 1. / 64.;
    const auto fill = [&](int x, int y, int z, double fraction) {
        const auto cell = layout.Cell(x, y, z);
        material.water[cell] = fraction;
        planes[cell] = y + fraction;
    };
    fill(1, 1, 1, .75);
    fill(2, 2, 2, .75);
    fill(2, 1, 3, .875);

    struct Link {
        std::size_t face, leftHalf, rightHalf;
        double leftMass, rightMass, leftSpeed, rightSpeed, gravity, pressure;
    };
    const std::array<Link, 3> links{{
        {layout.Face(0, 1, 1, 1), 2 * layout.Cell(0, 1, 1) + 1, 2 * layout.Cell(1, 1, 1),
         109.734375, 375.125, 1.25, -.5, .125, -.25},
        {layout.Face(1, 2, 2, 2), 2 * layout.Cell(2, 1, 2) + 1, 2 * layout.Cell(2, 2, 2),
         .5, 500., -.25, .75, -.25, .5},
        {layout.Face(2, 2, 1, 3), 2 * layout.Cell(2, 1, 2) + 1, 2 * layout.Cell(2, 1, 3),
         125.375, 437.5625, -1.25, .25, .5, .75},
    }};
    auto owned = Owned(layout);
    for (int axis = 0; axis < 3; ++axis) {
        const auto& link = links[axis];
        owned.velocity[axis][link.leftHalf] = link.leftSpeed;
        owned.velocity[axis][link.rightHalf] = link.rightSpeed;
    }
    GridGeometricMomentumStart start;
    MovingGridGeometry geometry;
    std::string error;
    const bool built = BuildMovingGridGeometry(layout, {block}, geometry, error) &&
        BuildGridGeometricMomentumStart(layout, geometry, {block}, 1, planes, material, owned,
                                       start, error);
    checks.Expect(built, built ? "build exact three-axis pressure owners" : error.c_str());
    if (!built)
        return;

    auto projected = EmptyProjection(layout);
    constexpr double dt = .125;
    GridVec expectedImpulse{}, expectedGravity{};
    double expectedWork = 0., expectedCorrection = 0., massError = 0., predictorError = 0.;
    for (int axis = 0; axis < 3; ++axis) {
        const auto& link = links[axis];
        const double mass = link.leftMass + link.rightMass;
        const double predictor = (link.leftMass * link.leftSpeed + link.rightMass * link.rightSpeed) / mass;
        massError = std::max(massError, std::abs(start.faceMass[axis][link.face] - mass));
        predictorError = std::max(predictorError,
            std::abs(start.predictor.velocity[axis][link.face] - predictor));
        projected.pressureInertialFace[axis][link.face] = true;
        projected.gravityVelocityIncrement[axis][link.face] = link.gravity;
        projected.flux[axis][link.face] = dt * (predictor + link.gravity + link.pressure);
        expectedImpulse[axis] = mass * link.pressure;
        expectedGravity[axis] = mass * link.gravity;
        expectedCorrection += .5 * mass * link.pressure * link.pressure;
        const auto ownerWork = [&](double ownerMass, double speed) {
            const double free = speed + link.gravity, corrected = free + link.pressure;
            return .5 * ownerMass * (corrected * corrected - free * free);
        };
        expectedWork += ownerWork(link.leftMass, link.leftSpeed) +
                        ownerWork(link.rightMass, link.rightSpeed);
    }
    checks.Expect(massError == 0. && predictorError < 1e-15,
                  "known masses include cut material and distinct wet and dry halves", massError);
    projected.pressureKineticWork = expectedWork;
    projected.pressureCorrectionEnergy = expectedCorrection;
    HalfDualPressureUpdate result;
    const bool applied = ApplyGridGeometricPressure(layout, geometry, start, projected, owned,
                                                   dt, result, error);
    checks.Expect(applied, applied ? "map independently manufactured pressure impulse" : error.c_str());
    if (!applied)
        return;
    double errorImpulse = 0., errorSpeed = 0.;
    for (int axis = 0; axis < 3; ++axis) {
        const auto& link = links[axis];
        for (std::size_t half = 0; half < owned.velocity[axis].size(); ++half) {
            const bool linked = half == link.leftHalf || half == link.rightHalf;
            const double expected = owned.velocity[axis][half] +
                (linked ? link.gravity + link.pressure : 0.);
            errorSpeed = std::max(errorSpeed, std::abs(result.state.velocity[axis][half] - expected));
        }
        errorImpulse = std::max({errorImpulse, std::abs(result.impulse[axis] - expectedImpulse[axis]),
            std::abs(result.gravityImpulse[axis] - expectedGravity[axis])});
    }
    checks.Expect(errorSpeed < 1e-14 && errorImpulse < 1e-11,
                  "impulse reaches exactly the two geometric owners on each axis", errorImpulse);
    checks.Expect(std::abs(result.kineticChange - expectedWork) < 1e-11 &&
                  std::abs(result.correctionEnergy - expectedCorrection) < 1e-11,
                  "pressure work equals independent per-owner kinetic change", result.kineticChange - expectedWork);

    const auto saved = result;
    projected.pressureKineticWork += .125;
    checks.Expect(!ApplyGridGeometricPressure(layout, geometry, start, projected, owned, dt, result, error) &&
                  error.find("work metric") != std::string::npos && result.state.velocity == saved.state.velocity,
                  "inconsistent pressure work rejects without changing momentum");

    const auto savedMass = start.faceMass;
    material.water[0] = std::numeric_limits<double>::quiet_NaN();
    checks.Expect(!BuildGridGeometricMomentumStart(layout, geometry, {block}, 1, planes, material,
                   owned, start, error) && start.faceMass == savedMass,
                  "nonfinite authoritative water rejects without changing pressure owners");
}

std::vector<double> FullPlanes(const GridLayout& layout) {
    std::vector<double> planes(layout.CellCount());
    for (int z = 0; z < layout.cells[2]; ++z)
        for (int y = 0; y < layout.cells[1]; ++y)
            for (int x = 0; x < layout.cells[0]; ++x)
                planes[layout.Cell(x, y, z)] = (y + 1) * layout.h;
    return planes;
}

void WetPiston(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, .25};
    const BoxMotion piston{1, {.25, .5, .25}, {.75, .875, .75}, {0., .04, 0.}};
    auto stationary = piston;
    stationary.translation = {};
    constexpr double dt = .05;
    const GridVec speed{.25, -.5, .75};
    const auto owned = Owned(layout, speed);
    const auto planes = FullPlanes(layout);
    MovingGridGeometry geometry;
    std::string error;
    if (!BuildMovingGridGeometry(layout, {piston}, geometry, error)) {
        checks.Expect(false, error.c_str());
        return;
    }
    MovingVolumeState material;
    material.water = geometry.startCapacity;
    for (int axis = 0; axis < 3; ++axis)
        material.velocity[axis].resize(layout.FaceCount(axis));
    GridGeometricMomentumStart start;
    MovingVolumeStep projected;
    const bool built = BuildGridGeometricMomentumStart(layout, geometry, {stationary}, 1, planes,
                           material, owned, start, error) &&
                       AdvanceExposureVolumePressure(layout, geometry, material, dt, true, projected, error);
    checks.Expect(built, built ? "pressure projects a fully wet piston with initially solid cells" : error.c_str());
    if (!built)
        return;
    const auto born = layout.Cell(1, 2, 1), lateral = layout.Face(0, 2, 2, 1);
    const double bornMass = 1000. * .25 * .25 * .04;
    checks.Expect(start.faceMass[0][lateral] == 0. && geometry.averageAperture[0][lateral] > 0. &&
                  start.storage[1].capacity[born] == std::array<double, 2>{} &&
                  std::abs(1000. * projected.state.water[born] - bornMass) < 1e-11,
                  "birth gains known water mass across an initially massless lateral connection");

    // This independent transport oracle has no air. Confirm that the actual
    // pressure result agrees, then state the exact single-phase ledger rather
    // than inferring a tiny air flux by subtracting nearly equal rounded values.
    double volumeError = 0., fluxError = 0.;
    for (std::size_t cell = 0; cell < layout.CellCount(); ++cell)
        volumeError = std::max(volumeError, std::abs(projected.state.water[cell] - geometry.endCapacity[cell]));
    for (int axis = 0; axis < 3; ++axis)
        for (std::size_t face = 0; face < layout.FaceCount(axis); ++face)
            fluxError = std::max(fluxError, std::abs(projected.materialMassFlux[axis][face] -
                                                    1000. * projected.flux[axis][face]));
    checks.Expect(volumeError < 1e-12 && fluxError < 1e-10,
                  "fully wet pressure output agrees with its exact phase ledger", volumeError);
    projected.state.water = geometry.endCapacity;
    projected.waterFlux = projected.flux;

    auto eliminated = EmptyProjection(layout);
    eliminated.flux[0][lateral] = .0001;
    HalfDualPressureUpdate pressure;
    const bool mapped = ApplyGridGeometricPressure(layout, geometry, start, eliminated, owned,
                                                  dt, pressure, error);
    checks.Expect(mapped && pressure.state.velocity == owned.velocity && pressure.impulse == GridVec{},
                  "eliminated pressure direction can carry volume without inventing initial impulse");
    eliminated.pressureInertialFace[0][lateral] = true;
    checks.Expect(!ApplyGridGeometricPressure(layout, geometry, start, eliminated, owned, dt, pressure, error) &&
                  pressure.state.velocity == owned.velocity,
                  "assigning fictitious initial inertia to the same direction rejects transactionally");

    GridGeometricTransport result;
    const bool advected = AdvectGridGeometricMomentum(layout, geometry, {piston}, 1, planes, start,
                                                     projected, owned, result, error);
    checks.Expect(advected, advected ? "geometric momentum crosses a genuine cell birth" : error.c_str());
    if (!advected)
        return;
    const auto& vertical = result.merged.state.velocity[1];
    checks.Expect(std::abs(vertical[2 * born] - speed[1]) < 1e-13 && vertical[2 * born + 1] == 0.,
                  "newborn lower half inherits momentum and solid upper half remains empty", vertical[2 * born]);
    double maximumResidual = 0.;
    for (const double residual : result.momentumResidual)
        maximumResidual = std::max(maximumResidual, std::abs(residual));
    checks.Expect(maximumResidual < 1e-10,
                  "uniform momentum transport conserves impulse before endpoint constraints", maximumResidual);
    // Two exterior faces contain 32 wall-normal half-owners per component.
    // The rising piston cuts .5 × .5 × .04 m³ into the top row's high halves.
    const GridVec wallMass{250., 240., 250.};
    double expectedWallLoss = 0., wallImpulseError = 0.;
    for (int axis = 0; axis < 3; ++axis) {
        expectedWallLoss += .5 * wallMass[axis] * speed[axis] * speed[axis];
        wallImpulseError = std::max(wallImpulseError,
            std::abs(result.merged.stationaryWallImpulse[axis] + wallMass[axis] * speed[axis]));
    }
    checks.Expect(std::abs(result.kineticChange) < 1e-10 && result.merged.mergeKineticLoss < 1e-10 &&
                  std::abs(result.merged.stationaryWallKineticLoss - expectedWallLoss) < 1e-10 &&
                  wallImpulseError < 1e-10,
                  "uniform transport changes energy only through the known exterior wall reaction",
                  result.kineticChange);
    auto rounded = projected;
    const auto first = std::find_if(rounded.flux[0].begin(), rounded.flux[0].end(),
                                    [](double flux) { return flux != 0.; });
    if (first == rounded.flux[0].end()) {
        checks.Expect(false, "roundoff control requires a nonzero fully wet transverse transfer");
        return;
    }
    const auto face = std::size_t(first - rounded.flux[0].begin());
    rounded.waterFlux[0][face] = std::nextafter(*first, 0.);
    rounded.materialMassFlux[0][face] = *first + 999. * rounded.waterFlux[0][face];
    const bool reconciled = AdvectGridGeometricMomentum(layout, geometry, {piston}, 1, planes, start,
                                                       rounded, owned, result, error);
    checks.Expect(reconciled && result.phaseFluxRoundoff > 0. &&
                  result.phaseFluxRoundoff < 1e-12 && std::abs(result.kineticChange) < 1e-10,
                  "a one-ulp phase complement is accounted without a fictitious stream",
                  result.phaseFluxRoundoff);
}

void WrongPhase(Checks& checks) {
    const GridLayout layout{{3, 3, 3}, 1.};
    const BoxMotion wall{1, {1.875, 1., 1.}, {2.125, 1.25, 2.}, {}};
    const auto owned = Owned(layout);
    std::vector<double> planes(layout.CellCount());
    MovingVolumeState material;
    material.water.resize(layout.CellCount());
    for (int z = 0; z < 3; ++z)
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x)
                planes[layout.Cell(x, y, z)] = y;
    const auto lowLeft = layout.Cell(1, 1, 1), lowRight = layout.Cell(2, 1, 1);
    for (const auto cell : {lowLeft, lowRight}) {
        planes[cell] = 1.25;
        material.water[cell] = .25 - .125 * .25;
    }
    GridGeometricMomentumStart start;
    MovingGridGeometry geometry;
    std::string error;
    const bool built = BuildMovingGridGeometry(layout, {wall}, geometry, error) &&
        BuildGridGeometricMomentumStart(layout, geometry, {wall}, 1, planes, material, owned, start, error);
    checks.Expect(built, built ? "build stratified phase-circulation witness" : error.c_str());
    if (!built)
        return;

    auto step = EmptyProjection(layout);
    step.state = material;
    constexpr double flux = .0001;
    const auto setAir = [&](int axis, int x, int y, double amount) {
        const auto face = layout.Face(axis, x, y, 1);
        step.flux[axis][face] = amount;
        step.materialMassFlux[axis][face] = amount;
    };
    setAir(0, 2, 1, -flux);
    setAir(0, 2, 2, flux);
    setAir(1, 1, 2, flux);
    setAir(1, 2, 2, -flux);
    GridGeometricTransport result;
    const bool advected = AdvectGridGeometricMomentum(layout, geometry, {wall}, 1, planes, start,
                                                     step, owned, result, error);
    checks.Expect(advected, advected ? "valid air moves around the same four-face loop" : error.c_str());
    if (!advected)
        return;

    // Both cells have water to spend, and the endpoint inventories close.
    // Their shared face is open to air but solid across the entire wet trace.
    const auto wrong = layout.Face(0, 2, 1, 1);
    step.materialMassFlux[0][wrong] = -1000. * flux;
    step.waterFlux[0][wrong] = -flux;
    step.state.water[lowLeft] += flux;
    step.state.water[lowRight] -= flux;
    const auto saved = result.merged.state.velocity;
    checks.Expect(!AdvectGridGeometricMomentum(layout, geometry, {wall}, 1, planes, start, step,
                      owned, result, error) && error.find("no transverse half-face support") != std::string::npos &&
                      result.merged.state.velocity == saved,
                  "balanced ledger cannot send water through a dry transverse trace");
}

void MidpointDam(Checks& checks) {
    const GridLayout layout{{3, 3, 3}, 1.};
    const auto left = layout.Cell(0, 1, 1), middle = layout.Cell(1, 1, 1), right = layout.Cell(2, 1, 1);
    const auto owned = Owned(layout, {.25, 0., 0.});
    constexpr double flux = .0625;
    GridGeometricTransport result;
    for (const bool wetOpening : {true, false}) {
        const double top = wetOpening ? 1.25 : 1.75;
        const BoxMotion dam{1, {1.4375, .75, .75}, {1.5625, top, 2.25}, {}};
        std::vector<double> planes(layout.CellCount());
        MovingVolumeState material;
        material.water.resize(layout.CellCount());
        for (int z = 0; z < 3; ++z)
            for (int y = 0; y < 3; ++y)
                for (int x = 0; x < 3; ++x)
                    planes[layout.Cell(x, y, z)] = y;
        for (const auto cell : {left, middle, right})
            planes[cell] = 1.5;
        material.water[left] = material.water[right] = .5;
        material.water[middle] = .5 - .125 * (std::min(top, 1.5) - 1.);

        GridGeometricMomentumStart start;
        MovingGridGeometry geometry;
        GridHalfCellApertures apertures;
        std::string error;
        const bool built = BuildMovingGridGeometry(layout, {dam}, geometry, error) &&
            BuildGridGeometricMomentumStart(layout, geometry, {dam}, 1, planes, material, owned, start, error) &&
            AverageGridHalfCellApertures(layout, {dam}, 1, 0, planes, apertures, error);
        checks.Expect(built, built ? "build a stationary cut inside the momentum cell" : error.c_str());
        if (!built)
            return;
        const auto& opening = apertures.midpoint[middle];
        checks.Expect(opening.open == (wetOpening ? .75 : .25) &&
                      opening.leftWet == (wetOpening ? .25 : 0.) && opening.rightWet == opening.leftWet,
                      wetOpening ? "lower dam has known wet midpoint aperture"
                                 : "higher dam has known air-only midpoint aperture", opening.leftWet);

        // Both coarse faces are wet and the middle inventory does not change.
        // The x-velocity halves still need +flux across the midpoint to balance.
        auto projected = EmptyProjection(layout);
        projected.state = material;
        projected.state.water[left] -= flux;
        projected.state.water[right] += flux;
        for (const int x : {1, 2}) {
            const auto face = layout.Face(0, x, 1, 1);
            projected.flux[0][face] = projected.waterFlux[0][face] = flux;
            projected.materialMassFlux[0][face] = 1000. * flux;
            const auto returnFace = layout.Face(0, x, 2, 1);
            projected.flux[0][returnFace] = projected.materialMassFlux[0][returnFace] = -flux;
        }
        const auto leftReturn = layout.Face(1, 0, 2, 1);
        const auto rightReturn = layout.Face(1, 2, 2, 1);
        projected.flux[1][leftReturn] = projected.materialMassFlux[1][leftReturn] = -flux;
        projected.flux[1][rightReturn] = projected.materialMassFlux[1][rightReturn] = flux;
        const auto prior = result.merged.state.velocity;
        const bool accepted = AdvectGridGeometricMomentum(layout, geometry, {dam}, 1, planes,
                                                           start, projected, owned, result, error);
        if (wetOpening) {
            checks.Expect(accepted, accepted ? "wet midpoint passes the balanced transfer" : error.c_str());
            if (accepted) {
                const auto& u = result.merged.state.velocity[0];
                checks.Expect(std::abs(u[2 * middle] - .25) < 1e-14 &&
                              std::abs(u[2 * middle + 1] - .25) < 1e-14 &&
                              std::abs(result.momentumResidual[0]) < 1e-12,
                              "wet midpoint conserves uniform material momentum", result.momentumResidual[0]);
            }
        } else {
            checks.Expect(!accepted && error.find("midpoint") != std::string::npos &&
                          result.merged.state.velocity == prior,
                          accepted || error.find("midpoint") != std::string::npos
                              ? "balanced water ledger cannot cross a stationary air-only midpoint"
                              : error.c_str());
        }
    }
}

MovingVolumeState SeedSurface(const GridLayout& layout, const BoxMotion& box, double height,
                              std::string& error) {
    MovingVolumeState result;
    MovingGridGeometry shape;
    GridPhaseGeometry phase;
    if (!BuildMovingGridGeometry(layout, {box}, shape, error) ||
        !BuildGridPhaseGeometry(layout, {box}, 1, height, phase, error))
        return result;
    result.water = std::move(phase.water);
    for (int a = 0; a < 3; ++a)
        result.velocity[a].resize(layout.FaceCount(a));
    for (std::size_t cell = 0; cell < layout.CellCount(); ++cell)
        result.water[cell] = std::min(result.water[cell], shape.startCapacity[cell]);
    return result;
}

void EvolvingPressureAndTransport(Checks& checks) {
    const GridLayout layout{{8, 8, 8}, .125};
    const BoxMotion body{4, {.25, .5, .25}, {.75, .875, .75}, {}};
    std::string error;
    auto material = SeedSurface(layout, body, .94, error);
    auto moving = body;
    moving.translation[1] = .004;
    OwnedEvolvingVolumeStep next;
    const bool stepped = AdvanceGeometricEvolvingVolume(layout, {moving}, material, Owned(layout),
                                                        1. / 60., 1, -9.81, next, error);
    checks.Expect(stepped, stepped ? "mixed grid opens a submerged cavity in the coupled transport"
                                  : error.c_str());
    if (!stepped)
        return;
    const auto volume = [](const auto& v) { return std::accumulate(v.begin(), v.end(), 0.); };
    double cavity = 0.;
    double largestOwnedSpeed = 0.;
    bool upperHalfRemainsSolid = true;
    bool allInletsKeepInertia = true;
    double inletConstitutiveError = 0., inletPressureDrop = 0.;
    constexpr double dt = 1. / 60.;
    const double inletArea = layout.h * layout.h;
    const double inletMass = .5 * inletArea * layout.h * 1000.;
    for (int z = 2; z < 6; ++z) {
        for (int x = 2; x < 6; ++x) {
            const auto cell = layout.Cell(x, 4, z);
            const auto donor = layout.Cell(x, 3, z);
            const auto inlet = layout.Face(1, x, 4, z);
            cavity += next.phase.fluid.state.water[cell];
            const auto& vertical = next.halfVelocity.velocity[1];
            largestOwnedSpeed = std::max(largestOwnedSpeed, std::abs(vertical[2 * cell]));
            upperHalfRemainsSolid = upperHalfRemainsSolid && vertical[2 * cell + 1] == 0.;
            const auto& pressure = next.phase.fluid;
            allInletsKeepInertia = allInletsKeepInertia && pressure.pressureInertialFace[1][inlet];
            const double drop = pressure.pressure[donor] - pressure.pressure[cell];
            const double scale = dt * inletArea;
            const double expected = scale * (pressure.gravityVelocityIncrement[1][inlet] +
                                             scale * drop / inletMass);
            inletConstitutiveError = std::max(inletConstitutiveError,
                std::abs(pressure.flux[1][inlet] - expected));
            inletPressureDrop = std::max(inletPressureDrop, std::abs(drop));
        }
    }
    constexpr double expectedCavity = .5 * .5 * .004;
    checks.Expect(std::abs(cavity - expectedCavity) < 1e-12 &&
                  std::abs(volume(material.water) - volume(next.phase.fluid.state.water)) < 1e-11,
                  "new wet cavity and entire mixed tank close their inventories", cavity);
    checks.Expect(largestOwnedSpeed > .01 && upperHalfRemainsSolid,
                  "new material inherits motion below the still-solid half", largestOwnedSpeed);
    checks.Expect(allInletsKeepInertia && inletPressureDrop > 1. && inletConstitutiveError < 1e-12,
                  "new cell's finite-mass inlet obeys its own pressure-drop relation",
                  inletConstitutiveError);
    checks.Expect(std::abs(next.transportMomentumError[1]) < 1e-12 &&
                  next.phase.fluid.pressureKineticWork > .1 && next.advectiveKineticChange <= 1e-12,
                  "projection and phase remap preserve their impulse and energy ledgers",
                  next.transportMomentumError[1]);
}

void DryGapWithGeometricMomentum(Checks& checks) {
    const GridLayout layout{{8, 8, 8}, .125};
    const BoxMotion body{8, {.22, .404, .3}, {.63, .64, .7}, {}};
    std::string error;
    const auto wet = SeedSurface(layout, body, .4, error);
    auto air = wet;
    std::fill(air.water.begin(), air.water.end(), 0.);
    OwnedEvolvingVolumeStep aboveWater, airOnly;
    const bool stepped = AdvanceGeometricEvolvingVolume(layout, {body}, wet, Owned(layout),
                                                         1. / 60., 1, -9.81, aboveWater, error) &&
        AdvanceGeometricEvolvingVolume(layout, {body}, air, Owned(layout),
                                       1. / 60., 1, -9.81, airOnly, error);
    checks.Expect(stepped, stepped ? "same-cell four-millimetre dry gap survives geometric momentum"
                                  : error.c_str());
    if (!stepped)
        return;
    double forceError = 0.;
    for (int a = 0; a < 3; ++a)
        forceError = std::max(forceError,
            std::abs(aboveWater.phase.loads.front().force[a] - airOnly.phase.loads.front().force[a]));
    checks.Expect(forceError < 1e-11 && aboveWater.phase.fluid.iterations == 0,
                  "water does no work across the dry gap compared with an air-only control", forceError);
}

void LateOpeningImpulse(Checks& checks) {
    const GridLayout layout{{8, 8, 8}, .125};
    const BoxMotion body{9, {.25, .49, .25}, {.75, .865, .75}, {}};
    constexpr double dt = 1. / 60., gap = .01, epsilon = 1e-6;
    std::string error;
    const auto material = SeedSurface(layout, body, .94, error);
    auto closing = body;
    auto opening = body;
    closing.translation[1] = gap - epsilon;
    opening.translation[1] = gap + epsilon;
    OwnedEvolvingVolumeStep before, after;
    const bool solved = AdvanceGeometricEvolvingVolume(layout, {closing}, material, Owned(layout),
                                                        dt, 1, 0., before, error) &&
        AdvanceGeometricEvolvingVolume(layout, {opening}, material, Owned(layout),
                                       dt, 1, 0., after, error);
    checks.Expect(solved, solved ? "prescribed paths approach opening from both sides" : error.c_str());
    if (!solved)
        return;
    const auto face = layout.Face(1, 3, 4, 3);
    const auto lower = layout.Cell(3, 3, 3), upper = layout.Cell(3, 4, 3);
    const double area = layout.h * layout.h;
    const double occupiedLength = .49 - .4375;
    const double drop = after.phase.fluid.pressure[lower] - after.phase.fluid.pressure[upper];
    // Q=A*epsilon and open-time fraction=epsilon/(gap+epsilon).
    // The duration-weighted inlet inertia gives finite pressure as epsilon→0.
    const double expected = 1000. * occupiedLength * (gap + epsilon) / (dt * dt);
    checks.Expect(std::abs(after.phase.fluid.flux[1][face] - area * epsilon) < 1e-15 &&
                  std::abs(drop - expected) < 1e-5,
                  "late inlet pressure follows independent open-duration inertia", drop);
    const double jump = dt * std::abs(before.phase.loads.front().force[1] -
                                     after.phase.loads.front().force[1]);
    checks.Expect(jump < .02 && after.geometricPressureMixingLoss > 0.,
                  "a late opening has no finite impulse jump as its duration vanishes", jump);
}

void RepresentedEndpoint(Checks& checks) {
    const GridLayout layout{{5, 5, 5}, .2};
    const BoxMotion body{10, {.31, .42, .39}, {.63, .69, .62}, {}};
    MovingGridGeometry geometry;
    std::string error;
    if (!BuildMovingGridGeometry(layout, {body}, geometry, error)) {
        checks.Expect(false, error.c_str());
        return;
    }
    const auto cut = layout.Cell(2, 2, 2);
    auto swept = geometry.startCapacity;
    swept[cut] = std::nextafter(swept[cut], std::numeric_limits<double>::infinity());
    GridCellPhase phase;
    checks.Expect(!ReconstructGridCellPhase(layout, {body}, 1, swept, phase, error),
                  "strict phase reconstruction rejects a swept endpoint overfill");
    GridCellPhaseReconciliation reconciliation{swept, {}};
    const bool published = ReconstructGridCellPhase(layout, {body}, 1, swept, phase, error,
                                                    &reconciliation);
    checks.Expect(published && reconciliation.water == geometry.startCapacity &&
                  reconciliation.volumeRoundoff == swept[cut] - geometry.startCapacity[cut],
                  "verified endpoint representation preserves exact fully wet ownership",
                  reconciliation.volumeRoundoff);
    const auto previous = reconciliation.water;
    swept[cut] += 1e-6;
    checks.Expect(!ReconstructGridCellPhase(layout, {body}, 1, swept, phase, error, &reconciliation) &&
                  reconciliation.water == previous,
                  "real endpoint capacity mismatch rejects without publishing a correction");
}

} // namespace

int main() {
    Checks checks;
    PressureWork(checks);
    WetPiston(checks);
    WrongPhase(checks);
    MidpointDam(checks);
    EvolvingPressureAndTransport(checks);
    DryGapWithGeometricMomentum(checks);
    LateOpeningImpulse(checks);
    RepresentedEndpoint(checks);
    std::printf("SUMMARY geometric momentum %d checks %d failures\n", checks.total, checks.failed);
    return checks.failed ? 1 : 0;
}
