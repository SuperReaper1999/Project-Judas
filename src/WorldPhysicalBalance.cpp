#include "RuntimeWorld.h"
#include "PerformanceProfiler.h"
#include "DebugDraw.h"
#include "Renderer.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace {
bool ResolveChains(const RagdollDefinition& rig,const Skeleton& skeleton,
                   BalanceSettings& settings,const BalanceDefinition& definition,std::string& error){
    auto index=[&](const std::string& key){const int node=FindSkeletonJoint(skeleton,key);if(node<0)return -1;
        for(size_t i=0;i<rig.bones.size();++i)if(FindSkeletonJoint(skeleton,rig.bones[i].joint)==node)return int(i);
        return -1;};
    if(!ValidBalanceDefinition(definition,error))return false;
    if(rig.bones.size()<7||!rig.bones.front().parent.empty()||!rig.selfCollision){error="balance requires a parent-first free root and ordinary non-adjacent self collision";return false;}
    for(unsigned f=0;f<2;++f){const auto& c=definition.chains[f];int upper=index(c.upper),lower=index(c.lower),foot=index(c.foot);
        if(upper<=0||lower<=upper||foot<=lower||index(rig.bones[upper].parent)!=0||index(rig.bones[lower].parent)!=upper||index(rig.bones[foot].parent)!=lower){error="balance keys must resolve to independent root/upper/lower/foot physical chains";return false;}
        auto& e=settings.effectors[f];e.upper=unsigned(upper);e.lower=unsigned(lower);e.foot=unsigned(foot);
        const auto& shape=rig.bones[foot];
        if(shape.shape!=RagdollShape::Box||glm::any(glm::lessThan(e.halfExtents,shape.halfExtents))){error="balance clearance envelope must contain the authored box foot";return false;}
    }
    return true;
}
}
bool RuntimeWorld::ConfigurePhysicalBalance(EntityId id,const std::optional<BalanceDefinition>& configuration,std::string& error){
    auto* d=RuntimeDefinition(id);auto* animation=RuntimeAnimation(id);
    if(!d||!d->ragdoll||!animation||!animation->asset){error="balance needs a ready animated ragdoll mapping";return false;}
    if(PhysicalBalanceOwnsJoints(id)){error="cancel physical balance before replacing its configuration";return false;}
    if(configuration){auto settings=configuration->settings;if(!ResolveChains(*d->ragdoll,animation->asset->skeleton,settings,*configuration,error))return false;}
    CancelPhysicalBalance(id);m_balances.erase(id);m_scriptDefinitions.at(id).ragdoll->balance=configuration;return true;
}
bool RuntimeWorld::RequestPhysicalBalance(EntityId id,const BalanceRequest& request,std::string& error){
    const auto* d=RuntimeDefinition(id);auto* animation=RuntimeAnimation(id);
    if(!d||!d->ragdoll||!d->ragdoll->balance||!d->ragdoll->balance->enabled||!animation||!animation->asset){error="balance needs enabled explicit configuration and a ready skeleton";return false;}
    if(request.task!=BalanceTask::Stand&&request.task!=BalanceTask::Recover){error="invalid physical balance task";return false;}
    auto settings=d->ragdoll->balance->settings;if(!ResolveChains(*d->ragdoll,animation->asset->skeleton,settings,*d->ragdoll->balance,error))return false;
    if(!m_balances.count(id)&&m_balances.size()>=128){error="physical balance participant bound (128)";return false;}
    PhysicalAnimationRequest handoff;handoff.mode=PhysicalAnimationMode::Passive;handoff.motorHandoff=request.motorHandoff;
    if(!RequestPhysicalAnimation(id,handoff,error))return false;
    auto& state=m_balances[id];state.pending=request;state.diagnostic.clear();return true;
}
bool RuntimeWorld::BuildPhysicalBalance(EntityId id,PhysicalBalanceController& controller,std::string& error){
    const auto* d=RuntimeDefinition(id);auto found=m_ragdolls.find(id);
    if(!d||!d->ragdoll||!d->ragdoll->balance||found==m_ragdolls.end()||found->second.partial){error="balance requires a complete dynamic articulation";return false;}
    auto& rig=found->second;auto settings=d->ragdoll->balance->settings;
    if(!ResolveChains(*d->ragdoll,rig.asset->skeleton,settings,*d->ragdoll->balance,error))return false;
    const float scale=rig.reference.scale.x;
    settings.standingHeight*=scale;settings.standingTolerance*=scale;settings.stepTrigger*=scale;settings.stepReach*=scale;settings.stepHeight*=scale;settings.touchdownTolerance*=scale;
    for(auto& e:settings.effectors){e.contactOffset*=scale;e.halfExtents*=scale;for(auto& p:e.patches){p.offset*=scale;p.radius*=scale;}}
    auto globals=PoseGlobalMatrices(rig.asset->skeleton,rig.asset->skeleton.rest);
    std::vector<BalanceBody> bodies;glm::quat rootRotation(1,0,0,0);size_t joint=0;
    for(size_t i=0;i<rig.bodies.size();++i){const auto& mapped=rig.bodies[i];JointTransform rest;
        if(!DecomposeRigidPose(globals[mapped.node],rest,error))return false;
        auto orientation=glm::normalize(rest.rotation*mapped.orientation);if(!i)rootRotation=orientation;
        bodies.push_back({mapped.body,mapped.parent<0?JointHandle{}:rig.joints.at(joint++),mapped.parent,glm::normalize(glm::inverse(rootRotation)*orientation)});
    }
    return controller.Configure(m_physics,bodies,settings,error);
}
void RuntimeWorld::CancelPhysicalBalance(EntityId id){
    auto found=m_balances.find(id);if(found==m_balances.end())return;
    if(found->second.configured)found->second.controller.Cancel(m_physics);
    found->second.configured=false;
    found->second.pending.reset();found->second.diagnostic="cancelled";
}
const RuntimeWorld::BalanceRuntime* RuntimeWorld::PhysicalBalanceSnapshot(EntityId id)const{
    auto found=m_balances.find(id);return found==m_balances.end()?nullptr:&found->second;
}
bool RuntimeWorld::PhysicalBalanceOwnsJoints(EntityId id)const{
    auto* state=PhysicalBalanceSnapshot(id);if(!state||!state->configured)return false;
    auto progress=state->controller.History().progress;
    return progress==BalanceProgress::Executing||progress==BalanceProgress::Succeeded;
}
void RuntimeWorld::PreparePhysicalBalances(float dt){
    if(m_balances.empty())return;
    JUDAS_PROFILE_SCOPE("Physical balance controller");
    for(auto& [id,state]:m_balances){const auto* d=RuntimeDefinition(id);auto rig=m_ragdolls.find(id);
        if(!d||!d->ragdoll||!d->ragdoll->enabled||!d->ragdoll->balance||!d->ragdoll->balance->enabled||rig==m_ragdolls.end()||rig->second.partial){CancelPhysicalBalance(id);continue;}
        if(state.pending){auto request=*state.pending;state.pending.reset();
            state.controller.Cancel(m_physics);state.configured=BuildPhysicalBalance(id,state.controller,state.diagnostic);
            if(!state.configured)continue;
            state.asset=rig->second.asset;
            if(!state.controller.Request(m_physics,request.task,request.stepping,request.effort,state.diagnostic)){state.configured=false;continue;}
        }
        if(!state.configured||!PhysicalBalanceOwnsJoints(id))continue;
        if(state.asset!=rig->second.asset){CancelPhysicalBalance(id);state.diagnostic="balance skeleton identity changed";continue;}
        // The same per-body selection used by the ordinary gravity integration.
        // An owner override is deliberately not inherited by transient bones.
        auto& gravity=state.gravityScratch;gravity.clear();gravity.reserve(rig->second.bodies.size());
        for(const auto& bone:rig->second.bodies)gravity.push_back(SampleEntityGravity(bone.entity,m_physics.GetCenterOfMass(bone.body)));
        state.controller.Prepare(m_physics,gravity,dt);state.diagnostic=state.controller.History().reason;
    }
}

bool RuntimeWorld::SetPhysicalBalanceDiagnostics(EntityId id,bool enabled){
    const auto* d=RuntimeDefinition(id);if(!d||!d->ragdoll||!d->ragdoll->balance)return false;
    m_scriptDefinitions.at(id).ragdoll->balance->visualize=enabled;return true;
}
void RuntimeWorld::DrawPhysicalBalances(Renderer& renderer)const{
    if(m_balances.empty())return;
    DebugLineList lines;
    for(const auto& [id,state]:m_balances){const auto* d=RuntimeDefinition(id);auto rig=m_ragdolls.find(id);
        if(!d||!d->ragdoll||!d->ragdoll->balance||!d->ragdoll->balance->visualize||rig==m_ragdolls.end())continue;
        for(size_t i=0;i<rig->second.bodies.size();++i){const auto& mapped=rig->second.bodies[i];if(!m_physics.IsBodyEnabled(mapped.body))continue;auto pose=m_physics.GetTransform(mapped.body);const auto& shape=d->ragdoll->bones.at(i);auto color=glm::vec3(.95f,.6f,.15f);
            if(shape.shape==RagdollShape::Box)lines.Box(pose.position,pose.rotation,shape.halfExtents*mapped.scale.x,color);else lines.Sphere(pose.position,shape.radius*mapped.scale.x,color,12);
        }
        const auto& o=state.controller.Observation();if(o.available){lines.Sphere(o.com,.05f,{.1f,1,.4f},12);lines.Arrow(o.com,o.com+o.gravity*.04f,{1,.2f,.2f});for(const auto& support:o.supports)if(support.contacts){lines.Cross(support.point,.08f,{.2f,.9f,1});lines.Arrow(support.point,support.point+support.normal*.25f,{.2f,.9f,1});}}
        const auto& h=state.controller.History();if(h.swing>=0&&m_physics.IsBodyEnabled(h.plannedSupport)){auto support=m_physics.GetTransform(h.plannedSupport);lines.Cross(support.position+support.rotation*h.landingLocal,.1f,{1,.2f,1});}
    }
    if(!lines.Empty())renderer.DrawDebugLines(lines.Lines(),false);
}
