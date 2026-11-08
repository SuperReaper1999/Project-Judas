#pragma once
#include "MeshData.h"

bool PickTerrain(const MeshData &, glm::vec3 origin, glm::vec3 direction, glm::vec3 &hit);
// Same two-sided triangle test as the original authoring picker. Retaining the
// nearest parameter across ranges preserves original flattened triangle order.
bool PickTerrainRange(const MeshData &, size_t first, size_t count, glm::vec3 origin,
                      glm::vec3 direction, float &nearest);
