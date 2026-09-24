#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "MeshData.h"

// Presentation only: extracts a lit, non-indexed isosurface from the current
// fluid particle positions. The result never feeds back into FluidWorld.
// The cubic kernel joins nearby particles into a visible volume while
// retaining separate droplets when they genuinely separate in space.
std::vector<MeshVertex> BuildFluidSurface(const std::vector<glm::vec3>& positions,
                                          float kernelRadius, float gridSpacing,
                                          float isoValue);
