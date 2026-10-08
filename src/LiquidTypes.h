#pragma once
#include "LiquidGeometry.h"
#include "LiquidSurface.h"
#include <map>
#include <optional>
#include <memory>
struct LiquidMaterial {std::string id="water";double density=1000;};
struct LiquidBasinSettings {LiquidSurfaceSettings surface;bool enabled=true;std::string geometry,asset;LiquidMaterial material;double initialVolume=0,volumeTolerance=1e-5,heightTolerance=.002;};
struct LiquidContainerSettings {bool enabled=true;std::string geometry;LiquidMaterial material;double initialVolume=0;std::vector<glm::dvec3> opening{{-.2,.3,-.2},{.2,.3,-.2},{.2,.3,.2},{-.2,.3,.2}};double openingArea=.01,discharge=.6;};
struct LiquidConnectionSettings {bool enabled=true,bidirectional=true;std::uint64_t source=0,destination=0;double openingArea=.01,discharge=.6;};
struct LiquidInteractionSettings {bool enabled=true;double drag=1;};
struct SceneObject;class Scene;class AssetDatabase;
std::map<std::string,std::string> LiquidProperties(const SceneObject&);
bool ApplyLiquidProperties(const std::map<std::string,std::string>&,SceneObject&,std::string&);
bool ValidateLiquidComponents(const SceneObject&,std::string&);
std::string LiquidSourceFingerprint(const Scene&,const SceneObject&,const AssetDatabase&,std::string& error);
bool PrepareLiquidBake(const Scene&,const SceneObject&,const AssetDatabase&,LiquidGeometry&,GravityEquilibrium&,std::string& fingerprint,std::string& error);
struct LiquidResource {LiquidGeometry geometry;std::shared_ptr<const LiquidBasinData> basin;};
bool DecodeLiquidResource(const std::vector<unsigned char>&,LiquidResource&,std::string&);
