// M70 same-rig/same-binary performance sanity check. M56 keeps the actual scope
// samples; allocation observations count ordinary C++ new calls in the step,
// not QuickJS malloc, GPU allocations or process startup. No solver shortcut.
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "SceneSerialization.h"
#include "Project.h"
#include "GameSession.h"
#include "Simulation.h"
#include "SceneSession.h"
#include "PerformanceProfiler.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>

namespace {std::atomic<bool> observeNew{false};std::atomic<unsigned long long> newCalls{0};}
#if defined(_MSC_VER)
#define M70_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define M70_NOINLINE __attribute__((noinline))
#else
#define M70_NOINLINE
#endif
M70_NOINLINE void* operator new(std::size_t bytes){if(observeNew.load(std::memory_order_relaxed))newCalls.fetch_add(1,std::memory_order_relaxed);if(auto* p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
M70_NOINLINE void* operator new[](std::size_t bytes){return ::operator new(bytes);}
M70_NOINLINE void operator delete(void* p)noexcept{std::free(p);}M70_NOINLINE void operator delete[](void* p)noexcept{std::free(p);}
M70_NOINLINE void operator delete(void* p,std::size_t)noexcept{std::free(p);}M70_NOINLINE void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace fs=std::filesystem;using Json=nlohmann::json;
namespace {
Json Stats(std::vector<double> values){if(values.empty())return nullptr;std::sort(values.begin(),values.end());return Json{{"median",values[values.size()/2]},{"p95",values[size_t(std::ceil(values.size()*.95))-1]},{"maximum",values.back()}};}
FullBodyIKSettings IK(const SkeletalAsset& asset){
 FullBodyIKSettings settings;settings.bodyRoot="Pivot";settings.spine={"Vertebra0","Vertebra1"};settings.rootMin=glm::vec3(-.15f);settings.rootMax=glm::vec3(.15f);
 settings.chains={{"palmA",{"ShoulderA","ElbowA","PalmA"}},{"palmB",{"ShoulderB","ElbowB","PalmB"}},{"soleA",{"ThighA","ShinA","SoleA"}},{"soleB",{"ThighB","ShinB","SoleB"}}};
 const auto globals=PoseGlobalMatrices(asset.skeleton,asset.skeleton.rest);
 for(const auto& chain:settings.chains){auto node=FindSkeletonJoint(asset.skeleton,chain.joints.back());JointTransform transform;std::string error;DecomposeRigidPose(globals[size_t(node)],transform,error);FullBodyIKTarget target;target.id=target.chain=chain.id;target.position=glm::dvec3(transform.translation)+glm::dvec3(0,.025,-.015);target.orientation=transform.rotation;target.orientationWeight=1;settings.targets.push_back(target);}
 return settings;
}
PhysicalAnimationSettings Drives(){PhysicalAnimationSettings settings;PhysicalAnimationRegion region;region.id="selected";region.joints={"Vertebra0","Vertebra1","ShoulderA","ElbowA","PalmA","ShoulderB","ElbowB","PalmB"};region.stiffness=32;region.damping=5;region.maxTorque=24;settings.regions.push_back(region);return settings;}
bool Ready(RuntimeWorld& world,ResourceManager& resources,std::string& error){for(int i=0;i<30;++i){resources.WaitForAll();world.UpdateAnimations(0);if(WorldPersistence::Prepare(world,error))return true;}return false;}
bool Build(RuntimeWorld& world,const Scene& scene,ResourceManager& resources,Project& project,std::string& error){for(int attempt=0;attempt<30;++attempt){if(world.Build(scene,&resources,error,&project.Settings().classification,&project.Settings().navigation))return true;if(error!="loading")return false;resources.WaitForAll();}return false;}

// Human follow-up reproduction: keep both original scene instances, ordinary
// objects and the actual JS control path. Press the authored passive action;
// do not replace the rig, policy, fixed cadence or physics with a synthetic one.
int PassiveRepro(const fs::path& out,EngineHost& host,Project& project,const Scene& scene,const std::string& mode="passive",const std::string& sceneName="physical"){
 const bool active=mode=="active";
 const char* requestedSteps=active?std::getenv("JUDAS_M70_REPRO_STEPS"):nullptr;
 const char* passiveFirst=active?std::getenv("JUDAS_M70_PASSIVE_FIRST"):nullptr;
 const int steps=requestedSteps?std::clamp(std::atoi(requestedSteps),61,1200):(active?180:600);
 const int requestStep=passiveFirst?300:60;
 auto& resources=host.Resources();auto& window=host.GetWindow();std::string error;
 for(const auto& object:scene.Objects())if(object.render&&!object.render->meshAsset.empty())resources.GetMesh(object.render->meshAsset,error);
 resources.WaitForAll();RuntimeWorld world;
 if(!Build(world,scene,resources,project,error)||!Ready(world,resources,error)){std::puts(error.c_str());return 1;}
 world.legacyGameplay=false;world.viewportWidth=800;world.viewportHeight=450;
 world.SetSceneControl(std::make_shared<SceneSession>(project,fs::absolute("projects/character_lab/Scenes/"+sceneName+".judas").string()));
 GameSession session;if(!session.Begin(world,error)){std::puts(error.c_str());return 1;}
 auto& profiler=PerformanceProfiler::Get();profiler.Enable(true);profiler.RegisterThread("M70 passive human follow-up");profiler.Clear();
 Json result={{"fixture","unmodified character_lab/Scenes/"+sceneName+".judas: both original actors and ordinary objects, actual public JS "+mode+" control"},{"fixedDeltaSeconds",1.f/60},{active?"activePressStep":"passivePressStep",requestStep},{"rendering","not measured; fixed motion plus normal frame/presentation scripts execute"},{"steps",Json::array()}};
 if(passiveFirst)result["passivePressStep"]=60;
 std::ofstream progress;if(active)progress.open(out/"active-repro-progress.jsonl");
 std::vector<double> before,after;unsigned chunk=0;bool finite=true;
 for(int step=0;step<steps;++step){
  ProfileFrame frame("M70 passive human follow-up");window.Input().BeginFrame();
  if(passiveFirst){if(step==60)window.Input().SetPhysical("key:P",1);else if(step==61)window.Input().SetPhysical("key:P",0);}
  if(step==requestStep)window.Input().SetPhysical(active?"key:G":"key:P",1);else if(step==requestStep+1)window.Input().SetPhysical(active?"key:G":"key:P",0);
  world.UpdateUIScripts(&window.Input(),1.f/60);world.UpdateScripts(&window.Input(),1.f/60);window.Input().BeginFixedStep();
  const auto start=PerformanceProfiler::Now();StepPlayedWorld(session,window,1.f/60);const auto end=PerformanceProfiler::Now();
  const double fixed=double(end-start)/1e6;world.PresentationScripts(&window.Input(),1.f/60,.5f);
  auto s=world.Physics().LastStepStats();Json row={{"step",step},{"fixedStepMs",fixed},{"bodies",s.bodies},{"dynamic",s.dynamicBodies},{"awake",s.awakeBodies},{"sleeping",s.sleepingBodies},{"candidatePairs",s.candidatePairs},{"contactPoints",s.contactPoints},{"impactEvents",s.impactEvents},{"impactSafetyFallback",s.impactSafetyFallback},{"impactEventCapFallback",s.impactEventCapFallback},{"impactQueries",s.impactQueries},{"impactSearchIterations",s.impactSearchIterations},{"impactPeakIterations",s.impactPeakIterations},{"impactSearchLimit",s.impactSearchLimit},{"impactUncertifiedAdvances",s.impactUncertifiedAdvances},{"impactSamplingFallbacks",s.impactSamplingFallbacks},{"impactSamplingTests",s.impactSamplingTests},{"impactSamplingResolutionCaps",s.impactSamplingResolutionCaps},{"impactSamplingMaxInterval",s.impactSamplingMaxInterval},{"broadphaseMs",s.broadphaseMilliseconds},{"narrowphaseMs",s.narrowphaseMilliseconds},{"solverMs",s.solverMilliseconds},{"totalPhysicsMs",s.totalMilliseconds}};
  row["geometry"]={{"predicates",s.geometryPredicates},{"exactFallbacks",s.geometryExactFallbacks},{"numericGapFallbacks",s.geometryNumericGapFallbacks},{"unresolved",s.geometryUnresolved},{"orientationCacheHits",s.orientationCacheHits},{"orientationCacheRebuilds",s.orientationCacheRebuilds}};
  row["motionSegments"]=s.motionSegments;row["motionStorageBytes"]=s.motionStorageBytes;
  // Exact binary32 values are serialized losslessly as JSON numbers. This is
  // an authoritative outcome comparison, separate from profiler timings.
  row["mappedBodies"]=Json::array();
  for(auto id:{10,11})for(const auto& mapped:scene.Find(id)->ragdoll->bones){
   const auto entity=world.RagdollBody(id,mapped.joint);const auto body=world.RuntimeBody(entity);if(!body.IsValid())continue;
   const auto pose=world.Physics().GetTransform(body);const auto v=world.Physics().GetLinearVelocity(body);const auto w=world.Physics().GetAngularVelocity(body);
   row["mappedBodies"].push_back({{"owner",id},{"joint",mapped.joint},{"entity",entity},{"body",body.id},{"position",{pose.position.x,pose.position.y,pose.position.z}},{"rotation",{pose.rotation.w,pose.rotation.x,pose.rotation.y,pose.rotation.z}},{"linearVelocity",{v.x,v.y,v.z}},{"angularVelocity",{w.x,w.y,w.z}}});
  }
  row["allBodies"]=Json::array();
  for(const auto body:world.Physics().AliveBodies()){
   const auto pose=world.Physics().GetTransform(body);const auto v=world.Physics().GetLinearVelocity(body);const auto w=world.Physics().GetAngularVelocity(body);
   row["allBodies"].push_back({{"body",body.id},{"position",{pose.position.x,pose.position.y,pose.position.z}},{"rotation",{pose.rotation.w,pose.rotation.x,pose.rotation.y,pose.rotation.z}},{"linearVelocity",{v.x,v.y,v.z}},{"angularVelocity",{w.x,w.y,w.z}}});
  }
  for(auto id:{10,11}){const auto state=world.PhysicalAnimationSnapshot(id);row["mode"+std::to_string(id)]=PhysicalAnimationModeName(state.mode);auto* animation=world.RuntimeAnimation(id);if(animation)for(const auto& p:animation->finalPose.local)finite&=std::isfinite(p.translation.x)&&std::isfinite(p.rotation.w);}
  (step<requestStep?before:after).push_back(fixed);frame.End();
  for(const auto& scope:profiler.Snapshot(profiler.FrameId()).scopes)if(std::string_view(scope.name).rfind("Physics ",0)==0)row["scopesMs"][scope.name.c_str()]=double(scope.end-scope.start)/1e6;
  if(active){progress<<row.dump()<<'\n';progress.flush();}
  result["steps"].push_back(std::move(row));
  if((step+1)%(active?20:120)==0){if(!profiler.Export((out/(std::string(active?"m56-active-repro-":"m56-passive-repro-")+std::to_string(chunk++)+".json")).string(),error)){std::puts(error.c_str());return 1;}}
 }
 result[active?"beforeActiveFixedMs":"beforePassiveFixedMs"]=Stats(before);result[active?"afterActiveFixedMs":"afterPassiveFixedMs"]=Stats(after);result["finitePoses"]=finite;result["scriptDiagnostics"]=Json::array();if(world.Scripts())for(const auto& d:world.Scripts()->Diagnostics())result["scriptDiagnostics"].push_back(d.message);
 std::ofstream(out/(active?"active-repro.json":"passive-repro.json"))<<result.dump(2)<<'\n';std::printf("M70 %s real scene before=%s after=%s finite=%d diagnostics=%zu\n",mode.c_str(),result[active?"beforeActiveFixedMs":"beforePassiveFixedMs"].dump().c_str(),result[active?"afterActiveFixedMs":"afterPassiveFixedMs"].dump().c_str(),finite,result["scriptDiagnostics"].size());
 session.End();world.EndScripts();world.Destroy();return finite&&result["scriptDiagnostics"].empty()?0:1;
}
}
int main(int argc,char** argv){
 const std::string option=argc==3?argv[2]:"";const bool active=option=="active-wall-repro"||option=="active-physical-repro";
 if(argc<2||argc>3||(argc==3&&option!="passive-repro"&&!active)){std::fprintf(stderr,"usage: %s <output-directory> [passive-repro|active-wall-repro|active-physical-repro]\n",argv[0]);return 2;}const auto out=fs::absolute(argv[1]);fs::create_directories(out);
 std::string error;EngineHost host;if(!host.Init("M70 same rig performance",640,360,false,error)){std::puts(error.c_str());return 1;}const auto root=fs::absolute("projects/character_lab");host.OpenProjectAssets(root.string(),(root/"Assets").string());auto& resources=host.Resources();auto& window=host.GetWindow();window.SetTestInputMode(true);
 const std::string sceneName=option=="active-wall-repro"?"wall":"physical";
 Project project;Scene source;if(!project.Load((root/"character_lab.judasproj").string(),error)||!LoadSceneFromFile((root/("Scenes/"+sceneName+".judas")).string(),source,error)){std::puts(error.c_str());return 1;}
 const auto model=source.Find(10)->render->meshAsset;resources.GetMesh(model,error);resources.WaitForAll();window.Input().SetMap(project.Settings().input,error);
 if(argc==3)return PassiveRepro(out,host,project,source,active?"active":"passive",sceneName);
 auto& profiler=PerformanceProfiler::Get();profiler.Enable(true);profiler.RegisterThread("M70 lab performance");Json result={{"fixture","character_lab/figure: 65 source joints, 71 source hierarchy nodes, 72 normalized runtime nodes, 6 parts, 130 palette entries"},{"fixedDeltaSeconds",1.f/60},{"warmupSteps",60},{"sampleSteps",120},{"allocationScope","ordinary C++ new/new[] calls inside StepPlayedWorld; excludes C malloc/QuickJS, aligned new, startup, collection and rendering"},{"rendering","not measured: this bounded workload evaluates authoritative fixed-step pose/physics and palette generation"},{"cases",Json::array()}};
 for(int count:{1,10})for(const std::string mode:{"disabled","ik","partial","passive"}){
  Scene scene=source;scene.Objects().clear();auto floor=*source.Find(1);scene.InsertObject(floor);auto gravity=*source.Find(2);scene.InsertObject(gravity);
  std::vector<EntityId> actors;for(int index=0;index<count;++index){auto actor=*source.Find(10);actor.id=100+index;actor.name="Same imported fixture "+std::to_string(index);actor.transform.position={float(index%5)*2.5f-5,0,float(index/5)*3-1.5f};actor.scripts.clear();actor.ui.reset();actor.socket.reset();actor.prefabAsset.clear();actor.prefabRoot=actor.prefabSource=0;actor.animation->fullBodyIK.reset();actor.ragdoll->physicalAnimation=Drives();actor.ragdoll->physicalAnimation->enabled=false;scene.InsertObject(actor);actors.push_back(actor.id);}
  RuntimeWorld world;if(!Build(world,scene,resources,project,error)||!Ready(world,resources,error)){std::puts(error.c_str());return 1;}world.legacyGameplay=false;
  for(auto id:actors){auto* animation=world.RuntimeAnimation(id);if(!animation||!animation->asset)return 1;if(mode=="ik"&&!world.ConfigureFullBodyIK(id,IK(*animation->asset),error)){std::puts(error.c_str());return 1;}if(mode=="partial"||mode=="passive"){if(!world.ConfigurePhysicalAnimation(id,Drives(),error)){std::puts(error.c_str());return 1;}PhysicalAnimationRequest request;request.mode=mode=="partial"?PhysicalAnimationMode::Partial:PhysicalAnimationMode::Passive;request.motorHandoff=true;if(!world.RequestPhysicalAnimation(id,request,error)){std::puts(error.c_str());return 1;}}}
  GameSession session;if(!session.Begin(world,error))return 1;for(int i=0;i<60;++i){window.Input().BeginFixedStep();StepPlayedWorld(session,window,1.f/60);}profiler.Clear();
  std::vector<double> fixed,newObservations,ikMicros,iterations;size_t dynamicMin=SIZE_MAX,dynamicMax=0,awakeMin=SIZE_MAX,awakeMax=0;unsigned reached=0;bool finite=true;
  for(int step=0;step<120;++step){ProfileFrame frame(("M70-"+mode+"-"+std::to_string(count)).c_str());window.Input().BeginFixedStep();const auto allocationsBefore=newCalls.load(std::memory_order_relaxed);const auto start=PerformanceProfiler::Now();observeNew=true;StepPlayedWorld(session,window,1.f/60);observeNew=false;const auto end=PerformanceProfiler::Now();newObservations.push_back(double(newCalls.load(std::memory_order_relaxed)-allocationsBefore));fixed.push_back(double(end-start)/1e6);
   double cost=0,iterationSum=0;for(auto id:actors){auto* animation=world.RuntimeAnimation(id);if(animation&&animation->fullBodyIK){cost+=animation->fullBodyIK->result.solveMicroseconds;iterationSum+=animation->fullBodyIK->result.iterations;reached+=animation->fullBodyIK->result.converged;}if(animation)for(const auto& transform:animation->finalPose.local)finite&=std::isfinite(transform.translation.x)&&std::isfinite(transform.rotation.w);}
   ikMicros.push_back(cost);iterations.push_back(iterationSum);auto dynamic=world.Physics().DynamicBodyCount(),awake=world.Physics().LastStepStats().awakeBodies;dynamicMin=std::min(dynamicMin,dynamic);dynamicMax=std::max(dynamicMax,dynamic);awakeMin=std::min(awakeMin,awake);awakeMax=std::max(awakeMax,awake);
  }
  std::vector<double> pose,drive,physics,conversion;for(const auto& sample:profiler.History()){double p=0,d=0,r=0,c=0;for(const auto& scope:sample.scopes){const auto ms=double(scope.end-scope.start)/1e6;if(scope.name=="Final pose resolution")p+=ms;else if(scope.name=="Physical animation drives")d+=ms;else if(scope.name=="Rigid physics")r+=ms;else if(scope.name=="Ragdoll physics to pose")c+=ms;}pose.push_back(p);drive.push_back(d);physics.push_back(r);conversion.push_back(c);}
  auto profileName="m56-"+mode+"-"+std::to_string(count)+".json";if(!profiler.Export((out/profileName).string(),error)){std::puts(error.c_str());return 1;}
  Json row={{"mode",mode},{"instances",count},{"fixedStepMs",Stats(fixed)},{"poseResolutionInclusiveMs",Stats(pose)},{"physicalDrivesMs",Stats(drive)},{"rigidPhysicsMs",Stats(physics)},{"physicsToPoseMs",Stats(conversion)},{"sharedIKSolveMicroseconds",Stats(ikMicros)},{"sharedIKIterationsSummedPerStep",Stats(iterations)},{"cppNewCallsPerStep",Stats(newObservations)},{"dynamicBodiesMin",dynamicMin},{"dynamicBodiesMax",dynamicMax},{"awakeBodiesMin",awakeMin},{"awakeBodiesMax",awakeMax},{"totalBodies",world.Physics().AliveBodyCount()},{"finiteFinalPoses",finite},{"convergedInstanceSamples",reached},{"m56Capture",profileName}};
  result["cases"].push_back(row);std::printf("M70 %-8s instances=%d fixed_ms median=%.6f p95=%.6f max=%.6f dynamic=%zu awake=%zu allocations_median=%.0f\n",mode.c_str(),count,row["fixedStepMs"]["median"].get<double>(),row["fixedStepMs"]["p95"].get<double>(),row["fixedStepMs"]["maximum"].get<double>(),dynamicMax,awakeMax,row["cppNewCallsPerStep"]["median"].get<double>());session.End();world.Destroy();if(!finite)return 1;
 }
 std::ofstream(out/"summary.json")<<result.dump(2)<<'\n';host.Shutdown();return 0;
}
