#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
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

namespace {
unsigned checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);std::fflush(stdout);}
struct Observations {
 EntityId root=0,wall=0;glm::vec3 initial{0},forward{0};
 std::array<glm::vec3,4> releasedMount{};std::array<bool,4> observedRelease{};
 std::array<std::vector<glm::dmat3>,4> initialPlastic{};
 double distance=0,speed=0,approachSpeed=0,carrierImpulse=0,chassisImpulse=0,patchDemand=0;
 double preWallDemand=0,postWallDemand=0;
 double detachedMotion=0;
 unsigned contactTick=0,releaseTick=0,released=0;bool unhealthy=false;
};
struct MountPatch {DeformableInstance* material=nullptr;size_t tip=0,root=0;};
MountPatch Mount(RuntimeWorld& world,EntityId owner,unsigned wheel){
 auto* train=world.RuntimePowertrain(owner);if(!train||wheel>=train->settings.links.size())return {};
 const auto& connection=train->settings.links[wheel].connection;std::string error;
 auto* material=world.RuntimeDeformable(connection.owner,error);if(!material)return {};
 size_t tip=material->settings.attachments.size(),root=tip;
 for(size_t i=0;i<material->settings.attachments.size();++i){const auto& attachment=material->settings.attachments[i];
  if(attachment.group==connection.attachment)tip=i;
  if(attachment.group==connection.rootAttachment)root=i;
 }
 if(tip>=material->released.size()||root>=material->released.size())return {};
 return {material,tip,root};
}
bool AllIntact(RuntimeWorld& world,EntityId root){
 for(unsigned i=0;i<4;++i){const auto patch=Mount(world,root,i);const auto* arm=patch.material;
  if(!arm||!arm->error.empty()||arm->released[patch.root]||arm->released[patch.tip])return false;
 }return true;
}
bool ReadContacts(RuntimeWorld& world,EntityId entity,EntityId wall,double& impulse){
 std::vector<PhysicsWorld::BodyContactSample> contacts;
 bool actual=false;
 if(world.Physics().ReadBodyContactSample(world.RuntimeBody(entity),contacts))for(const auto& contact:contacts)
  if(world.EntityIdOfBody(contact.other)==wall){impulse=std::max(impulse,double(contact.normalImpulse));actual|=contact.normalImpulse>0;}
 return actual;
}
}

int main(int argc,char** argv){
 if(argc!=4)return 2;
 const std::string scene=argv[1],mode=argv[2];const bool rigid=mode=="rigid-drive",drive=mode=="drive"||rigid;
 if(!drive&&mode!="rest")return 2;
 const auto output=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(output);
 std::ofstream trace(output/"crash-trace.csv");trace<<std::setprecision(12)
  <<"tick,root,distance_m,speed_mps,source_input,source_torque_nm,carrier_wall_impulse_ns,patch_peak_n,released,connected,transmission_torque_nm,error\n";
 std::ofstream motion(output/"motion-trace.csv");motion<<std::setprecision(12)
  <<"tick,root,x,y,z,vx,vy,vz,up_x,up_y,up_z,ground_contact_impulse_ns,wheel_normal_force_n\n";
 std::ofstream tyres(output/"wheel-trace.csv");tyres<<std::setprecision(12)
  <<"tick,root,wheel,available,contact,initial_overlap,length_m,normal_force_n,rpm,longitudinal_slip_mps,lateral_slip_mps,longitudinal_impulse_ns,lateral_impulse_ns,carrier_y,carrier_up_x,carrier_up_y,carrier_up_z,carrier_speed_mps,carrier_angular_speed_radps,patch_anchor_error_m,material_strain,plastic_rest_change_m\n";
 std::ofstream reactions(output/"reaction-trace.csv");reactions<<std::setprecision(12)
  <<"tick,root,wall_contact_this_step,wall_contact_seen,step_patch_peak_n,pre_wall_peak_n,post_wall_peak_n\n";
 std::array<Observations,2> observations{{{900,1500},{1100,1501}}};
 std::vector<double> fixedTimes,frameTimes;unsigned frame=0,ready=0;bool completed=false,equalCommands=true,restPassed=false;
 bool approachWitness=false,collisionWitness=false;
 auto frameStart=std::chrono::steady_clock::now();
 Window* window=nullptr;
 auto key=[&](bool down){window->QueueTestPhysical("key:W",down?1.f:0.f);};
 ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};
 control.hostReady=[&](EngineHost& host){window=&host.GetWindow();window->SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string ignored;host.Audio().Init(ignored,true);};
 control.beforeFrame=[&](EngineHost&,RuntimeWorld&,InteractivePlay&){frameStart=std::chrono::steady_clock::now();};
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  ++frame;bool resourcesReady=true;std::string error;
  for(auto& observation:observations){const auto root=world.RuntimeBody(observation.root);auto* train=world.RuntimePowertrain(observation.root);
   resourcesReady&=world.Physics().IsBodyEnabled(root)&&train&&train->state.available;
   if(!rigid)for(unsigned i=0;i<4;++i)resourcesReady&=Mount(world,observation.root,i).material!=nullptr;
  }
  if(!ready&&resourcesReady){
   ready=frame;
   for(auto& observation:observations){const auto pose=world.Physics().GetTransform(world.RuntimeBody(observation.root));
    observation.initial=pose.position;observation.forward=pose.rotation*glm::vec3(0,0,-1);
    // Retain the actually initialized material basis instead of exposing the
    // solver's private orientation or guessing a universal reference frame.
    if(!rigid)for(unsigned i=0;i<4;++i)observation.initialPlastic[i]=Mount(world,observation.root,i).material->plasticRest;
   }
   if(rigid){
    Check(std::abs(world.Physics().GetMass(world.RuntimeBody(900))-188)<.1&&std::abs(world.Physics().GetMass(world.RuntimeBody(1100))-188)<.1,
     "rigid isolation retains the approximate complete-vehicle gross mass");
    Check(world.RuntimeDefinition(901)->wheel->chassis==900&&world.RuntimeDefinition(1101)->wheel->chassis==1100,
     "rigid control wheels act directly on ordinary root bodies through original authored mounts");
   }else{
    const auto weakPatch=Mount(world,900,0),strongPatch=Mount(world,1100,0);
    const auto* weak=weakPatch.material;const auto* strong=strongPatch.material;
    Check(weak&&strong&&weak->asset==strong->asset&&
     weak->settings.material.shearCompliance==strong->settings.material.shearCompliance&&
     weak->settings.material.yieldStrain==strong->settings.material.yieldStrain&&
     weak->settings.attachments[weakPatch.tip].forceLimit<strong->settings.attachments[strongPatch.tip].forceLimit,
     "parallel vehicles share geometry and material; only authored mount strength changes");
    Check(world.RuntimeDefinition(901)->wheel->chassis==910&&world.RuntimeDefinition(1101)->wheel->chassis==1110,
     "ordinary mobile wheels terminate in physical material-supported carrier bodies");
   }
  }
  if(ready){const unsigned tick=frame-ready;
   if(tick>20){fixedTimes.push_back(play.LastFixedStepMilliseconds());
    // Frame work excludes optional evidence readback/PNG encoding below.
    frameTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStart).count());
   }
   for(auto& observation:observations){
    auto* train=world.RuntimePowertrain(observation.root);const auto body=world.RuntimeBody(observation.root);
    const auto pose=world.Physics().GetTransform(body);observation.distance=std::max(observation.distance,
      double(glm::dot(pose.position-observation.initial,observation.forward)));
    observation.speed=std::max(observation.speed,double(glm::length(world.Physics().GetLinearVelocity(body))));
    bool wallContact=ReadContacts(world,observation.root,observation.wall,observation.chassisImpulse);
    observation.released=0;unsigned connected=0;double demand=0,transmitted=0;
    for(unsigned i=0;i<4;++i){const auto patch=rigid?MountPatch{}:Mount(world,observation.root,i);auto* arm=patch.material;
     if(!rigid)wallContact|=ReadContacts(world,observation.root+10+i,observation.wall,observation.carrierImpulse);
     if(observation.carrierImpulse>0&&!observation.contactTick)observation.contactTick=tick;
     if(arm){observation.released+=arm->released[patch.tip]!=0;observation.unhealthy|=!arm->error.empty();
      if(arm->released[patch.tip]){const auto local=glm::inverse(pose.rotation)*
        (world.Physics().GetTransform(world.RuntimeBody(observation.root+10+i)).position-pose.position);
       if(!observation.observedRelease[i]){observation.observedRelease[i]=true;observation.releasedMount[i]=local;}
       else observation.detachedMotion=std::max(observation.detachedMotion,double(glm::length(local-observation.releasedMount[i])));
      }
      if(arm->attachmentState.size()>patch.tip)demand=std::max(demand,arm->attachmentState[patch.tip].peakForce);
     }else if(!rigid)observation.unhealthy=true;
     if(i<train->state.links.size()){connected+=train->state.links[i].connected;transmitted+=std::abs(train->state.links[i].torque);}
    }
    observation.patchDemand=std::max(observation.patchDemand,demand);
    const bool wallSeen=observation.carrierImpulse>0||observation.chassisImpulse>0;
    if(wallSeen)observation.postWallDemand=std::max(observation.postWallDemand,demand);
    else observation.preWallDemand=std::max(observation.preWallDemand,demand);
    reactions<<tick<<','<<observation.root<<','<<wallContact<<','<<wallSeen<<','<<demand<<','
     <<observation.preWallDemand<<','<<observation.postWallDemand<<'\n';
    if(observation.released&&!observation.releaseTick)observation.releaseTick=tick;
    // Initial vertical settling is not evidence of driven forward motion.
    // Measure the real approach component after input and before any wall hit.
    if(tick>80&&!observation.contactTick&&observation.chassisImpulse==0)
     observation.approachSpeed=std::max(observation.approachSpeed,
       double(glm::dot(world.Physics().GetLinearVelocity(body),observation.forward)));
    if(tick%15==0)trace<<tick<<','<<observation.root<<','<<observation.distance<<','<<observation.speed<<','
      <<train->command.sources[0].input<<','<<train->state.sourceTorque[0]<<','<<observation.carrierImpulse<<','
      <<demand<<','<<observation.released<<','<<connected<<','<<transmitted<<','<<observation.unhealthy<<'\n';
    if(tick%15==0||tick==80){
     const auto velocity=world.Physics().GetLinearVelocity(body),up=pose.rotation*glm::vec3(0,1,0);
     std::vector<PhysicsWorld::BodyContactSample> contacts;double groundImpulse=0,normalLoad=0;
     if(world.Physics().ReadBodyContactSample(body,contacts))for(const auto& contact:contacts)
      if(world.EntityIdOfBody(contact.other)==3)groundImpulse+=contact.normalImpulse;
     for(unsigned i=0;i<4;++i)if(auto* wheel=world.RuntimeWheel(observation.root+1+i)){
      const auto& state=wheel->state;const auto carrierBody=world.RuntimeBody(wheel->settings.chassis);
      const auto carrier=world.Physics().GetTransform(carrierBody);
      const auto carrierUp=carrier.rotation*glm::vec3(0,1,0);normalLoad+=state.normalForce;
      double anchorError=0,strain=0,plasticChange=0;
      if(auto* arm=rigid?nullptr:Mount(world,observation.root,i).material){
       strain=arm->stats.maximumStrain;
       for(const auto& attachment:arm->settings.attachments)if(attachment.kind==DeformableAttachment::Kind::Body){
        const auto target=world.Physics().GetTransform(world.RuntimeBody(attachment.target));
        if(auto group=arm->asset->groups.find(attachment.group);group!=arm->asset->groups.end())for(auto node:group->second){
         const auto point=glm::dvec3(target.position)+glm::dquat(target.rotation)*(arm->asset->nodes[node]+attachment.offset);
         anchorError=std::max(anchorError,glm::length(arm->positions[node]-point));
        }
       }
       for(size_t element=0;element<arm->plasticRest.size()&&element<observation.initialPlastic[i].size();++element)for(unsigned column=0;column<3;++column)
        plasticChange=std::max(plasticChange,glm::length(arm->plasticRest[element][column]-observation.initialPlastic[i][element][column]));
      }
      tyres<<tick<<','<<observation.root<<','<<observation.root+1+i<<','<<state.available<<','<<state.contact<<','
       <<state.initialOverlap<<','<<state.length<<','<<state.normalForce<<','<<state.angularVelocity*60/(2*3.14159265358979323846)<<','
       <<state.longitudinalSlip<<','<<state.lateralSlip<<','<<state.longitudinalImpulse<<','<<state.lateralImpulse<<','
       <<carrier.position.y<<','<<carrierUp.x<<','<<carrierUp.y<<','<<carrierUp.z<<','
       <<glm::length(world.Physics().GetLinearVelocity(carrierBody))<<','
       <<glm::length(world.Physics().GetAngularVelocity(carrierBody))<<','
       <<anchorError<<','<<strain<<','<<plasticChange<<'\n';
     }
     motion<<tick<<','<<observation.root<<','<<pose.position.x<<','<<pose.position.y<<','<<pose.position.z<<','
      <<velocity.x<<','<<velocity.y<<','<<velocity.z<<','<<up.x<<','<<up.y<<','<<up.z<<','<<groundImpulse<<','<<normalLoad<<'\n';
     if(tick==80||tick==300||tick==600)std::printf("RIDE tick=%u root=%llu height_m=%.9g current_speed_mps=%.9g up=(%.6g,%.6g,%.6g) ground_contact_impulse_ns=%.9g wheel_normal_force_n=%.9g\n",
      tick,static_cast<unsigned long long>(observation.root),pose.position.y,glm::length(velocity),up.x,up.y,up.z,groundImpulse,normalLoad);
    }
   }
   const auto* a=world.RuntimePowertrain(900);const auto* b=world.RuntimePowertrain(1100);
   equalCommands&=a&&b&&a->command.sources[0].input==b->command.sources[0].input;
   auto witness=[&](const char* filename){std::vector<unsigned char> pixels;
    host.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);
    return WriteRgbPng((output/filename).string(),window->Width(),window->Height(),pixels);
   };
   if(drive&&!rigid){const auto& weak=observations[0];
    if(!approachWitness&&weak.distance>8&&weak.contactTick==0&&weak.chassisImpulse==0)
     approachWitness=witness("approach.png");
    if(!collisionWitness&&weak.contactTick&&weak.released)
     collisionWitness=witness("collision.png");
    // Select the stronger comparison with its ordinary project action. This
    // changes presentation only; both vehicles retain the same held W input.
    if(tick==680)window->QueueTestPhysical("key:Tab",1);
    if(tick==681)window->QueueTestPhysical("key:Tab",0);
   }
   if(tick==80){
    size_t wheels=0,trains=0,materials=0,nodes=0,tets=0;
    for(const auto& definition:world.ScriptObjects(false)){
     wheels+=definition.wheel.has_value();trains+=definition.powertrain.has_value();
     if(definition.deformable)if(auto* material=world.RuntimeDeformable(definition.id,error)){
      ++materials;nodes+=material->positions.size();tets+=material->asset->solids.size();
     }
    }
    std::printf("WORKLOAD bodies=%zu dynamic_bodies=%zu wheels=%zu powertrains=%zu materials=%zu nodes=%zu tets=%zu fixed_hz=60\n",
     world.Physics().AliveBodyCount(),world.Physics().DynamicBodyCount(),wheels,trains,materials,nodes,tets);
    if(!rigid){restPassed=AllIntact(world,900)&&AllIntact(world,1100);
     Check(restPassed,"both complete vehicle assemblies support their normal resting load without mount failure");}
    bool supported=true;for(const auto& observation:observations){unsigned contacts=0;
     for(unsigned i=0;i<4;++i)if(auto* wheel=world.RuntimeWheel(observation.root+1+i))contacts+=wheel->state.contact;
     supported&=contacts>=3;
    }
    Check(supported,"ordinary tyre contacts support each complete vehicle before driving");
    if(drive)key(true);
   }
   if(tick==720){
    key(false);const auto& weak=observations[0];const auto& strong=observations[1];
    for(const auto& observation:observations)std::printf(
      "CRASH root=%llu distance_m=%.9g max_speed_mps=%.9g approach_speed_mps=%.9g carrier_wall_impulse_ns=%.9g chassis_wall_impulse_ns=%.9g peak_patch_n=%.9g pre_wall_peak_n=%.9g post_wall_peak_n=%.9g detached_motion_m=%.9g first_contact_tick=%u first_release_tick=%u released=%u unhealthy=%d\n",
      static_cast<unsigned long long>(observation.root),observation.distance,observation.speed,observation.approachSpeed,observation.carrierImpulse,
      observation.chassisImpulse,observation.patchDemand,observation.preWallDemand,observation.postWallDemand,
      observation.detachedMotion,observation.contactTick,observation.releaseTick,observation.released,observation.unhealthy);
    Check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),"ordinary crash content uses public JS input, camera and mechanical commands without script errors");
    Check(world.Physics().IsBodyEnabled(world.RuntimeBody(900))&&world.Physics().IsDynamicBody(world.RuntimeBody(900))&&
     world.Physics().IsBodyEnabled(world.RuntimeBody(1100))&&world.Physics().IsDynamicBody(world.RuntimeBody(1100)),
     "both physical chassis retain live dynamic ownership through the complete encounter");
    if(!rigid)Check(!weak.unhealthy&&!strong.unhealthy,"vehicle material remains within its supported physical solve envelope");
    if(drive){
     Check(equalCommands&&a->command.sources[0].input>0,"one ordinary logical drive input supplies equal source commands to both vehicles");
     Check(weak.distance>8&&strong.distance>8&&weak.approachSpeed>2&&strong.approachSpeed>2,
      "actual drivetrain and tyre forces accelerate both complete vehicles toward their walls");
     if(rigid){Check(weak.chassisImpulse>0&&strong.chassisImpulse>0,"rigid controls reach the ordinary walls through actual solved chassis contacts");}
     else{
     Check(weak.carrierImpulse>0&&strong.carrierImpulse>0,"both moving vehicles hit walls through actual solved carrier contacts");
     Check(weak.contactTick&&weak.releaseTick>=weak.contactTick&&weak.released>0,
      "weak mount failure follows genuine vehicle-wall impact rather than a scripted break");
     bool physical=true,cut=false,held=true;
     for(unsigned i=0;i<4;++i){const auto patch=Mount(world,900,i);auto* arm=patch.material;if(!arm||!arm->released[patch.tip])continue;
      const auto carrier=world.RuntimeBody(910+i);physical&=world.Physics().IsBodyEnabled(carrier)&&world.Physics().GetMass(carrier)>0;
      cut|=!a->state.links[i].connected&&!a->state.links[i].active&&std::abs(a->state.links[i].torque)<1e-9;
      held&=a->command.links[i].enabled;
     }
     Check(physical&&weak.detachedMotion>.15,"detached finite carriers retain real collision and move independently of the damaged chassis");
     Check(cut&&held,"actual physical mount loss stops the affected drive row while authored driver enable remains held");
     Check(strong.released==0&&AllIntact(world,1100)&&std::all_of(b->state.links.begin(),b->state.links.end(),[](const auto& link){return link.connected;}),
      "same-input stronger mounts preserve their physical wiring after the equivalent wall encounter");
     }
    }else{
     Check(AllIntact(world,900)&&AllIntact(world,1100)&&weak.carrierImpulse==0&&strong.carrierImpulse==0,
      "same-duration no-drive negative control retains both mounts with no wall impact");
     Check(weak.distance<.5&&strong.distance<.5&&a->command.sources[0].input==0&&b->command.sources[0].input==0,
      "negative vehicles do not receive hidden launch velocity or a scripted crash sequence");
    }
    const bool finalWitness=witness(drive&&!rigid?"strong-comparison.png":(mode+".png").c_str());
    if(drive&&!rigid)Check(approachWitness&&collisionWitness&&finalWitness,
     "actual approach, first wall-damage boundary and stronger final comparison have rendered witnesses");
    completed=true;world.UI().RequestQuit();
   }
  }
  if(frame>1600){Check(false,"bounded crash application deadline");world.UI().RequestQuit();}
 };
 Application application;std::string argument=scene;char name[]="judas";char* args[]={name,argument.data()};
 Check(application.Run(2,args,&control)==0&&completed,"registered current crash scene starts and shuts down normally");
 if(!fixedTimes.empty()){std::sort(fixedTimes.begin(),fixedTimes.end());std::sort(frameTimes.begin(),frameTimes.end());
  std::printf("PERF samples=%zu fixed_median_ms=%.6f fixed_p95_ms=%.6f fixed_max_ms=%.6f frame_work_median_ms=%.6f frame_work_p95_ms=%.6f frame_work_max_ms=%.6f\n",
   fixedTimes.size(),fixedTimes[fixedTimes.size()/2],fixedTimes[size_t(.95*(fixedTimes.size()-1))],fixedTimes.back(),
   frameTimes[frameTimes.size()/2],frameTimes[size_t(.95*(frameTimes.size()-1))],frameTimes.back());}
 std::printf("M83 crash application %s checks=%u failures=%u\n",mode.c_str(),checks,failures);return failures?1:0;
}
