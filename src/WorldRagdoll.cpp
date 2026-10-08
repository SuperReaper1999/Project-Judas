#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <set>

namespace {
glm::mat4 Matrix(const SceneTransform& t){return glm::translate(glm::mat4(1),t.position)*glm::mat4_cast(t.rotation)*glm::scale(glm::mat4(1),t.scale);}
bool UniformPositive(glm::vec3 s){return s.x>0&&std::abs(s.x-s.y)<1e-4f&&std::abs(s.x-s.z)<1e-4f;}
glm::vec3 AngularVelocity(glm::quat previous,glm::quat current,float dt){
    auto delta=glm::normalize(current*glm::inverse(previous));if(delta.w<0)delta=-delta;
    float length=glm::length(glm::vec3(delta.x,delta.y,delta.z));
    return length>1e-6f?glm::vec3(delta.x,delta.y,delta.z)*(2*std::atan2(length,delta.w)/(length*dt)):glm::vec3(0);
}

bool RegionMapping(const RagdollDefinition& definition,const Skeleton& skeleton,
    const PhysicalAnimationSettings& settings,std::vector<int>& regions,std::string& error,
    const RuntimeWorld::RagdollInstance* articulation=nullptr){
    regions.assign(definition.bones.size(),-1);
    std::vector<int> nodes;nodes.reserve(definition.bones.size());
    for(size_t i=0;i<definition.bones.size();++i)nodes.push_back(articulation&&i<articulation->bodies.size()?articulation->bodies[i].node:FindSkeletonJoint(skeleton,definition.bones[i].joint));
    for(size_t r=0;r<settings.regions.size();++r){const auto& region=settings.regions[r];
        std::set<int> seen;
        for(size_t j=0;j<region.joints.size();++j){const int node=FindSkeletonJoint(skeleton,region.joints[j]);
            auto b=std::find(nodes.begin(),nodes.end(),node);
            if(node<0||b==nodes.end()||!seen.insert(node).second){error="physical region["+std::to_string(r)+"] joint["+std::to_string(j)+"] is missing, ambiguous, duplicate or not physically mapped";return false;}
            if(region.enabled)regions[size_t(b-nodes.begin())]=int(r);
        }
    }return true;
}
void ClearRegionCache(PhysicalAnimationState& state){
    state.mappedAsset.reset();state.mappedSettings.reset();state.mappedBones.clear();state.mappedJointKeys.clear();state.mappedRegions.clear();
}
bool CachedRegionMapping(const RagdollDefinition& definition,
    const std::shared_ptr<const SkeletalAsset>& asset,const RuntimeWorld::RagdollInstance& articulation,
    PhysicalAnimationState& state,std::string& error){
    const auto& settings=definition.physicalAnimation;
    bool unchanged=state.mappedAsset==asset&&state.mappedBones.size()==definition.bones.size()&&bool(state.mappedSettings)==bool(settings);
    if(unchanged&&settings)unchanged=PhysicalAnimationSettingsEqual(*state.mappedSettings,*settings);
    if(unchanged)for(size_t i=0;i<definition.bones.size();++i)if(state.mappedBones[i]!=definition.bones[i].joint){unchanged=false;break;}
    if(unchanged)return true;
    std::vector<int> regions(definition.bones.size(),-1);std::vector<std::string> bones,keys;
    if(settings&&!RegionMapping(definition,asset->skeleton,*settings,regions,error,&articulation))return false;
    for(size_t i=0;i<definition.bones.size();++i){bones.push_back(definition.bones[i].joint);keys.push_back(SkeletonJointKey(asset->skeleton,articulation.bodies.at(i).node));}
    state.mappedAsset=asset;state.mappedSettings=settings;state.mappedBones=std::move(bones);state.mappedJointKeys=std::move(keys);state.mappedRegions=std::move(regions);return true;
}
const SkeletalPose& DriveReference(const RuntimeWorld::AnimationInstance& animation){
    return animation.solvedReferencePose.local.empty()?animation.finalPose:animation.solvedReferencePose;
}
bool CaptureActualArticulationPose(const Skeleton& skeleton,const SkeletalPose& reference,
    const RuntimeWorld::RagdollInstance& instance,const SceneTransform& transform,
    const PhysicsWorld& physics,SkeletalPose& pose,std::string& error){
    pose=reference;auto inverseModel=glm::inverse(Matrix(transform));std::vector<glm::mat4> globals(skeleton.parents.size());
    for(int node:skeleton.order){auto mapped=std::find_if(instance.bodies.begin(),instance.bodies.end(),[&](const auto& b){return b.node==node;});
        const size_t index=size_t(mapped-instance.bodies.begin());
        if(mapped!=instance.bodies.end()&&(!instance.partial||instance.dynamic[index])){
            auto body=physics.GetTransform(mapped->body);auto rotation=glm::normalize(body.rotation*glm::inverse(mapped->orientation));auto position=body.position-rotation*mapped->offset;
            globals[node]=inverseModel*glm::translate(glm::mat4(1),position)*glm::mat4_cast(rotation)*glm::scale(glm::mat4(1),mapped->scale);
            auto local=skeleton.parents[node]<0?globals[node]:glm::inverse(globals[skeleton.parents[node]])*globals[node];if(!skeleton.affine.empty())local=glm::inverse(skeleton.affine.at(node))*local;
            if(!DecomposeRigidPose(local,pose.local[node],error))return false;
        }else {const auto& local=pose.local[node];auto matrix=glm::translate(glm::mat4(1),local.translation)*glm::mat4_cast(local.rotation)*glm::scale(glm::mat4(1),local.scale);if(!skeleton.affine.empty())matrix=skeleton.affine.at(node)*matrix;globals[node]=skeleton.parents[node]<0?matrix:globals[skeleton.parents[node]]*matrix;}
    }return true;
}
}
bool RuntimeWorld::ConfigurePhysicalAnimation(EntityId id,const std::optional<PhysicalAnimationSettings>& settings,std::string& error){
    const auto* definition=RuntimeDefinition(id);auto* animation=RuntimeAnimation(id);
    if(!definition||!definition->ragdoll||!animation||!animation->asset){error="physical animation needs a ready animated ragdoll mapping";return false;}
    if(settings){if(!ValidPhysicalAnimationSettings(*settings,error))return false;std::vector<int> mapping;
        if(!RegionMapping(*definition->ragdoll,animation->asset->skeleton,*settings,mapping,error))return false;}
    if(bool(settings)==bool(definition->ragdoll->physicalAnimation)&&(!settings||PhysicalAnimationSettingsEqual(*settings,*definition->ragdoll->physicalAnimation))){
        if((!settings||!settings->enabled)&&m_physicalAnimations.count(id)&&m_physicalAnimations.at(id).mode!=PhysicalAnimationMode::Animation)m_physicalAnimations.at(id).pending=PhysicalAnimationRequest{};
        return true;
    }
    auto& state=m_physicalAnimations[id];
    if(state.mode==PhysicalAnimationMode::Partial&&settings&&settings->enabled){std::vector<int> previous,current;
        if(definition->ragdoll->physicalAnimation)RegionMapping(*definition->ragdoll,animation->asset->skeleton,*definition->ragdoll->physicalAnimation,previous,error);
        RegionMapping(*definition->ragdoll,animation->asset->skeleton,*settings,current,error);
        bool changed=previous.size()!=current.size();for(size_t i=0;i<std::min(previous.size(),current.size());++i)changed|=(previous[i]>=0)!=(current[i]>=0);
        if(changed){if(current.empty()||current.front()>=0||std::none_of(current.begin(),current.end(),[](int r){return r>=0;})){error="partial reconfiguration must retain an animation-owned physical root and a selected region";return false;}
            state.rebuild=true;PhysicalAnimationRequest request;request.mode=PhysicalAnimationMode::Partial;state.pending=request;}
    }
    m_scriptDefinitions.at(id).ragdoll->physicalAnimation=settings;
    state.previousDriveTargets.clear();ClearRegionCache(state);
    if(!settings||!settings->enabled){PhysicalAnimationRequest request;state.pending=request;}
    return true;
}
bool RuntimeWorld::RequestPhysicalAnimation(EntityId id,const PhysicalAnimationRequest& request,std::string& error){
    if(!ValidPhysicalAnimationRequest(request,error))return false;
    const auto* definition=RuntimeDefinition(id);auto* animation=RuntimeAnimation(id);
    if(!definition||!definition->ragdoll||!definition->ragdoll->enabled||!animation||!animation->asset||!definition->animation->enabled||definition->body){error="physical authority needs an enabled ready animated ragdoll mapping without a separate root collider";return false;}
    if(!ValidRagdollDefinition(*definition->ragdoll,error))return false;
    std::vector<int> mapping;const auto& settings=definition->ragdoll->physicalAnimation;
    if(request.mode==PhysicalAnimationMode::Partial||request.mode==PhysicalAnimationMode::Active){
        if(!settings||!settings->enabled){error="active/partial authority needs enabled physical animation settings";return false;}
        if(!RegionMapping(*definition->ragdoll,animation->asset->skeleton,*settings,mapping,error))return false;
        if(request.mode==PhysicalAnimationMode::Partial&&(mapping.empty()||mapping.front()>=0||std::none_of(mapping.begin(),mapping.end(),[](int r){return r>=0;}))){error="partial authority needs selected joints below an animation-owned physical root";return false;}
    }
    auto transform=PresentedTransform(id,definition->transform,1);if(!UniformPositive(transform.scale)){error="physical authority requires positive uniform entity scale";return false;}
    auto globals=PoseGlobalMatrices(animation->asset->skeleton,animation->finalPose);std::vector<bool> needed(definition->ragdoll->bones.size(),request.mode!=PhysicalAnimationMode::Partial);
    if(request.mode==PhysicalAnimationMode::Partial)for(size_t i=0;i<mapping.size();++i)if(mapping[i]>=0){needed[i]=true;const auto& parent=definition->ragdoll->bones[i].parent;for(size_t p=0;p<definition->ragdoll->bones.size();++p)if(definition->ragdoll->bones[p].joint==parent)needed[p]=true;}
    if(request.mode!=PhysicalAnimationMode::Animation)for(size_t i=0;i<definition->ragdoll->bones.size();++i){const auto& bone=definition->ragdoll->bones[i];int node=FindSkeletonJoint(animation->asset->skeleton,bone.joint);JointTransform mapped;
        if(node<0||(needed[i]&&(!DecomposeRigidPose(Matrix(transform)*globals[size_t(node)],mapped,error)||!UniformPositive(mapped.scale)))){error="physical mapping has an unavailable/nonrigid/nonuniform joint: "+bone.joint;return false;}}
    const bool fullyPhysical=request.mode==PhysicalAnimationMode::Active||request.mode==PhysicalAnimationMode::Passive;
    if(definition->characterMotor&&definition->characterMotor->enabled&&fullyPhysical&&!request.motorHandoff){error="full physical authority requires explicit motorHandoff";return false;}
    if(request.resumeMotor&&(!definition->characterMotor||!request.placement)){error="motor resume requires an explicit collision-valid placement";return false;}
    if(request.placement&&definition->characterMotor){auto* motor=RuntimeCharacter(id);
        auto filter=motor->filter;filter.includeSensors=false;if(auto r=m_ragdolls.find(id);r!=m_ragdolls.end())for(const auto& b:r->second.bodies)filter.ignoredBodies.push_back(b.body);
        auto p=*request.placement;p.rotation=glm::normalize(p.rotation);
        const auto hit=m_physics.SweepCapsuleMotion(motor->settings.radius,motor->settings.halfHeight,p.position+p.rotation*motor->settings.offset,p.rotation,glm::vec3(0),true,0,0,&filter,motor->settings.collisionLayer,motor->settings.collisionMask);
        if(hit.hit&&hit.penetration>1e-5f){error="physical return refused: requested motor placement penetrates collision geometry";return false;}
        // Validate the same bounded recovery + capsule sweep used by the motor.
        // These are read-only hypothetical queries; no root or body is moved.
        auto start=transform.position+p.rotation*motor->settings.offset;
        for(int recovery=0;recovery<8;++recovery){auto overlap=m_physics.SweepCapsuleMotion(motor->settings.radius,motor->settings.halfHeight,start,p.rotation,glm::vec3(0),true,0,0,&filter,motor->settings.collisionLayer,motor->settings.collisionMask);
            if(!overlap.hit||overlap.penetration<=1e-5f)break;
            start+=overlap.normal*(overlap.penetration+motor->settings.skin);}
        const auto displacement=p.position+p.rotation*motor->settings.offset-start;
        auto path=m_physics.SweepCapsuleMotion(motor->settings.radius,motor->settings.halfHeight,start,p.rotation,displacement,false,0,1,&filter,motor->settings.collisionLayer,motor->settings.collisionMask);
        if(path.hit&&path.distance+motor->settings.skin<glm::length(displacement)){error="physical return refused: requested motor placement path is blocked";return false;}
    }
    m_physicalAnimations[id].pending=request;return true;
}
PhysicalAnimationState RuntimeWorld::PhysicalAnimationSnapshot(EntityId id)const{
    PhysicalAnimationState result;auto found=m_physicalAnimations.find(id);if(found==m_physicalAnimations.end())return result;
    const auto& state=found->second;result.mode=state.mode;result.pending=state.pending;result.observations=state.observations;result.diagnostic=state.diagnostic;result.motorWasEnabled=state.motorWasEnabled;return result;
}
std::pair<EntityId,std::string> RuntimeWorld::ArticulatedIdentity(EntityId body)const{
    for(const auto& [owner,instance]:m_ragdolls)for(const auto& mapped:instance.bodies)if(mapped.entity==body&&RuntimeBody(body).id==mapped.body.id)
        return {owner,SkeletonJointKey(instance.asset->skeleton,mapped.node)};
    return {};
}
void RuntimeWorld::PreparePhysicalAnimations(float dt){
    JUDAS_PROFILE_SCOPE("Physical animation drives");
    if(!(dt>0)||!std::isfinite(dt))return;
    // Explicit participant registry, not a per-property whole-world search.
    for(auto id:m_animationOwners){const auto* definition=RuntimeDefinition(id);if(definition&&definition->ragdoll&&definition->ragdoll->physicalAnimation)m_physicalAnimations.try_emplace(id);}
    std::vector<EntityId> retired;
    for(auto& [id,state]:m_physicalAnimations){auto* animation=RuntimeAnimation(id);const auto* definition=RuntimeDefinition(id);
        if(!definition||!definition->ragdoll||!definition->animation||!animation){retired.push_back(id);continue;}
        state.observations.clear();
        if(!animation->asset){state.diagnostic="physical skeleton is not ready";continue;}
        if(auto found=m_ragdolls.find(id);found!=m_ragdolls.end()){
            const bool valid=std::all_of(found->second.bodies.begin(),found->second.bodies.end(),[&](const auto& body){return RuntimeBody(body.entity).id==body.body.id&&m_physics.IsBodyEnabled(body.body);});
            if(!valid){std::string error;LeaveRagdoll(id,0,error);state.pending.reset();state.diagnostic="physical articulation retired after mapped body destruction/disable";continue;}
        }
        if(state.observedAsset&&state.observedAsset!=animation->asset){std::string error;LeaveRagdoll(id,0,error);state.pending.reset();state.previousReferenceWorld.clear();state.recentReferenceWorld.clear();state.previousDriveTargets.clear();ClearRegionCache(state);state.motionDt=0;state.diagnostic="physical mapping retired after skeleton replacement";}
        state.observedAsset=animation->asset;
        const auto& skeleton=animation->asset->skeleton;
        auto transform=PresentedTransform(id,definition->transform,1);auto referenceWorld=PoseGlobalMatrices(skeleton,DriveReference(*animation));for(auto& matrix:referenceWorld)matrix=Matrix(transform)*matrix;
        state.previousReferenceWorld=std::move(state.recentReferenceWorld);state.recentReferenceWorld=std::move(referenceWorld);state.motionDt=dt;
        if(!definition->ragdoll->enabled||!definition->animation->enabled){std::string error;LeaveRagdoll(id,0,error);state.pending.reset();continue;}
        if(state.pending){auto request=*state.pending;state.pending.reset();std::string error;
            // Revalidate at publication: another callback may have moved geometry.
            if(!RequestPhysicalAnimation(id,request,error)){state.pending.reset();state.diagnostic=error;continue;}state.pending.reset();
            const bool wantPartial=request.mode==PhysicalAnimationMode::Partial;
            auto articulation=m_ragdolls.find(id);
            const bool topologyChange=articulation!=m_ragdolls.end()&&(articulation->second.partial!=wantPartial||state.rebuild);
            std::vector<EntityPhysicalState> preserved;std::vector<bool> wasDynamic;SkeletalPose captured=animation->finalPose;glm::vec3 rootVelocity(0);
            if(articulation!=m_ragdolls.end()){rootVelocity=m_physics.GetLinearVelocity(articulation->second.bodies.front().body);wasDynamic=articulation->second.dynamic;if(wasDynamic.empty())wasDynamic.assign(articulation->second.bodies.size(),true);
                if(topologyChange&&!CaptureActualArticulationPose(skeleton,DriveReference(*animation),articulation->second,transform,m_physics,captured,error)){state.diagnostic=error;continue;}
                for(const auto& body:articulation->second.bodies){EntityPhysicalState motion;GetEntityState(body.entity,motion);preserved.push_back(motion);}}
            const auto oldMode=state.mode;
            const bool priorMotorEnabled=definition->characterMotor&&definition->characterMotor->enabled;
            if(request.mode==PhysicalAnimationMode::Animation){
                // A partial motor has already advanced its root this fixed
                // phase. Capture actual bones in that current root frame,
                // rather than reusing the preceding physical local pose.
                if(topologyChange)animation->finalPose=captured;
                if(!LeaveRagdoll(id,request.fade,error)){state.diagnostic=error;continue;}
                if(request.placement){auto placed=transform;placed.position=request.placement->position;placed.rotation=glm::normalize(request.placement->rotation);SetRuntimeTransform(id,placed);}
                if(request.resumeMotor){auto* motor=RuntimeCharacter(id);auto settings=motor->settings;settings.enabled=true;SetCharacterSettings(id,settings);motor->velocity=rootVelocity;}
                state.mode=PhysicalAnimationMode::Animation;state.diagnostic.clear();state.rebuild=false;continue;
            }
            const bool fullyPhysical=request.mode==PhysicalAnimationMode::Active||request.mode==PhysicalAnimationMode::Passive;
            if(fullyPhysical&&definition->characterMotor&&definition->characterMotor->enabled){state.motorWasEnabled=true;auto settings=*definition->characterMotor;settings.enabled=false;SetCharacterSettings(id,settings);}
            if(topologyChange){if(!LeaveRagdoll(id,0,error)){state.diagnostic=error;continue;}animation->finalPose=captured;}
            state.mode=request.mode;
            if(!EnterRagdoll(id,error)){
                state.mode=oldMode;state.diagnostic=error;
                if(topologyChange){std::string ignored;animation->finalPose=captured;EnterRagdoll(id,ignored);auto restored=m_ragdolls.find(id);if(restored!=m_ragdolls.end())for(size_t i=0;i<std::min(restored->second.bodies.size(),preserved.size());++i)SetEntityState(restored->second.bodies[i].entity,preserved[i]);}
                if(auto* motor=RuntimeCharacter(id)){auto settings=motor->settings;settings.enabled=priorMotorEnabled;SetCharacterSettings(id,settings);}
                continue;
            }
            auto& created=m_ragdolls.at(id);created.physicalControlled=true;
            if(created.dynamic.empty())created.dynamic.assign(created.bodies.size(),true);
            if(topologyChange)for(size_t i=0;i<std::min(created.bodies.size(),preserved.size());++i)if(created.dynamic[i]&&wasDynamic[i])SetEntityState(created.bodies[i].entity,preserved[i]);
            state.previousDriveTargets.clear();state.rebuild=false;state.diagnostic.clear();
        }
        auto found=m_ragdolls.find(id);if(found==m_ragdolls.end()||!found->second.physicalControlled)continue;
        auto& instance=found->second;const auto& settings=definition->ragdoll->physicalAnimation;
        if(!CachedRegionMapping(*definition->ragdoll,animation->asset,instance,state,state.diagnostic))continue;
        const auto& regions=state.mappedRegions;
        if(instance.partial){
            for(size_t i=0;i<instance.bodies.size();++i){auto& mapped=instance.bodies[i];if(instance.dynamic[i])continue;JointTransform joint;std::string error;
                if(std::none_of(instance.bodies.begin(),instance.bodies.end(),[&](const auto& child){const auto index=size_t(&child-instance.bodies.data());return instance.dynamic[index]&&child.parent==int(i);}))continue;
                if(!DecomposeRigidPose(state.recentReferenceWorld[mapped.node],joint,error)){state.diagnostic=error;continue;}
                const auto position=joint.translation+joint.rotation*mapped.offset;const auto rotation=glm::normalize(joint.rotation*mapped.orientation);const auto previous=m_physics.GetTransform(mapped.body);
                if(glm::length(previous.position-position)>1e-7f||std::abs(glm::dot(previous.rotation,rotation))<1-1e-7f){m_physics.ResetBody(mapped.body,position,rotation);m_physics.SetLinearVelocity(mapped.body,(position-previous.position)/dt);m_physics.SetAngularVelocity(mapped.body,AngularVelocity(previous.rotation,rotation,dt));}
                else {m_physics.SetLinearVelocity(mapped.body,glm::vec3(0));m_physics.SetAngularVelocity(mapped.body,glm::vec3(0));}
            }
        }
        if(state.previousDriveTargets.size()!=instance.bodies.size())state.previousDriveTargets.assign(instance.bodies.size(),glm::quat(0,0,0,0));
        for(size_t i=0;i<instance.bodies.size();++i){auto& child=instance.bodies[i];if(!instance.dynamic[i]||child.parent<0||i>=regions.size()||regions[i]<0||!settings)continue;
            const auto& region=settings->regions[size_t(regions[i])];auto& parent=instance.bodies[size_t(child.parent)];PhysicalDriveObservation observation;observation.joint=state.mappedJointKeys[i];observation.region=region.id;
            JointTransform childReference,parentReference;std::string error;
            if(!DecomposeRigidPose(state.recentReferenceWorld[child.node],childReference,error)||!DecomposeRigidPose(state.recentReferenceWorld[parent.node],parentReference,error)){state.diagnostic=error;continue;}
            auto childTarget=glm::normalize(childReference.rotation*child.orientation),parentTarget=glm::normalize(parentReference.rotation*parent.orientation);
            bool limitSaturated=false;const auto& constraint=definition->ragdoll->bones[i].constraint;
            if(constraint.type==JointType::Hinge){auto frameA=childTarget*constraint.frameA,frameB=parentTarget*constraint.frameB;
                auto axis=frameA*glm::vec3(1,0,0),reference=frameA*glm::vec3(0,1,0),other=frameB*glm::vec3(0,1,0);
                float coordinate=std::atan2(glm::dot(axis,glm::cross(reference,other)),glm::dot(reference,other));const float limited=constraint.limits?std::clamp(coordinate,constraint.lower,constraint.upper):coordinate;
                limitSaturated=limited!=coordinate;childTarget=glm::normalize(frameB*glm::angleAxis(-limited,glm::vec3(1,0,0))*glm::inverse(constraint.frameA));}
            const auto target=glm::normalize(glm::inverse(parentTarget)*childTarget);const auto previous=state.previousDriveTargets[i];
            const bool changed=glm::dot(previous,previous)<.5f||std::abs(glm::dot(previous,target))<1-1e-7f;state.previousDriveTargets[i]=target;
            auto childPose=m_physics.GetTransform(child.body),parentPose=m_physics.GetTransform(parent.body);
            auto applied=region;if(state.mode==PhysicalAnimationMode::Passive)applied.effortWeight=0;
            auto torque=CalculatePhysicalTorque(target,childPose.rotation,parentPose.rotation,m_physics.GetAngularVelocity(child.body),m_physics.GetAngularVelocity(parent.body),m_physics.GetInertiaWorld(child.body),m_physics.IsDynamicBody(parent.body)?glm::inverse(m_physics.GetInertiaWorld(parent.body)):glm::mat3(0),applied,dt);
            observation.angleError=torque.angleError;observation.saturated=torque.saturated||limitSaturated;observation.sleeping=m_physics.IsSleeping(child.body)&&(!m_physics.IsDynamicBody(parent.body)||m_physics.IsSleeping(parent.body));
            if(!observation.sleeping||changed){m_physics.ApplyTorque(child.body,torque.torque,changed);m_physics.ApplyTorque(parent.body,-torque.torque,changed);observation.torque=glm::length(torque.torque);}
            state.observations.push_back(std::move(observation));
        }
    }
    for(auto id:retired)m_physicalAnimations.erase(id);
    JUDAS_PROFILE_COUNTER("Physical animation participants",double(m_physicalAnimations.size()),ProfileCounterMode::Latest);
}
bool RuntimeWorld::RagdollActive(EntityId id)const{return m_ragdolls.count(id)!=0;}
EntityId RuntimeWorld::RagdollBody(EntityId id,const std::string& key)const{
    auto it=m_ragdolls.find(id);if(it==m_ragdolls.end())return 0;
    int node=FindSkeletonJoint(it->second.asset->skeleton,key);
    for(const auto& b:it->second.bodies)if(b.node==node&&RuntimeBody(b.entity).id==b.body.id&&m_physics.IsBodyEnabled(b.body))return b.entity;
    return 0;
}
bool RuntimeWorld::EnterRagdoll(EntityId id,std::string& error){
    if(RagdollActive(id))return true;
    const auto* definition=RuntimeDefinition(id);
    if(!definition||!definition->ragdoll||!definition->ragdoll->enabled||!definition->animation||!definition->animation->enabled||definition->body){error="ragdoll needs an enabled animated entity without a separate root collider";return false;}
    auto* animation=RuntimeAnimation(id);if(!animation||!animation->asset){error="ragdoll animation asset is not ready";return false;}
    auto settings=*definition->ragdoll; // body creation may grow the runtime entity containers
    if(!ValidRagdollDefinition(settings,error))return false;
    auto reference=PresentedTransform(id,definition->transform,1);
    if(!UniformPositive(reference.scale)){error="ragdoll requires positive uniform entity scale";return false;}
    const auto asset=animation->asset;const auto& skeleton=asset->skeleton;
    auto globals=PoseGlobalMatrices(skeleton,animation->finalPose);auto model=Matrix(reference);
    RagdollInstance instance;instance.asset=asset;instance.reference=reference;
    auto physical=m_physicalAnimations.find(id);
    instance.physicalControlled=physical!=m_physicalAnimations.end()&&physical->second.mode!=PhysicalAnimationMode::Animation;
    instance.partial=instance.physicalControlled&&physical->second.mode==PhysicalAnimationMode::Partial;
    if(definition->characterMotor&&definition->characterMotor->enabled&&!instance.physicalControlled){error="legacy ragdoll activation cannot bypass an enabled motor; request explicit physical authority";return false;}
    std::vector<int> regions;
    if(instance.partial){if(!settings.physicalAnimation||!RegionMapping(settings,skeleton,*settings.physicalAnimation,regions,error))return false;
        if(regions.empty()||regions.front()>=0){error="partial authority requires an animation-owned mapped physical root; use active for a dynamic root";return false;}
        if(std::none_of(regions.begin(),regions.end(),[](int region){return region>=0;})){error="partial authority needs at least one enabled physical region";return false;}}
    instance.dynamic.resize(settings.bones.size(),true);
    if(instance.partial)for(size_t i=0;i<regions.size();++i)instance.dynamic[i]=regions[i]>=0;
    auto required=instance.dynamic;
    if(instance.partial)for(size_t i=0;i<settings.bones.size();++i)if(instance.dynamic[i])for(size_t p=0;p<settings.bones.size();++p)if(settings.bones[p].joint==settings.bones[i].parent)required[p]=true;
    std::vector<JointTransform> jointWorld;std::vector<int> nodes;
    for(const auto& b:settings.bones){
        int node=FindSkeletonJoint(skeleton,b.joint);
        if(node<0||std::find(nodes.begin(),nodes.end(),node)!=nodes.end()){error="missing/ambiguous/duplicate mapped skeleton joint: "+b.joint;return false;}
        JointTransform world;std::string rigidError;
        if(!DecomposeRigidPose(model*globals[node],world,rigidError)||!UniformPositive(world.scale)){
            if(required[nodes.size()]){error="ragdoll requires non-sheared, uniform joint scale at activation";return false;}
            // An uninvolved hidden branch has no physical pose authority. Its
            // inert identity placeholder has no contacts, queries or joints.
            world.translation=glm::vec3((model*globals[node])[3]);world.rotation=reference.rotation;world.scale=reference.scale;
        }
        int parent=-1;
        if(!b.parent.empty()){
            auto p=std::find_if(settings.bones.begin(),settings.bones.end(),[&](const auto& v){return v.joint==b.parent;});parent=int(p-settings.bones.begin());
            int ancestor=skeleton.parents[node];while(ancestor>=0&&ancestor!=nodes.at(parent))ancestor=skeleton.parents[ancestor];
            if(ancestor<0){error="physical parent must be a mapped skeleton ancestor";return false;}
        }
        instance.bodies.push_back({node,parent,0,{},b.offset*world.scale.x,world.scale,glm::normalize(b.orientation)});nodes.push_back(node);jointWorld.push_back(world);
        if(b.collisionLayer>=64||!m_categories.collision.names.count(b.collisionLayer)){error="unknown ragdoll collision layer";return false;}
    }
    // Full preflight completed. Roll back only these newly created ordinary bodies on error.
    auto rollback=[&](){for(auto j:instance.joints)m_physics.DestroyJoint(j);for(const auto& b:instance.bodies)if(b.entity)DestroyEntity(b.entity);};
    for(size_t i=0;i<settings.bones.size();++i){const auto& b=settings.bones[i];auto& mapped=instance.bodies[i];const auto& w=jointWorld[i];
        EntityPhysicalState state;state.position=w.translation+w.rotation*mapped.offset;state.rotation=glm::normalize(w.rotation*mapped.orientation);
        const auto& previous=instance.physicalControlled?physical->second.previousReferenceWorld:animation->previousWorld;
        const auto& recent=instance.physicalControlled?physical->second.recentReferenceWorld:animation->recentWorld;
        const float motionDt=instance.physicalControlled?physical->second.motionDt:animation->motionDt;
        if(previous.size()==skeleton.parents.size()&&recent.size()==skeleton.parents.size()&&motionDt>0){
            JointTransform previous,current;std::string ignored;
            const auto& oldMatrices=instance.physicalControlled?physical->second.previousReferenceWorld:animation->previousWorld;
            const auto& newMatrices=instance.physicalControlled?physical->second.recentReferenceWorld:animation->recentWorld;
            if(DecomposeRigidPose(oldMatrices[mapped.node],previous,ignored)&&DecomposeRigidPose(newMatrices[mapped.node],current,ignored)){
                auto oldCenter=previous.translation+previous.rotation*(b.offset*previous.scale.x);
                auto newCenter=current.translation+current.rotation*mapped.offset;
                state.linearVelocity=(newCenter-oldCenter)/motionDt;state.angularVelocity=AngularVelocity(previous.rotation,current.rotation,motionDt);
            }
        }else {EntityPhysicalState inherited;if(GetEntityState(id,inherited)){state.linearVelocity=inherited.linearVelocity;state.angularVelocity=inherited.angularVelocity;}}
        SceneObject body;body.name="Articulation: "+b.joint;body.body=SceneBodyComponent{};auto& component=*body.body;
        component.motion=instance.dynamic[i]?SceneBodyMotion::Dynamic:SceneBodyMotion::Static;component.shape=b.shape==RagdollShape::Box?SceneShape::Box:SceneShape::Sphere;
        component.halfExtents=b.halfExtents*w.scale.x;component.radius=b.radius*w.scale.x;component.mass=b.mass;component.friction=b.friction;component.restitution=b.restitution;component.collisionLayer=b.collisionLayer;component.collisionMask=b.collisionMask;
        if(!instance.dynamic[i])component.collisionMask=0;
        body.tags=definition->tags; // identity/filter consumers see normal mapped entities
        mapped.entity=CreateEntity(body,&state,&error);if(!mapped.entity){rollback();return false;}
        mapped.body=RuntimeBody(mapped.entity);auto* record=FindEntity(mapped.entity);record->transient=true;record->requiresFull=true;
        if(!instance.dynamic[i])m_physics.SetBodyQueriesEnabled(mapped.body,false);
        if(instance.physicalControlled)if(auto* motor=RuntimeCharacter(id))motor->filter.ignoredBodies.push_back(mapped.body);
    }
    instance.rootLocalPosition=glm::vec3(globals[instance.bodies.front().node][3]);
    for(size_t i=0;i<instance.bodies.size();++i){auto& b=instance.bodies[i];if(b.parent<0)continue;
        if(instance.partial&&!instance.dynamic[i])continue;
        auto& parent=instance.bodies[b.parent];auto constraint=settings.bones[i].constraint;constraint.bodyA=b.body;constraint.bodyB=parent.body;
        if(settings.bones[i].autoAnchors){auto childPose=m_physics.GetTransform(b.body),parentPose=m_physics.GetTransform(parent.body);auto pivot=jointWorld[i].translation;
            constraint.anchorA=glm::inverse(childPose.rotation)*(pivot-childPose.position);constraint.anchorB=glm::inverse(parentPose.rotation)*(pivot-parentPose.position);
        }else {constraint.anchorA*=jointWorld[i].scale.x;constraint.anchorB*=jointWorld[b.parent].scale.x;}
        auto joint=m_physics.CreateJoint(constraint);if(!joint.IsValid()){error="could not create ragdoll constraint";rollback();return false;}instance.joints.push_back(joint);
        if(settings.bones[i].suppressParentCollision)m_physics.SetPairCollisionEnabled(b.body,parent.body,false);
    }
    if(!settings.selfCollision)for(size_t a=0;a<instance.bodies.size();++a)for(size_t b=a+1;b<instance.bodies.size();++b)m_physics.SetPairCollisionEnabled(instance.bodies[a].body,instance.bodies[b].body,false);
    m_ragdollReturns.erase(id);RemovePoseContribution(id,"ragdollReturn");m_ragdolls.emplace(id,std::move(instance));UpdateRagdolls(0);return true;
}
bool RuntimeWorld::LeaveRagdoll(EntityId id,float seconds,std::string& error){
    if(!std::isfinite(seconds)||seconds<0||seconds>3600){error="invalid ragdoll return duration";return false;}
    auto it=m_ragdolls.find(id);if(it==m_ragdolls.end())return true;
    auto instance=std::move(it->second);m_ragdolls.erase(it); // callbacks/destruction cannot see a half articulation
    if(auto physical=m_physicalAnimations.find(id);physical!=m_physicalAnimations.end()){physical->second.mode=PhysicalAnimationMode::Animation;physical->second.previousDriveTargets.clear();physical->second.observations.clear();physical->second.pending.reset();}
    auto* animation=RuntimeAnimation(id);SkeletalPose captured;if(animation)captured=animation->finalPose;
    for(auto joint:instance.joints)m_physics.DestroyJoint(joint);
    for(const auto& b:instance.bodies)DestroyEntity(b.entity);
    RemovePoseContribution(id,"ragdollPhysics");
    if(seconds>0&&!captured.local.empty()){
        PoseContribution contribution;contribution.pose=std::move(captured);contribution.order=1000;
        if(!SetPoseContribution(id,"ragdollReturn",contribution,error))return false;
        m_ragdollReturns[id]={0,seconds};
    }else {RemovePoseContribution(id,"ragdollReturn");m_ragdollReturns.erase(id);}
    return true;
}
bool RuntimeWorld::SetRagdollEnabled(EntityId id,bool enabled,std::string& error){
    const auto* d=RuntimeDefinition(id);if(!d||!d->ragdoll){error="no ragdoll component";return false;}
    if(!enabled&&!LeaveRagdoll(id,.2f,error))return false;
    m_scriptDefinitions.at(id).ragdoll->enabled=enabled;return true;
}
bool RuntimeWorld::SetRagdollContactEvents(EntityId id,bool enabled){
    const auto* definition=RuntimeDefinition(id);if(!definition||!definition->ragdoll)return false;
    m_scriptDefinitions.at(id).ragdoll->receiveContactEvents=enabled;return true;
}
void RuntimeWorld::UpdateRagdolls(float dt){
    JUDAS_PROFILE_SCOPE("Ragdoll physics to pose");
    std::vector<EntityId> invalid;
    for(auto& [id,instance]:m_ragdolls){
        const auto* definition=RuntimeDefinition(id);auto* animation=RuntimeAnimation(id);
        bool valid=definition&&definition->ragdoll&&definition->ragdoll->enabled&&definition->animation&&definition->animation->enabled&&animation&&animation->asset==instance.asset;
        for(const auto& b:instance.bodies)valid&=RuntimeBody(b.entity).id==b.body.id&&m_physics.IsBodyEnabled(b.body);
        if(!valid){invalid.push_back(id);continue;}
        const auto& skeleton=instance.asset->skeleton;auto reference=instance.reference;
        if(instance.partial)reference=PresentedTransform(id,definition->transform,1);
        const auto& root=instance.bodies.front();auto rootPose=m_physics.GetTransform(root.body);
        auto rootRotation=glm::normalize(rootPose.rotation*glm::inverse(root.orientation));
        auto rootPosition=rootPose.position-rootRotation*root.offset;
        // Translation follows the physical root; the entity reference basis stays
        // fixed. Body rotations become skeletal rotations, never body teleports.
        if(!instance.partial){reference.position=rootPosition-reference.rotation*(reference.scale*instance.rootLocalPosition);
            if(instance.physicalControlled){
                if(dt>0)instance.previousPresentation=instance.presentationReady?instance.currentPresentation:PresentedTransform(id,definition->transform,1);
                instance.currentPresentation=reference;
                if(!instance.presentationReady){instance.previousPresentation=reference;instance.presentationReady=true;}
            }
            SetRuntimeTransform(id,reference);}
        auto inverseModel=glm::inverse(Matrix(reference));auto pose=instance.physicalControlled?DriveReference(*animation):animation->finalPose;
        const std::vector<int>* regions=nullptr;
        if(instance.physicalControlled){auto state=m_physicalAnimations.find(id);
            if(state==m_physicalAnimations.end()||!CachedRegionMapping(*definition->ragdoll,animation->asset,instance,state->second,animation->error)){invalid.push_back(id);continue;}
            regions=&state->second.mappedRegions;}
        std::vector<glm::mat4> globals(skeleton.parents.size());std::vector<int> mask;
        for(int node:skeleton.order){
            auto mapped=std::find_if(instance.bodies.begin(),instance.bodies.end(),[&](const auto& b){return b.node==node;});
            const auto mappedIndex=size_t(mapped-instance.bodies.begin());
            if(mapped!=instance.bodies.end()&&(!instance.partial||instance.dynamic[mappedIndex])){
                auto body=m_physics.GetTransform(mapped->body);auto rotation=glm::normalize(body.rotation*glm::inverse(mapped->orientation));auto position=body.position-rotation*mapped->offset;
                auto global=inverseModel*glm::translate(glm::mat4(1),position)*glm::mat4_cast(rotation)*glm::scale(glm::mat4(1),mapped->scale);
                auto local=skeleton.parents[node]<0?global:glm::inverse(globals[skeleton.parents[node]])*global;std::string error;
                if(!skeleton.affine.empty())local=glm::inverse(skeleton.affine.at(node))*local;
                JointTransform actual;if(!DecomposeRigidPose(local,actual,error)){animation->error=error;valid=false;break;}
                globals[node]=global;mask.push_back(node);
                float weight=1;if(regions&&mappedIndex<regions->size()&&regions->at(mappedIndex)>=0)weight=definition->ragdoll->physicalAnimation->regions[size_t(regions->at(mappedIndex))].poseWeight;
                const auto& referenceLocal=pose.local[node];actual.translation=glm::mix(referenceLocal.translation,actual.translation,weight);actual.scale=glm::mix(referenceLocal.scale,actual.scale,weight);actual.rotation=glm::normalize(glm::slerp(referenceLocal.rotation,actual.rotation,weight));pose.local[node]=actual;
            }else {const auto& t=pose.local[node];auto local=glm::translate(glm::mat4(1),t.translation)*glm::mat4_cast(t.rotation)*glm::scale(glm::mat4(1),t.scale);if(!skeleton.affine.empty())local=skeleton.affine.at(node)*local;globals[node]=skeleton.parents[node]<0?local:globals[skeleton.parents[node]]*local;}
        }
        if(!valid){invalid.push_back(id);continue;}
        PoseContribution contribution;contribution.pose=std::move(pose);contribution.mask=std::move(mask);contribution.order=1000;std::string error;
        SetPoseContribution(id,"ragdollPhysics",contribution,error);
        animation->recentWorld=PoseGlobalMatrices(skeleton,animation->finalPose);for(auto& m:animation->recentWorld)m=Matrix(reference)*m;
    }
    for(auto id:invalid){std::string error;LeaveRagdoll(id,.2f,error);}
    for(auto it=m_ragdollReturns.begin();it!=m_ragdollReturns.end();){auto* animation=RuntimeAnimation(it->first);
        if(!animation||!animation->asset){it=m_ragdollReturns.erase(it);continue;}
        it->second.elapsed+=dt;float weight=1-std::clamp(it->second.elapsed/it->second.duration,0.f,1.f);
        if(weight<=0){animation->external.erase("ragdollReturn");ResolveAnimationPose(*animation,0);it=m_ragdollReturns.erase(it);}
        else {auto found=animation->external.find("ragdollReturn");if(found!=animation->external.end())found->second.weight=weight;ResolveAnimationPose(*animation,0);++it;}
    }
    if(dt>0)for(auto id:m_animationOwners){const auto* d=RuntimeDefinition(id);if(d&&d->ragdoll&&d->ragdoll->enabled&&d->ragdoll->playOnStart&&!m_ragdollAutostarted.count(id)){
        auto* a=RuntimeAnimation(id);if(a&&a->asset){m_ragdollAutostarted.insert(id);std::string error;if(!EnterRagdoll(id,error))a->error=error;}
    }}
}
