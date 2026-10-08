#include "Ragdoll.h"
#include <glm/gtc/matrix_transform.hpp>
#include <set>
#include <cmath>
bool ValidRagdollDefinition(const RagdollDefinition& d,std::string& error){
    if(d.physicalAnimation&&!ValidPhysicalAnimationSettings(*d.physicalAnimation,error))return false;
    if(d.bones.empty()||d.bones.size()>32){error="ragdoll needs 1..32 mapped bones";return false;}
    std::set<std::string> keys;unsigned roots=0;
    for(const auto& b:d.bones){
        if(b.joint.empty()||!keys.insert(b.joint).second||(!b.parent.empty()&&!keys.count(b.parent))||b.parent==b.joint){error="ragdoll joints must be unique, parent-first keys";return false;}
        roots+=b.parent.empty();
        auto finite=[](glm::vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
        if(int(b.shape)<0||int(b.shape)>1||!finite(b.offset)||!finite(b.halfExtents)||glm::any(glm::lessThanEqual(b.halfExtents,glm::vec3(0)))||
           !std::isfinite(glm::dot(b.orientation,b.orientation))||glm::dot(b.orientation,b.orientation)<1e-12f||
           !std::isfinite(b.radius)||b.radius<=0||!std::isfinite(b.mass)||b.mass<=0||!std::isfinite(1/b.mass)||
           !std::isfinite(b.friction)||b.friction<0||!std::isfinite(b.restitution)||b.restitution<0||b.restitution>1||b.collisionLayer>=64||
           !ValidJointSettings(b.constraint)||b.constraint.motor||b.constraint.spring){error="invalid ragdoll geometry/material/passive constraint";return false;}
    }
    if(roots!=1){error="ragdoll needs exactly one physical root";return false;}
    return true;
}
bool RagdollDefinitionsEqual(const RagdollDefinition& a,const RagdollDefinition& b){
    if(bool(a.physicalAnimation)!=bool(b.physicalAnimation)||(a.physicalAnimation&&!PhysicalAnimationSettingsEqual(*a.physicalAnimation,*b.physicalAnimation)))return false;
    if(a.enabled!=b.enabled||a.playOnStart!=b.playOnStart||a.selfCollision!=b.selfCollision||a.receiveContactEvents!=b.receiveContactEvents||a.bones.size()!=b.bones.size())return false;
    for(size_t i=0;i<a.bones.size();++i){const auto& x=a.bones[i];const auto& y=b.bones[i];
        if(x.joint!=y.joint||x.parent!=y.parent||x.shape!=y.shape||x.offset!=y.offset||x.halfExtents!=y.halfExtents||x.orientation!=y.orientation||x.radius!=y.radius||x.mass!=y.mass||x.friction!=y.friction||x.restitution!=y.restitution||x.collisionLayer!=y.collisionLayer||x.collisionMask!=y.collisionMask||x.suppressParentCollision!=y.suppressParentCollision||x.autoAnchors!=y.autoAnchors||!JointSettingsEqual(x.constraint,y.constraint))return false;
    }return true;
}
std::vector<glm::mat4> PoseGlobalMatrices(const Skeleton& s,const SkeletalPose& pose){
    return ResolveJointMatrices(s,pose);
}
bool DecomposeRigidPose(const glm::mat4& m,JointTransform& p,std::string& error){
    p.translation=glm::vec3(m[3]);glm::mat3 basis(m);
    for(int i=0;i<3;++i){p.scale[i]=glm::length(basis[i]);if(!std::isfinite(p.scale[i])||p.scale[i]<1e-6f){error="singular rigid mapping";return false;}basis[i]/=p.scale[i];}
    if(std::abs(glm::dot(basis[0],basis[1]))>1e-4f||std::abs(glm::dot(basis[0],basis[2]))>1e-4f||std::abs(glm::dot(basis[1],basis[2]))>1e-4f||glm::determinant(basis)<0){error="ragdoll mapping does not support shear/reflection";return false;}
    p.rotation=glm::normalize(glm::quat_cast(basis));return true;
}
