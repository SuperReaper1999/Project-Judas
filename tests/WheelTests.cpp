#include "Wheel.h"
#include "SaveArchive.h"
#include "RadicalGravity.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <numeric>

namespace {
int checks=0,failures=0;
void Check(bool pass,const char* label){++checks;failures+=!pass;std::printf("%s %s\n",pass?"PASS":"FAIL",label);}
constexpr float Dt=1.f/60;
struct Assembly {
    PhysicsWorld world;BodyHandle ground,chassis;std::vector<WheelInstance> wheels;
    glm::quat rotation{1,0,0,0};glm::vec3 gravity{0,-9.81f,0};
    Assembly(int count=4,glm::quat q={1,0,0,0},bool dynamicGround=false):rotation(q),gravity(q*glm::vec3(0,-9.81f,0)) {
        world.Init();ground=dynamicGround?world.CreateDynamicBox(q*glm::vec3(0,-.5f,0),{8,.5f,8},2000,.8f,0):
            world.CreateStaticBox(q*glm::vec3(0,-.5f,0),q,{80,.5f,80},.8f,0);
        if(dynamicGround)world.ResetBody(ground,q*glm::vec3(0,-.5f,0),q);
        chassis=world.CreateDynamicBox(q*glm::vec3(0,.8f,0),{.7f,.15f,1.1f},400,.5f,0);
        world.ResetBody(chassis,q*glm::vec3(0,.8f,0),q);
        for(int i=0;i<count;++i){WheelInstance wheel;wheel.settings.chassis=1;
            wheel.settings.mount={(i%2?1.f:-1.f)*.7f,-.1f,count==1?0:float(i/2)*1.8f-.9f};
            if(count==1)wheel.settings.mount.x=0;
            wheel.settings.radius=.35f;wheel.settings.restLength=.4f;wheel.settings.maxLength=.7f;
            wheel.settings.springStiffness=14000;wheel.settings.damping=1500;wheel.settings.rollingResistance=0;
            wheel.state.length=.4f;wheels.push_back(wheel);
        }
    }
    void Step(bool gravityOn=true){if(gravityOn)world.ApplyLinearAcceleration(chassis,gravity,Dt);
        for(auto& wheel:wheels)wheel.Step(world,chassis,Dt);
        world.Step(Dt);
        for(auto& wheel:wheels)wheel.Publish(world,chassis);
    }
    void Settle(){for(int i=0;i<300;++i)Step();}
    glm::vec3 LocalPosition(){return glm::conjugate(rotation)*world.GetTransform(chassis).position;}
};
}
int main(){
    std::string error;WheelSettings settings;settings.chassis=1;
    Check(ValidateWheelSettings(settings,error),"valid generic wheel settings");
    settings.spinInertia=0;Check(!ValidateWheelSettings(settings,error),"zero spin inertia rejected");settings.spinInertia=1;
    settings.restLength=settings.minLength;Check(!ValidateWheelSettings(settings,error),"inverted travel rejected");
    WheelCommand command;command.brakeTorque=-1;Check(!ValidateWheelCommand(command,error),"negative brake rejected");
    Assembly a;a.Settle();float normal=0;for(auto& wheel:a.wheels)normal+=wheel.state.normalForce;
    std::printf("REST y %.6f speed %.6f load %.6f\n",a.LocalPosition().y,glm::length(a.world.GetLinearVelocity(a.chassis)),normal);
    Check(a.LocalPosition().y>.55f&&a.LocalPosition().y<.9f&&glm::length(a.world.GetLinearVelocity(a.chassis))<.03f,"four independent springs support actual chassis load");
    Check(std::abs(normal-400*9.81f)<80,"suspension load agrees with independent mg expectation");
    const float lightLength=a.wheels[0].state.length;
    for(int i=0;i<240;++i){a.world.ApplyForce(a.chassis,a.gravity*400.f);a.Step();}
    float loaded=0;for(auto& wheel:a.wheels)loaded+=wheel.state.normalForce;
    Check(loaded>normal*1.9f&&a.wheels[0].state.length<lightLength-.02f,"sustained external load increases real suspension compression");
    float before=a.LocalPosition().z;for(auto& wheel:a.wheels)wheel.command.driveTorque=80;
    for(int i=0;i<120;++i)a.Step();
    std::printf("DRIVE z %.6f velocity %.6f\n",a.LocalPosition().z,(glm::conjugate(a.rotation)*a.world.GetLinearVelocity(a.chassis)).z);
    Check(a.LocalPosition().z<before-2,"drive torque moves chassis forward through tyre contact");
    float speed=glm::length(a.world.GetLinearVelocity(a.chassis));for(auto& wheel:a.wheels){wheel.command.driveTorque=0;wheel.command.brakeTorque=180;}
    for(int i=0;i<180;++i)a.Step();
    Check(glm::length(a.world.GetLinearVelocity(a.chassis))<speed*.25f,"braking removes actual chassis motion");
    Assembly grip;grip.Settle();for(auto& w:grip.wheels){w.settings.friction=0;w.command.driveTorque=60;}
    before=grip.LocalPosition().z;for(int i=0;i<90;++i)grip.Step();
    Check(std::abs(grip.LocalPosition().z-before)<.15f&&std::abs(grip.wheels[0].state.angularVelocity)>20,"zero grip spins wheels without decorative chassis propulsion");
    Assembly lateral;lateral.Settle();lateral.world.SetLinearVelocity(lateral.chassis,{3,0,0});
    for(int i=0;i<120;++i)lateral.Step();
    Check(std::abs(lateral.world.GetLinearVelocity(lateral.chassis).x)<.1f,"lateral tyre grip resists sideways slip");
    Assembly steer;steer.Settle();for(auto& w:steer.wheels)w.command.driveTorque=60;
    for(int i=0;i<2;++i)steer.wheels[i].command.steeringAngle=.25f;
    for(int i=0;i<180;++i)steer.Step();
    Check(std::abs(steer.LocalPosition().x)>.5f&&std::abs(steer.world.GetAngularVelocity(steer.chassis).y)>.02f,"steering rotates contact frame and turns rigid chassis");
    const auto q=glm::angleAxis(.91f,glm::normalize(glm::vec3(1,2,3)));
    Assembly ordinary,rotated(4,q);ordinary.Settle();rotated.Settle();
    for(auto* setup:{&ordinary,&rotated})for(auto& w:setup->wheels)w.command.driveTorque=40;
    for(int i=0;i<90;++i){ordinary.Step();rotated.Step();}
    Check(glm::length(ordinary.LocalPosition()-rotated.LocalPosition())<.03f,"whole-scene rotation preserves wheel behaviour without world-up");
    Assembly uneven;uneven.world.CreateStaticBox({.7f,.04f,-.9f},{.45f,.04f,.5f},.8f,0);uneven.Settle();
    Check(uneven.wheels[1].state.contact&&std::abs(uneven.wheels[1].state.length-uneven.wheels[0].state.length)>.01f&&
        glm::length(uneven.world.GetLinearVelocity(uneven.chassis))<.05f,"independent wheel travel accommodates uneven collision geometry");
    Assembly planet;planet.world.DestroyBody(planet.ground);planet.ground=planet.world.CreateStaticSphere({0,-80,0},80,.8f,0);
    RadicalGravity radial({0,-80,0},9.81f);
    for(int i=0;i<300;++i){planet.world.ApplyLinearAcceleration(planet.chassis,radial.Sample(planet.world.GetTransform(planet.chassis).position),Dt);planet.Step(false);}
    for(auto& wheel:planet.wheels)wheel.command.driveTorque=35;
    for(int i=0;i<120;++i){planet.world.ApplyLinearAcceleration(planet.chassis,radial.Sample(planet.world.GetTransform(planet.chassis).position),Dt);planet.Step(false);}
    // Independent no-slip flat limit: s = .5 t^2 (sum T/r)/(m+sum I/r^2).
    // This fixture's 35 Nm wheels cannot cover 2 m in 2 s even in that limit.
    const float radialReference=.5f*4*(4*35.f/.35f)/(400+4/(.35f*.35f));
    std::printf("RADIAL z %.9g contact %d normalZ %.9g ideal_flat_displacement %.9g\n",
        planet.LocalPosition().z,planet.wheels[0].state.contact,planet.wheels[0].state.normal.z,radialReference);
    Check(planet.wheels[0].state.contact&&planet.LocalPosition().z<-radialReference*.8f&&
        std::abs(planet.wheels[0].state.normal.z)>.01f,"real radial gravity and curved surface support rolling travel");
    Assembly single(1);single.world.ResetBody(single.chassis,{0,.8f,0},{1,0,0,0});
    single.Step();Check(single.wheels[0].state.contact&&single.wheels[0].state.normalForce>0,"single-wheel assembly uses same solver without wheel-count assumptions");
    Assembly unsupported;unsupported.world.ResetBody(unsupported.chassis,{0,8,0},{1,0,0,0});
    for(auto& w:unsupported.wheels)w.command.driveTorque=10;
    unsupported.Step(false);
    Check(!unsupported.wheels[0].state.contact&&std::abs(unsupported.wheels[0].state.angularVelocity)>0&&
        glm::length(unsupported.world.GetAngularVelocity(unsupported.chassis))>0,"airborne drive spins wheel with opposite chassis torque");
    glm::vec3 angular=unsupported.world.GetInertiaWorld(unsupported.chassis)*unsupported.world.GetAngularVelocity(unsupported.chassis);
    auto currentQ=unsupported.world.GetTransform(unsupported.chassis).rotation;
    for(auto& w:unsupported.wheels)angular+=(currentQ*glm::vec3(-1,0,0))*w.settings.spinInertia*w.state.angularVelocity;
    Check(glm::length(angular)<.001f,"airborne shaft reaction conserves angular momentum at fixed axis");
    auto& released=unsupported.wheels[0];released.command={};released.settings.rollingResistance=1;
    released.state.normalForce=2000;const float airSpin=released.state.angularVelocity;
    released.Step(unsupported.world,unsupported.chassis,Dt);
    Check(!released.state.contact&&released.state.angularVelocity==airSpin,"lost ground contact cannot apply cached rolling resistance in air");
    Assembly paired(4,{1,0,0,0},true);paired.world.SetLinearVelocity(paired.chassis,{0,-1,-2});
    const auto momentum0=400.f*paired.world.GetLinearVelocity(paired.chassis)+2000.f*paired.world.GetLinearVelocity(paired.ground);
    paired.Step(false);
    const auto momentum1=400.f*paired.world.GetLinearVelocity(paired.chassis)+2000.f*paired.world.GetLinearVelocity(paired.ground);
    Check(glm::length(momentum1-momentum0)<.002f&&glm::length(paired.world.GetLinearVelocity(paired.ground))>.001f,"dynamic ground receives paired impulses and linear momentum is conserved");
    Assembly prescribed;prescribed.world.SetMotionType(prescribed.ground,BodyMotionType::Kinematic);
    prescribed.world.SetKinematicVelocity(prescribed.ground,{1,0,0},{0,0,0});prescribed.wheels[0].Step(prescribed.world,prescribed.chassis,Dt);
    Check(prescribed.wheels[0].state.contact&&prescribed.wheels[0].state.groundVelocity.x==1&&
        prescribed.world.GetLinearVelocity(prescribed.chassis).x>0,"new prescribed ground velocity acts on tyre in the same fixed interval");
    prescribed.world.Step(Dt);prescribed.world.StopKinematic(prescribed.ground);
    Check(glm::length(prescribed.world.GetStepPointVelocity(prescribed.ground,{0,0,0},Dt))==0&&
        prescribed.world.GetPointVelocity(prescribed.ground,{0,0,0}).x==1,"stop command supersedes preceding published ground velocity without mutating it");
    const auto pose=prescribed.world.GetTransform(prescribed.ground);
    prescribed.world.MoveKinematic(prescribed.ground,{pose.position+glm::vec3(1,0,0),glm::angleAxis(.2f,glm::vec3(0,0,1))},.5f);
    const auto preview=prescribed.world.GetStepPointVelocity(prescribed.ground,pose.position+glm::vec3(1,0,0),Dt);
    KinematicMotionState beforeCommand,afterCommand;prescribed.world.GetKinematicMotion(prescribed.ground,beforeCommand);
    prescribed.world.GetStepPointVelocity(prescribed.ground,pose.position,Dt);prescribed.world.GetKinematicMotion(prescribed.ground,afterCommand);
    Check(glm::length(preview-glm::vec3(2,.4f,0))<1e-5f&&
        beforeCommand.remainingSeconds==afterCommand.remainingSeconds&&prescribed.world.GetTransform(prescribed.ground).position==pose.position,
        "target and rotation command preview matches independent velocities and remains read-only");
    Assembly filtered;filtered.world.SetCollisionFilter(filtered.ground,2,CategoryMask(1)<<3);
    filtered.Step();Check(!filtered.wheels[0].state.contact,"bilateral M39 mask rejects wheel layer");
    for(auto& w:filtered.wheels)w.settings.collisionLayer=3;
    filtered.Step();Check(filtered.wheels[0].state.contact,"bilateral M39 mask admits configured wheel layer");
    for(auto& w:filtered.wheels)w.settings.requiredTags=CategoryMask(1)<<7;
    filtered.Step();Check(!filtered.wheels[0].state.contact,"required tag rejects unclassified ground");
    filtered.world.SetBodyTags(filtered.ground,CategoryMask(1)<<7);filtered.Step();Check(filtered.wheels[0].state.contact,"normal tag classification enables ground");
    filtered.world.SetBodyEnabled(filtered.ground,false);filtered.Step();Check(!filtered.wheels[0].state.contact,"disabled collider supplies no wheel support");
    const auto retired=filtered.chassis;filtered.world.DestroyBody(retired);
    auto reused=filtered.world.CreateDynamicBox({0,.8f,0},{1,.1f,1},100,.5f,0);
    filtered.wheels[0].Step(filtered.world,retired,Dt);
    Check(!filtered.wheels[0].state.available&&glm::length(filtered.world.GetLinearVelocity(reused))==0,"stale chassis cannot alias reused body slot");
    auto saved=ordinary.wheels[0];SaveArchive archive;saved.Persist(archive);
    WheelInstance restored;restored.settings=saved.settings;SaveArchive reader(archive.bytes);restored.Persist(reader);reader.Finish();
    Check(restored.command.driveTorque==saved.command.driveTorque&&restored.state.angularVelocity==saved.state.angularVelocity&&
        restored.state.angle==saved.state.angle&&!restored.state.body.IsValid(),"durable spin/command state roundtrips without raw contact handles");
    // Independent four-wheel bodies, not 100 overlapping supports on one body.
    // Queries, wheel solve and ordinary rigid stepping are measured separately.
    for(int count:{4,24,100}){
        Assembly perf;std::vector<BodyHandle> bodies{perf.chassis};std::vector<WheelInstance> many=perf.wheels;
        for(int vehicle=1;vehicle<count/4;++vehicle){
            const glm::vec3 position{float(vehicle%5)*4,.8f,float(vehicle/5)*4};
            auto body=perf.world.CreateDynamicBox(position,{.7f,.15f,1.1f},400,.5f,0);bodies.push_back(body);
            for(auto wheel:perf.wheels)many.push_back(wheel);
        }
        std::vector<double> wheelTimes,rigidTimes;unsigned contacts=0;
        for(int i=0;i<240;++i){for(auto body:bodies)perf.world.ApplyLinearAcceleration(body,perf.gravity,Dt);
            auto start=std::chrono::steady_clock::now();
            for(int j=0;j<count;++j)many[j].Step(perf.world,bodies[j/4],Dt);
            auto boundary=std::chrono::steady_clock::now();perf.world.Step(Dt);
            if(i>=60){wheelTimes.push_back(std::chrono::duration<double,std::milli>(boundary-start).count());
                rigidTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-boundary).count());}
        }
        for(auto& wheel:many)contacts+=wheel.state.contact;
        std::sort(wheelTimes.begin(),wheelTimes.end());std::sort(rigidTimes.begin(),rigidTimes.end());
        std::printf("PERF %d wheels/%zu chassis/%u contacts wheel_p50 %.6f ms p95 %.6f ms max %.6f ms rigid_p95 %.6f ms\n",
            count,bodies.size(),contacts,wheelTimes[90],wheelTimes[171],wheelTimes.back(),rigidTimes[171]);
    }
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
