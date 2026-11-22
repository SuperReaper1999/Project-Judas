#pragma once
#include "../Light.h"
#include "../Visibility.h"
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
class Renderer;
class RuntimeWorld;
class ResourceManager;
class Scene;
struct SceneSettings;
struct SceneRenderComponent;
namespace JudasRender { struct PresentedObjects; }
namespace JudasLighting {
struct ShadowCaster {
    VisualBounds bounds;
    std::string content;
    // Static draws borrow their render description for this presentation only.
    // Large worlds encode it only for charts that can actually reuse depth.
    const SceneRenderComponent* deferredRender=nullptr;
    std::uint64_t owner=0;
    glm::vec3 position{}, scale{1};
    glm::quat rotation{1,0,0,0};
};
struct ShadowScene {
    // Cache-key inputs. The draw callback owns the actual caster submission.
    std::vector<ShadowCaster> casters;
    std::vector<ShadowCaster> movingCasters;
    // Unknown mutable draw owners fail open: redraw, never publish stale depth.
    bool volatileContent=false;
    bool compactCasterKeys=false;
};
std::string ShadowRenderKey(const SceneRenderComponent&);
ShadowScene RuntimeShadowScene(const RuntimeWorld&,float alpha,JudasRender::PresentedObjects*);
ShadowScene AuthoredShadowScene(const Scene&,ResourceManager&);
// One owner for shadow planning in runtime, camera targets and Edit preview.
// Every selected dirty chart is rendered before the consuming colour pass.
void PrepareShadows(Renderer&,const SceneSettings&,const std::vector<DynamicLight>&,
                    const ShadowScene&,const std::function<void()>& draw);
void PrepareRuntimeShadows(Renderer&,const SceneSettings&,const std::vector<DynamicLight>&,
                          bool terrain, const std::function<const ShadowScene&()>& snapshot,
                          const std::function<void()>& draw);
}
