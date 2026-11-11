#pragma once
#include "../Light.h"
#include "../Visibility.h"
#include <functional>
#include <string>
#include <vector>
class Renderer;
class RuntimeWorld;
class ResourceManager;
class Scene;
struct SceneSettings;
namespace JudasLighting {
struct ShadowCaster { VisualBounds bounds; std::string content; };
struct ShadowScene {
    std::vector<ShadowCaster> casters;
    // Unknown mutable draw owners fail open: redraw, never publish stale depth.
    bool volatileContent=false;
};
ShadowScene RuntimeShadowScene(const RuntimeWorld&,float alpha);
ShadowScene AuthoredShadowScene(const Scene&,ResourceManager&);
// One owner for shadow planning in runtime, camera targets and Edit preview.
// Every selected dirty chart is rendered before the consuming colour pass.
void PrepareShadows(Renderer&,const SceneSettings&,const std::vector<DynamicLight>&,
                    const ShadowScene&,const std::function<void()>& draw);
}
