#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "EnginePaths.h"
#include "Simulation.h"
#include "InputSystem.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
constexpr float dt=1.f/60;
unsigned checks=0;
Scene Fixture(bool oldAxles,float slope) {
    Scene scene;std::string error;
    if(!LoadSceneFromFile(ResolveEngineDataPath("projects/vehicle_v2_workshop/Scenes/workshop.judas"),scene,error))
        throw std::runtime_error(error);
    // Real Workshop chassis, wheels, masses and powertrain. Isolate the flat
    // support and articulation from rendering, other vehicles and course props.
    scene.Objects().erase(std::remove_if(scene.Objects().begin(),scene.Objects().end(),[](const auto& o){
        return o.id!=1&&o.id!=2&&o.id!=600&&!(o.id>=400&&o.id<590);
    }),scene.Objects().end());
    const auto tilt=glm::angleAxis(slope,glm::vec3(1,0,0));
    for(auto& o:scene.Objects()) {
        o.render.reset();o.scripts.clear();
        if(o.id==2)o.body->halfExtents={1000,.5f,1000};
        if(o.id==2||o.id==400||o.id==500){o.transform.position=tilt*o.transform.position;o.transform.rotation=tilt*o.transform.rotation;}
        if(oldAxles&&o.wheel&&o.wheel->chassis==500)o.wheel->mount.z=o.id<505?-.95f:.95f;
    }
    return scene;
}
struct Result {float articulation=0,stoppingArticulation=0,gap=0,stoppedSpeed=0,stoppedYaw=0,reverseDistance=0,push=0;};
Result Run(bool oldAxles,float slope,bool coast,bool tractorBrakesOnly=false,bool cycles=false) {
    const auto scene=Fixture(oldAxles,slope);RuntimeWorld world;std::string error;
    if(!world.Build(scene,nullptr,error))throw std::runtime_error(error);
    InputSystem input;Result result;
    const unsigned count=coast?3600:cycles?3600:1200;
    float stopYaw=0;bool yawRecorded=false;
    glm::vec3 reverseStart{0};
    for(unsigned i=0;i<count;++i) {
        const unsigned step=cycles&&i>=180?180+(i-180)%840:i;
        float throttle=step>=180&&step<480?1.f:0;
        const float steer=step>=300&&step<900?.3f:0;
        float pressure=!coast&&step>=480?1.f:0;
        if(!coast&&step>=780&&step<840)pressure=0;
        if(!coast&&step>=840&&step<900){pressure=0;throttle=-.25f;}
        const auto tractor=world.RuntimeBody(400);
        const auto pose=world.Physics().GetTransform(tractor);
        const float speed=glm::length(world.Physics().GetLinearVelocity(tractor));
        for(unsigned id:{400u,500u}) {
            if(auto* power=world.RuntimePowertrain(id))for(auto& command:power->command.sources)command.input=throttle;
            for(const auto& o:scene.Objects())if(o.wheel&&o.wheel->chassis==id) {
                auto* wheel=world.RuntimeWheel(o.id);
                wheel->command.brakeTorque=pressure*(id==400?721.035f:tractorBrakesOnly?0:927.045f);
                wheel->command.steeringAngle=id==400&&wheel->settings.mount.z<0?steer*.42f/(1+.003f*speed*speed):0;
            }
        }
        if(i==840)reverseStart=pose.position;
        StepAuthoritativeWorld(world,input,dt);
        const auto a=world.Physics().GetTransform(tractor),b=world.Physics().GetTransform(world.RuntimeBody(500));
        const float angle=std::acos(std::clamp(glm::dot(a.rotation*glm::vec3(0,0,-1),b.rotation*glm::vec3(0,0,-1)),-1.f,1.f));
        if(i>=480)result.articulation=std::max(result.articulation,angle);
        if(step>=480&&step<780)result.stoppingArticulation=std::max(result.stoppingArticulation,angle);
        result.gap=std::max(result.gap,glm::length(a.position+a.rotation*glm::vec3(0,0,1.8f)-b.position-b.rotation*glm::vec3(0,0,-3.2f)));
        JointState hitch;world.Physics().GetJoint(world.RuntimeJoint(600),hitch);
        result.push=std::max(result.push,glm::dot(hitch.reactionImpulse,a.rotation*glm::vec3(0,0,-1))/dt);
        if(i==899)result.reverseDistance=glm::dot(a.position-reverseStart,pose.rotation*glm::vec3(0,0,1));
        if(!cycles&&i>=660&&i<780){
            const float yaw=std::atan2((a.rotation*glm::vec3(0,0,-1)).x,(a.rotation*glm::vec3(0,0,-1)).z);
            if(!yawRecorded){stopYaw=yaw;yawRecorded=true;}
            result.stoppedSpeed=std::max(result.stoppedSpeed,glm::length(world.Physics().GetLinearVelocity(tractor)));
            result.stoppedYaw=std::max(result.stoppedYaw,std::abs(std::remainder(yaw-stopYaw,6.2831853f)));
        }
    }
    return result;
}
void Require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
}
unsigned RunVehicleTowingChecks() {
    const auto negative=Run(true,0,true),coast=Run(false,0,true);
    std::cout<<"TOW_COAST old_max_articulation_rad="<<negative.articulation<<" current="<<coast.articulation<<'\n';
    Require(negative.articulation>2,"original fixed car-wheelbase trailer reproduces folding while coasting (negative control)");
    Require(coast.articulation<.6f&&coast.gap<.01f,"rear tandem trailer follows during a sustained coasting turn without folding");
    for(float slope:{0.f,.1f,-.1f}) {
        const auto braking=Run(false,slope,false);
        std::cout<<"TOW_BRAKE slope="<<slope<<" articulation_rad="<<braking.articulation<<" stopping_articulation_rad="<<braking.stoppingArticulation<<" stopped_speed="<<braking.stoppedSpeed
            <<" stopped_yaw_rad="<<braking.stoppedYaw<<" reverse_m="<<braking.reverseDistance<<" hitch_m="<<braking.gap<<" push_N="<<braking.push<<'\n';
        Require(// A real turn/reverse is allowed to articulate. The contract is a
        // following trailer (positive forward dot), not an artificial straight rig.
        braking.stoppingArticulation<1.0471976f&&braking.articulation<1.5707963f&&braking.gap<.01f,"turning service-brake stop and deliberate reverse retain free, attached articulation");
        Require(braking.stoppedSpeed<.05f&&braking.stoppedYaw<.02f,"braked flat/sloped rig remains stopped without yaw drift");
        Require(braking.reverseDistance>.1f,"release and re-press reverse genuinely backs the articulated rig");
        Require(braking.push>20,"braking fixture exercises a real compressive hitch reaction");
    }
    const auto pushing=Run(false,0,false,true);
    Require(pushing.gap<.01f&&std::isfinite(pushing.articulation)&&pushing.push>20,
            "unbraked pushing trailer remains a dynamic MK2 load, without artificial yaw locks");
    const auto repeated=Run(false,0,false,false,true);
    Require(repeated.articulation<1&&repeated.gap<.01f,"repeated drive, brake, deliberate reverse and stop cycles do not fold the rig");
    return checks;
}
