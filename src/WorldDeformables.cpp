#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "PoseComposition.h"
#include "SaveArchive.h"
#include "SceneFingerprint.h"
#include "PerformanceProfiler.h"
#include "WorldPresentation.h"
#include "deformables/StructuralSkin.h"
#include <atomic>
#include <algorithm>
#include <fstream>
#include <set>
#include <glm/gtc/matrix_transform.hpp>
namespace {
std::atomic<uint64_t> epochs{1};
constexpr const char* stateHeader="Judas.DeformableState.5\n";
glm::dmat4 matrix(const SceneTransform& t){return glm::translate(glm::dmat4(1),glm::dvec3(t.position))*glm::mat4_cast(glm::dquat(t.rotation));
        }
}
DeformableInstance* RuntimeWorld::RuntimeDeformable(EntityId id,std::string& error,bool staged){
    // Only residency restoration may prepare a pending owner. Script access
    // always uses the published path and cannot obtain a private region handle.
    auto pending=m_scriptDefinitions.find(id);
    const auto* definition=staged&&m_regionPending.count(id)&&pending!=m_scriptDefinitions.end()?&pending->second:RuntimeDefinition(id);
    if(!definition||!definition->deformable){error="stale entity/no deformable";
        return nullptr;
        }
    if(auto it=m_deformables.find(id);it!=m_deformables.end()){error=it->second.simulation.error;
        return &it->second.simulation;
        }
    if(!m_assets){error="deformable resource service unavailable";
        return nullptr;
        }auto asset=m_assets->GetDeformable(definition->deformable->asset,error);
    if(!asset)return nullptr;
    // Resource preparation validates source identity once, asynchronously.
    // Instances must not re-read a detailed model on the simulation thread.
    // A bone resource may publish after the cloth. Defer construction instead
    // of mistaking asynchronous startup for a permanently destroyed target.
    for(const auto& attachment:definition->deformable->attachments)if(attachment.enabled&&attachment.kind==DeformableAttachment::Kind::Bone){
        const auto* target=RuntimeDefinition(attachment.target);
        if(target&&target->animation&&target->render){auto* animation=RuntimeAnimation(attachment.target);if(!animation||!animation->asset){error="loading";return nullptr;}}
    }
    for(auto& a:definition->deformable->attachments)if(!asset->groups.count(a.group)){error="deformable attachment group not found: "+a.group;
        return nullptr;
        }
    auto& record=m_deformables[id];
    try{
        const bool physicalParts=asset->fracture&&asset->fracture->HasMaterials();
        // Lazy resource publication must use the current physical parent frame,
        // not a stale authored world transform after that parent has moved.
        auto pose=PresentedTransform(id,definition->transform,1);
        record.simulation.Initialize(asset,*definition->deformable,matrix(pose),epochs.fetch_add(1));
        if(physicalParts)if(const auto* local=m_hierarchy.Find(id);local&&local->parent){auto body=RuntimeBody(local->parent);if(m_physics.IsBodyEnabled(body))for(size_t n=0;n<record.simulation.positions.size();++n)record.simulation.velocities[n]=glm::dvec3(m_physics.GetPointVelocity(body,glm::vec3(record.simulation.positions[n])));}
    }
    catch(const std::exception& failure){error=failure.what();m_deformables.erase(id);return nullptr;}
    record.targets.resize(definition->deformable->attachments.size());
    error.clear();
    return &record.simulation;
}
bool RuntimeWorld::SampleDeformableTargets(EntityId id,bool currentRigidPose){
    std::string error;auto* simulation=RuntimeDeformable(id,error);if(!simulation)return false;
    auto& record=m_deformables.at(id);
        record.targets.resize(simulation->settings.attachments.size());
        bool loadingTarget=false;
        for(size_t i=0;i<simulation->settings.attachments.size();++i){auto& a=simulation->settings.attachments[i];
            auto& t=record.targets[i];
            if(simulation->released[i]||!a.enabled){t.valid=false;t.body={};continue;}
            glm::dmat4 current(1);
            bool valid=true;
            if(a.kind==DeformableAttachment::Kind::World){t.body={};current=matrix(RuntimeDefinition(id)->transform);
                }
            else{auto* d=RuntimeDefinition(a.target);
                if(!d||!IsPublished(a.target))valid=false;
                else{current=matrix(PresentedTransform(a.target,d->transform,1));
                    if(a.kind==DeformableAttachment::Kind::Body){std::string targetError;
                        if(!a.part.empty()&&a.target==id){simulation->error="structural panel cannot attach to itself";valid=false;}
                        else t.body=DeformableAttachmentBody(a,targetError);
                        if(targetError=="loading"){loadingTarget=true;break;}
                        if(!targetError.empty())simulation->error=targetError;
                        valid=m_physics.IsBodyEnabled(t.body);
                        if(valid&&(currentRigidPose||!a.part.empty())){auto pose=m_physics.GetTransform(t.body);current=glm::translate(glm::dmat4(1),glm::dvec3(pose.position))*glm::mat4_cast(glm::dquat(pose.rotation));}
                        }else{t.body={};
                        auto* animation=RuntimeAnimation(a.target);
                        if(!animation||!animation->asset||!d->animation->enabled)valid=false;
                        else{int joint=FindSkeletonJoint(animation->asset->skeleton,a.joint);
                            if(joint<0)valid=false;
                            else if(animation->recentWorld.size()!=animation->asset->skeleton.parents.size())valid=false;
                            else{current=glm::dmat4(animation->recentWorld[joint]);
                                }}}}
            }
            if(valid){t.previous=currentRigidPose&&a.kind==DeformableAttachment::Kind::Body?current:(t.valid?t.current:current);
                t.current=current;
                }t.valid=valid;
            if(!valid&&!simulation->released[i]){simulation->released[i]=true;simulation->attachmentEvents[a.group]="target-lost";simulation->Wake();}
             // lost target releases, never aliases a new body
        }
        return !loadingTarget;
}
void RuntimeWorld::UpdateDeformables(double dt){
    if(m_deformableOwners.empty())return;
    JUDAS_PROFILE_SCOPE("World deformables");
    std::set<EntityId> integrated;
    for(auto id:m_deformableOwners){if(!IsPublished(id))continue;
        std::string error;
        auto* simulation=RuntimeDeformable(id,error);
        if(!simulation)continue;
        auto& record=m_deformables.at(id);
        if(!SampleDeformableTargets(id)){if(!simulation->PhysicalStepActive())continue;simulation->error="required material target unavailable during physical interval";}
        const bool wasSleeping=simulation->sleeping;
        UpdatePanelCollisionExclusions(*simulation);
        if(simulation->asset->fracture&&simulation->asset->fracture->rigid){
            if(!record.rigid.initialized&&!m_restoreConstruction){std::string prepareError;if(!PrepareRigidFracture(id,prepareError)){simulation->error=prepareError;continue;}}
            if(record.rigid.initialized)UpdateRigidFracture(id,dt);
        }else if(simulation->PhysicalStepActive()){simulation->EndPhysicalStep(dt,m_physics);integrated.insert(id);}
        else {PrepareDeformableTerrain(id);simulation->Step(dt,m_physics,GravityForEntity(id),record.targets);}
        if(simulation->asset->fracture&&!simulation->asset->fracture->rigid&&simulation->settings.enabled)simulation->fracture.Commit(*simulation->asset->fracture);
        UpdatePanelCollisionExclusions(*simulation);
        if(!wasSleeping||!simulation->sleeping)++record.revision;
    }
    for(auto a=m_deformables.begin();a!=m_deformables.end();++a)for(auto b=std::next(a);b!=m_deformables.end();++b){auto& x=a->second.simulation;
        auto& y=b->second.simulation;
        if((x.asset->fracture&&x.asset->fracture->rigid)||(y.asset->fracture&&y.asset->fracture->rigid)||!x.settings.enabled||!y.settings.enabled||!x.error.empty()||!y.error.empty()||!IsPublished(a->first)||!IsPublished(b->first))continue;
        if((x.sleeping&&y.sleeping)||(integrated.count(a->first)&&integrated.count(b->first)))continue;
        double thickness=x.settings.material.thickness+y.settings.material.thickness;
        if(glm::any(glm::lessThan(x.Maximum()+glm::dvec3(thickness),y.Minimum()))||glm::any(glm::lessThan(y.Maximum()+glm::dvec3(thickness),x.Minimum())))continue;
        DeformableInstance::SurfaceContacts(x,y,dt);
        DeformableInstance::SurfaceContacts(y,x,dt);
        ++a->second.revision;++b->second.revision;
        }
}
void RuntimeWorld::DrawDeformables(Renderer& renderer,float alpha)const{
    auto& profiler=PerformanceProfiler::Get();
    const bool measured=profiler.Active();
    for(auto& [id,record]:m_deformables){const auto* d=RuntimeDefinition(id);
        if(!d||!d->deformable||!IsPublished(id)||!RenderVisible(id)||!record.simulation.settings.enabled)continue;
        auto& sim=record.simulation;
        const float materialAlpha=sim.sleeping?1.f:alpha;
        std::vector<glm::mat4> palette;
        if(record.mappedRevision!=record.revision||record.mappedAlpha!=materialAlpha){
            const auto mapStart=measured?profiler.Now():0;
            if(sim.asset->structuralSkin){
                if(!record.mesh.IsValid())sim.asset->structuralSkin->BuildGpuMesh(sim,record.presentation);
                else if(record.meshTopology!=sim.fracture.revision&&!sim.asset->structuralSkin->UpdateGpuTopology(sim,record.presentation))sim.asset->structuralSkin->BuildGpuMesh(sim,record.presentation);
            }else sim.MapRender(materialAlpha,record.presentation);
            const auto uploadStart=measured?profiler.Now():0;
            if(measured)JUDAS_PROFILE_COUNTER("Deformable render mapping CPU ms",double(uploadStart-mapStart)*1e-6,ProfileCounterMode::Sum);
            if(!record.mesh.IsValid())record.mesh=renderer.CreateMesh(record.presentation);
            else if(sim.asset->structuralSkin){
                if(record.meshTopology!=sim.fracture.revision&&!renderer.UpdateMeshSkinTopology(record.mesh,record.presentation.indices,record.presentation.skinVertices,record.presentation.primitives))throw std::runtime_error("invalid prepared structural skin topology");
            }
            else if(sim.asset->fracture&&record.meshTopology!=sim.fracture.revision){renderer.DestroyMesh(record.mesh);record.mesh=renderer.CreateMesh(record.presentation);}
            else if(!sim.asset->structuralSkin)renderer.UpdateMeshVertices(record.mesh,record.presentation.vertices);
            if(measured)JUDAS_PROFILE_COUNTER("Deformable geometry preparation/upload CPU ms",double(profiler.Now()-uploadStart)*1e-6,ProfileCounterMode::Sum);
            record.meshTopology=sim.fracture.revision;
            record.mappedRevision=record.revision;record.mappedAlpha=materialAlpha;
            // Immutable assets retain source images for resource rebuilding;
            // per-instance dynamic geometry does not need another image copy.
            for(auto& material:record.presentation.materials)for(auto& map:material.maps){map.embedded=TextureData{};std::vector<uint8_t>().swap(map.encodedImage);}
        }
        const auto drawStart=measured?profiler.Now():0;
        if(sim.asset->structuralSkin)sim.asset->structuralSkin->Palette(sim,materialAlpha,palette);
        renderer.SetRenderLayer(RenderLayerOf(id));
        renderer.SetMaterialBindings(d->render?BuildRenderMaterialBindings(m_assets,*d->render,&record.presentation.primitives):std::vector<MaterialBinding>{});
        auto texture=d->render&&m_assets?m_assets->TryGetTexture(d->render->textureAsset):TextureHandle{};
        renderer.DrawMesh(record.mesh,glm::vec3(0),glm::quat(1,0,0,0),glm::vec3(1),texture,d->render?d->render->color:glm::vec3(.5f,.6f,.8f),d->render?d->render->alpha:1.f,sim.asset->structuralSkin?&palette:nullptr,d->render?&d->render->hiddenParts:nullptr);
        if(measured)JUDAS_PROFILE_COUNTER("Deformable draw preparation CPU ms",double(profiler.Now()-drawStart)*1e-6,ProfileCounterMode::Sum);
    }
}
void RuntimeWorld::RemoveDeformable(EntityId id){auto it=m_deformables.find(id);
    if(it==m_deformables.end())return;
    auto children=it->second.rigid.parts;
    for(auto joint:it->second.rigid.bonds)m_physics.DestroyJoint(joint);
    for(auto joint:it->second.rigid.supports)m_physics.DestroyJoint(joint);
    if(m_assets&&m_assets->GetRenderer()&&it->second.mesh.IsValid())m_assets->GetRenderer()->DestroyMesh(it->second.mesh);
    m_deformables.erase(it);
    for(auto child:children)if(child&&FindEntity(child)){if(m_regionPending.count(child))RemoveRegionObject(child);else DestroyEntity(child);}
    }
void RuntimeWorld::ClearDeformables(){while(!m_deformables.empty())RemoveDeformable(m_deformables.begin()->first);
    }
void RuntimeWorld::InvalidateDeformableTargets(EntityId id){auto it=m_deformables.find(id);if(it!=m_deformables.end())it->second.targets.clear();}
bool RuntimeWorld::ResetDeformable(EntityId id,std::string& error){auto* s=RuntimeDeformable(id,error);
    if(!s)return false;
    if(s->asset->fracture){error="fracture is irreversible; reload the authored scene";return false;}
    s->Reset(matrix(RuntimeDefinition(id)->transform));
    ++m_deformables.at(id).revision;
    for(auto& t:m_deformables.at(id).targets)t.valid=false;
    return true;
    }
bool RuntimeWorld::CaptureDeformable(EntityId id,std::string& bytes,std::string& error){auto* s=RuntimeDeformable(id,error);
    if(!s)return false;
    try{SaveArchive a;
        const bool materials=s->asset->fracture&&s->asset->fracture->HasMaterials();
        s->Persist(a);
        if(materials)PersistFractureBundle(id,a);else if(s->asset->fracture&&s->asset->fracture->rigid)PersistRigidFracture(id,a);
        bytes=stateHeader+std::move(a.bytes);
        return true;
        }catch(const std::exception& e){error=e.what();
        return false;
        }}
bool RuntimeWorld::RestoreDeformable(EntityId id,const std::string& bytes,std::string& error,bool staged,const std::map<EntityId,EntityId>& remap,glm::dvec3 capturedOriginDelta){auto* s=RuntimeDeformable(id,error,staged);
    if(!s)return false;
    try{
        if(bytes.rfind(stateHeader,0)!=0)throw std::runtime_error("unsupported deformable state version; current V2 requires State.5");
        const bool materials=s->asset->fracture&&s->asset->fracture->HasMaterials();
        SaveArchive a(bytes.substr(std::char_traits<char>::length(stateHeader)));
        s->Persist(a);s->ShiftOrigin(capturedOriginDelta);
        for(auto& attachment:s->settings.attachments)if(remap.count(attachment.target))attachment.target=remap.at(attachment.target);
        if(materials){PersistFractureBundle(id,a,staged,remap,capturedOriginDelta);}else if(s->asset->fracture&&s->asset->fracture->rigid)PersistRigidFracture(id,a,nullptr,capturedOriginDelta);
        a.Finish();
        ++m_deformables.at(id).revision;m_deformables.at(id).meshTopology=~uint64_t(0);
        for(auto& t:m_deformables.at(id).targets)t.valid=false;
        return true;
        }catch(const std::exception& e){error=e.what();
        return false;
        }}

void RuntimeWorld::SetDeformableEnabled(EntityId id,bool enabled){auto it=m_deformables.find(id);if(it==m_deformables.end())return;auto& s=it->second.simulation;if(s.settings.enabled==enabled)return;s.settings.enabled=enabled;s.Wake();for(auto child:it->second.rigid.parts)if(RuntimeDefinition(child))SetColliderEnabled(child,enabled);}

// Prepare native MK2 material joints without changing unrelated body cadence.
void RuntimeWorld::PrepareDeformableMechanics(){
    for(auto id:m_deformableOwners)if(IsPublished(id)){
        std::string error;auto* material=RuntimeDeformable(id,error);
        if(!material||!material->asset->fracture||!material->asset->fracture->rigid||!material->settings.enabled||!material->error.empty())continue;
        if(!m_restoreConstruction&&!m_deformables.at(id).rigid.initialized&&!PrepareRigidFracture(id,error))material->error=error;
    }
}
