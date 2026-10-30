#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "WorldPersistence.h"
#include "TestInput.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <thread>
#include <cstdio>
unsigned checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
int main(int argc,char** argv){
 if(argc!=3)return 2;
 const auto out=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(out);
 std::ofstream csv(out/"frames.csv");csv<<"frame,phase,frame_ms,step_ms,bodies,draw_calls\n";
 ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};unsigned frame=0,villageAt=0,resumedAt=0,phase=0;uint64_t request=0;RuntimeWorld* prior=nullptr;glm::vec3 expected;std::vector<double> ordinary;bool complete=false;
 using Clock=std::chrono::steady_clock;Clock::time_point began;std::string error;
 control.hostReady=[](EngineHost& h){std::string e;h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);h.Audio().Init(e,true);};
 control.worldReady=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){prior=&w;};
 control.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){
  began=Clock::now();if(frame==10)QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,true);if(frame==70)QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,false);
  if(frame==35)QueueTestKey(h.GetWindow(),SDL_SCANCODE_SPACE,true);
  if(frame==36)QueueTestKey(h.GetWindow(),SDL_SCANCODE_SPACE,false);
  if(frame==95)QueueTestKey(h.GetWindow(),SDL_SCANCODE_2,true);
  if(frame==96)QueueTestKey(h.GetWindow(),SDL_SCANCODE_2,false);
  if(w.SceneControl()->Current()=="Scenes/village.judas"&&!villageAt){villageAt=frame;QueueTestKey(h.GetWindow(),SDL_SCANCODE_W,true);}
  if(villageAt&&frame==villageAt+25)QueueTestKey(h.GetWindow(),SDL_SCANCODE_W,false);
  if(phase==3&&&w!=prior){const auto actual=w.Physics().GetTransform(w.RuntimeBody(1)).position;Check(glm::length(actual-expected)<1e-6f,"restored physical endpoint equals captured native snapshot");prior=&w;resumedAt=frame;phase=4;}
 };
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){
  ++frame;const auto elapsed=std::chrono::duration<double,std::milli>(Clock::now()-began).count();csv<<frame<<','<<phase<<','<<elapsed<<','<<play.LastFixedStepMilliseconds()<<','<<w.Physics().AliveBodyCount()<<','<<h.GetRenderer().Stats().drawCalls<<'\n';
  auto quit=[](){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);};
  if(w.Scripts()&&!w.Scripts()->Diagnostics().empty()){Check(false,w.Scripts()->Diagnostics().front().message.c_str());quit();return;}
  if(frame>10&&phase==0)ordinary.push_back(elapsed);
  auto* saves=w.SceneControl()->Saves(h.Resources());
  if(villageAt&&frame>villageAt+35&&phase==0){
   Check(w.SceneControl()->Get("stars")!="null"&&w.SceneControl()->Get("stars")!="0","actual native trigger collected a star through project JavaScript");
   request=saves->Request("save","tilemap-app","Tilemap application","{}",error);Check(request!=0,"save actual played tilemap world");phase=1;
  }
  if(phase==1){auto* status=saves->Status(request);if(status&&status->state=="failed"){Check(false,status->error.c_str());quit();return;}
   if(status&&status->state=="completed"){GameSnapshot snapshot;bool recovered=false;SaveStorage storage(saves->Root());Scene scene;
    Check(storage.Read("tilemap-app",snapshot,recovered,error)&&WorldPersistence::SceneFrom(snapshot.participants,scene,error),"read independently committed save scene");expected=scene.Find(1)->transform.position;
    prior=&w;request=saves->Request("load","tilemap-app","","{}",error);Check(request!=0,"restore through real outer-boundary save service");phase=3;
   }
  }
  if(phase==3){auto* status=saves->Status(request);if(status&&status->state=="failed"){Check(false,status->error.c_str());quit();return;}}
  if(phase==4&&frame>resumedAt+20){Check(w.Physics().GetPlanarNormal(w.RuntimeBody(1))==glm::vec3(0,0,1),"restored character retains actual planar mobility");Check(w.SceneControl()->Get("stars")!="0","game progression survives real load");complete=true;quit();}
  if(frame>700){Check(false,"bounded application completion");quit();}if(phase==1||phase==3)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 };
 Application app;char program[]="judas";char* args[]={program,argv[1]};Check(app.Run(2,args,&control)==0&&complete,"production application renders, switches, saves, restores and shuts down");
 std::sort(ordinary.begin(),ordinary.end());if(!ordinary.empty())std::printf("ORDINARY_RENDERED_FRAME n=%zu median_ms=%.6f p95_ms=%.6f worst_ms=%.6f\n",ordinary.size(),ordinary[ordinary.size()/2],ordinary[size_t(std::ceil(ordinary.size()*.95))-1],ordinary.back());
 std::printf("TILEMAP_APPLICATION checks=%u failures=%u\n",checks,failures);return failures?1:0;
}
