#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "Light.h"

class Renderer;
class RuntimeWorld;
class Scene;
class ResourceManager;
struct SceneRenderComponent;
struct MaterialBinding;
struct RenderTargetHandle;

// Resolves durable instance intent to borrowed immutable GPU resources. Pending
// or failed texture replacements keep the inherited map until a valid publish.
std::vector<MaterialBinding> BuildRenderMaterialBindings(ResourceManager*,const SceneRenderComponent&);

// Presentation reads interpolated RuntimeWorld poses and submits geometry,
// lights, shadows and water without advancing simulation or owning gameplay.
// Editor observers can present the same world through their own camera.
struct WorldDrawOptions {
    bool includeTerrain = true;
};

// Opaque geometry only — safe inside a shadow pass.
void DrawWorldGeometry(Renderer& renderer, const RuntimeWorld& world,
                       float presentationAlpha, const WorldDrawOptions& options);

// Translucent compound walls, atmosphere and fire. Colour pass only.
void DrawWorldTransparents(Renderer& renderer, const RuntimeWorld& world,
                           float presentationAlpha);

// Authored dynamic lights in world space from presented poses.
std::vector<DynamicLight> BuildWorldLights(const RuntimeWorld& world,
                                           float presentationAlpha);

// One complete presented frame: shadow passes, colour pass, transparents.
// `shadowFocus` centres the directional shadow frustum (the player when
// playing, the editor camera otherwise).
// Observer uses the supplied camera and never advances authored camera cadence
// or composites local-player views. Runtime behavior remains the default.
// A destination is opened only after auxiliary passes; it receives the final
// main/split-screen composition. No nesting of renderer target passes occurs.
// Auxiliary updates authored camera textures without drawing the hidden main
// or split-screen display. ObserverFrame owns cadence and the visual clock
// when Scene is the only visible display. A secondary Observer never advances
// either. Simulation remains owned by InteractivePlay.
enum class WorldRenderPurpose { Runtime, Observer, Auxiliary, ObserverFrame };
void RenderWorldFrame(Renderer& renderer, int width, int height, const RuntimeWorld& world,
                      const glm::mat4& view, const glm::mat4& projection,
                      const glm::vec3& shadowFocus, float presentationAlpha,float frameSeconds=1.f/60,
                      WorldRenderPurpose purpose=WorldRenderPurpose::Runtime,
                      const RenderTargetHandle* destination=nullptr);

// Editor edit-mode view: draws AUTHORED objects straight from a Scene at
// their authored transforms, with no physics and no runtime instance.
// Meshes/terrains resolve through `resources`; unresolvable ones are skipped.
void DrawAuthoredScene(Renderer& renderer, const Scene& scene, ResourceManager& resources);
void DrawAuthoredScene(Renderer& renderer, const Scene& scene, ResourceManager& resources, bool basicWater);
// For composed editor views: submit all scenes with basicWater=false, then draw
// their water surfaces after all opaque terrain/region geometry is complete.
void DrawAuthoredWaterSurfaces(Renderer& renderer, const Scene& scene, ResourceManager& resources,bool basicWater=true);
std::vector<DynamicLight> BuildAuthoredLights(const Scene& scene);

struct WeatherSample;
WeatherSample SampleAuthoredWeather(const Scene&,glm::dvec3 absoluteEye);
