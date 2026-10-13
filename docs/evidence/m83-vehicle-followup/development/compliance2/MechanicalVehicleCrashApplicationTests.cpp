#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldPersistence.h"
#include "SaveService.h"
#include "SaveStorage.h"
#include "ScreenshotWriter.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <thread>

// This is a new end-to-end contract, not a relaxation of the earlier four
// bearing fixture. Only ordinary logical input drives the authored vehicle.
namespace {
unsigned checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);std::fflush(stdout);}
struct Observation {
 EntityId root=0,nose=0,material=0,wall=0;
 glm::vec3 initial{0},forward{0};glm::dvec3 initialSpan{0},cutRelativeNose{0};
 std::vector<glm::dmat3> initialPlastic{};
 std::vector<glm::dvec3> initialShape{};
 std::vector<std::array<double,6>> initialEdges{};
 double distance=0,approachSpeed=0,wallImpulse=0,plasticBefore=0,plasticImpact=0,shapeBefore=0,shapeImpact=0;
 double nonrigidBefore=0,nonrigidBeforeCut=0,nonrigidImpact=0,plasticBeforeCut=0;
 double compression=0,separation=0,tension=0,shear=0,peakForce=0;
 double noseMass=0;unsigned contactTick=0,cutTick=0,physicalBonds=0,scriptBonds=0;
 bool unhealthy=false,attachmentReleased=false,frontCommandHeld=true,rearPathSurvives=true;
};
glm::dvec3 GroupCenter(const DeformableInstance& material,const char* name){
 const auto found=material.asset->groups.find(name);if(found==material.asset->groups.end()||found->second.empty())return {};
 glm::dvec3 point{0};for(auto node:found->second)point+=material.positions[node];return point/double(found->second.size());
}
bool PatchesConnected(const DeformableInstance& material){
 const auto* cook=material.asset->fracture.get();if(!cook)return false;
 std::vector<unsigned> rearComponents,frontComponents;
 auto components=[&](const char* group,std::vector<unsigned>& values){
  auto found=material.asset->groups.find(group);if(found==material.asset->groups.end())return;
  for(auto node:found->second){const auto part=cook->nodePart[node];if(!material.fracture.removed[part])values.push_back(material.fracture.component[part]);}
 };
 components("rear",rearComponents);components("front",frontComponents);
 if(rearComponents.empty()||frontComponents.empty())return false;
 for(auto rear:rearComponents)for(auto front:frontComponents)if(rear==front)return true;
 return false;
}
double PlasticChange(const DeformableInstance& material,const Observation& observation){
 double result=0;for(size_t element=0;element<material.plasticRest.size()&&element<observation.initialPlastic.size();++element)
  for(unsigned column=0;column<3;++column)result=std::max(result,glm::length(material.plasticRest[element][column]-observation.initialPlastic[element][column]));
 return result;
}
double ShapeChange(const DeformableInstance& material,const BodyTransform& root,const Observation& observation){
 double result=0;for(size_t n=0;n<material.positions.size()&&n<observation.initialShape.size();++n){
  const auto local=glm::inverse(glm::dquat(root.rotation))*(material.positions[n]-glm::dvec3(root.position));
  result=std::max(result,glm::length(local-observation.initialShape[n]));
 }return result;
}
std::array<double,6> TetEdges(const DeformableInstance& material,const glm::uvec4& nodes){
 std::array<double,6> lengths{};unsigned edge=0;
 for(unsigned a=0;a<4;++a)for(unsigned b=a+1;b<4;++b)lengths[edge++]=glm::length(material.positions[nodes[a]]-material.positions[nodes[b]]);
 return lengths;
}
double NonrigidChange(const DeformableInstance& material,const Observation& observation){
 double result=0;for(size_t element=0;element<material.asset->solids.size()&&element<observation.initialEdges.size();++element){
  const auto edges=TetEdges(material,material.asset->solids[element].nodes);
  for(unsigned edge=0;edge<6;++edge)result=std::max(result,std::abs(edges[edge]-observation.initialEdges[element][edge]));
 }return result;
}
bool Contact(RuntimeWorld& world,EntityId body,EntityId other,double& peak){
 std::vector<PhysicsWorld::BodyContactSample> contacts;bool actual=false;
 if(world.Physics().ReadBodyContactSample(world.RuntimeBody(body),contacts))for(const auto& contact:contacts)
  if(world.EntityIdOfBody(contact.other)==other){peak=std::max(peak,double(contact.normalImpulse));actual|=contact.normalImpulse>0;}
 return actual;
}
bool LiveNose(RuntimeWorld& world,const Observation& observation){
 const auto body=world.RuntimeBody(observation.nose);Shape shape;BodyTransform pose;
 return world.Physics().IsBodyEnabled(body)&&world.Physics().IsDynamicBody(body)&&
  std::abs(world.Physics().GetMass(body)-observation.noseMass)<1e-6&&world.Physics().GetBodyShape(body,shape,pose)&&
  world.RuntimeWheel(observation.root+1)&&world.RuntimeWheel(observation.root+2)&&
  world.RuntimeWheel(observation.root+1)->settings.chassis==observation.nose&&
  world.RuntimeWheel(observation.root+2)->settings.chassis==observation.nose&&
  world.RuntimeWheel(observation.root+1)->state.available&&world.RuntimeWheel(observation.root+2)->state.available;
}
bool FrontCut(const PowertrainInstance& train,EntityId material){
 unsigned front=0;for(size_t i=0;i<train.settings.links.size();++i)if(train.settings.links[i].connection.owner==material){
  ++front;if(train.state.links[i].connected||train.state.links[i].active||std::abs(train.state.links[i].torque)>1e-9||!train.command.links[i].enabled)return false;
 }return front==2;
}
bool RearIntact(const PowertrainInstance& train){
 unsigned rear=0;for(size_t i=0;i<train.settings.links.size();++i)if(!train.settings.links[i].connection.owner){
  ++rear;if(!train.state.links[i].connected||!train.command.links[i].enabled)return false;
 }return rear==2;
}
void Dump(const std::filesystem::path& path,const std::string& bytes){std::ofstream file(path,std::ios::binary);file.write(bytes.data(),bytes.size());}
}

int main(int argc,char** argv){
 if(argc!=4)return 2;
 const std::string scene=argv[1],mode=argv[2];const bool probe=mode=="probe",watch=mode=="watch",drive=mode=="drive"||probe||mode=="write"||watch,read=mode=="read";
 const unsigned endTick=watch?2100:1200; // Allow the ordinary tour's own 30 s stop policy to complete.
 if(!drive&&!read&&mode!="rest")return 2;
 const auto output=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(output);
 std::ofstream trace(output/"vehicle-trace.csv");trace<<std::setprecision(12)
  <<"tick,root,distance_m,approach_speed_mps,current_forward_mps,root_x,root_y,root_z,root_up_x,root_up_y,root_up_z,nose_x,nose_y,nose_z,nose_speed_mps,nose_angular_radps,nose_wall_step,nose_wall_peak_ns,source_input,source_torque_nm,transmission_torque_nm,plastic_rest_change_m,shape_change_m,nonrigid_edge_change_m,compression_m,separation_m,physical_bonds,script_bonds,patches_connected,front_connected,rear_connected,max_strain,min_jacobian,patch_peak_n,tensile_peak_pa,shear_peak_pa,material_error\n";
 std::ofstream wheelTrace(output/"wheel-trace.csv");wheelTrace<<std::setprecision(12)
  <<"tick,root,wheel,available,contact,length_m,normal_force_n,rpm,longitudinal_slip_mps,longitudinal_impulse_ns,lateral_impulse_ns\n";
 std::array<Observation,2> observations{{{900,910,920,1500},{1100,1110,1120,1501}}};
 Window* window=nullptr;RuntimeWorld* original=nullptr;GameSnapshot snapshot;
 unsigned frame=0,ready=0,phase=0;bool complete=false,frozen=false,equalInput=true,approachPicture=false,cutPicture=false;
 std::vector<double> fixedTimes,frameTimes;auto frameStarted=std::chrono::steady_clock::now();
 auto key=[&](const char* name,bool down){window->QueueTestPhysical(std::string("key:")+name,down?1.f:0.f);};
 ApplicationControl control;control.hidden=true;control.frameSeconds=[&](float){return frozen?0.f:1.f/60;};
 control.hostReady=[&](EngineHost& host){window=&host.GetWindow();window->SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string ignored;host.Audio().Init(ignored,true);};
 control.worldReady=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){original=&world;};
 control.beforeFrame=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){
  frameStarted=std::chrono::steady_clock::now();
  if(read&&phase==1){key("F8",true);phase=2;}else if(read&&phase==2)key("F8",false);
  if(read&&phase==2&&&world!=original){frozen=true;phase=3;}
 };
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  ++frame;std::string error;bool available=world.Scripts()&&world.Scripts()->Diagnostics().empty();
  for(const auto& o:observations){const auto* train=world.RuntimePowertrain(o.root);auto* material=world.RuntimeDeformable(o.material,error);
   available&=train&&train->state.available&&material&&material->asset->fracture&&
    world.Physics().IsBodyEnabled(world.RuntimeBody(o.root))&&world.Physics().IsBodyEnabled(world.RuntimeBody(o.nose));
  }
  auto witness=[&](const char* name){std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);
   return WriteRgbPng((output/name).string(),window->Width(),window->Height(),pixels);};
  if(!ready&&available){
   ready=frame;
   for(auto& o:observations){auto* material=world.RuntimeDeformable(o.material,error);const auto root=world.Physics().GetTransform(world.RuntimeBody(o.root));
    o.initial=root.position;o.forward=root.rotation*glm::vec3(0,0,-1);o.initialPlastic=material->plasticRest;
    o.initialSpan=glm::inverse(glm::dquat(root.rotation))*(GroupCenter(*material,"front")-GroupCenter(*material,"rear"));
    for(auto point:material->positions)o.initialShape.push_back(glm::inverse(glm::dquat(root.rotation))*(point-glm::dvec3(root.position)));
    for(const auto& tet:material->asset->solids)o.initialEdges.push_back(TetEdges(*material,tet.nodes));
    o.noseMass=world.Physics().GetMass(world.RuntimeBody(o.nose));
   }
   const auto* weak=world.RuntimeDeformable(920,error);const auto* strong=world.RuntimeDeformable(1120,error);
   Check(weak->asset->nodes==strong->asset->nodes&&weak->asset->tetrahedra==strong->asset->tetrahedra&&
    weak->settings.material.shearCompliance==strong->settings.material.shearCompliance&&weak->settings.material.yieldStrain==strong->settings.material.yieldStrain&&
    weak->asset->fracture->compression>0&&weak->asset->fracture->compression<strong->asset->fracture->compression,
    "same actual vehicle geometry and plastic material compare independently authored cohesive strength");
   Check(weak->asset->fracture->structural&&!weak->asset->fracture->rigid&&weak->asset->fracture->parts.size()>2&&
    std::all_of(weak->settings.attachments.begin(),weak->settings.attachments.end(),[](const auto& a){return a.kind==DeformableAttachment::Kind::Body&&a.forceLimit==0&&a.torqueLimit==0;}),
    "load-bearing crumple region uses real plastic tetrahedra and interior cohesive failure rather than finite mount release");
   bool ownership=true;for(const auto& o:observations)for(unsigned i=0;i<4;++i){const auto* wheel=world.RuntimeWheel(o.root+1+i);
    ownership&=wheel&&wheel->settings.chassis==(i<2?o.nose:o.root);}
   Check(ownership&&observations[0].noseMass>=20&&observations[1].noseMass>=20,
    "two front wheels belong to a substantial physical nose/axle module and rear wheels to the cabin body");
   if(read){auto* service=world.SceneControl()->Saves(host.Resources());SaveStorage storage(service->Root());bool recovered=false;
    Check(storage.Read("damage-current",snapshot,recovered,error),"fresh process reads the ordinary actual crashed-vehicle save");phase=1;}
  }
  if(ready&&!read){
   const unsigned tick=frame-ready;
   if(tick>20){fixedTimes.push_back(play.LastFixedStepMilliseconds());frameTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStarted).count());}
   for(auto& o:observations){auto* material=world.RuntimeDeformable(o.material,error);auto* train=world.RuntimePowertrain(o.root);
    const auto root=world.Physics().GetTransform(world.RuntimeBody(o.root)),nose=world.Physics().GetTransform(world.RuntimeBody(o.nose));
    const auto velocity=world.Physics().GetLinearVelocity(world.RuntimeBody(o.root)),noseVelocity=world.Physics().GetLinearVelocity(world.RuntimeBody(o.nose));
    const auto localNose=glm::inverse(glm::dquat(root.rotation))*glm::dvec3(nose.position-root.position);
    o.distance=std::max(o.distance,double(glm::dot(root.position-o.initial,o.forward)));
    const bool actualContact=Contact(world,o.nose,o.wall,o.wallImpulse);
    if(actualContact&&!o.contactTick)o.contactTick=tick;
    if(tick>80&&!o.contactTick)o.approachSpeed=std::max(o.approachSpeed,double(glm::dot(velocity,o.forward)));
    const double plastic=PlasticChange(*material,o),shape=ShapeChange(*material,root,o),nonrigid=NonrigidChange(*material,o);
    const auto span=glm::inverse(glm::dquat(root.rotation))*(GroupCenter(*material,"front")-GroupCenter(*material,"rear"));
    const double compressed=glm::length(o.initialSpan)-glm::dot(span,glm::normalize(o.initialSpan));
    const bool path=PatchesConnected(*material);o.physicalBonds=o.scriptBonds=0;
    for(auto cause:material->fracture.broken){o.physicalBonds+=cause==1;o.scriptBonds+=cause==2;}
    o.attachmentReleased|=std::any_of(material->released.begin(),material->released.end(),[](auto r){return r!=0;});
    if(!o.contactTick){o.plasticBefore=std::max(o.plasticBefore,plastic);o.shapeBefore=std::max(o.shapeBefore,shape);o.nonrigidBefore=std::max(o.nonrigidBefore,nonrigid);}
    // Edge lengths are invariant under a fragment's rigid translation and
    // rotation. Impact-driven yielding after a cohesive cut still counts as
    // true nonrigid deformation, while freely moving intact debris cannot.
    if(o.contactTick){o.plasticImpact=std::max(o.plasticImpact,plastic);o.nonrigidImpact=std::max(o.nonrigidImpact,nonrigid);}
    if(o.contactTick&&!o.cutTick){o.plasticBeforeCut=std::max(o.plasticBeforeCut,plastic);o.nonrigidBeforeCut=std::max(o.nonrigidBeforeCut,nonrigid);
     o.shapeImpact=std::max(o.shapeImpact,shape);o.compression=std::max(o.compression,compressed);}
    if(!path&&!o.cutTick){o.cutTick=tick;o.cutRelativeNose=localNose;}
    if(o.cutTick)o.separation=std::max(o.separation,glm::length(localNose-o.cutRelativeNose));
    double transmitted=0;unsigned frontConnected=0,rearConnected=0;
    for(size_t i=0;i<train->settings.links.size();++i){transmitted+=std::abs(train->state.links[i].torque);
     if(train->settings.links[i].connection.owner){frontConnected+=train->state.links[i].connected;o.frontCommandHeld&=train->command.links[i].enabled;}
     else{rearConnected+=train->state.links[i].connected;o.rearPathSurvives&=train->state.links[i].connected&&train->command.links[i].enabled;}
    }
    for(const auto& a:material->attachmentState)o.peakForce=std::max(o.peakForce,a.peakForce);
    for(auto value:material->fracture.tensionDemand)o.tension=std::max(o.tension,value);
    for(auto value:material->fracture.shearDemand)o.shear=std::max(o.shear,value);
    o.unhealthy|=!material->error.empty()||!material->fracture.error.empty();
    const auto up=root.rotation*glm::vec3(0,1,0);
    trace<<tick<<','<<o.root<<','<<o.distance<<','<<o.approachSpeed<<','<<glm::dot(velocity,o.forward)<<','<<root.position.x<<','<<root.position.y<<','<<root.position.z<<','
     <<up.x<<','<<up.y<<','<<up.z<<','<<nose.position.x<<','<<nose.position.y<<','<<nose.position.z<<','<<glm::length(noseVelocity)<<','
     <<glm::length(world.Physics().GetAngularVelocity(world.RuntimeBody(o.nose)))<<','<<actualContact<<','<<o.wallImpulse<<','
     <<train->command.sources[0].input<<','<<train->state.sourceTorque[0]<<','<<transmitted<<','<<plastic<<','<<shape<<','<<nonrigid<<','<<compressed<<','<<o.separation<<','
     <<o.physicalBonds<<','<<o.scriptBonds<<','<<path<<','<<frontConnected<<','<<rearConnected<<','<<material->stats.maximumStrain<<','<<material->stats.minimumJacobian<<','
     <<o.peakForce<<','<<o.tension<<','<<o.shear<<','<<material->error<<'\n';
    for(unsigned i=0;i<4;++i)if(auto* wheel=world.RuntimeWheel(o.root+1+i)){const auto& s=wheel->state;
     wheelTrace<<tick<<','<<o.root<<','<<o.root+1+i<<','<<s.available<<','<<s.contact<<','<<s.length<<','<<s.normalForce<<','
      <<s.angularVelocity*60/(2*3.14159265358979323846)<<','<<s.longitudinalSlip<<','<<s.longitudinalImpulse<<','<<s.lateralImpulse<<'\n';}
   }
   const auto* weakTrain=world.RuntimePowertrain(900);const auto* strongTrain=world.RuntimePowertrain(1100);
   equalInput&=weakTrain->command.sources[0].input==strongTrain->command.sources[0].input;
   if(drive&&!approachPicture&&observations[0].distance>8&&!observations[0].contactTick)approachPicture=witness("approach.png");
   // The first topology boundary is earlier than a visibly separated module
   // and the next UI/presentation publication. Capture the physical result,
   // rather than labelling the preceding intact rendered frame as damage.
   if(drive&&!cutPicture&&observations[0].separation>.2&&observations[0].cutTick&&tick>observations[0].cutTick+2)
    cutPicture=witness("structural-separation.png");
   if(tick==80){
    size_t wheels=0,trains=0,materials=0,nodes=0,tets=0;for(const auto& d:world.ScriptObjects(false)){
     wheels+=d.wheel.has_value();trains+=d.powertrain.has_value();if(d.deformable)if(auto* m=world.RuntimeDeformable(d.id,error)){++materials;nodes+=m->positions.size();tets+=m->asset->solids.size();}}
    std::printf("WORKLOAD bodies=%zu dynamic_bodies=%zu wheels=%zu powertrains=%zu materials=%zu nodes=%zu tets=%zu material_substeps=%u iterations=%u fixed_hz=60\n",
     world.Physics().AliveBodyCount(),world.Physics().DynamicBodyCount(),wheels,trains,materials,nodes,tets,
     world.RuntimeDeformable(920,error)->settings.substeps,world.RuntimeDeformable(920,error)->settings.iterations);
    bool supported=true,intact=true;for(const auto& o:observations){unsigned contact=0;for(unsigned i=0;i<4;++i)contact+=world.RuntimeWheel(o.root+1+i)->state.contact;
     supported&=contact>=3;intact&=!o.cutTick&&!o.physicalBonds&&!o.attachmentReleased&&!o.unhealthy;}
    Check(supported&&intact,"both real complete vehicles support their ordinary pre-drive load without structural failure");
    if(watch)key("V",true);
    else if(drive)key("W",true);
   }
   if(watch&&tick==81)key("V",false);
   if(tick==endTick-60)key("Tab",true);
   if(tick==endTick-59)key("Tab",false);
   if(tick==endTick){
    key("W",false);
    for(const auto& o:observations)std::printf("VEHICLE root=%llu distance_m=%.9g approach_speed_mps=%.9g nose_wall_ns=%.9g prewall_plastic_m=%.9g impact_plastic_m=%.9g precut_plastic_m=%.9g prewall_nonrigid_m=%.9g impact_nonrigid_m=%.9g precut_nonrigid_m=%.9g prewall_shape_m=%.9g impact_shape_m=%.9g compression_m=%.9g module_separation_m=%.9g nose_mass_kg=%.9g physical_bonds=%u scripted_bonds=%u contact_tick=%u cut_tick=%u patch_peak_n=%.9g tensile_pa=%.9g shear_pa=%.9g error=%d\n",
     static_cast<unsigned long long>(o.root),o.distance,o.approachSpeed,o.wallImpulse,o.plasticBefore,o.plasticImpact,o.plasticBeforeCut,o.nonrigidBefore,o.nonrigidImpact,o.nonrigidBeforeCut,o.shapeBefore,o.shapeImpact,o.compression,o.separation,
     o.noseMass,o.physicalBonds,o.scriptBonds,o.contactTick,o.cutTick,o.peakForce,o.tension,o.shear,o.unhealthy);
    const auto& weak=observations[0];const auto& strong=observations[1];
    Check(available&&!weak.unhealthy&&!strong.unhealthy,"ordinary public-API vehicle content completes without script or physical-solve failure");
    Check(LiveNose(world,weak)&&LiveNose(world,strong),"substantial front modules retain original dynamic mass, collision and actual front wheels");
    if(drive){
     Check(equalInput&&(watch||weakTrain->command.sources[0].input>0),
      watch?"ordinary V watch action controls both physical sources through the authored tour policy":"equal ordinary logical input supplies the same held source policy to both vehicles");
     if(watch){bool finished=false;for(const auto& state:world.Scripts()->Capture())if(state.entity==10)
       finished=state.json.find("\"automatic\":false")!=std::string::npos;
      Check(finished&&weakTrain->command.sources[0].input==0&&strongTrain->command.sources[0].input==0,
       "ordinary watch policy finishes and releases throttle after its actual physical crash tour");}
     Check(weak.distance>8&&strong.distance>8&&weak.approachSpeed>2&&strong.approachSpeed>2,"actual powertrain and tyre forces produce a real forward wall approach");
     Check(weak.wallImpulse>0&&strong.wallImpulse>0,"actual physical front-module wall contacts carry nonzero solved impact impulses");
     if(!probe){
      Check(weak.plasticImpact>weak.plasticBefore+.005&&weak.nonrigidImpact>weak.nonrigidBefore+.03,
       "wall loading changes actual connected geometry and permanent material rest beyond ordinary ride deformation");
      Check(weak.cutTick>=weak.contactTick&&weak.contactTick&&weak.physicalBonds>0&&weak.scriptBonds==0&&!weak.attachmentReleased&&
       !PatchesConnected(*world.RuntimeDeformable(920,error)),"physical interior cohesive fracture severs the front/rear load path after impact without scripted or mount release");
      Check(weak.separation>.2&&LiveNose(world,weak),"a substantial complete front axle/nose module with both wheels separates through real physics");
      Check(FrontCut(*weakTrain,920)&&weak.frontCommandHeld&&RearIntact(*weakTrain)&&weak.rearPathSurvives,
       "actual topology loss removes front drive with commands still held while the independent rear drivetrain survives");
      Check(!strong.cutTick&&!strong.physicalBonds&&!strong.scriptBonds&&!strong.attachmentReleased&&PatchesConnected(*world.RuntimeDeformable(1120,error)),
       "otherwise identical stronger same-input vehicle retains its actual structural and mechanical load path");
      Check(PlasticChange(*world.RuntimeDeformable(920,error),weak)>.005,"permanent physical material rest remains changed after the damaging encounter");
     }
    }else{
     Check(weak.distance<.5&&strong.distance<.5&&weakTrain->command.sources[0].input==0&&strongTrain->command.sources[0].input==0,
      "same-duration no-drive control receives no hidden launch or scripted crash");
     Check(!weak.physicalBonds&&!strong.physicalBonds&&!weak.cutTick&&!strong.cutTick&&!weak.attachmentReleased&&!strong.attachmentReleased,
      "ordinary stationary load leaves both complete structural paths intact");
    }
    witness("strong-comparison.png");
    if(mode=="write"){key("F5",true);phase=6;}else{complete=true;world.UI().RequestQuit();}
   }
   if(tick==1201)key("F5",false);
   if(mode=="write"&&phase==6){auto* service=world.SceneControl()->Saves(host.Resources());
    for(uint64_t id=1;id<8;++id)if(const auto* status=service->Status(id)){
     if(status->state=="completed"){Check(true,"ordinary save commits actual crashed material, physical endpoints and drive state");complete=true;phase=9;world.UI().RequestQuit();}
     if(status->state=="failed"){std::printf("SAVE %s\n",status->error.c_str());Check(false,"actual crashed-vehicle save completes");complete=true;phase=9;world.UI().RequestQuit();}
    }
   }
  }
  if(read&&phase==3){std::map<std::string,SaveChunk> chunks;
   if(WorldPersistence::Capture(world,chunks,error)){
    Check(play.FixedStepsSinceReset()==0,"new process observes restored physical endpoint before any resumed solver step");
    for(const char* name:{"entities","deformables","wheels","powertrains"}){
     const auto found=chunks.find(name),expected=snapshot.participants.find(name);
     Check(found!=chunks.end()&&expected!=snapshot.participants.end()&&found->second.data==expected->second.data,
      (std::string("cold restoration retains exact authoritative ")+name+" rather than cached damage labels").c_str());
     if(found!=chunks.end())Dump(output/(std::string("restored-")+name+".bin"),found->second.data);
    }
    auto* weak=world.RuntimeDeformable(920,error);auto* train=world.RuntimePowertrain(900);
    Check(weak&&!PatchesConnected(*weak)&&FrontCut(*train,920)&&RearIntact(*train)&&LiveNose(world,observations[0]),
     "cold restored actual fractured body, wheels and derived front-drive cut preserve physical ownership");
    observations[0].initial=world.Physics().GetTransform(world.RuntimeBody(910)).position;
    const auto lateral=world.Physics().GetTransform(world.RuntimeBody(900)).rotation*glm::vec3(1,0,0);
    world.Physics().ApplyLinearImpulse(world.RuntimeBody(910),lateral*20.f);
    frozen=false;phase=4;ready=frame;
   }else if(error!="loading"&&error.find("required script instance not initialized")==std::string::npos){
    std::printf("RESTORE %s\n",error.c_str());Check(false,"cold restored actual vehicle becomes ready");complete=true;world.UI().RequestQuit();}
  }
  if(read&&phase==4&&frame>ready+45){auto* weak=world.RuntimeDeformable(920,error);const auto current=world.Physics().GetTransform(world.RuntimeBody(910)).position;
   Check(glm::length(current-observations[0].initial)>.01f&&weak&&!PatchesConnected(*weak)&&FrontCut(*world.RuntimePowertrain(900),920),
    "restored front module physically responds to a new ordinary impulse while its topology and drive stay disconnected");
   complete=true;world.UI().RequestQuit();}
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  if(frame>2600){Check(false,"bounded physical vehicle application deadline");world.UI().RequestQuit();}
 };
 Application application;std::string argument=scene;char name[]="judas";char* args[]={name,argument.data()};
 Check(application.Run(2,args,&control)==0&&complete,"ordinary registered vehicle scene starts and shuts down normally");
 if(!fixedTimes.empty()){std::sort(fixedTimes.begin(),fixedTimes.end());std::sort(frameTimes.begin(),frameTimes.end());
  std::printf("PERF samples=%zu fixed_median_ms=%.6f fixed_p95_ms=%.6f fixed_max_ms=%.6f frame_median_ms=%.6f frame_p95_ms=%.6f frame_max_ms=%.6f\n",
   fixedTimes.size(),fixedTimes[fixedTimes.size()/2],fixedTimes[size_t(.95*(fixedTimes.size()-1))],fixedTimes.back(),frameTimes[frameTimes.size()/2],frameTimes[size_t(.95*(frameTimes.size()-1))],frameTimes.back());}
 std::printf("M83 physical vehicle %s checks=%u failures=%u\n",mode.c_str(),checks,failures);return failures?1:0;
}
