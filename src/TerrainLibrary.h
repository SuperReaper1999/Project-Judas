#pragma once

#include <memory>
#include <string>
#include <vector>

class RadialTerrain;

// Milestone 28: the engine-constructible radial terrain surfaces a scene
// file may reference by identifier (SceneBodyComponent::terrainSurface).
// A scene never embeds an elevation function; it names one. Today the only
// entry is the authored M25 demonstration surface ("m25-radial"). A future
// heightmap/procedural asset pipeline would add entries here (or replace
// this lookup with real asset loading) without touching scene files that
// already reference an id. Unknown ids return nullptr so instantiation can
// fail clearly rather than inventing flat ground.
std::shared_ptr<const RadialTerrain> CreateTerrainSurface(const std::string& identifier);
const std::vector<std::string>& KnownTerrainSurfaces();
