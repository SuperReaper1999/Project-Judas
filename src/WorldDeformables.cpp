#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "PoseComposition.h"
#include "SaveArchive.h"
#include "SceneFingerprint.h"
#include "PerformanceProfiler.h"
#include "WorldPresentation.h"
#include <atomic>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
namespace {
std::atomic<uint64_t> epochs{1};
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
    if(asset->fracture&&(definition->liquidBasin||definition->liquidContainer||definition->liquidConnection)){error="liquid-bearing fracture unsupported; remove fracture or conserved liquid component";return nullptr;}
    if(!asset->sourceAsset.empty()){
        const auto* source=m_assets->Assets()->Find(asset->sourceAsset);
        std::string digest;
        if(!source||!SceneFingerprintSha256File(source->path,digest,error)||digest!=asset->sourceFingerprint){error="stale deformable binding: rebake source "+asset->sourceAsset;
            return nullptr;
            }
    }
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
    try{record.simulation.Initialize(asset,*definition->deformable,matrix(definition->transform),epochs.fetch_add(1));}
    catch(const std::exception& failure){error=failure.what();m_deformables.erase(id);return nullptr;}
    record.targets.resize(definition->deformable->attachments.size());
    error.clear();
    return &record.simulation;
}
void RuntimeWorld::UpdateDeformables(double dt){
    if(m_deformableOwners.empty())return;
    JUDAS_PROFILE_SCOPE("World deformables");
    for(auto id:m_deformableOwners){if(!IsPublished(id))continue;
        std::string error;
        auto* simulation=RuntimeDeformable(id,error);
        if(!simulation)continue;
        auto& record=m_deformables.at(id);
        record.targets.resize(simulation->settings.attachments.size());
        for(size_t i=0;i<simulation->settings.attachments.size();++i){auto& a=simulation->settings.attachments[i];
            auto& t=record.targets[i];
            glm::dmat4 current(1);
            bool valid=true;
            if(a.kind==DeformableAttachment::Kind::World){t.body={};current=matrix(RuntimeDefinition(id)->transform);
                }
            else{auto* d=RuntimeDefinition(a.target);
                if(!d||!IsPublished(a.target))valid=false;
                else{current=matrix(PresentedTransform(a.target,d->transform,1));
                    if(a.kind==DeformableAttachment::Kind::Body){t.body=RuntimeBody(a.target);
                        valid=m_physics.IsBodyEnabled(t.body);
                        }else{t.body={};
                        auto* animation=RuntimeAnimation(a.target);
                        if(!animation||!animation->asset||!d->animation->enabled)valid=false;
                        else{int joint=FindSkeletonJoint(animation->asset->skeleton,a.joint);
                            if(joint<0)valid=false;
                            else if(animation->recentWorld.size()!=animation->asset->skeleton.parents.size())valid=false;
                            else{current=glm::dmat4(animation->recentWorld[joint]);
                                }}}}
            }
            if(valid){t.previous=t.valid?t.current:current;
                t.current=current;
                }t.valid=valid;
            if(!valid&&!simulation->released[i])simulation->released[i]=true;
             // lost target releases, never aliases a new body
        }
        if(simulation->asset->fracture&&simulation->asset->fracture->rigid){
            if(!record.rigid.initialized&&!m_restoreConstruction){std::string prepareError;if(!PrepareRigidFracture(id,prepareError)){simulation->error=prepareError;continue;}}
            if(record.rigid.initialized)UpdateRigidFracture(id,dt);
        }else simulation->Step(dt,m_physics,m_gravityMap,record.targets);
        if(simulation->asset->fracture&&!simulation->asset->fracture->rigid&&simulation->settings.enabled)simulation->fracture.Commit(*simulation->asset->fracture);
        ++record.revision;
    }
    for(auto a=m_deformables.begin();a!=m_deformables.end();++a)for(auto b=std::next(a);b!=m_deformables.end();++b){auto& x=a->second.simulation;
        auto& y=b->second.simulation;
        if((x.asset->fracture&&x.asset->fracture->rigid)||(y.asset->fracture&&y.asset->fracture->rigid)||!x.settings.enabled||!y.settings.enabled||!x.error.empty()||!y.error.empty()||!IsPublished(a->first)||!IsPublished(b->first))continue;
        double thickness=x.settings.material.thickness+y.settings.material.thickness;
        if(glm::any(glm::lessThan(x.Maximum()+glm::dvec3(thickness),y.Minimum()))||glm::any(glm::lessThan(y.Maximum()+glm::dvec3(thickness),x.Minimum())))continue;
        DeformableInstance::SurfaceContacts(x,y,dt);
        DeformableInstance::SurfaceContacts(y,x,dt);
        }
}
void RuntimeWorld::DrawDeformables(Renderer& renderer,float alpha)const{
    for(auto& [id,record]:m_deformables){const auto* d=RuntimeDefinition(id);
        if(!d||!d->deformable||!IsPublished(id)||!RenderVisible(id)||!record.simulation.settings.enabled)continue;
        auto& sim=record.simulation;
        if(record.mappedRevision!=record.revision||record.mappedAlpha!=alpha){
            sim.MapRender(alpha,record.presentation);
            if(!record.mesh.IsValid())record.mesh=renderer.CreateMesh(record.presentation);
            else if(sim.asset->fracture&&record.meshTopology!=sim.fracture.revision){renderer.DestroyMesh(record.mesh);record.mesh=renderer.CreateMesh(record.presentation);}
            else renderer.UpdateMeshVertices(record.mesh,record.presentation.vertices);
            record.meshTopology=sim.fracture.revision;
            record.mappedRevision=record.revision;record.mappedAlpha=alpha;
        }
        renderer.SetRenderLayer(RenderLayerOf(id));
        renderer.SetMaterialBindings(d->render?BuildRenderMaterialBindings(m_assets,*d->render):std::vector<MaterialBinding>{});
        auto texture=d->render&&m_assets?m_assets->TryGetTexture(d->render->textureAsset):TextureHandle{};
        renderer.DrawMesh(record.mesh,glm::vec3(0),glm::quat(1,0,0,0),glm::vec3(1),texture,d->render?d->render->color:glm::vec3(.5f,.6f,.8f),d->render?d->render->alpha:1.f,nullptr,d->render?&d->render->hiddenParts:nullptr);
    }
}
void RuntimeWorld::RemoveDeformable(EntityId id){auto it=m_deformables.find(id);
    if(it==m_deformables.end())return;
    auto children=it->second.rigid.parts;
    for(auto joint:it->second.rigid.bonds)m_physics.DestroyJoint(joint);
    for(auto joint:it->second.rigid.supports)m_physics.DestroyJoint(joint);
    if(m_assets&&m_assets->GetRenderer()&&it->second.mesh.IsValid())m_assets->GetRenderer()->DestroyMesh(it->second.mesh);
    m_deformables.erase(it);
    for(auto child:children)if(child&&RuntimeDefinition(child))DestroyEntity(child);
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
        s->Persist(a);
        if(s->asset->fracture&&s->asset->fracture->rigid)PersistRigidFracture(id,a);
        bytes=std::move(a.bytes);
        return true;
        }catch(const std::exception& e){error=e.what();
        return false;
        }}
bool RuntimeWorld::RestoreDeformable(EntityId id,const std::string& bytes,std::string& error,bool staged){auto* s=RuntimeDeformable(id,error,staged);
    if(!s)return false;
    try{SaveArchive a(bytes);
        s->Persist(a);
        if(s->asset->fracture&&s->asset->fracture->rigid)PersistRigidFracture(id,a);
        a.Finish();
        ++m_deformables.at(id).revision;
        for(auto& t:m_deformables.at(id).targets)t.valid=false;
        return true;
        }catch(const std::exception& e){error=e.what();
        return false;
        }}

void RuntimeWorld::SetDeformableEnabled(EntityId id,bool enabled){auto it=m_deformables.find(id);if(it==m_deformables.end())return;auto& s=it->second.simulation;if(s.settings.enabled==enabled)return;s.settings.enabled=enabled;s.Wake();for(auto child:it->second.rigid.parts)if(RuntimeDefinition(child))SetColliderEnabled(child,enabled);}
