#include "MovingGridGeometry.h"

#include <cmath>
#include <iomanip>
#include <iostream>

int main() {
    using namespace judas::volume_grid;
    const GridLayout coarse{{4, 4, 4}, .25};
    const GridLayout refined{{8, 8, 8}, .125};
    const BoxMotion piston{1, {.25, .5, .25}, {.75, .875, .75}, {0., .04, 0.}};
    MovingGridGeometry parent, children;
    std::string error;
    if (!BuildMovingGridGeometry(coarse, {piston}, parent, error) ||
        !BuildMovingGridGeometry(refined, {piston}, children, error)) {
        std::cerr << error << '\n';
        return 2;
    }

    const auto cell = coarse.Cell(1, 2, 1);
    double lower = 0, upper = 0, midpointAperture = 0;
    for (int z = 2; z < 4; ++z) {
        for (int x = 2; x < 4; ++x) {
            lower += children.endCapacity[refined.Cell(x, 4, z)];
            upper += children.endCapacity[refined.Cell(x, 5, z)];
            midpointAperture += children.averageAperture[1][refined.Face(1, x, 5, z)];
        }
    }

    constexpr double sweptVolume = .25 * .25 * .04;
    const double lumpedLower = .5 * parent.endCapacity[cell];
    const double lumpedMidpointFlux = .5 * sweptVolume;
    std::cout << std::setprecision(17) << "start " << parent.startCapacity[cell]
              << " end " << parent.endCapacity[cell] << " exact lower " << lower
              << " exact upper " << upper << " midpoint aperture " << midpointAperture
              << " m2, lumped lower " << lumpedLower << " lumped midpoint flow "
              << lumpedMidpointFlux << " m3\n";
    const bool exact = parent.startCapacity[cell] == 0 && upper == 0 && midpointAperture == 0 &&
        std::abs(lower - sweptVolume) < 1e-16 &&
        std::abs(parent.endCapacity[cell] - sweptVolume) < 1e-16;
    const bool rejectedLumping = lumpedLower < lower && lumpedMidpointFlux > 0;
    std::cout << "geometric oracle " << (exact ? "PASS" : "FAIL")
              << ", half-lumped owner negative control " << (rejectedLumping ? "PASS" : "FAIL") << '\n';
    return exact && rejectedLumping ? 0 : 1;
}
