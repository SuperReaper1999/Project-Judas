#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "SceneSerialization.h"
#include <sstream>
#include "ScriptSystem.h"
#include "PerformanceProfiler.h"
#include <algorithm>
#include <set>
namespace {
using V=glm::dvec3;using M=glm::dmat3;
V center(const DeformableAsset& a,const FracturePart& p){V sum(0);double mass=0;for(auto n:p.nodes){sum+=a.nodes[n]*a.measures[n];mass+=a.measures[n];}return sum/mass;}
M inertia(const DeformableInstance& s,const FracturePart& p,V com){M tensor(0);for(auto n:p.nodes){V r=s.asset->nodes[n]-com;double mass=s.NodeMass(n);tensor+=mass*(M(glm::dot(r,r))-glm::outerProduct(r,r));}return tensor;}
}
EntityId RuntimeWorld::FracturePartEntity(EntityId owner,unsigned part,bool includeDisabled)const{auto it=m_deformables.find(owner);if(it==m_deformables.end()||part>=it->second.rigid.parts.size()||it->second.simulation.fracture.removed[part])return 0;auto id=it->second.rigid.parts[part];return RuntimeDefinition(id)&&(includeDisabled||m_physics.IsBodyEnabled(RuntimeBody(id)))?id:0;}
bool RuntimeWorld::PrepareRigidFracture(EntityId owner,std::string& error){
    JUDAS_PROFILE_SCOPE("Fracture rigid preparation/publication");
    auto it=m_deformables.find(owner);if(it==m_deformables.end())return false;auto& r=it->second;auto& sim=r.simulation;if(!sim.asset->fracture||!sim.asset->fracture->rigid)return true;if(r.rigid.initialized)return true;
    const auto& c=*sim.asset->fracture;const auto definition=*RuntimeDefinition(owner);
    if(definition.liquidBasin||definition.liquidContainer||definition.liquidConnection){error="liquid-bearing fracture is unsupported; conserved owner retained";return false;}
    for(auto& a:sim.settings.attachments)if(a.kind==DeformableAttachment::Kind::Bone){error="rigid fracture supports require world/body frames";return false;}
    // Preparation validates every child definition before allocating any body.
    std::vector<SceneObject> children;std::vector<EntityPhysicalState> motion;
    for(unsigned part=0;part<c.parts.size();++part){auto& p=c.parts[part];V com=center(*sim.asset,p);SceneObject d;d.name="Fracture part "+p.key;d.tags=definition.tags;d.renderLayer=definition.renderLayer;d.body=SceneBodyComponent{};auto& b=*d.body;
        b.enabled=sim.settings.enabled;b.motion=SceneBodyMotion::Dynamic;b.shape=SceneShape::Compound;b.compoundBoxes={{glm::vec3(p.proxyCenter-com),glm::vec3(p.proxyHalf)}};
        b.mass=0;for(auto n:p.nodes)b.mass+=float(sim.NodeMass(n));b.friction=float(sim.settings.material.friction);b.collisionLayer=sim.settings.collisionLayer;b.collisionMask=sim.settings.collisionMask;
        EntityPhysicalState state;state.rotation=definition.transform.rotation;V current(0),velocity(0);for(auto n:p.nodes){current+=sim.positions[n]*sim.NodeMass(n);velocity+=sim.velocities[n]*sim.NodeMass(n);}state.position=glm::vec3(current/double(b.mass));state.linearVelocity=glm::vec3(velocity/double(b.mass));
        // Fit angular momentum to the actual lumped nodal distribution. No
        // repeated impact, rest-shape swap, or copied whole-parent mass.
        M worldInertia=glm::mat3_cast(glm::dquat(state.rotation))*inertia(sim,p,com)*glm::transpose(glm::mat3_cast(glm::dquat(state.rotation)));V angular(0);for(auto n:p.nodes)angular+=glm::cross(sim.positions[n]-V(state.position),(sim.velocities[n]-V(state.linearVelocity))*sim.NodeMass(n));state.angularVelocity=glm::vec3(glm::inverse(worldInertia)*angular);
        if(!ValidateEntityCreation(d,error))return false;
        children.push_back(d);motion.push_back(state);
    }
    std::vector<EntityId> created;std::vector<JointHandle> joints,supports;
    auto rollback=[&](){for(auto j:joints)m_physics.DestroyJoint(j);for(auto j:supports)m_physics.DestroyJoint(j);for(auto id:created)DestroyEntity(id);};
    for(unsigned p=0;p<children.size();++p){auto id=CreateEntity(children[p],&motion[p],&error);if(!id){rollback();return false;}created.push_back(id);FindEntity(id)->requiresFull=true;auto handle=RuntimeBody(id);if(!m_physics.SetMassDistribution(handle,children[p].body->mass,glm::mat3(inertia(sim,c.parts[p],center(*sim.asset,c.parts[p]))))){error="invalid fragment inertia";rollback();return false;}}
    for(const auto& bond:c.bonds){JointSettings settings;settings.bodyA=RuntimeBody(created[bond.a]);settings.bodyB=RuntimeBody(created[bond.b]);settings.anchorA=glm::vec3(bond.centroid-center(*sim.asset,c.parts[bond.a]));settings.anchorB=glm::vec3(bond.centroid-center(*sim.asset,c.parts[bond.b]));auto j=m_physics.CreateJoint(settings);if(!j.IsValid()){error="fracture bond allocation failed";rollback();return false;}if(bond.material.enabled&&!m_physics.ConfigureJointMaterial(j,bond.material)){m_physics.DestroyJoint(j);error="invalid fracture material connection";rollback();return false;}joints.push_back(j);m_physics.SetPairCollisionEnabled(settings.bodyA,settings.bodyB,false);}
    for(unsigned a=0;a<sim.settings.attachments.size();++a){const auto& attachment=sim.settings.attachments[a];if(!attachment.enabled)continue;const auto& nodes=sim.asset->groups.at(attachment.group);
        for(unsigned p=0;p<c.parts.size();++p){V anchor(0);unsigned count=0;for(auto n:nodes)if(c.nodePart[n]==p){anchor+=sim.asset->nodes[n];++count;}if(!count)continue;anchor/=count;JointSettings s;s.bodyA=RuntimeBody(created[p]);s.anchorA=glm::vec3(anchor-center(*sim.asset,c.parts[p]));
            if(attachment.kind==DeformableAttachment::Kind::Body){s.bodyB=RuntimeBody(attachment.target);if(!s.bodyB.IsValid()){error="fracture support body not ready";rollback();return false;}s.anchorB=glm::vec3(anchor+attachment.offset);s.frameB=glm::inverse(m_physics.GetTransform(s.bodyB).rotation)*motion[p].rotation;}
            else{s.anchorB=definition.transform.position+definition.transform.rotation*glm::vec3(anchor+attachment.offset);s.frameB=definition.transform.rotation;}
            auto j=m_physics.CreateJoint(s);if(!j.IsValid()){error="fracture support allocation failed";rollback();return false;}supports.push_back(j);if(c.filterNeighbours&&s.bodyB.IsValid())m_physics.SetPairCollisionEnabled(s.bodyA,s.bodyB,false);
        }
    }
    for(auto pair:c.contactNeighbours)m_physics.SetPairCollisionEnabled(RuntimeBody(created[pair.x]),RuntimeBody(created[pair.y]),sim.fracture.component[pair.x]!=sim.fracture.component[pair.y]);
    r.rigid.parts=std::move(created);r.rigid.bonds=std::move(joints);r.rigid.supports=std::move(supports);r.rigid.initialized=true;sim.error.clear();return true;
}
void RuntimeWorld::UpdateRigidFracture(EntityId owner,double dt){
    JUDAS_PROFILE_SCOPE("Fracture rigid observations");auto& record=m_deformables.at(owner);auto& s=record.simulation;const auto& c=*s.asset->fracture;s.previous=s.positions;
    for(unsigned p=0;p<c.parts.size();++p){auto id=record.rigid.parts[p];auto body=RuntimeBody(id);if(!RuntimeDefinition(id)||!m_physics.IsBodyEnabled(body)){if(!RuntimeDefinition(id)&&!s.fracture.removed[p]){s.fracture.removed[p]=1;++s.fracture.revision;for(unsigned b=0;b<c.bonds.size();++b)if(c.bonds[b].a==p||c.bonds[b].b==p)s.fracture.pending[b]=2;}continue;}
        auto pose=m_physics.GetTransform(body);V com=center(*s.asset,c.parts[p]);V velocity=m_physics.GetLinearVelocity(body),omega=m_physics.GetAngularVelocity(body);for(auto n:c.parts[p].nodes){V arm=glm::dquat(pose.rotation)*(s.asset->nodes[n]-com);s.positions[n]=V(pose.position)+arm;s.velocities[n]=velocity+glm::cross(omega,arm);}

    }
    for(unsigned b=0;b<c.bonds.size();++b)if(!s.fracture.broken[b]){JointState joint;if(m_physics.GetJoint(record.rigid.bonds[b],joint)){auto pose=m_physics.GetTransform(joint.settings.bodyA);V normal=glm::dquat(pose.rotation)*c.bonds[b].normal;
            V moment=V(joint.reactionAngularImpulse)-glm::cross(glm::dquat(pose.rotation)*V(joint.settings.anchorA),V(joint.reactionImpulse));
            if(c.bonds[b].material.enabled){auto force=V(joint.reactionImpulse)/dt;double axial=glm::dot(force,normal)/c.bonds[b].area;s.fracture.tensionDemand[b]=std::max(s.fracture.tensionDemand[b],std::max(0.,axial));s.fracture.shearDemand[b]=std::max(s.fracture.shearDemand[b],glm::length(force-normal*glm::dot(force,normal))/c.bonds[b].area);s.fracture.plasticTranslation[b]=joint.constitutive.translation;s.fracture.plasticRotation[b]=joint.constitutive.rotation;s.fracture.plasticWork[b]=joint.constitutive.plasticWork;if(joint.constitutive.failed)s.fracture.pending[b]=1;}
            else if(c.structural){auto inverse=glm::inverse(glm::dquat(pose.rotation));s.fracture.ObserveSection(c,b,inverse*V(joint.reactionImpulse)/dt,inverse*moment/dt);}
            else {s.fracture.Observe(c,b,V(joint.reactionImpulse)/dt,normal);
                double bending=glm::length(moment-normal*glm::dot(moment,normal))/dt;double span=std::sqrt(c.bonds[b].area);s.fracture.Observe(c,b,normal*(bending*6/span),normal);
            }
        }}
    if(!s.settings.enabled)return;
    s.fracture.Commit(c);
    for(unsigned p=0;p<c.parts.size();++p)if(s.fracture.removed[p]&&RuntimeDefinition(record.rigid.parts[p]))DestroyEntity(record.rigid.parts[p]);
    for(unsigned b=0;b<c.bonds.size();++b)if(s.fracture.broken[b]){m_physics.DestroyJoint(record.rigid.bonds[b]);record.rigid.bonds[b]={};auto a=RuntimeBody(record.rigid.parts[c.bonds[b].a]),d=RuntimeBody(record.rigid.parts[c.bonds[b].b]);if(a.IsValid()&&d.IsValid())m_physics.SetPairCollisionEnabled(a,d,true);}
    // Release an authored support only when its normal attachment is released.
    // Support ordering is stable attachment order, then part identity.
    unsigned j=0;for(unsigned a=0;a<s.settings.attachments.size();++a){const auto& attachment=s.settings.attachments[a];if(!attachment.enabled)continue;auto& nodes=s.asset->groups.at(attachment.group);for(unsigned p=0;p<c.parts.size();++p){bool present=std::any_of(nodes.begin(),nodes.end(),[&](unsigned n){return c.nodePart[n]==p;});if(!present)continue;if(j<record.rigid.supports.size()&&(s.released[a]||s.fracture.removed[p])){JointState released;if(c.filterNeighbours&&m_physics.GetJoint(record.rigid.supports[j],released)&&released.settings.bodyB.IsValid())m_physics.SetPairCollisionEnabled(released.settings.bodyA,released.settings.bodyB,true);m_physics.DestroyJoint(record.rigid.supports[j]);record.rigid.supports[j]={};}++j;}}
    if(c.filterNeighbours)for(auto handle:record.rigid.supports){JointState support;if(m_physics.GetJoint(handle,support)&&support.settings.bodyB.IsValid())m_physics.SetPairCollisionEnabled(support.settings.bodyA,support.settings.bodyB,false);}
    s.fracture.Connectivity(c);
    for(auto pair:c.contactNeighbours){auto a=RuntimeBody(record.rigid.parts[pair.x]),b=RuntimeBody(record.rigid.parts[pair.y]);if(a.IsValid()&&b.IsValid())m_physics.SetPairCollisionEnabled(a,b,s.fracture.component[pair.x]!=s.fracture.component[pair.y]);}
}
void RuntimeWorld::PersistRigidFracture(EntityId owner,SaveArchive& a,const std::map<EntityId,EntityId>* remap){
    const auto resolveBody=[&](EntityId id){
        // Only this private restoration boundary may resolve staged actors.
        // Public RuntimeBody intentionally rejects unpublished definitions.
        if(a.reading&&remap&&m_regionPending.count(id)){if(auto* e=FindEntity(id);e&&e->slot!=std::numeric_limits<size_t>::max())return m_dynamicBodies[e->slot].Handle();auto found=m_entityCategories.find(id);return found==m_entityCategories.end()?BodyHandle{}:found->second.body;}
        return RuntimeBody(id);
    };
    auto& record=m_deformables.at(owner);auto& s=record.simulation;const auto& c=*s.asset->fracture;
    if(!a.reading){a.Require(record.rigid.initialized,"required fracture family not published");a.Require(!IsTransientEntity(owner),"required fracture owner cannot be omitted as transient");}
    a(record.rigid.parts);if(a.reading&&remap)for(auto& id:record.rigid.parts)if(remap->count(id))id=remap->at(id);a.Require(record.rigid.parts.size()==c.parts.size(),"fracture part ownership count differs");
    for(unsigned p=0;p<c.parts.size();++p){auto id=record.rigid.parts[p];a.Require(s.fracture.removed[p]||(resolveBody(id).IsValid()&&!IsTransientEntity(id)),"required fracture body missing/transient");if(a.reading&&!s.fracture.removed[p]){FindEntity(id)->requiresFull=true;a.Require(m_physics.SetMassDistribution(resolveBody(id),float([&](){double m=0;for(auto n:c.parts[p].nodes)m+=s.NodeMass(n);return m;}()),glm::mat3(inertia(s,c.parts[p],center(*s.asset,c.parts[p])))),"invalid restored fragment inertia");}}
    auto saveJoint=[&](JointHandle& handle,const JointMaterial* material=nullptr){bool exists=handle.IsValid();a(exists);EntityId first=0,second=0;JointSettings settings;
        if(exists){if(!a.reading){JointState state;a.Require(m_physics.GetJoint(handle,state),"stale owned fracture joint");settings=state.settings;first=EntityIdOfBody(settings.bodyA);second=EntityIdOfBody(settings.bodyB);}a(first,second,settings.anchorA,settings.anchorB,settings.frameA,settings.frameB);
            if(a.reading){if(remap){if(remap->count(first))first=remap->at(first);if(remap->count(second))second=remap->at(second);}settings.bodyA=resolveBody(first);settings.bodyB=resolveBody(second);a.Require(settings.bodyA.IsValid()&&(!second||settings.bodyB.IsValid()),"fracture attached body missing");handle=m_physics.CreateJoint(settings);a.Require(handle.IsValid(),"fracture constraint recreation failed");if(material&&material->enabled)a.Require(m_physics.ConfigureJointMaterial(handle,*material),"fracture material recreation failed");}m_physics.PersistJointSolverState(a,handle);if(material&&material->enabled)m_physics.PersistJointMaterial(a,handle);
        }else if(a.reading)handle={};};
    if(a.reading)record.rigid.bonds.resize(c.bonds.size());
    a.Require(record.rigid.bonds.size()==c.bonds.size(),"fracture bond ownership count differs");for(unsigned b=0;b<record.rigid.bonds.size();++b)saveJoint(record.rigid.bonds[b],&c.bonds[b].material);
    uint32_t n=uint32_t(record.rigid.supports.size());a(n);a.Require(n<=16384,"fracture support capacity");if(a.reading)record.rigid.supports.resize(n);for(auto& j:record.rigid.supports)saveJoint(j);
    if(a.reading){for(unsigned b=0;b<c.bonds.size();++b)if(!s.fracture.broken[b])m_physics.SetPairCollisionEnabled(resolveBody(record.rigid.parts[c.bonds[b].a]),resolveBody(record.rigid.parts[c.bonds[b].b]),false);for(auto pair:c.contactNeighbours)m_physics.SetPairCollisionEnabled(resolveBody(record.rigid.parts[pair.x]),resolveBody(record.rigid.parts[pair.y]),s.fracture.component[pair.x]!=s.fracture.component[pair.y]);if(c.filterNeighbours)for(auto handle:record.rigid.supports){JointState support;if(m_physics.GetJoint(handle,support)&&support.settings.bodyB.IsValid())m_physics.SetPairCollisionEnabled(support.settings.bodyA,support.settings.bodyB,false);}record.rigid.initialized=true;}
}

// Retained-region bundle owns its generated bodies. Source entity identities
// are relocation inputs only; fresh native handles are allocated on restoration.
EntityId RuntimeWorld::FracturePartOwner(EntityId part)const{
 for(const auto& [owner,r]:m_deformables)if(r.simulation.asset->fracture&&r.simulation.asset->fracture->HasMaterials()&&std::find(r.rigid.parts.begin(),r.rigid.parts.end(),part)!=r.rigid.parts.end())return owner;
 return 0;
}
void RuntimeWorld::PersistFractureBundle(EntityId owner,SaveArchive& a,bool staged,const std::map<EntityId,EntityId>& references){
 auto& r=m_deformables.at(owner);const auto& c=*r.simulation.asset->fracture;
 a.Require(c.rigid&&c.HasMaterials(),"retained physical bundle requires finite rigid material");
 a.Require(!a.reading||!r.rigid.initialized,"retained physical bundle requires a fresh owner");
 auto remap=references;std::vector<EntityId> created;
 try{
  uint32_t count=uint32_t(c.parts.size());a(count);a.Require(count==c.parts.size(),"retained physical bundle part count differs");
  for(unsigned p=0;p<count;++p){EntityId old=0;bool exists=false;std::string text;EntityPhysicalState state;
   if(!a.reading){old=r.rigid.parts.at(p);const auto* d=RuntimeDefinition(old);exists=d!=nullptr;if(exists){Scene single;single.Objects().push_back(*d);a.Require(SaveSceneToString(single,text)&&GetEntityState(old,state),"physical part capture unavailable");}}
   a(old,exists);a.Require(exists||r.simulation.fracture.removed[p],"required retained physical part missing");
   if(exists){a(text,state.position,state.rotation,state.linearVelocity,state.angularVelocity);a.Require(text.size()<=65536,"retained physical definition exceeds bound");
    if(a.reading){Scene single;std::string error;a.Require(LoadSceneFromString(text,single,error)&&single.Objects().size()==1,"invalid retained physical definition: "+error);auto d=single.Objects()[0];
     a.Require(d.id==old&&d.body&&d.body->motion==SceneBodyMotion::Dynamic&&!d.parent&&!d.deformable&&!d.ragdoll&&!d.wheel&&!d.powertrain&&!d.joint&&d.scripts.empty(),"retained physical part ownership differs");
     d.id=0;auto id=CreateEntity(d,&state,&error);a.Require(id!=0,"retained physical part allocation failed: "+error);created.push_back(id);remap[old]=id;FindEntity(id)->requiresFull=true;
     if(staged){auto body=RuntimeBody(id);m_physics.SetBodyEnabled(body,false);m_regionPending.insert(id);}
    }
   }
  }
  PersistRigidFracture(owner,a,a.reading?&remap:nullptr);
 }catch(...){for(auto id:created)RemoveRegionObject(id);throw;}
}

bool RuntimeWorld::LoadFracturePart(EntityId owner,unsigned part,glm::vec3 value,bool impulse){auto it=m_deformables.find(owner);if(it==m_deformables.end())return false;auto& sim=it->second.simulation;if(!sim.asset->fracture||part>=sim.asset->fracture->parts.size()||sim.fracture.removed[part]||!sim.settings.enabled)return false;if(!sim.asset->fracture->rigid)return sim.PartLoad(part,value,impulse);auto id=FracturePartEntity(owner,part);auto body=RuntimeBody(id);if(!body.IsValid())return false;if(impulse)m_physics.ApplyLinearImpulse(body,value);else m_physics.ApplyForce(body,value);return true;}
void RuntimeWorld::DispatchFractureEvents(){if(!m_scripts)return;struct Event {EntityId id;uint64_t revision;std::vector<std::string> keys;std::vector<unsigned> causes;};std::vector<Event> events;for(auto& [id,record]:m_deformables){auto& s=record.simulation;if(!s.asset->fracture||s.fracture.committed.empty())continue;Event e{id,s.fracture.revision,{},{}};for(auto b:s.fracture.committed){e.keys.push_back(s.asset->fracture->bonds[b].key);e.causes.push_back(s.fracture.broken[b]);}s.fracture.committed.clear();events.push_back(std::move(e));}for(auto& e:events)if(RuntimeDefinition(e.id))m_scripts->FractureEvent(e.id,e.revision,e.keys,e.causes);}
