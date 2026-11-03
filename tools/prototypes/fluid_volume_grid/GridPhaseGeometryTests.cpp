#include "GridPhaseGeometry.h"

#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>

namespace {

using namespace judas::volume_grid;
using Apertures = std::array<std::vector<double>, 3>;

struct Checks {
    int count = 0;
    int failures = 0;

    void Check(bool passed, const char* message) {
        ++count;
        if (!passed) {
            ++failures;
            std::cerr << "FAIL " << message << '\n';
        }
    }
};

double Sum(const std::vector<double>& values) {
    return std::accumulate(values.begin(), values.end(), 0.);
}

double PhaseClosure(const GridPhaseGeometry& phase, const MovingGridGeometry& moving) {
    double error = 0.;
    for (std::size_t i = 0; i < phase.water.size(); ++i)
        error = std::max(error, std::abs(phase.water[i] + phase.air[i] - moving.startCapacity[i]));
    return error;
}

double ApertureClosure(const GridPhaseGeometry& phase, const MovingGridGeometry& moving) {
    double error = 0.;
    for (int axis = 0; axis < 3; ++axis)
        for (std::size_t i = 0; i < phase.wetAperture[axis].size(); ++i)
            error = std::max(error, std::abs(phase.wetAperture[axis][i] +
                                             phase.dryAperture[axis][i] -
                                             moving.startAperture[axis][i]));
    return error;
}

void ExactControls(Checks& checks) {
    const GridLayout grid{{3, 4, 2}, .25};
    constexpr double level = .62;
    const BoxMotion dry{19, {.13, .624, .12}, {.55, .694, .44}, {}};
    const BoxMotion crossing{19, {.13, .52, .12}, {.55, .72, .44}, {}};
    std::string error;
    GridPhaseGeometry geometry;
    MovingGridGeometry staticGeometry;

    checks.Check(BuildGridPhaseGeometry(grid, {dry}, 1, level, geometry, error),
                 "4 mm dry gap geometry builds");
    checks.Check(geometry.water[grid.Cell(0, 2, 0)] > 0.,
                 "dry box shares a cell with actual water");
    checks.Check(geometry.wetPatches.empty() && !geometry.dryPatches.empty(),
                 "a dry box owns zero liquid wall area");
    checks.Check(std::abs(Sum(geometry.water) - .75 * .50 * level) < 2e-15,
                 "dry box leaves the liquid volume alone");

    checks.Check(BuildGridPhaseGeometry(grid, {crossing}, 1, level, geometry, error) &&
                     BuildMovingGridGeometry(grid, {crossing}, staticGeometry, error),
                 "partial immersion geometry builds");
    checks.Check(std::abs(Sum(geometry.water) - (.75 * .50 * level - .42 * .10 * .32)) < 2e-15,
                 "cell clipping matches independent submerged box volume");
    checks.Check(PhaseClosure(geometry, staticGeometry) < 2e-15 &&
                     ApertureClosure(geometry, staticGeometry) < 2e-15,
                 "both phases recover the total capacity and aperture");
    const auto face = grid.Face(0, 1, 2, 0);
    const double wetArea = .25 * (.62 - .50) - (.25 - .12) * (.62 - .52);
    checks.Check(std::abs(geometry.wetAperture[0][face] - wetArea) < 2e-15,
                 "shared face aperture clips both the water plane and the box");
    double wallArea = 0., firstMoment = 0.;
    for (const auto& rectangle : geometry.wetPatches) {
        const auto& patch = rectangle.patch;
        if (patch.axis != 0 || patch.sign != -1)
            continue;
        wallArea += patch.area;
        firstMoment += patch.area * patch.centroid[1];
    }
    checks.Check(std::abs(wallArea - .32 * .10) < 2e-15 &&
                     std::abs(firstMoment - .32 * .10 * .57) < 2e-15,
                 "wet rectangular patches preserve area and vertical moment");

    double volume = -1., inverse = -1.;
    checks.Check(FlatWaterVolume(grid, {crossing}, 1, level, volume, error) &&
                     FlatWaterHeight(grid, {crossing}, 1, volume, inverse, error) &&
                     std::abs(inverse - level) < 2e-15 &&
                     std::abs(volume - Sum(geometry.water)) < 2e-15,
                 "flat volume and height invert the piecewise clipped capacity");
    checks.Check(std::abs(LiquidLinkLength(.78, .53, .64) - .11) < 2e-15,
                 "chart pressure link integrates both phases without face rounding");
    for (const double endpoint : {0., 1.}) {
        checks.Check(FlatWaterVolume(grid, {crossing}, 1, endpoint, volume, error) &&
                         FlatWaterHeight(grid, {crossing}, 1, volume, inverse, error) &&
                         inverse == endpoint &&
                         BuildGridPhaseGeometry(grid, {crossing}, 1, endpoint, geometry, error),
                     "empty and full phase endpoints are representable");
        const auto& absentCells = endpoint == 0. ? geometry.water : geometry.air;
        const auto& absentFaces = endpoint == 0. ? geometry.wetAperture : geometry.dryAperture;
        checks.Check(Sum(absentCells) == 0. && Sum(absentFaces[0]) == 0. &&
                         Sum(absentFaces[1]) == 0. && Sum(absentFaces[2]) == 0.,
                     "endpoint has zero volume and aperture for absent phase");
    }

    const auto before = geometry.water;
    checks.Check(!BuildGridPhaseGeometry(grid, {crossing}, 1,
                                         std::numeric_limits<double>::quiet_NaN(), geometry, error) &&
                     before == geometry.water,
                 "invalid plane fails without replacing the prior phase");
    auto moving = crossing;
    moving.translation[0] = .01;
    checks.Check(!BuildGridPhaseGeometry(grid, {moving}, 1, level, geometry, error) &&
                     before == geometry.water,
                 "static phase validation rejects motion");
    const GridLayout huge{{100000, 100000, 100000}, .25};
    checks.Check(huge.CheckedCellCount() == 0 &&
                     !FlatWaterHeight(huge, {}, 1, 1., inverse, error) && inverse == 1.,
                 "oversized layout is rejected before allocation or publication");
}

void AcrossAxes(Checks& checks) {
    const GridLayout grid{{7, 6, 5}, .23};
    std::mt19937 engine(0x9b5ba246);
    const auto random = [&] { return (double(engine()) + .5) / 4294967296.; };
    double maxCell = 0., maxFace = 0., maxVolume = 0., maxHeight = 0.;
    for (int sample = 0; sample < 128; ++sample) {
        const int up = sample % 3;
        const int b = (up + 1) % 3, c = (up + 2) % 3;
        BoxMotion box{71};
        for (int axis = 0; axis < 3; ++axis) {
            const double extent = grid.h * grid.cells[axis];
            box.lo[axis] = (.04 + .42 * random()) * extent;
            box.hi[axis] = (.53 + .43 * random()) * extent;
        }
        const double level = random() * grid.h * grid.cells[up];
        const double basinArea = grid.h * grid.h * grid.cells[b] * grid.cells[c];
        const double cutArea = (box.hi[b] - box.lo[b]) * (box.hi[c] - box.lo[c]);
        const double cutHeight = std::max(0., std::min(level, box.hi[up]) - box.lo[up]);
        const double expected = basinArea * level - cutArea * cutHeight;
        MovingGridGeometry moving;
        GridPhaseGeometry phase;
        std::string error;
        double volume = -1., recovered = -1.;
        if (!BuildMovingGridGeometry(grid, {box}, moving, error) ||
            !BuildGridPhaseGeometry(grid, {box}, up, level, phase, error) ||
            !FlatWaterVolume(grid, {box}, up, level, volume, error) ||
            !FlatWaterHeight(grid, {box}, up, expected, recovered, error)) {
            checks.Check(false, "sampled phase geometry is representable");
            return;
        }
        maxCell = std::max(maxCell, PhaseClosure(phase, moving));
        maxFace = std::max(maxFace, ApertureClosure(phase, moving));
        maxVolume = std::max({maxVolume, std::abs(volume - expected),
                              std::abs(Sum(phase.water) - expected)});
        maxHeight = std::max(maxHeight, std::abs(recovered - level));
    }
    std::cout << std::setprecision(17) << "128 samples across all chart axes, maximum errors: cell "
              << maxCell << " m^3, face " << maxFace << " m^2, volume " << maxVolume
              << " m^3, height " << maxHeight << " m\n";
    checks.Check(maxCell < 2e-15 && maxFace < 2e-15 && maxVolume < 5e-14 && maxHeight < 5e-14,
                 "all axes preserve independent phase volumes, apertures and inverse height");
}

double MaximumDifference(const Apertures& a, const Apertures& b) {
    double result = 0.;
    for (int axis = 0; axis < 3; ++axis)
        for (std::size_t i = 0; i < a[axis].size(); ++i)
            result = std::max(result, std::abs(a[axis][i] - b[axis][i]));
    return result;
}

// Independent physical-coordinate rectangle overlap and composite midpoint
// time quadrature. It does not use the event integrator or its cell clipping.
std::pair<double, double> ReferenceFace(const GridLayout& grid,
                                        const std::vector<BoxMotion>& boxes,
                                        std::array<int, 3> face, int normal, int up,
                                        double level, int steps) {
    const int b = (normal + 1) % 3, c = (normal + 2) % 3;
    const double plane = face[normal] * grid.h;
    const double loB = face[b] * grid.h, hiB = (face[b] + 1) * grid.h;
    const double loC = face[c] * grid.h, hiC = (face[c] + 1) * grid.h;
    const auto wetArea = [&](double lB, double hB, double lC, double hC) {
        if (normal == up)
            return plane < level ? (hB - lB) * (hC - lC) : 0.;
        if (up == b)
            hB = std::min(hB, level);
        else
            hC = std::min(hC, level);
        return std::max(0., hB - lB) * std::max(0., hC - lC);
    };
    double wetBlocked = 0., dryBlocked = 0.;
    for (int step = 0; step < steps; ++step) {
        const double time = (step + .5) / steps;
        for (const auto& box : boxes) {
            const auto lower = [&](int a) { return box.lo[a] + time * box.translation[a]; };
            const auto upper = [&](int a) { return box.hi[a] + time * box.translation[a]; };
            if (plane < lower(normal) || plane > upper(normal))
                continue;
            const double lB = std::max(loB, lower(b)), hB = std::min(hiB, upper(b));
            const double lC = std::max(loC, lower(c)), hC = std::min(hiC, upper(c));
            if (hB <= lB || hC <= lC)
                continue;
            const double liquid = wetArea(lB, hB, lC, hC);
            wetBlocked += liquid;
            dryBlocked += (hB - lB) * (hC - lC) - liquid;
        }
    }
    const double openWet = wetArea(loB, hiB, loC, hiC);
    return {openWet - wetBlocked / steps, grid.h * grid.h - openWet - dryBlocked / steps};
}

void MovingWetFaces(Checks& checks) {
    const GridLayout grid{{4, 4, 4}, .25};
    constexpr double level = .62;
    const BoxMotion crossing{12, {.40, .51, .24}, {.60, .59, .51}, {0., .20, 0.}};
    const BoxMotion normalCrossing{13, {.40, .41, .24}, {.60, .48, .51}, {0., .25, 0.}};
    std::string error;
    Apertures wet;
    checks.Check(AverageWetAperture(grid, {crossing}, 1, level, wet, error),
                 "moving body can cross the fixed reference plane");
    checks.Check(std::abs(wet[0][grid.Face(0, 2, 2, 1)] - .023) < 2e-15,
                 "water-plane crossing has exact triangular blocked-area integral");
    checks.Check(AverageWetAperture(grid, {normalCrossing}, 1, level, wet, error) &&
                     std::abs(wet[1][grid.Face(1, 1, 2, 1)] - (.25 * .25 - .007)) < 2e-15,
                 "face-normal box crossings integrate only the occupied time interval");

    auto stationary = crossing;
    stationary.translation = {};
    GridPhaseGeometry still;
    checks.Check(AverageWetAperture(grid, {stationary}, 1, level, wet, error) &&
                     BuildGridPhaseGeometry(grid, {stationary}, 1, level, still, error) &&
                     MaximumDifference(wet, still.wetAperture) < 2e-15,
                 "stationary wet time average reduces to the static phase geometry");

    const std::vector<BoxMotion> paths{
        {1, {.21, .43, .24}, {.48, .57, .51}, {.05, .22, -.09}},
        {2, {.65, .72, .12}, {.85, .85, .42}, {-.06, -.15, .20}},
    };
    auto reversed = paths;
    for (auto& box : reversed) {
        for (int a = 0; a < 3; ++a) {
            box.lo[a] += box.translation[a];
            box.hi[a] += box.translation[a];
            box.translation[a] = -box.translation[a];
        }
    }
    MovingGridGeometry geometry;
    checks.Check(BuildMovingGridGeometry(grid, paths, geometry, error),
                 "time quadrature reference paths stay disjoint");
    double reversalError = 0., quadratureError = 0., phaseClosure = 0.;
    for (int up = 0; up < 3; ++up) {
        Apertures backwards;
        if (!AverageWetAperture(grid, paths, up, level, wet, error) ||
            !AverageWetAperture(grid, reversed, up, level, backwards, error)) {
            checks.Check(false, "forward and reverse paths are admitted on all axes");
            return;
        }
        reversalError = std::max(reversalError, MaximumDifference(wet, backwards));
        for (int normal = 0; normal < 3; ++normal) {
            for (int k = 0; k < grid.cells[2] + (normal == 2); ++k) {
                for (int j = 0; j < grid.cells[1] + (normal == 1); ++j) {
                    for (int i = 0; i < grid.cells[0] + (normal == 0); ++i) {
                        const auto [liquid, air] =
                            ReferenceFace(grid, paths, {i, j, k}, normal, up, level, 32768);
                        const auto index = grid.Face(normal, i, j, k);
                        quadratureError = std::max(quadratureError,
                                                  std::abs(wet[normal][index] - liquid));
                        phaseClosure = std::max(phaseClosure, std::abs(wet[normal][index] + air -
                                                geometry.averageAperture[normal][index]));
                    }
                }
            }
        }
    }
    checks.Check(reversalError < 2e-15, "time-reversing a path leaves the wet aperture unchanged");
    checks.Check(quadratureError < 3e-6,
                 "exact wet apertures agree with independent 32768-step quadrature");
    checks.Check(phaseClosure < 3e-6,
                 "independently integrated dry aperture complements the exact wet aperture");
    std::cout << "Maximum moving aperture errors: reversal " << reversalError
              << " m^2, refined quadrature " << quadratureError
              << " m^2, independent dry complement " << phaseClosure << " m^2\n";

    const auto previous = wet;
    checks.Check(!AverageWetAperture(grid, paths, 1, -1., wet, error) && wet == previous,
                 "invalid reference plane leaves the previous face apertures unchanged");
}

} // namespace

int main() {
    Checks checks;
    ExactControls(checks);
    AcrossAxes(checks);
    MovingWetFaces(checks);
    std::cout << checks.count << " flat phase geometry checks, " << checks.failures << " failures\n";
    return checks.failures ? 1 : 0;
}
