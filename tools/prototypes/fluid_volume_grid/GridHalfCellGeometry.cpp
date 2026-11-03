#include "GridHalfCellGeometry.h"

namespace judas::volume_grid {
namespace {

using namespace moving_geometry_detail;
using grid_phase_detail::VisitGrid;

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

bool ReadPlanes(const GridLayout& layout, int phaseAxis, int splitAxis,
                const std::vector<double>& waterPlane, std::vector<double>& normalized,
                std::string& error) {
    if (!grid_phase_detail::ValidateChart(layout, phaseAxis, error))
        return false;
    if (splitAxis < 0 || splitAxis > 2 || waterPlane.size() != layout.CellCount()) {
        error = "invalid half-cell split axis or phase array";
        return false;
    }
    normalized.resize(waterPlane.size());
    bool valid = true;
    VisitGrid(layout, -1, [&](const Index& cell) {
        const auto index = layout.Cell(cell[0], cell[1], cell[2]);
        const double lower = cell[phaseAxis], upper = lower + 1., middle = lower + .5;
        const double plane = waterPlane[index];
        if (!(plane >= lower * layout.h && plane <= upper * layout.h)) {
            valid = false;
            return;
        }
        if (plane == lower * layout.h)
            normalized[index] = lower;
        else if (plane == upper * layout.h)
            normalized[index] = upper;
        else if (plane == middle * layout.h)
            normalized[index] = middle;
        else
            normalized[index] = std::clamp(plane / layout.h, lower, upper);
    });
    if (!valid)
        error = "half-cell phase plane is outside its coarse cell";
    return valid;
}

double FaceArea(const Bounds& region, int normal) {
    const int a = (normal + 1) % 3, b = (normal + 2) % 3;
    return std::max(0., region.hi[a] - region.lo[a]) *
           std::max(0., region.hi[b] - region.lo[b]);
}

Bounds WetFace(Bounds face, int normal, int phaseAxis, double plane, bool left) {
    if (normal != phaseAxis)
        return ClipWet(face, phaseAxis, plane);

    // Sample the limit from inside the owner rather than classifying a
    // horizontal face exactly on a local phase plane as both wet or both dry.
    const double elevation = face.lo[normal];
    if (left ? plane < elevation : plane <= elevation) {
        const int tangent = (normal + 1) % 3;
        face.hi[tangent] = face.lo[tangent];
    }
    return face;
}

struct FacePieces {
    Bounds open, leftWet, rightWet;
};

GridHalfCellAperture Areas(const FacePieces& face, int normal, double area) {
    return {area * FaceArea(face.open, normal), area * FaceArea(face.leftWet, normal),
            area * FaceArea(face.rightWet, normal)};
}

double Blocked(const BoxMotion& box, int normal, const Bounds& region) {
    if (FaceArea(region, normal) == 0.)
        return 0.;
    return AverageBlockedFace(box, normal, region.lo, region.hi);
}

void SubtractBox(GridHalfCellAperture& aperture, const FacePieces& face, const BoxMotion& box,
                 int normal, double area) {
    aperture.open -= area * Blocked(box, normal, face.open);
    aperture.leftWet -= area * Blocked(box, normal, face.leftWet);
    aperture.rightWet -= area * Blocked(box, normal, face.rightWet);
}

bool AdmitAperture(GridHalfCellAperture& aperture, double maxArea) {
    // Validate subtraction residue at the face scale before comparing the
    // wet subset with an arbitrarily small true opening.
    if (!AdmitRoundoff(aperture.open, maxArea) ||
        !AdmitRoundoff(aperture.leftWet, maxArea) || !AdmitRoundoff(aperture.rightWet, maxArea))
        return false;
    const double roundoff = 128. * std::numeric_limits<double>::epsilon() * maxArea;
    if (aperture.leftWet > aperture.open + roundoff || aperture.rightWet > aperture.open + roundoff)
        return false;
    aperture.leftWet = std::min(aperture.leftWet, aperture.open);
    aperture.rightWet = std::min(aperture.rightWet, aperture.open);
    return true;
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

bool BuildGridHalfCellStorage(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                              int phaseAxis, int splitAxis, const std::vector<double>& waterPlane,
                              GridHalfCellStorage& output, std::string& error) {
    std::vector<double> planes;
    std::vector<BoxMotion> boxes;
    if (!ReadPlanes(layout, phaseAxis, splitAxis, waterPlane, planes, error) ||
        !grid_phase_detail::Prepare(layout, stationary, phaseAxis, boxes, error))
        return false;

    const double cellVolume = (layout.h * layout.h) * layout.h;
    const double halfVolume = .5 * cellVolume;
    GridHalfCellStorage next;
    next.capacity.assign(layout.CellCount(), {halfVolume, halfVolume});
    next.water.resize(layout.CellCount());
    VisitGrid(layout, -1, [&](const Index& cell) {
        const auto index = layout.Cell(cell[0], cell[1], cell[2]);
        for (int half = 0; half < 2; ++half) {
            const auto region = SplitHalf(cell, splitAxis, half);
            next.water[index][half] = cellVolume * Volume(ClipWet(region, phaseAxis, planes[index]));
        }
    });
    for (const auto& box : boxes) {
        VisitSweptBounds(layout, box, -1, [&](const Index& cell) {
            const auto index = layout.Cell(cell[0], cell[1], cell[2]);
            for (int half = 0; half < 2; ++half) {
                const auto region = SplitHalf(cell, splitAxis, half);
                next.capacity[index][half] -= cellVolume * OccupiedVolume(box, region.lo, region.hi, 0.);
                const auto wet = ClipWet(region, phaseAxis, planes[index]);
                next.water[index][half] -= cellVolume * OccupiedVolume(box, wet.lo, wet.hi, 0.);
            }
        });
    }
    const double roundoff = 128. * std::numeric_limits<double>::epsilon() * halfVolume;
    for (std::size_t cell = 0; cell < next.capacity.size(); ++cell) {
        for (int half = 0; half < 2; ++half) {
            double& cap = next.capacity[cell][half];
            double& water = next.water[cell][half];
            if (!AdmitRoundoff(cap, halfVolume) || !AdmitRoundoff(water, halfVolume) ||
                water > cap + roundoff) {
                error = "half-cell phase volume is outside the free capacity";
                return false;
            }
            water = std::min(water, cap);
        }
    }
    output = std::move(next);
    error.clear();
    return true;
}

bool AverageGridHalfCellApertures(const GridLayout& layout, const std::vector<BoxMotion>& moving,
                                 int phaseAxis, int splitAxis, const std::vector<double>& waterPlane,
                                 GridHalfCellApertures& output, std::string& error) {
    std::vector<double> planes;
    std::vector<BoxMotion> boxes;
    if (!ReadPlanes(layout, phaseAxis, splitAxis, waterPlane, planes, error) ||
        !moving_geometry_detail::NormalizeBoxes(layout, moving, boxes, error))
        return false;

    const double area = layout.h * layout.h;
    GridHalfCellApertures next;
    for (int normal = 0; normal < 3; ++normal) {
        if (normal == splitAxis)
            continue;
        const auto pieces = [&](const Index& face, int half) {
            const auto open = SplitHalf(face, splitAxis, half);
            auto dry = open;
            dry.hi[(normal + 1) % 3] = dry.lo[(normal + 1) % 3];
            auto leftCell = face;
            --leftCell[normal];
            const auto phase = [&](const Index& cell, bool left) {
                if (cell[normal] < 0 || cell[normal] >= layout.cells[normal])
                    return dry;
                return WetFace(open, normal, phaseAxis, planes[layout.Cell(cell[0], cell[1], cell[2])],
                               left);
            };
            return FacePieces{open, phase(leftCell, true), phase(face, false)};
        };
        auto& transverse = next.transverse[normal];
        transverse.resize(layout.FaceCount(normal));
        VisitGrid(layout, normal, [&](const Index& face) {
            auto& parts = transverse[layout.Face(normal, face[0], face[1], face[2])];
            for (int half = 0; half < 2; ++half)
                parts[half] = Areas(pieces(face, half), normal, area);
        });
        for (const auto& box : boxes) {
            VisitSweptBounds(layout, box, normal, [&](const Index& face) {
                auto& parts = transverse[layout.Face(normal, face[0], face[1], face[2])];
                for (int half = 0; half < 2; ++half)
                    SubtractBox(parts[half], pieces(face, half), box, normal, area);
            });
        }
        for (auto& parts : transverse) {
            if (!AdmitAperture(parts[0], .5 * area) || !AdmitAperture(parts[1], .5 * area)) {
                error = "transverse half-cell wet aperture is outside the geometric opening";
                return false;
            }
        }
    }

    const auto midpoint = [&](const Index& cell, std::size_t index) {
        const auto open = SplitHalf(cell, splitAxis, 1);
        return FacePieces{open, WetFace(open, splitAxis, phaseAxis, planes[index], true),
                          WetFace(open, splitAxis, phaseAxis, planes[index], false)};
    };
    next.midpoint.resize(layout.CellCount());
    VisitGrid(layout, -1, [&](const Index& cell) {
        const auto index = layout.Cell(cell[0], cell[1], cell[2]);
        next.midpoint[index] = Areas(midpoint(cell, index), splitAxis, area);
    });
    for (const auto& box : boxes) {
        VisitSweptBounds(layout, box, -1, [&](const Index& cell) {
            const auto index = layout.Cell(cell[0], cell[1], cell[2]);
            SubtractBox(next.midpoint[index], midpoint(cell, index), box, splitAxis, area);
        });
    }
    for (auto& face : next.midpoint) {
        if (!AdmitAperture(face, area)) {
            error = "half-cell midpoint wet aperture is outside the geometric opening";
            return false;
        }
    }
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
