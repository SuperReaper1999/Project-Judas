#pragma once

#include "MovingGridGeometry.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>

namespace judas::volume_grid {

struct GridPressureLoad {
    std::uint32_t owner = 0;
    GridVec force{};
    GridVec torque{};
};

namespace pressure_reaction_detail {

using LoadMap = std::map<std::uint32_t, GridPressureLoad>;

inline void AddPatch(LoadMap& loads, const SolidPatch& patch, double pressure,
                     const GridVec& torqueOrigin, const GridVec& centralPressureMoment = {}) {
    if (patch.axis < 0 || patch.axis > 2 || (patch.sign != -1 && patch.sign != 1) ||
        !std::isfinite(patch.area) || patch.area < 0 || !std::isfinite(pressure))
        throw std::invalid_argument("invalid grid pressure patch");

    GridVec moment{};
    for (int axis = 0; axis < 3; ++axis) {
        const double lever = patch.centroid[axis] - torqueOrigin[axis];
        if (!std::isfinite(lever) || !std::isfinite(centralPressureMoment[axis]))
            throw std::invalid_argument("invalid grid pressure patch centroid or moment");
        moment[axis] = lever * pressure + centralPressureMoment[axis];
    }

    auto& load = loads[patch.owner];
    load.owner = patch.owner;
    const double inwardArea = -patch.sign * patch.area;
    load.force[patch.axis] += inwardArea * pressure;
    // The traction has one axis component. The other two are the
    // corresponding cross product with the pressure-weighted lever arm.
    const int first = (patch.axis + 1) % 3;
    const int second = (patch.axis + 2) % 3;
    load.torque[first] += inwardArea * moment[second];
    load.torque[second] -= inwardArea * moment[first];
}

inline std::vector<GridPressureLoad> Finish(const LoadMap& loads) {
    std::vector<GridPressureLoad> result;
    result.reserve(loads.size());
    for (const auto& entry : loads) {
        const auto& load = entry.second;
        for (int axis = 0; axis < 3; ++axis)
            if (!std::isfinite(load.force[axis]) || !std::isfinite(load.torque[axis]))
                throw std::invalid_argument("nonfinite grid pressure load");
        result.push_back(load);
    }
    return result;
}

inline void ValidateOrigin(const GridVec& torqueOrigin) {
    for (double value : torqueOrigin)
        if (!std::isfinite(value))
            throw std::invalid_argument("invalid grid pressure torque origin");
}

} // namespace pressure_reaction_detail

// p is the pressure used by the volume projection, in pascals. Positive
// pressure pushes into the solid. With time-averaged patches this load is
// adjoint to the swept-volume constraint: F · Δx = -Σ p_c ΔV_s,c.
// The water/air solve supplies a single mixture pressure. A caller must not
// apply a separate buoyancy force in addition to this reaction.
inline std::vector<GridPressureLoad> AccumulateGridPressureReaction(
    const std::vector<SolidPatch>& patches,
    const std::vector<double>& cellPressure,
    const GridVec& torqueOrigin = {}) {
    pressure_reaction_detail::ValidateOrigin(torqueOrigin);
    pressure_reaction_detail::LoadMap loads;
    for (const SolidPatch& patch : patches) {
        if (patch.cell >= cellPressure.size())
            throw std::invalid_argument("grid pressure patch cell is out of range");
        pressure_reaction_detail::AddPatch(loads, patch, cellPressure[patch.cell], torqueOrigin);
    }
    return pressure_reaction_detail::Finish(loads);
}

} // namespace judas::volume_grid
