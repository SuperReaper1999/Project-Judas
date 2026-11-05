#pragma once

#include "CutRegionGeometry.h"

namespace judas::volume_grid {

struct CutRectangularOwners {
    CutRegionChart chart;
    CutRegionGeometry geometry;
    // Total mapping of original geometric regions to pressure/material owners.
    std::vector<std::size_t> owner;
};

// Join only exact rectangular unions across a fully open shared face.
// No coordinate snapping, solid deletion or diagonal connectivity. The
// resulting chart is not parent-binned: offsets is deliberately empty.
// minimumFraction specifies a target fraction of one original grid cell;
// it does not promise that every cut can be merged geometrically.
bool BuildCutRectangularOwners(const CutRegionChart& source,
                               const CutRegionGeometry& geometry,
                               double minimumFraction, CutRectangularOwners& output,
                               std::string& error);

} // namespace judas::volume_grid
