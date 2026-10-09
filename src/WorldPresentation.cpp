#include "PerformanceProfiler.h"
#include "WorldPresentation.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>

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

std::vector<MaterialBinding> BuildRenderMaterialBindings(ResourceManager* resources,const SceneRenderComponent& render) {
    const auto* parts=resources?resources->TryGetModelParts(render.meshAsset):nullptr;
    const size_t count=parts&&!parts->empty()?parts->size():1;
    std::vector<MaterialBinding> bindings;bindings.reserve(count);
    for(size_t i=0;i<count;++i){
        auto slot=ResolveRenderMaterial(render,unsigned(i),parts&&!parts->empty()?(*parts)[i].part:std::string{});
        if(resources&&!slot.asset.empty())resources->RequestMaterial(slot.asset);
        MaterialBinding binding{resources?resources->TryGetMaterial(slot.asset):MaterialHandle{},slot.overrides,!slot.asset.empty(),resources&&resources->StateOf(slot.asset)==ResourceState::Failed};
        for(unsigned map=0;map<5;++map)if(slot.overrides.textures[map]){
            const auto& asset=*slot.overrides.textures[map];
            if(asset.empty())binding.textureOverrides[map]=true;
            else if(resources){
                resources->RequestTexture(asset);
                binding.textures[map]=resources->TryGetTexture(asset);
                binding.textureOverrides[map]=binding.textures[map].IsValid();
            }
        }
        bindings.push_back(std::move(binding));
    }
    return bindings;
}

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

// The primary Render component and particles/fire/haze are distinct visual
// components. An entity gate affects all of them; the primary gate affects only
// its own geometry, leaving the other components' existing settings intact.
bool EntityRenderVisible(const RuntimeWorld& world,EntityId id){
    const auto* definition=world.RuntimeDefinition(id);return definition&&definition->renderVisible;
}

void DrawMeshOrPlaceholder(Renderer& r, ResourceManager* resources, const SceneRenderComponent& render,
                           const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale, float alpha, TextureHandle generated = {},const std::vector<glm::mat4>* skin = nullptr) {
    r.SetMaterialBindings(BuildRenderMaterialBindings(resources,render));
    const MeshHandle mesh = resources ? resources->TryGetMesh(render.meshAsset) : MeshHandle{};
    if (mesh.IsValid()) {
        const TextureHandle texture = render.textureCamera ? generated : (resources ? resources->TryGetTexture(render.textureAsset) : TextureHandle{});
        r.DrawMesh(mesh, position, rotation, scale, texture, render.color, alpha*render.alpha,skin,&render.hiddenParts);
        return;
    }
    if (!resources) return;  // headless: nothing to draw
    const bool failed = resources->StateOf(render.meshAsset) == ResourceState::Failed;
    r.DrawBox(position, rotation, scale * 0.5f, failed ? kFailedPlaceholderColor : kLoadingPlaceholderColor, alpha);
}

void DrawCompoundChild(Renderer& renderer,ResourceManager* resources,const CompoundBox& child,glm::vec3 position,glm::quat rotation,glm::vec3 color,float alpha=1){
 position+=rotation*child.localCenter;rotation=glm::normalize(rotation*child.rotation);
 if(child.type==ShapeType::Box)renderer.DrawBox(position,rotation,child.halfExtents,color,alpha);
 else if(child.type==ShapeType::Sphere)renderer.DrawSphereTransformed(position,rotation,glm::vec3(1),child.radius,color,alpha,TextureHandle{});
 else if(child.type==ShapeType::ConvexHull&&resources){auto mesh=resources->TryGetCollisionMesh(child.assetId);if(mesh.IsValid())renderer.DrawMesh(mesh,position,rotation,glm::vec3(1),TextureHandle{},color,alpha);}
}

void DrawRenderable(Renderer& r, ResourceManager* resources, const SceneRenderComponent& render,
                    const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale, float alpha, TextureHandle generated = {},const std::vector<glm::mat4>* skin = nullptr) {
    if(!render.visible)return;
    if(render.shape!=SceneShape::Mesh)r.SetMaterialBindings(BuildRenderMaterialBindings(resources,render));
    switch (render.shape) {
        case SceneShape::Box:
            r.DrawBox(position, rotation, render.halfExtents*scale, render.color, alpha*render.alpha, generated);
            break;
        case SceneShape::Sphere:
            r.DrawSphereTransformed(position,rotation,scale,render.radius,render.color,alpha*render.alpha,generated);
            break;
        case SceneShape::Mesh:
            DrawMeshOrPlaceholder(r, resources, render, position, rotation, scale, alpha, generated,skin);
            break;
        case SceneShape::Compound:
        case SceneShape::Terrain:
        case SceneShape::ConvexHull:
        case SceneShape::TriangleMesh:
            break;  // handled by their owners
    }
}
}  // namespace

void DrawWorldGeometry(Renderer& r, const RuntimeWorld& world, const GameSession* session,
                       float alpha, const WorldDrawOptions& options) {
    JUDAS_PROFILE_SCOPE("World culling and opaque submission");
    for (const RuntimeWorld::StaticRenderable& s : world.StaticRenderables()) {
        if(!world.IsPublished(s.id)||!world.RenderVisible(s.id)||world.RuntimeDefinition(s.id)->deformable)continue;
        r.SetRenderLayer(world.RenderLayerOf(s.id));
        const auto t=world.PresentedTransform(s.id,SceneTransform{s.position,s.rotation,s.scale},alpha);
        DrawRenderable(r, world.Resources(), s.render, t.position, t.rotation, t.scale, 1.0f, world.CameraTexture(s.render.textureCamera),world.AnimationSkin(s.id,alpha));
    }
    world.DrawDeformables(r,alpha);
    r.SetMaterialBindings({});
    if (options.includeTerrain) {
        for (const RuntimeWorld::Terrain& t : world.Terrains()) {
            if(!world.RenderVisible(t.id))continue;
            r.SetRenderLayer(world.RenderLayerOf(t.id));
            const auto* definition=world.RuntimeDefinition(t.id);
            r.SetMaterialBindings(definition&&definition->render?BuildRenderMaterialBindings(world.Resources(),*definition->render):std::vector<MaterialBinding>{});
            if (t.mesh.IsValid()) {
                r.DrawMesh(t.mesh, t.position, t.rotation, glm::vec3(1.0f), TextureHandle{}, t.color);
            }
        }
    }
    r.SetMaterialBindings({});
    for(size_t i=0;i<world.Doors().size();++i){auto id=world.DoorIds()[i];if(!world.RenderVisible(id))continue;r.SetRenderLayer(world.RenderLayerOf(id));const auto* d=world.RuntimeDefinition(id);r.SetMaterialBindings(d&&d->render?BuildRenderMaterialBindings(world.Resources(),*d->render):std::vector<MaterialBinding>{});world.Doors()[i].Draw(r,alpha);}
    for(size_t i=0;i<world.LightSwitches().size();++i){auto id=world.LightSwitchIds()[i];if(!world.RenderVisible(id))continue;r.SetRenderLayer(world.RenderLayerOf(id));const auto* d=world.RuntimeDefinition(id);r.SetMaterialBindings(d&&d->render?BuildRenderMaterialBindings(world.Resources(),*d->render):std::vector<MaterialBinding>{});world.LightSwitches()[i].Draw(r,alpha);}
    r.SetMaterialBindings({});
    r.SetRenderLayer(0);

    if (session && session->UsesLegacyGameplay() && !world.view) {
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
        r.SetRenderLayer(world.RenderLayerOf(v.id));
        if (!v.hasRender||!world.IsPublished(v.id)||!world.RenderVisible(v.id)) continue;
        const glm::vec3 position = bodies[i].GetPresentedPosition(alpha);
        const glm::quat rotation = bodies[i].GetPresentedOrientation(alpha);
        if (v.render.shape == SceneShape::Compound) {
            r.SetMaterialBindings(BuildRenderMaterialBindings(world.Resources(),v.render));
            // Opaque base in the colour pass; every part in a shadow pass
            // so translucent walls still cast shadows (M24).
            for (std::size_t part = 0; part < v.compoundBoxes.size(); ++part) {
                if (part != 0 && !r.IsShadowPass()) continue;
                const CompoundBox& box = v.compoundBoxes[part];
                DrawCompoundChild(r,world.Resources(),box,position,rotation,part==0?v.render.color:v.render.secondaryColor,v.render.alpha);
            }
            continue;
        }
        DrawRenderable(r, world.Resources(), v.render, position, rotation, v.scale, 1.0f, world.CameraTexture(v.render.textureCamera),world.AnimationSkin(v.id,alpha));
    }

    r.SetMaterialBindings({});
    r.SetRenderLayer(0);
    // The fluid receives ordinary lighting/shadows in the colour pass; its
    // constantly rebuilt surface does not occupy the depth maps.
    if (world.HasFluid() && !world.Fluid().Particles().empty() && world.FluidMesh().IsValid() &&
        !r.IsShadowPass()) {
        r.DrawMesh(world.FluidMesh(), glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f),
                   TextureHandle{}, glm::vec3(0.12f, 0.48f, 0.82f));
    }
}

void DrawWorldTransparents(Renderer& r, const RuntimeWorld& world, const GameSession* session, float alpha) {
    JUDAS_PROFILE_SCOPE("Particles and water presentation");
    r.FlushMaterialBlends();r.SetMaterialBindings({});
    const std::vector<DynamicBody>& bodies = world.DynamicBodies();
    const std::vector<RuntimeWorld::DynamicVisual>& visuals = world.DynamicVisuals();
    if(!world.Liquids().States().empty()){
      r.BeginTransparentPass();
      for(const auto& [id,s]:world.Liquids().States())if(s.enabled&&s.equilibriumValid&&s.volume>0&&(!s.entity||EntityRenderVisible(world,s.entity))){
        auto pose=s.pose;if(s.container)if(auto* o=world.RuntimeDefinition(s.entity))pose=world.PresentedTransform(s.entity,o->transform,alpha);
        VisualBounds localBounds;localBounds.Include(glm::vec3(s.minimum));localBounds.Include(glm::vec3(s.maximum));
        if(!r.AllowsLayer(world.RenderLayerOf(s.entity))||!r.IsVisible(TransformBounds(localBounds,glm::translate(glm::mat4(1),pose.position)*glm::mat4_cast(pose.rotation))))continue;
        MeshData mesh=s.dynamicSurface?s.dynamicSurface->Mesh(alpha):s.surface;for(auto& v:mesh.vertices){v.position=pose.position+pose.rotation*v.position;v.normal=pose.rotation*v.normal;}r.SetRenderLayer(world.RenderLayerOf(s.entity));r.DrawTransientSurface(mesh,{.035f,.34f,.72f},.76f);}
      r.SetRenderLayer(0);for(const auto& [id,p]:world.Liquids().Parcels())r.DrawSphere(p.position,float(std::cbrt(3*p.volume/(4*3.141592653589793))),{.05f,.5f,.9f},.85f);
      r.EndTransparentPass();
    }

    bool anyCompound = false;
    for (const RuntimeWorld::DynamicVisual& v : visuals) {
        if (v.hasRender && world.RenderVisible(v.id)&&v.render.shape == SceneShape::Compound && v.compoundBoxes.size() > 1) anyCompound = true;
    }
    if (anyCompound) {
        r.BeginTransparentPass();
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const RuntimeWorld::DynamicVisual& v = visuals[i];
        r.SetRenderLayer(world.RenderLayerOf(v.id));
            if (!world.IsPublished(v.id)||!world.RenderVisible(v.id)||!v.hasRender || v.render.shape != SceneShape::Compound) continue;
            r.SetMaterialBindings(BuildRenderMaterialBindings(world.Resources(),v.render));
            const glm::vec3 position = bodies[i].GetPresentedPosition(alpha);
            const glm::quat rotation = bodies[i].GetPresentedOrientation(alpha);
            for (std::size_t part = 1; part < v.compoundBoxes.size(); ++part) {
                const CompoundBox& box = v.compoundBoxes[part];
                DrawCompoundChild(r,world.Resources(),box,position,rotation,v.render.secondaryColor,v.render.secondaryAlpha);
            }
        }
        r.EndTransparentPass();
    }
    r.SetMaterialBindings({});

    if (world.GetAtmosphere()&&EntityRenderVisible(world,world.GetAtmosphere()->id)) {
        r.SetRenderLayer(world.RenderLayerOf(world.GetAtmosphere()->id));
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
            if(!EntityRenderVisible(world,c.id))continue;
            r.SetRenderLayer(world.RenderLayerOf(c.id));
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

    // Runtime visual state is updated once in Simulation, never per camera.
    for(auto& emitter:world.VisualEmitters()){
        if(!world.IsPublished(emitter.id)||!EntityRenderVisible(world,emitter.id))continue;
        r.SetRenderLayer(world.RenderLayerOf(emitter.id));
        if(!emitter.pool.settings.enabled)continue;
        if(const auto* entity=world.FindEntity(emitter.id))if(entity->lifecycle==EntityLifecycle::Destroyed)continue;
        const auto t=world.PresentedTransform(emitter.id,emitter.transform,alpha);
        const auto& particles=emitter.pool.Presentation(t.position,t.rotation,t.scale);
        const auto texture=world.Resources()?world.Resources()->TryGetTexture(emitter.pool.settings.textureAsset):TextureHandle{};
        r.DrawParticles(particles,emitter.pool.Bounds(),texture);
    }

    r.SetRenderLayer(0);
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
    if (world.legacyGameplay && world.GetVehicle()) {
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
        if(!world.IsPublished(s.id))continue;
        DynamicLight light;
        light.kind = s.light.kind == SceneLightKind::Spot ? LightKind::Spot : LightKind::Point;
        const auto t=world.PresentedTransform(s.id,SceneTransform{s.position,glm::quat(1,0,0,0),glm::vec3(1)},alpha);
        light.position = t.position;
        light.direction = t.rotation*glm::vec3(0,0,-1);
        if(t.rotation==glm::quat(1,0,0,0))light.direction=s.direction;
        light.color = s.light.color;
        light.range = s.light.range;
        light.innerConeDegrees = s.light.innerConeDegrees;
        light.outerConeDegrees = s.light.outerConeDegrees;
        lights.push_back(light);
    }
    return lights;
}

void UpdateFluidSurface(Renderer& renderer, const RuntimeWorld& world, float /*alpha*/) {
    JUDAS_PROFILE_SCOPE("Legacy fluid surface update");
    // 30 Hz particle state is held; rigid interpolation alpha would replay the
    // same fluid interval twice. Presentation never feeds the field sampler.
    if (!world.HasFluid() || !world.FluidMesh().IsValid() || !world.FluidSurfaceDirty()) return;
    std::vector<glm::vec3> positions;
    positions.reserve(world.Fluid().Particles().size());
    for (std::size_t i = 0; i < world.Fluid().Particles().size(); ++i) {
        positions.push_back(world.Fluid().Particles()[i].position);
    }
    // Presentation resolution follows particle spacing. The former fixed
    // 0.40 m grid over-refined coarser fields (cubic extraction work), even
    // though their simulation contained less detail. Preserve scale-1 and
    // scale-10 reference resolutions; this never affects authoritative water.
    const float scale = world.Settings().fluidScale;
    const float smoothing = scale > 1.0f ? world.FluidSettingsUsed().smoothingRadius : 0.105f;
    const float cell = scale > 1.0f ? std::max(0.05f, 0.04f * scale) : 0.05f;
    renderer.UpdateMeshVertices(world.FluidMesh(), BuildFluidSurface(positions, smoothing, cell, 0.45f));
    // The latest liquid state is held between executed liquid updates. Reuse
    // its exact mesh; a new/reset world owns a separate invalidation marker.
    world.MarkFluidSurfaceUploaded();
}

void RenderWorldFrame(Renderer& renderer, int width, int height, const RuntimeWorld& world,
                      const GameSession* session, const glm::mat4& view, const glm::mat4& projection,
                      const glm::vec3& shadowFocus, float alpha) {
    JUDAS_PROFILE_SCOPE("World render submission");
    // Existing runtime statistics may accumulate across frames. Observe this
    // submission's delta without resetting the engine's diagnostic state.
    const auto beforeStats = renderer.Stats();
    // One sample owns one immutable lighting/environment decision. Auxiliary
    // cameras never run scripts or advance an animation/fade between passes.
    const SceneSettings settings = world.Settings();
    if(world.Resources())world.Resources()->RequestEnvironment(settings.environmentAsset);
    renderer.SetSceneAppearance(settings.linearRendering,settings.exposure,world.Resources()?world.Resources()->TryGetEnvironment(settings.environmentAsset):EnvironmentHandle{},settings.environmentIntensity,settings.environmentRotation,settings.environmentBackground,settings.backgroundColor);
    renderer.SetLighting(settings.sunDirection, settings.sunEnabled?settings.sunColor*settings.sunIntensity:glm::vec3(0), settings.ambientColor);
    const std::vector<DynamicLight> lights = BuildWorldLights(world, session, alpha);

    // Milestone 15 shadow passes: the directional sun always, then each
    // shadow-casting spot light in the frame's list.
    const glm::mat4 dirShadow = ComputeDirectionalShadowMatrix(
        world.view ? world.view->pose.position : shadowFocus, settings.sunDirection, kDirShadowHalfExtent, kDirShadowDistance);
    static ProfileLabel shadowLabel("Shadow pass submission");
    ProfileScope shadowScope(shadowLabel);
    if(settings.sunEnabled){renderer.BeginShadowPass(kDirectionalShadowSlot, dirShadow);
        DrawWorldGeometry(renderer, world, session, alpha, WorldDrawOptions{});
        renderer.EndShadowPass();
    }else renderer.InvalidateShadowSlot(kDirectionalShadowSlot);
    shadowScope.End();
    for (const DynamicLight& light : lights) {
        if (light.shadowMapIndex != kTorchShadowSlot && light.shadowMapIndex != kShipHeadlightShadowSlot) continue;
        ProfileScope spotlightScope(shadowLabel);
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

    // Secondary views are presentation passes only; shadow maps above are shared.
    if (!world.PresentationCameras().empty()) {
        world.BindCameraRenderer(renderer);
        const auto frame = world.NextCameraFrame();
        for (auto& camera : world.PresentationCameras()) {
            if(!world.IsPublished(camera.id))continue;
            if (!camera.settings.enabled || frame % camera.settings.updateEveryFrames != 0) continue;
            glm::vec3 position = camera.transform.position;
            glm::quat rotation = glm::normalize(camera.transform.rotation);
            if (const auto* entity = world.FindEntity(camera.id)) {
                if (entity->lifecycle == EntityLifecycle::Destroyed) {
                    renderer.DestroyRenderTarget(camera.target); camera.target = {}; continue;
                }
                if (entity->lifecycle != EntityLifecycle::Active) continue;
                const auto t=world.PresentedTransform(camera.id,camera.transform,alpha);
                position=t.position;rotation=t.rotation;
            }
            if (camera.staticBody.IsValid()) {
                const auto pose = world.Physics().GetTransform(camera.staticBody);
                position = pose.position; rotation = pose.rotation;
            }
            const auto presented=world.PresentedTransform(camera.id,SceneTransform{position,rotation,camera.transform.scale},alpha);
            position=presented.position;rotation=presented.rotation;
            const auto& c = camera.settings;
            if (c.width != camera.attemptedWidth || c.height != camera.attemptedHeight || camera.attemptedLinear!=settings.linearRendering) camera.allocationAttempted = false;
            if (!camera.allocationAttempted) {
                camera.allocationAttempted = true;
                camera.attemptedLinear=settings.linearRendering;camera.attemptedWidth = c.width; camera.attemptedHeight = c.height;
                renderer.ResizeRenderTarget(camera.target, c.width, c.height, camera.error);
                if (!camera.error.empty()) std::fprintf(stderr, "camera %llu: %s\n", static_cast<unsigned long long>(camera.id), camera.error.c_str());
            }
            if (!camera.error.empty() || !renderer.BeginRenderTarget(camera.target)) continue;
            JUDAS_PROFILE_SCOPE("Secondary camera");
            RendererProfileScope cameraGPU(renderer,"Secondary camera",camera.id);
            renderer.SetCamera(glm::lookAt(position, position + rotation * glm::vec3(0,0,-1), rotation * glm::vec3(0,1,0)),
                glm::perspective(glm::radians(c.verticalFovDegrees), static_cast<float>(c.width)/c.height, c.nearPlane, c.farPlane));
            renderer.SetRenderMask(c.renderMask);
            renderer.SetDynamicLights(lights);
            renderer.SetWaterPaths(32,18,world.Liquids().OpticalPaths(renderer.ViewMatrix(),renderer.ProjectionMatrix(),32,18,alpha));
            DrawWorldGeometry(renderer, world, session, alpha, WorldDrawOptions{});
            DrawWorldTransparents(renderer, world, session, alpha);
            renderer.EndRenderTarget(); ++camera.updates;
        }
    }
    renderer.BeginFrame(width, height);
    JUDAS_PROFILE_SCOPE("Main camera");
    RendererProfileScope mainGPU(renderer,"Main camera");
    if(world.view){const auto& v=*world.view;auto q=v.pose.rotation;
        renderer.SetCamera(glm::lookAt(v.pose.position,v.pose.position+q*glm::vec3(0,0,-1),q*glm::vec3(0,1,0)),glm::perspective(glm::radians(v.fov),float(width)/height,v.nearPlane,v.farPlane));
    }else renderer.SetCamera(view, projection);
    renderer.SetRenderMask(settings.mainCameraRenderMask);
    renderer.SetDynamicLights(lights);
    renderer.SetWaterPaths(32,18,world.Liquids().OpticalPaths(renderer.ViewMatrix(),renderer.ProjectionMatrix(),32,18,alpha));
    DrawWorldGeometry(renderer, world, session, alpha, WorldDrawOptions{});
    DrawWorldTransparents(renderer, world, session, alpha);
    renderer.EndFrame();
    const auto& stats=renderer.Stats();
    JUDAS_PROFILE_COUNTER("Submitted draws all passes",stats.drawCalls-beforeStats.drawCalls,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Submitted triangles all passes",stats.triangles-beforeStats.triangles,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Visible renderables all passes",stats.renderablesVisible-beforeStats.renderablesVisible,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Frustum rejected all passes",stats.renderablesCulled-beforeStats.renderablesCulled,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Layer rejected all passes",stats.layerRejectedDraws-beforeStats.layerRejectedDraws,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Particles submitted",stats.particlesSubmitted-beforeStats.particlesSubmitted,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Secondary passes",stats.offscreenPasses-beforeStats.offscreenPasses,ProfileCounterMode::Sum);
}

void DrawAuthoredScene(Renderer& r, const Scene& scene, ResourceManager& assets) {
    JUDAS_PROFILE_SCOPE("Editor authored submission");
    const auto settings=scene.Settings();assets.RequestEnvironment(settings.environmentAsset);
    r.SetLighting(settings.sunDirection,settings.sunEnabled?settings.sunColor*settings.sunIntensity:glm::vec3(0),settings.ambientColor);
    r.SetSceneAppearance(settings.linearRendering,settings.exposure,assets.TryGetEnvironment(settings.environmentAsset),settings.environmentIntensity,settings.environmentRotation,settings.environmentBackground,settings.backgroundColor);
    r.SetRenderMask(scene.Settings().mainCameraRenderMask);
    for (const SceneObject& o : scene.Objects()) {
        if(!o.renderVisible)continue;
        r.SetRenderLayer(o.renderLayer);
        const glm::vec3 position = o.transform.position;
        const glm::quat rotation = glm::normalize(o.transform.rotation);
        if (o.render&&o.render->visible) {
            const SceneRenderComponent& render = *o.render;
            if(render.shape!=SceneShape::Mesh)r.SetMaterialBindings(BuildRenderMaterialBindings(&assets,render));
            if (render.shape == SceneShape::Mesh) {
                // Edit mode expresses demand by requesting each frame (a
                // hit once loaded); the editor holds the references.
                assets.RequestMesh(render.meshAsset);
                if (!render.textureAsset.empty()) assets.RequestTexture(render.textureAsset);
                DrawMeshOrPlaceholder(r, &assets, render, position, rotation, o.transform.scale, 1.0f);
            } else if (render.shape == SceneShape::Compound && o.body) {
                for (std::size_t part = 0; part < o.body->compoundBoxes.size(); ++part) {
                    const CompoundBox& box = o.body->compoundBoxes[part];
                    DrawCompoundChild(r,&assets,box,position,rotation,part==0?render.color:render.secondaryColor,render.alpha);
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
            r.SetMaterialBindings({});
            const SceneFluidVolumeComponent& f = *o.fluidVolume;
            const glm::vec3 half(0.5f * f.spacing * static_cast<float>(f.countX),
                                 0.5f * f.spacing * static_cast<float>(f.countY),
                                 0.5f * f.spacing * static_cast<float>(f.countZ));
            r.DrawBox(position + rotation * glm::vec3(0.0f, half.y - 0.5f * f.spacing, 0.0f), rotation, half,
                      glm::vec3(0.12f, 0.48f, 0.82f));
        }
    }
    r.SetRenderLayer(0);
    r.SetMaterialBindings({});
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
