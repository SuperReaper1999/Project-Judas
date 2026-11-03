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

void PhaseBirth(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, .25};
    const BoxMotion piston{1, {.25, .5, .25}, {.75, .875, .75}, {0., .04, 0.}};
    auto planes = std::vector<double>(layout.CellCount());
    grid_phase_detail::VisitGrid(layout, -1, [&](const auto& cell) {
        planes[layout.Cell(cell[0], cell[1], cell[2])] = (cell[1] + .5) * layout.h;
    });
    const auto left = layout.Cell(1, 2, 1), right = layout.Cell(2, 2, 1);
    planes[left] = .52;
    planes[right] = .51;
    GridHalfCellApertures aperture;
    GridHalfCellStorage before, after;
    auto start = piston, end = piston;
    start.translation = {};
    end.lo[1] += piston.translation[1];
    end.hi[1] += piston.translation[1];
    end.translation = {};
    std::string error;
    const bool built = BuildGridHalfCellStorage(layout, {start}, 1, 1, planes, before, error) &&
        BuildGridHalfCellStorage(layout, {end}, 1, 1, planes, after, error) &&
        AverageGridHalfCellApertures(layout, {piston}, 1, 1, planes, aperture, error);
    checks.Expect(built, built ? "swept birth has cut-cell phase geometry" : error.c_str());
    if (!built)
        return;
    checks.Expect(before.capacity[left] == std::array<double, 2>{} &&
                  before.water[left] == std::array<double, 2>{} &&
                  std::abs(after.capacity[left][0] - .0025) < 1e-16 &&
                  std::abs(after.water[left][0] - .00125) < 1e-16 &&
                  after.capacity[left][1] == 0. && after.water[left][1] == 0.,
                  "newborn water and air occupy only the lower half");

    const auto face = layout.Face(0, 2, 2, 1);
    const auto& birth = aperture.transverse[0][face];
    const auto& middle = aperture.midpoint[left];
    checks.Expect(std::abs(birth[0].open - .005) < 1e-16 &&
                  std::abs(birth[0].leftWet - .00375) < 1e-16 &&
                  std::abs(birth[0].rightWet - .0021875) < 1e-16,
                  "time-averaged slit opening and independent wet traces match analytic integrals");
    checks.Expect(birth[1].open == 0. && birth[1].leftWet == 0. && birth[1].rightWet == 0. &&
                  middle.open == 0. && middle.leftWet == 0. && middle.rightWet == 0.,
                  "closed upper slit and midpoint cannot supply either phase");

    end.translation[1] = -piston.translation[1];
    GridHalfCellApertures reversed;
    const bool reverse = AverageGridHalfCellApertures(layout, {end}, 1, 1, planes, reversed, error);
    checks.Expect(reverse &&
                  std::abs(reversed.transverse[0][face][0].open - birth[0].open) < 1e-16 &&
                  std::abs(reversed.transverse[0][face][0].leftWet - birth[0].leftWet) < 1e-16 &&
                  std::abs(reversed.transverse[0][face][0].rightWet - birth[0].rightWet) < 1e-16,
                  "reversing the solid path preserves both time-averaged wet traces");

    GridHalfCellApertures still;
    const bool rest = AverageGridHalfCellApertures(layout, {}, 1, 1, planes, still, error);
    const auto outside = layout.Cell(0, 2, 1);
    checks.Expect(rest && still.midpoint[outside].open == .0625 &&
                  still.midpoint[outside].leftWet == .0625 && still.midpoint[outside].rightWet == 0.,
                  "exact midpoint water plane has wet lower and dry upper traces");
    const auto boundary = layout.Face(0, 0, 2, 1);
    checks.Expect(rest && still.transverse[0][boundary][0].open == .03125 &&
                  still.transverse[0][boundary][0].leftWet == 0. &&
                  still.transverse[0][boundary][0].rightWet == .03125,
                  "missing boundary owner contributes no wet trace");

    planes[left] = std::nextafter(.75, 1.);
    const double storedWater = after.water[left][0], storedOpening = birth[0].open;
    checks.Expect(!BuildGridHalfCellStorage(layout, {start}, 1, 1, planes, after, error) &&
                  after.water[left][0] == storedWater &&
                  !AverageGridHalfCellApertures(layout, {piston}, 1, 1, planes, aperture, error) &&
                  aperture.transverse[0][face][0].open == storedOpening,
                  "out-of-cell phase rejects without modifying either geometry output");
}

double ApertureError(const GridHalfCellAperture& first, const GridHalfCellAperture& second) {
    return std::max({std::abs(first.open - second.open), std::abs(first.leftWet - second.leftWet),
                     std::abs(first.rightWet - second.rightWet)});
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
    std::vector<double> finePlanes(refined.CellCount());
    grid_phase_detail::VisitGrid(refined, -1, [&](const auto& cell) {
        const auto parentCell = layout.Cell(cell[0] / 2, cell[1] / 2, cell[2] / 2);
        finePlanes[refined.Cell(cell[0], cell[1], cell[2])] = std::clamp(phase.waterPlane[parentCell],
            cell[1] * refined.h, (cell[1] + 1) * refined.h);
    });
    std::array<std::vector<double>, 3> fineLeft, fineRight;
    const bool finePhase = AverageCellPhaseApertures(refined, boxes, 1, finePlanes,
                                                    fineLeft, fineRight, error);
    checks.Expect(finePhase, finePhase ? "reference fine-grid wet apertures build" : error.c_str());
    if (!finePhase)
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
        GridHalfCellStorage storage;
        GridHalfCellApertures phaseAperture;
        const bool phaseBuilt = BuildGridHalfCellStorage(layout, stationary, 1, axis,
            phase.waterPlane, storage, error) && AverageGridHalfCellApertures(layout, boxes, 1, axis,
            phase.waterPlane, phaseAperture, error);
        checks.Expect(phaseBuilt, phaseBuilt ? "wet half-face cuts build in every component" : error.c_str());
        if (!phaseBuilt)
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
        double capacityError = 0, areaError = 0, parentError = 0, phaseAreaError = 0, storageError = 0;
        const auto fineFace = [&](int normal, const auto& child, GridHalfCellAperture& sum) {
            const auto face = refined.Face(normal, child[0], child[1], child[2]);
            sum.open += fine.averageAperture[normal][face];
            sum.leftWet += fineLeft[normal][face];
            sum.rightWet += fineRight[normal][face];
        };
        for (int z = 0; z < layout.cells[2]; ++z) {
            for (int y = 0; y < layout.cells[1]; ++y) {
                for (int x = 0; x < layout.cells[0]; ++x) {
                    const auto cell = layout.Cell(x, y, z);
                    GridHalfCellAperture midpoint;
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
                                    fineFace(axis, child, midpoint);
                            }
                        }
                        capacityError = std::max({capacityError,
                            std::abs(start - half.startCapacity[cell][side]),
                            std::abs(end - half.endCapacity[cell][side])});
                        storageError = std::max({storageError,
                            std::abs(start - storage.capacity[cell][side]),
                            std::abs(wet.volume[cell][side] - storage.water[cell][side])});
                    }
                    areaError = std::max(areaError, std::abs(midpoint.open - half.midpointAperture[cell]));
                    phaseAreaError = std::max(phaseAreaError, ApertureError(midpoint, phaseAperture.midpoint[cell]));
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
                            GridHalfCellAperture area;
                            fineFace(normal, child, area);
                            ++child[other];
                            fineFace(normal, child, area);
                            areaError = std::max(areaError, std::abs(area.open - pair[side]));
                            phaseAreaError = std::max(phaseAreaError,
                                ApertureError(area, phaseAperture.transverse[normal][f][side]));
                        }
                    }
                }
            }
        }
        checks.Expect(capacityError < 2e-16, "half endpoint capacities agree with fine-cell cuts", capacityError);
        checks.Expect(areaError < 2e-16, "midpoint and transverse apertures agree with fine face cuts", areaError);
        checks.Expect(parentError < 2e-16, "exact child cuts sum to their primal control volume", parentError);
        checks.Expect(storageError < 2e-16, "endpoint phase storage agrees with independently refined cuts", storageError);
        checks.Expect(phaseAreaError < 2e-16, "half wet apertures agree with both fine-grid interior traces", phaseAreaError);
    }
}

} // namespace

int main() {
    Checks checks;
    Piston(checks);
    PhaseBirth(checks);
    CompareRefinedGrid(checks);
    std::printf("SUMMARY grid half-cell geometry %d checks %d failures\n", checks.total, checks.failed);
    return checks.failed ? 1 : 0;
}
