#pragma once

#include "GridOrientedGeometry.h"

namespace judas::volume_grid {

struct GridOrientedMotion {
    GridOrientedBox start;
    GridVec translation{};
    GridVec rotationAxis{0., 1., 0.};
    double angle = 0.; // Radians about a fixed chart-space axis through the moving center.
};

struct GridOrientedSweepPatch {
    std::uint32_t owner;
    std::size_t cell;
    GridVec normalArea; // Time average of ∫n dA, in m².
    GridVec torqueArea; // Time average of ∫(x-center(t)) × n dA, in m³.
};

struct GridOrientedSweep {
    GridOrientedGeometry start, end;
    std::array<std::vector<double>, 3> averageAperture, solidSweep;
    std::vector<GridOrientedSweepPatch> patches;
    double maxFaceResidual = 0., maxPatchResidual = 0.; // m³
    double estimatedQuadratureError = 0.; // Dimensionless estimate, not a rigorous bound.
    std::size_t evaluations = 0;
};

// One prescribed OBB, with linear translation and constant angular velocity
// in a closed chart. Endpoint cuts are spatially exact. Moving quantities
// use bounded adaptive integration and independent cellwise conservation
// checks. Unsupported motion or failed checks leave output intact.
bool BuildGridOrientedSweep(const GridLayout& layout, const GridOrientedMotion& motion,
                            GridOrientedSweep& output, std::string& error);

} // namespace judas::volume_grid
