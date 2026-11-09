#include "support/VehicleFixture.h"
#include "SaveStorage.h"
#include "WorldPersistence.h"
#include "SceneFingerprint.h"
#include "Prefab.h"
#include "ResourceManager.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainBrush.h"
#include "PerformanceProfiler.h"
#include "authoring/VehicleAssembly.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

unsigned RunVehicleTowingChecks();

namespace {
unsigned checks=0;
void Check(bool value,const std::string& label) {
    ++checks;
    if(!value)throw std::runtime_error(label);
    std::cout<<"PASS "<<label<<'\n';
}
void Build(RuntimeWorld& world,const Scene& scene) {
    std::string error;
    Check(world.Build(scene,nullptr,error),"build current vehicle fixture: "+error);
}
void SleepingAndOrder() {
    RuntimeWorld forward,reverse;
    Build(forward,VehicleFixture::Create());Build(reverse,VehicleFixture::Create(2,true));
    VehicleFixture::Tick(forward,300);VehicleFixture::Tick(reverse,300);
    Check(forward.Physics().IsSleeping(forward.RuntimeBody(3))&&
          reverse.Physics().IsSleeping(reverse.RuntimeBody(3)),"query-supported chassis naturally sleep in MK2");
    VehicleFixture::Drive(forward,60,.2f);VehicleFixture::Drive(reverse,60,.2f);
    for(unsigned i=0;i<600;++i){VehicleFixture::Tick(forward);VehicleFixture::Tick(reverse);}
    const auto a=forward.Physics().GetTransform(forward.RuntimeBody(3));
    const auto b=reverse.Physics().GetTransform(reverse.RuntimeBody(3));
    const float difference=glm::length(a.position-b.position);
    std::cout<<"ORDER position_difference_m="<<difference<<'\n';
    Check(difference<.01f,"reversed component registration preserves ten-second driving trajectory");
    Check(glm::length(a.position)>2&&!forward.Physics().IsSleeping(forward.RuntimeBody(3)),
          "drive wakes and physically moves chassis");
    RuntimeWorld removed;Build(removed,VehicleFixture::Create());VehicleFixture::Tick(removed,300);
    const float before=removed.Physics().GetTransform(removed.RuntimeBody(3)).position.y;
    removed.Physics().SetBodyEnabled(removed.RuntimeBody(1),false);VehicleFixture::Tick(removed,30);
    Check(!removed.Physics().IsSleeping(removed.RuntimeBody(3))&&
          removed.Physics().GetTransform(removed.RuntimeBody(3)).position.y<before-.2f,
          "removing support wakes a sleeping vehicle without stale suspension");
    RuntimeWorld moving;Build(moving,VehicleFixture::Create());VehicleFixture::Tick(moving,300);
    moving.Physics().SetMotionType(moving.RuntimeBody(1),BodyMotionType::Kinematic);
    moving.Physics().SetKinematicVelocity(moving.RuntimeBody(1),{1,0,0},{0,0,0});
    VehicleFixture::Tick(moving,60);
    Check(moving.Physics().GetTransform(moving.RuntimeBody(3)).position.x>.2f&&
          !moving.Physics().IsSleeping(moving.RuntimeBody(3)),"moving ground wakes and carries the real chassis");
    RuntimeWorld changed;Build(changed,VehicleFixture::Create());VehicleFixture::Tick(changed,300);
    const float oldHeight=changed.Physics().GetTransform(changed.RuntimeBody(3)).position.y;
    changed.Physics().ResetBody(changed.RuntimeBody(1),{0,-.3f,0},{1,0,0,0});
    VehicleFixture::Tick(changed);
    Check(!changed.Physics().IsSleeping(changed.RuntimeBody(3)),"changed support geometry wakes without a handle change");
    VehicleFixture::Tick(changed,120);
    Check(changed.Physics().GetTransform(changed.RuntimeBody(3)).position.y>oldHeight+.12f,
          "suspension re-equilibrates on the changed support plane");
    RuntimeWorld inserted;Build(inserted,VehicleFixture::Create());VehicleFixture::Tick(inserted,300);
    const auto ledge=inserted.Physics().CreateStaticBox({.7f,.14f,0},{.18f,.14f,2},.8f,0);
    VehicleFixture::Tick(inserted);
    Check(!inserted.Physics().IsSleeping(inserted.RuntimeBody(3))&&
          inserted.RuntimeWheel(5)->state.body.id==ledge.id,
          "sleep cache detects a new collider inside tread reach even when old support is unchanged");
    for(unsigned axles:{3u,4u}) {
        RuntimeWorld truck;Build(truck,VehicleFixture::Create(axles));VehicleFixture::Tick(truck,300);
        float load=0;unsigned contacts=0;
        for(unsigned i=0;i<axles*2;++i){auto* wheel=truck.RuntimeWheel(4+i);contacts+=wheel->state.contact;load+=wheel->state.normalForce;}
        Check(contacts==axles*2&&std::abs(load-(200.f*axles*9.81f))<100,
              "multi-axle support carries independent mg load");
        VehicleFixture::Drive(truck,60);VehicleFixture::Tick(truck,180);
        Check(truck.Physics().GetTransform(truck.RuntimeBody(3)).position.z<-1,
              "multi-axle vehicle drives without four-wheel assumptions");
    }
}
void UnorderedConstraints() {
    RuntimeWorld a,b;Build(a,VehicleFixture::Create());Build(b,VehicleFixture::Create());
    auto tick=[](RuntimeWorld& world,bool reverse) {
        auto& physics=world.Physics();const auto chassis=world.RuntimeBody(3);
        constexpr float h=1.f/240;
        physics.ApplyLinearAcceleration(chassis,{0,-9.81f,0},h);
        for(unsigned id=4;id<8;++id)world.RuntimeWheel(id)->PrepareInterval(physics,chassis,h);
        // Deliberately bypass runtime canonical sorting and symmetric sweeps.
        // Both permutations must converge with the production sweep budget.
        for(unsigned sweep=0;sweep<8;++sweep)for(unsigned n=0;n<4;++n)
            world.RuntimeWheel(reverse?7-n:4+n)->SolveIteration(physics);
        physics.Step(h);
        for(unsigned id=4;id<8;++id)world.RuntimeWheel(id)->FinishInterval(physics);
    };
    for(unsigned i=0;i<1200;++i){tick(a,false);tick(b,true);}
    VehicleFixture::Drive(a,60,.2f);VehicleFixture::Drive(b,60,.2f);
    for(unsigned i=0;i<2400;++i){tick(a,false);tick(b,true);}
    const float difference=glm::length(a.Physics().GetTransform(a.RuntimeBody(3)).position-
                                       b.Physics().GetTransform(b.RuntimeBody(3)).position);
    std::cout<<"UNSORTED_SOLVE position_difference_m="<<difference<<'\n';
    Check(difference<.01f,"constraint-order convergence is real without canonical component sorting");
}
double Energy(RuntimeWorld& world) {
    auto& physics=world.Physics();const auto chassis=world.RuntimeBody(3);
    const auto v=glm::dvec3(physics.GetLinearVelocity(chassis));
    const auto w=glm::dvec3(physics.GetAngularVelocity(chassis));
    double energy=.5*physics.GetMass(chassis)*glm::dot(v,v)+
        .5*glm::dot(w,glm::dmat3(physics.GetInertiaWorld(chassis))*w);
    for(unsigned id=4;id<8;++id) {
        const auto* wheel=world.RuntimeWheel(id);
        const double compression=std::max(0.f,wheel->settings.restLength-wheel->state.length);
        energy+=.5*wheel->settings.spinInertia*double(wheel->state.angularVelocity)*wheel->state.angularVelocity+
                .5*wheel->settings.springStiffness*compression*compression;
    }
    return energy;
}
void PassiveEnergy() {
    RuntimeWorld world;Build(world,VehicleFixture::Create());VehicleFixture::Tick(world,300);
    std::string error;Check(world.SetGravitySelection(3,GravitySelection{},error),"select zero external gravity for energy control");
    world.Physics().SetLinearVelocity(world.RuntimeBody(3),{3,0,-5});
    const double initial=Energy(world);double maximum=initial;
    for(unsigned i=0;i<240;++i){VehicleFixture::Tick(world);maximum=std::max(maximum,Energy(world));}
    std::cout<<"ENERGY initial_j="<<initial<<" maximum_j="<<maximum<<" final_j="<<Energy(world)<<'\n';
    Check(maximum<=initial*1.001+.01,"passive tyres and suspension do not create mechanical energy");
}
void AuthoredAssemblies() {
    for(unsigned axles:{2u,3u,4u}) {
        auto scene=VehicleFixture::Create();
        scene.Objects().erase(std::remove_if(scene.Objects().begin(),scene.Objects().end(),
            [](const auto& object){return object.id>=3;}),scene.Objects().end());
        JudasVehicles::AssemblyRecipe recipe;recipe.axles=axles;recipe.mass=250.f*axles;
        recipe.pose.position={0,1.1f,0};
        JudasVehicles::AssemblyResult created;std::string error;
        Check(JudasVehicles::CreateAssembly(scene,recipe,created,error),"author reusable vehicle assembly: "+error);
        Check(created.wheels.size()==2*axles&&scene.Find(created.root)->powertrain->shafts.size()==2*axles,
              "authoring derives wheel and drivetrain counts from the layout");
        Scene prefab;
        Check(CreatePrefab(scene,created.root,prefab,error),"complete authored vehicle becomes an ordinary prefab");
        RuntimeWorld world;Build(world,scene);VehicleFixture::Tick(world,300);
        Check(world.Physics().IsSleeping(world.RuntimeBody(created.root)),"authored parked assembly sleeps");
        auto* drive=world.RuntimePowertrain(created.root);
        for(auto& source:drive->command.sources)source.input=.6;
        VehicleFixture::Tick(world,180);
        Check(world.Physics().GetTransform(world.RuntimeBody(created.root)).position.z<-1&&drive->state.converged,
              "authored electric graph moves the real multi-wheel chassis");
        const auto before=scene;
        recipe.axles=0;
        Check(!JudasVehicles::CreateAssembly(scene,recipe,created,error)&&ScenesEqual(before,scene),
              "invalid vehicle authoring is an atomic scene failure");
    }
}
void TrailerAndSpeed() {
    auto scene=VehicleFixture::Create();
    JudasVehicles::AssemblyRecipe recipe;recipe.name="Trailer";recipe.powered=false;
    recipe.mass=300;recipe.pose.position={0,.8f,4};recipe.track=1.4f;recipe.axleSpacing=1.8f;
    JudasVehicles::AssemblyResult trailer;std::string error;
    Check(JudasVehicles::CreateAssembly(scene,recipe,trailer,error),"author unpowered trailer");
    auto& hitch=scene.CreateObject("Ball hitch");hitch.joint=SceneJointComponent{};
    hitch.joint->bodyA=3;hitch.joint->bodyB=trailer.root;hitch.joint->settings.type=JointType::Ball;
    hitch.joint->settings.anchorA={0,0,1.5f};hitch.joint->settings.anchorB={0,0,-2.5f};
    RuntimeWorld world;Build(world,scene);VehicleFixture::Tick(world,300);
    for(unsigned id=4;id<8;++id){auto* wheel=world.RuntimeWheel(id);wheel->command.driveTorque=60;wheel->command.steeringAngle=wheel->settings.mount.z<0?.15f:0;}
    float maximumGap=0;
    for(unsigned i=0;i<300;++i){
        VehicleFixture::Tick(world);
        const auto a=world.Physics().GetTransform(world.RuntimeBody(3));
        const auto b=world.Physics().GetTransform(world.RuntimeBody(trailer.root));
        maximumGap=std::max(maximumGap,glm::length(a.position+a.rotation*glm::vec3(0,0,1.5f)-
                                                 b.position-b.rotation*glm::vec3(0,0,-2.5f)));
    }
    std::cout<<"TRAILER maximum_hitch_error_m="<<maximumGap<<'\n';
    Check(maximumGap<.03f&&world.Physics().GetTransform(world.RuntimeBody(trailer.root)).position.z<3,
          "real joint tows unpowered wheels and preserves the articulated attachment");
    for(float speed:{5.f,15.f,30.f,50.f}) {
        RuntimeWorld fast;Build(fast,VehicleFixture::Create());VehicleFixture::Tick(fast,300);
        fast.Physics().SetLinearVelocity(fast.RuntimeBody(3),{0,0,-speed});
        for(unsigned id=4;id<8;++id)fast.RuntimeWheel(id)->state.angularVelocity=speed/.35f;
        const float start=fast.Physics().GetTransform(fast.RuntimeBody(3)).position.z;
        VehicleFixture::Tick(fast,120);
        const auto p=fast.Physics().GetTransform(fast.RuntimeBody(3)).position;
        Check(std::abs((start-p.z)-2*speed)<.1f&&p.y>.7f&&p.y<.9f,
              "rolling-speed control preserves support and independent distance expectation");
    }
    RuntimeWorld invalid;Build(invalid,VehicleFixture::Create());
    invalid.Physics().ResetBody(invalid.RuntimeBody(3),{0,50,0},{1,0,0,0});
    auto* wheel=invalid.RuntimeWheel(4);wheel->settings.spinInertia=1e-7f;
    wheel->command.driveTorque=std::numeric_limits<float>::max();
    wheel->PrepareInterval(invalid.Physics(),invalid.RuntimeBody(3),1);
    Check(!wheel->state.available&&!wheel->state.diagnostic.empty()&&wheel->state.angularVelocity==0&&
          invalid.Physics().GetAngularVelocity(invalid.RuntimeBody(3))==glm::vec3(0),
          "unrepresentable wheel effort fails before publishing any rotor or chassis impulse");
}
void TerrainAndIntervals() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("judas-vehicle-terrain-"+std::to_string(PerformanceProfiler::Now()));
    fs::create_directories(root/"Assets");
    auto source=TerrainV2::Create(65,65,.5f);std::fill(source.heights.begin(),source.heights.end(),0);
    std::vector<uint8_t> bytes;std::string error;
    Check(TerrainV2::EncodeAsset(source,bytes,error),"encode native terrain support fixture");
    const auto path=root/"Assets/ground.judasterrain";
    {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
    AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());AssetRecord record;
    Check(assets.Track(path.string(),record,error),"register actual terrain source: "+error);
    JobSystem jobs(2);ResourceManager resources(nullptr,&assets,&jobs);
    auto scene=VehicleFixture::Create();scene.Find(1)->landscape=SceneLandscapeSettings{record.id};scene.Find(1)->transform.position={0,0,0};
    RuntimeWorld world;
    Check(world.Build(scene,&resources,error)&&world.LiveTerrain().Wait(1,error),"publish native Terrain V2 vehicle collision: "+error);
    VehicleFixture::Tick(world,300);
    Check(world.Physics().IsSleeping(world.RuntimeBody(3)),"native terrain supports a sleeping vehicle");
    const auto before=world.Physics().GetTransform(world.RuntimeBody(3));
    const auto wheelPoint=world.RuntimeWheel(4)->state.point;
    const glm::vec3 delta{1024,0,0};
    Check(world.RequestOrigin(delta,error)&&world.CommitOrigin(),"commit real vehicle/terrain origin shift: "+error);
    Check(glm::length(world.Physics().GetTransform(world.RuntimeBody(3)).position-(before.position-delta))<1e-5f&&
          glm::length(world.RuntimeWheel(4)->state.point-(wheelPoint-delta))<1e-5f,
          "origin shift preserves chassis and wheel contact frame");
    Check(world.LiveTerrain().Brush(1,{-1024,0,0},{TerrainV2::Edit::Kind::Remove,{},2.5f},error)&&
          world.LiveTerrain().Wait(1,error),"excavate and republish terrain under the sleeping chassis: "+error);
    VehicleFixture::Tick(world,30);
    Check(!world.Physics().IsSleeping(world.RuntimeBody(3))&&
          world.Physics().GetTransform(world.RuntimeBody(3)).position.y<before.position.y-.15f,
          "terrain revision invalidates cached support and chassis falls into actual excavation");
    world.Destroy();resources.Shutdown();fs::remove_all(root);
    auto& profiler=PerformanceProfiler::Get();profiler.Enable(true);
    for(unsigned intervals:{1u,2u,4u}) {
        RuntimeWorld airborne;Build(airborne,VehicleFixture::Create());
        airborne.Physics().SetBodyEnabled(airborne.RuntimeBody(1),false);VehicleFixture::Drive(airborne,60);
        {ProfileFrame frame("vehicle-interval-control");
          airborne.BeginVehicleStep(VehicleFixture::Dt);
          auto coupling=PhysicsWorld::VelocityCoupling::ForEachSubstep(4,
              [&](const auto& substep){airborne.SolveVehicleSubstep(substep);},
              [&](const auto& substep){airborne.CompleteVehicleSubstep(substep);});
          airborne.Physics().StepCoupled(VehicleFixture::Dt,intervals,{}, {},&coupling);
        }
        double preparations=0;for(const auto& counter:profiler.Snapshot(profiler.FrameId()).counters)
            if(std::string(counter.name.c_str())=="Vehicle contact intervals")preparations=counter.value;
        Check(preparations==4&&std::abs(airborne.RuntimeWheel(4)->state.angularVelocity-1.f)<1e-5f,
              "structural subdivisions preserve four vehicle preparations and one physical torque interval");
    }
    profiler.Enable(false);
}
void TrafficAndTravel() {
    auto scene=VehicleFixture::Create();const auto first=scene.Objects();
    for(auto object:first)if(object.id>=3){object.id+=10;if(object.parent)object.parent+=10;
        if(object.wheel)object.wheel->chassis+=10;
        scene.Objects().push_back(object);}
    scene.SetNextId(18);scene.Find(3)->transform.position.z=-4;scene.Find(13)->transform.position.z=4;
    RuntimeWorld traffic;Build(traffic,scene);VehicleFixture::Tick(traffic,300);
    traffic.Physics().SetLinearVelocity(traffic.RuntimeBody(3),{0,0,8});
    traffic.Physics().SetLinearVelocity(traffic.RuntimeBody(13),{0,0,-8});
    for(unsigned n=4;n<=7;++n){traffic.RuntimeWheel(n)->state.angularVelocity=-8/.35f;traffic.RuntimeWheel(n+10)->state.angularVelocity=8/.35f;}
    VehicleFixture::Tick(traffic,120);
    const auto a=traffic.Physics().GetTransform(traffic.RuntimeBody(3)).position;
    const auto b=traffic.Physics().GetTransform(traffic.RuntimeBody(13)).position;
    Check(glm::distance(a,b)>2&&a.z<b.z&&a.y>.6f&&b.y>.6f,
          "two real wheel-supported chassis collide without crossing or losing support");
    Check(glm::length(traffic.Physics().GetLinearVelocity(traffic.RuntimeBody(3)))<1&&
          glm::length(traffic.Physics().GetLinearVelocity(traffic.RuntimeBody(13)))<1,
          "head-on vehicle impact settles through MK2 rather than a separate vehicle collision response");
    RuntimeWorld local,shifted;Build(local,VehicleFixture::Create());Build(shifted,VehicleFixture::Create());
    VehicleFixture::Tick(local,300);VehicleFixture::Tick(shifted,300);
    VehicleFixture::Drive(local,60);VehicleFixture::Drive(shifted,60);
    glm::vec3 delta{0};std::string error;
    for(unsigned step=0;step<1200;++step){
        if(step==1100){delta=glm::round(shifted.Physics().GetTransform(shifted.RuntimeBody(3)).position/512.f)*512.f;
            Check(glm::length(delta)>0&&shifted.RequestOrigin(delta,error)&&shifted.CommitOrigin(),"recenter origin on the vehicle during sustained driven travel");}
        VehicleFixture::Tick(local);VehicleFixture::Tick(shifted);
    }
    const auto expected=local.Physics().GetTransform(local.RuntimeBody(3)).position;
    const auto actual=shifted.Physics().GetTransform(shifted.RuntimeBody(3)).position+delta;
    std::cout<<"TRAVEL expected="<<expected.x<<","<<expected.y<<","<<expected.z<<" actual="<<actual.x<<","<<actual.y<<","<<actual.z<<" origin_shift_trajectory_error_m="<<glm::distance(actual,expected)<<'\n';
    Check(glm::distance(actual,expected)<.02f&&expected.z<-30,
          "twenty-second driven travel preserves trajectory through origin shift");
}
void Cold(const std::string& mode,const std::filesystem::path& root) {
    std::filesystem::create_directories(root);std::string error;
    SaveStorage storage(root.string());GameSnapshot saved;
    RuntimeWorld world;
    if(mode=="write") {
        auto scene=VehicleFixture::Create();
        PowertrainSettings power;
        PowertrainShaft rotor;rotor.name="rotor";rotor.inertia=.4;rotor.bearing=3;rotor.axis={-1,0,0};power.shafts.push_back(rotor);
        for(unsigned id:{6u,7u}){PowertrainShaft shaft;shaft.name="wheel_"+std::to_string(id);shaft.kind=PowertrainShaftKind::Wheel;shaft.target=id;power.shafts.push_back(shaft);}
        PowertrainSource source;source.name="engine";source.kind=PowertrainSourceKind::Electric;source.maxTorque=80;source.maxPower=10000;power.sources.push_back(source);
        PowertrainLink diff;diff.name="open";diff.kind=PowertrainLinkKind::Differential;diff.ratio=.3;power.links.push_back(diff);
        scene.Find(3)->powertrain=power;Build(world,scene);
        VehicleFixture::Tick(world,180);VehicleFixture::Drive(world,0,.1f);
        world.RuntimePowertrain(3)->command.sources[0].input=.7;
        VehicleFixture::Tick(world,60);
        saved.project="vehicle-v2";saved.scene="fixture";
        Check(ComputeSceneFingerprint(scene,saved.content,error)&&
              WorldPersistence::Capture(world,saved.participants,error)&&storage.Write("vehicle",saved,error),
              "write actual vehicle save to durable storage: "+error);
        VehicleFixture::Tick(world,30);
        const auto body=world.RuntimeBody(3);const auto pose=world.Physics().GetTransform(body);
        const auto velocity=world.Physics().GetLinearVelocity(body);
        std::ofstream expected(root/"expected.txt");expected<<std::setprecision(17)
            <<pose.position.x<<' '<<pose.position.y<<' '<<pose.position.z<<' '
            <<velocity.x<<' '<<velocity.y<<' '<<velocity.z<<' '
            <<world.RuntimeWheel(4)->state.angularVelocity<<' '<<world.RuntimeWheel(4)->state.angle<<' '
            <<world.RuntimePowertrain(3)->absoluteSpin[0]<<'\n';
        Check(bool(expected),"write independent continuation expectations");
    }else {
        bool recovered=false;Scene scene;
        Check(storage.Read("vehicle",saved,recovered,error)&&
              WorldPersistence::SceneFrom(saved.participants,scene,error)&&
              world.Build(scene,nullptr,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(world,error)&&
              WorldPersistence::Restore(world,saved.participants,error),"restore vehicle in a separate process: "+error);
        VehicleFixture::Tick(world,30);
        std::ifstream expected(root/"expected.txt");glm::vec3 position,velocity;float spin=0,angle=0;double rotor=0;
        expected>>position.x>>position.y>>position.z>>velocity.x>>velocity.y>>velocity.z>>spin>>angle>>rotor;
        Check(bool(expected),"read saved continuation expectations");
        const auto body=world.RuntimeBody(3);
        const auto distance=glm::length(world.Physics().GetTransform(body).position-position);
        std::cout<<"COLD position_error_m="<<distance<<'\n';
        Check(distance<.001f&&glm::length(world.Physics().GetLinearVelocity(body)-velocity)<.001f&&
              std::abs(world.RuntimeWheel(4)->state.angularVelocity-spin)<.001f&&
              std::abs(std::remainder(world.RuntimeWheel(4)->state.angle-angle,6.2831853f))<.001f&&
              std::abs(world.RuntimePowertrain(3)->absoluteSpin[0]-rotor)<.001,
              "cold process preserves driven body trajectory, wheel phase and actual rotor momentum");
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc==3)Cold(argv[1],argv[2]);else {SleepingAndOrder();UnorderedConstraints();PassiveEnergy();AuthoredAssemblies();TrailerAndSpeed();TerrainAndIntervals();TrafficAndTravel();checks+=RunVehicleTowingChecks();if(argc==2)Cold("write",argv[1]);}
        std::cout<<"SUMMARY "<<checks<<" checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
