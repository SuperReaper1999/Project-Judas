#pragma once

#include "GridCellPhase.h"

namespace judas::volume_grid {

struct GridWaterTriangle {
    std::array<GridVec, 3> vertices;
    GridVec normal; // Winding points from water into air.
};

struct GridWaterSurface {
    std::vector<GridWaterTriangle> triangles;
};

// Extract the water/air interface of the existing per-cell phase. The grid
// exterior and solid faces are walls. This emits neither wall triangles nor
// visual-only volume. Vertices are in metres relative to the grid chart.
// Invalid geometry or an oversized mesh leaves output intact.
bool BuildGridWaterSurface(const GridLayout& layout, const std::vector<BoxMotion>& stationary,
                           const GridCellPhase& phase, GridWaterSurface& output,
                           std::string& error);

} // namespace judas::volume_grid
