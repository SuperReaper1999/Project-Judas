#include "CutRegionGeometry.h"

namespace judas::volume_grid {
namespace {

using moving_geometry_detail::AddTangentEvents;
using moving_geometry_detail::Events;
using moving_geometry_detail::Index;
using moving_geometry_detail::PatchSample;

constexpr std::size_t maxRegions = 262'144;
constexpr std::size_t maxLocalPieces = 4'096;
constexpr std::size_t maxBinReferences = 4'000'000;
constexpr std::size_t maxOccupancyChecks = 32'000'000;
constexpr std::size_t maxPairChecks = 32'000'000;
constexpr std::size_t maxFaces = 1'500'000;

template<class Function>
bool ForCells(const Index& first, const Index& stop, const Function& function) {
    for (int z = first[2]; z < stop[2]; ++z)
        for (int y = first[1]; y < stop[1]; ++y)
            for (int x = first[0]; x < stop[0]; ++x)
                if (!function(Index{x, y, z}))
                    return false;
    return true;
}

std::size_t Cell(const GridLayout& layout, const Index& i) {
    return layout.Cell(i[0], i[1], i[2]);
}

bool Fail(std::string& error, const char* description) {
    error = description;
    return false;
}

bool Intersects(const GridVec& aLo, const GridVec& aHi, const GridVec& bLo, const GridVec& bHi,
                bool touching = false) {
    for (int axis = 0; axis < 3; ++axis) {
        const double lower = std::max(aLo[axis], bLo[axis]);
        const double upper = std::min(aHi[axis], bHi[axis]);
        if (touching ? lower > upper : lower >= upper)
            return false;
    }
    return true;
}

bool ValidBox(const GridLayout& layout, const BoxMotion& box) {
    if (box.translation != GridVec{})
        return false;
    for (int axis = 0; axis < 3; ++axis) {
        if (!(0. <= box.lo[axis] && box.lo[axis] < box.hi[axis] &&
              box.hi[axis] <= layout.cells[axis] * layout.h))
            return false;
    }
    return true;
}

bool Contains(const BoxMotion& box, const CutRegion& region) {
    for (int axis = 0; axis < 3; ++axis)
        if (box.lo[axis] > region.lo[axis] || box.hi[axis] < region.hi[axis])
            return false;
    return true;
}

double BoxVolume(const GridVec& lo, const GridVec& hi) {
    return (hi[0] - lo[0]) * (hi[1] - lo[1]) * (hi[2] - lo[2]);
}

struct IntervalMeasures {
    double covered, free;
};

IntervalMeasures ClipInterval(double lower, double upper, const BoxMotion& box,
                              int axis, double time) {
    // Work relative to the clipped interval so a narrow opening remains
    // measurable even when its absolute position is far from the chart origin.
    const double span = upper - lower;
    const double loFromLower = std::fma(box.translation[axis], time, box.lo[axis] - lower);
    const double hiFromLower = std::fma(box.translation[axis], time, box.hi[axis] - lower);
    const double loFromUpper = std::fma(box.translation[axis], time, box.lo[axis] - upper);
    const double hiFromUpper = std::fma(box.translation[axis], time, box.hi[axis] - upper);
    if (loFromUpper >= 0. || hiFromLower <= 0.)
        return {0., span};
    const double free = std::max(0., loFromLower) + std::max(0., -hiFromUpper);
    if (loFromLower <= 0.)
        return {hiFromUpper >= 0. ? span : hiFromLower, free};
    return {hiFromUpper >= 0. ? -loFromUpper : box.hi[axis] - box.lo[axis], free};
}

bool CoversFace(const BoxMotion& box, const CutRegionFace& face, double time) {
    const int axis = face.axis;
    const double plane = face.lo[axis];
    return std::fma(box.translation[axis], time, box.lo[axis] - plane) <= 0. &&
           std::fma(box.translation[axis], time, box.hi[axis] - plane) >= 0.;
}

struct FaceMeasure {
    double start = 0., mean = 0., blockedMean = 0.;
};

IntervalMeasures FaceClip(const CutRegionFace& face, const BoxMotion& box, const GridVec& lo,
                          const GridVec& hi, double time) {
    const int a = (face.axis + 1) % 3, b = (face.axis + 2) % 3;
    const auto x = ClipInterval(lo[a], hi[a], box, a, time);
    const auto y = ClipInterval(lo[b], hi[b], box, b, time);
    if (x.covered == 0. || y.covered == 0.)
        return {0., (hi[a] - lo[a]) * (hi[b] - lo[b])};
    return {x.covered * y.covered, x.free * (hi[b] - lo[b]) + x.covered * y.free};
}

FaceMeasure MeasureFace(const CutRegionFace& face, const std::optional<BoxMotion>& motion,
                        const GridVec& clipLo, const GridVec& clipHi, double begin, double end) {
    if (!(0. <= begin && begin < end && end <= 1.))
        return {};
    const int axis = face.axis, a = (axis + 1) % 3, b = (axis + 2) % 3;
    auto lo = face.lo, hi = face.hi;
    for (const int tangent : {a, b}) {
        lo[tangent] = std::max(lo[tangent], clipLo[tangent]);
        hi[tangent] = std::min(hi[tangent], clipHi[tangent]);
        if (!(lo[tangent] < hi[tangent]))
            return {};
    }
    const double area = (hi[a] - lo[a]) * (hi[b] - lo[b]);
    if (!motion)
        return {area, area, 0.};

    const auto& box = *motion;
    for (int d = 0; d < 3; ++d) {
        const auto below = [&](double time) {
            const double upper = std::fma(box.translation[d], time, box.hi[d] - lo[d]);
            return d == axis ? upper < 0. : upper <= 0.;
        };
        const auto above = [&](double time) {
            const double lower = std::fma(box.translation[d], time, box.lo[d] - hi[d]);
            return d == axis ? lower > 0. : lower >= 0.;
        };
        if ((below(begin) && below(end)) || (above(begin) && above(end)))
            return {area, area, 0.};
    }
    const auto measures = [&](double time) {
        return FaceClip(face, box, lo, hi, time);
    };
    if (box.translation == GridVec{}) {
        if (!CoversFace(box, face, begin))
            return {area, area, 0.};
        const auto clip = measures(begin);
        return {clip.free, clip.free, clip.covered};
    }
    FaceMeasure result;
    result.start = CoversFace(box, face, begin) ? measures(begin).free : area;
    Events events;
    events.Cross(box.lo[axis], box.translation[axis], face.lo[axis]);
    events.Cross(box.hi[axis], box.translation[axis], face.lo[axis]);
    for (const int tangent : {a, b})
        AddTangentEvents(events, box, tangent, lo[tangent], hi[tangent]);
    events.Sort();
    for (std::size_t i = 1; i < events.count; ++i) {
        const double t0 = std::max(begin, events.times[i - 1]);
        const double t1 = std::min(end, events.times[i]);
        if (t0 >= t1)
            continue;
        const double middle = t0 + .5 * (t1 - t0);
        const double duration = (t1 - t0) / (end - begin);
        if (!CoversFace(box, face, middle)) {
            result.mean += duration * area;
            continue;
        }
        const auto m0 = measures(t0), mm = measures(middle), m1 = measures(t1);
        // Each tangent interval is affine on these events. Integrate the
        // open and solid rectangles separately to retain arbitrarily small
        // positive support when the complementary part covers almost all.
        result.mean += (duration / 6.) * (m0.free + 4. * mm.free + m1.free);
        result.blockedMean += (duration / 6.) * (m0.covered + 4. * mm.covered + m1.covered);
    }
    return result;
}

PatchSample SampleWall(const CutRegion& region, const BoxMotion& box, int axis, double edge,
                       double time) {
    PatchSample result{1., {}};
    result.centroid[axis] = std::fma(box.translation[axis], time, edge);
    for (const int tangent : {(axis + 1) % 3, (axis + 2) % 3}) {
        const double base = region.lo[tangent], span = region.hi[tangent] - base;
        const double lo = std::max(0., std::fma(box.translation[tangent], time, box.lo[tangent] - base));
        const double hi = std::min(span, std::fma(box.translation[tangent], time, box.hi[tangent] - base));
        result.area *= std::max(0., hi - lo);
        result.centroid[tangent] = base + (lo + .5 * (hi - lo));
    }
    return result;
}

PatchSample AverageWall(const CutRegion& region, const BoxMotion& box, int axis, int sign) {
    const double edge = sign < 0 ? box.lo[axis] : box.hi[axis];
    const double lower = region.lo[axis], upper = region.hi[axis];
    Events events;
    events.Cross(edge, box.translation[axis], lower);
    events.Cross(edge, box.translation[axis], upper);
    for (const int tangent : {(axis + 1) % 3, (axis + 2) % 3})
        AddTangentEvents(events, box, tangent, region.lo[tangent], region.hi[tangent]);
    events.Sort();

    PatchSample result;
    for (std::size_t i = 1; i < events.count; ++i) {
        const double t0 = events.times[i - 1], t1 = events.times[i];
        const double middle = t0 + .5 * (t1 - t0);
        const double fromLower = std::fma(box.translation[axis], middle, edge - lower);
        const double fromUpper = std::fma(box.translation[axis], middle, edge - upper);
        // At a coincident wall the fluid-facing region owns the patch.
        if (t0 == t1 || fromLower < 0. || fromUpper > 0. ||
            (fromLower == 0. && sign < 0) || (fromUpper == 0. && sign > 0))
            continue;
        const auto m0 = SampleWall(region, box, axis, edge, t0);
        const auto mm = SampleWall(region, box, axis, edge, middle);
        const auto m1 = SampleWall(region, box, axis, edge, t1);
        const double factor = (t1 - t0) / 6.;
        result.area += factor * (m0.area + 4. * mm.area + m1.area);
        for (int d = 0; d < 3; ++d)
            result.centroid[d] += factor * (m0.area * m0.centroid[d] +
                4. * mm.area * mm.centroid[d] + m1.area * m1.centroid[d]);
    }
    if (result.area > 0.)
        for (double& moment : result.centroid)
            moment /= result.area;
    return result;
}

bool CreatesInteriorDivider(const CutRegion& region, const BoxMotion& box) {
    for (int axis = 0; axis < 3; ++axis) {
        const int a = (axis + 1) % 3, b = (axis + 2) % 3;
        if (box.hi[axis] - box.lo[axis] >= region.hi[axis] - region.lo[axis] ||
            box.hi[a] - box.lo[a] < region.hi[a] - region.lo[a] ||
            box.hi[b] - box.lo[b] < region.hi[b] - region.lo[b])
            continue;
        const auto divides = [&](double time) {
            const auto fromLower = [&](int d) {
                return std::fma(box.translation[d], time, box.lo[d] - region.lo[d]);
            };
            const auto fromUpper = [&](int d) {
                return std::fma(box.translation[d], time, box.hi[d] - region.hi[d]);
            };
            return fromLower(axis) > 0. && fromUpper(axis) < 0. &&
                fromLower(a) <= 0. && fromUpper(a) >= 0. &&
                fromLower(b) <= 0. && fromUpper(b) >= 0.;
        };
        Events events;
        for (const int d : {axis, a, b}) {
            events.Cross(box.lo[d], box.translation[d], region.lo[d]);
            events.Cross(box.hi[d], box.translation[d], region.hi[d]);
        }
        events.Sort();
        if (divides(0.))
            return true;
        for (std::size_t i = 1; i < events.count; ++i) {
            const double t0 = events.times[i - 1], t1 = events.times[i];
            if (divides(t1) || (t0 < t1 && divides(t0 + .5 * (t1 - t0))))
                return true;
        }
    }
    return false;
}

bool BakeFaces(CutRegionChart& chart, const std::optional<BoxMotion>& envelope,
               std::string& error) {
    std::size_t pairs = 0;
    const auto link = [&](std::size_t i, std::size_t j, int axis) {
        if (++pairs > maxPairChecks)
            return Fail(error, "cut region chart exceeds its face-pair budget");
        const auto& left = chart.regions[i];
        const auto& right = chart.regions[j];
        if (left.hi[axis] != right.lo[axis])
            return true;
        GridVec lo, hi;
        double area = 1.;
        for (int d = 0; d < 3; ++d) {
            if (d == axis) {
                lo[d] = hi[d] = right.lo[d];
                continue;
            }
            lo[d] = std::max(left.lo[d], right.lo[d]);
            hi[d] = std::min(left.hi[d], right.hi[d]);
            if (!(lo[d] < hi[d]))
                return true;
            area *= hi[d] - lo[d];
        }
        if (chart.faces.size() == maxFaces)
            return Fail(error, "cut region chart exceeds its face budget");
        const double length = .5 * (right.hi[axis] - left.lo[axis]);
        if (!(area > 0. && std::isfinite(area) && length > 0. && std::isfinite(length)))
            return Fail(error, "cut region face is outside representable geometry");
        if (envelope && Intersects(lo, hi, envelope->lo, envelope->hi, true))
            chart.movingFaces.push_back(chart.faces.size());
        chart.faces.push_back({i, j, axis, lo, hi, area, area, area, 0., length});
        return true;
    };
    return ForCells({}, chart.layout.cells, [&](const Index& index) {
        const auto cell = Cell(chart.layout, index);
        const auto begin = chart.offsets[cell], end = chart.offsets[cell + 1];
        for (int axis = 0; axis < 3; ++axis) {
            for (std::size_t i = begin; i < end; ++i)
                for (std::size_t j = i + 1; j < end; ++j)
                    if (!link(i, j, axis))
                        return false;
            if (index[axis] + 1 == chart.layout.cells[axis])
                continue;
            auto next = index;
            ++next[axis];
            const auto other = Cell(chart.layout, next);
            for (std::size_t i = begin; i < end; ++i)
                for (std::size_t j = chart.offsets[other]; j < chart.offsets[other + 1]; ++j)
                    if (!link(i, j, axis))
                        return false;
        }
        return true;
    });
}

} // namespace

bool PrepareCutRegionChart(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                           const std::optional<BoxMotion>& moving, const GridVec& minTravel,
                           const GridVec& maxTravel, CutRegionChart& output, std::string& error) {
    const auto count = layout.CheckedCellCount();
    if (count == 0 || count > maxRegions || stationary.size() > 256)
        return Fail(error, "invalid or oversized cut region chart");
    CutRegionChart chart;
    chart.layout = layout;
    chart.moving = moving;
    std::optional<BoxMotion> envelope;
    if (moving) {
        if (!ValidBox(layout, *moving))
            return Fail(error, "cut region moving box must have a finite initial pose");
        envelope = *moving;
        for (int axis = 0; axis < 3; ++axis) {
            if (!(std::isfinite(minTravel[axis]) && std::isfinite(maxTravel[axis]) &&
                  minTravel[axis] <= maxTravel[axis]))
                return Fail(error, "invalid cut region travel envelope");
            chart.minTravel[axis] = std::min(0., minTravel[axis]);
            chart.maxTravel[axis] = std::max(0., maxTravel[axis]);
            envelope->lo[axis] += chart.minTravel[axis];
            envelope->hi[axis] += chart.maxTravel[axis];
            if (!(envelope->lo[axis] > 0. && envelope->hi[axis] < layout.cells[axis] * layout.h))
                return Fail(error, "cut region moving box must stay inside the chart");
        }
    }

    std::array<std::vector<double>, 3> parentCuts;
    for (int axis = 0; axis < 3; ++axis)
        for (int i = 0; i <= layout.cells[axis]; ++i)
            parentCuts[axis].push_back(i * layout.h);
    std::vector<std::vector<std::size_t>> bins(count);
    std::size_t references = 0;
    for (std::size_t i = 0; i < stationary.size(); ++i) {
        const auto& box = stationary[i];
        if (!ValidBox(layout, box))
            return Fail(error, "cut region stationary union requires finite chart-contained boxes");
        if (envelope && Intersects(envelope->lo, envelope->hi, box.lo, box.hi, true))
            return Fail(error, "cut region moving envelope touches the stationary union");
        Index first, stop;
        for (int axis = 0; axis < 3; ++axis) {
            const auto& knots = parentCuts[axis];
            first[axis] = static_cast<int>(std::upper_bound(knots.begin(), knots.end(), box.lo[axis])
                - knots.begin()) - 1;
            stop[axis] = static_cast<int>(std::lower_bound(knots.begin(), knots.end(), box.hi[axis])
                - knots.begin());
        }
        if (!ForCells(first, stop, [&](const Index& cell) {
            if (++references > maxBinReferences)
                return Fail(error, "cut region chart exceeds its stationary bin budget");
            bins[Cell(layout, cell)].push_back(i);
            return true;
        }))
            return false;
    }

    chart.offsets.reserve(count + 1);
    chart.regions.reserve(count);
    std::size_t occupancyChecks = 0;
    if (!ForCells({}, layout.cells, [&](const Index& cell) {
        const auto parent = Cell(layout, cell);
        chart.offsets.push_back(chart.regions.size());
        CutRegion coarse{{}, {}, parent};
        for (int axis = 0; axis < 3; ++axis) {
            coarse.lo[axis] = parentCuts[axis][cell[axis]];
            coarse.hi[axis] = parentCuts[axis][cell[axis] + 1];
        }
        const auto& bin = bins[parent];
        const bool movingCut = envelope && Intersects(coarse.lo, coarse.hi, envelope->lo, envelope->hi);
        if (bin.empty() && !movingCut) {
            if (chart.regions.size() == maxRegions)
                return Fail(error, "cut region chart exceeds its control-volume budget");
            if (envelope && Intersects(coarse.lo, coarse.hi, envelope->lo, envelope->hi, true))
                chart.movingRegions.push_back(chart.regions.size());
            chart.regions.push_back(coarse);
            return true;
        }
        std::array<std::vector<double>, 3> cuts;
        Index stops;
        std::size_t pieceCount = 1;
        for (int axis = 0; axis < 3; ++axis) {
            auto& coordinates = cuts[axis];
            const double lo = coarse.lo[axis], hi = coarse.hi[axis];
            coordinates = {lo, hi};
            const auto add = [&](double edge) {
                if (lo < edge && edge < hi)
                    coordinates.push_back(edge);
            };
            for (const auto owner : bin) {
                add(stationary[owner].lo[axis]);
                add(stationary[owner].hi[axis]);
            }
            if (movingCut) {
                for (double wall : {moving->lo[axis], moving->hi[axis]})
                    for (double d : {0., minTravel[axis], maxTravel[axis]})
                        add(wall + d);
            }
            std::sort(coordinates.begin(), coordinates.end());
            coordinates.erase(std::unique(coordinates.begin(), coordinates.end()), coordinates.end());
            stops[axis] = static_cast<int>(coordinates.size()) - 1;
            if (std::size_t(stops[axis]) > maxLocalPieces / pieceCount)
                return Fail(error, "cut region cell exceeds its local partition budget");
            pieceCount *= std::size_t(stops[axis]);
        }
        if (pieceCount * bin.size() > maxOccupancyChecks - occupancyChecks)
            return Fail(error, "cut region chart exceeds its occupancy budget");
        occupancyChecks += pieceCount * bin.size();
        return ForCells({}, stops, [&](const Index& local) {
            CutRegion piece{{}, {}, parent};
            for (int axis = 0; axis < 3; ++axis) {
                piece.lo[axis] = cuts[axis][local[axis]];
                piece.hi[axis] = cuts[axis][local[axis] + 1];
            }
            if (std::any_of(bin.begin(), bin.end(), [&](std::size_t owner) {
                return Contains(stationary[owner], piece);
            }))
                return true;
            const double volume = BoxVolume(piece.lo, piece.hi);
            if (chart.regions.size() == maxRegions || !(volume > 0. && std::isfinite(volume)))
                return Fail(error, "cut region volume exceeds the chart's size or precision budget");
            if (envelope && Intersects(piece.lo, piece.hi, envelope->lo, envelope->hi, true))
                chart.movingRegions.push_back(chart.regions.size());
            chart.regions.push_back(piece);
            return true;
        });
    }))
        return false;
    chart.offsets.push_back(chart.regions.size());
    if (chart.regions.empty())
        return Fail(error, "cut region chart has no free volume");
    if (!BakeFaces(chart, envelope, error))
        return false;
    output = std::move(chart);
    error.clear();
    return true;
}

double CutRegionClippedVolume(const CutRegion& region, const std::optional<BoxMotion>& motion,
                              const GridVec& clipLo, const GridVec& clipHi, double time) {
    GridVec lo, hi;
    for (int axis = 0; axis < 3; ++axis) {
        lo[axis] = std::max(region.lo[axis], clipLo[axis]);
        hi[axis] = std::min(region.hi[axis], clipHi[axis]);
        if (!(lo[axis] < hi[axis]))
            return 0.;
    }
    if (!motion)
        return BoxVolume(lo, hi);
    const auto x = ClipInterval(lo[0], hi[0], *motion, 0, time);
    const auto y = ClipInterval(lo[1], hi[1], *motion, 1, time);
    const auto z = ClipInterval(lo[2], hi[2], *motion, 2, time);
    return x.free * (hi[1] - lo[1]) * (hi[2] - lo[2]) +
           x.covered * y.free * (hi[2] - lo[2]) + x.covered * y.covered * z.free;
}

bool BuildCutRegionGeometry(const CutRegionChart& chart, const GridVec& startOffset,
                            const GridVec& displacement, CutRegionGeometry& output,
                            std::string& error) {
    if (chart.regions.empty())
        return Fail(error, "cut region chart is empty");
    CutRegionGeometry geometry;
    geometry.motion = chart.moving;
    auto& endpoint = geometry.endpoint = chart.moving;
    if (geometry.motion) {
        auto& box = *geometry.motion;
        for (int axis = 0; axis < 3; ++axis) {
            const double end = startOffset[axis] + displacement[axis];
            if (!(chart.minTravel[axis] <= startOffset[axis] &&
                  startOffset[axis] <= chart.maxTravel[axis] &&
                  chart.minTravel[axis] <= end && end <= chart.maxTravel[axis]))
                return Fail(error, "cut region path exceeds the compiled travel envelope");
            box.lo[axis] += startOffset[axis];
            box.hi[axis] += startOffset[axis];
            endpoint->lo[axis] += end;
            endpoint->hi[axis] += end;
        }
        box.translation = displacement;
    } else if (startOffset != GridVec{} || displacement != GridVec{}) {
        return Fail(error, "cut region chart has no translating box");
    }
    geometry.startCapacity.reserve(chart.regions.size());
    for (const auto& region : chart.regions)
        geometry.startCapacity.push_back(BoxVolume(region.lo, region.hi));
    geometry.endCapacity = geometry.startCapacity;
    geometry.faces = chart.faces;
    if (!geometry.motion) {
        output = std::move(geometry);
        error.clear();
        return true;
    }
    const auto& box = *geometry.motion;
    for (const auto i : chart.movingRegions) {
        const auto& region = chart.regions[i];
        if (CreatesInteriorDivider(region, box))
            return Fail(error, "cut region motion divides one control volume into separate pockets");
        geometry.startCapacity[i] = CutRegionClippedVolume(region, geometry.motion, region.lo, region.hi);
        // Publish the same absolute endpoint representation that the next
        // interval will use for its start. Continuous integration still
        // controls swept volume and wall pressure work.
        geometry.endCapacity[i] = CutRegionClippedVolume(region, endpoint, region.lo, region.hi);
        for (int axis = 0; axis < 3; ++axis) {
            for (const int sign : {-1, 1}) {
                const auto patch = AverageWall(region, box, axis, sign);
                if (patch.area > 0.)
                    geometry.patches.push_back({box.owner, i, axis, sign, patch.centroid, patch.area});
            }
        }
    }
    std::vector<double> sweep(chart.regions.size()), patchSweep(chart.regions.size());
    for (const auto i : chart.movingFaces) {
        auto& face = geometry.faces[i];
        const auto measures = MeasureFace(face, geometry.motion, face.lo, face.hi, 0., 1.);
        face.startArea = measures.start;
        face.endArea = CoversFace(*endpoint, face, 0.)
            ? FaceClip(face, *endpoint, face.lo, face.hi, 0.).free : chart.faces[i].endArea;
        face.meanArea = measures.mean;
        face.sweptVolume = displacement[face.axis] * measures.blockedMean;
        sweep[face.left] += face.sweptVolume;
        sweep[face.right] -= face.sweptVolume;
    }
    for (const auto& patch : geometry.patches)
        patchSweep[patch.cell] -= patch.sign * patch.area * displacement[patch.axis];
    const double h = chart.layout.h;
    const double coarseVolume = h * h * h;
    for (const auto i : chart.movingRegions) {
        const double delta = geometry.endCapacity[i] - geometry.startCapacity[i];
        const double sweepError = std::abs(delta - sweep[i]);
        const double patchError = std::abs(delta - patchSweep[i]);
        const double tolerance = 256. * std::numeric_limits<double>::epsilon() *
            (coarseVolume + std::abs(sweep[i]) + std::abs(patchSweep[i]));
        if (!(sweepError <= tolerance && patchError <= tolerance))
            return Fail(error, "cut region motion fails local geometric conservation");
        geometry.maxSweepError = std::max(geometry.maxSweepError, sweepError);
        geometry.maxPatchError = std::max(geometry.maxPatchError, patchError);
    }
    output = std::move(geometry);
    error.clear();
    return true;
}

bool ReconstructCutRegionWaterPlanes(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                                    int defaultAxis, const std::vector<double>& startWater,
                                    const std::vector<CutRegionInterface>& interfaces,
                                    std::vector<CutRegionWaterPlane>& waterPlanes, std::string& error) {
    if (defaultAxis < 0 || defaultAxis > 2 || startWater.size() != chart.regions.size() ||
        geometry.startCapacity.size() != chart.regions.size() ||
        (!interfaces.empty() && interfaces.size() != chart.regions.size()))
        return Fail(error, "cut region phase data does not match its pressure geometry");
    std::vector<CutRegionWaterPlane> planes;
    planes.reserve(startWater.size());
    for (std::size_t i = 0; i < startWater.size(); ++i) {
        const auto& region = chart.regions[i];
        const auto orientation = interfaces.empty() ? CutRegionInterface{defaultAxis, false} : interfaces[i];
        const int phaseAxis = orientation.axis;
        if (phaseAxis < 0 || phaseAxis > 2)
            return Fail(error, "cut region phase plane axis is invalid");
        const double lo = region.lo[phaseAxis], hi = region.hi[phaseAxis];
        const double capacity = geometry.startCapacity[i], water = startWater[i];
        const double tolerance = 128. * std::numeric_limits<double>::epsilon() * capacity;
        if (!(0. <= capacity && std::isfinite(capacity) && 0. <= water &&
              std::isfinite(water) && water <= capacity + tolerance))
            return Fail(error, "water is outside its cut region capacity");
        // A region without water has no wet ownership, including where
        // the moving solid exposes additional volume during the interval.
        if (water == 0.) {
            planes.push_back({orientation, orientation.wetAbove ? hi : lo});
            continue;
        }
        std::array<double, 4> cuts{lo, hi, lo, hi};
        if (geometry.motion) {
            cuts[2] = std::clamp(geometry.motion->lo[phaseAxis], lo, hi);
            cuts[3] = std::clamp(geometry.motion->hi[phaseAxis], lo, hi);
        }
        std::sort(cuts.begin(), cuts.end());
        auto clipLo = region.lo, clipHi = region.hi;
        const double air = std::max(0., capacity - water);
        const bool locateWet = water <= air;
        const bool fromHigh = locateWet == orientation.wetAbove;
        double remaining = locateWet ? water : air;
        CutRegionWaterPlane plane{orientation, fromHigh ? lo : hi};
        for (std::size_t j = 1; j < cuts.size(); ++j) {
            const auto interval = fromHigh ? cuts.size() - j : j;
            const double lower = cuts[interval - 1], upper = cuts[interval];
            clipLo[phaseAxis] = lower;
            clipHi[phaseAxis] = upper;
            const double slab = CutRegionClippedVolume(region, geometry.motion, clipLo, clipHi);
            if (slab > 0. && remaining <= slab) {
                const double depth = (upper - lower) * (remaining / slab);
                const double anchor = fromHigh ? upper : lower;
                const double offset = fromHigh ? -depth : depth;
                plane.coordinate = anchor + offset;
                const double part = plane.coordinate - anchor;
                plane.remainder = (anchor - (plane.coordinate - part)) + (offset - part);
                break;
            }
            remaining -= slab;
        }
        planes.push_back(plane);
    }
    waterPlanes = std::move(planes);
    error.clear();
    return true;
}

std::array<double, 2> MeasureCutRegionPhaseVolumes(const CutRegion& region,
    const std::optional<BoxMotion>& motion, const GridVec& lo, const GridVec& hi,
    const CutRegionWaterPlane& water) {
    const auto [axis, wetAbove] = water.orientation;
    if (axis < 0 || axis > 2 || !std::isfinite(water.coordinate) || !std::isfinite(water.remainder))
        return {};
    auto cut = region;
    auto solid = motion;
    auto lower = lo, upper = hi;
    double plane = water.coordinate;
    if (water.remainder != 0.) {
        const double anchor = water.coordinate;
        cut.lo[axis] -= anchor;
        cut.hi[axis] -= anchor;
        lower[axis] -= anchor;
        upper[axis] -= anchor;
        if (solid) {
            solid->lo[axis] -= anchor;
            solid->hi[axis] -= anchor;
        }
        plane = water.remainder;
    }
    auto lowerHi = upper, upperLo = lower;
    lowerHi[axis] = std::min(lowerHi[axis], plane);
    upperLo[axis] = std::max(upperLo[axis], plane);
    const double below = CutRegionClippedVolume(cut, solid, lower, lowerHi);
    const double above = CutRegionClippedVolume(cut, solid, upperLo, upper);
    return wetAbove ? std::array{above, below} : std::array{below, above};
}

double MeasureCutRegionInterfaceArea(const CutRegion& region, const std::optional<BoxMotion>& motion,
                                     const CutRegionWaterPlane& plane) {
    const int axis = plane.orientation.axis;
    if (axis < 0 || axis > 2 ||
        !(plane.RelativeTo(region.lo[axis]) > 0. && plane.RelativeTo(region.hi[axis]) < 0.))
        return 0.;
    CutRegionFace face{};
    face.axis = axis;
    face.lo = region.lo;
    face.hi = region.hi;
    face.lo[axis] = face.hi[axis] = plane.remainder;
    auto local = motion;
    if (local) {
        local->lo[axis] -= plane.coordinate;
        local->hi[axis] -= plane.coordinate;
    }
    return MeasureFace(face, local, face.lo, face.hi, 0., 1.).start;
}

bool MeasureCutRegionFixedPhaseGeometry(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                                        const std::vector<CutRegionWaterPlane>& planes,
                                        CutRegionFixedPhaseGeometry& output, std::string& error) {
    const auto count = chart.regions.size();
    if (count == 0 || planes.size() != count || geometry.startCapacity.size() != count ||
        geometry.endCapacity.size() != count || geometry.motion.has_value() != geometry.endpoint.has_value())
        return Fail(error, "invalid cut-region fixed phase dimensions");
    CutRegionFixedPhaseGeometry next;
    next.startVolumes.resize(count);
    next.endVolumes.resize(count);
    std::vector<std::array<double, 2>> swept(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& region = chart.regions[i];
        const auto& plane = planes[i];
        if (plane.orientation.axis < 0 || plane.orientation.axis > 2 ||
            !std::isfinite(plane.coordinate) || !std::isfinite(plane.remainder))
            return Fail(error, "invalid fixed cut-region phase plane");
        next.startVolumes[i] = MeasureCutRegionPhaseVolumes(region, geometry.motion, region.lo, region.hi, plane);
        next.endVolumes[i] = MeasureCutRegionPhaseVolumes(region, geometry.endpoint, region.lo, region.hi, plane);
    }
    for (const auto& patch : geometry.patches) {
        if (!geometry.motion || patch.cell >= count || patch.axis < 0 || patch.axis > 2 ||
            (patch.sign != -1 && patch.sign != 1) || patch.owner != geometry.motion->owner)
            return Fail(error, "invalid fixed-phase wall patch");
        const auto& plane = planes[patch.cell];
        const auto [axis, wetAbove] = plane.orientation;
        auto box = *geometry.motion;
        auto region = chart.regions[patch.cell];
        const double anchor = plane.coordinate;
        region.lo[axis] -= anchor;
        region.hi[axis] -= anchor;
        box.lo[axis] -= anchor;
        box.hi[axis] -= anchor;
        for (int material = 0; material < 2; ++material) {
            auto clipped = region;
            if ((material == 0) == wetAbove)
                clipped.lo[axis] = std::max(clipped.lo[axis], plane.remainder);
            else
                clipped.hi[axis] = std::min(clipped.hi[axis], plane.remainder);
            if (!(clipped.lo[axis] < clipped.hi[axis]))
                continue;
            auto measure = AverageWall(clipped, box, patch.axis, patch.sign);
            if (measure.area == 0.)
                continue;
            measure.centroid[axis] += anchor;
            next.wallPatches[material].push_back({patch.owner, patch.cell, patch.axis, patch.sign,
                                                  measure.centroid, measure.area});
            swept[patch.cell][material] += patch.sign * measure.area * box.translation[patch.axis];
        }
    }
    const double h = chart.layout.h;
    const double coarseVolume = h * h * h;
    const double roundoff = 256. * std::numeric_limits<double>::epsilon();
    for (std::size_t i = 0; i < count; ++i) {
        const auto& a = next.startVolumes[i];
        const auto& b = next.endVolumes[i];
        if (!(std::abs((a[0] + a[1]) - geometry.startCapacity[i]) <= roundoff * coarseVolume &&
              std::abs((b[0] + b[1]) - geometry.endCapacity[i]) <= roundoff * coarseVolume))
            return Fail(error, "cut-region fixed phases do not cover the physical volume");
        for (int material = 0; material < 2; ++material) {
            const double defect = std::abs((a[material] - b[material]) - swept[i][material]);
            if (!(defect <= roundoff * (coarseVolume + std::abs(swept[i][material]))))
                return Fail(error, "cut-region phase wall sweep disagrees with its volume source");
            next.maximumSweepError = std::max(next.maximumSweepError, defect);
        }
    }
    output = std::move(next);
    error.clear();
    return true;
}

bool ReconstructCutRegionWaterPlanes(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                                    int phaseAxis, const std::vector<double>& startWater,
                                    std::vector<double>& waterPlanes, std::string& error) {
    std::vector<CutRegionWaterPlane> planes;
    if (!ReconstructCutRegionWaterPlanes(chart, geometry, phaseAxis, startWater, {}, planes, error))
        return false;
    waterPlanes.clear();
    waterPlanes.reserve(planes.size());
    for (const auto& plane : planes)
        waterPlanes.push_back(plane.coordinate);
    return true;
}

namespace {

double AverageConnectedPrism(const CutRegionFace& face, const std::optional<BoxMotion>& motion,
                            const GridVec& lo, const GridVec& hi, bool left) {
    for (int axis = 0; axis < 3; ++axis)
        if (!(lo[axis] < hi[axis]))
            return 0.;
    const double volume = BoxVolume(lo, hi);
    if (!motion)
        return volume;
    const auto& box = *motion;
    const int axis = face.axis;
    const double inwardLo = left ? -hi[axis] : lo[axis];
    const double inwardHi = left ? -lo[axis] : hi[axis];
    const double near = left ? -box.hi[axis] : box.lo[axis];
    const double far = left ? -box.lo[axis] : box.hi[axis];
    const double travel = left ? -box.translation[axis] : box.translation[axis];
    if (std::max(far, far + travel) < 0. || std::min(near, near + travel) >= inwardHi)
        return volume;
    for (const int tangent : {(axis + 1) % 3, (axis + 2) % 3}) {
        const double delta = box.translation[tangent];
        if (std::max(box.hi[tangent], box.hi[tangent] + delta) <= lo[tangent] ||
            std::min(box.lo[tangent], box.lo[tangent] + delta) >= hi[tangent])
            return volume;
    }
    const auto sample = [&](double time, bool columnStops) {
        if (!columnStops)
            return volume;
        const auto area = FaceClip(face, box, lo, hi, time);
        const double stop = std::fma(travel, time, near);
        const double connected = std::clamp(stop - inwardLo, 0., inwardHi - inwardLo);
        return area.free * (inwardHi - inwardLo) + area.covered * connected;
    };
    if (box.translation == GridVec{})
        return sample(0., far >= 0.);
    Events events;
    events.Cross(far, travel, 0.);
    events.Cross(near, travel, inwardLo);
    events.Cross(near, travel, inwardHi);
    for (const int tangent : {(axis + 1) % 3, (axis + 2) % 3})
        AddTangentEvents(events, box, tangent, lo[tangent], hi[tangent]);
    events.Sort();
    double mean = 0.;
    for (std::size_t i = 1; i < events.count; ++i) {
        const double start = events.times[i - 1], end = events.times[i];
        const double middle = start + .5 * (end - start);
        const bool columnStops = std::fma(travel, middle, far) >= 0.;
        // The tangential area is quadratic and the column extent is
        // affine on each event interval, so Simpson integrates the volume.
        mean += (end - start) / 6. *
            (sample(start, columnStops) + 4. * sample(middle, columnStops) + sample(end, columnStops));
    }
    return mean;
}

} // namespace

std::array<double, 2> MeasureCutRegionFaceInertiaVolumes(const CutRegionFace& face,
    const std::optional<BoxMotion>& motion, const CutRegion& region, bool left,
    const CutRegionWaterPlane& water) {
    const int normal = face.axis;
    const auto [axis, wetAbove] = water.orientation;
    if (normal < 0 || normal > 2 || axis < 0 || axis > 2 ||
        !std::isfinite(water.coordinate) || !std::isfinite(water.remainder))
        return {};
    auto localFace = face;
    auto solid = motion;
    auto lo = face.lo, hi = face.hi;
    const double depth = .5 * (region.hi[normal] - region.lo[normal]);
    const double mouth = face.lo[normal];
    lo[normal] = left ? -depth : 0.;
    hi[normal] = left ? 0. : depth;
    localFace.lo[normal] = localFace.hi[normal] = 0.;
    if (solid) {
        solid->lo[normal] -= mouth;
        solid->hi[normal] -= mouth;
    }
    double plane = water.coordinate;
    if (axis == normal) {
        plane = water.RelativeTo(mouth);
    } else if (water.remainder != 0.) {
        const double anchor = water.coordinate;
        lo[axis] -= anchor;
        hi[axis] -= anchor;
        localFace.lo[axis] -= anchor;
        localFace.hi[axis] -= anchor;
        if (solid) {
            solid->lo[axis] -= anchor;
            solid->hi[axis] -= anchor;
        }
        plane = water.remainder;
    }
    auto lowerHi = hi, upperLo = lo;
    lowerHi[axis] = std::min(hi[axis], plane);
    upperLo[axis] = std::max(lo[axis], plane);
    const double below = AverageConnectedPrism(localFace, solid, lo, lowerHi, left);
    const double above = AverageConnectedPrism(localFace, solid, upperLo, hi, left);
    return wetAbove ? std::array{above, below} : std::array{below, above};
}

CutRegionAperture MeasureCutRegionAperture(const CutRegionFace& face,
                                         const std::optional<BoxMotion>& motion,
                                         const GridVec& clipLo, const GridVec& clipHi,
                                         double begin, double end) {
    const auto m = MeasureFace(face, motion, clipLo, clipHi, begin, end);
    return {m.start, m.mean};
}

CutRegionFacePhase MeasureCutRegionFacePhase(const CutRegionFace& face,
                                           const std::optional<BoxMotion>& motion,
                                           const CutRegionWaterPlane& left,
                                           const CutRegionWaterPlane& right) {
    CutRegionFacePhase result{};
    for (int side = 0; side < 2; ++side) {
        const auto& shape = side == 0 ? left : right;
        const auto [phaseAxis, wetAbove] = shape.orientation;
        if (phaseAxis < 0 || phaseAxis > 2 || !std::isfinite(shape.coordinate) ||
            !std::isfinite(shape.remainder))
            continue;
        const CutRegionAperture whole{face.startArea, face.meanArea};
        if (face.axis == phaseAxis) {
            const double distance = shape.RelativeTo(face.lo[phaseAxis]);
            const bool wetBelow = side == 0 ? distance >= 0. : distance > 0.;
            const bool wet = wetBelow != wetAbove;
            auto& aperture = wet ? result.wet[side] : result.dry[side];
            aperture = whole;
        } else if (shape.RelativeTo(face.lo[phaseAxis]) <= 0.) {
            (wetAbove ? result.wet : result.dry)[side] = whole;
        } else if (shape.RelativeTo(face.hi[phaseAxis]) >= 0.) {
            (wetAbove ? result.dry : result.wet)[side] = whole;
        } else {
            auto clippedFace = face;
            auto solid = motion;
            double plane = shape.coordinate;
            if (shape.remainder != 0.) {
                const double anchor = shape.coordinate;
                clippedFace.lo[phaseAxis] -= anchor;
                clippedFace.hi[phaseAxis] -= anchor;
                if (solid) {
                    solid->lo[phaseAxis] -= anchor;
                    solid->hi[phaseAxis] -= anchor;
                }
                plane = shape.remainder;
            }
            auto lowerHi = clippedFace.hi, upperLo = clippedFace.lo;
            lowerHi[phaseAxis] = plane;
            upperLo[phaseAxis] = plane;
            (wetAbove ? result.dry : result.wet)[side] =
                MeasureCutRegionAperture(clippedFace, solid, clippedFace.lo, lowerHi);
            (wetAbove ? result.wet : result.dry)[side] =
                MeasureCutRegionAperture(clippedFace, solid, upperLo, clippedFace.hi);
        }
    }
    return result;
}

CutRegionAperture MeasureCutRegionSharedPhase(const CutRegionFace& face,
    const std::optional<BoxMotion>& motion, const CutRegionWaterPlane& left,
    const CutRegionWaterPlane& right, bool leftWet, bool rightWet) {
    if (face.axis < 0 || face.axis > 2)
        return {};
    struct Bound {
        double coordinate, remainder;

        double RelativeTo(double anchor) const { return (coordinate - anchor) + remainder; }
    };
    std::array<Bound, 3> lower, upper;
    for (int axis = 0; axis < 3; ++axis) {
        lower[axis] = {face.lo[axis], 0.};
        upper[axis] = {face.hi[axis], 0.};
    }
    const auto clip = [&](const CutRegionWaterPlane& plane, bool wet, bool leftSide) {
        const auto [axis, wetAbove] = plane.orientation;
        if (axis < 0 || axis > 2 || !std::isfinite(plane.coordinate) || !std::isfinite(plane.remainder))
            return false;
        const bool keepAbove = wet == wetAbove;
        if (axis == face.axis) {
            const double distance = plane.RelativeTo(face.lo[axis]);
            const bool below = leftSide ? distance >= 0. : distance > 0.;
            return below != keepAbove;
        }
        const Bound next{plane.coordinate, plane.remainder};
        auto& bound = keepAbove ? lower[axis] : upper[axis];
        const double difference = next.RelativeTo(bound.coordinate) - bound.remainder;
        if (keepAbove ? difference > 0. : difference < 0.)
            bound = next;
        return true;
    };
    if (!clip(left, leftWet, true) || !clip(right, rightWet, false))
        return {};
    auto localFace = face;
    auto solid = motion;
    GridVec lo = face.lo, hi = face.hi;
    for (int axis = 0; axis < 3; ++axis) {
        if (axis == face.axis)
            continue;
        const auto a = lower[axis], b = upper[axis];
        const double anchor = a.remainder != 0. ? a.coordinate :
            b.remainder != 0. ? b.coordinate : 0.;
        lo[axis] = a.RelativeTo(anchor);
        hi[axis] = b.RelativeTo(anchor);
        if (!(lo[axis] < hi[axis]))
            return {};
        localFace.lo[axis] -= anchor;
        localFace.hi[axis] -= anchor;
        if (solid) {
            solid->lo[axis] -= anchor;
            solid->hi[axis] -= anchor;
        }
    }
    return MeasureCutRegionAperture(localFace, solid, lo, hi);
}

CutRegionFacePhase MeasureCutRegionFacePhase(const CutRegionFace& face,
                                           const std::optional<BoxMotion>& motion, int phaseAxis,
                                           double leftPlane, double rightPlane) {
    return MeasureCutRegionFacePhase(face, motion, {{phaseAxis, false}, leftPlane},
                                                   {{phaseAxis, false}, rightPlane});
}

} // namespace judas::volume_grid
