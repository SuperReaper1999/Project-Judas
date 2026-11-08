#pragma once
#include "TerrainSource.h"
struct SceneObject;
struct SceneTransform;
class Scene;
class AssetDatabase;
namespace TerrainV2 {
// Choose a bounded geographic footprint around a model-space direction. The
// physical rim determines its sample pitch; callers never guess cube indices.
bool CreatePlanetPatch(const PlanetSettings &, glm::dvec3 direction, unsigned cells, Source &,
                       std::string &);
SceneTransform PlanetTerrainPlacement(const PlanetSettings &, const PlanetChart &,
                                      glm::dvec3 origin);
bool PlanetTerrainBoundaryLevel(const PlanetSettings &, const Source &, unsigned &, std::string &);
bool ValidatePlanetBinding(const SceneObject &, const Source &, const SceneObject *, glm::dvec3,
                           std::string &, bool requirePlacement = true);
bool ValidatePlanetTerrains(const Scene &, const AssetDatabase *, std::string &);
} // namespace TerrainV2
