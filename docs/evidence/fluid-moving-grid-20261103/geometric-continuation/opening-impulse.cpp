#include "OwnedEvolvingVolume.h"
#include "GridPhaseGeometry.h"
#include <iomanip>
#include <iostream>

int main() {
    using namespace judas::volume_grid;
    const GridLayout layout{{8, 8, 8}, .125};
    const BoxMotion initial{8, {.25, .49, .25}, {.75, .865, .75}, {}};
    constexpr double dt = 1. / 60.;
    MovingGridGeometry stationary;
    GridPhaseGeometry phase;
    std::string error;
    if (!BuildMovingGridGeometry(layout, {initial}, stationary, error) ||
        !BuildGridPhaseGeometry(layout, {initial}, 1, .94, phase, error))
        return 1;
    MovingVolumeState fluid;
    fluid.water = phase.water;
    for (std::size_t cell = 0; cell < layout.CellCount(); ++cell)
        fluid.water[cell] = std::min(fluid.water[cell], stationary.startCapacity[cell]);
    HalfDualVolumeState owned;
    for (int axis = 0; axis < 3; ++axis) {
        fluid.velocity[axis].resize(layout.FaceCount(axis));
        owned.velocity[axis].resize(2 * layout.CellCount());
    }
    const auto face = layout.Face(1, 3, 4, 3);
    const auto below = layout.Cell(3, 3, 3), above = layout.Cell(3, 4, 3);
    std::cout << std::setprecision(16);
    for (double delta : {-.001, -.0001, -.00001, -.000001, .000001, .00001, .0001, .001}) {
        auto moving = initial;
        moving.translation[1] = .01 + delta;
        MovingGridGeometry geometry;
        if (!BuildMovingGridGeometry(layout, {moving}, geometry, error))
            return 2;
        OwnedEvolvingVolumeStep step;
        if (!AdvanceGeometricEvolvingVolume(layout, {moving}, fluid, owned, dt, 1, 0., step, error)) {
            std::cout << delta << " rejected " << error << '\n';
            continue;
        }
        std::cout << delta << " aperture " << geometry.averageAperture[1][face] << " flow "
                  << step.phase.fluid.flux[1][face] << " pressure_drop "
                  << step.phase.fluid.pressure[below] - step.phase.fluid.pressure[above]
                  << " impulse " << dt * step.phase.loads.front().force[1]
                  << " pressure_residual " << step.phase.fluid.residual << '\n';
    }
}
