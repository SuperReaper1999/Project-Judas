#include "Application.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "InteractivePlay.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "ScriptSystem.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <cstdio>
using Clock=std::chrono::steady_clock;
int main(int argc,char** argv){
 if(argc!=2)return 2;
 unsigned frame=0,sample=0,requestFrame=0;uint64_t request=0;std::string state;bool failed=false;Clock::time_point requested;
 ApplicationControl c;c.hidden=true;c.frameSeconds=[](float){return 1.f/60;};
 c.hostReady=[](EngineHost& h){h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string e;h.Audio().Init(e,true);};
 const auto start=Clock::now();
 c.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){
  ++frame;auto quit=[](){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);};
  if(w.Scripts()&&!w.Scripts()->Diagnostics().empty()){failed=true;std::puts("FAIL original game script diagnostics");quit();return;}
  auto* service=w.SceneControl()->Saves(h.Resources());std::string error;
  if(!request&&frame>60&&!service->Busy()){
   if(sample==5){UIDocument modal;modal.modal=true;UIElement root;root.id="root";root.kind=UIKind::Canvas;modal.elements.push_back(root);w.UI().Add(modal,"save-measure-pause",0,error);}
   requested=Clock::now();requestFrame=frame;request=service->Request("save","latency-measure","Latency","{}",error);state.clear();
   if(!request){std::printf("FAIL request %s\n",error.c_str());failed=true;quit();return;}
  }
  if(request){const auto* s=service->Status(request);if(s&&s->state!=state){state=s->state;std::printf("PHASE sample=%u paused=%d state=%s frames=%u wall_ms=%.6f\n",sample,play.IsPaused(),state.c_str(),frame-requestFrame,std::chrono::duration<double,std::milli>(Clock::now()-requested).count());}
   if(s&&(state=="completed"||state=="failed")){
    std::printf("SAMPLE %u paused=%d frames=%u simulation_ms=%.3f wall_ms=%.6f capture_ms=%.6f worker_ms=%.6f bytes=%zu entities=%zu error=%s\n",sample,play.IsPaused(),frame-requestFrame,(frame-requestFrame)*1000./60,std::chrono::duration<double,std::milli>(Clock::now()-requested).count(),s->captureMs,s->workerMs,s->bytes,w.ScriptObjects().size(),s->error.c_str());
    std::printf("TIMINGS sample=%u elapsed_ms=%.6f queued_ms=%.6f identity_ms=%.6f worker_queue_ms=%.6f waiting_for=%s\n",sample,s->elapsedMs,s->queuedMs,s->identityMs,s->workerQueueMs,s->waitingFor.c_str());
    if(state=="failed"){failed=true;quit();return;}request=0;if(++sample==10)quit();
   }
  }
  if(std::chrono::duration<double>(Clock::now()-start).count()>30){failed=true;std::puts("FAIL bounded wall-time guard");quit();}
 };
 Application app;char name[]="judas";char* args[]={name,argv[1]};return app.Run(2,args,&c)||failed||sample!=10?1:0;
}
