#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "WorldPersistence.h"
#include "SaveService.h"
#include "SaveArchive.h"
#include "ScreenshotWriter.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <thread>

namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* message){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",message);std::fflush(stdout);}
std::string MaterialBytes(DeformableInstance* instance){SaveArchive archive;if(instance)instance->Persist(archive);return archive.bytes;}
std::string LocalMaterialBytes(DeformableInstance* instance,EntityId root,EntityId carrier){
 if(!instance)return {};
 auto copy=*instance;
 for(auto& attachment:copy.settings.attachments){if(attachment.target==root)attachment.target=1;else if(attachment.target==carrier)attachment.target=2;}
 return MaterialBytes(&copy);
}
std::string TrainBytes(PowertrainInstance* instance){SaveArchive archive;if(instance)instance->Persist(archive);return archive.bytes;}
double PlasticRest(const DeformableInstance* instance,glm::quat frame){
 double extent=0;if(instance)for(size_t i=0;i<instance->plasticRest.size();++i)
  for(int k=0;k<3;++k)extent=std::max(extent,glm::length(instance->plasticRest[i][k]-(glm::mat3_cast(glm::dquat(frame))*instance->asset->solids[i].rest)[k]));
 return extent;
}
void Dump(const std::filesystem::path& path,const std::string& bytes){std::ofstream file(path,std::ios::binary);file.write(bytes.data(),bytes.size());}
}

int main(int argc,char** argv){
 if(argc!=4)return 2;
 const std::string project=argv[1],mode=argv[2];const auto output=std::filesystem::absolute(argv[3]);
 std::filesystem::create_directories(output);std::string error;
 unsigned frame=0,ready=0,phase=0;bool frozen=false,completed=false,impactObserved=false;
 RuntimeWorld* original=nullptr;Window* window=nullptr;GameSnapshot snapshot;
 glm::vec3 carrierStart{0},carrierAxis{0};
 std::vector<glm::dvec3> intactBoundary;EntityId surfaceProbe=0,filteredProbe=0;double boundaryDisplacement=0;
 double plasticLoaded=0,strongLoaded=0,spinBefore=0;uint64_t region=0;EntityId streamed=0,spawned=0;
 std::string retainedMaterial,retainedDrive;std::vector<double> fixedTimes,frameTimes;
 auto frameStarted=std::chrono::steady_clock::now();
 auto key=[&](SDL_Scancode code,bool down){window->QueueTestPhysical(std::string("key:")+SDL_GetScancodeName(code),down?1.f:0.f);};
 ApplicationControl control;control.hidden=true;control.frameSeconds=[&](float){return frozen?0.f:1.f/60;};
 control.hostReady=[&](EngineHost& host){window=&host.GetWindow();window->SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string ignored;host.Audio().Init(ignored,true);};
 control.worldReady=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){original=&world;};
 control.beforeFrame=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){
  frameStarted=std::chrono::steady_clock::now();
  if(mode=="read"&&phase==1){key(SDL_SCANCODE_F8,true);phase=2;}
  else if(mode=="read"&&phase==2)key(SDL_SCANCODE_F8,false);
  if(mode=="read"&&phase==2&&&world!=original){frozen=true;phase=3;}
 };
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  ++frame;auto* plastic=world.RuntimeDeformable(102,error);auto* strong=world.RuntimeDeformable(202,error);
  auto* section=world.RuntimeDeformable(302,error);auto* drive=world.RuntimePowertrain(100);
  const bool healthy=world.Scripts()&&world.Scripts()->Diagnostics().empty();
  if(!healthy&&world.Scripts())for(const auto& diagnostic:world.Scripts()->Diagnostics())
   std::printf("SCRIPT %s %s\n",diagnostic.callback.c_str(),diagnostic.message.c_str());
  if(!ready&&plastic&&strong&&section&&drive&&drive->state.available&&healthy){
   ready=frame;const auto pose=world.Physics().GetTransform(world.RuntimeBody(101));
   carrierStart=pose.position;carrierAxis=pose.rotation*glm::vec3(1,0,0);
   intactBoundary=plastic->positions;
   Check(plastic->asset==strong->asset&&plastic->settings.material.shearCompliance!=strong->settings.material.shearCompliance,
    "ordinary async resources share immutable geometry with independently authored material strength");
   Check(world.RuntimeDefinition(103)->wheel->chassis==101&&world.RuntimeDefinition(503)->wheel->chassis==512,
    "bench and mobile vehicle wheels terminate in actual finite material-supported bearing bodies");
   Check(section->asset->fracture&&section->asset->fracture->structural&&!section->asset->fracture->rigid,
    "third powered bench uses existing M79 cohesive material load topology");
   if(mode=="read"){
    auto* service=world.SceneControl()->Saves(host.Resources());SaveStorage store(service->Root());bool recovered=false;
    Check(store.Read("damage-current",snapshot,recovered,error),"fresh process reads ordinary coupled-damage save bytes");phase=1;
   }
  }
  if(ready&&mode!="read"){
   const unsigned tick=frame-ready;
   if(tick>20){fixedTimes.push_back(play.LastFixedStepMilliseconds());frameTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStarted).count());}
   if(tick==80){
    size_t wheels=0,powertrains=0,materials=0,nodes=0,tets=0;
    for(const auto& definition:world.ScriptObjects(false)){
     wheels+=definition.wheel.has_value();powertrains+=definition.powertrain.has_value();
     if(definition.deformable)if(auto* instance=world.RuntimeDeformable(definition.id,error)){++materials;nodes+=instance->positions.size();tets+=instance->asset->solids.size();}
    }
    std::printf("WORKLOAD bodies=%zu dynamic_bodies=%zu wheels=%zu powertrains=%zu materials=%zu nodes=%zu tets=%zu material_substeps=%u material_iterations=%u fixed_hz=60\n",
      world.Physics().AliveBodyCount(),world.Physics().DynamicBodyCount(),wheels,powertrains,materials,nodes,tets,plastic->settings.substeps,plastic->settings.iterations);
    Check(healthy&&world.view.has_value()&&plastic->error.empty()&&strong->error.empty()&&section->error.empty(),
     "normal JS camera, input, UI and coupled physical assemblies execute without diagnostics");
    Check(!plastic->released[1]&&!strong->released[1]&&drive->state.links[0].active,
     "normal resting load leaves the authored mount and drive path intact");
    const auto gravity=world.Gravity().Sample(carrierStart);
    if(mode=="oblique")Check(std::abs(gravity.x)>5&&std::abs(gravity.y)>5,
     "identical mechanical content uses genuinely oblique authoritative gravity");
    if(mode=="lifecycle"){
     key(SDL_SCANCODE_P,true);region=world.SceneControl()->Streaming(world,error)->Request("annex",false,error);
    }else{key(SDL_SCANCODE_B,true);}
   }
   if(tick==81){key(SDL_SCANCODE_B,false);key(SDL_SCANCODE_P,false);}
   if(tick==260&&mode!="lifecycle"){
    plasticLoaded=PlasticRest(plastic,world.RuntimeDefinition(100)->transform.rotation);
    strongLoaded=PlasticRest(strong,world.RuntimeDefinition(200)->transform.rotation);
    const auto pose=world.Physics().GetTransform(world.RuntimeBody(101));
    const auto controlPose=world.Physics().GetTransform(world.RuntimeBody(201));
    const auto base=world.RuntimeDefinition(100)->transform.rotation;
    const auto axis=pose.rotation*glm::vec3(1,0,0),controlAxis=controlPose.rotation*glm::vec3(1,0,0);
    std::printf("LOADED plastic_rest_m=%.9g strong_rest_m=%.9g bearing_delta_m=%.9g alignment=%.9g control_alignment=%.9g contact=%d\n",
      plasticLoaded,strongLoaded,glm::length(pose.position-carrierStart),glm::dot(axis,carrierAxis),glm::dot(controlAxis,base*glm::vec3(1,0,0)),world.RuntimeWheel(103)->state.contact);
    Check(plasticLoaded>.001&&strongLoaded<plasticLoaded*.1&&plastic->error.empty(),
     "equal sustained physical loads yield soft material while stronger control retains its rest geometry");
    Check(glm::length(pose.position-carrierStart)>.01&&glm::dot(axis,carrierAxis)<.9999,
     "deformed material moves and rotates the real carrier rather than a renderer-only wheel mount");
    key(SDL_SCANCODE_B,true);
   }
   if(tick==261)key(SDL_SCANCODE_B,false);
   if(tick==340&&mode!="lifecycle"){
    Check(PlasticRest(plastic,world.RuntimeDefinition(100)->transform.rotation)>.001&&drive->state.links[0].active,
     "unloading preserves plastic material rest and leaves the still-connected mechanism operational");
    spinBefore=drive->state.shafts[0].angularVelocity;
    // Touch the CURRENT bent surface away from its end pins. A filtered-out
    // negative control starts at the same location with the same mass/gravity.
    // This would detect a visual-only bend or a rest-boundary collision proxy.
    glm::dvec3 center{0},normal{0};
    const auto frame=glm::dquat(world.RuntimeDefinition(100)->transform.rotation);
    for(const auto triangle:plastic->asset->triangles){
     const auto rest=(plastic->asset->nodes[triangle.x]+plastic->asset->nodes[triangle.y]+plastic->asset->nodes[triangle.z])/3.;
     if(std::abs(rest.x)>.35)continue;
     const auto point=(plastic->positions[triangle.x]+plastic->positions[triangle.y]+plastic->positions[triangle.z])/3.;
     const auto prior=(intactBoundary[triangle.x]+intactBoundary[triangle.y]+intactBoundary[triangle.z])/3.;
     const auto faceNormal=glm::normalize(glm::cross(plastic->positions[triangle.y]-plastic->positions[triangle.x],plastic->positions[triangle.z]-plastic->positions[triangle.x]));
     const double displacement=glm::length(point-prior);
     if(std::abs((glm::inverse(frame)*faceNormal).z)>.5&&displacement>boundaryDisplacement){boundaryDisplacement=displacement;center=point;normal=faceNormal;}
    }
    SceneObject probe;probe.name="Current material collision probe";probe.transform.position=glm::vec3(center+normal*.025);
    probe.body=SceneBodyComponent{};probe.body->motion=SceneBodyMotion::Dynamic;probe.body->shape=SceneShape::Sphere;
    probe.body->radius=.05f;probe.body->mass=.05f;probe.body->collisionLayer=0;probe.body->collisionMask=CategoryBit(1);
    surfaceProbe=world.CreateEntity(probe,nullptr,&error);probe.name="Filtered material collision control";probe.body->collisionMask=0;
    filteredProbe=world.CreateEntity(probe,nullptr,&error);
   }
   if(tick==341&&mode!="lifecycle"){
    const auto velocity=world.Physics().GetLinearVelocity(world.RuntimeBody(surfaceProbe)),controlVelocity=world.Physics().GetLinearVelocity(world.RuntimeBody(filteredProbe));
    std::printf("SURFACE displacement_m=%.9g contacts=%zu positive_speed=%.9g filtered_speed=%.9g response_delta=%.9g\n",boundaryDisplacement,plastic->stats.contacts,glm::length(velocity),glm::length(controlVelocity),glm::length(velocity-controlVelocity));
    Check(surfaceProbe&&filteredProbe&&boundaryDisplacement>.055&&plastic->stats.contacts>0&&glm::length(velocity-controlVelocity)>.01,
     "current deformed boundary transfers a physical contact impulse while an identical filtered-out probe does not");
    world.DestroyEntity(surfaceProbe);world.DestroyEntity(filteredProbe);
   }
   if(tick==341)key(SDL_SCANCODE_W,true);
   if(tick==430&&mode!="lifecycle"){
    std::printf("DRIVEN motor_radps=%.9g wheel_radps=%.9g link_torque_nm=%.9g body_speed_mps=%.9g\n",
      drive->state.shafts[0].angularVelocity,drive->state.shafts[1].angularVelocity,drive->state.links[0].torque,
      glm::length(world.Physics().GetLinearVelocity(world.RuntimeBody(101))));
    Check(std::abs(drive->state.sourceTorque[0])>.1&&drive->state.links[0].active&&std::abs(drive->state.shafts[1].angularVelocity)>.1,
     "actual source torque still reaches the misaligned wheel through the surviving mechanical link");
    key(SDL_SCANCODE_H,true);key(SDL_SCANCODE_I,true);
   }
   if(tick==431){key(SDL_SCANCODE_H,false);key(SDL_SCANCODE_I,false);}
   if(tick>430&&tick<570){
    std::vector<PhysicsWorld::BodyContactSample> contacts;
    if(world.Physics().ReadBodyContactSample(world.RuntimeBody(101),contacts))for(const auto& contact:contacts){
     const auto* other=world.RuntimeDefinition(world.EntityIdOfBody(contact.other));
     impactObserved|=other&&other->body&&other->body->mass==12&&contact.normalImpulse>0;
    }
   }
   if(tick==570&&mode!="lifecycle"){
    unsigned broken=0;for(auto failure:section->fracture.broken)broken+=failure!=0;
    std::printf("FAILURE released=%u section_broken=%u active=%d torque=%.9g command_enabled=%d motor_radps=%.9g wheel_radps=%.9g\n",
      unsigned(plastic->released[1]),broken,drive->state.links[0].active,drive->state.links[0].torque,
      drive->command.links[0].enabled,drive->state.shafts[0].angularVelocity,drive->state.shafts[1].angularVelocity);
    Check(plastic->released[1]&&!drive->state.links[0].active&&drive->command.links[0].enabled&&drive->state.links[0].torque==0,
     "overload irreversibly breaks a real mount and stops transmitted torque without gameplay disabling its link");
    Check(!strong->released[1]&&world.RuntimePowertrain(200)->state.links[0].active,
     "otherwise equivalent stronger material and mount change the physical outcome");
    Check(broken>0&&!world.RuntimePowertrain(300)->state.links[0].active,
     "M79 fracture interrupts the actual attachment-to-root material path and connected drive");
    Check(world.Physics().IsBodyEnabled(world.RuntimeBody(101))&&world.RuntimeWheel(103)->state.available,
     "detached carrier retains ordinary collision, wheel contact and finite rigid-body ownership");
    Check(impactObserved,"project-spawned finite ram strikes the carrier through actual solved rigid-body contact");
    key(SDL_SCANCODE_H,true);spinBefore=drive->state.shafts[0].angularVelocity;
   }
   if(tick==571)key(SDL_SCANCODE_H,false);
   if(tick==650&&mode!="lifecycle"){
    Check(drive->state.shafts[0].angularVelocity>spinBefore+.1&&!drive->state.links[0].active&&healthy,
     "post-failure source responds to removed reflected load while detached output remains disconnected");
    Check(plastic->error.empty()&&strong->error.empty()&&section->error.empty(),
     "coupled plastic and fractured material retains valid authoritative physical geometry");
    if(mode=="write"){key(SDL_SCANCODE_F5,true);phase=6;}
    else{std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);WriteRgbPng((output/"lab.png").string(),window->Width(),window->Height(),pixels);completed=true;world.UI().RequestQuit();}
   }
   if(tick==651)key(SDL_SCANCODE_F5,false);
   if(mode=="write"&&phase==6){
    auto* service=world.SceneControl()->Saves(host.Resources());for(uint64_t id=1;id<8;++id)if(const auto* status=service->Status(id)){
     if(status->state=="completed"){Check(true,"ordinary save transaction records yielded geometry, broken load paths and mechanical state");completed=true;world.UI().RequestQuit();phase=9;}
     if(status->state=="failed"){std::printf("SAVE %s\n",status->error.c_str());Check(false,"coupled-damage save transaction succeeds");completed=true;world.UI().RequestQuit();phase=9;}
    }
   }
   if(mode=="lifecycle"&&tick>100){
    auto* streaming=world.SceneControl()->Streaming(world,error);const auto status=streaming->Regions().front();
    if(!spawned)for(auto id:world.DefinitionIds())if(id!=100&&id!=200&&id!=300&&id!=500&&id!=700&&world.RuntimePowertrain(id)&&streaming->Owner(id)=="root"){
     spawned=id;const auto* owner=world.RuntimeDefinition(id);
     Check(owner&&owner->powertrain&&owner->powertrain->links.size()==4&&owner->powertrain->links[0].connection.owner!=520,
      "complete runtime prefab independently remaps material, bearing, wheel and physical connection identities");break;
    }
    if(phase==0&&status.state=="active"){
     streamed=streaming->Resolve("annex",1);auto* material=world.RuntimeDeformable(streaming->Resolve("annex",3),error);
     Check(streamed&&material&&world.RuntimePowertrain(streamed),"registered retained region publishes actual material-supported mechanical assembly");
     if(material){material->Impulse("right",{0,0,8});world.Physics().ApplyLinearImpulse(world.RuntimeBody(streaming->Resolve("annex",2)),{0,0,5});}
     phase=1;spinBefore=tick;
    }
    if(phase==1&&tick>spinBefore+70){
     auto* material=world.RuntimeDeformable(streaming->Resolve("annex",3),error);
     if(material){world.Physics().ApplyLinearImpulse(world.RuntimeBody(streaming->Resolve("annex",2)),{0,0,35});}
     phase=2;spinBefore=tick;
    }
    if(phase==2&&tick>spinBefore+35){
     auto* material=world.RuntimeDeformable(streaming->Resolve("annex",3),error);auto* train=world.RuntimePowertrain(streamed);
     Check(material&&material->released[1]&&train&&!train->state.links[0].active,
      "actual finite endpoint impact damages a streamed mount before retention");
     retainedMaterial=LocalMaterialBytes(material,streamed,streaming->Resolve("annex",2));retainedDrive=TrainBytes(train);Dump(output/"retained-material.bin",retainedMaterial);
     streaming->Release(region);frozen=true;phase=3;
    }
    if(phase==3&&status.state=="unloaded"){
     Check(!world.RuntimeDefinition(streamed)&&status.retained>0,"region suspension retires identities and retains durable damage without hidden simulation");
     region=streaming->Request("annex",false,error);phase=4;
    }
    if(phase==4&&status.state=="active"){
     const auto current=streaming->Resolve("annex",1);auto* material=world.RuntimeDeformable(streaming->Resolve("annex",3),error);
     auto* train=world.RuntimePowertrain(current);
     const auto carrier=streaming->Resolve("annex",2),materialOwner=streaming->Resolve("annex",3);
     Check(current&&current!=streamed&&LocalMaterialBytes(material,current,carrier)==retainedMaterial&&TrainBytes(train)==retainedDrive,
      "stream revisit preserves exact damaged geometry and drive commands with fresh remapped identities");
     Check(material&&material->settings.attachments[0].target==current&&material->settings.attachments[1].target==carrier
       &&train&&train->settings.links[0].connection.owner==materialOwner,
      "material support and transmission continuity references point to the new physical region entities");
     Check(material&&material->released[1]&&train&&!train->state.links[0].active,
      "retained physical mount and mechanically disconnected path do not heal on revisit");
     if(spawned){world.DestroyHierarchy(spawned,error);Check(!world.RuntimePowertrain(spawned),"hierarchy destruction retires material-supported prefab without stale endpoint ownership");}
     frozen=false;world.SceneControl()->Reload(error);phase=5;
    }
    if(phase==5&&play.FixedStepsSinceReset()<4){Check(healthy&&world.RuntimePowertrain(100),"ordinary reload cleans old damage, scripts and mechanical handles");completed=true;world.UI().RequestQuit();}
   }
  }
  if(mode=="read"&&phase==3){
   std::map<std::string,SaveChunk> chunks;
   if(WorldPersistence::Capture(world,chunks,error)){
    Check(play.FixedStepsSinceReset()==0,"cold-save comparison observes published physical state before resumed simulation");
    Check(chunks.at("deformables").data==snapshot.participants.at("deformables").data,
     "cold restoration preserves exact material positions, velocity, plastic rest and failed attachments");
    Check(chunks.at("powertrains").data==snapshot.participants.at("powertrains").data,
     "cold restoration preserves exact rotational momentum, commands and authored physical dependencies");
    Check(plastic->released[1]&&!drive->state.links[0].active&&drive->command.links[0].enabled,
     "restored physical connection stays broken without a cached script-status shortcut");
    Dump(output/"restored-deformables.bin",chunks.at("deformables").data);
    carrierStart=world.Physics().GetTransform(world.RuntimeBody(101)).position;spinBefore=drive->state.shafts[0].angularVelocity;
    frozen=false;key(SDL_SCANCODE_W,true);world.Physics().ApplyLinearImpulse(world.RuntimeBody(101),{0,0,2});ready=frame;phase=4;
   }else if(error!="loading"&&error.find("required script instance not initialized")==std::string::npos){
    std::printf("RESTORE %s\n",error.c_str());Check(false,"restored lifecycle initializes normally");completed=true;world.UI().RequestQuit();
   }
  }
  if(mode=="read"&&phase==4&&frame>ready+45){
   Check(world.Physics().GetTransform(world.RuntimeBody(101)).position!=carrierStart&&plastic->released[1]&&!drive->state.links[0].active&&healthy,
    "restored detached finite carrier physically responds to a new impulse while drive remains disconnected");
   completed=true;world.UI().RequestQuit();
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  if(frame>2200){Check(false,"bounded application deadline");world.UI().RequestQuit();}
 };
 // This regression retains the original bearing/load fixtures even when the
 // ordinary project changes its startup scene to the clearer crash lane.
 Application application;std::string argument=project;
 if(std::filesystem::path(project).extension()==".judasproj")
  argument=(std::filesystem::absolute(project).parent_path()/"Scenes/flat.judas").string();
 if(mode=="oblique")argument=(std::filesystem::absolute(project).parent_path()/"Scenes/oblique.judas").string();
 char name[]="judas";char* args[]={name,argument.data()};Check(application.Run(2,args,&control)==0&&completed,"normal application startup and shutdown");
 if(!fixedTimes.empty()){
  std::sort(fixedTimes.begin(),fixedTimes.end());std::sort(frameTimes.begin(),frameTimes.end());
  std::printf("PERF samples=%zu fixed_median_ms=%.6f p95_ms=%.6f max_ms=%.6f frame_median_ms=%.6f frame_p95_ms=%.6f\n",
    fixedTimes.size(),fixedTimes[fixedTimes.size()/2],fixedTimes[size_t(.95*(fixedTimes.size()-1))],fixedTimes.back(),frameTimes[frameTimes.size()/2],frameTimes[size_t(.95*(frameTimes.size()-1))]);
 }
 std::printf("M83 application %s checks=%u failures=%u\n",mode.c_str(),checks,failures);return failures?1:0;
}
