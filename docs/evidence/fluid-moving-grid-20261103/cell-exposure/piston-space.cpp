#include "ExposureVolumePressure.h"
#include "GridPressureReaction.h"
#include <iomanip>
#include <iostream>

int main() {
    using namespace judas::volume_grid;
    constexpr double dt = 1. / 600.;
    const BoxMotion body{1, {.25,.5,.25}, {.75,.875,.75}, {0,.004,0}};
    for (const int width : {4, 8, 16, 24, 32}) {
        const GridLayout layout{{width,width,width}, 1./width};
        MovingGridGeometry geometry;
        std::string error;
        if (!BuildMovingGridGeometry(layout, {body}, geometry, error)) {
            std::cout << "geometry rejected " << width << ' ' << error << '\n';
            return 1;
        }
        MovingVolumeState state;
        state.water = geometry.startCapacity;
        for (int axis = 0; axis < 3; ++axis)
            state.velocity[axis].resize(layout.FaceCount(axis));
        std::cout << std::setprecision(12) << "width " << width << " dx " << layout.h;
        for (bool group : {false, true}) {
            MovingVolumeStep pressure;
            const bool solved = group
                ? AdvanceExposureVolumePressure(layout, geometry, state, dt, true, pressure, error)
                : AdvanceMovingVolume(layout, geometry, state, dt, {}, pressure, error);
            std::cout << (group ? " group " : " original ");
            if (solved) {
                const auto load = AccumulateGridPressureReaction(geometry.averagePatches, pressure.pressure).front();
                std::cout << "impulse " << dt * load.force[1] << " residual " << pressure.residual;
            } else {
                std::cout << "rejected " << error;
            }
        }
        std::cout << '\n';
    }
}
