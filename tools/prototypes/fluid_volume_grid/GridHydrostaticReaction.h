#pragma once

#include "GridPhaseGeometry.h"
#include "GridPressureReaction.h"

namespace judas::volume_grid {
namespace hydrostatic_reaction_detail {

inline void Validate(int axis, double height, double gravity, double waterDensity,
                     double airDensity, const GridVec& origin) {
    if (axis < 0 || axis > 2 || !std::isfinite(height) || !std::isfinite(gravity) ||
        !std::isfinite(waterDensity) || !std::isfinite(airDensity) ||
        waterDensity < 0 || airDensity < 0)
        throw std::invalid_argument("invalid planar hydrostatic reference");
    pressure_reaction_detail::ValidateOrigin(origin);
}

struct PositiveMoments {
    double mean;
    double first;
};

// Integrate max(0, lerp(begin, end, t)) and t times that value for 0 <= t <= 1.
inline PositiveMoments PositivePart(double begin, double end) {
    if (begin >= 0 && end >= 0)
        return {(begin + end) / 2, (begin + 2 * end) / 6};
    if (begin <= 0 && end <= 0)
        return {0, 0};
    if (begin > 0) {
        const double duration = begin / (begin - end);
        return {begin * duration / 2, begin * duration * duration / 6};
    }
    const double duration = end / (end - begin);
    const double mean = end * duration / 2;
    return {mean, mean - end * duration * duration / 6};
}

} // namespace hydrostatic_reaction_detail

// Integrate the known affine pressure over the actual planar wet/dry wall
// pieces. The phase interface is at phase.height with water below it.
// The gauge is zero at that interface. Set airDensity to zero to observe
// the water load alone. This is a hydrostatic reference, not a dynamic load.
inline std::vector<GridPressureLoad> AccumulatePlanarHydrostaticReaction(
    const GridPhaseGeometry& phase, double signedGravity, double waterDensity,
    double airDensity, const GridVec& torqueOrigin = {}) {
    const int axis = phase.axis;
    hydrostatic_reaction_detail::Validate(axis, phase.height, signedGravity, waterDensity,
                                          airDensity, torqueOrigin);
    pressure_reaction_detail::LoadMap loads;
    const auto add = [&](const auto& patches, double density) {
        const double gradient = density * signedGravity;
        for (const auto& rectangle : patches) {
            const auto& patch = rectangle.patch;
            const double length = rectangle.hi[axis] - rectangle.lo[axis];
            if (!std::isfinite(length) || length < 0)
                throw std::invalid_argument("invalid hydrostatic wall rectangle");
            GridVec moment{};
            // Rectangle variance gives the exact torque correction for
            // affine pressure. The centroid alone integrates only force.
            moment[axis] = gradient * (length * length / 12);
            const double pressure = gradient * (patch.centroid[axis] - phase.height);
            pressure_reaction_detail::AddPatch(loads, patch, pressure, torqueOrigin, moment);
        }
    };
    add(phase.wetPatches, waterDensity);
    add(phase.dryPatches, airDensity);
    return pressure_reaction_detail::Finish(loads);
}

// Exact mean hydrostatic load over an axis-aligned box's linear translation
// through a fixed planar reference. Handles crossing the interface without
// using endpoint-only averaging. The torque origin stays fixed over the step.
// The force dotted with translation is -Δ∫solid p_h dV for the same reference.
inline GridPressureLoad MeasurePlanarHydrostaticMotion(
    const BoxMotion& box, int axis, double height, double signedGravity,
    double waterDensity, double airDensity, const GridVec& torqueOrigin = {}) {
    hydrostatic_reaction_detail::Validate(axis, height, signedGravity, waterDensity,
                                          airDensity, torqueOrigin);
    GridVec size{}, lever{};
    for (int i = 0; i < 3; ++i) {
        size[i] = box.hi[i] - box.lo[i];
        lever[i] = box.lo[i] + size[i] / 2 - torqueOrigin[i];
        if (!std::isfinite(size[i]) || size[i] <= 0 || !std::isfinite(lever[i]) ||
            !std::isfinite(box.translation[i]))
            throw std::invalid_argument("invalid hydrostatic box motion");
    }

    const int first = (axis + 1) % 3;
    const int second = (axis + 2) % 3;
    const double tangentArea = size[first] * size[second];
    const double lower = height - box.lo[axis];
    const double upper = height - box.hi[axis];
    const double travel = box.translation[axis];
    hydrostatic_reaction_detail::PositiveMoments wet{};
    if (upper >= std::max(0., travel)) {
        wet = {size[axis], size[axis] / 2};
    } else if (lower > std::min(0., travel)) {
        const auto low = hydrostatic_reaction_detail::PositivePart(lower, lower - travel);
        const auto high = hydrostatic_reaction_detail::PositivePart(upper, upper - travel);
        wet = {low.mean - high.mean, low.first - high.first};
    }
    const double densityJump = waterDensity - airDensity;
    const double scale = -signedGravity * tangentArea;
    const double meanForce = scale * (airDensity * size[axis] + densityJump * wet.mean);
    const double firstForce =
        scale * (airDensity * size[axis] / 2 + densityJump * wet.first);

    GridPressureLoad result;
    result.owner = box.owner;
    result.force[axis] = meanForce;
    result.torque[first] = lever[second] * meanForce + box.translation[second] * firstForce;
    result.torque[second] = -lever[first] * meanForce - box.translation[first] * firstForce;
    if (!std::isfinite(meanForce) || !std::isfinite(result.torque[first]) ||
        !std::isfinite(result.torque[second]))
        throw std::invalid_argument("nonfinite hydrostatic box load");
    return result;
}

} // namespace judas::volume_grid
