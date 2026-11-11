#include "NetworkSession.h"
#include "CameraProjection.h"
#include "PerformanceProfiler.h"
#include "WorldPresentation.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <optional>

#include "CelestialGravity.h"
#include "FirePresentation.h"
#include "fluid/GameWaterRuntime.h"
#include "LightTransforms.h"
#include "RadialTerrain.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "ShadowTransforms.h"
#include "PlanetTerrain.h"
#include "lighting/ShadowSystem.h"

std::vector<MaterialBinding> BuildRenderMaterialBindings(ResourceManager* resources,const SceneRenderComponent& render,const std::vector<MeshPrimitive>* derivedParts) {
    const auto* parts=derivedParts?derivedParts:resources?resources->TryGetModelParts(render.meshAsset):nullptr;
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
    r.SetMeshLodMultiplier(render.lodMultiplier);
    if(resources&&resources->VisualStreaming()&&r.AcceptsCurrentLayer()){
        auto matrix=glm::translate(glm::mat4(1),position)*glm::mat4_cast(rotation)*glm::scale(glm::mat4(1),scale);
        if(!r.IsVisible(TransformBounds(resources->VisualMeshBounds(render.meshAsset),matrix)))return;
        resources->DemandVisual(render.meshAsset);resources->DemandVisual(render.textureAsset);
    }
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
                    const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale, float alpha, TextureHandle generated = {},const std::vector<glm::mat4>* skin = nullptr,SceneObjectId identity=0) {
    if(!render.visible)return;
    if(render.shape!=SceneShape::Mesh)r.SetMaterialBindings(BuildRenderMaterialBindings(resources,render));
    switch (render.shape) {
        case SceneShape::Planar: return; // collider-only, never a renderer
        case SceneShape::Sprite: {
            TextureHandle texture;
            auto sprite=render.sprite;auto image=render.textureAsset;std::string error;
            if(render.spritePlayback&&resources){
                if(auto sheet=resources->GetSpriteSheet(render.spritePlayback->sheet,error)){
                    if(SampleSpriteFrame(*sheet,*render.spritePlayback,sprite,error))image=sheet->texture;
                }
            }
            auto color=glm::vec4(render.color,alpha*render.alpha)*sprite.tint;
            if(!error.empty()&&error!="loading")color=glm::vec4(kFailedPlaceholderColor,color.a);
            if(resources&&!image.empty()) {
                resources->RequestTexture(image);
                texture=resources->TryGetTexture(image);
                if(!texture.IsValid())color=glm::vec4(resources->StateOf(image)==ResourceState::Failed?kFailedPlaceholderColor:kLoadingPlaceholderColor,color.a);
            }
            r.QueueSprite(sprite,position,rotation,scale,texture,color,identity);
            break;
        }
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
        case SceneShape::ConvexHull:
        case SceneShape::TriangleMesh:
            break;  // handled by their owners
    }
}
}  // namespace

namespace {
SceneTransform PresentedBody(const RuntimeWorld& world, SceneObjectId id,
                             const DynamicBody& body, float alpha,
                             glm::vec3 scale=glm::vec3(1)) {
    SceneTransform pose{body.GetPresentedPosition(alpha), body.GetPresentedOrientation(alpha), scale};
    if(auto* network=world.NetworkIfLoaded())network->Presentation(id,pose);
    return pose;
}

// These values belong to one presentation sample. Camera texture handles,
// resource touches, culling and LOD still resolve in the individual pass.
struct PresentedObject {
    SceneTransform pose;
    const std::vector<glm::mat4>* skin;
};
struct PresentedObjects {
    std::vector<std::optional<PresentedObject>> statics, dynamics;
};

template<class SamplePose>
PresentedObject SampleObject(const RuntimeWorld& world, SceneObjectId id, float alpha,
                             std::optional<PresentedObject>* slot, SamplePose samplePose) {
    if (slot && *slot) return **slot;
    PresentedObject result{samplePose(), world.AnimationSkin(id, alpha)};
    if (slot) *slot = result;
    return result;
}

}

static void DrawWorldGeometry(Renderer& r, const RuntimeWorld& world,                               float alpha, const WorldDrawOptions& options, PresentedObjects* objects) {
    JUDAS_PROFILE_SCOPE("World culling and opaque submission");
    r.BeginMeshBatch();
    const auto& statics = world.StaticRenderables();
    for (std::size_t i = 0; i < statics.size(); ++i) {
        const auto& s = statics[i];
        if(!world.IsPublished(s.id)||!world.RenderVisible(s.id)||world.RuntimeDefinition(s.id)->deformable||world.HasRuntimeTerrain(s.id))continue;
        r.SetRenderLayer(world.RenderLayerOf(s.id));
        const auto [t, skin] = SampleObject(world, s.id, alpha, objects ? &objects->statics[i] : nullptr, [&] {
            return world.PresentedTransform(s.id, {s.position, s.rotation, s.scale}, alpha);
        });
        DrawRenderable(r, world.Resources(), s.render, t.position, t.rotation, t.scale, 1.0f, world.CameraTexture(s.render.textureCamera),skin,s.id);
    }
    if(options.includeTerrain)world.DrawPlanets(r);
    world.DrawDeformables(r,alpha);
    world.DrawRuntimeTerrain(r);
    world.DrawPhysicalBalances(r);
    r.SetMaterialBindings({});

    r.SetMaterialBindings({});
    r.SetMaterialBindings({});
    r.SetRenderLayer(0);



    const std::vector<DynamicBody>& bodies = world.DynamicBodies();
    const std::vector<RuntimeWorld::DynamicVisual>& visuals = world.DynamicVisuals();
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const RuntimeWorld::DynamicVisual& v = visuals[i];
        r.SetRenderLayer(world.RenderLayerOf(v.id));
        if (!v.hasRender||!world.IsPublished(v.id)||!world.RenderVisible(v.id)) continue;
        const auto [pose, skin] = SampleObject(world, v.id, alpha, objects ? &objects->dynamics[i] : nullptr, [&] {
            return PresentedBody(world, v.id, bodies[i], alpha, v.scale);
        });
        const glm::vec3 position=pose.position;
        const glm::quat rotation=pose.rotation;
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
        DrawRenderable(r, world.Resources(), v.render, position, rotation, v.scale, 1.0f, world.CameraTexture(v.render.textureCamera),skin,v.id);
    }

    r.SetMaterialBindings({});
    r.SetRenderLayer(0);
    // The fluid receives ordinary lighting/shadows in the colour pass; its
    // constantly rebuilt surface does not occupy the depth maps.

    r.EndMeshBatch();
}

void DrawWorldGeometry(Renderer& renderer, const RuntimeWorld& world,                        float alpha, const WorldDrawOptions& options) {
    DrawWorldGeometry(renderer, world, alpha, options, nullptr);
}

void DrawWorldTransparents(Renderer& r, const RuntimeWorld& world, float alpha) {
    JUDAS_PROFILE_SCOPE("Particles and water presentation");
    r.FlushMaterialBlends();r.FlushSprites();r.SetMaterialBindings({});
    const std::vector<DynamicBody>& bodies = world.DynamicBodies();
    const std::vector<RuntimeWorld::DynamicVisual>& visuals = world.DynamicVisuals();
    if(world.BasicWaterEnabled()){
        const auto waters=world.BasicWaters(alpha);
        if(!waters.empty()){
            r.BeginTransparentPass();
            for(const auto& water:waters)if(world.RenderVisible(water.entity)){r.SetRenderLayer(world.RenderLayerOf(water.entity));r.DrawBasicWater(water.settings,water.position,water.rotation,world.SimulationTimeSeconds());}
            r.SetRenderLayer(0);
            r.EndTransparentPass();
        }
    }

    if(const auto* grid=world.GridWater()){
        r.BeginTransparentPass();grid->Draw(r);r.EndTransparentPass();
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
            const auto pose=PresentedBody(world,v.id,bodies[i],alpha,v.scale);
            const glm::vec3 position=pose.position;
            const glm::quat rotation=pose.rotation;
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
            const auto pose=PresentedBody(world,c.id,body,alpha);
            const glm::vec3 position=pose.position;
            const AtmosphereSample gas = atmosphere.field.Sample(position, atmosphere.frame);
            FirePresentationInput input;
            input.position = position;
            input.orientation = pose.rotation;
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
        const auto texture=world.Resources()?world.Resources()->TryGetTexture(emitter.pool.settings.textureAsset):TextureHandle{};
        if(emitter.gpu){
            // Resident state is view-independent. Each eye only prepares its
            // own GPU order/indirect command; no simulation or state download.
            VisualBounds bounds;
            r.DrawGpuParticles(*emitter.gpu,bounds,texture);
            continue;
        }
        const auto& particles=emitter.pool.Presentation(t.position,t.rotation,t.scale);
        r.DrawParticles(particles,emitter.pool.Bounds(),texture);
    }

    r.SetRenderLayer(0);

}





void RenderWorldFrame(Renderer& renderer, int width, int height, const RuntimeWorld& world,
                      const glm::mat4& view, const glm::mat4& projection,
                      const glm::vec3& shadowFocus, float alpha,float frameSeconds,WorldRenderPurpose purpose,const RenderTargetHandle* destination) {
    const bool observer=purpose==WorldRenderPurpose::Observer || purpose==WorldRenderPurpose::ObserverFrame;
    const bool advancePresentation=purpose!=WorldRenderPurpose::Observer;
    const bool mainView=purpose!=WorldRenderPurpose::Auxiliary;
    const bool gameInterest=purpose==WorldRenderPurpose::Runtime || purpose==WorldRenderPurpose::Observer;
    JUDAS_PROFILE_SCOPE("World render submission");
    renderer.ClearWeather();
    // Existing runtime statistics may accumulate across frames. Observe this
    // submission's delta without resetting the engine's diagnostic state.
    auto planetEye=world.view?world.view->pose.position:glm::vec3(glm::inverse(view)[3]);
    const double planetFov=world.view?glm::radians(double(world.view->fov)):2*std::atan(1./double(projection[1][1]));
    if(world.HasPlanets()||world.HasRuntimeLandscapes()){
    std::vector<PlanetView> planetViews;
    const auto origin=world.Settings().worldOrigin;
    if(observer){
        const auto eye=glm::vec3(glm::inverse(view)[3]);
        const bool orthographic=std::abs(projection[3][3])>.5f;
        planetViews.push_back({origin+glm::dvec3(eye),2*std::atan(1./double(projection[1][1])),unsigned(std::max(1,height)),orthographic?2./std::abs(double(projection[1][1])):0});
    }

    if(auto* players=world.PlayersIfLoaded();gameInterest&&players&&players->HasViews()){
        for(const auto& [_,p]:players->All())if(p.view){auto size=LocalPlayers::ViewSize(p.viewport,width,height);planetViews.push_back({origin+glm::dvec3(p.view->position),glm::radians(double(p.fov)),unsigned(std::max(1,size.y)),p.lens.projection==CameraProjectionKind::Orthographic?CameraVisibleHeight(p.lens):0});}
    }else if(gameInterest)planetViews.push_back({origin+glm::dvec3(planetEye),planetFov,unsigned(std::max(1,height)),world.view&&world.view->lens.projection==CameraProjectionKind::Orthographic?CameraVisibleHeight(world.view->lens):0});
    for(const auto& camera:world.PresentationCameras())if(camera.settings.enabled&&world.IsPublished(camera.id)){
        auto pose=world.PresentedTransform(camera.id,camera.transform,alpha);double fov=glm::radians(double(camera.settings.verticalFovDegrees));
        if(camera.hasFrustum)fov=2*std::atan(double(camera.frustum.w-camera.frustum.z)/(2*camera.settings.nearPlane));
        planetViews.push_back({origin+glm::dvec3(pose.position),std::clamp(fov,.011,3.12),unsigned(camera.settings.height),camera.settings.lens.projection==CameraProjectionKind::Orthographic?CameraVisibleHeight(camera.settings.lens):0});
    }
    world.PreparePlanets(renderer,planetViews,advancePresentation?frameSeconds:0);
    world.PrepareRuntimeTerrain(planetViews,advancePresentation?frameSeconds:0);
    }
    if(auto* assets=world.Resources();assets&&assets->VisualStreaming()){
        struct Interest {Frustum frustum;CategoryMask mask;};std::vector<Interest> interests;
        auto addView=[&](glm::vec3 position,glm::quat rotation,float fov,int vw,int vh,float nearPlane,float farPlane,CategoryMask mask,const CameraLens& lens){auto frame=BuildCameraFrame(position,rotation,fov,nearPlane,farPlane,lens,vw,vh);interests.push_back({Frustum(frame.projection*frame.view),mask});};
        if(auto* players=world.PlayersIfLoaded();gameInterest&&players&&players->HasViews()){for(const auto& [_,p]:players->All())if(p.view){auto size=LocalPlayers::ViewSize(p.viewport,width,height);addView(p.view->position,p.view->rotation,p.fov,size.x,size.y,p.nearPlane,p.farPlane,p.renderMask,p.lens);}}
        else if(gameInterest&&world.view){auto& v=*world.view;addView(v.pose.position,v.pose.rotation,v.fov,width,height,v.nearPlane,v.farPlane,world.Settings().mainCameraRenderMask,v.lens);}
        else if(gameInterest)interests.push_back({Frustum(projection*view),world.Settings().mainCameraRenderMask});
        if(observer)interests.push_back({Frustum(projection*view),world.Settings().mainCameraRenderMask});
        for(const auto& c:world.PresentationCameras())if(c.settings.enabled&&world.IsPublished(c.id)){auto pose=world.PresentedTransform(c.id,c.transform,alpha);auto v=glm::lookAt(pose.position,pose.position+pose.rotation*glm::vec3(0,0,-1),pose.rotation*glm::vec3(0,1,0));auto plane=c.hasClipPlane?ViewSpacePlane(v,c.clipNormal,c.clipPoint):glm::vec4(0);auto frame=BuildCameraFrame(pose.position,pose.rotation,c.settings.verticalFovDegrees,c.settings.nearPlane,c.settings.farPlane,c.settings.lens,c.settings.width,c.settings.height);if(c.hasFrustum||c.hasClipPlane)frame.projection=SecondaryCameraProjection(c.settings.verticalFovDegrees,float(frame.content.z)/frame.content.w,c.settings.nearPlane,c.settings.farPlane,c.hasFrustum?&c.frustum:nullptr,c.hasClipPlane?&plane:nullptr);interests.push_back({Frustum(frame.projection*frame.view),c.settings.renderMask});}
        // Use the same presentation owners as DrawWorldGeometry. Entities()
        // contains managed rigid bodies, not ordinary static renderables. Demand
        // all enabled views even on frames where a cached camera is not redrawn.
        auto demand = [&](SceneObjectId id, const SceneRenderComponent& render,
                          const SceneTransform& pose) {
            if (!world.IsPublished(id) || !world.RenderVisible(id) ||
                !render.visible || render.shape != SceneShape::Mesh) return;
            auto matrix = glm::translate(glm::mat4(1), pose.position) *
                          glm::mat4_cast(pose.rotation) * glm::scale(glm::mat4(1), pose.scale);
            auto bounds = TransformBounds(assets->VisualMeshBounds(render.meshAsset), matrix);
            for (const auto& interest : interests)
                if ((interest.mask & CategoryBit(world.RenderLayerOf(id))) &&
                    interest.frustum.IsVisible(bounds)) {
                    assets->DemandVisual(render.meshAsset);
                    assets->DemandVisual(render.textureAsset);
                    break;
                }
        };
        for (const auto& s : world.StaticRenderables())
            demand(s.id, s.render, world.PresentedTransform(s.id,
                   SceneTransform{s.position, s.rotation, s.scale}, alpha));
        const auto& bodies = world.DynamicBodies();
        const auto& visuals = world.DynamicVisuals();
        for (size_t i = 0; i < bodies.size(); ++i)
            if (visuals[i].hasRender)
                demand(visuals[i].id, visuals[i].render,
                       PresentedBody(world,visuals[i].id,bodies[i],alpha,visuals[i].scale));
    }
    const auto beforeStats = renderer.Stats();
    // One sample owns one immutable lighting/environment decision. Auxiliary
    // cameras never run scripts or advance an animation/fade between passes.
    const SceneSettings settings = world.Settings();
    if(world.Resources())world.Resources()->RequestEnvironment(settings.environmentAsset);
    renderer.SetSceneAppearance(settings.linearRendering,settings.exposure,world.Resources()?world.Resources()->TryGetEnvironment(settings.environmentAsset):EnvironmentHandle{},settings.environmentIntensity,settings.environmentRotation,settings.environmentBackground,settings.backgroundColor);
    renderer.SetLighting(settings.sunDirection, settings.sunEnabled?settings.sunColor*settings.sunIntensity:glm::vec3(0), settings.ambientColor);
    const std::vector<DynamicLight> lights = BuildWorldLights(world, alpha);

    // Each camera receives its own relevance decision and actual shadow chart.
    // Targets/viewport are restored by Renderer; no pass borrows another eye's
    // directional coverage or stale spotlight assignment.
    PresentedObjects presentedObjects;
    presentedObjects.statics.resize(world.StaticRenderables().size());
    presentedObjects.dynamics.resize(world.DynamicBodies().size());
    const auto shadowScene=JudasLighting::RuntimeShadowScene(world,alpha);
    auto lightPass = [&]() {
        JudasLighting::PrepareShadows(renderer,settings,lights,shadowScene,[&]{
            DrawWorldGeometry(renderer,world,alpha,WorldDrawOptions{},&presentedObjects);
        });
    };
    (void)shadowFocus;
    // Secondary views are presentation passes only; shadow maps above are shared.
    if (advancePresentation && !world.PresentationCameras().empty()) {
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
            if (!renderer.BeginRenderTarget(camera.target)) continue;
            camera.targetSize=renderer.RenderTargetSize(camera.target);
            JUDAS_PROFILE_SCOPE("Secondary camera");
            RendererProfileScope cameraGPU(renderer,"Secondary camera",camera.id);
            const glm::mat4 cameraView=glm::lookAt(position, position + rotation * glm::vec3(0,0,-1), rotation * glm::vec3(0,1,0));
            const glm::vec4 viewPlane=camera.hasClipPlane?ViewSpacePlane(cameraView,camera.clipNormal,camera.clipPoint):glm::vec4(0);
            auto frame=BuildCameraFrame(position,rotation,c.verticalFovDegrees,c.nearPlane,c.farPlane,c.lens,camera.targetSize.x,camera.targetSize.y);
            if(camera.hasFrustum||camera.hasClipPlane)frame.projection=SecondaryCameraProjection(c.verticalFovDegrees,float(frame.content.z)/frame.content.w,c.nearPlane,c.farPlane,camera.hasFrustum?&camera.frustum:nullptr,camera.hasClipPlane?&viewPlane:nullptr);
            renderer.SetCameraFrame(frame);
            world.PrepareWeather(renderer,world.Settings().worldOrigin+glm::dvec3(position),camera.id);
            renderer.SetRenderMask(c.renderMask);
            lightPass();
            renderer.SetWaterPaths(32,18,world.WaterOpticalPaths(renderer.ViewMatrix(),renderer.ProjectionMatrix(),32,18,alpha,false));
            DrawWorldGeometry(renderer, world, alpha, WorldDrawOptions{}, &presentedObjects);
            DrawWorldTransparents(renderer, world, alpha);
            renderer.DrawWeather(true);
            renderer.EndRenderTarget(); ++camera.updates;
        }
    }
    if(mainView) {
    if(auto* players=world.PlayersIfLoaded();!observer&&players&&players->HasViews()){
        players->BindRenderer(renderer);
        // Each view borrows the same immutable presented world and shadow maps.
        // No extra simulation, input dispatch or audio update occurs per camera.
        for(auto& entry:players->All()){auto& p=entry.second;if(!p.view)continue;
            const auto size=LocalPlayers::ViewSize(p.viewport,width,height);const int vw=size.x,vh=size.y;
            if(p.attemptedSize!=size||p.attemptedLinear!=settings.linearRendering||(!p.target.IsValid()&&p.renderError.empty())){p.attemptedSize=size;p.attemptedLinear=settings.linearRendering;renderer.ResizeRenderTarget(p.target,vw,vh,p.renderError,true);}
            if(!p.renderError.empty()||!renderer.BeginRenderTarget(p.target))continue;
            JUDAS_PROFILE_SCOPE("Local player view");RendererProfileScope playerGPU(renderer,"Local player view",p.id);
            const auto& pose=*p.view;const auto q=pose.rotation;
            renderer.SetCameraFrame(BuildCameraFrame(pose.position,q,p.fov,p.nearPlane,p.farPlane,p.lens,vw,vh));
            world.PrepareWeather(renderer,world.Settings().worldOrigin+glm::dvec3(pose.position),p.id|(uint64_t(1)<<63));
            renderer.SetRenderMask(p.renderMask);lightPass();
            renderer.SetWaterPaths(32,18,world.WaterOpticalPaths(renderer.ViewMatrix(),renderer.ProjectionMatrix(),32,18,alpha,false));
            DrawWorldGeometry(renderer,world,alpha,WorldDrawOptions{}, &presentedObjects);DrawWorldTransparents(renderer,world,alpha);renderer.DrawWeather(true);renderer.EndRenderTarget();
        }
        if(destination&&!renderer.BeginRenderTarget(*destination))return;
        renderer.ClearWeather();renderer.BeginFrame(width,height);renderer.EndFrame();renderer.BeginUIFrame(width,height);
        for(auto& entry:players->All()){auto& p=entry.second;if(p.view&&p.renderError.empty())renderer.DrawUIImage({p.viewport.x*width,p.viewport.y*height},{p.viewport.z*width,p.viewport.w*height},renderer.RenderTargetTexture(p.target),{1,1,1,1},false,true);}
        renderer.EndUIFrame();
    }else{
    if(destination&&!renderer.BeginRenderTarget(*destination))return;
    renderer.BeginFrame(width, height);
    JUDAS_PROFILE_SCOPE("Main camera");
    RendererProfileScope mainGPU(renderer,"Main camera");
    if(!observer&&world.view){const auto& v=*world.view;auto q=v.pose.rotation;
        renderer.SetCameraFrame(BuildCameraFrame(v.pose.position,q,v.fov,v.nearPlane,v.farPlane,v.lens,width,height));
    }else renderer.SetCamera(view, projection);
    world.PrepareWeather(renderer,world.Settings().worldOrigin+glm::dvec3(glm::inverse(renderer.ViewMatrix())[3]),observer?UINT64_MAX-1:0);
    renderer.SetRenderMask(settings.mainCameraRenderMask);
    lightPass();
    renderer.SetWaterPaths(32,18,world.WaterOpticalPaths(renderer.ViewMatrix(),renderer.ProjectionMatrix(),32,18,alpha,false));
    DrawWorldGeometry(renderer, world, alpha, WorldDrawOptions{}, &presentedObjects);
    DrawWorldTransparents(renderer, world, alpha);
    renderer.DrawWeather();
    renderer.EndFrame();
    }
    if(destination)renderer.EndRenderTarget(/*restoreCamera=*/false);
    }
    const auto& stats=renderer.Stats();
    JUDAS_PROFILE_COUNTER("Instanced draw submissions",stats.instancedDraws-beforeStats.instancedDraws,ProfileCounterMode::Sum);
    // Renderer statistics accumulate in the runtime, while the editor may
    // reset them. Publish deltas for this complete world submission in either
    // case, after transparent/material work has finished. Do not reset shared
    // statistics or label lifetime transfer totals as per-frame work.
    JUDAS_PROFILE_COUNTER("Presentation upload bytes (view/light/instances/billboards)",double(stats.uploadedBytes-beforeStats.uploadedBytes),ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Presentation upload calls",double(stats.uploadCalls-beforeStats.uploadCalls),ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Material applications",double(stats.materialApplications-beforeStats.materialApplications),ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Material uniform publishes",double(stats.materialUniformPublishes-beforeStats.materialUniformPublishes),ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Material uniform reuses",double(stats.materialUniformReuses-beforeStats.materialUniformReuses),ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Instances submitted",stats.instances-beforeStats.instances,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Light tile overflow",stats.lightTileOverflow-beforeStats.lightTileOverflow,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Light grid GPU bytes",stats.lightBytes,ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Shadow GPU bytes",stats.shadowBytes,ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Submitted draws all passes",stats.drawCalls-beforeStats.drawCalls,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Submitted triangles all passes",stats.triangles-beforeStats.triangles,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Visible renderables all passes",stats.renderablesVisible-beforeStats.renderablesVisible,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Frustum rejected all passes",stats.renderablesCulled-beforeStats.renderablesCulled,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Layer rejected all passes",stats.layerRejectedDraws-beforeStats.layerRejectedDraws,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Particles submitted",stats.particlesSubmitted-beforeStats.particlesSubmitted,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Secondary passes",stats.offscreenPasses-beforeStats.offscreenPasses,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Reused shadow charts",stats.reusedShadowPasses-beforeStats.reusedShadowPasses,ProfileCounterMode::Sum);
}

namespace {
void SetAuthoredSceneAppearance(Renderer& r, const Scene& scene, ResourceManager& assets) {
    const auto& settings = scene.Settings();
    assets.RequestEnvironment(settings.environmentAsset);
    r.SetLighting(settings.sunDirection,
        settings.sunEnabled ? settings.sunColor * settings.sunIntensity : glm::vec3(0),
        settings.ambientColor);
    r.SetSceneAppearance(settings.linearRendering, settings.exposure,
        assets.TryGetEnvironment(settings.environmentAsset), settings.environmentIntensity,
        settings.environmentRotation, settings.environmentBackground, settings.backgroundColor);
    r.SetRenderMask(settings.mainCameraRenderMask);
}
}

void DrawAuthoredWaterSurfaces(Renderer& r, const Scene& scene, ResourceManager& assets,bool basicWater) {
    if (r.IsShadowPass()) return;
    const bool hasWater = std::any_of(scene.Objects().begin(), scene.Objects().end(),
        [basicWater](const SceneObject& o) { return o.renderVisible &&
            ((basicWater&&o.basicWater&&o.basicWater->enabled)||
             (o.fluidVolume&&o.fluidVolume->fillHeight>0)); });
    if (!hasWater) return;
    // All opaque geometry, including neighbouring regions, must be submitted
    // before this pass. Transparent water deliberately does not write depth.
    r.EndMeshBatch();
    r.FlushMaterialBlends();
    SetAuthoredSceneAppearance(r, scene, assets);
    r.SetMaterialBindings({});
    r.BeginTransparentPass();
    for (const auto& o : scene.Objects()) {
        if (!o.renderVisible) continue;
        r.SetRenderLayer(o.renderLayer);
        if(basicWater&&o.basicWater)r.DrawBasicWater(*o.basicWater, o.transform.position,
                        glm::normalize(o.transform.rotation), 0);
        if(o.fluidVolume&&o.fluidVolume->fillHeight>0){
            const auto& f=*o.fluidVolume;BasicWaterSettings style;style.color=f.color;style.opacity=f.opacity;
            style.halfExtents={f.HalfExtents().x*o.transform.scale.x,f.fillHeight*o.transform.scale.y,f.HalfExtents().z*o.transform.scale.z};
            style.waveAmplitude=0;
            r.DrawBasicWater(style,o.transform.position+o.transform.rotation*glm::vec3(0,f.fillHeight*o.transform.scale.y,0),o.transform.rotation,0);
        }
    }
    r.SetRenderLayer(0);
    r.EndTransparentPass();
}

void DrawAuthoredScene(Renderer& r,const Scene& scene,ResourceManager& assets){
    DrawAuthoredScene(r,scene,assets,true);
}
void DrawAuthoredScene(Renderer& r, const Scene& scene, ResourceManager& assets, bool basicWater) {
    JUDAS_PROFILE_SCOPE("Editor authored submission");
    r.BeginMeshBatch();
    SetAuthoredSceneAppearance(r, scene, assets);
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
            } else {
                DrawRenderable(r, &assets, render, position, rotation, o.transform.scale, 1.0f,{},nullptr,o.id);
            }
        }
        // Authored liquid is shown as its particle lattice bounds.

    }
    r.SetRenderLayer(0);
    r.SetMaterialBindings({});
    r.EndMeshBatch();
    r.FlushSprites();
    if(basicWater)DrawAuthoredWaterSurfaces(r, scene, assets);
}



// Authored edit preview is deliberately clock-zero. Playing creates independent
// deterministic field state; drawing the inspector never advances simulation.
WeatherSample SampleAuthoredWeather(const Scene& scene,glm::dvec3 eye){
    std::vector<const SceneObject*> owners;for(const auto& o:scene.Objects())if(o.weather&&o.weather->enabled)owners.push_back(&o);
    std::sort(owners.begin(),owners.end(),[](const auto* a,const auto* b){return a->weather->priority==b->weather->priority?a->id<b->id:a->weather->priority<b->weather->priority;});
    WeatherSample result;result.profile.cloudCoverage=0;result.profile.cloudDensity=0;result.profile.wind=glm::dvec3(0);result.profile.gust=0;
    for(const auto* o:owners){std::string error;auto settings=RuntimeWorld::ResolveWeatherSettings(*o,scene.Settings().worldOrigin);if(!ValidateWeatherSettings(settings,error))continue;auto next=WeatherRuntime(settings).Sample(eye);if(!next.active)continue;const double w=next.weight;auto previous=result;result=next;result.profile=BlendWeatherProfiles(previous.profile,next.profile,w);result.cloudOffset=glm::mix(previous.cloudOffset,next.cloudOffset,w);result.wind=glm::mix(previous.wind,next.wind/w,w);result.airDensity=glm::mix(previous.airDensity,next.airDensity/w,w);result.weight=previous.weight+(1-previous.weight)*w;}
    return result;
}
