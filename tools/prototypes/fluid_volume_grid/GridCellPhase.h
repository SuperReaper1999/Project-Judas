#pragma once

#include "GridPhaseGeometry.h"

namespace judas::volume_grid {

struct GridCellPhase {
    int axis = 1;
    std::vector<double> waterPlane; // Metres in the grid chart, one plane per cell.
    std::vector<double> capacity; // Available cell volume in m³.
};

namespace grid_cell_phase_detail {

struct SolidSlice {
    double lower, upper, area; // Grid units. Area is perpendicular to the phase axis.
};

inline bool InvertStorage(const std::vector<SolidSlice>& solids, int cell, double fraction,
                          double& waterPlane) {
    if (solids.empty()) {
        waterPlane = cell + fraction;
        return true;
    }

    std::vector<double> cuts{double(cell), cell + 1.};
    for (const auto& solid : solids) {
        cuts.push_back(solid.lower);
        cuts.push_back(solid.upper);
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());

    double volume = 0.;
    for (std::size_t i = 1; i < cuts.size(); ++i) {
        const double lower = cuts[i - 1], upper = cuts[i];
        double area = 1.;
        for (const auto& solid : solids) {
            if (solid.lower <= lower && solid.upper >= upper)
                area -= solid.area;
        }
        if (!moving_geometry_detail::AdmitRoundoff(area, 1.))
            return false;
        const double next = std::fma(area, upper - lower, volume);
        if (fraction <= next) {
            // Choose the first height attaining this volume if there is a
            // solid-only slab with no usable storage.
            waterPlane = fraction == next ? upper : lower + (fraction - volume) / area;
            return true;
        }
        volume = next;
    }

    // The capacity subtraction and height integral are equivalent, but their
    // rounding can differ at a fully wet upper bound.
    if (fraction - volume > 128. * std::numeric_limits<double>::epsilon())
        return false;
    waterPlane = cell + 1.;
    return true;
}

} // namespace grid_cell_phase_detail

// Recover a local horizontal wet half-space in each cell from independently
// transported water volumes. This is a first-order storage reconstruction,
// not a constraint that neighboring interface heights be equal. Box geometry
// must be an admitted stationary snapshot. Failure leaves output intact.
inline bool ReconstructGridCellPhase(const GridLayout& layout,
                                     const std::vector<BoxMotion>& stationary, int axis,
                                     const std::vector<double>& water, GridCellPhase& output,
                                     std::string& error) {
    using grid_cell_phase_detail::SolidSlice;
    using grid_phase_detail::Index;
    using moving_geometry_detail::AdmitRoundoff;
    using moving_geometry_detail::CellOverlap;
    using moving_geometry_detail::VisitSweptBounds;

    std::vector<BoxMotion> boxes;
    if (!grid_phase_detail::Prepare(layout, stationary, axis, boxes, error))
        return false;
    const auto count = layout.CellCount();
    const double cellVolume = (layout.h * layout.h) * layout.h;
    if (water.size() != count ||
        std::any_of(water.begin(), water.end(), [&](double v) { return !(v >= 0. && v <= cellVolume); })) {
        error = "water volumes do not match the cell reconstruction grid";
        return false;
    }

    GridCellPhase next;
    next.axis = axis;
    next.waterPlane.resize(count);
    next.capacity.assign(count, cellVolume);
    std::vector<std::vector<SolidSlice>> slices(count);
    const int b = (axis + 1) % 3, c = (axis + 2) % 3;
    for (const auto& box : boxes) {
        VisitSweptBounds(layout, box, -1, [&](const Index& cell) {
            GridVec overlap{};
            for (int direction = 0; direction < 3; ++direction)
                overlap[direction] = CellOverlap(box, direction, cell[direction], 0.);
            if (overlap[0] == 0. || overlap[1] == 0. || overlap[2] == 0.)
                return;

            const auto index = layout.Cell(cell[0], cell[1], cell[2]);
            // Match the canonical geometry builder's arithmetic for the
            // endpoint capacity, including the order of the multiplications.
            next.capacity[index] -= cellVolume * overlap[0] * overlap[1] * overlap[2];
            if (water[index] > 0.) {
                slices[index].push_back({std::max(double(cell[axis]), box.lo[axis]),
                                         std::min(cell[axis] + 1., box.hi[axis]),
                                         overlap[b] * overlap[c]});
            }
        });
    }

    for (int k = 0; k < layout.cells[2]; ++k) {
        for (int j = 0; j < layout.cells[1]; ++j) {
            for (int i = 0; i < layout.cells[0]; ++i) {
                const auto index = layout.Cell(i, j, k);
                const int cell = Index{i, j, k}[axis];
                const double volume = water[index];
                double& capacity = next.capacity[index];
                if (!AdmitRoundoff(capacity, cellVolume) || volume > capacity) {
                    error = "water exceeds the available cut cell capacity";
                    return false;
                }
                double plane = cell;
                if (volume > 0.) {
                    if (volume == capacity)
                        plane = cell + 1.;
                    else if (!grid_cell_phase_detail::InvertStorage(slices[index], cell,
                                                                    volume / cellVolume, plane)) {
                        error = "cut cell storage inverse is not representable";
                        return false;
                    }
                }
                next.waterPlane[index] = std::clamp(plane, double(cell), cell + 1.) * layout.h;
            }
        }
    }
    output = std::move(next);
    error.clear();
    return true;
}

// Average the wet opening of a shared face as seen from either adjacent cell.
// The per-cell planes stay fixed during this geometry integral. Boxes may
// cross either plane, and the two sides need not have equal liquid coverage.
// A missing exterior side has zero wet area. On a phase-normal face, a
// full lower cell is wet at its upper interior trace, while an empty upper
// cell is dry at its lower interior trace. Failure preserves both outputs.
inline bool AverageCellPhaseApertures(const GridLayout& layout, const std::vector<BoxMotion>& moving,
                                      int axis, const std::vector<double>& waterPlane,
                                      std::array<std::vector<double>, 3>& leftWet,
                                      std::array<std::vector<double>, 3>& rightWet,
                                      std::string& error) {
    using grid_phase_detail::Index;
    using moving_geometry_detail::AdmitRoundoff;
    using moving_geometry_detail::AverageBlockedFace;
    using moving_geometry_detail::FaceUpper;
    using moving_geometry_detail::VisitSweptBounds;

    if (!grid_phase_detail::ValidateChart(layout, axis, error))
        return false;
    const auto count = layout.CellCount();
    if (waterPlane.size() != count || &leftWet == &rightWet) {
        error = "invalid per-cell phase apertures input or output shape";
        return false;
    }
    std::vector<BoxMotion> boxes;
    if (!moving_geometry_detail::NormalizeBoxes(layout, moving, boxes, error))
        return false;

    std::vector<double> planes(count);
    bool validPlanes = true;
    grid_phase_detail::VisitGrid(layout, -1, [&](const Index& cell) {
        const auto index = layout.Cell(cell[0], cell[1], cell[2]);
        const double lower = cell[axis] * layout.h, upper = (cell[axis] + 1) * layout.h;
        const double plane = waterPlane[index];
        if (!(plane >= lower && plane <= upper)) {
            validPlanes = false;
            return;
        }
        if (plane == lower)
            planes[index] = cell[axis];
        else if (plane == upper)
            planes[index] = cell[axis] + 1.;
        else
            planes[index] = std::clamp(plane / layout.h, double(cell[axis]), cell[axis] + 1.);
    });
    if (!validPlanes) {
        error = "per-cell phase plane is outside its cell";
        return false;
    }

    const auto wetSpan = [&](int normal, const Index& face, bool left) {
        Index cell = face;
        cell[normal] -= left;
        if (cell[normal] < 0 || cell[normal] >= layout.cells[normal])
            return 0.;
        const auto index = layout.Cell(cell[0], cell[1], cell[2]);
        if (normal == axis) {
            const double elevation = face[axis] * layout.h;
            return double(left ? waterPlane[index] >= elevation : waterPlane[index] > elevation);
        }
        return planes[index] - face[axis];
    };
    const auto blocked = [&](const BoxMotion& box, int normal, const Index& face, double span) {
        if (span == 0.)
            return 0.;
        auto upper = FaceUpper(face);
        if (normal != axis)
            upper[axis] = face[axis] + span;
        return AverageBlockedFace(box, normal, face, upper);
    };

    const double area = layout.h * layout.h;
    std::array<std::vector<double>, 3> nextLeft, nextRight;
    for (int normal = 0; normal < 3; ++normal) {
        nextLeft[normal].resize(layout.FaceCount(normal));
        nextRight[normal].resize(layout.FaceCount(normal));
        grid_phase_detail::VisitGrid(layout, normal, [&](const Index& face) {
            const auto index = layout.Face(normal, face[0], face[1], face[2]);
            nextLeft[normal][index] = area * wetSpan(normal, face, true);
            nextRight[normal][index] = area * wetSpan(normal, face, false);
        });
        for (const auto& box : boxes) {
            VisitSweptBounds(layout, box, normal, [&](const Index& face) {
                const auto index = layout.Face(normal, face[0], face[1], face[2]);
                const double leftSpan = wetSpan(normal, face, true);
                const double rightSpan = wetSpan(normal, face, false);
                const double leftBlocked = blocked(box, normal, face, leftSpan);
                const double rightBlocked = leftSpan == rightSpan
                    ? leftBlocked : blocked(box, normal, face, rightSpan);
                nextLeft[normal][index] -= area * leftBlocked;
                nextRight[normal][index] -= area * rightBlocked;
            });
        }
        for (std::size_t index = 0; index < layout.FaceCount(normal); ++index) {
            if (!AdmitRoundoff(nextLeft[normal][index], area) ||
                !AdmitRoundoff(nextRight[normal][index], area)) {
                error = "per-cell wet aperture is outside the face area";
                return false;
            }
        }
    }
    leftWet = std::move(nextLeft);
    rightWet = std::move(nextRight);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
