#include "CoupledVolumeBox.h"
#include "OwnedEvolvingVolume.h"
#include "GridWaterQuery.h"
#include "GridParcelSeed.h"
#include "FluidSettlingContract.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string_view>

using namespace judas::volume_grid;

int main(int argc, char** argv) {
    const int hz = argc >= 2 ? std::stoi(argv[1]) : 60;
    const bool strictWarmup = argc == 3 && std::string_view(argv[2]) == "--strict-warmup";
    if ((hz != 60 && hz != 30) || argc > 3 || (argc == 3 && !strictWarmup))
        return 1;
    const GridLayout grid{{10,15,10}, .20};
    const double half = double(.3f);
    const GridVec center{1., double(.7f), 1.};
    BoxMotion shape{1};
    for (int a = 0; a < 3; ++a) {
        shape.lo[a] = center[a] - half;
        shape.hi[a] = center[a] + half;
    }
    MovingVolumeState fluid;
    HalfDualVolumeState owned;
    for (int a = 0; a < 3; ++a) {
        fluid.velocity[a].resize(grid.FaceCount(a));
        owned.velocity[a].resize(2 * grid.CellCount());
    }
    fluid.water.resize(grid.CellCount());
    for (int j = 0; j < 12; ++j)
        for (int k = 0; k < 10; ++k)
            for (int i = 0; i < 10; ++i) {
                constexpr float spacing = .20f;
                const std::array<float, 3> point{(i+.5f)*spacing-1.f,(j+.5f)*spacing,(k+.5f)*spacing-1.f};
                const std::array<float, 3> sourceCenter{0.f, .7f, 0.f};
                bool omitted = true;
                for (int a = 0; a < 3; ++a)
                    omitted = omitted && std::abs(point[a] - sourceCenter[a]) < .3f + spacing * .36f;
                if (!omitted)
                    fluid.water[grid.Cell(i,j,k)] = grid.h*grid.h*grid.h;
            }
    const double water = std::accumulate(fluid.water.begin(), fluid.water.end(), 0.);
    std::string error;
    for (int radius : {0, 1, 2, 3, 4, -1}) {
        GridParcelSeed seed;
        const bool admitted = ImportGridParcelSeed(grid, {shape}, fluid.water, radius, seed, error);
        std::cout << std::setprecision(17) << "radius " << radius << " authored "
                  << water << " accepted " << admitted << " relocated " << seed.movedVolume
                  << " max " << seed.maximumGraphRadius;
        if (!admitted)
            std::cout << " reason " << error;
        else
            std::cout << " represented " << std::accumulate(seed.water.begin(), seed.water.end(), 0.);
        std::cout << '\n';
    }
}
