#include "RuntimeWorld.h"
#include "PerformanceProfiler.h"
#include "lighting/ShadowSystem.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace {
JudasHair::Frame FrameOf(const SceneTransform& pose) {
    return {pose.position,pose.rotation,pose.scale};
}
bool SameFrame(const JudasHair::Frame& a,const JudasHair::Frame& b) {
    return a.position==b.position&&a.rotation==b.rotation&&a.scale==b.scale;
}
}
void RuntimeWorld::UpdateHair(float dt) {
    if(m_hair.empty())return;
    JUDAS_PROFILE_SCOPE("Hair guides");
    for(auto& [id,instance]:m_hair) {
        const auto* definition=RuntimeDefinition(id);
        if(!definition||!definition->hairGroom||!IsPublished(id)) {
            instance.simulation.strands.clear();
            instance.dirty=true;
            continue;
        }
        const auto& groom=*definition->hairGroom;
        if(!groom.enabled||!groom.simulated)continue;
        const auto pose=FrameOf(SimulationTransform(id,definition->transform));
        const auto attachment=definition->socket&&definition->socket->enabled?definition->socket->target:id;
        const auto gravity=SampleEntityGravity(attachment,pose.position);
        const auto wind=glm::vec3(SampleWeather(m_settings.worldOrigin+glm::dvec3(pose.position)).wind);
        JudasHair::SweepTips sweep;
        if(groom.worldCollision) {
            PhysicsQueryFilter filter;
            const auto own=RuntimeBody(id),attached=RuntimeBody(attachment);
            if(own.IsValid())filter.ignoredBodies.push_back(own);
            if(attached.IsValid()&&(attached.id!=own.id||attached.world!=own.world))filter.ignoredBodies.push_back(attached);
            sweep=[this,filter,radius=std::max(.003f,groom.width*.6f)](glm::vec3 from,glm::vec3& to) {
                const auto delta=to-from;
                const auto distance=glm::length(delta);
                if(distance<1e-5f)return;
                const auto hit=m_physics.SphereCast(from,radius,delta/distance,distance,filter);
                if(hit.hit&&!hit.initialOverlap)to=from+delta*std::max(0.f,hit.fraction-.001f);
            };
        }
        JudasHair::Step(instance.simulation,groom,pose,gravity,wind,dt,sweep);
        instance.dirty=true;
    }
}
void RuntimeWorld::PrepareHair(Renderer& renderer,float alpha,glm::vec3 eye) const {
    if(m_hair.empty())return;
    JUDAS_PROFILE_SCOPE("Hair mesh preparation");
    m_hairRenderer=&renderer;
    for(auto& [id,instance]:m_hair) {
        const auto* definition=RuntimeDefinition(id);
        unsigned density=0;
        JudasHair::Frame pose;
        const auto* groom=definition&&definition->hairGroom?&*definition->hairGroom:nullptr;
        if(groom&&groom->enabled&&IsPublished(id)&&RenderVisible(id)) {
            pose=FrameOf(PresentedTransform(id,definition->transform,alpha));
            float distance=glm::length(eye-pose.position);
            for(const auto& camera:m_renderCameras)
                if(camera.settings.enabled&&IsPublished(camera.id))
                    distance=std::min(distance,glm::length(PresentedTransform(camera.id,camera.transform,alpha).position-pose.position));
            if(m_players&&m_players->HasViews())
                for(const auto& [_,player]:m_players->All())
                    if(player.view)distance=std::min(distance,glm::length(player.view->position-pose.position));
            // The existing decoration control scales presentation only. The
            // authored guide population and its fixed-step work are independent.
            density=JudasHair::VisualStrands(*groom,distance,renderer.Quality().decorativeDensity);
        }
        if(!density) {
            if(instance.mesh.IsValid())renderer.DestroyMesh(instance.mesh);
            instance.mesh={};
            instance.density=0;
            continue;
        }
        if(instance.mesh.IsValid() && !instance.dirty && instance.density==density) {
            if(!groom->simulated&&instance.rendered.scale==pose.scale) {
                instance.rendered=pose;
                continue;
            }
            if(SameFrame(instance.rendered,pose)&&instance.alpha==alpha)continue;
        }
        JudasHair::Frame meshFrame=pose;
        if(!groom->simulated) {
            meshFrame.position=glm::vec3(0);
            meshFrame.rotation=glm::quat(1,0,0,0);
        }
        if(instance.simulation.strands.empty())
            JudasHair::Seed(instance.simulation,*groom,meshFrame);
        JudasHair::BuildVertices(instance.simulation,*groom,meshFrame,alpha,density,m_hairVertices);
        if(!instance.mesh.IsValid() || instance.density!=density) {
            MeshData data;
            data.vertices.swap(m_hairVertices);
            JudasHair::BuildIndices(*groom,density,data.indices);
            const auto mesh=renderer.CreateMesh(data);
            renderer.DestroyMesh(instance.mesh);
            instance.mesh=mesh;
            data.vertices.swap(m_hairVertices);
        } else if(!renderer.UpdateMeshVertices(instance.mesh,m_hairVertices,false)) {
            renderer.DestroyMesh(instance.mesh);
            instance.mesh={};
        }
        if(!instance.mesh.IsValid())continue;
        if(!m_hairMaterial.IsValid()) {
            MaterialDefinition material;
            material.roughness=.64f;
            material.doubleSided=true;
            auto& map=material.maps[0];
            map.embedded.width=map.embedded.height=16;
            map.sampler.wrapS=map.sampler.wrapT=GL_CLAMP_TO_EDGE;
            for(unsigned y=0;y<16;++y)
                for(unsigned x=0;x<16;++x) {
                    const auto shade=uint8_t(158+4*y+2*x);
                    map.embedded.pixels.insert(map.embedded.pixels.end(),{shade,shade,shade,255});
                }
            m_hairMaterial=renderer.CreateMaterial(material);
        }
        instance.bounds={};
        for(const auto& vertex:m_hairVertices)instance.bounds.Include(vertex.position);
        ++instance.geometryRevision;
        instance.rendered=pose;
        instance.alpha=alpha;
        instance.density=density;
        instance.dirty=false;
        JUDAS_PROFILE_COUNTER("Hair upload bytes",double(m_hairVertices.size()*sizeof(MeshVertex)),ProfileCounterMode::Sum);
    }
    if(PerformanceProfiler::Get().Active()) {
        size_t resident=0;
        for(const auto& [_,instance]:m_hair)resident+=renderer.MeshResidentBytes(instance.mesh);
        JUDAS_PROFILE_COUNTER("Hair resident mesh bytes",double(resident),ProfileCounterMode::Latest);
    }
}
void RuntimeWorld::DrawHair(Renderer& renderer) const {
    if(!m_hairMaterial.IsValid())return;
    RendererProfileScope gpu(renderer,renderer.IsShadowPass()?nullptr:"Hair and fur");
    MaterialBinding binding;
    binding.handle=m_hairMaterial;
    binding.hairShading=true;
    const std::vector<MaterialBinding> bindings{binding};
    renderer.SetMaterialBindings(bindings);
    for(const auto& [id,instance]:m_hair) {
        if(!instance.mesh.IsValid())continue;
        const auto* definition=RuntimeDefinition(id);
        if(!definition||!definition->hairGroom||!RenderVisible(id))continue;
        const auto& groom=*definition->hairGroom;
        if(!groom.enabled||(renderer.IsShadowPass()&&!groom.castsShadow))continue;
        renderer.SetRenderLayer(RenderLayerOf(id));
        const auto position=groom.simulated?glm::vec3(0):instance.rendered.position;
        const auto rotation=groom.simulated?glm::quat(1,0,0,0):instance.rendered.rotation;
        renderer.DrawMesh(instance.mesh,position,rotation,{1,1,1},{},groom.color,1);
    }
    renderer.SetRenderLayer(0);
    renderer.SetMaterialBindings({});
}
void RuntimeWorld::RemoveHair(EntityId id) {
    const auto it=m_hair.find(id);
    if(it==m_hair.end())return;
    if(m_hairRenderer)m_hairRenderer->DestroyMesh(it->second.mesh);
    m_hair.erase(it);
}
void RuntimeWorld::ReleaseHairGraphics() {
    if(!m_hairRenderer)return;
    for(auto& [_,instance]:m_hair) {
        m_hairRenderer->DestroyMesh(instance.mesh);
        instance.mesh={};
    }
    m_hairRenderer->DestroyMaterial(m_hairMaterial);
    m_hairMaterial={};
    m_hairRenderer=nullptr;
}
void RuntimeWorld::AppendHairShadowCasters(std::vector<JudasLighting::ShadowCaster>& casters) const {
    for(const auto& [id,instance]:m_hair) {
        if(!instance.mesh.IsValid())continue;
        const auto* d=RuntimeDefinition(id);
        if(!d||!d->hairGroom||!d->hairGroom->castsShadow)continue;
        JudasLighting::ShadowCaster caster;
        const bool local=!d->hairGroom->simulated;
        const auto& frame=instance.rendered;
        caster.bounds=local?TransformBounds(instance.bounds,glm::translate(glm::mat4(1),frame.position)*
                                                        glm::mat4_cast(frame.rotation)):instance.bounds;
        caster.content="hair";
        // This live mesh/generation owns the geometry identity, including
        // degenerate LOD transitions. Updating vertices affects only charts
        // whose frusta actually contain this caster.
        const auto append=[&](const auto& value) {
            caster.content.append(reinterpret_cast<const char*>(&value),sizeof(value));
        };
        append(id);
        append(instance.mesh.id);
        append(instance.mesh.generation);
        append(instance.geometryRevision);
        if(local) {
            append(frame.position.x);append(frame.position.y);append(frame.position.z);
            append(frame.rotation.w);append(frame.rotation.x);append(frame.rotation.y);append(frame.rotation.z);
        }
        casters.push_back(std::move(caster));
    }
}
const JudasHair::Guides* RuntimeWorld::HairGuides(EntityId id) const {
    const auto it=m_hair.find(id);
    return it==m_hair.end()?nullptr:&it->second.simulation;
}
void RuntimeWorld::ShiftHair(glm::dvec3 delta) {
    for(auto& [_,instance]:m_hair) {
        JudasHair::Shift(instance.simulation,delta);
        instance.dirty=true;
    }
}
