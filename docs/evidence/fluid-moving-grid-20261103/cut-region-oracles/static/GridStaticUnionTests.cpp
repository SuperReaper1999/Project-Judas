#include "GridStaticUnion.h"
#include "GridPressureReaction.h"

#include <chrono>
#include <cstdio>
#include <numeric>

namespace {

using namespace judas::volume_grid;

struct Checks {
    int total = 0, failed = 0;
    void Expect(bool pass, const char* label, double value = 0.) {
        ++total;
        failed += !pass;
        std::printf("%s %s %.17g\n", pass ? "PASS" : "FAIL", label, value);
    }
};

bool Equal(const GridStaticUnion& a, const GridStaticUnion& b) {
    return a.capacity == b.capacity && a.aperture == b.aperture;
}

void OverlapAndTouch(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, 1.};
    const BoxMotion a{1, {1.25, 1.125, 1.25}, {2.25, 1.625, 1.75}, {}},
                    b{2, {1.5, 1.375, 1.375}, {2.5, 1.875, 1.875}, {}};
    const auto cell = layout.Cell(1, 1, 1), face = layout.Face(0, 2, 1, 1);
    GridStaticUnion result, reordered, duplicate;
    std::string error;
    const bool built = BuildGridStaticUnion(layout, {a, b}, result, error);
    checks.Expect(built, built ? "overlapping walls build as one static union" : error.c_str());
    if (built) {
        checks.Expect(result.capacity[cell] == .734375 && result.aperture[0][face] == .59375,
                      "capacity and aperture include intersection exactly once", result.aperture[0][face]);
        checks.Expect(BuildGridStaticUnion(layout, {b, a}, reordered, error) &&
                      BuildGridStaticUnion(layout, {a, b, a}, duplicate, error) &&
                      Equal(result, reordered) && Equal(result, duplicate),
                      "reordering or duplicating static geometry leaves physical measure identical");
    }

    const BoxMotion low{3, {.75, 1., 1.}, {2.25, 1.5, 2.}, {}},
                    high{4, {.75, 1.5, 1.}, {2.25, 2., 2.}, {}};
    const bool joined = BuildGridStaticUnion(layout, {low, high}, result, error);
    checks.Expect(joined && result.capacity[cell] == 0. && result.aperture[0][face] == 0.,
                  joined ? "touching wall halves leave no fictitious opening" : error.c_str());

    GridStaticUnion single;
    MovingGridGeometry original;
    const GridLayout nonBinary{{5, 5, 5}, .2};
    const BoxMotion inner{5, {.27, .31, .42}, {.74, .87, .91}, {}};
    const bool compared = BuildGridStaticUnion(nonBinary, {inner}, single, error) &&
                          BuildMovingGridGeometry(nonBinary, {inner}, original, error);
    double maximum = 0.;
    if (compared) {
        for (std::size_t i = 0; i < single.capacity.size(); ++i)
            maximum = std::max(maximum, std::abs(single.capacity[i] - original.startCapacity[i]));
        for (int axis = 0; axis < 3; ++axis)
            for (std::size_t i = 0; i < single.aperture[axis].size(); ++i)
                maximum = std::max(maximum, std::abs(single.aperture[axis][i] - original.startAperture[axis][i]));
    }
    checks.Expect(compared && maximum < 1e-16,
                  "stationary disjoint geometry agrees with the existing cut builder", maximum);
}

void FreeConnectivity(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, 1.};
    const BoxMotion dividing{1, {1.375, 1., 1.}, {1.625, 2., 2.}, {}};
    GridStaticUnion result;
    std::string error;
    const bool baseline = BuildGridStaticUnion(layout, {}, result, error);
    checks.Expect(baseline, "empty grid establishes rollback baseline");
    const auto saved = result;
    checks.Expect(!BuildGridStaticUnion(layout, {dividing}, result, error) &&
                  error.find("disconnected free pockets in cell (1,1,1)") != std::string::npos && Equal(result, saved),
                  "an in-cell slab with two free pockets rejects transactionally");
    const BoxMotion cornerA{2, {1., 1., 1.}, {1.5, 1.5, 2.}, {}},
                    cornerB{3, {1.5, 1.5, 1.}, {2., 2., 2.}, {}};
    checks.Expect(!BuildGridStaticUnion(layout, {cornerA, cornerB}, result, error) &&
                  error.find("disconnected free pockets") != std::string::npos && Equal(result, saved),
                  "a point or line connection cannot join disconnected fluid pockets");

    auto nearlyDividing = dividing;
    nearlyDividing.hi[1] = std::nextafter(2., 0.);
    const bool open = BuildGridStaticUnion(layout, {nearlyDividing}, result, error);
    checks.Expect(open && result.capacity[layout.Cell(1, 1, 1)] >= .75,
                  open ? "one representable step of positive gap connects the two pockets" : error.c_str());
    const auto openState = result;
    std::vector<BoxMotion> many;
    for (int i = 0; i < 10; ++i) {
        const double lo = 1.125 + .03125 * i, hi = lo + .015625;
        many.push_back({std::uint32_t(i), {lo, lo, lo}, {hi, hi, hi}, {}});
    }
    checks.Expect(!BuildGridStaticUnion(layout, many, result, error) &&
                  error.find("piece budget") != std::string::npos && Equal(result, openState),
                  "local partition has a bounded work budget and preserves the last result");
}

std::vector<BoxMotion> Fixture(bool sealed) {
    constexpr GridVec origin{-1.5, -.5, -1.5};
    std::vector<BoxMotion> boxes;
    const auto box = [&](const std::array<float, 3>& center, const std::array<float, 3>& half) {
        BoxMotion result{std::uint32_t(boxes.size() + 1)};
        for (int axis = 0; axis < 3; ++axis) {
            result.lo[axis] = double(center[axis]) - half[axis] - origin[axis];
            result.hi[axis] = double(center[axis]) + half[axis] - origin[axis];
        }
        boxes.push_back(result);
    };
    // Preserve the float authored constants from ProductionFluidTests.cpp.
    box({0.f, -.1f, 0.f}, {1.1f, .1f, 1.1f});
    box({-1.1f, 1.5f, 0.f}, {.1f, 1.5f, 1.1f});
    box({1.1f, 1.5f, 0.f}, {.1f, 1.5f, 1.1f});
    const float zWallHalfHeight = sealed ? 1.5f : 1.499f;
    box({0.f, 1.5f, -1.1f}, {1.f, zWallHalfHeight, .1f});
    box({0.f, 1.5f, 1.1f}, {1.f, zWallHalfHeight, .1f});
    return boxes;
}

double IndependentFreeVolume(const GridLayout& layout, const std::vector<BoxMotion>& boxes) {
    long double solid = 0.;
    // Inclusion-exclusion, independent of the elementary partition.
    for (unsigned set = 1; set < (1u << boxes.size()); ++set) {
        std::array<long double, 3> lo{}, hi{};
        for (int axis = 0; axis < 3; ++axis)
            hi[axis] = layout.cells[axis] * layout.h;
        bool odd = false;
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            if ((set & (1u << i)) == 0)
                continue;
            odd = !odd;
            for (int axis = 0; axis < 3; ++axis) {
                lo[axis] = std::max(lo[axis], static_cast<long double>(boxes[i].lo[axis]));
                hi[axis] = std::min(hi[axis], static_cast<long double>(boxes[i].hi[axis]));
            }
        }
        long double volume = 1.;
        for (int axis = 0; axis < 3; ++axis)
            volume *= std::max(0.L, hi[axis] - lo[axis]);
        solid += odd ? volume : -volume;
    }
    return double(layout.CellCount() * std::pow(static_cast<long double>(layout.h), 3) - solid);
}

void AuthoredFixture(Checks& checks) {
    const GridLayout layout{{24, 32, 24}, .125};
    const double gap = 1.5 - double(1.499f);
    const double expected = 2. * gap;
    for (const bool sealed : {false, true}) {
        const auto boxes = Fixture(sealed);
        GridStaticUnion geometry;
        std::string error;
        const bool built = BuildGridStaticUnion(layout, boxes, geometry, error);
        checks.Expect(built, built ? (sealed ? "matched Z-floor seam control builds" : "authored half_025 five walls build")
                                  : error.c_str());
        if (!built)
            continue;
        double aperture[2]{};
        for (int side = 0; side < 2; ++side) {
            // x ∈ [-1,1], y ∈ [0,.125], z = ±1.125 in world coordinates.
            const int z = side == 0 ? 3 : 21;
            for (int x = 4; x < 20; ++x)
                aperture[side] += geometry.aperture[2][layout.Face(2, x, 4, z)];
            checks.Expect(std::abs(aperture[side] - (sealed ? 0. : expected)) < 1e-16,
                sealed ? "matched control seals measured seam at z=±1.125" :
                         "authored measured seam at z=±1.125 agrees with analytic area", aperture[side]);
        }
        const double volume = double(std::accumulate(geometry.capacity.begin(), geometry.capacity.end(), 0.L));
        checks.Expect(std::abs(volume - IndependentFreeVolume(layout, boxes)) < 1e-14,
                      "authored chart free volume agrees with independent box inclusion-exclusion", volume);

        std::vector<double> samples;
        for (int i = 0; i < 11; ++i) {
            const auto begin = std::chrono::steady_clock::now();
            if (!BuildGridStaticUnion(layout, boxes, geometry, error)) {
                checks.Expect(false, error.c_str());
                break;
            }
            const auto end = std::chrono::steady_clock::now();
            samples.push_back(std::chrono::duration<double, std::milli>(end - begin).count());
        }
        if (!samples.empty()) {
            std::sort(samples.begin(), samples.end());
            std::printf("TIMING %s cells=%zu samples=%zu median_ms=%.6f max_ms=%.6f max_pieces=%zu checks_bound=%zu\n",
                sealed ? "sealed" : "authored", layout.CellCount(), samples.size(), samples[samples.size()/2],
                samples.back(), geometry.maximumLocalPieces, geometry.occupancyChecksBound);
        }
    }
}

void AuthoredComposition(Checks& checks) {
    const GridLayout layout{{24, 32, 24}, .125};
    const auto walls = Fixture(false);
    GridStaticUnion stationary;
    MovingGridGeometry geometry;
    std::string error;
    constexpr double half = double(.3f), release = double(.7f) + .5, travel = 1.25;
    const BoxMotion body{10, {1.5 - half, release - half, 1.5 - half},
                            {1.5 + half, release + half, 1.5 + half}, {0., travel, 0.}};
    const bool composed = BuildGridStaticUnion(layout, walls, stationary, error) &&
        BuildGridStaticMovingUnion(stationary, {body}, geometry, error);
    checks.Expect(composed, composed ? "compose actual static walls and authored moving release box" : error.c_str());
    if (!composed)
        return;
    constexpr double surface = 1.7094379663467407 + .5;
    checks.Expect(body.hi[1] < surface && body.lo[1] + travel < surface && body.hi[1] + travel > surface,
                  "prescribed authored box crosses from full immersion to partial immersion");
    const auto overlap = [](double first, double last, double lo, double hi) {
        return std::max(0., std::min(last, hi) - std::max(first, lo));
    };
    const auto atCell = [&](int axis, int index, double time) {
        return overlap(body.lo[axis] + body.translation[axis] * time,
                       body.hi[axis] + body.translation[axis] * time,
                       index * layout.h, (index + 1) * layout.h);
    };
    const auto ySweep = [&](int x, int y, int z) {
        const double plane = y * layout.h;
        const double first = std::clamp((plane - body.hi[1]) / travel, 0., 1.);
        const double last = std::clamp((plane - body.lo[1]) / travel, 0., 1.);
        return travel * std::max(0., last - first) * atCell(0, x, 0.) * atCell(2, z, 0.);
    };
    std::vector<double> patchFlux(layout.CellCount()), pressure(layout.CellCount());
    bool ownerOnly = !geometry.averagePatches.empty();
    for (const auto& patch : geometry.averagePatches) {
        ownerOnly = ownerOnly && patch.owner == body.owner;
        if (patch.axis == 1)
            patchFlux[patch.cell] -= patch.sign * travel * patch.area;
    }
    double capacityError = 0., sweptError = 0., balanceError = 0., patchError = 0., work = 0.;
    std::size_t active = 0;
    for (int z = 0; z < layout.cells[2]; ++z) {
        for (int y = 0; y < layout.cells[1]; ++y) {
            for (int x = 0; x < layout.cells[0]; ++x) {
                const auto c = layout.Cell(x, y, z);
                const auto low = layout.Face(1, x, y, z), high = layout.Face(1, x, y + 1, z);
                const double side = atCell(0, x, 0.) * atCell(2, z, 0.);
                const double oldVolume = side * atCell(1, y, 0.), newVolume = side * atCell(1, y, 1.);
                const double change = geometry.endCapacity[c] - geometry.startCapacity[c];
                capacityError = std::max({capacityError,
                    std::abs(geometry.startCapacity[c] - (stationary.capacity[c] - oldVolume)),
                    std::abs(geometry.endCapacity[c] - (stationary.capacity[c] - newVolume))});
                const double lowSweep = geometry.solidSweep[1][low], highSweep = geometry.solidSweep[1][high];
                sweptError = std::max({sweptError, std::abs(lowSweep - ySweep(x, y, z)),
                                       std::abs(highSweep - ySweep(x, y + 1, z))});
                balanceError = std::max(balanceError, std::abs(change - (highSweep - lowSweep)));
                patchError = std::max(patchError, std::abs(change - patchFlux[c]));
                pressure[c] = 2. + .01 * y * y + .02 * x + .005 * z;
                work += pressure[c] * (newVolume - oldVolume);
                active += oldVolume > 0. || newVolume > 0.;
            }
        }
    }
    checks.Expect(active > 100 && capacityError < 1e-16 && sweptError < 1e-16 && balanceError < 1e-16,
                  "all composed cells close against independently clipped capacity and signed face sweep", balanceError);
    const auto loads = AccumulateGridPressureReaction(geometry.averagePatches, pressure);
    const auto frozen = AccumulateGridPressureReaction(geometry.startPatches, pressure);
    const double workError = loads.empty() ? 1. : work + loads[0].force[1] * travel;
    const double frozenError = frozen.empty() ? 0. : work + frozen[0].force[1] * travel;
    checks.Expect(ownerOnly && loads.size() == 1 && patchError < 1e-16 && std::abs(workError) < 1e-13,
                  "dynamic-only swept patch traction closes against independent pressure work", workError);
    checks.Expect(std::abs(frozenError) > .01,
                  "frozen start-wall patches fail the same nonuniform pressure-work check", frozenError);

    const double expectedSeam = 2. * (1.5 - double(1.499f));
    for (const int z : {3, 21}) {
        double startArea = 0., endArea = 0., meanArea = 0.;
        for (int x = 4; x < 20; ++x) {
            const auto f = layout.Face(2, x, 4, z);
            startArea += geometry.startAperture[2][f];
            endArea += geometry.endAperture[2][f];
            meanArea += geometry.averageAperture[2][f];
        }
        checks.Expect(startArea == expectedSeam && endArea == expectedSeam && meanArea == expectedSeam,
                      "authored seam remains open throughout composed body motion", meanArea);
    }

    const auto savedCapacity = geometry.endCapacity;
    const auto savedAperture = geometry.averageAperture;
    const BoxMotion crossing{11, {2.36, 1.45, 1.45}, {2.44, 1.55, 1.55}, {.45, 0., 0.}};
    auto before = crossing, after = crossing;
    before.translation = {};
    after.lo[0] += crossing.translation[0];
    after.hi[0] += crossing.translation[0];
    after.translation = {};
    MovingGridGeometry endpoint;
    const bool endpoints = BuildGridStaticMovingUnion(stationary, {before}, endpoint, error) &&
        BuildGridStaticMovingUnion(stationary, {after}, endpoint, error);
    checks.Expect(endpoints && !BuildGridStaticMovingUnion(stationary, {crossing}, geometry, error) &&
                  error.find("swept path") != std::string::npos && geometry.endCapacity == savedCapacity &&
                  geometry.averageAperture == savedAperture,
                  "a transient wall crossing rejects even when both endpoint boxes are disjoint");
    auto touching = before;
    touching.hi[0] = walls[2].lo[0] - .125;
    touching.lo[0] = touching.hi[0] - .08;
    touching.translation[0] = .125;
    checks.Expect(!BuildGridStaticMovingUnion(stationary, {touching}, geometry, error) &&
                  error.find("swept path") != std::string::npos && geometry.endCapacity == savedCapacity,
                  "zero-volume wall contact still rejects instead of double-owning a face");
    auto duplicate = body;
    duplicate.owner = body.owner + 1;
    checks.Expect(!BuildGridStaticMovingUnion(stationary, {body, duplicate}, geometry, error) &&
                  error.find("remain disjoint") != std::string::npos && geometry.endCapacity == savedCapacity,
                  "overlapping moving bodies reject transactionally");
}

void CachedClips(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, 1.};
    const BoxMotion a{1, {1.25, 1.125, 1.25}, {2.25, 1.625, 1.75}, {}},
                    b{2, {1.5, 1.375, 1.375}, {2.5, 1.875, 1.875}, {}};
    GridStaticUnion geometry;
    std::string error;
    const bool built = BuildGridStaticUnion(layout, {a, b}, geometry, error);
    checks.Expect(built && geometry.cachedPieces > 0, "build cached overlapping wall partitions");
    if (!built)
        return;
    double water = -1., opening = -1.;
    const std::array<int, 3> cell{1, 1, 1};
    const bool volume = ClipGridStaticCellVolume(geometry, cell, {1., 1., 1.}, {1.5, 1.4375, 2.}, water, error);
    // Raw half-cell cut .5 × .4375 × 1, less one box's .25 × .3125 × .5.
    checks.Expect(volume && water == .1796875,
                  "cached half-cell partial fill matches independent rectangular volume", water);
    const bool aperture = ClipGridStaticFaceArea(geometry, cell, 0, 1.5, {0., 1., 1.},
                                                 {0., 1.75, 1.875}, opening, error);
    // At x=1.5 the second box begins exactly on the plane. Both solid
    // footprints count and overlap by .25 × .375 m².
    checks.Expect(aperture && opening == .3125,
                  "midpoint wet rectangle measures the union from both incident slabs", opening);
    const bool coincident = ClipGridStaticFaceArea(geometry, cell, 0, 1.5, {0., 1.7, 1.8},
                                                   {0., 1.75, 1.85}, opening, error);
    checks.Expect(coincident && opening == 0.,
                  "midpoint rectangle on an exactly coincident solid face is closed", opening);

    const double saved = opening;
    checks.Expect(!ClipGridStaticFaceArea(geometry, cell, 0, 2.125, {}, {}, opening, error) && opening == saved,
                  "out-of-cell interior clip rejects without publishing an area");
    checks.Expect(!ClipGridStaticCellVolume(geometry, cell, {1.5, 1., 1.}, {1.25, 2., 2.}, water, error) &&
                  water == .1796875, "reversed volume bounds reject without publishing a value");

    const GridLayout tank{{24, 32, 24}, .125};
    const double gap = 1.5 - double(1.499f), waterTop = .5005;
    const double expectedWetArea = 2. * (waterTop - .5);
    for (const bool sealed : {false, true}) {
        const bool tankBuilt = BuildGridStaticUnion(tank, Fixture(sealed), geometry, error);
        checks.Expect(tankBuilt, tankBuilt ? "cache authored tank phase-query geometry" : error.c_str());
        if (!tankBuilt)
            continue;
        for (const int face : {3, 21}) {
            const int inner = face == 3 ? face - 1 : face;
            const double midpoint = (inner + .5) * tank.h, shared = face * tank.h;
            const GridVec lo{.5, .5, 0.}, hi{2.5, waterTop, 0.};
            double lowArea = 0., highArea = 0., midArea = 0.;
            bool measured = true;
            for (int x = 4; x < 20 && measured; ++x) {
                double low = 0., high = 0., mid = 0.;
                measured = ClipGridStaticFaceArea(geometry, {x, 4, face - 1}, 2, shared, lo, hi, low, error) &&
                    ClipGridStaticFaceArea(geometry, {x, 4, face}, 2, shared, lo, hi, high, error) &&
                    ClipGridStaticFaceArea(geometry, {x, 4, inner}, 2, midpoint, lo, hi, mid, error);
                lowArea += low;
                highArea += high;
                midArea += mid;
            }
            const double expected = sealed ? 0. : expectedWetArea;
            checks.Expect(measured && std::abs(lowArea - expected) < 2e-16 && lowArea == highArea && lowArea == midArea,
                          "both shared sides and the in-wall midpoint retain the clipped wet seam area", lowArea);

            // The half of this cell next to the measured wall face lies
            // entirely within the Z wall thickness. Its upper phase clip
            // is above the seam, so the entire retained liquid is the gap.
            const GridVec halfLo{1., .5, std::min(midpoint, shared)},
                          halfHi{1.125, .5015, std::max(midpoint, shared)};
            const bool stored = ClipGridStaticCellVolume(geometry, {8, 4, inner}, halfLo, halfHi, water, error);
            const double expectedHalf = sealed ? 0. : .125 * .0625 * gap;
            checks.Expect(stored && std::abs(water - expectedHalf) < 1e-18,
                          "clipped half-cell stores exactly the actual floor-seam water volume", water);
            if (!sealed) {
                const double solidBottom = .5 + gap;
                double wallSurface = -1., justBelow = -1.;
                const bool resolved = ClipGridStaticFaceArea(geometry, {8, 4, inner}, 1, solidBottom,
                    halfLo, halfHi, wallSurface, error) &&
                    ClipGridStaticFaceArea(geometry, {8, 4, inner}, 1, std::nextafter(solidBottom, .5),
                        halfLo, halfHi, justBelow, error);
                checks.Expect(resolved && wallSurface == 0. && justBelow == .125 * .0625,
                              "actual wall underside is closed while the next representable plane in the slit is open",
                              justBelow);
            }
        }
    }
}

} // namespace

int main() {
    Checks checks;
    OverlapAndTouch(checks);
    FreeConnectivity(checks);
    AuthoredFixture(checks);
    AuthoredComposition(checks);
    CachedClips(checks);
    std::printf("SUMMARY static wall union %d checks %d failures\n", checks.total, checks.failed);
    return checks.failed ? 1 : 0;
}
