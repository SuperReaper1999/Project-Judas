// Real-clock ordinary application workload; logical-input pilot, unchanged game.
#include "Application.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "InteractivePlay.h"
#include "RuntimeUI.h"
#include "ScriptSystem.h"
#include "PerformanceProfiler.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <fstream>
#include <algorithm>
#include <sys/resource.h>
using Clock=std::chrono::steady_clock;using J=nlohmann::json;
int main(){Application app;ApplicationControl control;control.hidden=true;std::vector<double> frames,steps;auto start=Clock::now(),previous=start;bool measured=false,observer=false;unsigned maxBodies=0,framesRendered=0;J last;
 control.hostReady=[](EngineHost& h){h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);};
 control.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){
  J state=J::object();if(w.Scripts())for(const auto& r:w.Scripts()->Capture())if(r.entity==50)state=J::parse(r.json);
  h.GetWindow().QueueTestPhysical("key:R",(state.value("phase","")=="won"||state.value("phase","")=="lost")?1:0);
  h.GetWindow().QueueTestPhysical("key:Space",state.value("phase","")=="ready"?1:0);
  const auto balls=w.QueryEntities(CategoryBit(2)),paddles=w.QueryEntities(CategoryBit(1));
  if(!balls.empty()&&!paddles.empty()){
   auto x=w.Physics().GetTransform(w.RuntimeBody(balls[0])).position.x-w.Physics().GetTransform(w.RuntimeBody(paddles[0])).position.x;
   h.GetWindow().QueueTestPhysical("stick:LeftX",std::clamp(x*1.8f,-1.f,1.f));}
 };
 control.afterFrame=[&](EngineHost&,RuntimeWorld& w,InteractivePlay& play){
  const auto now=Clock::now();const double elapsed=std::chrono::duration<double>(now-start).count();
  if(!observer){play.SetFixedStepMeasurementFlags(true,false,false);play.SetFixedStepObserver([&](const FixedStepMeasurements&,double ms){if(measured)steps.push_back(ms);});observer=true;}
  if(elapsed>2){measured=true;frames.push_back(std::chrono::duration<double,std::milli>(now-previous).count());++framesRendered;maxBodies=std::max(maxBodies,unsigned(w.Physics().AliveBodyCount()));}
  previous=now;
  if(elapsed>10){if(w.Scripts())for(const auto& r:w.Scripts()->Capture())if(r.entity==50)last=J::parse(r.json);w.UI().RequestQuit();}
 };
 char executable[]="judas",project[]="projects/breakwater/breakwater.judasproj";char* argv[]={executable,project};int result=app.Run(2,argv,&control);
 auto summary=[](std::vector<double> a){std::sort(a.begin(),a.end());if(a.empty())return J::object();return J{{"samples",a.size()},{"medianMs",a[a.size()/2]},{"p95Ms",a[std::min(a.size()-1,std::size_t(a.size()*.95))]},{"worstMs",a.back()}};};
 struct rusage usage{};getrusage(RUSAGE_SELF,&usage);J report={{"method","Real-clock Linux application; hidden 1024x768 native GL window, uncapped swap; logical-input pilot; 2s warm-up + 8s measurement. Throughput is not display refresh or phone performance."},{"frame",summary(frames)},{"fixedStep",summary(steps)},{"frameSamplesMs",frames},{"fixedSamplesMs",steps},{"maxBodies",maxBodies},{"peakRssBytes",uint64_t(usage.ru_maxrss)*1024},{"lastGameState",last},{"result",result}};
 std::ofstream(".cache/m88/linux-performance.json")<<report.dump(2)<<'\n';return result;}
