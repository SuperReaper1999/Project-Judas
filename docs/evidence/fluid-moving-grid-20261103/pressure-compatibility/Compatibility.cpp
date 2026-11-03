#include "MovingVolumeGrid.cpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>

int main(int argc, char**) {
    using namespace judas::volume_grid;
    const bool expectPhysical = argc == 2;
    const std::vector<FaceFlux> graph{{0, 1, 0, 0, 1., 1., 1e-6, 0., 0.},
                                      {1, 2, 1, 0, 1., 1., 1e-29, 0., 0.}};
    constexpr double transported = 3e-5;
    std::vector<double> rhs{transported, -transported + 1e-19, 0.};
    const double incompatibility = rhs[0] + rhs[1];
    std::vector<double> pressure(3);
    int iterations = 0;
    std::string error;
    if (!SolvePressure(graph, rhs, 1e-24, .001953125, pressure, iterations, error)) {
        std::cerr << error << '\n';
        return 2;
    }
    const double physicalFlux = graph[0].conductance * (pressure[0] - pressure[1]);
    const double tailDifference = pressure[1] - pressure[2];
    const double tailFlux = graph[1].conductance * tailDifference;
    const double edgeWork = physicalFlux * (pressure[0] - pressure[1]) + tailFlux * tailDifference;
    const double independentlyExpected = rhs[0] * rhs[0] / graph[0].conductance;
    const bool physical = std::abs(tailFlux) < 1e-24 && std::abs(tailDifference) < 1e-12 &&
        std::abs(edgeWork - independentlyExpected) < 1e-15;

    std::vector<double> invalid{transported, -.99 * transported, 0.};
    std::vector<double> rejected(3);
    int count = 0;
    const bool rejectedIncompatible = !SolvePressure(graph, invalid, 1e-24, .001953125, rejected, count, error) &&
        error.find("incompatible sealed pressure component") != std::string::npos &&
        rejected == std::vector<double>(3);

    std::cout << std::setprecision(17) << "raw incompatibility=" << incompatibility
              << " tail rhs after correction=" << rhs[2]
              << " physical flux=" << physicalFlux << " tail flux=" << tailFlux
              << " tail pressure jump=" << tailDifference << " graph work=" << edgeWork
              << " independent work=" << independentlyExpected
              << " work defect=" << edgeWork - independentlyExpected << " physical=" << physical
              << " rejected incompatible=" << rejectedIncompatible << '\n';
    return physical == expectPhysical && rejectedIncompatible ? EXIT_SUCCESS : EXIT_FAILURE;
}
