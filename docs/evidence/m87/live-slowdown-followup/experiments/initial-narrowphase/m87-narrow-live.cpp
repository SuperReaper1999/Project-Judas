// Temporary live-loop observer. Engine source and simulation remain unchanged.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "PerformanceProfiler.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <cmath>

extern "C" void JudasM87PrototypePhysicsJobs(JobSystem*,unsigned);
using Clock=std::chrono::steady_clock;
static double ms(Clock::duration d){return std::chrono::duration<double,std::milli>(d).count();}
struct PhysicalDigest {std::uint64_t hash=14695981039346656037ull;double maxSpeed=0,maxSpin=0;std::size_t nonfinite=0;
 void u32(std::uint32_t value){for(unsigned i=0;i<4;++i){hash^=(value>>(i*8))&255u;hash*=1099511628211ull;}}
 void scalar(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);u32(bits);if(!std::isfinite(value))++nonfinite;}
 void vector(glm::vec3 value){scalar(value.x);scalar(value.y);scalar(value.z);}
 std::string hex()const{std::ostringstream out;out<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;return out.str();}
};
static PhysicalDigest digest(PhysicsWorld& physics){PhysicalDigest d;auto bodies=physics.AliveBodies();std::sort(bodies.begin(),bodies.end(),[](auto a,auto b){return a.id<b.id;});d.u32(std::uint32_t(bodies.size()));
 for(auto body:bodies){d.u32(body.id);auto pose=physics.GetTransform(body);d.vector(pose.position);d.scalar(pose.rotation.w);d.scalar(pose.rotation.x);d.scalar(pose.rotation.y);d.scalar(pose.rotation.z);auto linear=physics.GetLinearVelocity(body),angular=physics.GetAngularVelocity(body);d.vector(linear);d.vector(angular);d.u32(physics.IsSleeping(body)?1:0);d.maxSpeed=std::max(d.maxSpeed,double(glm::length(linear)));d.maxSpin=std::max(d.maxSpin,double(glm::length(angular)));}return d;
}
int main(int argc,char** argv){
 if(argc<3){std::cerr<<"usage: live PROJECT OUTPUT_DIR [seconds=10] [frames=300] [swap=1] [tour=1] [hidden=1] [frame_seconds=0:measured] [profile=1] [periodic_export=0]\n";return 2;}
 const std::string project=argv[1],output=argv[2];const double seconds=argc>3?std::stod(argv[3]):10.;
 const unsigned maximumFrames=argc>4?unsigned(std::stoul(argv[4])):300;
 const int swap=argc>5?std::stoi(argv[5]):1;const bool tour=argc>6?std::stoi(argv[6])!=0:true;
 const bool hidden=argc>7?std::stoi(argv[7])!=0:true;
 const float frameSeconds=argc>8?std::stof(argv[8]):0.f;const bool profile=argc>9?std::stoi(argv[9])!=0:true;
 const bool periodicExport=argc>10?std::stoi(argv[10])!=0:false;
 std::filesystem::create_directories(output);std::ofstream frames(output+"/frames.csv"),steps(output+"/steps.csv"),begins(output+"/begins.csv");
 frames<<std::setprecision(12);steps<<std::setprecision(12);begins<<std::setprecision(12);
 frames<<"frame,wall_ms,interval_ms,work_before_swap_ms,fixed_steps,last_fixed_ms,surface_ms,scene_cpu_ms,bodies,awake,sleeping,candidates,contacts,impact_events,impact_queries,impact_iterations,impact_peak,total_physics_ms,broadphase_ms,narrowphase_ms,solver_ms,draw_calls,triangles\n";
 steps<<"step,frame,wall_ms,fixed_ms,bodies,awake,sleeping,candidates,contacts,impact_events,impact_queries,impact_iterations,impact_peak,total_physics_ms,broadphase_ms,narrowphase_ms,solver_ms,trajectory_digest,max_linear_speed,max_angular_speed,nonfinite_fields\n";
 begins<<"frame,wall_ms,interval_ms\n";
 setenv("JUDAS_PROFILE",profile?"1":"0",1);unsetenv("JUDAS_TEST_SCRIPT");
 setenv("JUDAS_PROFILE_OUTPUT",(output+"/profile-final.json").c_str(),1);
 setenv("JUDAS_WORLD_STATE","none",1);
 unsigned frame=0,step=0;auto origin=Clock::now(),begin=origin,previous=origin;bool started=false;double currentInterval=0;bool finishRequested=false;
 ApplicationControl control;control.hidden=hidden;control.frameSeconds=[&](float measured){return finishRequested?0.f:(frameSeconds>0?frameSeconds:measured);};
 control.hostReady=[&](EngineHost& host){const char* requested=std::getenv("M87_PARALLEL_WORKERS");JudasM87PrototypePhysicsJobs(&host.Jobs(),requested?unsigned(std::stoul(requested)):4);host.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(swap);std::cerr<<"diagnostic swap="<<SDL_GL_GetSwapInterval()<<" hidden="<<hidden<<" tour="<<tour<<" frame_seconds="<<frameSeconds<<" profile="<<profile<<" periodic_export="<<periodicExport<<"\n";};
 control.beforeFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  const auto now=Clock::now();if(!started){origin=now;previous=now;started=true;
   play.SetFixedStepObserver([&](const FixedStepMeasurements&,double){const auto& s=world.Physics().LastStepStats();const auto d=digest(world.Physics());steps<<++step<<','<<frame<<','<<ms(Clock::now()-origin)<<','<<play.LastFixedStepMilliseconds()<<','<<s.bodies<<','<<s.awakeBodies<<','<<s.sleepingBodies<<','<<s.candidatePairs<<','<<s.contactPoints<<','<<s.impactEvents<<','<<s.impactQueries<<','<<s.impactSearchIterations<<','<<s.impactPeakIterations<<','<<s.totalMilliseconds<<','<<s.broadphaseMilliseconds<<','<<s.narrowphaseMilliseconds<<','<<s.solverMilliseconds<<','<<d.hex()<<','<<d.maxSpeed<<','<<d.maxSpin<<','<<d.nonfinite<<'\n';steps.flush();});
  }
  if(profile&&periodicExport&&frame&&frame%60==0){std::string error;if(!PerformanceProfiler::Get().Export(output+"/profile-"+std::to_string(frame)+".json",error))std::cerr<<error<<'\n';}
  begin=Clock::now();const double interval=ms(begin-previous);currentInterval=interval;previous=begin;
  begins<<frame<<','<<ms(begin-origin)<<','<<interval<<'\n';begins.flush();
  if((frame>=maximumFrames)||(ms(begin-origin)>=seconds*1000.)){finishRequested=true;world.UI().RequestQuit();}
  if(tour&&frame==0)host.GetWindow().QueueTestPhysical("key:F5",1);
  if(tour&&frame==1)host.GetWindow().QueueTestPhysical("key:F5",0);
 };
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){const auto now=Clock::now();const auto&s=world.Physics().LastStepStats();const auto&r=host.GetRenderer().Stats();
  frames<<frame<<','<<ms(now-origin)<<','<<currentInterval<<','<<ms(now-begin)<<','<<play.LastFixedStepsThisFrame()<<','<<play.LastFixedStepMilliseconds()<<','<<play.LastSurfaceMilliseconds()<<','<<play.LastSceneMilliseconds()<<','<<s.bodies<<','<<s.awakeBodies<<','<<s.sleepingBodies<<','<<s.candidatePairs<<','<<s.contactPoints<<','<<s.impactEvents<<','<<s.impactQueries<<','<<s.impactSearchIterations<<','<<s.impactPeakIterations<<','<<s.totalMilliseconds<<','<<s.broadphaseMilliseconds<<','<<s.narrowphaseMilliseconds<<','<<s.solverMilliseconds<<','<<r.drawCalls<<','<<r.triangles<<'\n';frames.flush();++frame;
 };
 control.beforeShutdown=[&](EngineHost&,RuntimeWorld&,InteractivePlay&){std::string error;if(profile&&!PerformanceProfiler::Get().Export(output+"/profile-shutdown.json",error))std::cerr<<error<<'\n';std::cerr<<"completed frames="<<frame<<" steps="<<step<<" wall_ms="<<ms(Clock::now()-origin)<<'\n';};
 char* args[]={argv[0],const_cast<char*>(project.c_str())};return Application().Run(2,args,&control);
}
