#pragma once
#include "PhysicsHandles.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
enum class JointType {Fixed,Hinge,Ball,Slider};
struct JointSettings {
    JointType type=JointType::Fixed;
    BodyHandle bodyA,bodyB; // invalid B means a world anchor, never invalid A
    glm::vec3 anchorA{0},anchorB{0};
    glm::quat frameA{1,0,0,0},frameB{1,0,0,0}; // X is hinge/slider axis; Y is angle reference
    bool enabled=true,limits=false,motor=false,spring=false;
    float lower=-1,upper=1,speed=0,maxForce=10,rest=0,stiffness=10,damping=1;
    float rotationalResistance=0; // Viscous torque coefficient, N m s/rad, on free angular axes.
};
// Opt-in joint actuator, solved alongside ordinary contacts/constraints. Target
// is B's orientation relative to A; effort is a magnitude bound in N m.
// Runtime controller state is deliberately separate from authored M45 settings.
struct JointAngularDrive {
    bool enabled=false;
    glm::quat target{1,0,0,0};
    float stiffness=0,damping=0,maxTorque=0;
    glm::vec3 relativeAngularVelocity{0}; // B-A target angular rate in world rad/s
    glm::vec3 impulses{0}; // disposable warm history, N m s
};
struct JointState {JointSettings settings;bool active=false;float coordinate=0,motorImpulse=0;glm::vec3 reactionImpulse{0},reactionAngularImpulse{0};JointAngularDrive angularDrive;};
inline bool ValidJointSettings(const JointSettings& s){
 auto finite=[](glm::vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
 auto quat=[&](glm::quat q){return finite({q.x,q.y,q.z})&&std::isfinite(q.w)&&glm::dot(q,q)>1e-12f&&std::isfinite(glm::dot(q,q));};
 return int(s.type)>=0&&int(s.type)<=3&&finite(s.anchorA)&&finite(s.anchorB)&&quat(s.frameA)&&quat(s.frameB)&&
 std::isfinite(s.lower)&&std::isfinite(s.upper)&&s.lower<=s.upper&&std::isfinite(s.speed)&&std::isfinite(s.maxForce)&&s.maxForce>=0&&
 std::isfinite(s.rest)&&std::isfinite(s.stiffness)&&s.stiffness>=0&&std::isfinite(s.damping)&&s.damping>=0&&std::isfinite(s.rotationalResistance)&&s.rotationalResistance>=0&&
 (s.type!=JointType::Hinge||!s.limits||(s.lower>=-3.1415927f&&s.upper<=3.1415927f));
}

inline bool JointSettingsEqual(const JointSettings& a,const JointSettings& b){return a.type==b.type&&a.bodyA.id==b.bodyA.id&&a.bodyB.id==b.bodyB.id&&a.anchorA==b.anchorA&&a.anchorB==b.anchorB&&a.frameA==b.frameA&&a.frameB==b.frameB&&a.enabled==b.enabled&&a.limits==b.limits&&a.motor==b.motor&&a.spring==b.spring&&a.lower==b.lower&&a.upper==b.upper&&a.speed==b.speed&&a.maxForce==b.maxForce&&a.rest==b.rest&&a.stiffness==b.stiffness&&a.damping==b.damping&&a.rotationalResistance==b.rotationalResistance;}
