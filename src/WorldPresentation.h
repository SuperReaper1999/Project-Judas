#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "Light.h"

class GameSession;
class Renderer;
class RuntimeWorld;
class Scene;
class ResourceManager;
struct AerodynamicDragResult;

// Milestone 28: everything that turns a RuntimeWorld (and, optionally, the
// GameSession playing it) into Renderer calls. The presentation-side
// counterpart of Simulation.h: it reads presented (interpolated) poses,
// builds the frame's light list from them, runs the M15 shadow passes and
// the M24-M27 transparent passes, and never writes simulation state.
//
// `session` may be null (editor Play-less preview, harness screenshots of
// a bare world): then no player, torch, vehicle lights or fire heater tip
// are drawn and the world is presented at alpha 1.
struct WorldDrawOptions {
    bool includePlayerModel = true;
    bool includeTerrain = true;
};

// Opaque geometry only — safe inside a shadow pass.
void DrawWorldGeometry(Renderer& renderer, const RuntimeWorld& world, const GameSession* session,
                       float presentationAlpha, const WorldDrawOptions& options);

// Translucent extras: compound walls, atmosphere haze shells, fire, the
// heater tip. Colour pass only.
void DrawWorldTransparents(Renderer& renderer, const RuntimeWorld& world, const GameSession* session,
                           float presentationAlpha);

// This frame's dynamic lights (torch, vehicle rig, switch lamps, standalone
// scene lights) in world space from presented poses.
std::vector<DynamicLight> BuildWorldLights(const RuntimeWorld& world, const GameSession* session,
                                           float presentationAlpha);

// One complete presented frame: shadow passes, colour pass, transparents.
// `shadowFocus` centres the directional shadow frustum (the player when
// playing, the editor camera otherwise).
void RenderWorldFrame(Renderer& renderer, int width, int height, const RuntimeWorld& world,
                      const GameSession* session, const glm::mat4& view, const glm::mat4& projection,
                      const glm::vec3& shadowFocus, float presentationAlpha);

// The fluid surface mesh is rebuilt from presented particle positions each
// frame; call before RenderWorldFrame when the world has fluid.
void UpdateFluidSurface(Renderer& renderer, const RuntimeWorld& world, float presentationAlpha);

// Editor edit-mode view: draws AUTHORED objects straight from a Scene at
// their authored transforms, with no physics and no runtime instance.
// Meshes/terrains resolve through `resources`; unresolvable ones are skipped.
void DrawAuthoredScene(Renderer& renderer, const Scene& scene, ResourceManager& resources);
std::vector<DynamicLight> BuildAuthoredLights(const Scene& scene);
