#include "OrientedMovingVolume.h"

#include "MovingVolumePressureInternal.h"

#include <numeric>

namespace judas::volume_grid {

bool AdvanceOrientedMovingVolume(const GridLayout& layout, const GridOrientedMotion& motion,
                                 const MovingVolumeState& state, double dt, const GridVec& gravity,
                                 OrientedMovingVolumeStep& output, std::string& error) {
    if (std::any_of(gravity.begin(), gravity.end(), [](double g) { return !std::isfinite(g); })) {
        error = "invalid oriented moving-volume gravity";
        return false;
    }
    GridOrientedSweep sweep;
    if (!BuildGridOrientedSweep(layout, motion, sweep, error))
        return false;
    MovingGridGeometry geometry;
    geometry.startCapacity = std::move(sweep.start.capacity);
    geometry.endCapacity = std::move(sweep.end.capacity);
    geometry.startAperture = std::move(sweep.start.aperture);
    geometry.endAperture = std::move(sweep.end.aperture);
    geometry.averageAperture = std::move(sweep.averageAperture);
    geometry.solidSweep = std::move(sweep.solidSweep);
    if (state.water.size() != geometry.startCapacity.size()) {
        error = "oriented phase state does not match the chart";
        return false;
    }
    const double cellVolume = layout.h * layout.h * layout.h;
    const double phaseTolerance = 1e-12 * cellVolume;
    bool waterOnly = true, airOnly = true;
    for (std::size_t cell = 0; cell < state.water.size(); ++cell) {
        const double water = state.water[cell];
        if (!std::isfinite(water) || water < 0) {
            error = "invalid oriented moving-volume water amount";
            return false;
        }
        waterOnly = waterOnly &&
            std::abs(water - geometry.startCapacity[cell]) <= phaseTolerance;
        airOnly = airOnly && water == 0;
    }
    if (!waterOnly && !airOnly) {
        error = "oriented moving-volume pressure currently requires one uniform phase";
        return false;
    }
    MovingVolumeState canonical = state;
    double inputRoundoff = 0;
    if (waterOnly) {
        for (std::size_t cell = 0; cell < canonical.water.size(); ++cell)
            inputRoundoff += std::abs(canonical.water[cell] - geometry.startCapacity[cell]);
        canonical.water = geometry.startCapacity;
    }

    OrientedMovingVolumeStep next;
    if (!detail::AdvanceVolumeForVerifiedSweep(layout, geometry, canonical, dt, sweep.maxFaceResidual,
                                               next.fluid, error))
        return false;
    next.fluid.waterRoundoffCorrection += inputRoundoff;
    next.load.owner = motion.start.owner;
    for (const auto& patch : sweep.patches) {
        const double pressure = next.fluid.pressure[patch.cell];
        for (int axis = 0; axis < 3; ++axis) {
            next.load.force[axis] -= pressure * patch.normalArea[axis];
            next.load.torque[axis] -= pressure * patch.torqueArea[axis];
        }
    }
    const double linearWork = std::inner_product(next.load.force.begin(), next.load.force.end(),
                                                 motion.translation.begin(), 0.);
    const double angularWork = motion.angle *
        std::inner_product(next.load.torque.begin(), next.load.torque.end(),
                           motion.rotationAxis.begin(), 0.);
    next.pressureWorkResidual = linearWork + angularWork + next.fluid.pressureFluidWork;
    next.maximumSweepResidual = std::max(sweep.maxFaceResidual, sweep.maxPatchResidual);
    next.geometryEvaluations = sweep.evaluations;
    const auto& half = motion.start.half;
    const double displacedMass = (waterOnly ? 1000. : 1.) * 8 * half[0] * half[1] * half[2];
    for (int axis = 0; axis < 3; ++axis) {
        next.load.force[axis] -= displacedMass * gravity[axis];
        if (!std::isfinite(next.load.force[axis]) || !std::isfinite(next.load.torque[axis]) ||
            !std::isfinite(next.pressureWorkResidual)) {
            error = "oriented pressure load or work is not representable";
            return false;
        }
    }
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
