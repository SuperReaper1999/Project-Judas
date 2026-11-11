#include "ShadowSystem.h"
#include "../Renderer.h"
#include "../RuntimeWorld.h"
#include "../ResourceManager.h"
#include "../SceneSerialization.h"
#include "../ShadowTransforms.h"
#include "../PerformanceProfiler.h"
#include "../render/ShadowPassCache.h"
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
    Append(out,r.instanceOverrides);Append(out,std::uint64_t(r.materials.size()));for(const auto& m:r.materials)Append(out,m);
    Append(out,std::uint64_t(r.hiddenParts.size()));for(const auto& part:r.hiddenParts)Append(out,part);
    for(const auto* bindings:{&r.partMaterials,&r.runtimeMaterials}){Append(out,std::uint64_t(bindings->size()));for(const auto& [key,slot]:*bindings){Append(out,key);Append(out,slot);}}
}
ShadowCaster Caster(SceneObjectId id,const SceneRenderComponent& render,const SceneTransform& pose,
                    ResourceManager* resources,const std::vector<glm::mat4>* skin=nullptr) {
    ShadowCaster result;result.content.reserve(320);
    Append(result.content,id);Append(result.content,pose.position);Append(result.content,pose.rotation);Append(result.content,pose.scale);Append(result.content,render);
    if(skin){result.content.append(reinterpret_cast<const char*>(skin->data()),skin->size()*sizeof(glm::mat4));return result;}
    VisualBounds local;
    if(render.shape==SceneShape::Box){local.min=-render.halfExtents;local.max=render.halfExtents;}
    else if(render.shape==SceneShape::Sphere){local.min=glm::vec3(-render.radius);local.max=glm::vec3(render.radius);}
    else if(render.shape==SceneShape::Mesh&&resources)local=resources->VisualMeshBounds(render.meshAsset);
    result.bounds=TransformBounds(local,glm::translate(glm::mat4(1),pose.position)*glm::mat4_cast(pose.rotation)*glm::scale(glm::mat4(1),pose.scale));
    return result;
}
}
ShadowScene RuntimeShadowScene(const RuntimeWorld& world,float alpha) {
    JUDAS_PROFILE_SCOPE("Lighting caster snapshot");ShadowScene result;
    result.volatileContent=world.HasPlanets()||world.HasRuntimeLandscapes()||world.NetworkIfLoaded();
    for(const auto& s:world.StaticRenderables()){
        const auto* def=world.RuntimeDefinition(s.id);
        if(!def||!world.IsPublished(s.id)||!world.RenderVisible(s.id)||!s.render.visible)continue;
        if(def->deformable||world.PhysicalBalanceSnapshot(s.id))result.volatileContent=true;
        if(s.render.textureCamera)result.volatileContent=true;
        if(s.render.shape==SceneShape::Sprite||s.render.shape==SceneShape::Planar)continue;
        result.casters.push_back(Caster(s.id,s.render,world.PresentedTransform(s.id,{s.position,s.rotation,s.scale},alpha),world.Resources(),world.AnimationSkin(s.id,alpha)));
    }
    const auto& bodies=world.DynamicBodies();const auto& visuals=world.DynamicVisuals();
    for(size_t i=0;i<bodies.size();++i){const auto& v=visuals[i];
        if(!v.hasRender||!v.render.visible||!world.IsPublished(v.id)||!world.RenderVisible(v.id))continue;
        SceneTransform pose{bodies[i].GetPresentedPosition(alpha),bodies[i].GetPresentedOrientation(alpha),v.scale};
        auto caster=Caster(v.id,v.render,pose,world.Resources(),world.AnimationSkin(v.id,alpha));
        if(v.render.shape==SceneShape::Compound||v.render.shape==SceneShape::ConvexHull||v.render.shape==SceneShape::TriangleMesh)result.volatileContent=true;
        if(v.render.textureCamera)result.volatileContent=true;
        result.casters.push_back(std::move(caster));
    }
    // Specialized physical presentations may have no ordinary render component.
    for(const auto& e:world.Entities())if(world.IsPublished(e.id)&&(e.definition.deformable||e.definition.ragdoll))result.volatileContent=true;
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
                    const ShadowScene& scene,const std::function<void()>& draw){
    JUDAS_PROFILE_SCOPE("Lighting shadow planning");
    const auto view=renderer.ViewMatrix(),projection=renderer.ProjectionMatrix();
    auto selected=SelectShadowLights(lights,view,projection,renderer.ShadowBudget());
    // Importance decides which lights qualify, not which unchanged depth
    // allocation they own. Retain source slots when relevance order changes.
    std::array<bool,8> used{};
    for(auto& light:selected)if(light.shadowMapIndex>=0){
        const int old=renderer.ShadowCache().LocalSlotForSource(light.identity);
        if(old>=kLocalShadowBase&&!used[unsigned(old-kLocalShadowBase)/6]){light.shadowMapIndex=old;used[unsigned(old-kLocalShadowBase)/6]=true;}
        else light.shadowMapIndex=-2;
    }
    for(auto& light:selected)if(light.shadowMapIndex==-2){
        for(unsigned n=0;n<used.size();++n)if(!used[n]){used[n]=true;light.shadowMapIndex=kLocalShadowBase+int(n)*6;break;}
    }
    unsigned layers=settings.sunEnabled?3:0;
    for(const auto& l:selected)if(l.shadowMapIndex>=0)layers=std::max(layers,unsigned(l.shadowMapIndex+(l.kind==LightKind::Point?6:1)));
    renderer.ReserveShadowLayers(layers);
    renderer.SetDirectionalCascadeCount(settings.sunEnabled?3:0);
    for(int i=0;i<kShadowMapCount;++i)renderer.InvalidateShadowSlot(i);
    const auto* reuse=std::getenv("JUDAS_SHADOW_REUSE");
    const bool cacheable=!scene.volatileContent&&!(reuse&&std::strcmp(reuse,"0")==0);
    auto render=[&](int slot,std::uint64_t identity,const glm::mat4& matrix){
        JudasRender::ShadowPassKey key;key.slot=slot;key.source=identity;key.lightViewProjection=matrix;
        key.viewDependentLod=renderer.HasViewDependentMeshLods();
        if(key.viewDependentLod){key.cameraView=view;key.cameraProjection=projection;key.viewport=renderer.RenderTargetDiagnostics().viewport;}
        key.resourceRevision=renderer.ShadowResourceRevision();
        const Frustum frustum(matrix);
        for(const auto& c:scene.casters)if(frustum.IsVisible(c.bounds)){Append(key.content,c.content);}
        if(cacheable&&renderer.ShadowCache().Matches(key)&&renderer.ReuseShadowChart(slot,matrix))return;
        renderer.BeginShadowPass(slot,matrix);draw();renderer.EndShadowPass();
        // Resource publication during drawing makes this observation ineligible
        // next time. Unknown dynamic owners never publish reusable snapshots.
        if(cacheable)renderer.ShadowCache().Remember(key);
    };
    if(renderer.ShadowsEnabled()){
        if(settings.sunEnabled){
            const auto eye=glm::vec3(glm::inverse(view)[3]);
            for(int cascade=0;cascade<3;++cascade){const float extent=cascade==0?16.f:cascade==1?48.f:144.f;
                render(cascade,0,ComputeDirectionalShadowMatrix(eye,settings.sunDirection,extent,extent*3));}
        }
        for(const auto& l:selected)if(l.shadowMapIndex>=0){
            if(l.kind==LightKind::Spot)render(l.shadowMapIndex,l.identity,ComputeSpotShadowMatrix(l.position,l.direction,l.outerConeDegrees,l.range));
            else for(unsigned face=0;face<6;++face)render(l.shadowMapIndex+int(face),l.identity,ComputePointShadowMatrix(l.position,l.range,face));
        }
    }
    renderer.SetDynamicLights(selected);
}
}
