#pragma once
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

namespace FluidHydrostaticDetail {
// A compact reconstruction of particle cubes in the query's gravity frame.
// Short pair hulls fill unresolved sampling holes only within both particles'
// support balls. This is approximate wet geometry, not a second mass ledger.
struct LocalWetCell {
    glm::dvec3 center; // tangent/up/bitangent coordinates relative to the query
    double spacing;
    double rounding;
};
using LocalWetInterval = std::pair<double, double>;

inline bool LocalWetCellMayTouchQuery(const LocalWetCell& cell, double halfHeight) {
    const double half = cell.spacing * .5;
    const bool raw = std::abs(cell.center.x) <= half + cell.rounding &&
                     std::abs(cell.center.z) <= half + cell.rounding &&
                     cell.center.y - half <= halfHeight &&
                     cell.center.y + half >= -halfHeight;
    const double dy = std::max(0., std::abs(cell.center.y) - halfHeight);
    return raw || cell.center.x * cell.center.x + cell.center.z * cell.center.z +
                  dy * dy <= cell.spacing * cell.spacing;
}

inline std::vector<LocalWetInterval> LocalWetIntervals(
        const std::vector<LocalWetCell>& cells, double halfHeight) {
    std::vector<LocalWetInterval> intervals;
    const auto append = [&](double lo, double hi) {
        lo = std::max(lo, -halfHeight);
        hi = std::min(hi, halfHeight);
        if (lo <= hi) intervals.emplace_back(lo, hi);
    };
    for (const auto& cell : cells) {
        const double half = cell.spacing * .5;
        if (std::abs(cell.center.x) <= half + cell.rounding &&
            std::abs(cell.center.z) <= half + cell.rounding)
            append(cell.center.y - half, cell.center.y + half);
    }
    const auto merge = [&] {
        std::sort(intervals.begin(), intervals.end());
        std::size_t count = 0;
        for (const auto& span : intervals) {
            if (count && span.first <= intervals[count - 1].second)
                intervals[count - 1].second = std::max(intervals[count - 1].second, span.second);
            else intervals[count++] = span;
        }
        intervals.resize(count);
    };
    merge();
    if (intervals.size() == 1 && intervals[0].first <= -halfHeight &&
        intervals[0].second >= halfHeight) return intervals;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const auto& a = cells[i];
        for (std::size_t j = i + 1; j < cells.size(); ++j) {
            const auto& b = cells[j];
            const double ha = a.spacing * .5, hb = b.spacing * .5;
            const double fine = std::min(a.spacing, b.spacing);
            const double limit = ha + hb + fine;
            const double error = std::max(a.rounding, b.rounding);
            const auto delta = b.center - a.center;
            if (glm::dot(delta, delta) > (limit + error) * (limit + error)) continue;
            double low = 0, high = 1;
            const double dh = hb - ha;
            // c(t) +/- h(t) on both horizontal axes must contain the query ray.
            const auto constrain = [&](double value, double slope) {
                if (slope == 0) return value <= error;
                const double boundary = (error - value) / slope;
                if (slope > 0) high = std::min(high, boundary);
                else low = std::max(low, boundary);
                return low <= high;
            };
            bool supported = true;
            for (int axis : {0, 2}) {
                supported = supported && constrain(a.center[axis] - ha, delta[axis] - dh);
                supported = supported && constrain(-a.center[axis] - ha, -delta[axis] - dh);
            }
            if (!supported) continue;
            const auto bottom = [&](double t) { return a.center.y - ha + t * (delta.y - dh); };
            const auto top = [&](double t) { return a.center.y + ha + t * (delta.y + dh); };
            double lo = std::min(bottom(low), bottom(high));
            double hi = std::max(top(low), top(high));
            // The compact support lens shrinks to zero as a pair separates;
            // a hard edge would add/remove a finite water prism at once.
            for (const auto* cell : {&a, &b}) {
                const double radius = cell->spacing * .5 + fine * .5;
                const double horizontal2 = cell->center.x * cell->center.x + cell->center.z * cell->center.z;
                // A numerically tangent support ball proves no positive
                // ray length. Taking sqrt of a rounded tiny positive
                // radicand would magnify frame error into false wetness.
                const double interior = std::max(0., radius - cell->rounding);
                if (horizontal2 >= interior * interior) { supported = false; break; }
                const double reach = std::sqrt(interior * interior - horizontal2);
                lo = std::max(lo, cell->center.y - reach);
                hi = std::min(hi, cell->center.y + reach);
            }
            if (supported && hi > lo) append(lo, hi);
        }
    }
    merge();
    return intervals;
}
} // namespace FluidHydrostaticDetail
