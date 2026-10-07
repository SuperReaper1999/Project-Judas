#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace {
glm::mat4 Matrix(const SceneTransform& t){return glm::translate(glm::mat4(1),t.position)*glm::mat4_cast(t.rotation)*glm::scale(glm::mat4(1),t.scale);}
bool UniformPositive(glm::vec3 s){return s.x>0&&std::abs(s.x-s.y)<1e-4f&&std::abs(s.x-s.z)<1e-4f;}
glm::vec3 AngularVelocity(glm::quat previous,glm::quat current,float dt){
    auto delta=glm::normalize(current*glm::inverse(previous));if(delta.w<0)delta=-delta;
    float length=glm::length(glm::vec3(delta.x,delta.y,delta.z));
    return length>1e-6f?glm::vec3(delta.x,delta.y,delta.z)*(2*std::atan2(length,delta.w)/(length*dt)):glm::vec3(0);
}
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
    std::vector<JointTransform> jointWorld;std::vector<int> nodes;
    for(const auto& b:settings.bones){
        int node=FindSkeletonJoint(skeleton,b.joint);
        if(node<0||std::find(nodes.begin(),nodes.end(),node)!=nodes.end()){error="missing/ambiguous/duplicate mapped skeleton joint: "+b.joint;return false;}
        JointTransform world;if(!DecomposeRigidPose(model*globals[node],world,error)||!UniformPositive(world.scale)){error="ragdoll requires non-sheared, uniform joint scale at activation";return false;}
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
        if(animation->previousWorld.size()==skeleton.parents.size()&&animation->recentWorld.size()==skeleton.parents.size()&&animation->motionDt>0){
            JointTransform previous,current;std::string ignored;
            if(DecomposeRigidPose(animation->previousWorld[mapped.node],previous,ignored)&&DecomposeRigidPose(animation->recentWorld[mapped.node],current,ignored)){
                auto oldCenter=previous.translation+previous.rotation*(b.offset*previous.scale.x);
                auto newCenter=current.translation+current.rotation*mapped.offset;
                state.linearVelocity=(newCenter-oldCenter)/animation->motionDt;state.angularVelocity=AngularVelocity(previous.rotation,current.rotation,animation->motionDt);
            }
        }else {EntityPhysicalState inherited;if(GetEntityState(id,inherited)){state.linearVelocity=inherited.linearVelocity;state.angularVelocity=inherited.angularVelocity;}}
        SceneObject body;body.name="Articulation: "+b.joint;body.body=SceneBodyComponent{};auto& component=*body.body;
        component.motion=SceneBodyMotion::Dynamic;component.shape=b.shape==RagdollShape::Box?SceneShape::Box:SceneShape::Sphere;
        component.halfExtents=b.halfExtents*w.scale.x;component.radius=b.radius*w.scale.x;component.mass=b.mass;component.friction=b.friction;component.restitution=b.restitution;component.collisionLayer=b.collisionLayer;component.collisionMask=b.collisionMask;
        body.tags=definition->tags; // identity/filter consumers see normal mapped entities
        mapped.entity=CreateEntity(body,&state,&error);if(!mapped.entity){rollback();return false;}
        mapped.body=RuntimeBody(mapped.entity);auto* record=FindEntity(mapped.entity);record->transient=true;record->requiresFull=true;
    }
    instance.rootLocalPosition=glm::vec3(globals[instance.bodies.front().node][3]);
    for(size_t i=0;i<instance.bodies.size();++i){auto& b=instance.bodies[i];if(b.parent<0)continue;
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
void RuntimeWorld::UpdateRagdolls(float dt){
    JUDAS_PROFILE_SCOPE("Ragdoll physics to pose");
    std::vector<EntityId> invalid;
    for(auto& [id,instance]:m_ragdolls){
        const auto* definition=RuntimeDefinition(id);auto* animation=RuntimeAnimation(id);
        bool valid=definition&&definition->ragdoll&&definition->ragdoll->enabled&&definition->animation&&definition->animation->enabled&&animation&&animation->asset==instance.asset;
        for(const auto& b:instance.bodies)valid&=RuntimeBody(b.entity).id==b.body.id&&m_physics.IsBodyEnabled(b.body);
        if(!valid){invalid.push_back(id);continue;}
        const auto& skeleton=instance.asset->skeleton;auto reference=instance.reference;
        const auto& root=instance.bodies.front();auto rootPose=m_physics.GetTransform(root.body);
        auto rootRotation=glm::normalize(rootPose.rotation*glm::inverse(root.orientation));
        auto rootPosition=rootPose.position-rootRotation*root.offset;
        // Translation follows the physical root; the entity reference basis stays
        // fixed. Body rotations become skeletal rotations, never body teleports.
        reference.position=rootPosition-reference.rotation*(reference.scale*instance.rootLocalPosition);
        SetRuntimeTransform(id,reference);
        auto inverseModel=glm::inverse(Matrix(reference));auto pose=animation->finalPose;
        std::vector<glm::mat4> globals(skeleton.parents.size());std::vector<int> mask;
        for(int node:skeleton.order){
            auto mapped=std::find_if(instance.bodies.begin(),instance.bodies.end(),[&](const auto& b){return b.node==node;});
            if(mapped!=instance.bodies.end()){
                auto body=m_physics.GetTransform(mapped->body);auto rotation=glm::normalize(body.rotation*glm::inverse(mapped->orientation));auto position=body.position-rotation*mapped->offset;
                auto global=inverseModel*glm::translate(glm::mat4(1),position)*glm::mat4_cast(rotation)*glm::scale(glm::mat4(1),mapped->scale);
                auto local=skeleton.parents[node]<0?global:glm::inverse(globals[skeleton.parents[node]])*global;std::string error;
                if(!skeleton.affine.empty())local=glm::inverse(skeleton.affine.at(node))*local;
                if(!DecomposeRigidPose(local,pose.local[node],error)){animation->error=error;valid=false;break;}globals[node]=global;mask.push_back(node);
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
