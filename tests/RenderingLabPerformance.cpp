// Actual wall-clock/swap-cadence application observation, not fixed test time.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "ScreenshotWriter.h"
#include "PerformanceProfiler.h"
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <cstdio>
int main(int argc,char**argv){std::filesystem::path out=argc>1?argv[1]:".cache/m92/watch";std::filesystem::create_directories(out);std::string project=argc>2?argv[2]:"projects/rendering_lab/rendering_lab.judasproj";
 std::ofstream csv(out/"frames.csv");csv<<"frame,submission_ms,interval_ms,process_cpu_ms,fixed_step_ms,draws,instances,lights,light_bytes,shadow_bytes\n";
 ApplicationControl control;control.hidden=true;unsigned frames=0;using Clock=std::chrono::steady_clock;auto start=Clock::now(),previous=start;double interval=0;std::clock_t cpu=0;
 control.hostReady=[](EngineHost& h){h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(1);};
 control.worldReady=[](EngineHost& h,RuntimeWorld&,InteractivePlay&){h.Resources().WaitForAll();};
 control.beforeFrame=[&](EngineHost& h,RuntimeWorld&,InteractivePlay&){h.GetRenderer().ResetStats();start=Clock::now();interval=std::chrono::duration<double,std::milli>(start-previous).count();previous=start;cpu=std::clock();};
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){++frames;auto s=h.GetRenderer().Stats();double elapsed=std::chrono::duration<double,std::milli>(Clock::now()-start).count(),cpuMs=1000.*double(std::clock()-cpu)/CLOCKS_PER_SEC;
  csv<<frames<<','<<elapsed<<','<<interval<<','<<cpuMs<<','<<play.LastFixedStepMilliseconds()<<','<<s.drawCalls<<','<<s.instances<<','<<s.dynamicLights<<','<<s.lightBytes<<','<<s.shadowBytes<<'\n';
  if(frames==360){std::vector<unsigned char> image;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),image);WriteRgbPng((out/"watch.png").string(),h.GetWindow().Width(),h.GetWindow().Height(),image);std::printf("WATCH size=%dx%d lights=%u draws=%u script_faults=%zu\n",h.GetWindow().Width(),h.GetWindow().Height(),s.dynamicLights,s.drawCalls,w.Scripts()->Diagnostics().size());w.UI().RequestQuit();}
 };
 std::string name="judas";char* args[]={name.data(),project.data()};Application app;int code=app.Run(2,args,&control);std::string error;PerformanceProfiler::Get().Export((out/"profile.json").string(),error);return code||frames!=360?1:0;
}
