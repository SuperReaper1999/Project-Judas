#pragma once
#include "../Light.h"
#include <vector>
class RuntimeWorld;
class Scene;
// Immutable presentation snapshots borrowed from authoritative scene/runtime
// components. No GPU handles or duplicate gameplay-owned light state.
std::vector<DynamicLight> BuildWorldLights(const RuntimeWorld&,float presentationAlpha);
std::vector<DynamicLight> BuildAuthoredLights(const Scene& flattenedScene);
