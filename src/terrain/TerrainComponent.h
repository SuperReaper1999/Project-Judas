#pragma once
#include <string>
struct SceneObject;
class RuntimeWorld;
class AssetDatabase;
class Scene;
struct SceneLandscapeSettings {
    std::string asset;
};
namespace TerrainV2 {
bool ValidateOwnerPlacement(const SceneObject &, std::string &);
bool ValidateOwner(const SceneObject &, const AssetDatabase *, std::string &);
bool BuildAuthored(RuntimeWorld &, const SceneObject &, std::string &);
bool SourceFingerprint(const Scene &, const AssetDatabase *, std::string &, std::string &);
} // namespace TerrainV2
