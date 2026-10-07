// Short owner-thread/worker measurements, using the real outer application loop.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "PerformanceProfiler.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <thread>
#include <cstdio>
#include <filesystem>
using Clock=std::chrono::steady_clock;
int main(int argc,char** argv){
 if(argc!=3){std::fprintf(stderr,"usage: %s <project.judasproj> <output-dir>\n",argv[0]);return 2;}
 std::string project=argv[1],out=argv[2];std::filesystem::create_directories(out);
 unsigned frame=0,phase=0,samples=0;uint64_t request=0;bool failed=false,resume=false;Clock::time_point before;
 ApplicationControl control;control.hidden=true;
 control.hostReady=[](EngineHost& host){std::string error;host.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);host.Audio().Init(error,true);};
 control.frameSeconds=[&](float){return resume?1.f/60:0.f;};
 control.beforeFrame=[&](EngineHost&,RuntimeWorld&,InteractivePlay&){before=Clock::now();};
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  ++frame;std::string error;auto* service=world.SceneControl()->Saves(host.Resources());
  auto quit=[](){SDL_Event event{};event.type=SDL_QUIT;SDL_PushEvent(&event);};
  if(world.Scripts()&&!world.Scripts()->Diagnostics().empty()){failed=true;quit();return;}
  if(phase==0&&frame>60&&!service->Busy()){
   auto start=Clock::now();for(int i=0;i<1000;++i){auto unused=std::unique_ptr<RuntimeWorld>{};service->Advance(unused,play,error);}auto idle=std::chrono::duration<double,std::nano>(Clock::now()-start).count()/1000;
   std::printf("IDLE service_ns=%.5f entities=%zu bodies=%zu owners=%zu\n",idle,world.ScriptObjects().size(),world.Physics().AliveBodyCount(),world.Liquids().States().size());phase=1;
  }
  if(phase==1&&!service->Busy()){request=service->Request("save","performance","Measurement","{}",error);if(request)phase=2;}
  else if(phase==2||phase==4){auto* status=service->Status(request);if(status&&status->state=="failed"){std::printf("FAIL %s\n",status->error.c_str());failed=true;quit();return;}
   if(status&&status->state=="completed"){
    std::printf("SAMPLE %s capture=%.6f worker=%.6f restore=%.6f frame=%.6f bytes=%zu entities=%zu bodies=%zu owners=%zu rss=%llu\n",status->operation.c_str(),status->captureMs,status->workerMs,status->restoreMs,std::chrono::duration<double,std::milli>(Clock::now()-before).count(),status->bytes,world.ScriptObjects().size(),world.Physics().AliveBodyCount(),world.Liquids().States().size(),(unsigned long long)PerformanceProfiler::Get().Snapshot(PerformanceProfiler::Get().FrameId()-1).processResidentBytes);
    if(phase==2){request=service->Request("load","performance","","{}",error);if(request)phase=4;else{std::printf("FAIL %s\n",error.c_str());failed=true;quit();}}
    else {resume=true;phase=6;}
   }
  }
  if(phase==6&&resume&&service->Status(request)->state=="completed"){static unsigned resumedAt=0;if(!resumedAt)resumedAt=frame;else if(frame>resumedAt){std::printf("FIRST_RESUME frame_ms=%.6f entities=%zu bodies=%zu owners=%zu voices=%zu jobs=%zu\n",std::chrono::duration<double,std::milli>(Clock::now()-before).count(),world.ScriptObjects().size(),world.Physics().AliveBodyCount(),world.Liquids().States().size(),host.Audio().Diagnostics().voices,host.Resources().Jobs()->Stats().running);resumedAt=0;resume=false;if(++samples==7){std::puts("PASS seven save/load cycles complete with bounded native counts");quit();phase=5;}else phase=1;}}
  std::this_thread::sleep_for(std::chrono::milliseconds(1));if(frame>1800){failed=true;quit();}
 };
 Application app;char name[]="judas";char* args[]={name,project.data()};int result=app.Run(2,args,&control);return failed||result||phase!=5?1:0;
}
