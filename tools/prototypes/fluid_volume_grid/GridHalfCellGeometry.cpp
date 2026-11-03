#include "GridHalfCellGeometry.h"

namespace judas::volume_grid {
namespace {

using namespace moving_geometry_detail;

GridVec Lower(const Index& cell) {
    return {double(cell[0]), double(cell[1]), double(cell[2])};
}

struct Bounds {
    GridVec lo, hi;
};

Bounds SplitHalf(const Index& cell, int axis, int side) {
    Bounds bounds{Lower(cell), FaceUpper(cell)};
    if (side == 0)
        bounds.hi[axis] -= .5;
    else
        bounds.lo[axis] += .5;
    return bounds;
}

double Volume(const Bounds& bounds) {
    double result = 1.;
    for (int axis = 0; axis < 3; ++axis)
        result *= std::max(0., bounds.hi[axis] - bounds.lo[axis]);
    return result;
}

Bounds ClipWet(Bounds bounds, int phaseAxis, double height) {
    bounds.hi[phaseAxis] = std::clamp(height, bounds.lo[phaseAxis], bounds.hi[phaseAxis]);
    return bounds;
}

double OccupiedVolume(const BoxMotion& box, const GridVec& lo, const GridVec& hi, double time) {
    double result = 1.;
    for (int axis = 0; axis < 3; ++axis) {
        result *= Overlap(lo[axis], hi[axis], At(box.lo[axis], box.translation[axis], time),
                          At(box.hi[axis], box.translation[axis], time));
    }
    return result;
}

} // namespace

bool BuildGridHalfCellGeometry(const GridLayout& layout, const std::vector<BoxMotion>& motions,
                               int axis, GridHalfCellGeometry& output, std::string& error) {
    const auto count = layout.CheckedCellCount();
    if (count == 0 || axis < 0 || axis > 2) {
        error = "invalid half-cell layout or split axis";
        return false;
    }
    std::vector<BoxMotion> boxes;
    if (!NormalizeBoxes(layout, motions, boxes, error))
        return false;

    constexpr std::array<double, 2> full{.5, .5};
    GridHalfCellGeometry next;
    next.axis = axis;
    next.startCapacity.assign(count, full);
    next.endCapacity.assign(count, full);
    next.midpointAperture.assign(count, 1.);
    for (int direction = 0; direction < 3; ++direction) {
        if (direction != axis)
            next.transverseAperture[direction].assign(layout.FaceCount(direction), full);
    }

    for (const auto& box : boxes) {
        VisitSweptBounds(layout, box, -1, [&](const Index& cell) {
            const auto index = layout.Cell(cell[0], cell[1], cell[2]);
            for (int side = 0; side < 2; ++side) {
                const auto [lo, hi] = SplitHalf(cell, axis, side);
                next.startCapacity[index][side] -= OccupiedVolume(box, lo, hi, 0.);
                next.endCapacity[index][side] -= OccupiedVolume(box, lo, hi, 1.);
            }
            const auto [middle, upper] = SplitHalf(cell, axis, 1);
            next.midpointAperture[index] -= AverageBlockedFace(box, axis, middle, upper);
        });
        for (int direction = 0; direction < 3; ++direction) {
            if (direction == axis)
                continue;
            VisitSweptBounds(layout, box, direction, [&](const Index& face) {
                const auto index = layout.Face(direction, face[0], face[1], face[2]);
                auto& halves = next.transverseAperture[direction][index];
                for (int side = 0; side < 2; ++side) {
                    const auto [lo, hi] = SplitHalf(face, axis, side);
                    halves[side] -= AverageBlockedFace(box, direction, lo, hi);
                }
            });
        }
    }

    const double area = layout.h * layout.h, volume = area * layout.h;
    const auto scalePairs = [](auto& values, double scale) {
        for (auto& pair : values) {
            for (double& value : pair) {
                if (!AdmitRoundoff(value, .5))
                    return false;
                value *= scale;
            }
        }
        return true;
    };
    bool valid = scalePairs(next.startCapacity, volume) && scalePairs(next.endCapacity, volume);
    for (auto& apertures : next.transverseAperture)
        valid = valid && scalePairs(apertures, area);
    for (double& aperture : next.midpointAperture) {
        valid = valid && AdmitRoundoff(aperture, 1.);
        aperture *= area;
    }
    if (!valid) {
        error = "half-cell cuts exceed their geometric support";
        return false;
    }
    output = std::move(next);
    error.clear();
    return true;
}

bool BuildGridHalfCellWater(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                            const GridCellPhase& phase, int axis, GridHalfCellWater& output,
                            std::string& error) {
    const auto count = layout.CheckedCellCount();
    if (count == 0 || axis < 0 || axis > 2 || phase.axis < 0 || phase.axis > 2 ||
        phase.waterPlane.size() != count ||
        std::any_of(phase.waterPlane.begin(), phase.waterPlane.end(),
                    [](double height) { return !std::isfinite(height); })) {
        error = "invalid half-cell phase planes or split axis";
        return false;
    }
    std::vector<BoxMotion> boxes;
    if (!grid_phase_detail::Prepare(layout, stationary, phase.axis, boxes, error))
        return false;

    GridHalfCellWater next;
    next.volume.resize(count);
    const auto fill = [&](const Index& cell, std::size_t index, int side) {
        return ClipWet(SplitHalf(cell, axis, side), phase.axis, phase.waterPlane[index] / layout.h);
    };
    for (int z = 0; z < layout.cells[2]; ++z) {
        for (int y = 0; y < layout.cells[1]; ++y) {
            for (int x = 0; x < layout.cells[0]; ++x) {
                const auto index = layout.Cell(x, y, z);
                for (int side = 0; side < 2; ++side)
                    next.volume[index][side] = Volume(fill({x, y, z}, index, side));
            }
        }
    }
    for (const auto& box : boxes) {
        VisitSweptBounds(layout, box, -1, [&](const Index& cell) {
            const auto index = layout.Cell(cell[0], cell[1], cell[2]);
            for (int side = 0; side < 2; ++side) {
                const auto [lo, hi] = fill(cell, index, side);
                next.volume[index][side] -= OccupiedVolume(box, lo, hi, 0.);
            }
        });
    }
    const double cellVolume = layout.h * layout.h * layout.h;
    for (auto& pair : next.volume) {
        for (double& water : pair) {
            if (!AdmitRoundoff(water, .5)) {
                error = "half-cell phase exceeds its geometric capacity";
                return false;
            }
            water *= cellVolume;
        }
    }
    output = std::move(next);
    error.clear();
    return true;
}

bool BalanceGridHalfCell(const GridHalfCellGeometry& geometry, std::size_t cell,
                         const HalfCellInflow& inflow, double& midpointFlux, std::string& error) {
    if (cell >= geometry.startCapacity.size() || cell >= geometry.endCapacity.size() ||
        cell >= geometry.midpointAperture.size() || !(geometry.midpointAperture[cell] >= 0) ||
        !std::isfinite(geometry.midpointAperture[cell])) {
        error = "invalid half-cell balance geometry";
        return false;
    }
    const auto& start = geometry.startCapacity[cell];
    const auto& end = geometry.endCapacity[cell];
    const double lower = inflow.axialLow + inflow.netTransverse[0] + (start[0] - end[0]);
    const double upper = inflow.axialHigh - inflow.netTransverse[1] - (start[1] - end[1]);
    const double scale = start[0] + start[1] + end[0] + end[1] + std::abs(inflow.axialLow) +
        std::abs(inflow.axialHigh) + std::abs(inflow.netTransverse[0]) + std::abs(inflow.netTransverse[1]);
    const double roundoff = 128 * std::numeric_limits<double>::epsilon() * scale;
    if (!std::isfinite(lower) || !std::isfinite(upper) || !std::isfinite(roundoff) ||
        std::abs(lower - upper) > roundoff) {
        error = "half-cell transfers do not close the occupied volume balance";
        return false;
    }
    const bool blocked = geometry.midpointAperture[cell] == 0;
    if (blocked && (std::abs(lower) > roundoff || std::abs(upper) > roundoff)) {
        error = "half-cell transfers require flow through a solid midpoint";
        return false;
    }
    midpointFlux = blocked ? 0. : lower;
    error.clear();
    return true;
}

} // namespace judas::volume_grid
