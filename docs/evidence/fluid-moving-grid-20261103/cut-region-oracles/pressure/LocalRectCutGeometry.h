#pragma once

#include "RectCutGeometry.h"

namespace judas::volume_grid::rect_cut {

struct LocalVolume {
    GridVec lo, hi;
    std::size_t parent;
};

struct LocalConnection : Connection {
    double length;
};

struct LocalChart {
    GridLayout chart;
    std::vector<LocalVolume> volumes;
    std::vector<std::size_t> offsets;
};

struct LocalGeometry {
    std::vector<double> startCapacity, endCapacity;
    std::vector<LocalConnection> links;
    std::vector<SolidPatch> patches;
};

inline bool PrepareLocalChart(const GridLayout& chart, const BoxMotion& initial,
                              const GridVec& minTravel, const GridVec& maxTravel,
                              LocalChart& output) {
    using namespace moving_geometry_detail;
    const auto count = chart.CheckedCellCount();
    if (count == 0)
        return false;
    for (int axis = 0; axis < 3; ++axis) {
        if (!(minTravel[axis] <= maxTravel[axis] && initial.lo[axis] < initial.hi[axis] &&
              initial.lo[axis] + std::min(minTravel[axis], 0.) > 0. &&
              initial.hi[axis] + std::max(maxTravel[axis], 0.) < chart.cells[axis] * chart.h))
            return false;
    }
    LocalChart next{chart, {}, {}};
    next.offsets.reserve(count + 1);
    for (int z = 0; z < chart.cells[2]; ++z) {
        for (int y = 0; y < chart.cells[1]; ++y) {
            for (int x = 0; x < chart.cells[0]; ++x) {
                const Index index{x, y, z};
                const auto parent = chart.Cell(x, y, z);
                next.offsets.push_back(next.volumes.size());
                GridVec lo, hi;
                bool cut = true;
                for (int axis = 0; axis < 3; ++axis) {
                    lo[axis] = index[axis] * chart.h;
                    hi[axis] = (index[axis] + 1) * chart.h;
                    cut = cut && Overlap(lo[axis], hi[axis],
                        initial.lo[axis] + std::min(0., minTravel[axis]),
                        initial.hi[axis] + std::max(0., maxTravel[axis])) > 0.;
                }
                if (!cut) {
                    next.volumes.push_back({lo, hi, parent});
                    continue;
                }
                std::array<std::vector<double>, 3> planes;
                for (int axis = 0; axis < 3; ++axis) {
                    auto& cuts = planes[axis];
                    cuts = {lo[axis], hi[axis]};
                    for (double wall : {initial.lo[axis], initial.hi[axis]}) {
                        for (double d : {0., minTravel[axis], maxTravel[axis]}) {
                            const double edge = wall + d;
                            if (edge > lo[axis] && edge < hi[axis])
                                cuts.push_back(edge);
                        }
                    }
                    std::sort(cuts.begin(), cuts.end());
                    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
                }
                for (std::size_t k = 1; k < planes[2].size(); ++k)
                    for (std::size_t j = 1; j < planes[1].size(); ++j)
                        for (std::size_t i = 1; i < planes[0].size(); ++i)
                            next.volumes.push_back({{planes[0][i-1], planes[1][j-1], planes[2][k-1]},
                                {planes[0][i], planes[1][j], planes[2][k]}, parent});
                if (next.volumes.size() > 262144)
                    return false;
            }
        }
    }
    next.offsets.push_back(next.volumes.size());
    output = std::move(next);
    return true;
}

inline bool BuildLocalGeometry(const LocalChart& data, const BoxMotion& body,
                               LocalGeometry& output) {
    using namespace moving_geometry_detail;
    LocalGeometry next;
    next.startCapacity.resize(data.volumes.size());
    next.endCapacity.resize(data.volumes.size());
    for (std::size_t i = 0; i < data.volumes.size(); ++i) {
        const auto& volume = data.volumes[i];
        next.startCapacity[i] = Capacity(body, volume.lo, volume.hi, 0.);
        next.endCapacity[i] = Capacity(body, volume.lo, volume.hi, 1.);
        for (int axis = 0; axis < 3; ++axis) {
            for (int sign : {-1, 1}) {
                const auto patch = PatchAverage(body, axis, sign, volume.lo, volume.hi);
                if (patch.area > 0.)
                    next.patches.push_back({body.owner, i, axis, sign, patch.centroid, patch.area});
            }
        }
    }

    const auto link = [&](std::size_t i, std::size_t j, int axis) {
        const auto& left = data.volumes[i];
        const auto& right = data.volumes[j];
        if (left.hi[axis] != right.lo[axis])
            return true;
        GridVec lo, hi;
        double area = 1.;
        for (int d = 0; d < 3; ++d) {
            if (d == axis)
                lo[d] = hi[d] = right.lo[d];
            else {
                lo[d] = std::max(left.lo[d], right.lo[d]);
                hi[d] = std::min(left.hi[d], right.hi[d]);
                if (!(lo[d] < hi[d]))
                    return true;
                area *= hi[d] - lo[d];
            }
        }
        const double blocked = AverageBlockedFace(body, axis, lo, hi);
        LocalConnection face{{i, j, axis, lo, hi,
            area - BlockedAt(body, axis, lo, hi, 0.),
            area - BlockedAt(body, axis, lo, hi, 1.),
            area - blocked, body.translation[axis] * blocked},
            .5 * (right.hi[axis] - left.lo[axis])};
        if (!AdmitRoundoff(face.startArea, area) || !AdmitRoundoff(face.endArea, area) ||
            !AdmitRoundoff(face.meanArea, area))
            return false;
        if (face.meanArea > 0. || face.sweptVolume != 0.)
            next.links.push_back(face);
        return true;
    };

    const auto& chart = data.chart;
    for (int z = 0; z < chart.cells[2]; ++z) {
        for (int y = 0; y < chart.cells[1]; ++y) {
            for (int x = 0; x < chart.cells[0]; ++x) {
                const Index index{x, y, z};
                const auto cell = chart.Cell(x, y, z);
                for (int axis = 0; axis < 3; ++axis) {
                    for (auto i = data.offsets[cell]; i < data.offsets[cell + 1]; ++i)
                        for (auto j = i + 1; j < data.offsets[cell + 1]; ++j)
                            if (!link(i, j, axis))
                                return false;
                    if (index[axis] + 1 == chart.cells[axis])
                        continue;
                    auto right = index;
                    ++right[axis];
                    const auto neighbor = chart.Cell(right[0], right[1], right[2]);
                    for (auto i = data.offsets[cell]; i < data.offsets[cell + 1]; ++i)
                        for (auto j = data.offsets[neighbor]; j < data.offsets[neighbor + 1]; ++j)
                            if (!link(i, j, axis))
                                return false;
                }
            }
        }
    }
    output = std::move(next);
    return true;
}

} // namespace judas::volume_grid::rect_cut
