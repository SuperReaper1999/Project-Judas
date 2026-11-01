#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>
#include <numeric>
#include <set>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

// The local hydrostatic approximation may interpolate across displaced-body
// holes, but it must not extrapolate wetness outside the observed lateral
// water envelope. Column footprints have the represented particle spacing;
// the much larger motion-donor search radius does not enlarge this envelope.
// The hull is an outer rejection bound; physical admission below additionally
// checks local water/solid support. This is not a multiphase interface model.
inline bool FluidWetFootprintContainsOrigin(std::vector<glm::dvec2>& points) {
    if (points.empty()) return false;
    std::sort(points.begin(), points.end(), [](auto a, auto b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    points.erase(std::unique(points.begin(), points.end()), points.end());
    if (points.size() < 3) return false;
    const auto cross = [](glm::dvec2 a, glm::dvec2 b, glm::dvec2 c) {
        const auto u = b - a, v = c - a;
        return u.x * v.y - u.y * v.x;
    };
    std::vector<glm::dvec2> hull;
    hull.reserve(points.size() + 1);
    for (const auto& p : points) {
        while (hull.size() >= 2 && cross(hull[hull.size()-2], hull.back(), p) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }
    const auto lower = hull.size();
    for (auto i = points.size() - 1; i-- > 0;) {
        const auto p = points[i];
        while (hull.size() > lower && cross(hull[hull.size()-2], hull.back(), p) <= 0)
            hull.pop_back();
        hull.push_back(p);
    }
    hull.pop_back();
    double scale = 1;
    for (const auto& p : hull) scale = std::max(scale, glm::dot(p, p));
    const double tolerance = 64 * std::numeric_limits<double>::epsilon() * scale;
    for (std::size_t i = 0; i < hull.size(); ++i)
        if (cross(hull[i], hull[(i+1)%hull.size()], {0,0}) < -tolerance) return false;
    return true;
}

struct FluidWetColumnFootprint {
    std::int64_t x, z;
    glm::dvec2 center;
    bool wet = false;
    bool observedWet = false;
    bool resolvedAir = false; // internal vertical void, distinct from an ordinary free surface
};

// Columns arrive in sorted grid order. Loads require a connected wet patch:
// two disconnected reservoirs must not support an object in their dry gap.
// Interpolation within one patch may still span displaced-solid holes.
inline bool FluidWetColumnsContainOrigin(const std::vector<FluidWetColumnFootprint>& columns,
                                        double width, bool connected) {
    if (columns.empty()) return false;
    std::vector<std::size_t> parent(columns.size());
    std::iota(parent.begin(), parent.end(), 0);
    const auto find = [&](std::size_t i) {
        while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; }
        return i;
    };
    if (connected) {
        for (std::size_t i = 0; i < columns.size(); ++i) {
            for (int dx : {-1,0,1}) for (int dz : {-1,0,1}) {
                if (!dx && !dz) continue;
                const auto x = columns[i].x + dx, z = columns[i].z + dz;
                const auto it = std::lower_bound(columns.begin(), columns.end(), std::make_pair(x,z),
                    [](const auto& a, const auto& b) {return a.x < b.first || (a.x == b.first && a.z < b.second);});
                if (it != columns.end() && it->x == x && it->z == z) {
                    auto a = find(i), b = find(std::size_t(it-columns.begin()));
                    if (a != b) parent[std::max(a,b)] = std::min(a,b);
                }
            }
        }
    }
    std::vector<std::vector<glm::dvec2>> patches(connected ? columns.size() : 1);
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (!columns[i].wet) continue;
        auto& patch = patches[connected ? find(i) : 0];
        for (double dx : {-width*.5,width*.5})
            for (double dz : {-width*.5,width*.5}) patch.push_back(columns[i].center + glm::dvec2(dx,dz));
    }
    for (auto& patch : patches) if (FluidWetFootprintContainsOrigin(patch)) return true;
    return false;
}

// A convex pool envelope is only an outer rejection boundary. A missing column
// must be reached through the queried solid's actual connected component, and
// end at represented water that touches that solid. Cell adjacency alone must
// not bridge a dry gap or borrow support from a separate compound child.
// A closer missing exterior column is dry evidence, unless actual particle
// footprints overlap it. Binning alone cannot prove empty space. Existing lower
// columns already vote dry in the vertical mean and are not counted again.
// covered() limits traversal to the sample's solid component. notEmpty() excludes
// solid/particle-occupied cells from dry evidence. wetContact() requires actual
// particle-footprint contact, not contact with a rounded bin centre. A partial
// exterior footprint is insufficient proof for a reconstructed cap: its caller
// disables allowDirectRoot to require actual particle/selected-solid contact.
template<class Covered, class NotEmpty, class WetContact>
bool FluidWetSupportThroughSolid(const std::vector<FluidWetColumnFootprint>& columns,
                                 double width, glm::dvec2 origin, Covered covered, NotEmpty notEmpty, WetContact wetContact, bool allowDirectRoot = true) {
    using Cell = std::pair<std::int64_t, std::int64_t>;
    const Cell root{static_cast<std::int64_t>(std::floor(-origin.x / width + .5)),
                    static_cast<std::int64_t>(std::floor(-origin.y / width + .5))};
    const auto column = [&](Cell cell) -> const FluidWetColumnFootprint* {
        auto it = std::lower_bound(columns.begin(), columns.end(), cell,
            [](const auto& a, Cell b) { return a.x < b.first || (a.x == b.first && a.z < b.second); });
        return it != columns.end() && it->x == cell.first && it->z == cell.second ? &*it : nullptr;
    };
    const auto* initial = column(root);
    if (allowDirectRoot && initial && initial->wet) return true;
    if (!covered(root.first, root.second)) return false;
    if (initial && initial->observedWet && wetContact(root.first, root.second)) return true;
    const auto distance2 = [&](Cell cell) {
        const auto p = origin + width * glm::dvec2(cell.first, cell.second);
        const auto gap = glm::max(glm::abs(p) - glm::dvec2(width * .5), glm::dvec2(0));
        return glm::dot(gap, gap);
    };
    double wetDistance2 = std::numeric_limits<double>::infinity();
    for (const auto& c : columns) if (c.observedWet)
        wetDistance2 = std::min(wetDistance2, distance2({c.x, c.z}));
    const double rounding = 64 * std::numeric_limits<double>::epsilon() * std::max(1., wetDistance2);
    std::set<Cell> checked;
    for (const auto& c : columns) {
        for (int dx : {-1, 0, 1}) for (int dz : {-1, 0, 1}) {
            const Cell air{c.x + dx, c.z + dz};
            if (distance2(air) + rounding >= wetDistance2 || !checked.insert(air).second) continue;
            const auto* present = column(air);
            // A lower free surface already votes dry in the occupancy mean.
            // A missing column or a resolved internal void at this height can
            // provide closer air evidence. Do not apply that rule to ordinary
            // lower surfaces, which would double-count their dry contribution.
            if ((!present || present->resolvedAir) && !notEmpty(air.first, air.second)) return false;
        }
    }

    std::vector<Cell> pending{root};
    std::set<Cell> visited{root};
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const auto [x, z] = pending[i];
        for (int dx : {-1, 0, 1}) for (int dz : {-1, 0, 1}) {
            if (!dx && !dz) continue;
            const Cell next{x + dx, z + dz};
            if (!visited.insert(next).second) continue;
            const auto* observed = column(next);
            if (observed && observed->observedWet && wetContact(next.first, next.second)) return true;
            if (covered(next.first, next.second)) pending.push_back(next);
        }
    }
    return false;
}
