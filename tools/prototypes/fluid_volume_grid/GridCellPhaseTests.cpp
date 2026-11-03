#include "GridCellPhase.h"

#include <iomanip>
#include <iostream>
#include <limits>
#include <random>

namespace {

using namespace judas::volume_grid;

struct Checks {
    int count = 0;
    int failures = 0;

    void Check(bool passed, const char* description) {
        ++count;
        if (!passed) {
            ++failures;
            std::cerr << "FAIL " << description << '\n';
        }
    }
};

// Independent physical-coordinate intersection, evaluated at a supplied plane
// rather than by inverting the implementation's normalized storage profile.
double ReferenceWater(const GridLayout& layout, const std::vector<BoxMotion>& boxes,
                      std::array<int, 3> cell, int axis, double height) {
    GridVec lo{}, hi{};
    for (int direction = 0; direction < 3; ++direction) {
        lo[direction] = cell[direction] * layout.h;
        hi[direction] = (cell[direction] + 1) * layout.h;
    }
    hi[axis] = std::min(hi[axis], height);
    double result = 1.;
    for (int direction = 0; direction < 3; ++direction)
        result *= std::max(0., hi[direction] - lo[direction]);
    for (const auto& box : boxes) {
        double occupied = 1.;
        for (int direction = 0; direction < 3; ++direction) {
            const double overlap = std::min(hi[direction], box.hi[direction]) -
                                   std::max(lo[direction], box.lo[direction]);
            occupied *= std::max(0., overlap);
        }
        result -= occupied;
    }
    return result;
}

void NonFlatControls(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, .25};
    const std::vector<BoxMotion> boxes{
        {1, {.05, .27, .06}, {.15, .38, .21}, {}},
        {2, {.17, .30, .03}, {.23, .47, .24}, {}},
    };
    const auto cut = layout.Cell(0, 1, 0), left = layout.Cell(1, 1, 0),
               right = layout.Cell(2, 1, 0);
    std::vector<double> water(layout.CellCount());
    water[cut] = .25 * .25 * (.35 - .25) -
                 (.10 * .15 * (.35 - .27) + .06 * .21 * (.35 - .30));
    water[left] = .25 * .25 * (.31 - .25);
    water[right] = .25 * .25 * (.48 - .25);
    GridCellPhase phase;
    MovingGridGeometry geometry;
    std::string error;
    checks.Check(ReconstructGridCellPhase(layout, boxes, 1, water, phase, error) &&
                     BuildMovingGridGeometry(layout, boxes, geometry, error),
                 "reconstruct independent neighboring water planes in cut cells");
    checks.Check(phase.axis == 1 &&
                     std::abs(phase.waterPlane[cut] - .35) < 2e-15 &&
                     std::abs(phase.waterPlane[left] - .31) < 2e-15 &&
                     std::abs(phase.waterPlane[right] - .48) < 2e-15,
                 "two cut bodies in one cell leave three distinct exact heights");
    checks.Check(phase.capacity == geometry.startCapacity,
                 "canonical moving-grid endpoint capacities are unchanged");

    const auto before = phase;
    water[cut] = std::nextafter(phase.capacity[cut], 1.);
    checks.Check(!ReconstructGridCellPhase(layout, boxes, 1, water, phase, error) &&
                     phase.waterPlane == before.waterPlane && phase.capacity == before.capacity,
                 "one representable step above cut capacity is rejected transactionally");
    water[cut] = std::numeric_limits<double>::quiet_NaN();
    checks.Check(!ReconstructGridCellPhase(layout, boxes, 1, water, phase, error) &&
                     phase.waterPlane == before.waterPlane,
                 "NaN stored water is rejected transactionally");
    auto moving = boxes;
    moving[0].translation[0] = .01;
    water[cut] = 0.;
    checks.Check(!ReconstructGridCellPhase(layout, moving, 1, water, phase, error) &&
                     phase.waterPlane == before.waterPlane,
                 "moving geometry cannot be substituted for an endpoint snapshot");
}

void PlateausAndEndpoints(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, .25};
    const std::vector<BoxMotion> slab{
        {3, {.25, .3125, .25}, {.50, .4375, .50}, {}},
        {4, {.75, .25, .25}, {.875, .50, .50}, {}},
    };
    const auto index = layout.Cell(1, 1, 1);
    std::vector<double> water(layout.CellCount());
    water[index] = .25 * .25 * .0625;
    GridCellPhase phase;
    std::string error;
    checks.Check(ReconstructGridCellPhase(layout, slab, 1, water, phase, error) &&
                     phase.waterPlane[index] == .3125,
                 "partially filled cell stops at the first height on a solid plateau");
    water[index] += .25 * .25 * .03125;
    checks.Check(ReconstructGridCellPhase(layout, slab, 1, water, phase, error) &&
                     phase.waterPlane[index] == .46875,
                 "extra water crosses a zero-capacity interval before raising the plane");
    water[index] = phase.capacity[index];
    checks.Check(ReconstructGridCellPhase(layout, slab, 1, water, phase, error) &&
                     phase.waterPlane[index] == .50,
                 "full cut cell has exact upper height");
    water[index] = 0.;
    const BoxMotion closed{3, {.25, .25, .25}, {.50, .50, .50}, {}};
    checks.Check(ReconstructGridCellPhase(layout, {closed}, 1, water, phase, error) &&
                     phase.capacity[index] == 0. && phase.waterPlane[index] == .25,
                 "closed cell cannot invent a wet height");
    water[index] = std::numeric_limits<double>::denorm_min();
    checks.Check(!ReconstructGridCellPhase(layout, {closed}, 1, water, phase, error) &&
                     phase.capacity[index] == 0. && phase.waterPlane[index] == .25,
                 "closed cell rejects even a minimal nonzero volume");
}

void IndependentVolumes(Checks& checks) {
    const GridLayout layout{{5, 6, 7}, .125};
    const std::vector<BoxMotion> boxes{
        {4, {.09375, .15625, .109375}, {.21875, .390625, .4375}, {}},
        {5, {.28125, .34375, .25}, {.5625, .65625, .71875}, {}},
    };
    std::mt19937 rng(0xa5ed82f1);
    const auto random = [&] { return (double(rng()) + .5) / 4294967296.; };
    std::vector<double> water(layout.CellCount()), expected(layout.CellCount());
    GridCellPhase phase;
    std::string error;
    double maxHeight = 0., maxVolume = 0.;
    std::size_t measured = 0;
    for (int up = 0; up < 3; ++up) {
        for (int sample = 0; sample < 32; ++sample) {
            for (int k = 0; k < layout.cells[2]; ++k) {
                for (int j = 0; j < layout.cells[1]; ++j) {
                    for (int i = 0; i < layout.cells[0]; ++i) {
                        const std::array<int, 3> cell{i, j, k};
                        const auto id = layout.Cell(i, j, k);
                        expected[id] = (cell[up] + .1 + .8 * random()) * layout.h;
                        water[id] = ReferenceWater(layout, boxes, cell, up, expected[id]);
                    }
                }
            }
            if (!ReconstructGridCellPhase(layout, boxes, up, water, phase, error)) {
                checks.Check(false, "all independent cut-cell water volumes are admitted");
                std::cerr << error << '\n';
                return;
            }
            for (int k = 0; k < layout.cells[2]; ++k) {
                for (int j = 0; j < layout.cells[1]; ++j) {
                    for (int i = 0; i < layout.cells[0]; ++i) {
                        const auto id = layout.Cell(i, j, k);
                        maxVolume = std::max(maxVolume, std::abs(water[id] -
                            ReferenceWater(layout, boxes, {i, j, k}, up, phase.waterPlane[id])));
                        if (water[id] != 0. && water[id] != phase.capacity[id])
                            maxHeight = std::max(maxHeight,
                                                 std::abs(phase.waterPlane[id] - expected[id]));
                        ++measured;
                    }
                }
            }
        }
    }
    std::cout << std::setprecision(17) << measured << " independent volumes across three axes, "
              << "maximum reconstruction error " << maxVolume << " m^3, plane error "
              << maxHeight << " m\n";
    checks.Check(maxVolume < 2e-15 && maxHeight < 2e-15,
                 "non-flat cell fields roundtrip their individual storage");
}

void LocalFaceControls(Checks& checks) {
    const GridLayout layout{{4, 4, 4}, .25};
    std::vector<double> planes(layout.CellCount());
    for (int k = 0; k < layout.cells[2]; ++k)
        for (int j = 0; j < layout.cells[1]; ++j)
            for (int i = 0; i < layout.cells[0]; ++i)
                planes[layout.Cell(i, j, k)] = j * layout.h;
    std::array<std::vector<double>, 3> left, right;
    std::string error;

    planes[layout.Cell(1, 1, 1)] = .42;
    planes[layout.Cell(2, 1, 1)] = .33;
    const auto jump = layout.Face(0, 2, 1, 1);
    checks.Check(AverageCellPhaseApertures(layout, {}, 1, planes, left, right, error) &&
                     std::abs(left[0][jump] - .0425) < 2e-15 &&
                     std::abs(right[0][jump] - .02) < 2e-15,
                 "stationary lateral face has distinct wet aperture on either side");

    planes[layout.Cell(1, 1, 1)] = .50;
    planes[layout.Cell(0, 0, 0)] = .125;
    planes[layout.Cell(0, 3, 0)] = 1.;
    const auto upward = layout.Face(1, 1, 2, 1);
    const auto bottom = layout.Face(1, 0, 0, 0), top = layout.Face(1, 0, 4, 0);
    checks.Check(AverageCellPhaseApertures(layout, {}, 1, planes, left, right, error) &&
                     left[1][upward] == .0625 && right[1][upward] == 0.,
                 "full donor is wet at upper face beside an empty receiver");
    checks.Check(left[1][bottom] == 0. && right[1][bottom] == .0625 &&
                     left[1][top] == .0625 && right[1][top] == 0.,
                 "missing exterior side contributes zero aperture");
    planes[layout.Cell(1, 1, 1)] = .49;
    planes[layout.Cell(1, 2, 1)] = std::nextafter(.50, .75);
    checks.Check(AverageCellPhaseApertures(layout, {}, 1, planes, left, right, error) &&
                     left[1][upward] == 0. && right[1][upward] == .0625,
                 "a representable positive wet layer owns its lower face");

    planes[layout.Cell(1, 2, 1)] = .62;
    planes[layout.Cell(2, 2, 1)] = .68;
    const BoxMotion crossing{11, {.40, .51, .24}, {.60, .59, .51}, {0., .20, 0.}};
    const auto clipped = layout.Face(0, 2, 2, 1);
    checks.Check(AverageCellPhaseApertures(layout, {crossing}, 1, planes, left, right, error) &&
                     std::abs(left[0][clipped] - .023) < 2e-15 &&
                     std::abs(right[0][clipped] - .032) < 2e-15,
                 "moving box crosses both side planes with independent analytic area integrals");

    const auto originalLeft = left, originalRight = right;
    planes[layout.Cell(1, 2, 1)] = .76;
    checks.Check(!AverageCellPhaseApertures(layout, {crossing}, 1, planes, left, right, error) &&
                     left == originalLeft && right == originalRight,
                 "out-of-cell wet plane preserves both aperture arrays");
}

} // namespace

int main() {
    Checks checks;
    NonFlatControls(checks);
    PlateausAndEndpoints(checks);
    IndependentVolumes(checks);
    LocalFaceControls(checks);
    std::cout << checks.count << " cell phase checks, " << checks.failures << " failures\n";
    return checks.failures ? 1 : 0;
}
