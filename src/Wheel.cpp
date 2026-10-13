#include "Wheel.h"
#include "SaveArchive.h"
#include "PerformanceProfiler.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
bool Finite(glm::vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
glm::mat3 InverseInertia(PhysicsWorld& world,BodyHandle body) {
    return world.IsDynamicBody(body)?glm::inverse(world.GetInertiaWorld(body)):glm::mat3(0);
}
float InverseMass(PhysicsWorld& world,BodyHandle body) {
    const float mass=world.GetMass(body);return mass>0?1/mass:0;
}
struct Row {
    glm::vec3 direction, chassisAngular, groundAngular;
    float spin=0, inverseMass=0;
};
Row MakeRow(PhysicsWorld& world,BodyHandle chassis,BodyHandle ground,
            glm::vec3 center,glm::vec3 point,glm::vec3 axis,glm::vec3 direction,float spinInertia) {
    Row row;row.direction=direction;
    // The axle transmits the non-spinning part of contact torque. Axial
    // torque changes the independent wheel DOF instead. This distinction
    // avoids counting tyre torque once in the chassis and again in the wheel.
    row.spin=glm::dot(axis,glm::cross(point-center,direction));
    row.chassisAngular=glm::cross(point-world.GetCenterOfMass(chassis),direction)-axis*row.spin;
    row.groundAngular=glm::cross(point-world.GetCenterOfMass(ground),direction);
    row.inverseMass=InverseMass(world,chassis)+InverseMass(world,ground)+
        glm::dot(row.chassisAngular,InverseInertia(world,chassis)*row.chassisAngular)+
        glm::dot(row.groundAngular,InverseInertia(world,ground)*row.groundAngular)+row.spin*row.spin/spinInertia;
    return row;
}
float Speed(PhysicsWorld& world,BodyHandle chassis,BodyHandle ground,
            glm::vec3 point,glm::vec3 axis,float spin,const Row& row,float dt) {
    return glm::dot(world.GetStepPointVelocity(chassis,point,dt)-world.GetStepPointVelocity(ground,point,dt),row.direction)+
        (spin-glm::dot(world.GetAngularVelocity(chassis),axis))*row.spin;
}
void Impulse(PhysicsWorld& world,BodyHandle chassis,BodyHandle ground,
             glm::vec3 axis,const Row& row,float impulse,float inertia,float& spin) {
    if(!std::isfinite(impulse)||std::abs(impulse)<1e-8f)return;
    // An already-awake body need not restart its quiet clock for a balanced
    // support impulse. Sleeping ground wakes before receiving new work.
    world.ApplyLinearImpulse(chassis,row.direction*impulse,false);
    world.ApplyAngularImpulse(chassis,row.chassisAngular*impulse,false);
    const bool wakeGround=world.IsSleeping(ground);
    world.ApplyLinearImpulse(ground,-row.direction*impulse,wakeGround);
    world.ApplyAngularImpulse(ground,-row.groundAngular*impulse,wakeGround);
    spin+=row.spin*impulse/inertia;
    (void)axis;
}
glm::quat MountRotation(const WheelSettings& s,const BodyTransform& body,const WheelCommand& command) {
    return glm::normalize(body.rotation*s.orientation*glm::angleAxis(command.steeringAngle,glm::vec3(0,1,0)));
}
}

bool ValidateWheelSettings(const WheelSettings& s,std::string& error) {
    const float values[]={s.radius,s.minLength,s.restLength,s.maxLength,s.springStiffness,s.damping,
        s.spinInertia,s.friction,s.rollingResistance,s.orientation.w,s.orientation.x,s.orientation.y,s.orientation.z};
    for(float x:values)if(!std::isfinite(x)){error="nonfinite wheel setting";return false;}
    const double norm=glm::dot(glm::dquat(s.orientation),glm::dquat(s.orientation));
    if(!s.chassis||!Finite(s.mount)||!std::isfinite(glm::length(s.mount))||norm<1e-12||norm>1e12||s.radius<=0||
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
    JUDAS_PROFILE_SCOPE("Wheel contact/suspension");
    previous=state;
    const float spin=state.angularVelocity,angle=state.angle;
    state={};state.angularVelocity=spin;state.angle=angle;state.length=settings.maxLength;
    std::string error;
    if(!std::isfinite(dt)||dt<=0||!ValidateWheelSettings(settings,error)||
       !ValidateWheelCommand(command,error)||!world.IsBodyEnabled(chassis)||!world.IsDynamicBody(chassis))return;
    state.available=true;
    if(!settings.enabled){Publish(world,chassis);return;}
    const auto transform=world.GetTransform(chassis);
    const auto mountRotation=MountRotation(settings,transform,command);
    const auto suspensionRotation=glm::normalize(transform.rotation*settings.orientation);
    const glm::vec3 down=suspensionRotation*glm::vec3(0,-1,0);
    const glm::vec3 axis=mountRotation*glm::vec3(-1,0,0);
    const glm::vec3 anchor=transform.position+transform.rotation*settings.mount;

    PhysicsQueryFilter filter;filter.includeLayers=settings.collisionMask;
    filter.requiredTags=settings.requiredTags;filter.excludedTags=settings.excludedTags;
    filter.ignoredBodies.push_back(chassis);
    // Cast policy also respects the other collider's ordinary M39 mask. The
    // same broadphase finds those candidates; no private wheel collision tree.
    const auto endpoint=anchor+down*settings.maxLength;
    world.QueryBodiesInAabbInto(glm::min(anchor,endpoint)-glm::vec3(settings.radius),
        glm::max(anchor,endpoint)+glm::vec3(settings.radius),candidates,filter);
    for(auto body:candidates){unsigned layer=0;CategoryMask mask=0;
        if(world.GetCollisionFilter(body,layer,mask)&&!(mask&(CategoryMask(1)<<settings.collisionLayer)))filter.ignoredBodies.push_back(body);
    }
    auto hit=world.SphereCast(anchor,settings.radius,down,settings.maxLength,filter);
    const float projection=hit.hit?glm::dot(hit.normal,-down):0;
    if(hit.hit&&projection>1e-4f) {
        state.contact=true;state.body=hit.body;state.point=hit.point;state.normal=hit.normal;
        state.initialOverlap=hit.initialOverlap;
        state.length=hit.distance;
        state.groundVelocity=world.GetStepPointVelocity(hit.body,hit.point,dt);
    }
    state.center=anchor+down*state.length;
    // Sleeping ordinary bodies already defer their gravity kick. Leave a
    // quiet supported assembly asleep, but wake for vanished/moving support
    // or any drive input. This is a physical lifetime decision, not a vehicle mode.
    if(world.IsSleeping(chassis)) {
        if(state.contact&&previous.contact&&state.body.id==previous.body.id&&
           glm::length(state.groundVelocity)<1e-5f&&command.driveTorque==0&&
           std::abs(state.angularVelocity-glm::dot(world.GetAngularVelocity(chassis),axis))<1e-4f) {
            state.normalForce=previous.normalForce;Publish(world,chassis);return;
        }
        world.Wake(chassis);
    }
    // Drive and brake are axle torques with an equal chassis reaction.
    const auto inverseChassis=InverseInertia(world,chassis);
    state.angularVelocity+=command.driveTorque*dt/settings.spinInertia;
    world.ApplyAngularImpulse(chassis,-axis*(command.driveTorque*dt));
    const float relativeSpin=state.angularVelocity-glm::dot(world.GetAngularVelocity(chassis),axis);
    const float resistance=command.brakeTorque+(state.contact?settings.rollingResistance*previous.normalForce*settings.radius:0);
    const float shaftInverse=1/settings.spinInertia+glm::dot(axis,inverseChassis*axis);
    const float brake=std::clamp(-relativeSpin/shaftInverse,-resistance*dt,resistance*dt);
    state.angularVelocity+=brake/settings.spinInertia;
    world.ApplyAngularImpulse(chassis,-axis*brake);

    if(state.contact) {
        auto normal=MakeRow(world,chassis,state.body,state.center,state.point,axis,state.normal,settings.spinInertia);
        const float normalSpeed=Speed(world,chassis,state.body,state.point,axis,state.angularVelocity,normal,dt);
        const double p=projection,stiffness=settings.springStiffness,damping=settings.damping;
        // Backward-Euler compliant contact: x_next=x-dt*v_next/p. The
        // damping and stiffness enter the effective mass, so a stiff spring
        // does not need hidden substeps or a cadence reduction to stay bounded.
        const double denominator=1+normal.inverseMass*(dt*damping+dt*dt*stiffness)/(p*p);
        float normalImpulse=float(std::max(0.0,dt*(stiffness*(settings.restLength-state.length)-
            damping*normalSpeed/p)/(p*denominator)));
        // Minimum travel is a unilateral bump stop. An initial-overlap cast
        // supplies no depth, so never invent a positional correction for it.
        if(!hit.initialOverlap&&state.length<settings.minLength&&normal.inverseMass>0){
            const float target=.2f*(settings.minLength-state.length)*projection/dt;
            normalImpulse=std::max(normalImpulse,std::max(0.f,(target-normalSpeed)/normal.inverseMass));
        }
        Impulse(world,chassis,state.body,axis,normal,normalImpulse,settings.spinInertia,state.angularVelocity);
        state.normalForce=normalImpulse/dt;

        const glm::vec3 tangent=glm::cross(axis,state.normal);
        if(glm::length(tangent)>1e-5f) {
            const auto forward=glm::normalize(tangent),lateral=glm::normalize(glm::cross(state.normal,forward));
            const auto longitudinal=MakeRow(world,chassis,state.body,state.center,state.point,axis,forward,settings.spinInertia);
            const auto transverse=MakeRow(world,chassis,state.body,state.center,state.point,axis,lateral,settings.spinInertia);
            state.longitudinalSlip=Speed(world,chassis,state.body,state.point,axis,state.angularVelocity,longitudinal,dt);
            state.lateralSlip=Speed(world,chassis,state.body,state.point,axis,state.angularVelocity,transverse,dt);
            // Solve the 2x2 tangent effective mass, then project onto the
            // Coulomb disk. Saturation intentionally permits tyre slip.
            const double crossMass=glm::dot(longitudinal.chassisAngular,inverseChassis*transverse.chassisAngular)+
                glm::dot(longitudinal.groundAngular,InverseInertia(world,state.body)*transverse.groundAngular)+
                longitudinal.spin*transverse.spin/settings.spinInertia;
            const double determinant=double(longitudinal.inverseMass)*transverse.inverseMass-crossMass*crossMass;
            double a=0,b=0;
            if(determinant>std::numeric_limits<double>::epsilon()*
                double(longitudinal.inverseMass)*transverse.inverseMass){
                a=(-state.longitudinalSlip*transverse.inverseMass+crossMass*state.lateralSlip)/determinant;
                b=(-state.lateralSlip*longitudinal.inverseMass+crossMass*state.longitudinalSlip)/determinant;
            }
            std::string material;float groundFriction=0,restitution=0;
            world.GetPhysicalMaterial(state.body,material,groundFriction,restitution);
            const double limit=std::sqrt(double(settings.friction)*std::max(0.f,groundFriction))*normalImpulse;
            const double length=std::hypot(a,b);if(length>limit&&length>0){a*=limit/length;b*=limit/length;}
            state.longitudinalImpulse=float(a);state.lateralImpulse=float(b);
            Impulse(world,chassis,state.body,axis,longitudinal,float(a),settings.spinInertia,state.angularVelocity);
            Impulse(world,chassis,state.body,axis,transverse,float(b),settings.spinInertia,state.angularVelocity);
        }
    }
    const float relativeEnd=state.angularVelocity-glm::dot(world.GetAngularVelocity(chassis),axis);
    state.angle=std::remainder(state.angle+relativeEnd*dt,glm::two_pi<float>());
    Publish(world,chassis);
}

void WheelInstance::Publish(PhysicsWorld& world,BodyHandle chassis) {
    std::string error;
    state.available=world.IsBodyEnabled(chassis)&&world.IsDynamicBody(chassis)&&ValidateWheelSettings(settings,error);
    if(!state.available)return;
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
