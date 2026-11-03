#include "GridHalfCellGeometry.h"

#include <cstdio>

namespace {

using namespace judas::volume_grid;

struct Checks {
    int total = 0, failed = 0;
    void Expect(bool pass, const char* label, double value = 0) {
        ++total;
        if (!pass)
            ++failed;
        std::printf("%s %s %.17g\n", pass ? "PASS" : "FAIL", label, value);
    }
};

void Piston(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, .25};
    const auto cell = layout.Cell(1, 2, 1);
    const BoxMotion piston{1, {.25, .5, .25}, {.75, .875, .75}, {0., .04, 0.}};
    GridHalfCellGeometry geometry;
    std::string error;
    const bool built = BuildGridHalfCellGeometry(layout, {piston}, 1, geometry, error);
    checks.Expect(built, built ? "translating piston cuts actual half cells" : error.c_str());
    if (!built)
        return;
    constexpr double volume = .25 * .25 * .04;
    checks.Expect(geometry.startCapacity[cell] == std::array<double, 2>{} &&
                  std::abs(geometry.endCapacity[cell][0] - volume) < 1e-16 &&
                  geometry.endCapacity[cell][1] == 0. && geometry.midpointAperture[cell] == 0.,
                  "new volume lies below a midpoint that remains solid", geometry.endCapacity[cell][0]);
    double midpointFlux = -1.;
    const bool acceptsInflow = BalanceGridHalfCell(geometry, cell, {volume, 0., {}},
                                                  midpointFlux, error);
    checks.Expect(acceptsInflow && midpointFlux == 0.,
                  "known single inlet fills the true owner without midpoint flux", midpointFlux);
    midpointFlux = 42.;
    const bool acceptsFalseFlow = BalanceGridHalfCell(geometry, cell, {.5 * volume, 0., {0., .5 * volume}},
                                                     midpointFlux, error);
    checks.Expect(!acceptsFalseFlow && error.find("solid midpoint") != std::string::npos &&
                  midpointFlux == 42., "same total inflow to the wrong halves is rejected");
    const bool acceptsImbalance = BalanceGridHalfCell(geometry, cell, {.9 * volume, 0., {}},
                                                     midpointFlux, error);
    checks.Expect(!acceptsImbalance && error.find("volume balance") != std::string::npos &&
                  midpointFlux == 42., "half-cell volume imbalance rejects transactionally");

    auto reverse = piston;
    reverse.lo[1] += reverse.translation[1];
    reverse.hi[1] += reverse.translation[1];
    reverse.translation[1] *= -1;
    const bool reversed = BuildGridHalfCellGeometry(layout, {reverse}, 1, geometry, error) &&
        BalanceGridHalfCell(geometry, cell, {-volume, 0., {}}, midpointFlux, error);
    checks.Expect(reversed && midpointFlux == 0. && geometry.endCapacity[cell][0] == 0.,
                  "reversed piston evacuates precisely the disappearing lower volume");

    reverse.translation = {};
    GridPhaseGeometry fill;
    GridCellPhase phase;
    GridHalfCellWater vertical, across;
    const bool wetCut = BuildGridPhaseGeometry(layout, {reverse}, 1, .52, fill, error) &&
        ReconstructGridCellPhase(layout, {reverse}, 1, fill.water, phase, error) &&
        BuildGridHalfCellWater(layout, {reverse}, phase, 1, vertical, error) &&
        BuildGridHalfCellWater(layout, {reverse}, phase, 0, across, error);
    checks.Expect(wetCut, wetCut ? "piston water and air have separate geometric owners" : error.c_str());
    if (wetCut) {
        checks.Expect(std::abs(vertical.volume[cell][0] - .5 * volume) < 1e-16 &&
                      vertical.volume[cell][1] == 0.,
                      "wet and dry layers fit below the solid midpoint", vertical.volume[cell][0]);
        checks.Expect(std::abs(across.volume[cell][0] - .25 * volume) < 1e-16 &&
                      std::abs(across.volume[cell][1] - .25 * volume) < 1e-16,
                      "crosswise momentum owners clip the same horizontal water layer");
    }
}

void CompareRefinedGrid(Checks& checks) {
    const GridLayout layout{{4, 5, 6}, .25}, refined{{8, 10, 12}, .125};
    const std::vector<BoxMotion> boxes{{2, {.145, .24, .23}, {.62, .71, .84}, {.1, .08, .065}},
                                       {3, {.82, .12, 1.08}, {.94, .62, 1.27}, {-.06, .03, .08}}};
    MovingGridGeometry parent, fine;
    std::string error;
    const bool built = BuildMovingGridGeometry(layout, boxes, parent, error) &&
        BuildMovingGridGeometry(refined, boxes, fine, error);
    checks.Expect(built, built ? "reference primal and fine cuts build" : error.c_str());
    if (!built)
        return;
    auto stationary = boxes;
    for (auto& box : stationary)
        box.translation = {};
    auto water = parent.startCapacity;
    for (std::size_t cell = 0; cell < water.size(); ++cell)
        water[cell] *= .2 + .6 * double(cell % 17) / 16.;
    GridCellPhase phase;
    const bool reconstructed = ReconstructGridCellPhase(layout, stationary, 1, water, phase, error);
    checks.Expect(reconstructed, reconstructed ? "nonflat phase inventory reconstructs" : error.c_str());
    if (!reconstructed)
        return;
    for (int axis = 0; axis < 3; ++axis) {
        GridHalfCellGeometry half;
        GridHalfCellWater wet;
        const bool cut = BuildGridHalfCellGeometry(layout, boxes, axis, half, error);
        checks.Expect(cut, cut ? "half-grid cuts build in every component" : error.c_str());
        if (!cut)
            continue;
        const bool split = BuildGridHalfCellWater(layout, stationary, phase, axis, wet, error);
        checks.Expect(split, split ? "nonflat water splits into actual half volumes" : error.c_str());
        if (!split)
            continue;
        double wetError = 0., excess = 0.;
        for (std::size_t cell = 0; cell < water.size(); ++cell) {
            wetError = std::max(wetError, std::abs(water[cell] - wet.volume[cell][0] - wet.volume[cell][1]));
            for (int side = 0; side < 2; ++side)
                excess = std::max(excess, wet.volume[cell][side] - half.startCapacity[cell][side]);
        }
        checks.Expect(wetError < 2e-16 && excess < 2e-16,
                      "water and remaining air fit their actual half-cell capacities", wetError);
        const int b = (axis + 1) % 3, c = (axis + 2) % 3;
        double capacityError = 0, areaError = 0, parentError = 0;
        for (int z = 0; z < layout.cells[2]; ++z) {
            for (int y = 0; y < layout.cells[1]; ++y) {
                for (int x = 0; x < layout.cells[0]; ++x) {
                    const auto cell = layout.Cell(x, y, z);
                    double midpoint = 0;
                    for (int side = 0; side < 2; ++side) {
                        double start = 0, end = 0;
                        for (int i = 0; i < 2; ++i) {
                            for (int j = 0; j < 2; ++j) {
                                std::array<int, 3> child{2*x, 2*y, 2*z};
                                child[axis] += side;
                                child[b] += i;
                                child[c] += j;
                                const auto ref = refined.Cell(child[0], child[1], child[2]);
                                start += fine.startCapacity[ref];
                                end += fine.endCapacity[ref];
                                if (side == 1)
                                    midpoint += fine.averageAperture[axis][refined.Face(
                                        axis, child[0], child[1], child[2])];
                            }
                        }
                        capacityError = std::max({capacityError,
                            std::abs(start - half.startCapacity[cell][side]),
                            std::abs(end - half.endCapacity[cell][side])});
                    }
                    areaError = std::max(areaError, std::abs(midpoint - half.midpointAperture[cell]));
                    const auto& end = half.endCapacity[cell];
                    parentError = std::max(parentError, std::abs(end[0] + end[1] - parent.endCapacity[cell]));
                }
            }
        }
        for (int normal : {b, c}) {
            auto limits = layout.cells;
            ++limits[normal];
            const int other = normal == b ? c : b;
            for (int z = 0; z < limits[2]; ++z) {
                for (int y = 0; y < limits[1]; ++y) {
                    for (int x = 0; x < limits[0]; ++x) {
                        const auto f = layout.Face(normal, x, y, z);
                        const auto pair = half.transverseAperture[normal][f];
                        parentError = std::max(parentError,
                            std::abs(pair[0] + pair[1] - parent.averageAperture[normal][f]));
                        for (int side = 0; side < 2; ++side) {
                            std::array<int, 3> child{2*x, 2*y, 2*z};
                            child[axis] += side;
                            double area = fine.averageAperture[normal][refined.Face(
                                normal, child[0], child[1], child[2])];
                            ++child[other];
                            area += fine.averageAperture[normal][refined.Face(
                                normal, child[0], child[1], child[2])];
                            areaError = std::max(areaError, std::abs(area - pair[side]));
                        }
                    }
                }
            }
        }
        checks.Expect(capacityError < 2e-16, "half endpoint capacities agree with fine-cell cuts", capacityError);
        checks.Expect(areaError < 2e-16, "midpoint and transverse apertures agree with fine face cuts", areaError);
        checks.Expect(parentError < 2e-16, "exact child cuts sum to their primal control volume", parentError);
    }
}

} // namespace

int main() {
    Checks checks;
    Piston(checks);
    CompareRefinedGrid(checks);
    std::printf("SUMMARY grid half-cell geometry %d checks %d failures\n", checks.total, checks.failed);
    return checks.failed ? 1 : 0;
}
