// Get-up from lying, for stand and recover tasks whose ordinary stand cannot
// lift the root. Driving every joint straight to the standing pose works only
// for a rig whose feet outweigh its trunk's lever (the feet then stay put while
// the hips lift the trunk); a top-heavy rig with light feet just thrashes on
// the floor. Instead both chains play a short staged motion about the rig's
// lateral axis, chosen by which way it faces: plant the feet flat, gather them
// under the hips, then extend hard enough to carry the centre of mass over
// them. The ordinary stand takes over once the root is upright on both feet,
// ramping from the pose the rise reached. A rise that falls back is retried
// from a fresh assessment. Only joint drives act: no body is moved or held.
#include "PhysicalBalance.h"
#include <cmath>

namespace {
constexpr float kDegrees=3.14159265358979f/180;
constexpr float kHandoverUpright=.8f;  // root upright to hand over to the stand
constexpr float kRetrySeconds=1;       // after the rise, before a fresh attempt
}

bool PhysicalBalanceController::GetUp(PhysicsWorld& world,const glm::vec3& up,float dt){
    if(!m_getUp){
        // The ordinary stand goes first: rigs whose feet anchor them rise on
        // it directly. Only a stand that has not lifted the root past
        // getUpUpright within getUpPatience switches to the staged motion; a
        // failed rise is assessed again at once.
        if(m_observation.upright>=m_settings.getUpUpright)m_getUpReached=true;
        if(m_history.progress!=BalanceProgress::Executing||m_history.hasStood||m_getUpReached)return false;
        if(!m_getUpAttempts&&m_history.elapsed<m_settings.getUpPatience)return false;
        if(m_getUpAttempts>=m_settings.getUpAttempts){Fail(world,"no physical get-up after the authored attempts");return true;}
        m_getUp=true;m_getUpTime=0;++m_getUpAttempts;
        m_getUpFaceUp=glm::dot(world.GetTransform(m_bodies[0].body).rotation*m_settings.forward,up)>0;
    }
    m_getUpTime+=dt;
    const auto& keys=m_getUpFaceUp?m_settings.getUpFaceUp:m_settings.getUpFaceDown;
    BalanceGetUpKey pose;float end=0;bool rising=false;
    for(const auto& key:keys){end+=key.seconds;if(m_getUpTime<=end){pose=key;rising=true;break;}}
    if(!rising){
        // The rise is over: rest pose until the stand can take over.
        if(m_observation.upright>kHandoverUpright&&m_observation.supports[0].contacts&&m_observation.supports[1].contacts){
            m_getUp=false;m_history.frameReady=false;m_history.referenceTime=0;
            for(size_t i=1;i<m_bodies.size();++i)m_history.initialTargets[i]=glm::inverse(world.GetTransform(m_bodies[i].body).rotation)*world.GetTransform(m_bodies[m_bodies[i].parent].body).rotation;
            return false;
        }
        if(m_getUpTime>end+kRetrySeconds)m_getUp=false; // assess again next step
    }
    const glm::vec3 lateral=glm::normalize(glm::cross(m_settings.up,m_settings.forward));
    for(size_t i=1;i<m_bodies.size();++i){
        float angle=0;
        for(const auto& e:m_settings.effectors)angle=i==e.upper?pose.hip:i==e.lower?pose.knee:i==e.foot?pose.ankle:angle;
        const auto& body=m_bodies[i];const auto& parent=m_bodies[body.parent];
        const auto relative=glm::inverse(parent.restRotation)*body.restRotation;
        const auto bend=glm::angleAxis(-angle*kDegrees,glm::inverse(body.restRotation)*lateral);
        JointAngularDrive drive;drive.enabled=m_history.effort;drive.stiffness=m_settings.stiffness;drive.damping=m_settings.damping;drive.maxTorque=m_settings.maxTorque;
        drive.target=glm::inverse(relative*bend);
        if(!world.SetJointAngularDrive(body.joint,drive)){Fail(world,"bounded joint drive rejected");return true;}
    }
    return true;
}
