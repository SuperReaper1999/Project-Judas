#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "SaveArchive.h"
#include "UniformGravity.h"
#include "ResourceManager.h"
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cstdio>

namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* label){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",label);}
Scene Fixture(glm::quat rotation){
 Scene scene;scene.Settings().worldOrigin={0,6371328,0};
 auto add=[&](uint64_t id,glm::vec3 p){SceneObject o;o.id=id;o.name="assembly";o.transform.position=rotation*p;o.transform.rotation=rotation;scene.Objects().push_back(o);return &scene.Objects().back();};
 auto* floor=add(1,{0,-.5f,0});floor->body=SceneBodyComponent{};floor->body->halfExtents={40,.5,40};
 auto* gravity=add(2,{0,0,0});gravity->gravity=SceneGravityComponent{};gravity->gravity->kind=SceneGravityKind::Uniform;gravity->gravity->regionRadius=1e6;
 auto* chassis=add(3,{0,1,3});chassis->body=SceneBodyComponent{};chassis->body->motion=SceneBodyMotion::Dynamic;chassis->body->mass=600;chassis->body->halfExtents={.8f,.25f,1.4f};
 PowertrainSettings train;PowertrainShaft rotor;rotor.name="rotor";rotor.inertia=2;rotor.bearing=3;train.shafts.push_back(rotor);
 PowertrainSource source;source.name="motor";source.kind=PowertrainSourceKind::Custom;train.sources.push_back(source);
 for(unsigned i=0;i<4;++i){auto* wheel=add(10+i,{0,0,0});wheel->wheel=WheelSettings{};auto& w=*wheel->wheel;w.chassis=3;w.mount={i%2?.85f:-.85f,-.1f,i<2?1.f:-1.f};w.radius=.32f;w.restLength=.55f;w.maxLength=.75f;w.springStiffness=14000;w.damping=1800;
  PowertrainShaft shaft;shaft.name="wheel"+std::to_string(i);shaft.kind=PowertrainShaftKind::Wheel;shaft.target=10+i;train.shafts.push_back(shaft);
  PowertrainLink gear;gear.name=shaft.name;gear.input=0;gear.output=i+1;train.links.push_back(gear);
 }
 scene.Find(3)->powertrain=train;
 auto* motor=add(20,{4,.94f,0});motor->characterMotor=CharacterMotorSettings{};
 auto* prop=add(21,{-4,1,0});prop->body=SceneBodyComponent{};prop->body->motion=SceneBodyMotion::Dynamic;prop->body->mass=3;prop->body->halfExtents={.4f,.4f,.4f};
 auto* hinge=add(22,{-4,1,0});hinge->joint=SceneJointComponent{};hinge->joint->bodyA=21;hinge->joint->settings.type=JointType::Hinge;
 auto* emitter=add(23,{1,2,0});emitter->particleEmitter=ParticleEmitterSettings{};emitter->particleEmitter->burst=8;emitter->particleEmitter->rate=0;
 scene.SetNextId(100);return scene;
}
std::string TrainBytes(PowertrainInstance* train){SaveArchive a;train->Persist(a);return a.bytes;}
void Step(RuntimeWorld& world,float dt){
 world.FixedScripts(nullptr,dt);
 for(auto id:{3ull,21ull})world.Physics().ApplyLinearAcceleration(world.RuntimeBody(id),world.SampleEntityGravity(id,world.Physics().GetTransform(world.RuntimeBody(id)).position),dt);
 world.UpdatePowertrains(dt);world.UpdateWheels(dt);world.ResolvePowertrainLoads(dt);world.Physics().Step(dt);world.PublishWheels();world.PublishPowertrains(dt);SyncDynamicBodiesFromPhysics(world.DynamicBodies(),world.Physics());world.UpdateCharacters(dt);world.UpdateVisualParticles(dt);
}
void Assemblies(glm::quat rotation){
 std::string error;RuntimeWorld control,shifted;control.legacyGameplay=shifted.legacyGameplay=false;auto scene=Fixture(rotation);
 Check(control.Build(scene,nullptr,error)&&shifted.Build(scene,nullptr,error),"independent matched mechanical worlds build");
 if(!control.RuntimePowertrain(3)||!shifted.RuntimePowertrain(3)){std::puts(error.c_str());return;}
 control.RuntimePowertrain(3)->command.sources[0].customTorque=60;shifted.RuntimePowertrain(3)->command.sources[0].customTorque=60;
 for(unsigned i=0;i<80;++i){Step(control,1.f/60);Step(shifted,1.f/60);}
 auto body=shifted.RuntimeBody(3);auto motor=shifted.RuntimeBody(20);const auto before=shifted.Physics().GetTransform(body);auto spin=TrainBytes(shifted.RuntimePowertrain(3));auto wheel=shifted.RuntimeWheel(10)->state;const auto initialOrigin=shifted.Settings().worldOrigin;
 const glm::dvec3 delta(512,-512,512);auto requested=initialOrigin+delta;
 Check(shifted.RequestOrigin(requested,error),"complete vehicle, motor, particles and constraints accept origin request");
 if(!error.empty()){std::puts(error.c_str());return;}
 Check(shifted.CommitOrigin(),"native origin transaction commits");
 Check(shifted.RuntimeBody(3).id==body.id&&shifted.RuntimeBody(20).id==motor.id,"body and query-capsule generations survive without recreation");
 Check(glm::length(glm::dvec3(shifted.Physics().GetTransform(body).position)+delta-glm::dvec3(before.position))<.0002,"absolute chassis endpoint is invariant");
 Check(spin==TrainBytes(shifted.RuntimePowertrain(3)),"mechanical command and rotor continuation bytes are unchanged");
 Check(glm::length(glm::dvec3(shifted.RuntimeWheel(10)->state.point)+delta-glm::dvec3(wheel.point))<.0002&&shifted.RuntimeWheel(10)->state.body.id==wheel.body.id,"wheel contact point translates while contact identity remains");
 Check(glm::length(glm::dvec3(shifted.RuntimeWheel(10)->previous.center)+delta-glm::dvec3(control.RuntimeWheel(10)->previous.center))<.0002,"wheel presentation history translates");
 double maxPosition=0,maxVelocity=0;std::vector<double> times;
 for(unsigned i=0;i<180;++i){Step(control,1.f/60);auto start=std::chrono::steady_clock::now();Step(shifted,1.f/60);times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
  for(auto id:{3ull,20ull,21ull}){auto a=control.RuntimeBody(id),b=shifted.RuntimeBody(id);maxPosition=std::max(maxPosition,glm::length(glm::dvec3(shifted.Physics().GetTransform(b).position)+delta-glm::dvec3(control.Physics().GetTransform(a).position)));maxVelocity=std::max(maxVelocity,double(glm::length(shifted.Physics().GetLinearVelocity(b)-control.Physics().GetLinearVelocity(a))));}
 }
 Check(maxPosition<.025&&maxVelocity<.06,"continued real wheel/hinge/motor simulation agrees with unshifted control");
 Check(glm::length(control.Physics().GetTransform(control.RuntimeBody(3)).position-scene.Find(3)->transform.position)>.3f,"positive control actually drives rather than remaining inert");
 Check(shifted.RuntimeCharacter(20)->result.supported,"CharacterMotor retains actual floor support");
 std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(shifted,chunks,error),"shifted assembly full save captures");Scene saved;Check(WorldPersistence::SceneFrom(chunks,saved,error),"saved chart reconstructs");RuntimeWorld cold;cold.legacyGameplay=false;
 const bool restored=cold.Build(saved,nullptr,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(cold,error)&&WorldPersistence::Restore(cold,chunks,error);
 Check(restored,"fresh world restores shifted vehicle and constraints");if(!restored)std::puts(error.c_str());
 if(restored){Check(TrainBytes(cold.RuntimePowertrain(3))==TrainBytes(shifted.RuntimePowertrain(3)),"fresh save retains exact powertrain spin/commands");Check(glm::length(cold.Physics().GetTransform(cold.RuntimeBody(3)).position-shifted.Physics().GetTransform(body).position)<.0001f,"restoration checks physical endpoint, not cached status");Step(cold,1.f/60);Check(cold.RuntimeWheel(10)->state.available,"restored wheels continue on actual geometry");}
 const auto revision=shifted.OriginRevision();const auto pose=shifted.Physics().GetTransform(body).position;
 Check(!shifted.RequestOrigin(requested+glm::dvec3(50000,0,0),error)&&shifted.OriginRevision()==revision&&shifted.Physics().GetTransform(body).position==pose,"unsafe distant chart rejected atomically");
 Check(!shifted.RequestOrigin({NAN,0,0},error),"nonfinite chart rejected");
 std::sort(times.begin(),times.end());std::printf("MEASURE position_error_m=%.9g velocity_error_mps=%.9g median_ms=%.6f p95_ms=%.6f worst_ms=%.6f bodies=%zu\n",maxPosition,maxVelocity,times[times.size()/2],times[times.size()*95/100],times.back(),shifted.Physics().AliveBodyCount());
}
void Navigation(){
 std::string error;Scene scene;SceneObject floor;floor.id=1;floor.body=SceneBodyComponent{};floor.body->halfExtents={8,.5f,8};floor.transform.position={0,-.5f,0};scene.Objects().push_back(floor);SceneObject surface;surface.id=2;surface.navigationSurface=NavigationSurfaceSettings{};surface.navigationSurface->halfExtents={10,4,10};surface.navigationSurface->minRegion=1;scene.Objects().push_back(surface);
 scene.SetNextId(100);NavigationData data;Check(BakeNavigation(scene,surface,{},data,error),"real navigation asset bakes");std::filesystem::create_directories(".cache/m90/navigation/Assets");Check(SaveNavigation(".cache/m90/navigation/Assets/surface.judasnav",data,error),"navigation fixture publishes a real asset");AssetDatabase assets;assets.Scan(std::filesystem::absolute(".cache/m90/navigation").string(),std::filesystem::absolute(".cache/m90/navigation/Assets").string());AssetRecord asset;if(auto* existing=assets.FindByRelativePath("Assets/surface.judasnav"))asset=*existing;else Check(assets.Track(std::filesystem::absolute(".cache/m90/navigation/Assets/surface.judasnav").string(),asset,error),"navigation asset registered");scene.Find(2)->navigationSurface->asset=asset.id;ResourceManager resources(nullptr,&assets);ProjectNavigation config;RuntimeWorld world;world.legacyGameplay=false;const bool built=world.Build(scene,&resources,error,nullptr,&config);Check(built,"navigation world builds");if(!built){std::puts(error.c_str());return;}Check(world.Navigation().LoadSurface(2,{},std::make_shared<NavigationData>(data),error),"surface-local Detour data installs");
 world.Navigation().RegisterAgent(10);auto* a=world.Navigation().Agent(10);a->destination={5,0,0};a->hasDestination=true;a->path=world.Navigation().FindPath({-5,0,0},a->destination);const auto revision=world.Navigation().Revision();const auto corners=a->path.corners.size();const glm::vec3 delta(512,512,-512);
 Check(world.RequestOrigin(delta,error)&&world.CommitOrigin(),"active navigation surface and corridor accept shift");auto path=world.Navigation().FindPath(glm::vec3(-5,0,0)-delta,glm::vec3(5,0,0)-delta);
 Check(path.status==NavigationPath::Status::Complete&&path.distance>9.8f,"translated Detour path matches real translated floor");
 Check(a->path.corners.size()==corners&&world.Navigation().Revision()==revision&&glm::length(a->destination+delta-glm::vec3(5,0,0))<.0001f,"navigation intent and revision survive without repath/reset");
 Check(!world.Navigation().Sample({0,0,0},1),"negative query at old chart position no longer hits surface");
}
}
int main(){Assemblies({1,0,0,0});Assemblies(glm::angleAxis(.7f,glm::normalize(glm::vec3(1,2,3))));Navigation();std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;}
