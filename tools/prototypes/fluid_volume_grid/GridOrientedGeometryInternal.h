#pragma once

#include "GridOrientedGeometry.h"

namespace judas::volume_grid::oriented_geometry_detail {

// Blocked-face first moment about the solid center, in grid units.
// This internal path is sampled only by the one-body swept geometry operator.
using FaceMoments = std::array<std::vector<GridVec>, 3>;

// Only aperture, wall normals, and first moments are needed at intermediate
// quadrature times. Endpoint cuts still use the complete geometry builder.
bool BuildSweepSample(const GridLayout& layout, const GridOrientedBox& box,
                      GridOrientedGeometry& output, FaceMoments* blockedArm,
                      std::string& error);

} // namespace judas::volume_grid::oriented_geometry_detail
