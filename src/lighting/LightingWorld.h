#pragma once
#include "../Light.h"
#include <vector>
#include <limits>
class RuntimeWorld;
class Scene;
// Immutable presentation snapshots borrowed from authoritative scene/runtime
// components. Simulation samples explicitly bypass remote render interpolation.
// No GPU handles or duplicate gameplay-owned light state.
std::vector<DynamicLight> BuildWorldLights(const RuntimeWorld&,float presentationAlpha,unsigned maximumSources=std::numeric_limits<unsigned>::max(),bool simulation=false);
std::vector<DynamicLight> BuildAuthoredLights(const Scene& flattenedScene);
