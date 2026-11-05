#include "CutRectangularOwners.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace judas::volume_grid {

bool BuildCutRectangularOwners(const CutRegionChart& source,
                               const CutRegionGeometry& geometry,
                               double minimumFraction, CutRectangularOwners& output,
                               std::string& error) {
    const auto n = source.regions.size();
    if (n == 0 || geometry.startCapacity.size() != n || geometry.endCapacity.size() != n ||
        !(minimumFraction > 0. && minimumFraction <= 1.) ||
        (geometry.motion && geometry.motion->translation != GridVec{})) {
        error = "invalid rectangular-owner geometry or threshold";
        return false;
    }
    auto regions = source.regions;
    std::vector<std::size_t> parent(n), order(n);
    std::iota(parent.begin(), parent.end(), 0);
    std::iota(order.begin(), order.end(), 0);
    const auto root = [&](std::size_t i) {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    };
    std::vector<double> volume = geometry.startCapacity;
    for (std::size_t i = 0; i < n; ++i) {
        if (!(volume[i] >= 0. && std::isfinite(volume[i])) || volume[i] != geometry.endCapacity[i]) {
            error = "rectangular owners require static finite capacities";
            return false;
        }
    }
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
        return volume[a] < volume[b];
    });
    std::vector<std::vector<std::size_t>> neighbors(n);
    for (const auto& face : geometry.faces) {
        if (face.left >= n || face.right >= n || face.axis < 0 || face.axis > 2) {
            error = "invalid rectangular-owner face";
            return false;
        }
        const int a = (face.axis + 1) % 3, b = (face.axis + 2) % 3;
        const double area = (face.hi[a] - face.lo[a]) * (face.hi[b] - face.lo[b]);
        if (face.startArea == area && face.endArea == area && area > 0.) {
            neighbors[face.left].push_back(face.right);
            neighbors[face.right].push_back(face.left);
        }
    }
    const double h = source.layout.h, target = minimumFraction * h * h * h;
    for (auto index : order) {
        const auto i = root(index);
        if (volume[i] >= target || volume[i] == 0.)
            continue;
        for (auto neighbor : neighbors[index]) {
            const auto j = root(neighbor);
            if (i == j || volume[j] == 0.)
                continue;
            int adjoining = -1;
            bool rectangular = true;
            for (int axis = 0; axis < 3; ++axis) {
                const auto& a = regions[i];
                const auto& b = regions[j];
                if (a.lo[axis] == b.lo[axis] && a.hi[axis] == b.hi[axis])
                    continue;
                if (adjoining != -1 || !(a.lo[axis] == b.hi[axis] || a.hi[axis] == b.lo[axis])) {
                    rectangular = false;
                    break;
                }
                adjoining = axis;
            }
            if (!rectangular || adjoining == -1)
                continue;
            const double lo = std::min(regions[i].lo[adjoining], regions[j].lo[adjoining]);
            const double hi = std::max(regions[i].hi[adjoining], regions[j].hi[adjoining]);
            if (hi - lo > 2. * h)
                continue;
            regions[j].lo[adjoining] = lo;
            regions[j].hi[adjoining] = hi;
            volume[j] += volume[i];
            parent[i] = j;
            break;
        }
    }
    CutRectangularOwners next;
    next.chart.layout = source.layout;
    // This owner map is qualified only at the supplied frozen pose.
    // Retain that solid for clipping, but do not advertise the source's
    // travel envelope as a qualified moving-owner remap.
    next.chart.moving = geometry.motion;
    std::vector<std::size_t> compact(n, n);
    next.owner.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        const auto r = root(i);
        if (compact[r] == n) {
            compact[r] = next.chart.regions.size();
            next.chart.regions.push_back(regions[r]);
        }
        next.owner[i] = compact[r];
    }
    // Keep every external sub-face and patch footprint; only internal
    // owner boundaries disappear. All moving indices are rebuilt below.
    for (auto face : geometry.faces) {
        face.left = next.owner[face.left];
        face.right = next.owner[face.right];
        if (face.left == face.right)
            continue;
        const auto& a = next.chart.regions[face.left];
        const auto& b = next.chart.regions[face.right];
        face.length = .5 * ((a.hi[face.axis] - a.lo[face.axis]) +
                            (b.hi[face.axis] - b.lo[face.axis]));
        next.chart.faces.push_back(face);
    }
    if (next.chart.moving) {
        next.chart.movingRegions.resize(next.chart.regions.size());
        std::iota(next.chart.movingRegions.begin(), next.chart.movingRegions.end(), 0);
        next.chart.movingFaces.resize(next.chart.faces.size());
        std::iota(next.chart.movingFaces.begin(), next.chart.movingFaces.end(), 0);
    }
    CutRegionGeometry audit;
    if (!BuildCutRegionGeometry(next.chart, {}, audit, error))
        return false;
    // Preserve the original capacity ledger. Re-evaluating the product of
    // the union's side lengths can differ by an ULP from summing its cuts.
    // This is not permission to alter a geometric capacity materially.
    std::vector<double> capacities(next.chart.regions.size());
    for (std::size_t i = 0; i < n; ++i)
        capacities[next.owner[i]] += geometry.startCapacity[i];
    for (std::size_t i = 0; i < capacities.size(); ++i) {
        const double bound = 128. * std::numeric_limits<double>::epsilon() * capacities[i];
        if (!(std::abs(audit.startCapacity[i] - capacities[i]) <= bound)) {
            error = "rectangular union changed physical capacity";
            return false;
        }
    }
    audit.startCapacity = capacities;
    audit.endCapacity = std::move(capacities);
    next.geometry = std::move(audit);
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
