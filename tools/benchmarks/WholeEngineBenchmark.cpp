// The shipping application loop, including its real fixed-step accumulator,
// resource publication, rendering and swap. No synthetic simulation clock.
#include "Application.h"
#include "BuildIdentity.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "ScreenshotWriter.h"
#include "../../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <filesystem>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
using Json=nlohmann::json;
int main(int argc,char** argv){try{
 std::string project,out;unsigned frames=1800,warmup=240;bool profile=true;std::string key,pulse;double seconds=0,warmupSeconds=0,cap=0;
 for(int i=1;i<argc;++i){std::string a=argv[i];auto value=[&](){if(++i==argc)throw std::runtime_error("missing option value");return std::string(argv[i]);};
  if(a=="--project")project=value();else if(a=="--out")out=value();else if(a=="--frames")frames=std::stoul(value());else if(a=="--warmup")warmup=std::stoul(value());else if(a=="--key")key=value();else if(a=="--pulse")pulse=value();else if(a=="--seconds")seconds=std::stod(value());else if(a=="--warmup-seconds")warmupSeconds=std::stod(value());else if(a=="--cap")cap=std::stod(value());else if(a=="--no-profile")profile=false;else throw std::runtime_error("unknown option "+a);
 }
 if(project.empty()||out.empty()||frames<120||frames>100000||warmup>10000||seconds<0||seconds>300||warmupSeconds<0||warmupSeconds>30||cap<0||cap>1000||!std::isfinite(seconds)||!std::isfinite(warmupSeconds)||!std::isfinite(cap))throw std::runtime_error("--project FILE --out NEW_DIR [--frames 1800 --warmup 240 --key key:W --no-profile]");
 if(std::filesystem::exists(out))throw std::runtime_error("output directory already exists");
 std::filesystem::create_directories(out);
 SDL_setenv("JUDAS_PROFILE",profile?"1":"0",1);
#ifdef _WIN32
 _putenv_s("JUDAS_PROFILE_OUTPUT","");
#else
 unsetenv("JUDAS_PROFILE_OUTPUT");
#endif
 if(!std::getenv("JUDAS_PROFILE_DEPTH"))SDL_setenv("JUDAS_PROFILE_DEPTH","4",1);
 auto& p=PerformanceProfiler::Get();std::ofstream data(std::filesystem::path(out)/"frames.jsonl"),gpuData(std::filesystem::path(out)/"gpu.jsonl");unsigned samples=0;
 unsigned count=0;uint64_t previous=0,begin=0;JobStats first,last;int swapResult=0,swapInterval=0;std::string graphics,graphicsVersion;bool fault=false,pulsed=false;uint64_t sampleStart=0,lastResourceCheck=0,quietSince=0;bool resourcesReady=false,screenshotDone=false;Json calibration,finalCalibration;
 ApplicationControl control;
 control.hostReady=[&](EngineHost& h){swapResult=SDL_GL_SetSwapInterval(0);swapInterval=SDL_GL_GetSwapInterval();graphics=(const char*)glGetString(GL_RENDERER);graphicsVersion=(const char*)glGetString(GL_VERSION);h.GetWindow().SetTestInputMode(true);if(profile&&glGetInteger64v){GLint64 stamp=0;const auto a=p.Now();glGetInteger64v(GL_TIMESTAMP,&stamp);const auto z=p.Now();calibration={{"cpu_before_ns",a},{"gpu_ns",stamp},{"cpu_after_ns",z}};}};
 control.worldReady=[&](EngineHost& h,RuntimeWorld&,InteractivePlay& play){play.SetPointerCaptureAllowed(false);first=h.Jobs().Stats();};
 control.beforeFrame=[&](EngineHost& h,RuntimeWorld& world,InteractivePlay&){const auto now=p.Now();if(!begin)begin=now;
  if(!sampleStart&&now-lastResourceCheck>100000000){const auto loading=h.Resources().Stats().loading;if(loading)quietSince=0;else if(!quietSince)quietSince=now;resourcesReady=quietSince&&now-quietSince>=1000000000;lastResourceCheck=now;}
  const bool ready=screenshotDone&&resourcesReady&&(warmupSeconds>0?double(now-begin)/1e9>=warmupSeconds:count>warmup);const bool starting=ready&&!sampleStart;if(starting){sampleStart=now;first=h.Jobs().Stats();}const bool done=seconds>0?(sampleStart&&double(now-sampleStart)/1e9>=seconds):samples>=frames;
  if(previous&&ready&&!done&&!starting){Json row={{"index",samples},{"interval_ms",double(now-previous)/1e6}};
   if(profile){const auto f=p.Snapshot(p.FrameId()-1);row["frame_id"]=f.id;row["outer_ms"]=double(f.end-f.start)/1e6;row["waiting_ms"]=double(f.waiting)/1e6;row["collection_ms"]=double(f.collectionNanoseconds)/1e6;row["rss_bytes"]=f.processResidentBytes;row["steps"]=f.fixed.size();row["discarded_seconds"]=f.discardedSeconds;row["cap_reached"]=f.capReached;row["scope_dropped_total"]=f.diagnostics.droppedEvents;row["records_truncated_total"]=f.diagnostics.truncatedFrames;row["cpu_start_ns"]=f.start;row["cpu_end_ns"]=f.end;Json phases=Json::object();
    for(const auto& s:f.scopes)if(s.thread==f.mainThread){auto name=std::string(s.name.c_str());double ms=double(s.end-s.start)/1e6;phases[name]=phases.value(name,0.)+ms;}
    row["phases_ms"]=std::move(phases);
    if(p.FrameId()>10){auto old=p.Snapshot(p.FrameId()-8);Json passes=Json::array();for(const auto& g:old.gpu)passes.push_back({{"name",g.pass.c_str()},{"pending",g.pending},{"ms",g.milliseconds},{"submitted_cpu_ns",g.submitted},{"gpu_start_ns",g.gpuStart},{"gpu_end_ns",g.gpuEnd}});gpuData<<Json({{"frame_id",old.id},{"passes",passes}}).dump()<<'\n';}
   }data<<row.dump()<<'\n';++samples;
  }
  previous=now;++count;if(done){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}
  if(!key.empty())h.GetWindow().Input().SetPhysical(key,ready?1:0);
  if(!pulse.empty()){h.GetWindow().Input().SetPhysical(pulse,ready&&!pulsed?1:0);if(ready)pulsed=true;}
  if(auto* scripts=world.Scripts();scripts&&!scripts->Diagnostics().empty())fault=true;
 };
 control.afterFrame=[&](EngineHost& h,RuntimeWorld&,InteractivePlay&){if(!screenshotDone&&resourcesReady){std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);if(!WriteRgbPng((std::filesystem::path(out)/"workload.png").string(),h.GetWindow().Width(),h.GetWindow().Height(),pixels))throw std::runtime_error("workload preview capture failed");screenshotDone=true;}if(cap>0){JUDAS_PROFILE_WAIT("Benchmark FPS cap");const auto elapsed=double(p.Now()-previous)/1e6;const double remaining=1000./cap-elapsed;if(remaining>0)SDL_Delay(Uint32(remaining));}};
 control.beforeShutdown=[&](EngineHost& h,RuntimeWorld&,InteractivePlay&){last=h.Jobs().Stats();if(profile&&glGetInteger64v){GLint64 stamp=0;const auto a=p.Now();glGetInteger64v(GL_TIMESTAMP,&stamp);const auto z=p.Now();finalCalibration={{"cpu_before_ns",a},{"gpu_ns",stamp},{"cpu_after_ns",z}};}};
 std::vector<std::string> args={"whole-engine-benchmark",project};std::vector<char*> av;for(auto& a:args)av.push_back(a.data());int result=Application{}.Run(int(av.size()),av.data(),&control);
 std::ofstream meta(std::filesystem::path(out)/"run.json");meta<<Json({{"schema",1},{"project",project},{"frames",samples},{"requested_seconds",seconds},{"cap_hz",cap},{"gpu_clock_calibration",calibration},{"gpu_clock_calibration_end",finalCalibration},{"build_identity",Json::parse(BuildIdentityJson())},{"sample_start_steady_ns",sampleStart},{"sample_end_steady_ns",previous},{"warmup_wall_seconds",double(sampleStart-begin)/1e9},{"warmup_frames",warmup},{"profile",profile},{"wall_seconds",double(p.Now()-begin)/1e9},{"swap_set_result",swapResult},{"swap_interval",swapInterval},{"renderer",graphics},{"graphics_version",graphicsVersion},{"exit",result},{"script_fault",fault},{"resource_workers",last.workers},{"resource_worker_busy_seconds",last.workerBusySeconds-first.workerBusySeconds},{"resource_workers_elapsed_seconds",last.elapsedSeconds-first.elapsedSeconds},{"resource_jobs",last.submitted-first.submitted},{"resource_jobs_completed",last.completed-first.completed}}).dump(2);
 std::string error;if(profile&&!p.Export((std::filesystem::path(out)/"timeline-last120.json").string(),error))throw std::runtime_error(error);
 if(!data.good()||!gpuData.good()||!meta.good()||result||fault||(seconds==0?samples!=frames:samples<120)||swapInterval!=0)throw std::runtime_error("invalid run: exit, script fault, sample count or swap interval");
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
