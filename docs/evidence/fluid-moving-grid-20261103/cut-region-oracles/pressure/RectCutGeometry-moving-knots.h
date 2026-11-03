#pragma once

#include "MovingGridGeometry.h"

namespace judas::volume_grid::rect_cut {

using Index = moving_geometry_detail::Index;

struct Grid {
    std::array<std::vector<double>, 3> planes;
    Index cells{};

    std::size_t Count() const {
        return std::size_t(cells[0]) * cells[1] * cells[2];
    }

    std::size_t Cell(const Index& i) const {
        return (std::size_t(i[2]) * cells[1] + i[1]) * cells[0] + i[0];
    }
};

struct Connection {
    std::size_t left, right;
    int axis;
    GridVec lower, upper;
    double startArea, endArea, meanArea;
    double sweptVolume;
};

struct Geometry {
    Grid grid;
    std::vector<double> startCapacity, endCapacity;
    std::vector<Connection> links;
    std::vector<SolidPatch> patches;
};

inline bool ClipGrid(const GridLayout& chart, const BoxMotion& body, Grid& output) {
    using moving_geometry_detail::At;
    if (chart.CheckedCellCount() == 0)
        return false;

    Grid grid;
    for (int axis = 0; axis < 3; ++axis) {
        auto& planes = grid.planes[axis];
        const double end = chart.cells[axis] * chart.h;
        for (int i = 0; i <= chart.cells[axis]; ++i)
            planes.push_back(i * chart.h);
        for (const double point : {body.lo[axis], body.hi[axis]}) {
            const double moved = At(point, body.translation[axis], 1.);
            if (!(point > 0. && point < end && moved > 0. && moved < end))
                return false;
            planes.push_back(point);
            planes.push_back(moved);
        }
        if (!(body.lo[axis] < body.hi[axis]))
            return false;
        std::sort(planes.begin(), planes.end());
        planes.erase(std::unique(planes.begin(), planes.end()), planes.end());
        grid.cells[axis] = static_cast<int>(planes.size() - 1);
    }
    if (grid.Count() > 262144)
        return false;
    output = std::move(grid);
    return true;
}

inline std::pair<GridVec, GridVec> CellBounds(const Grid& grid, const Index& i) {
    GridVec lo, hi;
    for (int axis = 0; axis < 3; ++axis) {
        lo[axis] = grid.planes[axis][i[axis]];
        hi[axis] = grid.planes[axis][i[axis] + 1];
    }
    return {lo, hi};
}

inline double Capacity(const BoxMotion& body, const GridVec& lo, const GridVec& hi, double t) {
    using namespace moving_geometry_detail;
    double volume = 1., blocked = 1.;
    for (int axis = 0; axis < 3; ++axis) {
        volume *= hi[axis] - lo[axis];
        blocked *= Overlap(lo[axis], hi[axis], At(body.lo[axis], body.translation[axis], t),
                          At(body.hi[axis], body.translation[axis], t));
    }
    return volume - blocked;
}

inline double BlockedAt(const BoxMotion& body, int axis, const GridVec& lo,
                        const GridVec& hi, double t) {
    using namespace moving_geometry_detail;
    if (!CoversPlane(body, axis, lo[axis], t))
        return 0.;
    double area = 1.;
    for (int tangent : {(axis + 1) % 3, (axis + 2) % 3})
        area *= Overlap(lo[tangent], hi[tangent], At(body.lo[tangent], body.translation[tangent], t),
                        At(body.hi[tangent], body.translation[tangent], t));
    return area;
}

inline moving_geometry_detail::PatchSample SamplePatch(const BoxMotion& body, int axis, double edge,
                                                         const GridVec& lo, const GridVec& hi, double t) {
    using namespace moving_geometry_detail;
    PatchSample result{1., {}};
    result.centroid[axis] = At(edge, body.translation[axis], t);
    for (int tangent : {(axis + 1) % 3, (axis + 2) % 3}) {
        const double lower = std::max(lo[tangent], At(body.lo[tangent], body.translation[tangent], t));
        const double upper = std::min(hi[tangent], At(body.hi[tangent], body.translation[tangent], t));
        result.area *= std::max(0., upper - lower);
        result.centroid[tangent] = lower + .5 * (upper - lower);
    }
    return result;
}

inline moving_geometry_detail::PatchSample PatchAverage(const BoxMotion& body, int axis, int sign,
                                                          const GridVec& lo, const GridVec& hi) {
    using namespace moving_geometry_detail;
    const double edge = sign < 0 ? body.lo[axis] : body.hi[axis];
    Events events;
    events.Cross(edge, body.translation[axis], lo[axis]);
    events.Cross(edge, body.translation[axis], hi[axis]);
    for (int tangent : {(axis + 1) % 3, (axis + 2) % 3})
        AddTangentEvents(events, body, tangent, lo[tangent], hi[tangent]);
    events.Sort();

    PatchSample result;
    for (std::size_t j = 1; j < events.count; ++j) {
        const double start = events.times[j - 1], end = events.times[j];
        const double middle = start + (end - start) / 2.;
        const double coordinate = At(edge, body.translation[axis], middle);
        if (start == end || coordinate < lo[axis] || coordinate > hi[axis] ||
            (coordinate == lo[axis] && sign < 0) || (coordinate == hi[axis] && sign > 0))
            continue;
        const auto a = SamplePatch(body, axis, edge, lo, hi, start);
        const auto b = SamplePatch(body, axis, edge, lo, hi, middle);
        const auto c = SamplePatch(body, axis, edge, lo, hi, end);
        const double w = (end - start) / 6.;
        result.area += w * (a.area + 4. * b.area + c.area);
        for (int d = 0; d < 3; ++d)
            result.centroid[d] += w * (a.area * a.centroid[d] + 4. * b.area * b.centroid[d] +
                                      c.area * c.centroid[d]);
    }
    if (result.area > 0.)
        for (double& moment : result.centroid)
            moment /= result.area;
    return result;
}

inline bool Build(const GridLayout& chart, const BoxMotion& body, Geometry& output) {
    using namespace moving_geometry_detail;
    Geometry next;
    if (!ClipGrid(chart, body, next.grid))
        return false;
    const Grid& grid = next.grid;
    next.startCapacity.resize(grid.Count());
    next.endCapacity.resize(grid.Count());
    for (int z = 0; z < grid.cells[2]; ++z) {
        for (int y = 0; y < grid.cells[1]; ++y) {
            for (int x = 0; x < grid.cells[0]; ++x) {
                const Index i{x, y, z};
                const auto [lo, hi] = CellBounds(grid, i);
                const auto cell = grid.Cell(i);
                next.startCapacity[cell] = Capacity(body, lo, hi, 0.);
                next.endCapacity[cell] = Capacity(body, lo, hi, 1.);
                for (int normal = 0; normal < 3; ++normal) {
                    for (int sign : {-1, 1}) {
                        const auto patch = PatchAverage(body, normal, sign, lo, hi);
                        if (patch.area > 0.)
                            next.patches.push_back({body.owner, cell, normal, sign,
                                                    patch.centroid, patch.area});
                    }
                    if (i[normal] == grid.cells[normal] - 1)
                        continue;
                    auto right = i;
                    ++right[normal];
                    auto lower = lo;
                    lower[normal] = hi[normal];
                    const int a = (normal + 1) % 3, b = (normal + 2) % 3;
                    const double area = (hi[a] - lo[a]) * (hi[b] - lo[b]);
                    const double blocked = AverageBlockedFace(body, normal, lower, hi);
                    Connection face{cell, grid.Cell(right), normal, lower, hi,
                        area - BlockedAt(body, normal, lower, hi, 0.),
                        area - BlockedAt(body, normal, lower, hi, 1.),
                        area - blocked, body.translation[normal] * blocked};
                    if (!AdmitRoundoff(face.startArea, area) || !AdmitRoundoff(face.endArea, area) ||
                        !AdmitRoundoff(face.meanArea, area))
                        return false;
                    if (face.meanArea > 0. || face.sweptVolume != 0.)
                        next.links.push_back(face);
                }
            }
        }
    }
    output = std::move(next);
    return true;
}

} // namespace judas::volume_grid::rect_cut
