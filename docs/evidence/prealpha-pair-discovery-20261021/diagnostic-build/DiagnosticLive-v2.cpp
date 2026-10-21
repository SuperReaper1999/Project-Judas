#include "StageCapture.h"
#include "MainThreadSampler.h"
#include "BuildIdentity.h"
#include <glad/gl.h>
#include <stdexcept>
// M87: measure the ordinary rendered project through its real application loop.
// CSV inspection/export runs outside the recorded fixed-step span. Explicit
// synthetic frame cadence is a reproducibility mode, never a displayed-FPS claim.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "PerformanceProfiler.h"
#include "Narrowphase.h"
#include "../third_party/nlohmann/json.hpp"
#include <set>
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

static void Environment(const char* key,const char* value){
#ifdef _WIN32
 _putenv_s(key,value?value:"");
#else
 if(value)setenv(key,value,1);else unsetenv(key);
#endif
}
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
// Optional M91 endpoint observation, outside all measured fixed-step spans.
// Real current narrowphase geometry, not finite-field or visual proxies.
static void quality(RuntimeWorld& world,const std::string& output){
 const auto begin=Clock::now();auto& physics=world.Physics();
 const int tag=world.Categories().tags.Find("loose_piece");
 nlohmann::json result={{"scope","endpoint primitive geometry for the authored loose_piece population"},{"outside_fixed_step_timing",true},{"tag_available",tag>=0}};
 double deepest=0;std::size_t contacts=0,uncertain=0,unsupported=0,population=0;
 std::set<std::pair<unsigned,unsigned>> visited;
 if(tag>=0)for(auto entity:world.QueryEntities(CategoryBit(unsigned(tag)))){
  const auto body=world.RuntimeBody(entity);Shape a;BodyTransform pa;
  if(!physics.GetBodyShape(body,a,pa))continue;
  ++population;
  if((a.type!=ShapeType::Box&&a.type!=ShapeType::Sphere)||a.pivotOffset!=glm::vec3(0)){++unsupported;continue;}
  const auto bound=ShapeAabb(a,pa.position,pa.rotation);
  PhysicsQueryFilter filter;filter.includeSensors=false;
  const auto neighbours=physics.QueryBodiesInAabb(bound.min,bound.max,filter);
  for(auto other:neighbours){
   if(other.id==body.id||!visited.insert(std::minmax(body.id,other.id)).second)continue;
   unsigned la=0,lb=0;CategoryMask ma=0,mb=0;
   if(!physics.GetCollisionFilter(body,la,ma)||!physics.GetCollisionFilter(other,lb,mb)||!CollisionPermitted(la,ma,lb,mb))continue;
   Shape b;BodyTransform pb;if(!physics.GetBodyShape(other,b,pb))continue;
   if((b.type!=ShapeType::Box&&b.type!=ShapeType::Sphere)||b.pivotOffset!=glm::vec3(0)){++unsupported;continue;}
   RigidBody ra,rb;ra.position=pa.position;ra.orientation=pa.rotation;rb.position=pb.position;rb.orientation=pb.rotation;
   const auto manifold=ComputeContacts(a,ra,b,rb,0);uncertain+=manifold.uncertain;
   contacts+=manifold.count;
   for(int i=0;i<manifold.count;++i)deepest=std::max(deepest,manifold.points[i].hasLocalAnchors?-manifold.points[i].signedSeparation:double(manifold.points[i].penetration));
  }
 }
 result.update({{"population",population},{"candidate_pairs",visited.size()},{"contact_points",contacts},{"uncertain_pairs",uncertain},{"unsupported_candidates",unsupported},{"deepest_endpoint_penetration_m",deepest},{"observation_ms",ms(Clock::now()-begin)}});
 std::ofstream(output+"/quality.json")<<result.dump(2)<<'\n';
}
int main(int argc,char** argv){
 if(argc<3){std::cerr<<"usage: live PROJECT OUTPUT_DIR [seconds=10] [frames=300] [swap=1] [tour=1] [hidden=1] [frame_seconds=0:measured] [profile=1] [periodic_export=0] [endpoint_quality=0]\n";return 2;}
 const std::string project=argv[1],output=argv[2];const double seconds=argc>3?std::stod(argv[3]):10.;
 const unsigned maximumFrames=argc>4?unsigned(std::stoul(argv[4])):300;
 const int swap=argc>5?std::stoi(argv[5]):1;const bool tour=argc>6?std::stoi(argv[6])!=0:true;
 const bool hidden=argc>7?std::stoi(argv[7])!=0:true;
 const float frameSeconds=argc>8?std::stof(argv[8]):0.f;const bool profile=argc>9?std::stoi(argv[9])!=0:true;
 const bool periodicExport=argc>10?std::stoi(argv[10])!=0:false;
 const bool endpointQuality=argc>11?std::stoi(argv[11])!=0:false;
 std::filesystem::create_directories(output);std::ofstream frames(output+"/frames.csv"),steps(output+"/steps.csv"),begins(output+"/begins.csv");
 const auto cpuPeriodUs=std::getenv("JUDAS_CPU_SAMPLE_PERIOD_US")?std::stoull(std::getenv("JUDAS_CPU_SAMPLE_PERIOD_US")):2000ull;
 sampler::Session cpuSampler(std::filesystem::path(output)/"sampling",std::getenv("JUDAS_CPU_SAMPLE")!=nullptr,cpuPeriodUs);
 if(std::getenv("JUDAS_STAGE_CAPTURE")&&!profile)throw std::runtime_error("stage capture requires existing profiler enabled");
 stage_capture::Session stageCapture(output,std::getenv("JUDAS_STAGE_CAPTURE")!=nullptr);
 std::ofstream camera(output+"/camera.csv");camera<<std::setprecision(12)<<"frame,wall_ms,x,y,z\n";
 frames<<std::setprecision(12);steps<<std::setprecision(12);begins<<std::setprecision(12);
 frames<<"frame,wall_ms,interval_ms,work_before_swap_ms,fixed_steps,last_fixed_ms,surface_ms,scene_cpu_ms,bodies,awake,sleeping,candidates,contacts,impact_events,impact_queries,impact_iterations,impact_peak,total_physics_ms,broadphase_ms,narrowphase_ms,solver_ms,draw_calls,triangles\n";
 steps<<"step,frame,wall_ms,fixed_ms,bodies,awake,sleeping,candidates,contacts,impact_events,impact_queries,impact_iterations,impact_peak,total_physics_ms,broadphase_ms,narrowphase_ms,solver_ms,trajectory_digest,max_linear_speed,max_angular_speed,nonfinite_fields\n";
 begins<<"frame,wall_ms,interval_ms\n";
 Environment("JUDAS_PROFILE",profile?"1":"0");Environment("JUDAS_TEST_SCRIPT",nullptr);
 Environment("JUDAS_PROFILE_OUTPUT",(output+"/profile-final.json").c_str());
 Environment("JUDAS_WORLD_STATE","none");
 unsigned frame=0,step=0;auto origin=Clock::now(),begin=origin,previous=origin;bool started=false;double currentInterval=0;bool finishRequested=false;
 ApplicationControl control;control.hidden=hidden;control.frameSeconds=[&](float measured){return finishRequested?0.f:(frameSeconds>0?frameSeconds:measured);};
 control.hostReady=[&](EngineHost& host){const auto* glVendor=glGetString(GL_VENDOR);const auto* glRenderer=glGetString(GL_RENDERER);const std::string vendor=glVendor?reinterpret_cast<const char*>(glVendor):"";const std::string renderer=glRenderer?reinterpret_cast<const char*>(glRenderer):"";std::cerr<<"NATIVE_GL vendor="<<vendor<<" renderer="<<renderer<<"\n";if(vendor.find("NVIDIA")==std::string::npos||renderer.find("NVIDIA")==std::string::npos)throw std::runtime_error("native NVIDIA renderer required for matched host performance run");std::cerr<<"BUILD_IDENTITY "<<BuildIdentityJson()<<"\n";host.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(swap);std::cerr<<"diagnostic swap="<<SDL_GL_GetSwapInterval()<<" hidden="<<hidden<<" tour="<<tour<<" frame_seconds="<<frameSeconds<<" profile="<<profile<<" periodic_export="<<periodicExport<<"\n";};
 control.beforeFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){stageCapture.AtFrameStart(frame);
  const auto now=Clock::now();if(!started){origin=now;previous=now;started=true;
   play.SetFixedStepObserver([&](const FixedStepMeasurements&,double){const auto& s=world.Physics().LastStepStats();const auto d=digest(world.Physics());steps<<++step<<','<<frame<<','<<ms(Clock::now()-origin)<<','<<play.LastFixedStepMilliseconds()<<','<<s.bodies<<','<<s.awakeBodies<<','<<s.sleepingBodies<<','<<s.candidatePairs<<','<<s.contactPoints<<','<<s.impactEvents<<','<<s.impactQueries<<','<<s.impactSearchIterations<<','<<s.impactPeakIterations<<','<<s.totalMilliseconds<<','<<s.broadphaseMilliseconds<<','<<s.narrowphaseMilliseconds<<','<<s.solverMilliseconds<<','<<d.hex()<<','<<d.maxSpeed<<','<<d.maxSpin<<','<<d.nonfinite<<'\n';steps.flush();cpuSampler.MarkCompletedStep(step);});
  }
  if(profile&&periodicExport&&frame&&frame%60==0){std::string error;if(!PerformanceProfiler::Get().Export(output+"/profile-"+std::to_string(frame)+".json",error))std::cerr<<error<<'\n';}
  begin=Clock::now();const double interval=ms(begin-previous);currentInterval=interval;previous=begin;
  begins<<frame<<','<<ms(begin-origin)<<','<<interval<<'\n';begins.flush();
  if((frame>=maximumFrames)||(ms(begin-origin)>=seconds*1000.)){finishRequested=true;world.UI().RequestQuit();}
  if(tour&&frame==0)host.GetWindow().QueueTestPhysical("key:F5",1);
  if(tour&&frame==1)host.GetWindow().QueueTestPhysical("key:F5",0);
  if(std::getenv("JUDAS_BENCH_WASD")){
   const char* keys[]={"key:W","key:D","key:S","key:A"};
   if(frame%180==0){for(const auto* key:keys)host.GetWindow().QueueTestPhysical(key,0);if(frame<720)host.GetWindow().QueueTestPhysical(keys[frame/180],1);}
  }
 };
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){const auto now=Clock::now();if(world.view)camera<<frame<<','<<ms(now-origin)<<','<<world.view->pose.position.x<<','<<world.view->pose.position.y<<','<<world.view->pose.position.z<<'\n';const auto&s=world.Physics().LastStepStats();const auto&r=host.GetRenderer().Stats();
  frames<<frame<<','<<ms(now-origin)<<','<<currentInterval<<','<<ms(now-begin)<<','<<play.LastFixedStepsThisFrame()<<','<<play.LastFixedStepMilliseconds()<<','<<play.LastSurfaceMilliseconds()<<','<<play.LastSceneMilliseconds()<<','<<s.bodies<<','<<s.awakeBodies<<','<<s.sleepingBodies<<','<<s.candidatePairs<<','<<s.contactPoints<<','<<s.impactEvents<<','<<s.impactQueries<<','<<s.impactSearchIterations<<','<<s.impactPeakIterations<<','<<s.totalMilliseconds<<','<<s.broadphaseMilliseconds<<','<<s.narrowphaseMilliseconds<<','<<s.solverMilliseconds<<','<<r.drawCalls<<','<<r.triangles<<'\n';frames.flush();++frame;
 };
 control.beforeShutdown=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){cpuSampler.Stop();stageCapture.AtShutdown(frame);if(endpointQuality)quality(world,output);std::string error;if(profile&&!PerformanceProfiler::Get().Export(output+"/profile-shutdown.json",error))std::cerr<<error<<'\n';std::cerr<<"completed frames="<<frame<<" steps="<<step<<" wall_ms="<<ms(Clock::now()-origin)<<'\n';};
 char* args[]={argv[0],const_cast<char*>(project.c_str())};return Application().Run(2,args,&control);
}
