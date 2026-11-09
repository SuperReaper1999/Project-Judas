#pragma once
#include "RuntimeWorld.h"
#include "Simulation.h"
#include "InputSystem.h"
#include <algorithm>

namespace VehicleFixture {
inline constexpr float Dt=1.f/60;
inline Scene Create(unsigned axles=2,bool reverse=false) {
    Scene scene;
    auto& floor=scene.CreateObject("Ground");
    floor.body=SceneBodyComponent{};floor.body->halfExtents={1000,.5f,1000};
    floor.body->friction=.8f;floor.transform.position={0,-.5f,0};
    auto& gravity=scene.CreateObject("Gravity");
    gravity.gravity=SceneGravityComponent{};gravity.gravity->kind=SceneGravityKind::Uniform;
    gravity.gravity->regionRadius=10000;
    auto& chassis=scene.CreateObject("Chassis");
    chassis.body=SceneBodyComponent{};chassis.body->motion=SceneBodyMotion::Dynamic;
    chassis.body->mass=200.f*axles;chassis.body->halfExtents={.7f,.15f,.9f*axles-.7f};
    chassis.body->friction=.5f;chassis.body->restitution=0;chassis.body->managed=true;
    chassis.transform.position={0,.8f,0};
    std::vector<glm::vec3> mounts;
    for(unsigned axle=0;axle<axles;++axle)for(float x:{-.7f,.7f})
        mounts.push_back({x,-.1f,1.8f*axle-.9f*(axles-1)});
    if(reverse)std::reverse(mounts.begin(),mounts.end());
    for(auto mount:mounts) {
        auto& wheel=scene.CreateObject("Wheel");wheel.parent=3;
        wheel.wheel=WheelSettings{};wheel.wheel->chassis=3;wheel.wheel->mount=mount;
        wheel.wheel->restLength=.4f;wheel.wheel->maxLength=.7f;
        wheel.wheel->springStiffness=14000;wheel.wheel->damping=1500;
        wheel.wheel->rollingResistance=0;
        wheel.transform.position=mount+glm::vec3(0,-.4f,0);
    }
    return scene;
}
inline void Tick(RuntimeWorld& world,unsigned count=1) {
    InputSystem input;
    for(unsigned i=0;i<count;++i)StepAuthoritativeWorld(world,input,Dt);
}
inline void Drive(RuntimeWorld& world,float torque,float steer=0) {
    for(const auto& o:world.ScriptObjects(false))if(o.wheel) {
        auto* wheel=world.RuntimeWheel(o.id);
        wheel->command.driveTorque=torque;wheel->command.steeringAngle=steer;
    }
}
}
