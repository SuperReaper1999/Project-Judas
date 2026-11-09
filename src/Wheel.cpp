#include "Wheel.h"
#include "SaveArchive.h"
#include "PerformanceProfiler.h"
#include "vehicles/WheelContact.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
bool Finite(glm::vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
glm::quat MountRotation(const WheelSettings& s,const BodyTransform& body,const WheelCommand& command) {
    return glm::normalize(body.rotation*s.orientation*glm::angleAxis(command.steeringAngle,glm::vec3(0,1,0)));
}
}

bool ValidateWheelSettings(const WheelSettings& s,std::string& error) {
    const float values[]={s.radius,s.width,s.minLength,s.restLength,s.maxLength,s.springStiffness,s.damping,
        s.reboundRatio,s.longitudinalStiffness,s.lateralStiffness,
        s.spinInertia,s.friction,s.rollingResistance,s.orientation.w,s.orientation.x,s.orientation.y,s.orientation.z};
    for(float x:values)if(!std::isfinite(x)){error="nonfinite wheel setting";return false;}
    const double norm=glm::dot(glm::dquat(s.orientation),glm::dquat(s.orientation));
    if(s.part.size()>128||!s.chassis||!Finite(s.mount)||!std::isfinite(glm::length(s.mount))||norm<1e-12||norm>1e12||s.radius<=0||
       s.width<=0||s.reboundRatio<0||s.longitudinalStiffness<=0||s.lateralStiffness<=0||
       s.minLength<0||s.restLength<=s.minLength||s.maxLength<s.restLength||
       s.springStiffness<=0||s.damping<0||s.spinInertia<=1e-8f||s.friction<0||
       s.rollingResistance<0||s.collisionLayer>=64) {
        error="invalid wheel chassis, dimensions, spring, inertia or filter";return false;
    }
    return true;
}
bool ValidateWheelCommand(const WheelCommand& c,std::string& error) {
    if(!std::isfinite(c.driveTorque)||!std::isfinite(c.brakeTorque)||!std::isfinite(c.steeringAngle)||c.brakeTorque<0){
        error="invalid wheel torque, brake or steering command";return false;
    }
    return true;
}

void WheelInstance::Step(PhysicsWorld& world,BodyHandle chassis,float dt) {
    previous=state;
    PrepareInterval(world,chassis,dt);
    for(unsigned iteration=0;iteration<8;++iteration)SolveIteration(world);
    FinishInterval(world);
}

void WheelInstance::Publish(PhysicsWorld& world,BodyHandle chassis) {
    std::string error;
    state.available=world.IsBodyEnabled(chassis)&&world.IsDynamicBody(chassis)&&ValidateWheelSettings(settings,error);
    if(!state.available)return;
    state.sleeping=world.IsSleeping(chassis);
    const auto transform=world.GetTransform(chassis);
    const auto base=glm::normalize(transform.rotation*settings.orientation);
    state.center=transform.position+transform.rotation*settings.mount+base*glm::vec3(0,-state.length,0);
    state.rotation=glm::normalize(MountRotation(settings,transform,command)*
        glm::angleAxis(-state.angle,glm::vec3(1,0,0)));
}

void WheelInstance::Persist(SaveArchive& archive) {
    archive(command.driveTorque,command.brakeTorque,command.steeringAngle,
        state.angularVelocity,state.angle,state.length,state.normalForce);
    std::string error;archive.Require(ValidateWheelCommand(command,error),error);
    archive.Require(state.length>=0&&state.length<=settings.maxLength+1e-5f&&state.normalForce>=0,"invalid saved wheel travel/load");
    if(archive.reading){state.available=false;state.contact=false;state.body={};previous=state;}
}
