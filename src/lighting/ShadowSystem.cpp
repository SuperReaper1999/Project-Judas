#include "ShadowSystem.h"
#include "../Renderer.h"
#include "../RuntimeWorld.h"
#include "../ResourceManager.h"
#include "../SceneSerialization.h"
#include "../ShadowTransforms.h"
#include "../PerformanceProfiler.h"
#include "../render/ShadowPassCache.h"
#include "../render/PresentedObjects.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdlib>
#include <cstring>

namespace JudasLighting {
namespace {
// Ephemeral exact-key encoding: fieldwise values, never padding bytes,
// pointers, locale text or a probabilistic hash. No persistence format.
template<class T> void Append(std::string& out,const T& value){out.append(reinterpret_cast<const char*>(&value),sizeof(value));}
void Append(std::string& out,const std::string& value){Append(out,std::uint64_t(value.size()));out+=value;}
void Append(std::string& out,const glm::vec2& v){Append(out,v.x);Append(out,v.y);}
void Append(std::string& out,const glm::vec3& v){Append(out,v.x);Append(out,v.y);Append(out,v.z);}
void Append(std::string& out,const glm::vec4& v){Append(out,v.x);Append(out,v.y);Append(out,v.z);Append(out,v.w);}
void Append(std::string& out,const glm::quat& q){Append(out,q.w);Append(out,q.x);Append(out,q.y);Append(out,q.z);}
template<class T> void Append(std::string& out,const std::optional<T>& value){Append(out,bool(value));if(value)Append(out,*value);}
void Append(std::string& out,const MaterialOverride& m){
    Append(out,m.baseColor);Append(out,m.metallic);Append(out,m.roughness);Append(out,m.emissiveIntensity);
    Append(out,m.emissive);Append(out,m.uvScale);Append(out,m.uvOffset);Append(out,m.alpha);Append(out,m.alphaCutoff);
    Append(out,m.normalStrength);Append(out,m.occlusionStrength);Append(out,m.doubleSided);
    for(const auto& texture:m.textures)Append(out,texture);
}
void Append(std::string& out,const MaterialSlot& m){Append(out,m.asset);Append(out,m.useSource);Append(out,m.overrides);}
void Append(std::string& out,const SceneRenderComponent& r){
    Append(out,r.shape);Append(out,r.visible);Append(out,r.halfExtents);Append(out,r.radius);Append(out,r.color);Append(out,r.alpha);
    Append(out,r.secondaryColor);Append(out,r.secondaryAlpha);Append(out,r.lodMultiplier);Append(out,r.meshAsset);Append(out,r.textureAsset);Append(out,r.textureCamera);
    Append(out,r.decorative);
    Append(out,r.instanceOverrides);Append(out,std::uint64_t(r.materials.size()));for(const auto& m:r.materials)Append(out,m);
    Append(out,std::uint64_t(r.hiddenParts.size()));for(const auto& part:r.hiddenParts)Append(out,part);
    for(const auto* bindings:{&r.partMaterials,&r.runtimeMaterials}){Append(out,std::uint64_t(bindings->size()));for(const auto& [key,slot]:*bindings){Append(out,key);Append(out,slot);}}
}
std::string EncodeObject(SceneObjectId id,const SceneRenderComponent& render,
                         glm::vec3 position,glm::quat rotation,glm::vec3 scale) {
    std::string key;
    key.reserve(320);
    key.push_back('o');
    Append(key,id);Append(key,position);Append(key,rotation);Append(key,scale);Append(key,render);
    return key;
}
ShadowCaster Caster(SceneObjectId id,const SceneRenderComponent& render,const SceneTransform& pose,
                    ResourceManager* resources,const std::vector<glm::mat4>* skin=nullptr,
                    bool cullSkin=false,bool defer=false) {
    ShadowCaster result;
    if(defer) {
        result.deferredRender=&render;
        result.owner=id;
        result.position=pose.position;
        result.rotation=pose.rotation;
        result.scale=pose.scale;
    }else result.content=EncodeObject(id,render,pose.position,pose.rotation,pose.scale);
    VisualBounds local;
    if(skin&&!skin->empty()){
        result.content.append(reinterpret_cast<const char*>(skin->data()),skin->size()*sizeof(glm::mat4));
        if(cullSkin&&render.shape==SceneShape::Mesh&&resources&&resources->GetRenderer())
            local=resources->GetRenderer()->ShadowSkinnedMeshBounds(resources->TryGetMesh(render.meshAsset),*skin);
    }else if(render.shape==SceneShape::Box){local.min=-render.halfExtents;local.max=render.halfExtents;}
    else if(render.shape==SceneShape::Sphere){local.min=glm::vec3(-render.radius);local.max=glm::vec3(render.radius);}
    else if(render.shape==SceneShape::Mesh&&resources)local=resources->VisualMeshBounds(render.meshAsset);
    result.bounds=TransformBounds(local,glm::translate(glm::mat4(1),pose.position)*glm::mat4_cast(pose.rotation)*glm::scale(glm::mat4(1),pose.scale));
    return result;
}
}
std::string ShadowRenderKey(const SceneRenderComponent& render) {
    std::string result;
    Append(result,render);
    return result;
}
ShadowScene RuntimeShadowScene(const RuntimeWorld& world,float alpha,JudasRender::PresentedObjects* samples) {
    JUDAS_PROFILE_SCOPE("Lighting caster snapshot");ShadowScene result;
    result.volatileContent=world.HasPlanets()||world.NetworkIfLoaded()||world.HasActiveRagdolls();
    result.compactCasterKeys=world.HasRuntimeLandscapes();
    // These draw owners already require every chart to be redrawn.
    if(result.volatileContent)return result;
    const auto& statics=world.StaticRenderables();
    result.casters.reserve(statics.size());
    for(std::size_t i=0;i<statics.size();++i){
        const auto& s=statics[i];
        const auto* def=world.RuntimeDefinition(s.id);
        if(!def||!world.IsPublished(s.id)||!s.render.visible||!def->renderVisible||
           (def->render&&!def->render->visible))continue;
        if(def->deformable||s.render.textureCamera)result.volatileContent=true;
        if(s.render.shape==SceneShape::Sprite||s.render.shape==SceneShape::Planar)continue;
        const auto object=JudasRender::SampleObject(world,s.id,alpha,
            samples?&samples->statics[i]:nullptr,true,[&]{
                return world.PresentedTransform(s.id,{s.position,s.rotation,s.scale},alpha);
            },def);
        if(!object.visible)continue;
        const bool moving=result.compactCasterKeys&&object.skin&&!object.skin->empty();
        auto& casters=moving?result.movingCasters:result.casters;
        casters.push_back(Caster(s.id,s.render,object.pose,world.Resources(),object.skin,
                                 result.compactCasterKeys,result.compactCasterKeys&&!moving));
    }
    const auto& bodies=world.DynamicBodies();const auto& visuals=world.DynamicVisuals();
    for(size_t i=0;i<bodies.size();++i){
        const auto& v=visuals[i];
        if(!v.hasRender||!v.render.visible||!world.IsPublished(v.id))continue;
        const auto object=JudasRender::SampleObject(world,v.id,alpha,
            samples?&samples->dynamics[i]:nullptr,false,[&]{
                return SceneTransform{bodies[i].GetPresentedPosition(alpha),
                                      bodies[i].GetPresentedOrientation(alpha),v.scale};
            });
        if(!object.visible)continue;
        auto caster=Caster(v.id,v.render,object.pose,world.Resources(),object.skin,result.compactCasterKeys);
        if(v.render.shape==SceneShape::Compound||v.render.shape==SceneShape::ConvexHull||v.render.shape==SceneShape::TriangleMesh)result.volatileContent=true;
        if(v.render.textureCamera)result.volatileContent=true;
        (result.compactCasterKeys?result.movingCasters:result.casters).push_back(std::move(caster));
    }
    // Inactive rigs have no physics presentation. Their ordinary skinned
    // draws are already keyed by the presented pose and complete GPU palette.
    // Keep active physics rigs and deformables on the conservative redraw path.
    for(const auto& e:world.Entities())
        if(world.IsPublished(e.id)&&e.definition.deformable)result.volatileContent=true;
    world.AppendTerrainShadowCasters(result.casters);
    world.AppendHairShadowCasters(result.compactCasterKeys?result.movingCasters:result.casters);
    return result;
}
ShadowScene AuthoredShadowScene(const Scene& scene,ResourceManager& resources){
    ShadowScene result;
    for(const auto& o:scene.Objects()){
        if(o.landscape||o.planet||o.deformable)result.volatileContent=true;
        if(!o.render||!o.render->visible||!o.renderVisible)continue;
        if(o.render->textureCamera||o.render->shape==SceneShape::Compound)result.volatileContent=true;
        result.casters.push_back(Caster(o.id,*o.render,o.transform,&resources));
    }return result;
}
void PrepareShadows(Renderer& renderer,const SceneSettings& settings,const std::vector<DynamicLight>& lights,
                    const ShadowScene& scene,const std::function<void()>& draw) {
    PrepareRuntimeShadows(renderer, settings, lights, false, [&]() -> const ShadowScene& {return scene;},draw);
}
void PrepareRuntimeShadows(Renderer& renderer,const SceneSettings& settings,const std::vector<DynamicLight>& lights,
                          bool terrain,const std::function<const ShadowScene&()>& snapshot,
                          const std::function<void()>& draw){
    JUDAS_PROFILE_SCOPE("Lighting shadow planning");
    const auto view=renderer.ViewMatrix(),projection=renderer.ProjectionMatrix();
    auto selected=[&]{
        JUDAS_PROFILE_SCOPE("Shadow light selection");
        return SelectShadowLights(lights,view,projection,renderer.ShadowBudget());
    }();
    // Per-view importance selects lights; a bounded context-owned LRU retains
    // inactive charts for other cameras instead of thrashing the first slots.
    renderer.ShadowCache().AssignLocalSlots(selected);
    const auto requested=std::count_if(lights.begin(),lights.end(),[](const auto& l){return l.castsShadow||l.shadowMapIndex>=1;});
    const auto chosen=std::count_if(selected.begin(),selected.end(),[](const auto& l){return l.shadowMapIndex>=0;});
    JUDAS_PROFILE_COUNTER("Shadow sources requested",requested,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Shadow sources selected",chosen,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Shadow requests not selected",requested-chosen,ProfileCounterMode::Sum);
    const auto& quality=renderer.Quality();
    const unsigned cascades=settings.sunEnabled?quality.directionalCascades:0;
    unsigned layers=cascades;
    for(const auto& l:selected)if(l.shadowMapIndex>=0)layers=std::max(layers,unsigned(l.shadowMapIndex+(l.kind==LightKind::Point?6:1)));
    for(int i=0;i<kShadowMapCount;++i)renderer.InvalidateShadowSlot(i);
    const bool allocated=renderer.ReserveShadowLayers(layers);
    renderer.SetDirectionalCascadeCount(allocated?cascades:0);
    if(!allocated){
        JUDAS_PROFILE_COUNTER("Shadow allocation rejected",1,ProfileCounterMode::Sum);
        for(auto& light:selected)light.shadowMapIndex=-1;
        renderer.SetDynamicLights(selected);
        return;
    }
    std::array<glm::mat4, kDirectionalShadowCascades> directional{};
    const auto eye=glm::vec3(glm::inverse(view)[3]);
    for(unsigned cascade=0;cascade<cascades;++cascade){
        constexpr float extents[]{16,48,144};
        const float extent=extents[cascade]*quality.shadowDistanceScale;
        directional[cascade]=ComputeStableDirectionalShadowMatrix(
            eye,settings.sunDirection,extent,extent*3,quality.shadowResolution);
    }
    auto &cache=renderer.ShadowCache();
    const auto* reuse=std::getenv("JUDAS_SHADOW_REUSE");
    const bool unchangedProjection=!terrain||cache.ObserveDirectionalCharts(directional,cascades);
    const bool observeContent=renderer.ShadowsEnabled()&&(chosen||cascades)&&
        (chosen||unchangedProjection)&&!(reuse&&std::strcmp(reuse,"0")==0);
    // When every projection must redraw, avoid serializing a large terrain
    // scene. After motion stops, observe a new exact key before enabling reuse.
    const auto* scene=observeContent?&snapshot():nullptr;
    const bool cacheable=scene&&!scene->volatileContent;
    std::vector<std::uint64_t> tokens,movingTokens;
    if(cacheable&&scene->compactCasterKeys){
        cache.BeginCasterSnapshot();
        tokens.resize(scene->casters.size());
        movingTokens.resize(scene->movingCasters.size());
    }
    auto render=[&](int slot,std::uint64_t identity,const glm::mat4& matrix){
        JudasRender::ShadowPassKey key;
        bool trackContent=cacheable;
        if(trackContent){
            JUDAS_PROFILE_SCOPE("Shadow cache keys");
            key.slot=slot;key.source=identity;key.lightViewProjection=matrix;
            key.compactContent=scene->compactCasterKeys;
            Append(key.content,settings.linearRendering);
            key.viewDependentLod=renderer.HasViewDependentMeshLods();
            if(key.viewDependentLod){key.cameraView=view;key.cameraProjection=projection;key.viewport=renderer.RenderTargetDiagnostics().viewport;}
            key.resourceRevision=renderer.ShadowResourceRevision();
            const Frustum frustum(matrix);
            const auto appendCasters=[&](const std::vector<ShadowCaster>& casters,
                                         std::vector<std::uint64_t>& ids){
                for(std::size_t i=0;i<casters.size();++i){
                    const auto& caster=casters[i];
                    if(renderer.CullingEnabled()&&!frustum.IsVisible(caster.bounds))continue;
                    if(scene->compactCasterKeys){
                        auto& token=ids[i];
                        if(!token){
                            token=caster.deferredRender
                                ?cache.CasterToken(EncodeObject(caster.owner,*caster.deferredRender,
                                    caster.position,caster.rotation,caster.scale))
                                :cache.CasterToken(caster.content);
                        }
                        if(!token)return false;
                        Append(key.content,token);
                    }else Append(key.content,caster.content);
                    if(key.content.size()>JudasRender::ShadowPassCache::MaxContentBytes)return false;
                }
                return true;
            };
            using Match=JudasRender::ShadowPassCache::Match;
            trackContent=appendCasters(scene->movingCasters,movingTokens);
            const bool movingCastersChanged=trackContent&&terrain&&unsigned(slot)<cascades&&
                !cache.ObserveMovingCasters(unsigned(slot),key.content);
            if(movingCastersChanged){
                JUDAS_PROFILE_COUNTER("Shadow redraw moving caster",1,ProfileCounterMode::Sum);
                trackContent=false;
            }
            if(trackContent)trackContent=appendCasters(scene->casters,tokens);
            const auto match=trackContent?cache.Inspect(key):(movingCastersChanged?Match::Invalid:Match::TooLarge);
            switch(match){
            case Match::Hit:
                if(renderer.ReuseShadowChart(slot,matrix))return;
                JUDAS_PROFILE_COUNTER("Shadow miss depth allocation",1,ProfileCounterMode::Sum);break;
            case Match::Chart: JUDAS_PROFILE_COUNTER("Shadow miss light-space matrix",1,ProfileCounterMode::Sum);break;
            case Match::View: JUDAS_PROFILE_COUNTER("Shadow miss camera LOD view",1,ProfileCounterMode::Sum);break;
            case Match::Resources: JUDAS_PROFILE_COUNTER("Shadow miss resource revision",1,ProfileCounterMode::Sum);break;
            case Match::Casters: JUDAS_PROFILE_COUNTER("Shadow miss caster content",1,ProfileCounterMode::Sum);break;
            case Match::Invalid: break;
            case Match::TooLarge: JUDAS_PROFILE_COUNTER("Shadow miss key size",1,ProfileCounterMode::Sum);break;
            default: JUDAS_PROFILE_COUNTER("Shadow miss no matching slot",1,ProfileCounterMode::Sum);break;
            }
        }
        if(scene&&scene->volatileContent)JUDAS_PROFILE_COUNTER("Shadow redraw conservative owner",1,ProfileCounterMode::Sum);
        if(!unchangedProjection&&!chosen)JUDAS_PROFILE_COUNTER("Shadow redraw changing directional projection",1,ProfileCounterMode::Sum);
        {
        JUDAS_PROFILE_SCOPE("Shadow draw submission");
        renderer.BeginShadowPass(slot,matrix);draw();renderer.EndShadowPass();
        }
        // Resource publication during drawing makes this observation ineligible
        // next time. Unknown dynamic owners never publish reusable snapshots.
        if(trackContent)cache.Remember(key);
    };
    if(renderer.ShadowsEnabled()){
        for(unsigned cascade=0;cascade<cascades;++cascade)
            render(cascade,0,directional[cascade]);
        for(const auto& l:selected)if(l.shadowMapIndex>=0){
            if(l.kind==LightKind::Spot)render(l.shadowMapIndex,l.identity,ComputeSpotShadowMatrix(l.position,l.direction,l.outerConeDegrees,l.range));
            else for(unsigned face=0;face<6;++face)render(l.shadowMapIndex+int(face),l.identity,ComputePointShadowMatrix(l.position,l.range,face));
        }
    }
    renderer.SetDynamicLights(selected);
}
}
