#include "WorldPresentation.h"

#include <glm/gtc/quaternion.hpp>

#include "CelestialGravity.h"
#include "FirePresentation.h"
#include "FluidSurface.h"
#include "GameSession.h"
#include "LightTransforms.h"
#include "PilotAttachment.h"
#include "RadialTerrain.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "ShadowTransforms.h"
#include "TerrainLibrary.h"

namespace {
const glm::vec3 kPlayerColor(0.2f, 0.6f, 0.9f);

// Milestone 14 torch (see docs/ARCHITECTURE.md, "Milestone 14").
const glm::vec3 kTorchColor(3.0f, 2.9f, 2.6f);
constexpr float kTorchRange = 35.0f;
constexpr float kTorchInnerConeDegrees = 18.0f;
constexpr float kTorchOuterConeDegrees = 28.0f;

// Milestone 14 vehicle light rig, in the vehicle's own frame: one
// headlight at the nose (local -Z), red/green navigation lights at the
// wingtips (local -X/+X). Offsets scale with the body's half-extents.
const glm::vec3 kShipHeadlightColor(4.0f, 4.0f, 3.8f);
constexpr float kShipHeadlightRange = 45.0f;
constexpr float kShipHeadlightInnerConeDegrees = 12.0f;
constexpr float kShipHeadlightOuterConeDegrees = 22.0f;
const glm::vec3 kShipPortLightColor(2.2f, 0.15f, 0.1f);
const glm::vec3 kShipStarboardLightColor(0.1f, 2.2f, 0.2f);
constexpr float kShipNavLightRange = 12.0f;

// Milestone 15 directional shadow frustum.
constexpr float kDirShadowHalfExtent = 25.0f;
constexpr float kDirShadowDistance = 40.0f;

// Milestone 31: a mesh render resolves its assets through the resource
// manager every frame. Not Ready yet -> a neutral grey placeholder box of
// the object's scale; Failed -> the same box in magenta, so a broken
// asset is visible and never mistaken for a loaded one.
const glm::vec3 kLoadingPlaceholderColor(0.55f, 0.55f, 0.58f);
const glm::vec3 kFailedPlaceholderColor(0.95f, 0.15f, 0.85f);

void DrawMeshOrPlaceholder(Renderer& r, ResourceManager* resources, const SceneRenderComponent& render,
                           const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale, float alpha) {
    const MeshHandle mesh = resources ? resources->TryGetMesh(render.meshAsset) : MeshHandle{};
    if (mesh.IsValid()) {
        const TextureHandle texture = resources ? resources->TryGetTexture(render.textureAsset) : TextureHandle{};
        r.DrawMesh(mesh, position, rotation, scale, texture, render.color, alpha);
        return;
    }
    if (!resources) return;  // headless: nothing to draw
    const bool failed = resources->StateOf(render.meshAsset) == ResourceState::Failed;
    r.DrawBox(position, rotation, scale * 0.5f, failed ? kFailedPlaceholderColor : kLoadingPlaceholderColor, alpha);
}

void DrawRenderable(Renderer& r, ResourceManager* resources, const SceneRenderComponent& render,
                    const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale, float alpha) {
    switch (render.shape) {
        case SceneShape::Box:
            r.DrawBox(position, rotation, render.halfExtents, render.color, alpha);
            break;
        case SceneShape::Sphere:
            r.DrawSphere(position, render.radius, render.color, alpha);
            break;
        case SceneShape::Mesh:
            DrawMeshOrPlaceholder(r, resources, render, position, rotation, scale, alpha);
            break;
        case SceneShape::Compound:
        case SceneShape::Terrain:
            break;  // handled by their owners
    }
}
}  // namespace

void DrawWorldGeometry(Renderer& r, const RuntimeWorld& world, const GameSession* session,
                       float alpha, const WorldDrawOptions& options) {
    for (const RuntimeWorld::StaticRenderable& s : world.StaticRenderables()) {
        DrawRenderable(r, world.Resources(), s.render, s.position, s.rotation, s.scale, 1.0f);
    }
    if (options.includeTerrain) {
        for (const RuntimeWorld::Terrain& t : world.Terrains()) {
            if (t.mesh.IsValid()) {
                r.DrawMesh(t.mesh, t.position, t.rotation, glm::vec3(1.0f), TextureHandle{}, t.color);
            }
        }
    }
    for (const Door& door : world.Doors()) door.Draw(r, alpha);
    for (const LightSwitch& lightSwitch : world.LightSwitches()) lightSwitch.Draw(r, alpha);

    if (session) {
        // Milestone 11: while attached, the pilot is rendered from the
        // SAME presented vehicle pose the camera and vehicle mesh use.
        glm::vec3 playerPosition;
        glm::quat playerOrientation;
        const PlayerController& player = session->Player();
        if (session->IsPiloting() && session->Attachment().attached) {
            const DynamicBody& ship = world.DynamicBodies()[world.GetVehicle()->dynamicIndex];
            const BodyTransform presented{ship.GetPresentedPosition(alpha), ship.GetPresentedOrientation(alpha)};
            ApplyPilotAttachment(session->Attachment(), presented, playerPosition, playerOrientation);
        } else {
            playerPosition = player.GetPresentedPosition(alpha);
            playerOrientation = player.GetPresentedOrientation(alpha);
        }
        const bool visible = session->ViewMode() == PlayerViewMode::ThirdPerson || session->IsPiloting();
        if (options.includePlayerModel && visible) {
            r.DrawBox(playerPosition, playerOrientation, player.GetRenderHalfExtents(), kPlayerColor);
        }
    }

    const std::vector<DynamicBody>& bodies = world.DynamicBodies();
    const std::vector<RuntimeWorld::DynamicVisual>& visuals = world.DynamicVisuals();
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const RuntimeWorld::DynamicVisual& v = visuals[i];
        if (!v.hasRender) continue;
        const glm::vec3 position = bodies[i].GetPresentedPosition(alpha);
        const glm::quat rotation = bodies[i].GetPresentedOrientation(alpha);
        if (v.render.shape == SceneShape::Compound) {
            // Opaque base in the colour pass; every part in a shadow pass
            // so translucent walls still cast shadows (M24).
            for (std::size_t part = 0; part < v.compoundBoxes.size(); ++part) {
                if (part != 0 && !r.IsShadowPass()) continue;
                const CompoundBox& box = v.compoundBoxes[part];
                r.DrawBox(position + rotation * box.localCenter, rotation, box.halfExtents,
                          part == 0 ? v.render.color : v.render.secondaryColor);
            }
            continue;
        }
        DrawRenderable(r, world.Resources(), v.render, position, rotation, v.scale, 1.0f);
    }

    // The fluid receives ordinary lighting/shadows in the colour pass; its
    // constantly rebuilt surface does not occupy the depth maps.
    if (world.HasFluid() && !world.Fluid().Particles().empty() && world.FluidMesh().IsValid() &&
        !r.IsShadowPass()) {
        r.DrawMesh(world.FluidMesh(), glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f),
                   TextureHandle{}, glm::vec3(0.12f, 0.48f, 0.82f));
    }
}

void DrawWorldTransparents(Renderer& r, const RuntimeWorld& world, const GameSession* session, float alpha) {
    const std::vector<DynamicBody>& bodies = world.DynamicBodies();
    const std::vector<RuntimeWorld::DynamicVisual>& visuals = world.DynamicVisuals();

    bool anyCompound = false;
    for (const RuntimeWorld::DynamicVisual& v : visuals) {
        if (v.hasRender && v.render.shape == SceneShape::Compound && v.compoundBoxes.size() > 1) anyCompound = true;
    }
    if (anyCompound) {
        r.BeginTransparentPass();
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const RuntimeWorld::DynamicVisual& v = visuals[i];
            if (!v.hasRender || v.render.shape != SceneShape::Compound) continue;
            const glm::vec3 position = bodies[i].GetPresentedPosition(alpha);
            const glm::quat rotation = bodies[i].GetPresentedOrientation(alpha);
            for (std::size_t part = 1; part < v.compoundBoxes.size(); ++part) {
                const CompoundBox& box = v.compoundBoxes[part];
                r.DrawBox(position + rotation * box.localCenter, rotation, box.halfExtents,
                          v.render.secondaryColor, v.render.secondaryAlpha);
            }
        }
        r.EndTransparentPass();
    }

    if (world.GetAtmosphere()) {
        // Two faint shells mark the extent of the gas field; they are not
        // density, pressure or a collision boundary.
        const AtmosphereParameters& p = world.GetAtmosphere()->field.Parameters();
        const float base = p.referenceRadius;
        const float thickness = p.topRadius - base;
        const glm::vec3 centre = world.GetAtmosphere()->frame.originPosition;
        r.BeginTransparentPass();
        r.DrawSphere(centre, base + 0.90f * thickness, glm::vec3(0.35f, 0.58f, 0.82f), 0.035f);
        r.DrawSphere(centre, base + 0.40f * thickness, glm::vec3(0.35f, 0.62f, 0.88f), 0.045f);
        r.EndTransparentPass();
    }

    if (!world.Combustibles().empty() && world.GetAtmosphere()) {
        const RuntimeWorld::Atmosphere& atmosphere = *world.GetAtmosphere();
        r.BeginTransparentPass();
        for (const RuntimeWorld::Combustible& c : world.Combustibles()) {
            const ThermalBodyState* state = world.Combustion().State(c.handle);
            if (!state || state->burnRateKgPerSecond <= 0.0f) continue;
            const DynamicBody& body = bodies[c.dynamicIndex];
            const glm::vec3 position = body.GetPresentedPosition(alpha);
            const AtmosphereSample gas = atmosphere.field.Sample(position, atmosphere.frame);
            FirePresentationInput input;
            input.position = position;
            input.orientation = body.GetPresentedOrientation(alpha);
            input.burnRateKgPerSecond = state->burnRateKgPerSecond;
            input.temperatureKelvin = world.Combustion().PresentedTemperature(c.handle, alpha);
            input.bodyVelocity = world.Physics().GetLinearVelocity(c.handle);
            input.gasVelocity = gas.velocity;
            input.gasDensity = gas.density;
            input.gravityAcceleration = CelestialGravity::AccelerationFromPointMass(
                atmosphere.frame.originPosition, atmosphere.gravitationalParameter, position);
            input.sourceRadius = c.sourceRadius;
            for (const FireVisualPrimitive& primitive : BuildFirePresentation(input)) {
                r.DrawSphere(primitive.position, primitive.radius, primitive.color, primitive.alpha);
            }
        }
        r.EndTransparentPass();
    }

    if (session && session->IgniterPowered()) {
        glm::vec3 eye, look;
        session->Player().GetTorchTransform(alpha, eye, look);
        r.BeginTransparentPass();
        r.DrawSphere(eye + look * 1.5f, 0.08f, glm::vec3(1.0f, 0.46f, 0.10f), 0.8f);
        r.EndTransparentPass();
    }
}

std::vector<DynamicLight> BuildWorldLights(const RuntimeWorld& world, const GameSession* session, float alpha) {
    std::vector<DynamicLight> lights;
    if (session && session->TorchOn()) {
        DynamicLight torch;
        torch.kind = LightKind::Spot;
        session->Player().GetTorchTransform(alpha, torch.position, torch.direction);
        torch.color = kTorchColor;
        torch.range = kTorchRange;
        torch.innerConeDegrees = kTorchInnerConeDegrees;
        torch.outerConeDegrees = kTorchOuterConeDegrees;
        torch.shadowMapIndex = kTorchShadowSlot;
        lights.push_back(torch);
    }
    if (world.GetVehicle()) {
        const RuntimeWorld::Vehicle& vehicle = *world.GetVehicle();
        const DynamicBody& ship = world.DynamicBodies()[vehicle.dynamicIndex];
        const glm::vec3 position = ship.GetPresentedPosition(alpha);
        const glm::quat orientation = ship.GetPresentedOrientation(alpha);
        if (vehicle.component.headlight) {
            DynamicLight headlight;
            headlight.kind = LightKind::Spot;
            headlight.position = TransformLocalLightPosition(position, orientation,
                                                             glm::vec3(0.0f, 0.0f, -vehicle.halfExtents.z));
            headlight.direction = TransformLocalLightDirection(orientation, glm::vec3(0.0f, 0.0f, -1.0f));
            headlight.color = kShipHeadlightColor;
            headlight.range = kShipHeadlightRange;
            headlight.innerConeDegrees = kShipHeadlightInnerConeDegrees;
            headlight.outerConeDegrees = kShipHeadlightOuterConeDegrees;
            headlight.shadowMapIndex = kShipHeadlightShadowSlot;
            lights.push_back(headlight);
        }
        if (vehicle.component.navigationLights) {
            DynamicLight port;
            port.kind = LightKind::Point;
            port.position = TransformLocalLightPosition(position, orientation,
                                                        glm::vec3(-vehicle.halfExtents.x, 0.0f, 0.0f));
            port.color = kShipPortLightColor;
            port.range = kShipNavLightRange;
            lights.push_back(port);
            DynamicLight starboard;
            starboard.kind = LightKind::Point;
            starboard.position = TransformLocalLightPosition(position, orientation,
                                                             glm::vec3(vehicle.halfExtents.x, 0.0f, 0.0f));
            starboard.color = kShipStarboardLightColor;
            starboard.range = kShipNavLightRange;
            lights.push_back(starboard);
        }
    }
    for (const LightSwitch& lightSwitch : world.LightSwitches()) {
        if (!lightSwitch.IsLampOn()) continue;
        DynamicLight lamp;
        lamp.kind = LightKind::Point;
        lamp.position = lightSwitch.GetLampPosition();
        lamp.color = lightSwitch.GetLampColor();
        lamp.range = lightSwitch.GetLampRange();
        lights.push_back(lamp);
    }
    for (const RuntimeWorld::StaticLight& s : world.StaticLights()) {
        DynamicLight light;
        light.kind = s.light.kind == SceneLightKind::Spot ? LightKind::Spot : LightKind::Point;
        light.position = s.position;
        light.direction = s.direction;
        light.color = s.light.color;
        light.range = s.light.range;
        light.innerConeDegrees = s.light.innerConeDegrees;
        light.outerConeDegrees = s.light.outerConeDegrees;
        lights.push_back(light);
    }
    return lights;
}

void UpdateFluidSurface(Renderer& renderer, const RuntimeWorld& world, float alpha) {
    if (!world.HasFluid() || !world.FluidMesh().IsValid() || world.Fluid().Particles().empty()) return;
    std::vector<glm::vec3> positions;
    positions.reserve(world.Fluid().Particles().size());
    for (std::size_t i = 0; i < world.Fluid().Particles().size(); ++i) {
        positions.push_back(world.Fluid().PresentedPosition(i, alpha));
    }
    // Surface extraction scales with the solver's own smoothing radius:
    // the M24 cup values at scale 1, the M25 lake values at scale 10.
    const float scale = world.Settings().fluidScale;
    const float smoothing = scale > 1.0f ? world.FluidSettingsUsed().smoothingRadius : 0.105f;
    const float cell = scale > 1.0f ? 0.40f : 0.05f;
    renderer.UpdateMeshVertices(world.FluidMesh(), BuildFluidSurface(positions, smoothing, cell, 0.45f));
}

void RenderWorldFrame(Renderer& renderer, int width, int height, const RuntimeWorld& world,
                      const GameSession* session, const glm::mat4& view, const glm::mat4& projection,
                      const glm::vec3& shadowFocus, float alpha) {
    const SceneSettings& settings = world.Settings();
    renderer.SetLighting(glm::normalize(settings.sunDirection), settings.sunColor, settings.ambientColor);
    const std::vector<DynamicLight> lights = BuildWorldLights(world, session, alpha);

    // Milestone 15 shadow passes: the directional sun always, then each
    // shadow-casting spot light in the frame's list.
    const glm::mat4 dirShadow = ComputeDirectionalShadowMatrix(
        shadowFocus, glm::normalize(settings.sunDirection), kDirShadowHalfExtent, kDirShadowDistance);
    renderer.BeginShadowPass(kDirectionalShadowSlot, dirShadow);
    DrawWorldGeometry(renderer, world, session, alpha, WorldDrawOptions{});
    renderer.EndShadowPass();
    for (const DynamicLight& light : lights) {
        if (light.shadowMapIndex != kTorchShadowSlot && light.shadowMapIndex != kShipHeadlightShadowSlot) continue;
        const glm::mat4 spotShadow = ComputeSpotShadowMatrix(light.position, light.direction,
                                                             light.outerConeDegrees, light.range);
        renderer.BeginShadowPass(light.shadowMapIndex, spotShadow);
        WorldDrawOptions options;
        // The torch sits inside the player's own body box: exclude it from
        // only the torch's pass (M15 post-validation fix).
        options.includePlayerModel = light.shadowMapIndex != kTorchShadowSlot;
        // A whole planet far outside a small light's range costs a depth
        // pass for nothing.
        options.includeTerrain = true;
        for (const RuntimeWorld::Terrain& t : world.Terrains()) {
            if (glm::distance(light.position, t.position) > light.range + t.surface->BoundRadius()) {
                options.includeTerrain = false;
            }
        }
        DrawWorldGeometry(renderer, world, session, alpha, options);
        renderer.EndShadowPass();
    }

    renderer.BeginFrame(width, height);
    renderer.SetCamera(view, projection);
    renderer.SetDynamicLights(lights);
    DrawWorldGeometry(renderer, world, session, alpha, WorldDrawOptions{});
    DrawWorldTransparents(renderer, world, session, alpha);
    renderer.EndFrame();
}

void DrawAuthoredScene(Renderer& r, const Scene& scene, ResourceManager& assets) {
    for (const SceneObject& o : scene.Objects()) {
        const glm::vec3 position = o.transform.position;
        const glm::quat rotation = glm::normalize(o.transform.rotation);
        if (o.render) {
            const SceneRenderComponent& render = *o.render;
            if (render.shape == SceneShape::Mesh) {
                // Edit mode expresses demand by requesting each frame (a
                // hit once loaded); the editor holds the references.
                assets.RequestMesh(render.meshAsset);
                if (!render.textureAsset.empty()) assets.RequestTexture(render.textureAsset);
                DrawMeshOrPlaceholder(r, &assets, render, position, rotation, o.transform.scale, 1.0f);
            } else if (render.shape == SceneShape::Compound && o.body) {
                for (std::size_t part = 0; part < o.body->compoundBoxes.size(); ++part) {
                    const CompoundBox& box = o.body->compoundBoxes[part];
                    r.DrawBox(position + rotation * box.localCenter, rotation, box.halfExtents,
                              part == 0 ? render.color : render.secondaryColor);
                }
            } else if (render.shape == SceneShape::Terrain && o.body) {
                const std::shared_ptr<const RadialTerrain> surface = CreateTerrainSurface(o.body->terrainSurface);
                if (surface) {
                    const MeshHandle mesh = assets.GetTerrainMesh(o.body->terrainSurface, *surface);
                    if (mesh.IsValid()) {
                        r.DrawMesh(mesh, position, rotation, glm::vec3(1.0f), TextureHandle{}, render.color);
                    }
                }
            } else if (o.door) {
                // A closed door's panel hangs from its hinge edge (Door.h).
                r.DrawBox(position + rotation * glm::vec3(render.halfExtents.x, 0.0f, 0.0f), rotation,
                          render.halfExtents, render.color);
            } else if (o.lightSwitch) {
                r.DrawBox(position + rotation * glm::vec3(render.halfExtents.x, 0.0f, 0.0f), rotation,
                          render.halfExtents, render.color);
            } else {
                DrawRenderable(r, &assets, render, position, rotation, o.transform.scale, 1.0f);
            }
        }
        // Authored liquid is shown as its particle lattice bounds.
        if (o.fluidVolume && !r.IsShadowPass()) {
            const SceneFluidVolumeComponent& f = *o.fluidVolume;
            const glm::vec3 half(0.5f * f.spacing * static_cast<float>(f.countX),
                                 0.5f * f.spacing * static_cast<float>(f.countY),
                                 0.5f * f.spacing * static_cast<float>(f.countZ));
            r.DrawBox(position + rotation * glm::vec3(0.0f, half.y - 0.5f * f.spacing, 0.0f), rotation, half,
                      glm::vec3(0.12f, 0.48f, 0.82f));
        }
    }
}

std::vector<DynamicLight> BuildAuthoredLights(const Scene& scene) {
    std::vector<DynamicLight> lights;
    for (const SceneObject& o : scene.Objects()) {
        if (!o.light) continue;
        DynamicLight light;
        light.kind = o.light->kind == SceneLightKind::Spot ? LightKind::Spot : LightKind::Point;
        light.position = o.transform.position;
        light.direction = glm::normalize(glm::normalize(o.transform.rotation) * glm::vec3(0.0f, 0.0f, -1.0f));
        light.color = o.light->color;
        light.range = o.light->range;
        light.innerConeDegrees = o.light->innerConeDegrees;
        light.outerConeDegrees = o.light->outerConeDegrees;
        lights.push_back(light);
    }
    return lights;
}
