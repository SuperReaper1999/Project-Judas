// FTFT7: serialized scenes -> RuntimeWorld -> GameSession -> StepPlayedWorld.
// These fixtures add application-path evidence to the existing direct control,
// orbit and reference-frame suites. Input overrides only provide key states;
// they do not replace force accumulation, gravity selection or integration.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <glm/gtc/quaternion.hpp>
#include "GameSession.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "SimulationTiming.h"
#include "Window.h"

namespace {
namespace fs = std::filesystem;
int checks=0,failures=0,steps=0,fixtures=0;
constexpr float dt=SimulationTiming::kFixedTimestep;
constexpr float shipMass=80.0f;
const glm::vec3 halfExtents(.7f,.3f,1.1f);
struct Observation {std::string label;glm::dvec3 actual,expected;double tolerance,error;};
std::vector<Observation> observations;
void Check(bool ok,const std::string& label) {
    ++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';
}
void Vector(const std::string& label,glm::dvec3 actual,glm::dvec3 expected,double tolerance) {
    const double error=glm::length(actual-expected);observations.push_back({label,actual,expected,tolerance,error});
    std::cout<<std::setprecision(17)<<"VECTOR "<<label<<" actual "<<actual.x<<' '<<actual.y<<' '<<actual.z
             <<" expected "<<expected.x<<' '<<expected.y<<' '<<expected.z<<" error "<<error<<" tolerance "<<tolerance<<'\n';
    Check(std::isfinite(error)&&error<=tolerance,label);
}
struct Frame {
    std::string name;glm::quat rotation{1,0,0,0};glm::vec3 shift{0};glm::dvec3 origin{0};
    glm::vec3 Point(glm::vec3 x)const{return shift+rotation*x;}
    glm::vec3 Direction(glm::vec3 x)const{return rotation*x;}
};
glm::dvec3 Rotate(glm::quat q,glm::dvec3 x) {
    // Double reference rotation from the represented authored quaternion.
    const glm::dquat d=glm::normalize(glm::dquat(q));return d*x;
}
Scene SceneWithVehicle(const Frame& frame,glm::vec3 pilotOffset,float mass=shipMass) {
    Scene scene;scene.Settings().name="Spacecraft session "+frame.name;scene.Settings().worldOrigin=frame.origin;
    auto& body=scene.CreateObject("Craft");body.transform.position=frame.Point({0,0,0});body.transform.rotation=frame.rotation;
    body.body=SceneBodyComponent{};body.body->motion=SceneBodyMotion::Dynamic;body.body->halfExtents=halfExtents;
    body.body->mass=mass;body.body->friction=0;body.body->restitution=0;
    body.vehicle=SceneVehicleComponent{};body.vehicle->initialPilotAttached=true;
    auto& start=scene.CreateObject("Pilot");start.transform.position=frame.Point(pilotOffset);start.playerStart=ScenePlayerStartComponent{};
    return scene;
}
void AddUniform(Scene& scene,const Frame& frame,float magnitude) {
    auto& field=scene.CreateObject("Acceleration region");field.transform.position=frame.Point({0,0,0});field.transform.rotation=frame.rotation;
    field.gravity=SceneGravityComponent{};field.gravity->kind=SceneGravityKind::Uniform;field.gravity->magnitude=magnitude;field.gravity->regionRadius=3;
}
struct Played {
    RuntimeWorld world;GameSession session;Window window;Scene loaded;
    bool Begin(const Scene& scene,const fs::path& directory,const std::string& label) {
        ++fixtures;std::string text,error;bool ok=SaveSceneToString(scene,text);
        if(ok){std::ofstream file(directory/(label+".judas"));file<<text;ok=bool(file);}
        Check(ok,label+" ordinary authored serialization");
        if(!ok)return false;
        ok=LoadSceneFromString(text,loaded,error);Check(ok,label+" ordinary scene load: "+error);if(!ok)return false;
        window.SetTestInputMode(true);ok=world.Build(loaded,nullptr,error)&&session.Begin(world,error);
        Check(ok,label+" runtime/session creation: "+error);return ok;
    }
    BodyHandle Ship()const{return session.VehicleControl().handle;}
    void Step(int count=1){for(int i=0;i<count;++i){StepPlayedWorld(session,window,dt);++steps;}}
    void Input(bool sas=false){session.HandleFrameInput(window,false,false,false,false,sas);}
};
void ForceAndTorque(const Frame& frame,float mass,const fs::path& directory) {
    const std::string name="control-"+frame.name+"-"+std::to_string(int(mass));
    Scene scene=SceneWithVehicle(frame,{1.4f,1.7f,-.9f},mass);Played p;if(!p.Begin(scene,directory,name))return;
    auto& physics=p.world.Physics();const auto h=p.Ship();const auto before=physics.GetTransform(h);
    Check(p.session.IsPiloting()&&p.session.Attachment().attached,name+" authored attachment activates normal controls");
    p.window.SetTestActionState(Action::MoveForward,true);p.Input();
    Vector(name+" frame input does not write velocity",physics.GetLinearVelocity(h),{0,0,0},0);
    Check(physics.GetTransform(h).rotation==before.rotation,name+" frame input does not write orientation");
    p.Step();
    // Authored controller magnitude is 1200 N. Newton's impulse/mass oracle
    // uses body-local -Z independently of ApplyFlyingPrimitiveControl.
    const glm::dvec3 expectedV=Rotate(p.loaded.Objects()[0].transform.rotation,{0,0,-1})*(1200.*double(dt)/double(mass));
    Vector(name+" real thrust deltaV",physics.GetLinearVelocity(h),expectedV,2e-6);
    Vector(name+" force-driven drift",glm::dvec3(physics.GetTransform(h).position)-glm::dvec3(before.position),expectedV*double(dt),3e-6);
    p.window.SetTestActionState(Action::MoveForward,false);p.Input();p.Step(3);
    Vector(name+" release of thrust preserves inertial velocity",physics.GetLinearVelocity(h),expectedV,2e-6);
    p.window.SetTestActionState(Action::PitchUp,true);p.Input();p.Step();
    // Cuboid I_x=m*(b^2+c^2)/12, using full authored side lengths.
    const double b=2.*double(halfExtents.y),c=2.*double(halfExtents.z);
    const double inertia=double(mass)*(b*b+c*c)/12.;
    const glm::dvec3 expectedOmega=Rotate(p.loaded.Objects()[0].transform.rotation,{1,0,0})*(450.*double(dt)/inertia);
    Vector(name+" real torque through box inertia",physics.GetAngularVelocity(h),expectedOmega,2e-6);
    Vector(name+" torque leaves linear momentum unchanged",physics.GetLinearVelocity(h),expectedV,2e-6);
    p.window.SetTestActionState(Action::PitchUp,false);p.Input();p.Step(3);
    Vector(name+" release of torque preserves angular coast",physics.GetAngularVelocity(h),expectedOmega,2e-6);
    Check(physics.LastStepContactCount()==0,name+" control fixture remains collision-free");
}
void Sas(const Frame& frame,const fs::path& directory) {
    const std::string name="sas-"+frame.name;Played p;if(!p.Begin(SceneWithVehicle(frame,{1.4f,1.7f,-.9f}),directory,name))return;
    auto& physics=p.world.Physics();const auto h=p.Ship();
    const glm::vec3 v=frame.Direction({1,-.2f,.4f}),omega=frame.Direction({.18f,-.24f,.3f});
    physics.SetLinearVelocity(h,v);physics.SetAngularVelocity(h,omega);const glm::quat target=physics.GetTransform(h).rotation;
    p.window.RequestTestSasToggle();p.Input(p.window.ConsumeSasToggleRequest());
    Check(p.session.VehicleControl().sasEnabled,name+" ordinary frame-input toggles SAS");
    Check(physics.GetTransform(h).rotation==target,name+" enabling SAS does not snap orientation");
    Vector(name+" enabling SAS does not overwrite spin",physics.GetAngularVelocity(h),glm::dvec3(omega),0);
    p.window.SetTestActionState(Action::YawLeft,true);p.Step();
    // Captured target initially equals current attitude, so only the documented
    // -10*omega angular-acceleration damping acts. This is torque/inertia, not a
    // replacement orientation or angular-rate assignment in the test.
    Vector(name+" first SAS impulse",physics.GetAngularVelocity(h),glm::dvec3(omega)*(1.-10.*double(dt)),2e-6);
    const double moved=glm::length(glm::dquat(physics.GetTransform(h).rotation)-glm::dquat(target));
    Check(moved>1e-5&&moved<.02,name+" finite torque step moves continuously instead of snapping");
    Vector(name+" SAS leaves translation inertial",physics.GetLinearVelocity(h),glm::dvec3(v),2e-6);
    p.Step(239);p.window.SetTestActionState(Action::YawLeft,false);
    Vector(name+" SAS damps spin through ordinary repeated steps",physics.GetAngularVelocity(h),{0,0,0},2e-4);
    Check(std::abs(glm::dot(physics.GetTransform(h).rotation,target))>1.-2e-6,name+" SAS restores captured attitude with turn input held");
    Vector(name+" long SAS hold preserves translation",physics.GetLinearVelocity(h),glm::dvec3(v),2e-6);
    p.window.RequestTestControlToggle();p.Input();
    Check(!p.session.IsPiloting()&&!p.session.Attachment().attached&&p.session.VehicleControl().sasEnabled,name+" SAS survives ordinary pilot release");
    physics.SetAngularVelocity(h,omega);p.Step(120);
    Check(glm::length(physics.GetAngularVelocity(h))<.01f,name+" released craft still receives SAS counter-torque");
}
void GravityContracts(const Frame& frame,const fs::path& directory) {
    for(bool fixedSource:{false,true}) {
        const std::string name=(fixedSource?"celestial-static-":"local-mutual-")+frame.name;
        Scene scene=SceneWithVehicle(frame,{1.4f,1.7f,-.9f});AddUniform(scene,frame,2.3f);
        scene.Objects()[0].vehicle->gravity=fixedSource?SceneVehicleGravity::Celestial:SceneVehicleGravity::Local;
        auto& source=scene.CreateObject("Gravity source");const auto sourceId=source.id;
        source.transform.position=frame.Point({-8,0,0});source.body=SceneBodyComponent{};source.body->shape=SceneShape::Sphere;
        source.body->radius=.25f;source.body->mass=1.0e11f;source.body->motion=fixedSource?SceneBodyMotion::Static:SceneBodyMotion::Dynamic;
        source.celestial=SceneCelestialComponent{};source.celestial->gravitationalParameter=6.67430f;
        Played p;if(!p.Begin(scene,directory,name))continue;
        const auto h=p.Ship();auto& physics=p.world.Physics();const glm::dvec3 shipPosition(physics.GetTransform(h).position);
        const auto* loadedSource=p.loaded.Find(sourceId);const glm::dvec3 displacement=glm::dvec3(loadedSource->transform.position)-shipPosition;
        const double distance=glm::length(displacement);
        // Newtonian 1/r^2 oracle in double from represented scene inputs; never
        // calls the production field sampler or ForceOnB helper.
        const double mu=fixedSource?double(loadedSource->celestial->gravitationalParameter):double(6.67430e-11f)*double(loadedSource->body->mass);
        const glm::dvec3 central=displacement*(mu/(distance*distance*distance));
        const glm::dvec3 local=Rotate(p.loaded.Objects()[2].transform.rotation,{0,-double(2.3f),0});
        const glm::dvec3 expected=(central+(fixedSource?glm::dvec3(0):local))*double(dt);
        BodyHandle other;if(!fixedSource){const auto* e=p.world.FindEntity(sourceId);other=p.world.DynamicBodies()[e->slot].Handle();}
        p.Step();Vector(name+" scene gravity deltaV",physics.GetLinearVelocity(h),expected,2e-7);
        Check(glm::length(local*double(dt))>.03,name+" absent/present local-field contribution is material");
        if(!fixedSource) {
            const glm::dvec3 expectedOther=-central*(double(shipMass)*double(dt)/double(loadedSource->body->mass));
            Vector(name+" independently predicted reciprocal source deltaV",physics.GetLinearVelocity(other),expectedOther,2e-18);
            const glm::dvec3 total=double(shipMass)*glm::dvec3(physics.GetLinearVelocity(h))+double(loadedSource->body->mass)*glm::dvec3(physics.GetLinearVelocity(other));
            Vector(name+" pair momentum closes after local external impulse",total,double(shipMass)*local*double(dt),2e-5);
        } else {
            Check(p.world.PointMassSources().size()==1,name+" fixed source remains prescribed one-way celestial field");
        }
        Check(physics.LastStepContactCount()==0,name+" gravity fixture has no contacts");
    }
}
void Release(const Frame& frame,bool gravityClearance,const fs::path& directory) {
    const std::string name="release-"+frame.name+(gravityClearance?"-clearance":"-zero-g");
    const glm::vec3 offset=gravityClearance?glm::vec3(.25f,.35f,-.2f):glm::vec3(1.4f,1.7f,-.9f);
    Scene scene=SceneWithVehicle(frame,offset);if(gravityClearance)AddUniform(scene,frame,2.3f);
    Played p;if(!p.Begin(scene,directory,name))return;
    auto& physics=p.world.Physics();const auto h=p.Ship();
    const glm::vec3 velocity=frame.Direction({2,-.4f,.7f}),omega=frame.Direction({.2f,-.3f,.5f});
    physics.SetLinearVelocity(h,velocity);physics.SetAngularVelocity(h,omega);
    const glm::dquat initialRotation=glm::normalize(glm::dquat(physics.GetTransform(h).rotation));
    const glm::dvec3 initialRelative=glm::inverse(initialRotation)*(glm::dvec3(p.session.Player().GetPosition())-glm::dvec3(physics.GetTransform(h).position));
    p.Step(5);const BodyTransform pose=physics.GetTransform(h);const glm::vec3 player=p.session.Player().GetPosition();
    const glm::dvec3 expectedPosition=glm::dvec3(pose.position)+glm::normalize(glm::dquat(pose.rotation))*initialRelative;
    Vector(name+" attached point follows moving/rotating authoritative frame",player,expectedPosition,5e-6);
    // Read actual represented state at the instant before detachment; the
    // independent kinematic point-velocity oracle does not call pilot helpers.
    const glm::dvec3 r=glm::dvec3(player)-glm::dvec3(pose.position);
    const glm::dvec3 linear(physics.GetLinearVelocity(h)),angular(physics.GetAngularVelocity(h));
    const glm::dvec3 expectedVelocity=linear+glm::cross(angular,r);
    Check(glm::length(glm::cross(angular,r))>.02,name+" angular point velocity is a material nonzero term");
    p.window.RequestTestControlToggle();p.Input();
    Check(!p.session.IsPiloting()&&!p.session.Attachment().attached,name+" actual frame-input release changes attachment/control state");
    Vector(name+" dismount inherits velocity at original attachment point",p.session.Player().GetVelocity(),expectedVelocity,3e-6);
    Vector(name+" dismount does not kick ship COM",physics.GetLinearVelocity(h),linear,0);
    Vector(name+" dismount does not kick ship spin",physics.GetAngularVelocity(h),angular,0);
    const glm::vec3 released=p.session.Player().GetPosition();
    if(gravityClearance) {
        Check(glm::length(released-player)>.5f,name+" production clearance actually moved the player");
        const glm::dvec3 wrong=linear+glm::cross(angular,glm::dvec3(released)-glm::dvec3(pose.position));
        Check(glm::length(wrong-expectedVelocity)>.1,name+" new clearance offset would give a detectably wrong release velocity");
    } else Vector(name+" zero-gravity release adds no artificial clearance",released,glm::dvec3(player),0);
}
void JsonVector(std::ostream& out,const glm::dvec3& v){out<<'['<<v.x<<','<<v.y<<','<<v.z<<']';}
}
int main(int argc,char**argv) {
    const auto start=std::chrono::steady_clock::now();fs::path directory;
    if(argc==3&&std::string(argv[1])=="--output")directory=argv[2];
    else if(argc==1)directory=fs::temp_directory_path()/("judas-ftft7-spacecraft-"+std::to_string(start.time_since_epoch().count()));
    else {std::cerr<<"usage: judas_spacecraft_frame_integration_tests [--output fresh-directory]\n";return 2;}
    if(fs::exists(directory)&&!fs::is_empty(directory)){std::cerr<<"Refusing to overwrite spacecraft evidence\n";return 2;}
    fs::create_directories(directory);
    const std::vector<Frame> frames{
        {"identity",glm::quat(1,0,0,0),glm::vec3(0),glm::dvec3(0)},
        {"rotated",glm::angleAxis(.83f,glm::normalize(glm::vec3(1,-2,3))),glm::vec3(0),glm::dvec3(0)},
        {"translated-far",glm::angleAxis(-.59f,glm::normalize(glm::vec3(2,1,-1))),glm::vec3(10,-7,5),glm::dvec3(1e12,-2e12,3e12)}};
    for(const auto& frame:frames){ForceAndTorque(frame,80,directory);ForceAndTorque(frame,240,directory);Sas(frame,directory);GravityContracts(frame,directory);Release(frame,false,directory);Release(frame,true,directory);}
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::ostringstream out;out<<std::setprecision(17)<<"{\"fixtures\":"<<fixtures<<",\"checks\":"<<checks<<",\"failures\":"<<failures<<",\"fixed_steps\":"<<steps<<",\"seconds\":"<<seconds<<",\"pass\":"<<(failures?"false":"true")<<",\"observations\":[";
    for(std::size_t i=0;i<observations.size();++i){if(i)out<<',';const auto& o=observations[i];out<<"{\"label\":\""<<o.label<<"\",\"actual\":";JsonVector(out,o.actual);out<<",\"expected\":";JsonVector(out,o.expected);out<<",\"error\":"<<o.error<<",\"tolerance\":"<<o.tolerance<<'}';}
    out<<"]}\n";std::ofstream(directory/"results.json")<<out.str();std::cout<<"SUMMARY fixtures="<<fixtures<<" checks="<<checks<<" failures="<<failures<<" fixed_steps="<<steps<<" seconds="<<seconds<<'\n';return failures?1:0;
}
