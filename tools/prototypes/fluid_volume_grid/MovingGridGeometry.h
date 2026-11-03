#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace judas::volume_grid {

using GridVec = std::array<double, 3>;

struct GridLayout {
    std::array<int, 3> cells{};
    double h = 0;

    std::size_t CheckedCellCount() const {
        constexpr std::size_t maxCells = 1u << 20;
        std::size_t count = 1;
        for (int length : cells) {
            if (length <= 0 || std::size_t(length) > maxCells / count)
                return 0;
            count *= std::size_t(length);
        }
        const double volume = (h * h) * h;
        return h > 0. && volume > 0. && std::isfinite(double(count) * volume) ? count : 0;
    }

    std::size_t CellCount() const {
        return std::size_t(cells[0]) * std::size_t(cells[1]) * std::size_t(cells[2]);
    }

    std::size_t FaceCount(int axis) const {
        return std::size_t(cells[0] + (axis == 0)) * std::size_t(cells[1] + (axis == 1)) *
               std::size_t(cells[2] + (axis == 2));
    }

    std::size_t Cell(int i, int j, int k) const {
        return (std::size_t(k) * cells[1] + j) * cells[0] + i;
    }

    std::size_t Face(int axis, int i, int j, int k) const {
        return (std::size_t(k) * (cells[1] + (axis == 1)) + j) *
               (cells[0] + (axis == 0)) + i;
    }
};

// Coordinates are relative to the grid origin. Each owner supplies one
// axis-aligned box that translates linearly over the entire step.
struct BoxMotion {
    std::uint32_t owner = 0;
    GridVec lo{}, hi{}, translation{};
};

struct SolidPatch {
    std::uint32_t owner;
    std::size_t cell;
    int axis, sign; // The sign of the normal from the solid into the fluid.
    GridVec centroid;
    double area;
};

struct MovingGridGeometry {
    std::vector<double> startCapacity, endCapacity;
    std::array<std::vector<double>, 3> startAperture, endAperture, averageAperture, solidSweep;
    std::vector<SolidPatch> startPatches, endPatches, averagePatches;
};

namespace moving_geometry_detail {

using Index = std::array<int, 3>;

// All intersection work uses grid units. A grid cell is a unit cube.
inline double At(double x, double delta, double time) {
    return std::fma(delta, time, x);
}

inline double Overlap(double a0, double a1, double b0, double b1) {
    return std::max(0., std::min(a1, b1) - std::max(a0, b0));
}

inline double CellOverlap(const BoxMotion& box, int axis, int cell, double time) {
    return Overlap(At(box.lo[axis], box.translation[axis], time),
                   At(box.hi[axis], box.translation[axis], time), cell, cell + 1);
}

struct Events {
    std::array<double, 14> times{0., 1.};
    std::size_t count = 2;

    void Cross(double bound, double delta, double plane) {
        if (delta == 0.)
            return;
        const double t = (plane - bound) / delta;
        if (t > 0. && t < 1.)
            times[count++] = t;
    }

    void Sort() {
        std::sort(times.begin(), times.begin() + count);
    }
};

inline void AddTangentEvents(Events& events, const BoxMotion& box, int axis,
                             double lower, double upper) {
    for (const double edge : {box.lo[axis], box.hi[axis]}) {
        events.Cross(edge, box.translation[axis], lower);
        events.Cross(edge, box.translation[axis], upper);
    }
}

template <typename Visit>
void VisitSweptBounds(const GridLayout& layout, const BoxMotion& box, int faceAxis, Visit visit) {
    Index first{}, stop{};
    for (int axis = 0; axis < 3; ++axis) {
        const double lower = std::min(box.lo[axis], At(box.lo[axis], box.translation[axis], 1.));
        const double upper = std::max(box.hi[axis], At(box.hi[axis], box.translation[axis], 1.));
        // Include both cells adjacent to a coincident solid face.
        first[axis] = std::max(0, int(std::floor(lower)) - 1);
        stop[axis] = std::min(layout.cells[axis] + (axis == faceAxis), int(std::floor(upper)) + 2);
    }
    for (int k = first[2]; k < stop[2]; ++k)
        for (int j = first[1]; j < stop[1]; ++j)
            for (int i = first[0]; i < stop[0]; ++i)
                visit(Index{i, j, k});
}

inline bool SweptBoxesTouch(const BoxMotion& a, const BoxMotion& b) {
    double first = 0., last = 1.;
    for (int axis = 0; axis < 3; ++axis) {
        const double lower = a.lo[axis] - b.hi[axis];
        const double upper = a.hi[axis] - b.lo[axis];
        const double relative = a.translation[axis] - b.translation[axis];
        if (relative == 0.) {
            if (lower > 0. || upper < 0.)
                return false;
            continue;
        }
        const double t0 = -upper / relative, t1 = -lower / relative;
        first = std::max(first, std::min(t0, t1));
        last = std::min(last, std::max(t0, t1));
        if (first > last)
            return false;
    }
    return true;
}

inline bool NormalizeBoxes(const GridLayout& layout, const std::vector<BoxMotion>& motions,
                           std::vector<BoxMotion>& boxes, std::string& error) {
    constexpr std::size_t maxBoxes = 256;
    if (motions.size() > maxBoxes) {
        error = "too many moving boxes";
        return false;
    }
    boxes.reserve(motions.size());
    for (auto box : motions) {
        for (int axis = 0; axis < 3; ++axis) {
            box.lo[axis] /= layout.h;
            box.hi[axis] /= layout.h;
            box.translation[axis] /= layout.h;
            const double endLo = At(box.lo[axis], box.translation[axis], 1.);
            const double endHi = At(box.hi[axis], box.translation[axis], 1.);
            if (!(0. < box.lo[axis] && box.lo[axis] < box.hi[axis] &&
                  box.hi[axis] < layout.cells[axis] && 0. < endLo && endLo < endHi &&
                  endHi < layout.cells[axis])) {
                error = "moving box must have finite positive extent and stay inside the grid";
                return false;
            }
        }
        for (const auto& existing : boxes) {
            if (box.owner == existing.owner || SweptBoxesTouch(box, existing)) {
                error = "moving boxes must have unique owners and remain disjoint throughout the step";
                return false;
            }
        }
        boxes.push_back(box);
    }
    return true;
}

inline bool CoversPlane(const BoxMotion& box, int axis, double plane, double time) {
    return At(box.lo[axis], box.translation[axis], time) <= plane &&
           At(box.hi[axis], box.translation[axis], time) >= plane;
}

inline double TangentArea(const BoxMotion& box, int axis, const Index& cell, double time) {
    const int b = (axis + 1) % 3, c = (axis + 2) % 3;
    return CellOverlap(box, b, cell[b], time) * CellOverlap(box, c, cell[c], time);
}

// The two tangent intervals may begin and end inside a parent face.
inline double AverageBlockedFace(const BoxMotion& box, int axis, const GridVec& face,
                                 const GridVec& upper) {
    const int b = (axis + 1) % 3, c = (axis + 2) % 3;
    const auto tangentArea = [&](double time) {
        const auto length = [&](int tangent) {
            return Overlap(At(box.lo[tangent], box.translation[tangent], time),
                           At(box.hi[tangent], box.translation[tangent], time),
                           face[tangent], upper[tangent]);
        };
        return length(b) * length(c);
    };
    Events events;
    events.Cross(box.lo[axis], box.translation[axis], face[axis]);
    events.Cross(box.hi[axis], box.translation[axis], face[axis]);
    for (int tangent : {b, c})
        AddTangentEvents(events, box, tangent, face[tangent], upper[tangent]);
    events.Sort();

    double blocked = 0.;
    for (std::size_t i = 1; i < events.count; ++i) {
        const double start = events.times[i - 1], end = events.times[i];
        const double middle = start + (end - start) / 2.;
        if (start == end || !CoversPlane(box, axis, face[axis], middle))
            continue;
        // The normal crossing is constant within each interval. Both tangent
        // overlap lengths are affine, so Simpson integrates their product.
        blocked += (end - start) / 6. *
                   (tangentArea(start) + 4. * tangentArea(middle) + tangentArea(end));
    }
    return blocked;
}

inline GridVec FaceUpper(const Index& face) {
    return {face[0] + 1., face[1] + 1., face[2] + 1.};
}

inline double AverageBlockedFace(const BoxMotion& box, int axis, const Index& face,
                                 const GridVec& upper) {
    return AverageBlockedFace(box, axis, GridVec{double(face[0]), double(face[1]), double(face[2])},
                              upper);
}

inline double AverageBlockedFace(const BoxMotion& box, int axis, const Index& face) {
    return AverageBlockedFace(box, axis, face, FaceUpper(face));
}

struct PatchSample {
    double area = 0.;
    GridVec centroid{};
};

inline bool OwnsPatch(double position, int cell, int sign) {
    // At exact coincidence choose the fluid-facing cell, never the solid one.
    return position >= cell && position <= cell + 1 &&
           !(position == cell && sign < 0) && !(position == cell + 1 && sign > 0);
}

inline PatchSample SamplePatch(const BoxMotion& box, int axis, double edge, const Index& cell,
                               double time) {
    PatchSample sample{1., {}};
    sample.centroid[axis] = At(edge, box.translation[axis], time);
    for (int tangent : {(axis + 1) % 3, (axis + 2) % 3}) {
        const double lower = std::max(double(cell[tangent]),
                                      At(box.lo[tangent], box.translation[tangent], time));
        const double upper = std::min(double(cell[tangent] + 1),
                                      At(box.hi[tangent], box.translation[tangent], time));
        sample.area *= std::max(0., upper - lower);
        sample.centroid[tangent] = (lower + upper) / 2.;
    }
    return sample;
}

inline PatchSample AveragePatch(const BoxMotion& box, int axis, int sign, double edge,
                                const Index& cell) {
    Events events;
    events.Cross(edge, box.translation[axis], cell[axis]);
    events.Cross(edge, box.translation[axis], cell[axis] + 1);
    for (int tangent : {(axis + 1) % 3, (axis + 2) % 3})
        AddTangentEvents(events, box, tangent, cell[tangent], cell[tangent] + 1);
    events.Sort();

    PatchSample result;
    for (std::size_t i = 1; i < events.count; ++i) {
        const double start = events.times[i - 1], end = events.times[i];
        const double middle = start + (end - start) / 2.;
        if (start == end || !OwnsPatch(At(edge, box.translation[axis], middle), cell[axis], sign))
            continue;
        const auto a = SamplePatch(box, axis, edge, cell, start);
        const auto b = SamplePatch(box, axis, edge, cell, middle);
        const auto c = SamplePatch(box, axis, edge, cell, end);
        const double scale = (end - start) / 6.;
        result.area += scale * (a.area + 4. * b.area + c.area);
        // Area times centroid is at most cubic on this interval.
        for (int d = 0; d < 3; ++d) {
            result.centroid[d] += scale * (a.area * a.centroid[d] + 4. * b.area * b.centroid[d] +
                                           c.area * c.centroid[d]);
        }
    }
    if (result.area > 0.)
        for (double& value : result.centroid)
            value /= result.area;
    return result;
}

inline void AppendPatch(std::vector<SolidPatch>& patches, const GridLayout& layout,
                        const BoxMotion& box, const Index& cell, int axis, int sign,
                        PatchSample sample) {
    if (sample.area == 0.)
        return;
    for (double& value : sample.centroid)
        value *= layout.h;
    patches.push_back({box.owner, layout.Cell(cell[0], cell[1], cell[2]), axis, sign,
                       sample.centroid, sample.area * (layout.h * layout.h)});
}

inline bool AdmitRoundoff(double& quantity, double maximum) {
    const double rounding = 128. * std::numeric_limits<double>::epsilon() * maximum;
    if (!std::isfinite(quantity) || quantity < -rounding || quantity > maximum + rounding)
        return false;
    quantity = std::clamp(quantity, 0., maximum);
    return true;
}

} // namespace moving_geometry_detail

// Capacities and signed sweeps are in m³, apertures and patch areas are in m².
// A positive sweep transfers solid in the positive axis direction. Thus
// capacity[end] - capacity[start] equals the outward divergence of solidSweep
// and the negative integrated outward solid-patch motion. A failed build
// leaves output intact. The bounded prototype admits separated boxes strictly
// inside the domain; wall contact and overlapping solids require union ownership.
inline bool BuildMovingGridGeometry(const GridLayout& layout, const std::vector<BoxMotion>& motions,
                                    MovingGridGeometry& output, std::string& error) {
    using namespace moving_geometry_detail;
    const double area = layout.h * layout.h, volume = area * layout.h;
    const auto count = layout.CheckedCellCount();
    if (count == 0) {
        error = "invalid or oversized moving grid";
        return false;
    }

    std::vector<BoxMotion> boxes;
    if (!NormalizeBoxes(layout, motions, boxes, error))
        return false;

    MovingGridGeometry next;
    next.startCapacity.assign(count, volume);
    next.endCapacity.assign(count, volume);
    for (int axis = 0; axis < 3; ++axis) {
        const auto n = layout.FaceCount(axis);
        next.startAperture[axis].assign(n, area);
        next.endAperture[axis].assign(n, area);
        next.averageAperture[axis].assign(n, area);
        next.solidSweep[axis].resize(n);
    }
    for (const auto& box : boxes) {
        VisitSweptBounds(layout, box, -1, [&](const Index& cell) {
            const auto index = layout.Cell(cell[0], cell[1], cell[2]);
            double start = volume, end = volume;
            for (int axis = 0; axis < 3; ++axis) {
                start *= CellOverlap(box, axis, cell[axis], 0.);
                end *= CellOverlap(box, axis, cell[axis], 1.);
            }
            next.startCapacity[index] -= start;
            next.endCapacity[index] -= end;

            for (int axis = 0; axis < 3; ++axis) {
                for (int sign : {-1, 1}) {
                    const double edge = sign < 0 ? box.lo[axis] : box.hi[axis];
                    if (OwnsPatch(edge, cell[axis], sign))
                        AppendPatch(next.startPatches, layout, box, cell, axis, sign,
                                    SamplePatch(box, axis, edge, cell, 0.));
                    if (OwnsPatch(At(edge, box.translation[axis], 1.), cell[axis], sign))
                        AppendPatch(next.endPatches, layout, box, cell, axis, sign,
                                    SamplePatch(box, axis, edge, cell, 1.));
                    AppendPatch(next.averagePatches, layout, box, cell, axis, sign,
                                AveragePatch(box, axis, sign, edge, cell));
                }
            }
        });

        for (int axis = 0; axis < 3; ++axis) {
            VisitSweptBounds(layout, box, axis, [&](const Index& face) {
                const auto index = layout.Face(axis, face[0], face[1], face[2]);
                if (CoversPlane(box, axis, face[axis], 0.))
                    next.startAperture[axis][index] -= area * TangentArea(box, axis, face, 0.);
                if (CoversPlane(box, axis, face[axis], 1.))
                    next.endAperture[axis][index] -= area * TangentArea(box, axis, face, 1.);
                const double average = AverageBlockedFace(box, axis, face);
                next.averageAperture[axis][index] -= area * average;
                next.solidSweep[axis][index] += volume * box.translation[axis] * average;
            });
        }
    }

    for (auto* capacities : {&next.startCapacity, &next.endCapacity}) {
        for (double& capacity : *capacities) {
            if (!AdmitRoundoff(capacity, volume)) {
                error = "moving box capacity is outside the cell volume";
                return false;
            }
        }
    }
    for (auto* apertures : {&next.startAperture, &next.endAperture, &next.averageAperture}) {
        for (auto& faces : *apertures) {
            for (double& aperture : faces) {
                if (!AdmitRoundoff(aperture, area)) {
                    error = "moving box aperture is outside the face area";
                    return false;
                }
            }
        }
    }
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
