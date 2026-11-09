#include "Wheel.h"
#include "WheelContact.h"
#include "PerformanceProfiler.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace {
glm::mat3 InverseInertia(PhysicsWorld& world, BodyHandle body) {
    return world.IsDynamicBody(body) ? glm::inverse(world.GetInertiaWorld(body)) : glm::mat3(0);
}
float InverseMass(PhysicsWorld& world, BodyHandle body) {
    const float mass=world.GetMass(body);
    return mass>0 ? 1/mass : 0;
}
JudasVehicles::WheelRow MakeRow(PhysicsWorld& world,BodyHandle chassis,BodyHandle ground,
                               glm::vec3 center,glm::vec3 point,glm::vec3 axle,
                               glm::vec3 direction,float inertia) {
    JudasVehicles::WheelRow row;
    row.direction=direction;
    row.spin=glm::dot(axle,glm::cross(point-center,direction));
    row.chassisAngular=glm::cross(point-world.GetCenterOfMass(chassis),direction)-axle*row.spin;
    row.groundAngular=glm::cross(point-world.GetCenterOfMass(ground),direction);
    row.inverseMass=InverseMass(world,chassis)+InverseMass(world,ground)+
        glm::dot(row.chassisAngular,InverseInertia(world,chassis)*row.chassisAngular)+
        glm::dot(row.groundAngular,InverseInertia(world,ground)*row.groundAngular)+row.spin*row.spin/inertia;
    return row;
}
float Speed(PhysicsWorld& world,const WheelInstance& wheel,const JudasVehicles::WheelRow& row,
            const JudasVehicles::WheelConstraintState& solve) {
    return glm::dot(world.GetStepPointVelocity(solve.chassis,wheel.state.point,solve.dt)-
                    world.GetStepPointVelocity(wheel.state.body,wheel.state.point,solve.dt),row.direction)+
        (wheel.state.angularVelocity-glm::dot(world.GetAngularVelocity(solve.chassis),solve.axle))*row.spin;
}
void Apply(PhysicsWorld& world,WheelInstance& wheel,const JudasVehicles::WheelRow& row,
           const JudasVehicles::WheelConstraintState& solve,float impulse) {
    if(impulse==0)return;
    world.ApplyLinearImpulse(solve.chassis,row.direction*impulse,false);
    world.ApplyAngularImpulse(solve.chassis,row.chassisAngular*impulse,false);
    const bool wake=world.IsSleeping(wheel.state.body);
    world.ApplyLinearImpulse(wheel.state.body,-row.direction*impulse,wake);
    world.ApplyAngularImpulse(wheel.state.body,-row.groundAngular*impulse,wake);
    wheel.state.angularVelocity+=row.spin*impulse/wheel.settings.spinInertia;
}
}

void WheelInstance::PrepareInterval(PhysicsWorld& world,BodyHandle chassis,float dt) {
    JUDAS_PROFILE_SCOPE("Vehicle wheel preparation");
    solve={};
    const auto old=state;
    state={};
    state.angularVelocity=old.angularVelocity;
    state.angle=old.angle;
    state.length=settings.maxLength;
    std::string error;
    if(!std::isfinite(dt)||dt<=0||!ValidateWheelSettings(settings,error)||
       !ValidateWheelCommand(command,error)||!world.IsBodyEnabled(chassis)||
       !world.IsDynamicBody(chassis)) {
        state.diagnostic=error.empty()?"wheel requires an enabled dynamic chassis":error;
        return;
    }
    state.available=true;
    if(!settings.enabled)return;
    solve.active=true;
    solve.chassis=chassis;
    solve.dt=dt;
    const auto transform=world.GetTransform(chassis);
    const auto frame=glm::normalize(transform.rotation*settings.orientation);
    const auto down=frame*glm::vec3(0,-1,0);
    solve.axle=glm::normalize(frame*glm::angleAxis(command.steeringAngle,glm::vec3(0,1,0)))*glm::vec3(-1,0,0);
    const auto anchor=transform.position+transform.rotation*settings.mount;
    const auto hit=JudasVehicles::FindSupport(world,chassis,settings,anchor,down,solve.axle,candidates,
                                            &contactCache,world.IsSleeping(chassis)&&command.driveTorque==0);
    const float projection=hit.hit ? glm::dot(hit.normal,-down) : 0;
    if(hit.hit&&projection>1e-4f) {
        state.contact=true;state.body=hit.body;state.point=hit.point;state.normal=hit.normal;
        state.initialOverlap=hit.initialOverlap;state.length=hit.distance;
        state.groundVelocity=world.GetStepPointVelocity(hit.body,hit.point,dt);
    }
    state.center=anchor+down*state.length;
    if(world.IsSleeping(chassis)) {
        const bool unchanged=state.contact&&old.contact&&state.body.id==old.body.id&&
            state.body.world==old.body.world&&std::abs(state.length-old.length)<1e-4f&&
            glm::dot(state.normal,old.normal)>.9999f&&glm::length(state.groundVelocity)<1e-4f&&
            command.driveTorque==0&&std::abs(state.angularVelocity-
                glm::dot(world.GetAngularVelocity(chassis),solve.axle))<.03f;
        if(unchanged) {solve.sleeping=true;state.sleeping=true;state.normalForce=old.normalForce;return;}
        world.Wake(chassis);
    }
    const auto inverseChassis=InverseInertia(world,chassis);
    const double drive=double(command.driveTorque)*dt;
    const float nextSpin=float(double(state.angularVelocity)+drive/settings.spinInertia);
    const auto nextChassis=world.GetAngularVelocity(chassis)-inverseChassis*solve.axle*float(drive);
    const auto fail=[&] {
        state.available=false;state.contact=false;state.normalForce=0;solve.active=false;
        state.diagnostic="wheel effort or inertia exceeds physical numeric representation";
    };
    const auto applyDrive=[&] {
        state.angularVelocity=nextSpin;
        world.ApplyAngularImpulse(chassis,-solve.axle*float(drive));
    };
    solve.shaftInverse=1/settings.spinInertia+glm::dot(solve.axle,inverseChassis*solve.axle);
    solve.brakeLimit=float((double(command.brakeTorque)+(state.contact ?
        double(settings.rollingResistance)*old.normalForce*settings.radius : 0))*dt);
    if(!std::isfinite(nextSpin)||!std::isfinite(float(drive))||
       !std::isfinite(glm::length(nextChassis))||!std::isfinite(solve.shaftInverse)||
       !std::isfinite(solve.brakeLimit)){fail();return;}
    if(!state.contact){applyDrive();return;}
    solve.normal=MakeRow(world,chassis,state.body,state.center,state.point,solve.axle,state.normal,settings.spinInertia);
    const double normalSpeed=Speed(world,*this,solve.normal,solve);
    const double damping=double(settings.damping)*(normalSpeed>0?settings.reboundRatio:1);
    const double compliance=double(dt)*(damping+double(dt)*settings.springStiffness);
    solve.gamma=float(double(projection)*projection/compliance);
    solve.bias=float(settings.springStiffness*(settings.restLength-state.length)*projection/
                     (damping+double(dt)*settings.springStiffness));
    solve.stopBias=!state.initialOverlap&&state.length<settings.minLength ?
        .2f*(settings.minLength-state.length)*projection/dt : 0;
    const auto direction=glm::cross(solve.axle,state.normal);
    if(glm::length(direction)>1e-5f) {
        solve.tangent=true;
        const auto forward=glm::normalize(direction);
        const auto lateral=glm::normalize(glm::cross(state.normal,forward));
        solve.longitudinal=MakeRow(world,chassis,state.body,state.center,state.point,solve.axle,forward,settings.spinInertia);
        solve.lateral=MakeRow(world,chassis,state.body,state.center,state.point,solve.axle,lateral,settings.spinInertia);
        solve.tangentCrossMass=glm::dot(solve.longitudinal.chassisAngular,inverseChassis*solve.lateral.chassisAngular)+
            glm::dot(solve.longitudinal.groundAngular,InverseInertia(world,state.body)*solve.lateral.groundAngular)+
            solve.longitudinal.spin*solve.lateral.spin/settings.spinInertia;
        std::string material;float friction=0,restitution=0;
        world.GetPhysicalMaterial(state.body,material,friction,restitution);
        solve.friction=float(std::sqrt(double(settings.friction)*std::max(0.f,friction)));
    }
    for(float value:{solve.gamma,solve.bias,solve.stopBias,solve.normal.inverseMass,
                    solve.longitudinal.inverseMass,solve.lateral.inverseMass,
                    solve.tangentCrossMass,solve.friction})
        if(!std::isfinite(value)){fail();return;}
    applyDrive();
}

void WheelInstance::SolveIteration(PhysicsWorld& world) {
    if(!solve.active||solve.sleeping)return;
    // The accumulated brake capacity belongs to the interval, not each sweep.
    const float speed=state.angularVelocity-glm::dot(world.GetAngularVelocity(solve.chassis),solve.axle);
    const float brake=std::clamp(solve.brakeImpulse-speed/solve.shaftInverse,
                                 -solve.brakeLimit,solve.brakeLimit);
    const float deltaBrake=brake-solve.brakeImpulse;
    solve.brakeImpulse=brake;
    state.angularVelocity+=deltaBrake/settings.spinInertia;
    world.ApplyAngularImpulse(solve.chassis,-solve.axle*deltaBrake,false);
    if(!state.contact)return;
    const float normalSpeed=Speed(world,*this,solve.normal,solve);
    const float normal=std::max(0.f,solve.normalImpulse+
        (solve.bias-normalSpeed-solve.gamma*solve.normalImpulse)/(solve.normal.inverseMass+solve.gamma));
    Apply(world,*this,solve.normal,solve,normal-solve.normalImpulse);
    solve.normalImpulse=normal;
    if(solve.stopBias>0&&solve.normal.inverseMass>0) {
        const float stop=std::max(0.f,solve.stopImpulse+
            (solve.stopBias-Speed(world,*this,solve.normal,solve))/solve.normal.inverseMass);
        Apply(world,*this,solve.normal,solve,stop-solve.stopImpulse);solve.stopImpulse=stop;
    }
    state.normalForce=(solve.normalImpulse+solve.stopImpulse)/solve.dt;
    if(!solve.tangent)return;
    state.longitudinalSlip=Speed(world,*this,solve.longitudinal,solve);
    state.lateralSlip=Speed(world,*this,solve.lateral,solve);
    const float groundSpeed=glm::dot(world.GetStepPointVelocity(solve.chassis,state.point,solve.dt)-
        world.GetStepPointVelocity(state.body,state.point,solve.dt),solve.longitudinal.direction);
    const float rimSpeed=(state.angularVelocity-glm::dot(world.GetAngularVelocity(solve.chassis),solve.axle))*settings.radius;
    const float speedScale=std::max({.5f,std::abs(groundSpeed),std::abs(rimSpeed)});
    state.slipRatio=-state.longitudinalSlip/speedScale;
    state.slipAngle=std::atan2(state.lateralSlip,std::max(.5f,std::abs(groundSpeed)));
    const float normalImpulse=solve.normalImpulse+solve.stopImpulse;
    const bool staticContact=std::abs(groundSpeed)<.5f&&std::abs(rimSpeed)<.5f&&std::abs(state.lateralSlip)<.5f;
    const double gx=!staticContact&&normalImpulse>0 ? speedScale/(normalImpulse*settings.longitudinalStiffness):0;
    const double gy=!staticContact&&normalImpulse>0 ? speedScale/(normalImpulse*settings.lateralStiffness):0;
    const double a=solve.longitudinal.inverseMass+gx,b=solve.lateral.inverseMass+gy,c=solve.tangentCrossMass;
    const double determinant=a*b-c*c;
    if(determinant<=1e-12*a*b)return;
    const double vx=state.longitudinalSlip+gx*solve.longImpulse;
    const double vy=state.lateralSlip+gy*solve.lateralImpulse;
    double x=solve.longImpulse+(-vx*b+c*vy)/determinant;
    double y=solve.lateralImpulse+(-vy*a+c*vx)/determinant;
    const double length=std::hypot(x,y),limit=solve.friction*(solve.normalImpulse+solve.stopImpulse);
    if(length>limit&&length>0){x*=limit/length;y*=limit/length;}
    Apply(world,*this,solve.longitudinal,solve,float(x)-solve.longImpulse);
    Apply(world,*this,solve.lateral,solve,float(y)-solve.lateralImpulse);
    solve.longImpulse=float(x);solve.lateralImpulse=float(y);
    state.longitudinalImpulse=solve.longImpulse;state.lateralImpulse=solve.lateralImpulse;
    state.gripUtilization=limit>0 ? float(std::hypot(x,y)/limit) : 0;
}

void WheelInstance::FinishInterval(PhysicsWorld& world) {
    if(!solve.active)return;
    const float spin=state.angularVelocity-glm::dot(world.GetAngularVelocity(solve.chassis),solve.axle);
    state.angle=std::remainder(state.angle+spin*solve.dt,glm::two_pi<float>());
    Publish(world,solve.chassis);
}

bool WheelInstance::HasSleepSupport() const {
    return solve.active&&state.contact&&state.normalForce>0;
}
bool WheelInstance::QuietSupport(const PhysicsWorld& world) const {
    return command.driveTorque==0&&glm::length(state.groundVelocity)<1e-4f&&
        std::abs(state.angularVelocity-glm::dot(world.GetAngularVelocity(solve.chassis),solve.axle))<.03f;
}
