#pragma once
#include <string>
struct MeshData;
struct CollisionAsset;
namespace TerrainV2 {
// Exact specialization for unedited tensor-product heightfields. Proves the
// grid's geometry and full index pattern before using its known adjacency.
// Returns false without publication if it is not such a grid. The generic
// strict cooker remains authoritative for volumes and arbitrary meshes.
bool CookHeightfieldCollision(const MeshData &, unsigned cells, CollisionAsset &, std::string &);
} // namespace TerrainV2
