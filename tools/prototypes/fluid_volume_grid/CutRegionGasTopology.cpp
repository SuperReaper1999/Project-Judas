#include "CutRegionGasTopology.h"

#include <numeric>

namespace judas::volume_grid {
namespace {

bool Fail(std::string& error, const char* message) {
    error = message;
    return false;
}

bool ValidVent(const CutRegionChart& chart, const CutRegionGasVent& vent) {
    if (vent.axis < 0 || vent.axis > 2 || (vent.sign != -1 && vent.sign != 1))
        return false;
    for (int axis = 0; axis < 3; ++axis) {
        const double chartEnd = chart.layout.cells[axis] * chart.layout.h;
        if (axis == vent.axis) {
            const double side = vent.sign > 0 ? chartEnd : 0.;
            if (vent.lo[axis] != side || vent.hi[axis] != side)
                return false;
        } else if (!(0. <= vent.lo[axis] && vent.lo[axis] < vent.hi[axis] && vent.hi[axis] <= chartEnd)) {
            return false;
        }
    }
    return true;
}

bool Overlaps(const CutRegionGasVent& a, const CutRegionGasVent& b) {
    if (a.axis != b.axis || a.sign != b.sign)
        return false;
    for (int axis = 0; axis < 3; ++axis)
        if (axis != a.axis && std::min(a.hi[axis], b.hi[axis]) <= std::max(a.lo[axis], b.lo[axis]))
            return false;
    return true;
}

bool DividesDrySlab(const CutRegion& region, const CutRegionWaterPlane& water, const BoxMotion& solid) {
    GridVec gapBelow, gapAbove;
    moving_geometry_detail::Events events;
    for (int axis = 0; axis < 3; ++axis) {
        gapBelow[axis] = axis == water.orientation.axis && !water.orientation.wetAbove
            ? -water.RelativeTo(solid.lo[axis]) : solid.lo[axis] - region.lo[axis];
        gapAbove[axis] = axis == water.orientation.axis && water.orientation.wetAbove
            ? water.RelativeTo(solid.hi[axis]) : region.hi[axis] - solid.hi[axis];
        events.Cross(gapBelow[axis], solid.translation[axis], 0.);
        events.Cross(gapAbove[axis], -solid.translation[axis], 0.);
    }
    const auto divides = [&](double t) {
        GridVec below, above;
        for (int axis = 0; axis < 3; ++axis) {
            below[axis] = std::fma(solid.translation[axis], t, gapBelow[axis]);
            above[axis] = std::fma(-solid.translation[axis], t, gapAbove[axis]);
        }
        for (int axis = 0; axis < 3; ++axis) {
            const int a = (axis + 1) % 3, b = (axis + 2) % 3;
            if (below[axis] > 0. && above[axis] > 0. && below[a] <= 0. &&
                above[a] <= 0. && below[b] <= 0. && above[b] <= 0.)
                return true;
        }
        return false;
    };
    if (divides(0.))
        return true;
    if (solid.translation == GridVec{})
        return false;
    events.Sort();
    for (std::size_t i = 1; i < events.count; ++i) {
        const double lo = events.times[i - 1], hi = events.times[i];
        if (divides(hi) || (lo < hi && divides(lo + .5 * (hi - lo))))
            return true;
    }
    return false;
}

class GasGraph {
public:
    explicit GasGraph(std::size_t count) : parents_(count), rank_(count) {
        std::iota(parents_.begin(), parents_.end(), 0);
    }

    std::size_t Root(std::size_t node) {
        while (parents_[node] != node) {
            parents_[node] = parents_[parents_[node]];
            node = parents_[node];
        }
        return node;
    }

    void Join(std::size_t a, std::size_t b) {
        a = Root(a);
        b = Root(b);
        if (a == b)
            return;
        if (rank_[a] < rank_[b])
            std::swap(a, b);
        parents_[b] = a;
        if (rank_[a] == rank_[b])
            ++rank_[a];
    }

private:
    std::vector<std::size_t> parents_;
    std::vector<unsigned char> rank_;
};

} // namespace

CutRegionGasVent FullCutRegionGasVent(const CutRegionChart& chart, int axis, int sign) {
    if (axis < 0 || axis > 2 || (sign != -1 && sign != 1))
        return {};
    CutRegionGasVent vent{axis, sign};
    for (int d = 0; d < 3; ++d)
        vent.hi[d] = chart.layout.cells[d] * chart.layout.h;
    if (sign < 0)
        vent.hi[axis] = 0.;
    else
        vent.lo[axis] = vent.hi[axis];
    return vent;
}

CutRegionAperture MeasureCutRegionGasVent(const CutRegion& region, const std::optional<BoxMotion>& solid,
                       const CutRegionGasVent& vent, const CutRegionWaterPlane& water) {
    const int normal = vent.axis;
    if (normal < 0 || normal > 2 || (vent.sign != -1 && vent.sign != 1))
        return {};
    const double boundary = vent.lo[normal];
    const double side = vent.sign > 0 ? region.hi[normal] : region.lo[normal];
    if (side != boundary)
        return {};
    CutRegionFace face{};
    face.axis = normal;
    for (int axis = 0; axis < 3; ++axis) {
        face.lo[axis] = axis == normal ? boundary : std::max(region.lo[axis], vent.lo[axis]);
        face.hi[axis] = axis == normal ? boundary : std::min(region.hi[axis], vent.hi[axis]);
    }
    const auto opening = MeasureCutRegionAperture(face, solid, face.lo, face.hi);
    face.startArea = opening.start;
    face.meanArea = opening.mean;
    if (face.meanArea == 0.)
        return {};
    const auto phase = MeasureCutRegionFacePhase(face, solid, water, water);
    return phase.dry[vent.sign > 0 ? 0 : 1];
}

bool BuildCutRegionGasTopology(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                              const std::vector<CutRegionWaterPlane>& water,
                              const std::vector<CutRegionGasVent>& vents,
                              CutRegionGasTopology& output, std::string& error) {
    const auto count = chart.regions.size();
    if (count == 0 || geometry.startCapacity.size() != count || water.size() != count ||
        geometry.faces.size() != chart.faces.size() || chart.layout.CheckedCellCount() == 0)
        return Fail(error, "invalid cut-region gas topology dimensions");
    for (std::size_t i = 0; i < vents.size(); ++i) {
        if (!ValidVent(chart, vents[i]))
            return Fail(error, "gas vent is not a valid chart boundary rectangle");
        for (std::size_t j = 0; j < i; ++j)
            if (Overlaps(vents[i], vents[j]))
                return Fail(error, "gas vent rectangles overlap");
    }

    // Connectivity owns start-of-step geometry. The interval average would
    // incorrectly join a chamber before a translating solid uncovers a vent.
    auto solid = geometry.motion;
    if (solid)
        solid->translation = {};
    CutRegionGasTopology next;
    next.regionOwner.assign(count, NoCutRegionGas);
    next.regionVolume.resize(count);
    next.ventArea.resize(count);
    next.sharedDryArea.resize(geometry.faces.size());
    GasGraph graph(count + 1);
    const auto exterior = count;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& region = chart.regions[i];
        const auto& plane = water[i];
        const auto axis = plane.orientation.axis;
        if (axis < 0 || axis > 2 || !std::isfinite(plane.coordinate) || !std::isfinite(plane.remainder) ||
            !(plane.RelativeTo(region.lo[axis]) >= 0. && plane.RelativeTo(region.hi[axis]) <= 0.))
            return Fail(error, "gas interface is outside its cut region");
        const auto [wet, dry] = MeasureCutRegionPhaseVolumes(region, solid, region.lo, region.hi, plane);
        const double capacity = geometry.startCapacity[i];
        const double tolerance = 128. * std::numeric_limits<double>::epsilon() * capacity;
        if (!(capacity >= 0. && std::isfinite(capacity) && wet >= 0. && dry >= 0. &&
              std::abs(wet + dry - capacity) <= tolerance))
            return Fail(error, "gas and water volumes do not close their cut region");
        if (dry == 0.)
            continue;
        if (geometry.motion && DividesDrySlab(region, plane, *geometry.motion))
            return Fail(error, "moving solid divides dry material inside one cut region");
        next.regionVolume[i] = dry;
        for (const auto& vent : vents)
            next.ventArea[i] += MeasureCutRegionGasVent(region, solid, vent, plane).start;
        if (next.ventArea[i] > 0.)
            graph.Join(i, exterior);
    }
    for (std::size_t i = 0; i < geometry.faces.size(); ++i) {
        const auto& face = geometry.faces[i];
        if (face.left >= count || face.right >= count || face.left == face.right || face.axis < 0 ||
            face.axis > 2 || !(face.startArea >= 0.) || !std::isfinite(face.startArea))
            return Fail(error, "invalid shared gas aperture");
        if (face.startArea == 0. || next.regionVolume[face.left] == 0. || next.regionVolume[face.right] == 0.)
            continue;
        const double area = MeasureCutRegionSharedPhase(face, solid, water[face.left], water[face.right],
                                                        false, false).start;
        if (!(area >= 0. && area <= face.startArea * (1. + 128. * std::numeric_limits<double>::epsilon())))
            return Fail(error, "shared gas area exceeds its physical opening");
        next.sharedDryArea[i] = area;
        if (area > 0.)
            graph.Join(face.left, face.right);
    }

    std::vector<std::size_t> ownerForRoot(count + 1, NoCutRegionGas);
    ownerForRoot[graph.Root(exterior)] = ExteriorCutRegionGas;
    next.components.emplace_back();
    std::vector<long double> totals(1);
    for (std::size_t i = 0; i < count; ++i) {
        if (next.regionVolume[i] == 0.)
            continue;
        auto& owner = ownerForRoot[graph.Root(i)];
        if (owner == NoCutRegionGas) {
            owner = next.components.size();
            next.components.emplace_back();
            totals.push_back(0.);
        }
        next.regionOwner[i] = owner;
        next.components[owner].regions.push_back(i);
        totals[owner] += next.regionVolume[i];
    }
    for (std::size_t i = 0; i < next.components.size(); ++i)
        next.components[i].volume = static_cast<double>(totals[i]);
    output = std::move(next);
    error.clear();
    return true;
}

} // namespace judas::volume_grid
