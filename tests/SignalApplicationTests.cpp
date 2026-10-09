#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "ScriptSystem.h"
#include "WorldStreaming.h"
#include "SaveService.h"
#include "TestInput.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <filesystem>
#include <cstdio>
#include <thread>
namespace fs=std::filesystem;using Json=nlohmann::json;
namespace {int checks=0,failures=0;void check(bool good,const char* name){++checks;failures+=!good;std::printf("%s %s\n",good?"PASS":"FAIL",name);}
Json state(RuntimeWorld& w,EntityId id,uint64_t slot=1){if(!w.Scripts())return {};for(auto& r:w.Scripts()->Capture())if(r.entity==id&&r.slot==slot)return Json::parse(r.json);return {};}
void quit(){SDL_Event event{};event.type=SDL_QUIT;SDL_PushEvent(&event);}}
int main(int argc,char** argv){
 std::string project=argc>1?argv[1]:"projects/signals_lab/signals_lab.judasproj";
 bool loading=argc>2&&std::string(argv[2])=="load";int frames=0;double pausedAt=0;bool saved=false,loadRequested=false,loaded=false;unsigned rootSubscriptions=0;int revisit=0,adoptedFrame=0,restoreFrame=0;uint64_t saveRequest=0;uint64_t regionToken=0;EntityId regionReceiver=0;
 ApplicationControl c;c.hidden=true;c.frameSeconds=[&](float){return frames==41?0.f:(frames==22?1.f/20:1.f/60);};
 c.hostReady=[](EngineHost& h){SDL_GL_SetSwapInterval(0);h.GetWindow().SetTestInputMode(true);std::string e;h.Audio().Init(e,true);};
 c.beforeFrame=[&](EngineHost& h,RuntimeWorld&,InteractivePlay&){
  if(loading){if(frames==12||frames==13)QueueTestKey(h.GetWindow(),SDL_SCANCODE_F7,frames==12);return;}
  for(auto [at,key]:{std::pair<int,SDL_Scancode>{10,SDL_SCANCODE_B},{20,SDL_SCANCODE_T},{30,SDL_SCANCODE_ESCAPE},{35,SDL_SCANCODE_B},{40,SDL_SCANCODE_RETURN},{55,SDL_SCANCODE_ESCAPE},{66,SDL_SCANCODE_P},{80,SDL_SCANCODE_U},{86,SDL_SCANCODE_B},{95,SDL_SCANCODE_D},{97,SDL_SCANCODE_D}})
   if(frames==at||frames==at+1)QueueTestKey(h.GetWindow(),key,frames==at);
 };
 c.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& p){++frames;
  if(!w.Scripts()){if(frames>300){check(false,"script VM startup");quit();}return;}
  if(loading){
   auto* saves=w.SceneControl()->Saves(h.Resources());if(frames>14&&!loadRequested){loadRequested=true;}
   if(loadRequested&&state(w,20).value("restores",0)>0&&!restoreFrame)restoreFrame=frames;
   // Restored slots start independently; observe a settled outer frame, not a global start barrier.
   if(!loaded&&restoreFrame&&frames>restoreFrame+8){loaded=true;check(state(w,20)["count"]==4&&state(w,30).is_null(),"fresh-process M61 load restores counts and removed receiver");
    auto records=w.Scripts()->Capture();bool independent=false;for(auto& r:records)if(r.entity>=kRuntimeEntityIdBase)independent|=Json::parse(r.json).value("count",0)==1;
    check(independent,"fresh-process load restores independent prefab receiver state");
    unsigned expected=2;for(auto& r:records){auto value=Json::parse(r.json);if(value.contains("listening"))expected+=value["listening"].get<bool>()?2:1;}
    auto diag=state(w,90)["diag"];check(diag["subscriptions"]==expected&&diag["queuedEvents"]==0&&diag["accepted"]==0,"restore registers exactly once with empty transient queues");(void)saves;
    check(w.Scripts()->Diagnostics().empty(),"restore helper reconstructs owned subscriptions without faults");
    std::printf("LOADED %s\n",state(w,90).dump().c_str());quit();}
   if(frames>600){check(false,"fresh-process load completed");quit();}return;
  }
  if(frames==19){check(state(w,20)["count"]==1&&state(w,20,2)["count"]==1&&state(w,30)["count"]==1,"actual fixed boundary broadcasts to independent slots");
   const auto* d=w.RuntimeDefinition(20);check(d&&d->render&&d->render->runtimeMaterials.count("#0")&&d->render->runtimeMaterials.at("#0").overrides.baseColor.has_value(),"signal receiver changes real M72 material override");
  }
  if(frames==29)check(state(w,20)["count"]==2&&state(w,30)["count"]==1,"actual targeted send only changes left entity slots");
  if(frames==33){check(p.IsPaused(),"authored modal pauses gameplay");pausedAt=w.SimulationTimeSeconds();}
  if(frames==51){check(w.SimulationTimeSeconds()==pausedAt&&state(w,20)["count"]==2,"fixed lane waits while paused");check(state(w,90)["menuCount"]==1,"UI lane onUI signal drains while paused, including zero-step frame");}
  if(frames==60)check(!p.IsPaused()&&state(w,20)["count"]==4&&state(w,30)["count"]==3,"resume fixed messages after UI closure without gameplay leak");
  if(frames==93)check(state(w,20)["count"]==4&&!state(w,20)["listening"]&&state(w,30)["count"]==4,"unsubscribe leaves other receivers active");
  if(frames==100){check(!w.RuntimeDefinition(30)&&w.Scripts()->Diagnostics().empty(),"repeated receiver removal is safe and leaves lab controls active");
   if(!w.Scripts()->Diagnostics().empty()){quit();return;} // Report script faults before inspecting absent captured state.
   std::string e;auto* stream=w.SceneControl()->Streaming(w,e);check(stream!=nullptr,"ordinary registered world streaming service");if(stream)regionToken=stream->Request("annex",false,e,90,1);
  }
  if(frames>=100){std::string e;auto* stream=w.SceneControl()->Streaming(w,e);if(stream){
    auto active=stream->Resolve("annex",10);
    if(revisit==0&&active&&state(w,active).is_object()){
     regionReceiver=active;adoptedFrame=frames;revisit=5;
    }
    if(revisit==5&&frames>adoptedFrame+3){
     // Preserve a compatible living instance through generic root adoption.
     check(stream->Adopt(regionReceiver,"root",e),"compatible region adoption uses existing live instance");
     rootSubscriptions=state(w,90)["diag"]["subscriptions"].get<unsigned>();adoptedFrame=frames;revisit=4;
    }
    if(revisit==4&&frames>adoptedFrame+2){
     check(state(w,90)["diag"]["subscriptions"].get<unsigned>()==rootSubscriptions,"compatible adoption preserves subscriptions exactly once");
     stream->ReleaseRequester(90,1);check(stream->Unload("annex"),"normal region unload accepted after demand release");revisit=1;
    }
    if(revisit==1&&!stream->Resolve("annex",11)){regionToken=stream->Request("annex",false,e,90,1);revisit=2;}
    if(revisit==2&&stream->Resolve("annex",11)&&state(w,stream->Resolve("annex",11)).is_object()){
      check(w.Scripts()->Diagnostics().empty(),"region revisit reconstructs independent subscriptions");revisit=3;
      auto req=w.SceneControl()->Saves(h.Resources())->Request("save","signals","Signals Lab","{}",e);check(req!=0,"normal modern save request accepted");saveRequest=req;saved=true;
    }
   }}
  if(saved){auto* service=w.SceneControl()->Saves(h.Resources());auto* status=service->Status(saveRequest);
   if(status&&status->state=="completed"){
    std::printf("SAVED %s\n",state(w,20).dump().c_str());check(w.Scripts()->Diagnostics().empty(),"final application has no script faults");quit();saved=false;
   }else if(status&&status->state=="failed"){std::printf("SAVE_ERROR %s\n",status->error.c_str());check(false,"modern save completed");quit();saved=false;}
  }
  if(frames==1000){std::string error;for(auto& st:w.SceneControl()->Streaming(w,error)->Regions())std::printf("REGION %s %s %s\n",st.id.c_str(),st.state.c_str(),st.error.c_str());check(false,"stream/save application proof completed");quit();}
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 };
 char executable[]="judas";char* args[]={executable,project.empty()?nullptr:project.data()};Application app;auto rc=app.Run(project.empty()?1:2,args,&c);
 check(rc==0,"normal application exit");if(loading)check(loaded,"fresh-process restore observed");
 (void)regionToken;(void)regionReceiver;std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
