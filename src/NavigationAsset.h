#pragma once
#include "NavigationTypes.h"
#include "Scene.h"
#include <memory>
// Immutable CPU resource. Runtime worlds build independent mutable Detour caches.
class AssetDatabase;
struct NavigationData {
 NavigationSurfaceSettings settings; NavigationProfile profile;
 std::string fingerprint; std::vector<std::vector<unsigned char>> layers;
};
struct NavigationGeometry {
 std::vector<float> vertices; std::vector<int> triangles;
 struct Volume {std::vector<float> polygon;float bottom=0,top=0;unsigned area=0;bool blocked=false;};
 std::vector<Volume> modifiers; std::string fingerprint;
};
bool CollectNavigationGeometry(const Scene&,const SceneObject&,const ProjectNavigation&,NavigationGeometry&,std::string&,const AssetDatabase* assets=nullptr);
bool DecodeNavigation(const std::vector<unsigned char>&,NavigationData&,std::string&);
bool LoadNavigation(const std::string&,NavigationData&,std::string&);
bool SaveNavigation(const std::string&,const NavigationData&,std::string&);
// Shared bounded geometry baker, used by tools and opt-in live terrain.
bool BakeNavigationGeometry(const NavigationGeometry&,const NavigationSurfaceSettings&,const NavigationProfile&,NavigationData&,std::string&);
// Scene collection and authoring wrapper.
bool BakeNavigation(const Scene&,const SceneObject&,const ProjectNavigation&,NavigationData&,std::string&,const AssetDatabase* assets=nullptr);
