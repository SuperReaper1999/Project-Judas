#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

// The local hydrostatic approximation may interpolate across displaced-body
// holes, but it must not extrapolate wetness outside the observed lateral
// water envelope. Column footprints have the represented particle spacing;
// the much larger motion-donor search radius does not enlarge this envelope.
// This is a bounded convex envelope, not a multiphase/concave shoreline model.
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
