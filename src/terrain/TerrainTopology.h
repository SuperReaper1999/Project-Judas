#pragma once
#include "MeshData.h"
namespace TerrainV2 {
// Preserve positions and surface shape while making clipped heightfield and
// tetrahedral band edges share every collinear vertex. Uses the native cook's
// geometric tolerance; this does not relax its manifold/T-junction checks.
void ConformChunkTopology(MeshData &, float binWidth);
} // namespace TerrainV2
