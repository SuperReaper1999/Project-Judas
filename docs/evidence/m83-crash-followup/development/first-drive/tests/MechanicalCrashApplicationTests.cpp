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
 double distance=0,speed=0,carrierImpulse=0,chassisImpulse=0,patchDemand=0;
 double detachedMotion=0;
 unsigned contactTick=0,releaseTick=0,released=0;bool unhealthy=false;
};
bool AllIntact(RuntimeWorld& world,EntityId root){
 for(unsigned i=0;i<4;++i){std::string error;auto* arm=world.RuntimeDeformable(root+20+i,error);
  if(!arm||!arm->error.empty()||arm->released.size()!=2||arm->released[0]||arm->released[1])return false;
 }return true;
}
void ReadContacts(RuntimeWorld& world,EntityId entity,EntityId wall,double& impulse){
 std::vector<PhysicsWorld::BodyContactSample> contacts;
 if(world.Physics().ReadBodyContactSample(world.RuntimeBody(entity),contacts))for(const auto& contact:contacts)
  if(world.EntityIdOfBody(contact.other)==wall)impulse=std::max(impulse,double(contact.normalImpulse));
}
}

int main(int argc,char** argv){
 if(argc!=4)return 2;
 const std::string scene=argv[1],mode=argv[2];const bool drive=mode=="drive";
 if(!drive&&mode!="rest")return 2;
 const auto output=std::filesystem::absolute(argv[3]);std::filesystem::create_directories(output);
 std::ofstream trace(output/"crash-trace.csv");trace<<std::setprecision(12)
  <<"tick,root,distance_m,speed_mps,source_input,source_torque_nm,carrier_wall_impulse_ns,patch_peak_n,released,connected,transmission_torque_nm,error\n";
 std::array<Observations,2> observations{{{900,1500},{1100,1501}}};
 std::vector<double> fixedTimes;unsigned frame=0,ready=0;bool completed=false,equalCommands=true,restPassed=false;
 Window* window=nullptr;
 auto key=[&](bool down){window->QueueTestPhysical("key:W",down?1.f:0.f);};
 ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};
 control.hostReady=[&](EngineHost& host){window=&host.GetWindow();window->SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string ignored;host.Audio().Init(ignored,true);};
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  ++frame;bool resourcesReady=true;std::string error;
  for(auto& observation:observations){const auto root=world.RuntimeBody(observation.root);auto* train=world.RuntimePowertrain(observation.root);
   resourcesReady&=world.Physics().IsBodyEnabled(root)&&train&&train->state.available;
   for(unsigned i=0;i<4;++i)resourcesReady&=world.RuntimeDeformable(observation.root+20+i,error)!=nullptr;
  }
  if(!ready&&resourcesReady){
   ready=frame;
   for(auto& observation:observations){const auto pose=world.Physics().GetTransform(world.RuntimeBody(observation.root));
    observation.initial=pose.position;observation.forward=pose.rotation*glm::vec3(0,0,-1);
   }
   const auto* weak=world.RuntimeDeformable(920,error);const auto* strong=world.RuntimeDeformable(1120,error);
   Check(weak&&strong&&weak->asset==strong->asset&&
    weak->settings.material.shearCompliance==strong->settings.material.shearCompliance&&
    weak->settings.material.yieldStrain==strong->settings.material.yieldStrain&&
    weak->settings.attachments[1].forceLimit<strong->settings.attachments[1].forceLimit,
    "parallel vehicles share geometry and material; only authored mount strength changes");
   Check(world.RuntimeDefinition(901)->wheel->chassis==910&&world.RuntimeDefinition(1101)->wheel->chassis==1110,
    "ordinary mobile wheels terminate in physical material-supported carrier bodies");
  }
  if(ready){const unsigned tick=frame-ready;
   if(tick>20)fixedTimes.push_back(play.LastFixedStepMilliseconds());
   for(auto& observation:observations){
    auto* train=world.RuntimePowertrain(observation.root);const auto body=world.RuntimeBody(observation.root);
    const auto pose=world.Physics().GetTransform(body);observation.distance=std::max(observation.distance,
      double(glm::dot(pose.position-observation.initial,observation.forward)));
    observation.speed=std::max(observation.speed,double(glm::length(world.Physics().GetLinearVelocity(body))));
    ReadContacts(world,observation.root,observation.wall,observation.chassisImpulse);
    observation.released=0;unsigned connected=0;double demand=0,transmitted=0;
    for(unsigned i=0;i<4;++i){auto* arm=world.RuntimeDeformable(observation.root+20+i,error);
     ReadContacts(world,observation.root+10+i,observation.wall,observation.carrierImpulse);
     if(observation.carrierImpulse>0&&!observation.contactTick)observation.contactTick=tick;
     if(arm){observation.released+=arm->released[1]!=0;observation.unhealthy|=!arm->error.empty();
      if(arm->released[1]){const auto local=glm::inverse(pose.rotation)*
        (world.Physics().GetTransform(world.RuntimeBody(observation.root+10+i)).position-pose.position);
       if(!observation.observedRelease[i]){observation.observedRelease[i]=true;observation.releasedMount[i]=local;}
       else observation.detachedMotion=std::max(observation.detachedMotion,double(glm::length(local-observation.releasedMount[i])));
      }
      if(arm->attachmentState.size()>1)demand=std::max(demand,arm->attachmentState[1].peakForce);
     }else observation.unhealthy=true;
     if(i<train->state.links.size()){connected+=train->state.links[i].connected;transmitted+=std::abs(train->state.links[i].torque);}
    }
    observation.patchDemand=std::max(observation.patchDemand,demand);
    if(observation.released&&!observation.releaseTick)observation.releaseTick=tick;
    if(tick%15==0)trace<<tick<<','<<observation.root<<','<<observation.distance<<','<<observation.speed<<','
      <<train->command.sources[0].input<<','<<train->state.sourceTorque[0]<<','<<observation.carrierImpulse<<','
      <<demand<<','<<observation.released<<','<<connected<<','<<transmitted<<','<<observation.unhealthy<<'\n';
   }
   const auto* a=world.RuntimePowertrain(900);const auto* b=world.RuntimePowertrain(1100);
   equalCommands&=a&&b&&a->command.sources[0].input==b->command.sources[0].input;
   if(tick==80){
    restPassed=AllIntact(world,900)&&AllIntact(world,1100);
    Check(restPassed,"both complete vehicle assemblies support their normal resting load without mount failure");
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
      "CRASH root=%llu distance_m=%.9g max_speed_mps=%.9g carrier_wall_impulse_ns=%.9g chassis_wall_impulse_ns=%.9g peak_patch_n=%.9g detached_motion_m=%.9g first_contact_tick=%u first_release_tick=%u released=%u unhealthy=%d\n",
      static_cast<unsigned long long>(observation.root),observation.distance,observation.speed,observation.carrierImpulse,
      observation.chassisImpulse,observation.patchDemand,observation.detachedMotion,observation.contactTick,observation.releaseTick,observation.released,observation.unhealthy);
    Check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),"ordinary crash content uses public JS input, camera and mechanical commands without script errors");
    Check(!weak.unhealthy&&!strong.unhealthy,"vehicle material remains within its supported physical solve envelope");
    if(drive){
     Check(equalCommands&&a->command.sources[0].input>0,"one ordinary logical drive input supplies equal source commands to both vehicles");
     Check(weak.distance>8&&strong.distance>8&&weak.speed>2&&strong.speed>2,
      "actual drivetrain and tyre forces accelerate both complete vehicles toward their walls");
     Check(weak.carrierImpulse>0&&strong.carrierImpulse>0,"both moving vehicles hit walls through actual solved carrier contacts");
     Check(weak.contactTick&&weak.releaseTick>=weak.contactTick&&weak.released>0,
      "weak mount failure follows genuine vehicle-wall impact rather than a scripted break");
     bool physical=true,cut=false,held=true;
     for(unsigned i=0;i<4;++i){auto* arm=world.RuntimeDeformable(920+i,error);if(!arm||!arm->released[1])continue;
      const auto carrier=world.RuntimeBody(910+i);physical&=world.Physics().IsBodyEnabled(carrier)&&world.Physics().GetMass(carrier)>0;
      cut|=!a->state.links[i].connected&&!a->state.links[i].active&&std::abs(a->state.links[i].torque)<1e-9;
      held&=a->command.links[i].enabled;
     }
     Check(physical&&weak.detachedMotion>.15,"detached finite carriers retain real collision and move independently of the damaged chassis");
     Check(cut&&held,"actual physical mount loss stops the affected drive row while authored driver enable remains held");
     Check(strong.released==0&&AllIntact(world,1100)&&std::all_of(b->state.links.begin(),b->state.links.end(),[](const auto& link){return link.connected;}),
      "same-input stronger mounts preserve their physical wiring after the equivalent wall encounter");
    }else{
     Check(AllIntact(world,900)&&AllIntact(world,1100)&&weak.carrierImpulse==0&&strong.carrierImpulse==0,
      "same-duration no-drive negative control retains both mounts with no wall impact");
     Check(weak.distance<.5&&strong.distance<.5&&a->command.sources[0].input==0&&b->command.sources[0].input==0,
      "negative vehicles do not receive hidden launch velocity or a scripted crash sequence");
    }
    std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);
    WriteRgbPng((output/(mode+".png")).string(),window->Width(),window->Height(),pixels);
    completed=true;world.UI().RequestQuit();
   }
  }
  if(frame>1600){Check(false,"bounded crash application deadline");world.UI().RequestQuit();}
 };
 Application application;std::string argument=scene;char name[]="judas";char* args[]={name,argument.data()};
 Check(application.Run(2,args,&control)==0&&completed,"registered current crash scene starts and shuts down normally");
 if(!fixedTimes.empty()){std::sort(fixedTimes.begin(),fixedTimes.end());std::printf("PERF samples=%zu fixed_median_ms=%.6f fixed_p95_ms=%.6f fixed_max_ms=%.6f\n",
  fixedTimes.size(),fixedTimes[fixedTimes.size()/2],fixedTimes[size_t(.95*(fixedTimes.size()-1))],fixedTimes.back());}
 std::printf("M83 crash application %s checks=%u failures=%u\n",mode.c_str(),checks,failures);return failures?1:0;
}
