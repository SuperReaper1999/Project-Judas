#include "OwnedEvolvingVolume.h"
#include "GridPressureReaction.h"

#include <fstream>
#include <iomanip>
#include <iostream>

using namespace judas::volume_grid;

int main(int argc, char** argv) {
    if (argc != 2)
        return 1;
    const GridLayout layout{{8, 8, 8}, .125};
    BoxMotion box{4};
    GridVec oldVelocity, gravity;
    double mass, dt;
    MovingVolumeState fluid;
    HalfDualVolumeState owned;
    std::ifstream input(argv[1], std::ios::binary);
    const auto read = [&](double* values, std::size_t count) {
        input.read(reinterpret_cast<char*>(values), count * sizeof(double));
    };
    read(box.lo.data(), 3);
    read(box.hi.data(), 3);
    read(oldVelocity.data(), 3);
    read(&mass, 1);
    read(&dt, 1);
    read(gravity.data(), 3);
    fluid.water.resize(layout.CellCount());
    read(fluid.water.data(), fluid.water.size());
    for (int axis = 0; axis < 3; ++axis) {
        fluid.velocity[axis].resize(layout.FaceCount(axis));
        read(fluid.velocity[axis].data(), fluid.velocity[axis].size());
    }
    for (auto& velocity : owned.velocity) {
        velocity.resize(2 * layout.CellCount());
        read(velocity.data(), velocity.size());
    }
    if (!input || input.peek() != std::char_traits<char>::eof())
        return 2;
    std::cout << std::setprecision(17)
              << "dx,dy,lower_x_load,upper_x_load,total_x_load,ry,aperture_x2,aperture_x6,x2_gated_abs_flux,x6_gated_abs_flux,max_x_face_velocity,fy\n";
    for (double dy : {-.0036349140061924913, 0.}) {
        for (double dx : {-1e-6, -1e-8, -1e-10, -1e-12, -1e-14, 0., 1e-14, 1e-12, 1e-10, 1e-8, 1e-6}) {
            box.translation = {dx, dy, 0.};
            MovingGridGeometry geometry;
            OwnedEvolvingVolumeStep step;
            std::string error;
            if (!BuildMovingGridGeometry(layout, {box}, geometry, error) ||
                !AdvanceGeometricEvolvingVolume(layout, {box}, fluid, owned, dt, 1, gravity[1], step, error)) {
                std::cout << dx << ',' << dy << ',' << error << '\n';
                continue;
            }
            const auto& pressure = step.phase.fluid.pressure;
            GridVec loads{};
            for (const auto& patch : geometry.averagePatches)
                if (patch.axis == 0)
                    loads[patch.sign > 0] -= patch.sign * patch.area * pressure[patch.cell];
            const auto& force = step.phase.loads.front().force;
            const double residualY = dy - dt * (oldVelocity[1] + dt * gravity[1]) -
                dt * dt * force[1] / mass;
            GridVec gatedFlux{};
            double peakFaceSpeed = 0.;
            for (int z = 2; z < 6; ++z) {
                for (int y = 4; y < 6; ++y) {
                    const auto left = layout.Face(0, 2, y, z);
                    const auto right = layout.Face(0, 6, y, z);
                    for (const auto face : {left, right}) {
                        const double area = geometry.averageAperture[0][face];
                        if (geometry.startAperture[0][face] == 0. && area > 0.) {
                            const double q = step.phase.fluid.flux[0][face];
                            gatedFlux[face == right] += std::abs(q);
                            peakFaceSpeed = std::max(peakFaceSpeed, std::abs(q)/(dt*area));
                        }
                    }
                }
            }
            std::cout << dx << ',' << dy << ',' << loads[0] << ',' << loads[1] << ','
                      << force[0] << ',' << residualY << ','
                      << geometry.averageAperture[0][layout.Face(0, 2, 4, 4)] << ','
                      << geometry.averageAperture[0][layout.Face(0, 6, 4, 4)] << ','
                      << gatedFlux[0] << ',' << gatedFlux[1] << ',' << peakFaceSpeed << ',' << force[1] << '\n';
        }
    }
}
