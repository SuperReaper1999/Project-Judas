#pragma once

#include "GridCellPhase.h"

namespace judas::volume_grid {

struct GridWaterSample {
    double wetVolume = 0.; // m³
    GridVec centroid{}; // Metres in the grid chart. Zero when dry.
};

namespace grid_water_query_detail {

struct Rectangle {
    GridVec lo, hi;
};

inline bool Intersect(Rectangle& clipped, const Rectangle& cut) {
    for (int axis = 0; axis < 3; ++axis) {
        clipped.lo[axis] = std::max(clipped.lo[axis], cut.lo[axis]);
        clipped.hi[axis] = std::min(clipped.hi[axis], cut.hi[axis]);
        if (clipped.lo[axis] >= clipped.hi[axis])
            return false;
    }
    return true;
}

// Emit disjoint positive-volume slabs around the excluded intersection. This
// avoids recovering a tiny fluid sliver by subtracting two large wet volumes.
inline void Subtract(Rectangle region, const Rectangle& cut, std::vector<Rectangle>& output) {
    Rectangle excluded = region;
    if (!Intersect(excluded, cut)) {
        output.push_back(region);
        return;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (region.lo[axis] < excluded.lo[axis]) {
            Rectangle slab = region;
            slab.hi[axis] = excluded.lo[axis];
            output.push_back(slab);
            region.lo[axis] = excluded.lo[axis];
        }
        if (region.hi[axis] > excluded.hi[axis]) {
            Rectangle slab = region;
            slab.lo[axis] = excluded.hi[axis];
            output.push_back(slab);
            region.hi[axis] = excluded.hi[axis];
        }
    }
}

} // namespace grid_water_query_detail

// Intersect an AABB sample with the local wet half-space in each intersected
// cell, excluding actual solid geometry. Exterior space is dry. This samples
// the existing phase reconstruction, so disconnected pockets in one cell
// cannot carry independent levels. Invalid samples or visited phase planes
// leave the output intact.
inline bool QueryGridWaterVolume(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                                 int axis, const std::vector<double>& waterPlane, const GridVec& lo,
                                 const GridVec& hi, GridWaterSample& output, std::string& error) {
    using namespace grid_water_query_detail;
    using grid_phase_detail::Index;

    std::vector<BoxMotion> validated;
    if (!grid_phase_detail::Prepare(layout, stationary, axis, validated, error))
        return false;
    if (waterPlane.size() != layout.CellCount()) {
        error = "water phase does not match the geometric query grid";
        return false;
    }
    for (int direction = 0; direction < 3; ++direction) {
        if (!std::isfinite(lo[direction]) || !std::isfinite(hi[direction]) ||
            lo[direction] >= hi[direction]) {
            error = "water query requires finite positive sample extents";
            return false;
        }
    }
    const Rectangle domain{{}, {layout.cells[0] * layout.h, layout.cells[1] * layout.h,
                                layout.cells[2] * layout.h}};
    Rectangle sample{lo, hi};
    if (!Intersect(sample, domain)) {
        output = {};
        error.clear();
        return true;
    }

    Index first{}, stop{};
    for (int direction = 0; direction < 3; ++direction) {
        // Include the next cell at either end so rounded coordinate division
        // cannot skip a thin fragment. Exact physical clipping decides it.
        first[direction] = std::max(0, int(std::floor(sample.lo[direction] / layout.h)) - 1);
        stop[direction] = std::min(layout.cells[direction],
                                   int(std::ceil(sample.hi[direction] / layout.h)) + 1);
    }

    double volume = 0.;
    GridVec moment{};
    std::vector<Rectangle> fragments, remaining;
    for (int k = first[2]; k < stop[2]; ++k) {
        for (int j = first[1]; j < stop[1]; ++j) {
            for (int i = first[0]; i < stop[0]; ++i) {
                const Rectangle cell{{i * layout.h, j * layout.h, k * layout.h},
                                     {(i + 1) * layout.h, (j + 1) * layout.h, (k + 1) * layout.h}};
                Rectangle wet = sample;
                if (!Intersect(wet, cell))
                    continue;
                const double plane = waterPlane[layout.Cell(i, j, k)];
                if (!(plane >= cell.lo[axis] && plane <= cell.hi[axis])) {
                    error = "water query encountered a phase plane outside its cell";
                    return false;
                }
                wet.hi[axis] = std::min(wet.hi[axis], plane);
                if (wet.lo[axis] >= wet.hi[axis])
                    continue;

                fragments.clear();
                fragments.push_back(wet);
                for (const auto& solid : stationary) {
                    Rectangle cut = wet;
                    if (!Intersect(cut, {solid.lo, solid.hi}))
                        continue;
                    remaining.clear();
                    for (const auto& fragment : fragments)
                        Subtract(fragment, cut, remaining);
                    fragments.swap(remaining);
                    if (fragments.empty())
                        break;
                }
                for (const auto& fragment : fragments) {
                    GridVec center{};
                    double amount = 1.;
                    for (int direction = 0; direction < 3; ++direction) {
                        const double length = fragment.hi[direction] - fragment.lo[direction];
                        amount *= length / layout.h;
                        center[direction] = (fragment.lo[direction] + .5 * length) / layout.h;
                    }
                    volume += amount;
                    for (int direction = 0; direction < 3; ++direction)
                        moment[direction] += amount * center[direction];
                }
            }
        }
    }

    // Accumulate in grid units so the first moment cannot overflow when the
    // grid's physical volume itself is still representable.
    GridWaterSample next;
    next.wetVolume = volume * ((layout.h * layout.h) * layout.h);
    if (volume > 0.) {
        if (!(next.wetVolume > 0.) || !std::isfinite(next.wetVolume)) {
            error = "water query volume is not representable";
            return false;
        }
        for (int direction = 0; direction < 3; ++direction)
            next.centroid[direction] = (moment[direction] / volume) * layout.h;
    }
    output = next;
    error.clear();
    return true;
}

} // namespace judas::volume_grid
