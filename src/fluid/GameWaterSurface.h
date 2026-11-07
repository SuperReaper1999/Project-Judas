#pragma once
#include "GameWater.h"
#include "MeshData.h"
namespace JudasWater {
// Bounded pool presentation; the simulation remains three-dimensional.
// This first presentation extracts the highest visible layer per column.
MeshData BuildPoolSurface(const Grid &);
} // namespace JudasWater
