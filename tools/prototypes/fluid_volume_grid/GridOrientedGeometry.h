#pragma once

#include "MovingGridGeometry.h"

#include <optional>

namespace judas::volume_grid {

struct GridOrientedBox {
    std::uint32_t owner = 0;
    GridVec center{}, half{};
    std::array<GridVec, 3> axes{{{1., 0., 0.}, {0., 1., 0.}, {0., 0., 1.}}};
};

struct GridOrientedPatch {
    std::uint32_t owner;
    std::size_t cell;
    GridVec normal, centroid;
    double area;
    // Integral of (position-centroid)(position-centroid)^T over this patch.
    std::array<GridVec, 3> centeredMoment;
};

struct GridOrientedPlane {
    int axis = 1;
    double height = 0.;
};

struct GridOrientedGeometry {
    std::vector<double> capacity, wetCapacity; // m³
    std::array<std::vector<double>, 3> aperture, wetAperture; // m²
    std::vector<GridOrientedPatch> patches, wetPatches;
};

// Static, separated OBBs strictly within the domain. Box axes must form an
// orthonormal right-handed frame. An optional chart plane clips water below
// its height. No motion, mesh contact, or phase transport is implied.
// Invalid input leaves the previous output intact.
bool BuildGridOrientedGeometry(const GridLayout& layout, const std::vector<GridOrientedBox>& boxes,
                               GridOrientedGeometry& output, std::string& error,
                               std::optional<GridOrientedPlane> wet = std::nullopt);

} // namespace judas::volume_grid
