#include "Application.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "InteractivePlay.h"
#include "ScriptSystem.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <fstream>
#include <algorithm>
#include <sys/resource.h>
using Clock=std::chrono::steady_clock;using J=nlohmann::json;
int main(int argc,char** argv){const unsigned count=argc>1?unsigned(std::stoi(argv[1])):4;const bool split=argc>2&&std::string(argv[2])=="split";if((count!=2&&count!=4)||argc<4)return 2;Application app;ApplicationControl c;c.hidden=true;unsigned frame=0,maxBodies=0;bool observer=false,measuring=false;std::vector<double> frames,steps;const auto start=Clock::now();auto previous=start;J state;
 c.hostReady=[](EngineHost& h){h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);};
 c.beforeFrame=[&](EngineHost& h,RuntimeWorld& world,InteractivePlay&){++frame;if(frame==5)h.GetWindow().QueueTestPhysical(count==4?"key:F4":"key:F2",1);if(frame==7)h.GetWindow().QueueTestPhysical(count==4?"key:F4":"key:F2",0);if(split&&frame==9)h.GetWindow().QueueTestPhysical("key:V",1);if(split&&frame==11)h.GetWindow().QueueTestPhysical("key:V",0);
  static const char* keys[4][5]={{"W","S","A","D","Space"},{"Up","Down","Left","Right","Right Shift"},{"I","K","J","L","O"},{"T","G","F","H","Y"}};const double t=std::chrono::duration<double>(Clock::now()-start).count();unsigned i=0;if(auto* players=world.PlayersIfLoaded())for(auto& pair:players->All()){if(i>=4)break;const auto body=world.RuntimeBody(pair.second.entity);const auto p=world.Physics().GetTransform(body).position;const auto dx=std::cos(t+i)*7-p.x,dz=std::sin(t+i)*7-p.z;for(int j=0;j<5;++j){const float value=j==0?dz<-.3:j==1?dz>.3:j==2?dx<-.3:j==3?dx>.3:frame%180==0;h.GetWindow().QueueTestDevice("keyboard",std::string("key:")+keys[i][j],value);}++i;}
 };
 c.afterFrame=[&](EngineHost&,RuntimeWorld& world,InteractivePlay& play){const auto now=Clock::now();const double elapsed=std::chrono::duration<double>(now-start).count();if(!observer){play.SetFixedStepMeasurementFlags(true,false,false);play.SetFixedStepObserver([&](const FixedStepMeasurements&,double ms){if(measuring)steps.push_back(ms);});observer=true;}if(elapsed>2){measuring=true;frames.push_back(std::chrono::duration<double,std::milli>(now-previous).count());maxBodies=std::max(maxBodies,unsigned(world.Physics().AliveBodyCount()));}previous=now;if(elapsed>7){if(world.Scripts())for(auto& r:world.Scripts()->Capture())if(r.entity==950)state=J::parse(r.json);world.UI().RequestQuit();}};
 char exe[]="judas",project[]="projects/core_scramble/core_scramble.judasproj";char* args[]={exe,project};const int result=app.Run(2,args,&c);auto summarize=[](auto v){std::sort(v.begin(),v.end());return v.empty()?J::object():J{{"samples",v.size()},{"medianMs",v[v.size()/2]},{"p95Ms",v[std::min(v.size()-1,size_t(v.size()*.95))]},{"worstMs",v.back()}};};rusage usage{};getrusage(RUSAGE_SELF,&usage);std::ofstream(argv[3])<<J{{"players",count},{"split",split},{"method","Linux Release, same Core Scramble arena/physics, real clock logical-input pilot; hidden 1024x768 GL window, uncapped swap; 2s warm-up + 5s measurement; throughput not display refresh"},{"frame",summarize(frames)},{"fixed",summarize(steps)},{"maxBodies",maxBodies},{"peakRssBytes",uint64_t(usage.ru_maxrss)*1024},{"state",state},{"frameSamplesMs",frames},{"fixedSamplesMs",steps},{"result",result}}.dump(2)<<'\n';return result;}
