#include "GridStaticMovingPhase.h"

#include <chrono>
#include <cstdio>

namespace {

using namespace judas::volume_grid;
using Index = std::array<int, 3>;

struct Checks {
    int total = 0, failed = 0;
    void Expect(bool pass, const char* label, double value = 0.) {
        ++total;
        failed += !pass;
        std::printf("%s %s %.17g\n", pass ? "PASS" : "FAIL", label, value);
    }
};

void MixedCell(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, 1.};
    const BoxMotion wall{1, {1.125, 1., 1.125}, {2.25, 1.75, 1.375}, {}},
                    snapshot{2, {1.625, 1.25, 1.5}, {1.875, 1.75, 1.875}, {}};
    auto moving = snapshot;
    moving.translation[0] = .5;
    const auto left = layout.Cell(1, 1, 1), right = layout.Cell(2, 1, 1);
    const auto shared = layout.Face(0, 2, 1, 1);
    std::vector<double> water(layout.CellCount());
    water[left] = .3671875;
    water[right] = .5859375;
    GridStaticUnion fixed;
    GridCellPhase phase;
    GridHalfCellStorage half;
    GridHalfCellApertures halfOpen, crossOpen;
    std::array<std::vector<GridHalfCellAperture>, 3> open;
    std::string error;
    const bool built = BuildGridStaticUnion(layout, {wall}, fixed, error) &&
        ReconstructStaticMovingPhase(fixed, {snapshot}, 1, water, phase, error) &&
        BuildStaticMovingHalfStorage(fixed, {snapshot}, phase, 0, half, error) &&
        AverageStaticMovingWetApertures(fixed, {moving}, phase, open, error) &&
        AverageStaticMovingHalfApertures(fixed, {moving}, phase, 1, halfOpen, error) &&
        AverageStaticMovingHalfApertures(fixed, {moving}, phase, 0, crossOpen, error);
    checks.Expect(built, built ? "build disjoint moving and fixed cuts sharing one coarse cell" : error.c_str());
    if (!built)
        return;
    checks.Expect(phase.waterPlane[left] == 1.5 && phase.waterPlane[right] == 1.625,
                  "different analytic cell volumes reconstruct independent heights", phase.waterPlane[left]);
    checks.Expect(half.capacity[left] == std::array<double, 2>{.4296875, .359375} &&
                  half.water[left] == std::array<double, 2>{.203125, .1640625},
                  "half capacities and water match separate fixed and moving intersections", half.water[left][1]);
    const auto& face = open[0][shared];
    checks.Expect(face.open == .71875 && face.leftWet == .328125 && face.rightWet == .3984375,
                  "shared face keeps fixed cut and exact moving-time average for each phase trace", face.open);
    const auto& halves = halfOpen.transverse[0][shared];
    checks.Expect(halves[0].open + halves[1].open == face.open &&
                  halves[0].leftWet + halves[1].leftWet == face.leftWet &&
                  halves[0].rightWet + halves[1].rightWet == face.rightWet &&
                  halves[1].leftWet == 0. && halves[1].rightWet == .0703125,
                  "transverse halves retain an open wet/dry difference across one shared face", halves[1].rightWet);
    const auto& midpoint = crossOpen.midpoint[left];
    checks.Expect(midpoint.open == .8125 && midpoint.leftWet == .375 && midpoint.rightWet == .375,
                  "local midpoint excludes its fixed wall while the moving box stays clear", midpoint.open);
    const auto saved = phase.waterPlane;
    water[left] = std::nextafter(phase.capacity[left], 1.);
    checks.Expect(!ReconstructStaticMovingPhase(fixed, {snapshot}, 1, water, phase, error) &&
                  phase.waterPlane == saved, "over-capacity inventory rejects transactionally");
    auto wallHit = moving;
    wallHit.translation[2] = -.25;
    checks.Expect(!AverageStaticMovingWetApertures(fixed, {wallHit}, phase, open, error) &&
                  error.find("touches static") != std::string::npos && open[0][shared].open == .71875,
                  "wall-crossing path rejects wet aperture publication");
}

std::vector<BoxMotion> Walls(bool sealed) {
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
    box({0.f, -.1f, 0.f}, {1.1f, .1f, 1.1f});
    box({-1.1f, 1.5f, 0.f}, {.1f, 1.5f, 1.1f});
    box({1.1f, 1.5f, 0.f}, {.1f, 1.5f, 1.1f});
    const float y = sealed ? 1.5f : 1.499f;
    box({0.f, 1.5f, -1.1f}, {1.f, y, .1f});
    box({0.f, 1.5f, 1.1f}, {1.f, y, .1f});
    return boxes;
}

long double Volume(const std::array<long double, 3>& lo, const std::array<long double, 3>& hi) {
    long double result = 1.;
    for (int axis = 0; axis < 3; ++axis)
        result *= std::max(0.L, hi[axis] - lo[axis]);
    return result;
}

double IndependentWater(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                        const Index& cell, double height) {
    std::array<long double, 3> lo{}, hi{};
    for (int axis = 0; axis < 3; ++axis) {
        lo[axis] = cell[axis] * layout.h;
        hi[axis] = (cell[axis] + 1) * layout.h;
    }
    hi[1] = std::min(hi[1], static_cast<long double>(height));
    long double water = Volume(lo, hi);
    if (water == 0.)
        return 0.;
    // Inclusion-exclusion gives an independent small-set union oracle.
    for (unsigned set = 1; set < (1u << boxes.size()); ++set) {
        auto low = lo, high = hi;
        bool odd = false;
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            if ((set & (1u << i)) == 0)
                continue;
            odd = !odd;
            for (int axis = 0; axis < 3; ++axis) {
                low[axis] = std::max(low[axis], static_cast<long double>(boxes[i].lo[axis]));
                high[axis] = std::min(high[axis], static_cast<long double>(boxes[i].hi[axis]));
            }
        }
        water += (odd ? -1. : 1.) * Volume(low, high);
    }
    return double(water);
}

void Pool(Checks& checks, bool sealed, bool seamOnly) {
    const GridLayout layout{{24, 32, 24}, .125};
    constexpr double half = double(.3f), y = double(.7f) + .5;
    const BoxMotion body{6, {1.5-half, y-half, 1.5-half}, {1.5+half, y+half, 1.5+half}, {}};
    auto moving = body;
    moving.translation[1] = 1.25;
    const double waterline = seamOnly ? .5005 : 1.7094379663467407 + .5;
    const auto walls = Walls(sealed);
    auto analyticBoxes = walls;
    analyticBoxes.push_back(body);
    GridStaticUnion fixed;
    MovingGridGeometry geometry;
    std::string error;
    const bool built = BuildGridStaticUnion(layout, walls, fixed, error) &&
        BuildGridStaticMovingUnion(fixed, {moving}, geometry, error);
    checks.Expect(built, built ? "build authored walls and reference release body" : error.c_str());
    if (!built)
        return;
    std::vector<double> water(layout.CellCount());
    double sourceError = 0.;
    grid_phase_detail::VisitGrid(layout, -1, [&](const Index& cell) {
        const auto c = layout.Cell(cell[0], cell[1], cell[2]);
        const double exact = IndependentWater(layout, analyticBoxes, cell, waterline);
        water[c] = std::clamp(exact, 0., geometry.startCapacity[c]);
        sourceError = std::max(sourceError, std::abs(exact - water[c]));
    });
    checks.Expect(sourceError < 1e-17, "independent union inventory fits canonical composed capacity", sourceError);

    GridCellPhase phase;
    std::array<std::vector<GridHalfCellAperture>, 3> open;
    const auto begin = std::chrono::steady_clock::now();
    const bool reconstructed = ReconstructStaticMovingPhase(fixed, {body}, 1, water, phase, error) &&
        AverageStaticMovingWetApertures(fixed, {moving}, phase, open, error);
    checks.Expect(reconstructed, reconstructed ? "reconstruct actual pool phase and swept wet apertures" : error.c_str());
    if (!reconstructed)
        return;
    double maxHeight = 0., maxOpening = 0.;
    std::size_t partial = 0;
    grid_phase_detail::VisitGrid(layout, -1, [&](const Index& cell) {
        const auto c = layout.Cell(cell[0], cell[1], cell[2]);
        if (water[c] > 0. && water[c] + 1e-15 < phase.capacity[c]) {
            ++partial;
            maxHeight = std::max(maxHeight, std::abs(phase.waterPlane[c] - waterline));
        }
    });
    for (int axis = 0; axis < 3; ++axis)
        for (std::size_t f = 0; f < open[axis].size(); ++f)
            maxOpening = std::max(maxOpening, std::abs(open[axis][f].open - geometry.averageAperture[axis][f]));
    checks.Expect(partial > 0 && maxHeight < 1e-12 && phase.capacity == geometry.startCapacity,
                  "independent partial pool plane and canonical capacities match every cell", maxHeight);
    checks.Expect(maxOpening < 1e-16, "wet geometry preserves the pressure operator's total openings", maxOpening);

    for (int split = 0; split < 3; ++split) {
        GridHalfCellStorage storage;
        GridHalfCellApertures apertures;
        const bool cut = BuildStaticMovingHalfStorage(fixed, {body}, phase, split, storage, error) &&
            AverageStaticMovingHalfApertures(fixed, {moving}, phase, split, apertures, error);
        checks.Expect(cut, cut ? "build all actual pool half-storage and wet openings" : error.c_str());
        if (!cut)
            continue;
        double storageError = 0., splitError = 0.;
        for (std::size_t c = 0; c < water.size(); ++c) {
            storageError = std::max({storageError,
                std::abs(storage.capacity[c][0] + storage.capacity[c][1] - phase.capacity[c]),
                std::abs(storage.water[c][0] + storage.water[c][1] - water[c])});
        }
        for (int normal = 0; normal < 3; ++normal) {
            if (normal == split)
                continue;
            for (std::size_t f = 0; f < open[normal].size(); ++f) {
                const auto& pair = apertures.transverse[normal][f];
                const auto& whole = open[normal][f];
                splitError = std::max({splitError, std::abs(pair[0].open + pair[1].open - whole.open),
                    std::abs(pair[0].leftWet + pair[1].leftWet - whole.leftWet),
                    std::abs(pair[0].rightWet + pair[1].rightWet - whole.rightWet)});
            }
        }
        checks.Expect(storageError < 1e-16 && splitError < 1e-16,
                      "half owners exactly partition primal capacities, volumes and transverse wet areas", splitError);
    }
    for (int z : {3, 21}) {
        double left = 0., right = 0.;
        for (int x = 4; x < 20; ++x) {
            const auto& a = open[2][layout.Face(2, x, 4, z)];
            left += a.leftWet;
            right += a.rightWet;
        }
        const double gap = 1.5 - double(1.499f);
        const double expected = sealed ? 0. : 2. * std::min(gap, waterline - .5);
        checks.Expect(std::abs(left - expected) < 1e-14 && std::abs(right - expected) < 1e-14,
                      "both donor wet traces preserve the analytic authored or sealed seam", left);
    }
    const auto end = std::chrono::steady_clock::now();
    std::printf("GEOMETRY %s %s cells=%zu phase_and_half_ms=%.6f\n", sealed ? "sealed" : "authored",
        seamOnly ? "seam-partial-fill" : "release-surface", layout.CellCount(),
        std::chrono::duration<double, std::milli>(end - begin).count());

    if (!seamOnly) {
        auto endpoint = body;
        endpoint.lo[1] += moving.translation[1];
        endpoint.hi[1] += moving.translation[1];
        analyticBoxes.back() = endpoint;
        // Prescribed fixed-plane endpoint geometry, independent of any
        // putative transport. A genuine run must supply its transported water.
        grid_phase_detail::VisitGrid(layout, -1, [&](const Index& cell) {
            const auto c = layout.Cell(cell[0], cell[1], cell[2]);
            water[c] = std::clamp(IndependentWater(layout, analyticBoxes, cell, waterline),
                                  0., geometry.endCapacity[c]);
        });
        const bool rebuilt = ReconstructStaticMovingPhase(fixed, {endpoint}, 1, water, phase, error);
        checks.Expect(rebuilt && phase.capacity == geometry.endCapacity,
                      rebuilt ? "reconstruct the moved body endpoint against actual end capacities" : error.c_str());
        if (rebuilt) {
            const auto edgeCell = layout.Cell(9, 17, 9);
            checks.Expect(water[edgeCell] > 0. && water[edgeCell] < phase.capacity[edgeCell] &&
                          std::abs(phase.waterPlane[edgeCell] - waterline) < 1e-14,
                          "partially wet endpoint cell containing both water and moved solid recovers its plane",
                          phase.waterPlane[edgeCell]);
        }
    }
}

} // namespace

int main() {
    Checks checks;
    MixedCell(checks);
    Pool(checks, false, true);
    Pool(checks, true, true);
    Pool(checks, false, false);
    std::printf("SUMMARY static/moving phase geometry %d checks %d failures\n", checks.total, checks.failed);
    return checks.failed ? 1 : 0;
}
