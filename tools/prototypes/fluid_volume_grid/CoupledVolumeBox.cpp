#include "CoupledVolumeBox.h"

#include "GridHydrostaticReaction.h"
#include "GridPressureReaction.h"
#include "MovingVolumePressureInternal.h"
#include "PlanarMovingVolume.h"

namespace judas::volume_grid {
namespace {

double MaxAbs(const GridVec& value) {
    return std::max({std::abs(value[0]), std::abs(value[1]), std::abs(value[2])});
}

double Dot(const GridVec& a, const GridVec& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

bool SolveTangent(std::array<GridVec, 3> matrix, GridVec rhs, GridVec& direction) {
    for (int col = 0; col < 3; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 3; ++row)
            if (std::abs(matrix[row][col]) > std::abs(matrix[pivot][col]))
                pivot = row;
        if (!(std::abs(matrix[pivot][col]) > std::numeric_limits<double>::epsilon()))
            return false;
        std::swap(matrix[col], matrix[pivot]);
        std::swap(rhs[col], rhs[pivot]);
        for (int row = col + 1; row < 3; ++row) {
            const double scale = matrix[row][col] / matrix[col][col];
            for (int k = col + 1; k < 3; ++k)
                matrix[row][k] -= scale * matrix[col][k];
            rhs[row] -= scale * rhs[col];
        }
    }
    for (int row = 2; row >= 0; --row) {
        for (int col = row + 1; col < 3; ++col)
            rhs[row] -= matrix[row][col] * direction[col];
        direction[row] = rhs[row] / matrix[row][row];
        if (!std::isfinite(direction[row]))
            return false;
    }
    return true;
}

bool AdvanceBox(const GridLayout& layout, const CoupledBoxState& body,
                const MovingVolumeState& fluid, double dt, const GridVec& gravity,
                bool transport, int phaseAxis, CoupledBoxStep& output, std::string& error) {
    if (!std::isfinite(dt) || dt <= 0 || !std::isfinite(body.mass) || body.mass <= 0) {
        error = "invalid coupled box mass or timestep";
        return false;
    }
    double displacedVolume = 1.;
    GridVec freeVelocity, gravityDisplacement;
    for (int axis = 0; axis < 3; ++axis) {
        const double extent = body.hi[axis] - body.lo[axis];
        if (!(extent > layout.h) || !std::isfinite(extent) || !std::isfinite(body.velocity[axis]) ||
            !std::isfinite(gravity[axis])) {
            error = "coupled box extents must exceed one cell, with finite velocity and gravity";
            return false;
        }
        displacedVolume *= extent;
        freeVelocity[axis] = body.velocity[axis] + dt * gravity[axis];
    }
    const double displacedMass = 1000 * displacedVolume;
    if (!(displacedMass > 0) || !std::isfinite(displacedMass)) {
        error = "coupled box displacement is not representable";
        return false;
    }

    MovingGridGeometry initial;
    if (!BuildMovingGridGeometry(layout, {{body.owner, body.lo, body.hi, {}}}, initial, error))
        return false;
    if (fluid.water.size() != initial.startCapacity.size()) {
        error = "coupled box material state does not match the grid";
        return false;
    }
    const bool fullyWet = phaseAxis < 0;
    MovingVolumeState fullWater = fluid;
    const double cellVolume = layout.h * layout.h * layout.h;
    const double phaseTolerance = 1e-12 * cellVolume;
    double inputRoundoff = 0;
    for (std::size_t i = 0; fullyWet && i < fluid.water.size(); ++i) {
        const double difference = std::abs(fluid.water[i] - initial.startCapacity[i]);
        if (!std::isfinite(difference) || difference > phaseTolerance || fluid.water[i] < 0) {
            error = "coupled box currently requires a completely water-filled tank";
            return false;
        }
        inputRoundoff += difference;
    }
    if (fullyWet)
        fullWater.water = initial.startCapacity;
    for (int axis = 0; axis < 3; ++axis) {
        // Rest with equal density starts with exactly zero predictor
        // displacement. The uniform hydrostatic gradient was factored out.
        gravityDisplacement[axis] = fullyWet ? dt *
            (body.velocity[axis] + dt * gravity[axis] * ((body.mass - displacedMass) / body.mass))
            : dt * freeVelocity[axis];
    }

    int evaluations = 0;
    struct Sample {
        MovingGridGeometry geometry;
        MovingVolumeStep fluid;
        GridVec force{}, hydrostaticForce{}, residual{};
    };
    const auto evaluate = [&](const GridVec& displacement, Sample& sample) {
        ++evaluations;
        const BoxMotion motion{body.owner, body.lo, body.hi, displacement};
        if (!fullyWet) {
            PlanarMovingStep phase;
            if (!AdvanceEvolvingVolume(layout, {motion}, fluid, dt, phaseAxis, gravity[phaseAxis],
                                       phase, error))
                return false;
            if (phase.loads.size() != 1 || phase.loads.front().owner != body.owner) {
                error = "coupled surface reaction lost its solid owner";
                return false;
            }
            sample.force = phase.loads.front().force;
            sample.hydrostaticForce = MeasurePlanarHydrostaticMotion(
                motion, phaseAxis, phase.referenceHeight, gravity[phaseAxis], 1000., 1.).force;
            sample.fluid = std::move(phase.fluid);
            for (int axis = 0; axis < 3; ++axis) {
                sample.residual[axis] = displacement[axis] - gravityDisplacement[axis] -
                    (dt * dt / body.mass) * sample.force[axis];
                if (!std::isfinite(sample.residual[axis])) {
                    error = "coupled surface displacement is not representable";
                    return false;
                }
            }
            return true;
        }
        auto& geometry = sample.geometry;
        if (!BuildMovingGridGeometry(layout, {motion}, geometry, error))
            return false;
        const bool projected = transport
            ? detail::AdvanceVolumeWithDualMass(layout, geometry, fullWater, dt, sample.fluid, error)
            : AdvanceMovingVolume(layout, geometry, fullWater, dt, {}, sample.fluid, error);
        if (!projected)
            return false;
        sample.fluid.waterRoundoffCorrection += inputRoundoff;
        const auto loads = AccumulateGridPressureReaction(geometry.averagePatches,
                                                          sample.fluid.pressure);
        if (loads.size() != 1 || loads.front().owner != body.owner) {
            error = "coupled box pressure reaction lost its solid owner";
            return false;
        }
        for (int axis = 0; axis < 3; ++axis) {
            const double dynamicForce = loads.front().force[axis];
            sample.hydrostaticForce[axis] = -displacedMass * gravity[axis];
            sample.force[axis] = dynamicForce + sample.hydrostaticForce[axis];
            sample.residual[axis] = displacement[axis] - gravityDisplacement[axis] -
                                    (dt * dt / body.mass) * dynamicForce;
            if (!std::isfinite(sample.force[axis]) || !std::isfinite(sample.residual[axis])) {
                error = "coupled box pressure reaction is not representable";
                return false;
            }
        }
        // Both phases have fixed density and the tank started fully wet.
        // The phase solve must preserve that invariant to roundoff before
        // canonicalizing exact full cells for the next geometry build.
        for (std::size_t i = 0; i < geometry.endCapacity.size(); ++i) {
            const double expected = geometry.endCapacity[i];
            const double tolerance = sample.fluid.residual +
                256 * std::numeric_limits<double>::epsilon() * layout.h * layout.h * layout.h;
            const double correction = std::abs(sample.fluid.state.water[i] - expected);
            if (correction > tolerance) {
                error = "coupled box lost the uniformly filled phase";
                return false;
            }
            sample.fluid.waterRoundoffCorrection += correction;
            sample.fluid.state.water[i] = expected;
        }
        return true;
    };

    GridVec displacement = gravityDisplacement;
    Sample current;
    if (!evaluate(displacement, current)) {
        displacement = {};
        if (!evaluate(displacement, current))
            return false;
    }

    constexpr int workLimit = 12;
    const double tolerance = 1e-10 * layout.h;
    for (int iteration = 0; MaxAbs(current.residual) > tolerance; ++iteration) {
        if (iteration == workLimit) {
            error = "coupled box could not resolve the pressure/displacement residual";
            return false;
        }
        std::array<GridVec, 3> tangent;
        for (int col = 0; col < 3; ++col) {
            const double increment = std::cbrt(std::numeric_limits<double>::epsilon()) *
                (layout.h + std::abs(displacement[col]));
            auto changed = displacement;
            changed[col] += increment;
            Sample sample;
            double difference = increment;
            if (!evaluate(changed, sample)) {
                changed[col] = displacement[col] - increment;
                difference = -increment;
                if (!evaluate(changed, sample))
                    return false;
            }
            for (int row = 0; row < 3; ++row)
                tangent[row][col] = (sample.residual[row] - current.residual[row]) / difference;
        }
        GridVec rhs, direction{};
        for (int axis = 0; axis < 3; ++axis)
            rhs[axis] = -current.residual[axis];
        if (!SolveTangent(tangent, rhs, direction)) {
            error = "coupled box lost a finite pressure/displacement search direction";
            return false;
        }
        bool improved = false;
        for (int search = 0; search < 12; ++search) {
            const double fraction = std::ldexp(1., -search);
            GridVec proposed;
            for (int axis = 0; axis < 3; ++axis)
                proposed[axis] = displacement[axis] + fraction * direction[axis];
            Sample trial;
            if (evaluate(proposed, trial) && MaxAbs(trial.residual) < MaxAbs(current.residual)) {
                displacement = proposed;
                current = std::move(trial);
                improved = true;
                break;
            }
        }
        if (!improved) {
            error = "coupled box found no admissible pressure/displacement update";
            return false;
        }
    }

    CoupledBoxStep step;
    step.body = body;
    step.fluid = std::move(current.fluid);
    step.pressureForce = current.force;
    step.rigidResidual = MaxAbs(current.residual);
    step.geometryEvaluations = evaluations;
    GridVec velocityChange;
    for (int axis = 0; axis < 3; ++axis) {
        step.body.lo[axis] += displacement[axis];
        step.body.hi[axis] += displacement[axis];
        step.body.velocity[axis] = displacement[axis] / dt;
        velocityChange[axis] = step.body.velocity[axis] - freeVelocity[axis];
    }
    // Hydrostatic work on the box replaces, and is opposite to, the exact
    // change in the displaced water's potential. Remaining projection work
    // is shared with the fluid through the swept-volume adjoint.
    const double rigidWork = .5 * body.mass *
        (Dot(step.body.velocity, step.body.velocity) - Dot(freeVelocity, freeVelocity));
    const double hydrostaticPotential = -Dot(current.hydrostaticForce, displacement);
    step.pressureEnergyResidual = rigidWork + step.fluid.pressureKineticWork +
        hydrostaticPotential + step.fluid.pressureCorrectionEnergy +
        .5 * body.mass * Dot(velocityChange, velocityChange);
    if (!std::isfinite(step.pressureEnergyResidual)) {
        error = "coupled box energy balance is not representable";
        return false;
    }
    if (transport) {
        const auto& geometry = current.geometry;
        if (!AdvectMovingVolumeMomentum(layout, geometry, fullWater, step.fluid, step.transport, error))
            return false;
        step.fluid.state.velocity = step.transport.velocity;
        // Transport includes the algebraic half cells at the sealed boundary.
        // Record the external constraint impulse before removing their normal
        // momentum from the next pressure predictor.
        for (int axis = 0; axis < 3; ++axis) {
            auto limits = layout.cells;
            ++limits[axis];
            for (int k = 0; k < limits[2]; ++k) {
                for (int j = 0; j < limits[1]; ++j) {
                    for (int i = 0; i < limits[0]; ++i) {
                        std::array<int, 3> cell{i, j, k};
                        if (cell[axis] != 0 && cell[axis] != layout.cells[axis])
                            continue;
                        const auto f = layout.Face(axis, i, j, k);
                        if (cell[axis] > 0)
                            --cell[axis];
                        const auto c = layout.Cell(cell[0], cell[1], cell[2]);
                        const double mass = 500 * geometry.endCapacity[c];
                        const double speed = step.fluid.state.velocity[axis][f];
                        step.stationaryWallImpulse[axis] -= mass * speed;
                        step.stationaryWallKineticLoss += .5 * mass * speed * speed;
                        step.fluid.state.velocity[axis][f] = 0;
                    }
                }
            }
        }
        if (!std::isfinite(step.stationaryWallKineticLoss) ||
            std::any_of(step.stationaryWallImpulse.begin(), step.stationaryWallImpulse.end(),
                        [](double impulse) { return !std::isfinite(impulse); })) {
            error = "coupled box stationary-wall reaction is not representable";
            return false;
        }
    }
    output = std::move(step);
    error.clear();
    return true;
}

} // namespace

bool AdvanceCoupledVolumeBox(const GridLayout& layout, const CoupledBoxState& body,
                             const MovingVolumeState& fluid, double dt, const GridVec& gravity,
                             CoupledBoxStep& output, std::string& error) {
    return AdvanceBox(layout, body, fluid, dt, gravity, false, -1, output, error);
}

bool AdvanceTransportedVolumeBox(const GridLayout& layout, const CoupledBoxState& body,
                                 const MovingVolumeState& fluid, double dt,
                                 const GridVec& gravity, CoupledBoxStep& output,
                                 std::string& error) {
    return AdvanceBox(layout, body, fluid, dt, gravity, true, -1, output, error);
}

bool AdvanceEvolvingCoupledVolumeBox(const GridLayout& layout, const CoupledBoxState& body,
                                     const MovingVolumeState& fluid, double dt, int axis,
                                     double signedGravity, CoupledBoxStep& output,
                                     std::string& error) {
    if (axis < 0 || axis > 2 || !std::isfinite(signedGravity) || signedGravity > 0) {
        error = "invalid coupled surface gravity";
        return false;
    }
    GridVec gravity{};
    gravity[axis] = signedGravity;
    return AdvanceBox(layout, body, fluid, dt, gravity, false, axis, output, error);
}

} // namespace judas::volume_grid
