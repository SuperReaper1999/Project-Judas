#pragma once

#include "MovingGridGeometry.h"

namespace judas::volume_grid {

struct RectangularPatch {
    SolidPatch patch;
    GridVec lo, hi; // Equal coordinates along the outward normal.
};

struct GridPhaseGeometry {
    int axis = 1;
    double height = 0.;
    std::vector<double> water, air; // Cell volumes in m³.
    std::array<std::vector<double>, 3> wetAperture, dryAperture; // Shared areas in m².
    std::vector<RectangularPatch> wetPatches, dryPatches;
};

// Magnitude of the part of a chart-axis pressure link below the water plane.
// The remaining link length belongs to air, including a centre exactly on it.
inline double LiquidLinkLength(double z0, double z1, double waterHeight) {
    const double lower = std::min(z0, z1), upper = std::max(z0, z1);
    return std::max(0., std::min(upper, waterHeight) - lower);
}

namespace grid_phase_detail {

using moving_geometry_detail::Index;
using moving_geometry_detail::Overlap;

inline bool ValidateChart(const GridLayout& layout, int axis, std::string& error) {
    if (axis < 0 || axis > 2 || layout.CheckedCellCount() == 0) {
        error = "invalid flat water grid or chart axis";
        return false;
    }
    return true;
}

inline bool Prepare(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                    int axis, std::vector<BoxMotion>& boxes, std::string& error) {
    if (!ValidateChart(layout, axis, error))
        return false;
    for (const auto& box : stationary) {
        if (box.translation != GridVec{}) {
            error = "flat water phase geometry requires a stationary snapshot";
            return false;
        }
    }
    return moving_geometry_detail::NormalizeBoxes(layout, stationary, boxes, error);
}

inline bool HeightInGridUnits(const GridLayout& layout, int axis, double height,
                              double& normalized, std::string& error) {
    const double top = layout.cells[axis] * layout.h;
    if (!(height >= 0. && height <= top)) {
        error = "flat water height is outside the grid";
        return false;
    }
    normalized = height == top ? layout.cells[axis] : height / layout.h;
    return true;
}

// Geometry is in grid units. Intersect with either half-space, then use the
// result only where the clipped interval has positive length.
inline void ClipInterval(double& lo, double& hi, double height, bool wet) {
    if (wet)
        hi = std::min(hi, height);
    else
        lo = std::max(lo, height);
}

inline double CellSpan(int cell, double lo, double hi, double height, bool wet) {
    ClipInterval(lo, hi, height, wet);
    return Overlap(lo, hi, cell, cell + 1);
}

inline bool PlaneIsWet(int plane, double height, int top) {
    return plane < height || (height == top && plane == top);
}

template <typename Visit>
void VisitGrid(const GridLayout& layout, int faceAxis, Visit visit) {
    for (int k = 0; k < layout.cells[2] + (faceAxis == 2); ++k)
        for (int j = 0; j < layout.cells[1] + (faceAxis == 1); ++j)
            for (int i = 0; i < layout.cells[0] + (faceAxis == 0); ++i)
                visit(Index{i, j, k});
}

inline void AppendRectangularPatch(std::vector<RectangularPatch>& patches,
                                   const GridLayout& layout, const BoxMotion& box,
                                   const Index& cell, int normal, int sign,
                                   int up, double height, bool wet) {
    const double edge = sign < 0 ? box.lo[normal] : box.hi[normal];
    if (!moving_geometry_detail::OwnsPatch(edge, cell[normal], sign) ||
        (normal == up && ((edge < height) != wet)))
        return;

    GridVec lo{}, hi{}, centroid{};
    double area = 1.;
    for (int axis = 0; axis < 3; ++axis) {
        if (axis == normal) {
            lo[axis] = hi[axis] = edge;
        } else {
            lo[axis] = std::max(double(cell[axis]), box.lo[axis]);
            hi[axis] = std::min(double(cell[axis] + 1), box.hi[axis]);
            if (axis == up)
                ClipInterval(lo[axis], hi[axis], height, wet);
            if (hi[axis] <= lo[axis])
                return;
            area *= hi[axis] - lo[axis];
        }
        centroid[axis] = (lo[axis] + hi[axis]) * (.5 * layout.h);
        lo[axis] *= layout.h;
        hi[axis] *= layout.h;
    }
    patches.push_back({{box.owner, layout.Cell(cell[0], cell[1], cell[2]), normal, sign,
                        centroid, area * (layout.h * layout.h)}, lo, hi});
}

struct CapacityKnot {
    double height;
    double volume;
};

inline bool CapacityProfile(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                            int up, std::vector<CapacityKnot>& profile, std::string& error) {
    std::vector<double> cuts{0., double(layout.cells[up])};
    for (const auto& box : boxes) {
        cuts.push_back(box.lo[up]);
        cuts.push_back(box.hi[up]);
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());

    const int b = (up + 1) % 3, c = (up + 2) % 3;
    profile.push_back({0., 0.});
    for (std::size_t i = 1; i < cuts.size(); ++i) {
        double area = double(layout.cells[b]) * layout.cells[c];
        for (const auto& box : boxes) {
            if (box.lo[up] <= cuts[i - 1] && box.hi[up] >= cuts[i])
                area -= (box.hi[b] - box.lo[b]) * (box.hi[c] - box.lo[c]);
        }
        const double volume = std::fma(area, cuts[i] - cuts[i - 1], profile.back().volume);
        if (!(area > 0.) || !(volume > profile.back().volume)) {
            error = "flat water layer capacity is not representable";
            return false;
        }
        profile.push_back({cuts[i], volume});
    }
    return true;
}

} // namespace grid_phase_detail

// The water plane lies perpendicular to axis. Water lies below it in chart
// coordinates. An aperture or patch exactly on the plane belongs to air,
// except for a full grid's upper boundary, whose interior is water.
// In a quiescent pool the plane's water-gauge hydrostatic pressure is zero.
inline bool BuildGridPhaseGeometry(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                                   int axis, double height, GridPhaseGeometry& output,
                                   std::string& error) {
    using namespace grid_phase_detail;
    using moving_geometry_detail::AdmitRoundoff;
    using moving_geometry_detail::VisitSweptBounds;

    std::vector<BoxMotion> boxes;
    double plane = 0.;
    if (!Prepare(layout, stationary, axis, boxes, error) ||
        !HeightInGridUnits(layout, axis, height, plane, error))
        return false;

    const double area = layout.h * layout.h, volume = area * layout.h;
    const auto count = layout.CellCount();
    GridPhaseGeometry next;
    next.axis = axis;
    next.height = height;
    const auto wetPlane = [&](int index) {
        return PlaneIsWet(index, plane, layout.cells[axis]);
    };
    for (const bool wet : {true, false}) {
        auto& cells = wet ? next.water : next.air;
        auto& apertures = wet ? next.wetAperture : next.dryAperture;
        auto& patches = wet ? next.wetPatches : next.dryPatches;
        cells.resize(count);
        VisitGrid(layout, -1, [&](const Index& cell) {
            cells[layout.Cell(cell[0], cell[1], cell[2])] =
                volume * CellSpan(cell[axis], 0., layout.cells[axis], plane, wet);
        });
        for (int normal = 0; normal < 3; ++normal) {
            apertures[normal].resize(layout.FaceCount(normal));
            VisitGrid(layout, normal, [&](const Index& face) {
                const double fraction = normal == axis
                    ? double(wetPlane(face[axis]) == wet)
                    : CellSpan(face[axis], 0., layout.cells[axis], plane, wet);
                apertures[normal][layout.Face(normal, face[0], face[1], face[2])] =
                    area * fraction;
            });
        }

        for (const auto& box : boxes) {
            VisitSweptBounds(layout, box, -1, [&](const Index& cell) {
                double occupied = volume;
                for (int direction = 0; direction < 3; ++direction) {
                    occupied *= direction == axis
                        ? CellSpan(cell[axis], box.lo[axis], box.hi[axis], plane, wet)
                        : Overlap(box.lo[direction], box.hi[direction],
                                  cell[direction], cell[direction] + 1);
                }
                cells[layout.Cell(cell[0], cell[1], cell[2])] -= occupied;
                for (int normal = 0; normal < 3; ++normal)
                    for (int sign : {-1, 1})
                        AppendRectangularPatch(patches, layout, box, cell, normal, sign,
                                               axis, plane, wet);
            });

            for (int normal = 0; normal < 3; ++normal) {
                VisitSweptBounds(layout, box, normal, [&](const Index& face) {
                    if (!moving_geometry_detail::CoversPlane(box, normal, face[normal], 0.) ||
                        (normal == axis && (wetPlane(face[axis]) != wet)))
                        return;
                    double occupied = area;
                    for (int tangent : {(normal + 1) % 3, (normal + 2) % 3}) {
                        occupied *= tangent == axis
                            ? CellSpan(face[axis], box.lo[axis], box.hi[axis], plane, wet)
                            : Overlap(box.lo[tangent], box.hi[tangent],
                                      face[tangent], face[tangent] + 1);
                    }
                    apertures[normal][layout.Face(normal, face[0], face[1], face[2])] -= occupied;
                });
            }
        }

        for (double& cell : cells) {
            if (!AdmitRoundoff(cell, volume)) {
                error = "invalid flat water cell volume";
                return false;
            }
        }
        for (auto& faces : apertures) {
            for (double& face : faces) {
                if (!AdmitRoundoff(face, area)) {
                    error = "invalid flat water face aperture";
                    return false;
                }
            }
        }
    }
    output = std::move(next);
    error.clear();
    return true;
}

// Average the open part of each shared face below a fixed planar reference.
// Linear box paths may cross the plane. The phase plane itself does not move.
// A failed build leaves the prior apertures intact.
inline bool AverageWetAperture(const GridLayout& layout, const std::vector<BoxMotion>& movingBoxes,
                               int axis, double height,
                               std::array<std::vector<double>, 3>& output, std::string& error) {
    using namespace grid_phase_detail;
    using moving_geometry_detail::AdmitRoundoff;
    using moving_geometry_detail::AverageBlockedFace;
    using moving_geometry_detail::FaceUpper;
    using moving_geometry_detail::VisitSweptBounds;

    std::vector<BoxMotion> boxes;
    double plane = 0.;
    if (!ValidateChart(layout, axis, error) ||
        !HeightInGridUnits(layout, axis, height, plane, error) ||
        !moving_geometry_detail::NormalizeBoxes(layout, movingBoxes, boxes, error))
        return false;

    const double area = layout.h * layout.h;
    const auto wetFraction = [&](int normal, const Index& face) {
        return normal == axis
            ? double(PlaneIsWet(face[axis], plane, layout.cells[axis]))
            : std::clamp(plane - face[axis], 0., 1.);
    };
    std::array<std::vector<double>, 3> next;
    for (int normal = 0; normal < 3; ++normal) {
        next[normal].resize(layout.FaceCount(normal));
        VisitGrid(layout, normal, [&](const Index& face) {
            next[normal][layout.Face(normal, face[0], face[1], face[2])] =
                area * wetFraction(normal, face);
        });
        for (const auto& box : boxes) {
            VisitSweptBounds(layout, box, normal, [&](const Index& face) {
                if (wetFraction(normal, face) == 0.)
                    return;
                auto upper = FaceUpper(face);
                if (normal != axis)
                    upper[axis] = std::min(upper[axis], plane);
                next[normal][layout.Face(normal, face[0], face[1], face[2])] -=
                    area * AverageBlockedFace(box, normal, face, upper);
            });
        }
        for (double& aperture : next[normal]) {
            if (!AdmitRoundoff(aperture, area)) {
                error = "moving wet aperture is outside the face area";
                return false;
            }
        }
    }
    output = std::move(next);
    error.clear();
    return true;
}

// Integrate or invert the exact piecewise-linear storage profile. These
// scalar operations do not allocate cell or face arrays.
inline bool FlatWaterVolume(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                            int axis, double height, double& volume, std::string& error) {
    using namespace grid_phase_detail;
    std::vector<BoxMotion> boxes;
    std::vector<CapacityKnot> profile;
    double plane = 0.;
    if (!Prepare(layout, stationary, axis, boxes, error) ||
        !HeightInGridUnits(layout, axis, height, plane, error) ||
        !CapacityProfile(layout, boxes, axis, profile, error))
        return false;

    const auto upper = std::lower_bound(profile.begin(), profile.end(), plane,
                                        [](const auto& knot, double x) { return knot.height < x; });
    double inGrid = upper->volume;
    if (upper->height != plane) {
        const auto& lower = *(upper - 1);
        const double t = (plane - lower.height) / (upper->height - lower.height);
        inGrid = std::fma(t, upper->volume - lower.volume, lower.volume);
    }
    volume = inGrid * ((layout.h * layout.h) * layout.h);
    error.clear();
    return true;
}

inline bool FlatWaterHeight(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                            int axis, double volume, double& height, std::string& error) {
    using namespace grid_phase_detail;
    std::vector<BoxMotion> boxes;
    std::vector<CapacityKnot> profile;
    if (!Prepare(layout, stationary, axis, boxes, error) ||
        !CapacityProfile(layout, boxes, axis, profile, error))
        return false;

    const double cellVolume = (layout.h * layout.h) * layout.h;
    if (!(volume >= 0. && volume <= profile.back().volume * cellVolume)) {
        error = "flat water volume exceeds grid capacity";
        return false;
    }
    const auto upper = std::lower_bound(profile.begin(), profile.end(), volume,
                                        [cellVolume](const auto& knot, double x) {
                                            return knot.volume * cellVolume < x;
                                        });
    double inGrid = upper->height;
    if (upper->volume * cellVolume != volume) {
        const auto& lower = *(upper - 1);
        const double t = (volume / cellVolume - lower.volume) / (upper->volume - lower.volume);
        inGrid = std::fma(t, upper->height - lower.height, lower.height);
    }
    height = inGrid * layout.h;
    error.clear();
    return true;
}

} // namespace judas::volume_grid
