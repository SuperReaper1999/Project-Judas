// Real GL/resources/QuickJS lab controls, plus separate process save fixtures.
// This test driver is not shipped as a gameplay autopilot.
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "WorldPresentation.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "SceneFingerprint.h"
#include "GameSession.h"
#include "Simulation.h"
#include "SaveStorage.h"
#include "WorldStreaming.h"
#include "PerformanceProfiler.h"
#include <thread>
#include <chrono>
#include "third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
namespace fs=std::filesystem;using Json=nlohmann::json;
namespace {
int checks=0,failures=0;bool measure=false;Renderer* measuredRenderer=nullptr;std::vector<Json> measurements;std::ostream* stepTrace=nullptr;unsigned traceFrame=0;
void Check(bool good,const std::string& label){++checks;failures+=!good;std::cout<<(good?"PASS ":"FAIL ")<<label<<'\n';}
Json State(RuntimeWorld& w){if(w.Scripts())for(const auto& record:w.Scripts()->Capture())if(record.entity==90)return Json::parse(record.json);return {};}
bool Ready(RuntimeWorld& w,ResourceManager& resources,std::string& error){for(int i=0;i<30;++i){resources.WaitForAll();w.UpdateAnimations(0);if(WorldPersistence::Prepare(w,error))return true;if(error!="loading")return false;}return false;}
void Step(RuntimeWorld& w,GameSession& game,Window& window,ResourceManager& resources,int count=1,const char* control=nullptr,float value=0){
 for(int i=0;i<count;++i){
  auto& profiler=PerformanceProfiler::Get();const auto begin=PerformanceProfiler::Now();if(measure)profiler.BeginFrame("M77 controlled lab benchmark");
  window.Input().BeginFrame();if(i==0&&control)window.Input().SetPhysical(control,value);w.UpdateUIScripts(&window.Input(),1.f/60);w.UpdateScripts(&window.Input(),1.f/60);window.Input().BeginFixedStep();
  const auto fixedBegin=PerformanceProfiler::Now();StepPlayedWorld(game,window,1.f/60);const auto fixedEnd=PerformanceProfiler::Now();w.PresentationScripts(&window.Input(),1.f/60,.5f);resources.WaitForAll();
  if(stepTrace){auto* b=w.PhysicalBalanceSnapshot(12);if(b){const auto& h=b->controller.History();const auto& o=b->controller.Observation();auto vec=[](glm::vec3 v){return Json::array({v.x,v.y,v.z});};Json feet=Json::array();for(const auto& e:b->controller.Settings().effectors){auto body=b->controller.Bodies()[e.foot].body;feet.push_back(vec(w.Physics().GetTransform(body).position));}*stepTrace<<Json({{"frame",traceFrame++},{"status",int(h.progress)},{"reason",b->diagnostic},{"swing",h.swing},{"lastSwing",h.lastSwing},{"landings",h.landings},{"lifted",h.lifted},{"transfer",h.transferTime},{"swingTime",h.swingTime},{"com",vec(o.com)},{"root",vec(w.Physics().GetTransform(b->controller.Bodies()[0].body).position)},{"velocity",vec(o.velocity)},{"height",o.height},{"up",o.upright},{"contacts",{o.supports[0].contacts,o.supports[1].contacts}},{"loads",{o.supports[0].load,o.supports[1].load}},{"feet",feet},{"start",vec(h.startLocal)},{"landing",vec(h.landingLocal)},{"stance",vec(h.stanceLocal)}}).dump()<<'\n';}}
  const auto renderBegin=PerformanceProfiler::Now();
  if(measure){auto view=w.view->pose;measuredRenderer->BeginFrame(800,450);RenderWorldFrame(*measuredRenderer,800,450,w,&game,glm::inverse(glm::translate(glm::mat4(1),view.position)*glm::mat4_cast(view.rotation)),glm::perspective(glm::radians(55.f),800.f/450,.08f,250.f),view.position,.5f);measuredRenderer->EndFrame();}
  const auto end=PerformanceProfiler::Now();if(measure){profiler.EndFrame();Json sample={{"frame",measurements.size()},{"frame_wall_ms",double(end-begin)/1e6},{"fixed_wall_ms",double(fixedEnd-fixedBegin)/1e6},{"render_cpu_ms",double(end-renderBegin)/1e6}};
   auto snapshot=profiler.Snapshot(profiler.FrameId());for(const auto& scope:snapshot.scopes){const std::string key=std::string(scope.name.c_str())+" inclusive_ms";sample[key]=sample.value(key,0.)+double(scope.end-scope.start)/1e6;}measurements.push_back(std::move(sample));}
 }

}
void Pulse(RuntimeWorld& w,GameSession& game,Window& window,ResourceManager& resources,const char* key){Step(w,game,window,resources,1,key,1);Step(w,game,window,resources,1,key,0);}
void Metrics(RuntimeWorld& w,EntityId id){auto* b=w.PhysicalBalanceSnapshot(id);if(!b){std::cout<<"ACTOR "<<id<<" no controller\n";return;}const auto& o=b->controller.Observation();auto root=b->controller.Bodies().empty()?BodyHandle{}:b->controller.Bodies()[0].body;auto p=w.Physics().GetTransform(root);std::cout<<"ACTOR "<<id<<" status="<<int(b->controller.History().progress)<<" root="<<p.position.x<<","<<p.position.y<<","<<p.position.z<<" height="<<o.height<<" up="<<o.upright<<" speed="<<o.speed<<" supports="<<o.supports[0].contacts<<"/"<<o.supports[1].contacts<<" landings="<<b->controller.History().landings<<" reason="<<b->diagnostic<<'\n';}
bool Standing(RuntimeWorld& w,EntityId id){auto* b=w.PhysicalBalanceSnapshot(id);if(!b||!b->configured)return false;const auto& o=b->controller.Observation();return o.available&&o.upright>.96f&&std::abs(o.height-b->controller.Settings().standingHeight)<b->controller.Settings().standingTolerance&&o.speed<b->controller.Settings().standingSpeed&&o.supports[0].contacts&&o.supports[1].contacts&&w.Physics().IsDynamicBody(b->controller.Bodies()[0].body);}
}
int main(int argc,char** argv){
 if(argc!=3){std::cerr<<"usage: balance_lab_tests <owned-output> standing|supports|recovery|radial|cold-write|cold-read\n";return 2;}
 const fs::path out=fs::absolute(argv[1]),root=out/"project";const std::string mode=argv[2];const bool read=mode.find("cold-")==0&&mode.size()>=4&&mode.compare(mode.size()-4,4,"read")==0,write=mode.find("cold-")==0&&mode.size()>=5&&mode.compare(mode.size()-5,5,"write")==0;const bool recoverySave=mode.find("recovery-")!=std::string::npos,standingSave=mode.find("stand-")!=std::string::npos;const EntityId savedOwner=recoverySave?10:12;if(fs::exists(root)&&!read)return 2;
 fs::create_directories(out);if(!read)fs::copy("projects/balance_lab",root,fs::copy_options::recursive);
 if(mode=="cookbook")fs::copy_file("docs/judasjs/examples/physical-balance.js",root/"Assets/scripts/balance-example.js");
 if(mode=="fault"){auto path=root/"Assets/scripts/lab.js";std::ifstream file(path);std::string source((std::istreambuf_iterator<char>(file)),{});const std::string before="if(this.hud.modal)return;this.state.elapsed+=dt;";source.replace(source.find(before),before.size(),before+"if(this.state.elapsed>7)throw Error('M77 deliberate game-slot fault');");std::ofstream(path)<<source;}
 std::string error;EngineHost host;Check(host.Init("M77 Balance Lab checks",800,450,false,error),"actual GL host: "+error);if(failures)return 1;
 host.OpenProjectAssets(root.string(),(root/"Assets").string());auto& resources=host.Resources();auto& window=host.GetWindow();window.SetTestInputMode(true);
 AssetRecord cookbookAsset;if(mode=="cookbook")Check(host.Assets().Track((root/"Assets/scripts/balance-example.js").string(),cookbookAsset,error),"cookbook uses ordinary asset registration");
 Project project;Check(project.Load((root/"balance_lab.judasproj").string(),error),"normal project loads: "+error);Check(window.Input().SetMap(project.Settings().input,error),"logical project controls: "+error);
 SaveStorage storage((out/"slots").string());GameSnapshot snapshot;bool recovered=false;Scene scene;const std::string label=mode=="variants"?"supports":mode=="blocked-recovery"?"recovery":mode=="low-grip"?"standing":mode.find("perf-")==0?(mode=="perf-recovery"?"recovery":"standing"):(mode=="lifecycle"||mode=="regions"||mode=="fault"||mode=="denied"||mode=="step-trace"||mode=="cookbook")?"standing":write||read?(recoverySave?"recovery":"standing"):mode;
 bool loaded=read?(storage.Read("m77",snapshot,recovered,error)&&!recovered&&WorldPersistence::SceneFrom(snapshot.participants,scene,error)):LoadSceneFromFile((root/"Scenes"/(label+".judas")).string(),scene,error);Check(loaded,"normal authored/captured scene: "+error);if(!loaded)return 1;
 // Benchmark copies use the same ordinary material on the fourth actor;
 // the shipped fourth actor remains a low-grip negative control.
 if(mode.find("perf-")==0)for(auto& bone:scene.Find(13)->ragdoll->bones)bone.friction=.8f;
 if(mode=="cookbook"){scene.Find(90)->scripts.clear();scene.Find(12)->scripts.push_back({1,cookbookAsset.id,true,"{}"});}
 if(mode=="regions"){Scene region;region.Objects().push_back(*scene.Find(1));Check(SaveSceneToFile(region,(root/"Scenes/supports.judas").string(),error),"normal region source saved");scene.Objects().erase(std::remove_if(scene.Objects().begin(),scene.Objects().end(),[](const auto& o){return o.id==1;}),scene.Objects().end());}
 // NEW review-only mixed-world fixture. All original M77 timing, inputs and acceptance remain untouched.
 // The independent M70 motor is a clone of the existing immutable rig content;
 // it has no script, balance task, active articulation, or interaction with M77.
 if(!read){auto independent=*scene.Find(10);independent.id=1000;independent.name="Independent M70 publication-isolation motor";independent.transform.position={100,3,0};independent.scripts.clear();independent.socket.reset();independent.ui.reset();independent.ragdoll->balance.reset();independent.ragdoll->physicalAnimation=PhysicalAnimationSettings{};independent.ragdoll->playOnStart=false;independent.characterMotor->enabled=true;scene.Objects().push_back(std::move(independent));}
 for(const auto& o:scene.Objects())if(o.render&&!o.render->meshAsset.empty())resources.GetMesh(o.render->meshAsset,error);
 resources.WaitForAll();RuntimeWorld world;
 bool built=false;for(int i=0;i<30&&!built;++i){built=world.Build(scene,&resources,error,&project.Settings().classification,&project.Settings().navigation,read);if(!built&&error!="loading")break;resources.WaitForAll();}
 Check(built&&Ready(world,resources,error),"normal immutable assets and world publish: "+error);if(!built)return 1;
 world.legacyGameplay=false;world.viewportWidth=800;world.viewportHeight=450;world.SetSceneControl(std::make_shared<SceneSession>(project,(root/"Scenes"/(label+".judas")).string()));GameSession game;
 std::unique_ptr<WorldStreaming> streaming;uint64_t floorDemand=0;EntityId streamedFloor=0;
 auto pump=[&](const std::function<bool()>& done){for(int n=0;n<1000;++n){streaming->Advance(false);if(done())return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;};
 if(mode=="regions"){WorldManifest manifest;WorldRegion region;region.id="floor";region.scene="Scenes/supports.judas";manifest.regions.emplace(region.id,region);streaming=std::make_unique<WorldStreaming>(world,resources,project,manifest);floorDemand=streaming->Request("floor",false,error);Check(pump([&]{return streaming->Status(floorDemand)->state=="active";}),"ordinary region asynchronously publishes support");streamedFloor=streaming->Resolve("floor",1);Check(streamedFloor&&streaming->Owner(streamedFloor)=="floor","support belongs to actual region");}
 if(mode.find("perf-")==0){
  Check(game.Begin(world,error),"ordinary benchmark game starts");Step(world,game,window,resources,40);
  const bool zero=mode=="perf-zero",passive=mode=="perf-passive",four=mode=="perf-four",recover=mode=="perf-recovery",activation=mode=="perf-activation";
  const std::vector<EntityId> actors=(four||passive)?std::vector<EntityId>{10,11,12,13}:std::vector<EntityId>{recover?10ull:12ull};
  if(!zero&&!activation){for(auto id:actors){PhysicalAnimationRequest request;request.mode=PhysicalAnimationMode::Passive;request.motorHandoff=true;Check(world.RequestPhysicalAnimation(id,request,error),"benchmark explicit passive authority");}Step(world,game,window,resources,120);}
  if(!zero&&!passive&&!recover&&!activation){for(auto id:actors)Check(world.RequestPhysicalBalance(id,{BalanceTask::Stand,true,true,true},error),"benchmark explicit native task");Step(world,game,window,resources,360);}
  if(zero||passive)Step(world,game,window,resources,360);
  if(recover||activation)Check(world.RequestPhysicalBalance(actors[0],{recover?BalanceTask::Recover:BalanceTask::Stand,true,true,true},error),"benchmark task starts at measured activation boundary");
  if(mode=="perf-step")Pulse(world,game,window,resources,"key:J");
  auto& profiler=PerformanceProfiler::Get();profiler.RegisterThread("M77 lab main");profiler.Enable(true);profiler.Clear();measure=true;measuredRenderer=&host.GetRenderer();Step(world,game,window,resources,recover?1080:600);measure=false;
  Json results={{"mode",mode},{"cadence_hz",60},{"viewport",{800,450}},{"samples",measurements},{"rigid_bodies",world.Physics().AliveBodyCount()}};
  for(auto id:actors){Metrics(world,id);auto* b=world.PhysicalBalanceSnapshot(id);results["actors"].push_back({{"id",id},{"owns",world.PhysicalBalanceOwnsJoints(id)},{"standing",Standing(world,id)},{"status",b?int(b->controller.History().progress):0}});}
  std::ofstream(out/(mode+"-performance.json"))<<results.dump(2)<<'\n';Metrics(world,actors[0]);Check(!profiler.Diagnostics().droppedEvents,"bounded profiler records representative workload without dropped events");
 }
 else if(read){
  Check(WorldPersistence::Restore(world,snapshot.participants,error),"fresh-process participants restore: "+error);WorldPersistence::Published(world);auto* b=world.PhysicalBalanceSnapshot(savedOwner);const auto saved=Json::parse(snapshot.metadata);
  Check(b&&b->configured&&b->controller.History().elapsed==saved["elapsed"].get<float>()&&b->controller.History().referenceTime==saved["referenceTime"].get<float>(),"fresh process preserves step/recovery clocks without replay");
  Check(b&&!b->controller.Observation().available,"fresh controller does not fabricate saved contact observations");
  Check(game.Begin(world,error),"restored game starts: "+error);
  std::cout<<"MIXED_BOUNDARY restored_token_before="<<world.Physics().HasRestoredCompletedContactObservations(1.f/60)<<" first_clock="<<b->controller.History().elapsed<<" independent_enabled="<<world.RuntimeCharacter(1000)->settings.enabled<<'\n';
  Step(world,game,window,resources,1);
  std::cout<<"MIXED_BOUNDARY first_clock_after="<<b->controller.History().elapsed<<" progress="<<int(b->controller.History().progress)<<'\n';
  Step(world,game,window,resources,(recoverySave?1080:720)-1);Metrics(world,savedOwner);Check(Standing(world,savedOwner),"restored native task reaches measured physical standing");
 }else{
  Check(game.Begin(world,error),"ordinary game starts: "+error);Step(world,game,window,resources,40);Check(!world.RagdollActive(12)&&!world.PhysicalBalanceSnapshot(12),"authored configuration does not activate physical authority");
  auto* a=world.RuntimeAnimation(10);auto* shared=world.RuntimeAnimation(13);auto* other=world.RuntimeAnimation(11);
  Check(a&&shared&&other&&a->asset==shared->asset&&a->asset!=other->asset,"shared immutable asset and distinct second rig use ordinary resource sharing");
  if(write&&recoverySave){Pulse(world,game,window,resources,"key:Tab");Pulse(world,game,window,resources,"key:Tab");Pulse(world,game,window,resources,"key:P");Step(world,game,window,resources,120);Pulse(world,game,window,resources,"key:G");Step(world,game,window,resources,90);}
  else {
  if(label=="radial"){Pulse(world,game,window,resources,"key:Tab");Pulse(world,game,window,resources,"key:Tab");}
  if(label=="standing"||label=="radial"){Pulse(world,game,window,resources,"key:P");Step(world,game,window,resources,120);}
  if(mode=="denied")Pulse(world,game,window,resources,"key:T");
  Pulse(world,game,window,resources,"key:B");Step(world,game,window,resources,label=="radial"?1080:360);const EntityId actor=label=="radial"?10:12;Metrics(world,actor);
  Check(world.RagdollActive(actor)&&world.PhysicalBalanceOwnsJoints(actor)&&!world.RuntimeCharacter(actor)->settings.enabled,"real JS request makes full dynamic authority with explicit motor handoff");
  Check(Standing(world,actor),"normal resource/JS path reaches actual standing");
  }
  if(write){
   if(!recoverySave&&!standingSave){Pulse(world,game,window,resources,"key:J");Step(world,game,window,resources,20);}
   auto* b=world.PhysicalBalanceSnapshot(savedOwner);Check(b&&b->configured,"capture contains an owned native task");
   if(!recoverySave&&!standingSave)Check(b&&b->controller.History().swing>=0,"capture while corrective step is actually in progress");
   snapshot.project=project.Settings().saveIdentity;snapshot.scene="Scenes/"+label+".judas";snapshot.displayName="M77 native task";snapshot.metadata=Json{{"elapsed",b->controller.History().elapsed},{"referenceTime",b->controller.History().referenceTime}}.dump();ComputeSceneFingerprint(scene,snapshot.content,error);
   Check(WorldPersistence::Capture(world,snapshot.participants,error)&&storage.Write("m77",snapshot,error),"atomic save retains physical and native task state: "+error);
   Check(snapshot.participants.at("physical-animation").version==2&&snapshot.participants.count("physical-balance"),"active task declares its required save participant");
   // Restore validation runs on disposable candidates, as the normal save system
   // does; a malformed participant must never partially publish into live play.
   const auto livePose=world.Physics().GetTransform(b->controller.Bodies()[0].body);
   for(int invalid=0;invalid<3;++invalid){
    auto broken=snapshot.participants;
    if(invalid==0)broken.erase("physical-balance");
    else if(invalid==1)broken.at("physical-balance").version=99;
    else broken.at("physical-balance").data.resize(3);
    RuntimeWorld candidate;std::string failure;
    Check(candidate.Build(scene,&resources,failure,&project.Settings().classification,&project.Settings().navigation,true)&&Ready(candidate,resources,failure),"detached restore candidate is ready");
    Check(!WorldPersistence::Restore(candidate,broken,failure)&&!failure.empty(),"missing/newer/truncated native task participant is rejected");candidate.Destroy();
   }
   const auto unchanged=world.Physics().GetTransform(b->controller.Bodies()[0].body);
   Check(glm::length(unchanged.position-livePose.position)<1e-6f,"failed detached restores leave live physical state untouched");

  }
  if(label=="standing"&&!write&&mode!="lifecycle"&&mode!="regions"&&mode!="fault"&&mode!="cookbook"&&mode!="low-grip"){
   std::ofstream trace;if(mode=="step-trace"){trace.open(out/"step-trace.jsonl");stepTrace=&trace;}Pulse(world,game,window,resources,"key:J");std::cout<<"POST IMPULSE "<<State(world).dump()<<'\n';Metrics(world,12);Step(world,game,window,resources,720);stepTrace=nullptr;Metrics(world,12);Check(mode=="denied"?(!Standing(world,12)&&world.PhysicalBalanceSnapshot(12)->controller.History().landings==0):(Standing(world,12)&&world.PhysicalBalanceSnapshot(12)->controller.History().landings>=1),mode=="denied"?"same public impulse with stepping denied exceeds standing envelope":"public human impulse causes real step and post-step standing");
  }
  if(label=="recovery"&&!write&&mode!="blocked-recovery"){
   // Human selection controls, never a native body repair.
   Pulse(world,game,window,resources,"key:Tab");Pulse(world,game,window,resources,"key:Tab");Pulse(world,game,window,resources,"key:P");Step(world,game,window,resources,120);Pulse(world,game,window,resources,"key:G");Step(world,game,window,resources,1080);Metrics(world,10);Check(Standing(world,10),"face-up authored robot gets up through public JS request");
   Pulse(world,game,window,resources,"key:Tab");Pulse(world,game,window,resources,"key:P");Step(world,game,window,resources,120);Pulse(world,game,window,resources,"key:G");Step(world,game,window,resources,1080);Metrics(world,11);Check(Standing(world,11),"different face-down rig gets up physically");
  }
  if(label=="radial"){
   Check(world.SetGravitySelection(10,GravitySelection{},error),"ordinary owner zero-gravity selection accepted");Step(world,game,window,resources,60);
   auto* native=world.PhysicalBalanceSnapshot(10);Check(glm::length(world.SampleEntityGravity(10,world.RuntimeDefinition(10)->transform.position))==0&&native&&glm::length(native->controller.Observation().gravity)>9,"physical balance samples actual bone gravity without inheriting owner override");
   const auto bone=world.RagdollBody(10,world.RuntimeDefinition(10)->ragdoll->bones[1].joint);
   Check(world.SetGravitySelection(bone,GravitySelection{},error),"ordinary mapped-body gravity selection accepted");Step(world,game,window,resources,5);Check(!world.PhysicalBalanceOwnsJoints(10),"incoherent/zero mapped-body gravity refuses physical standing honestly");
  }
  if(mode=="regions"){
   streaming->Release(floorDemand);streaming->Unload("floor");streaming->Advance(false);bool pinned=false;for(const auto& r:streaming->Regions())for(const auto& reason:r.pins)pinned|=reason=="external physical balance support/foothold";Check(pinned&&world.RuntimeDefinition(streamedFloor),"actual external balance support pins its source region");
   const auto bone=world.RagdollBody(12,world.RuntimeDefinition(12)->ragdoll->bones[0].joint);
   Check(!streaming->Adopt(bone,"floor",error),"individual transient physical bone adoption refuses");
   Check(streaming->Adopt(12,"floor",error)&&streaming->Owner(bone)=="floor"&&world.PhysicalBalanceOwnsJoints(12),"complete articulation adoption changes inferred bone ownership without retiring task");
   Check(streaming->Adopt(12,"root",error)&&streaming->Owner(bone)=="root","complete articulation returns to persistent ownership");
   const auto staleFloor=world.RuntimeBody(streamedFloor);for(EntityId id:{10,11,12,13})world.DestroyEntity(id,&error);
   Check(pump([&]{return streaming->Regions()[0].state=="unloaded";}),"retiring dependent actors releases region support pins");
   Check(!world.Physics().IsBodyEnabled(staleFloor),"region unload invalidates old support generation");floorDemand=streaming->Request("floor",false,error);Check(pump([&]{return streaming->Status(floorDemand)->state=="active";}),"support region revisits normally");Check(!world.Physics().IsBodyEnabled(staleFloor),"revisited support cannot alias old generation");
  }
  if(mode=="cookbook"){Check(Standing(world,12)&&world.PhysicalBalanceOwnsJoints(12),"copyable cookbook requests actual native standing");}
  if(mode=="fault"){Check(world.Scripts()&&!world.Scripts()->Diagnostics().empty(),"ordinary callback fault is isolated");Step(world,game,window,resources,360);Check(Standing(world,12)&&world.PhysicalBalanceOwnsJoints(12),"native owned task survives its initiating game-slot fault");}
  if(mode=="variants"){
   for(const auto id:{10ull,11ull,13ull}){
    const unsigned index=id==10?0:id==11?1:3;
    while(State(world).value("selected",2u)!=index)Pulse(world,game,window,resources,"key:Tab");
    Pulse(world,game,window,resources,"key:P");Step(world,game,window,resources,120);Pulse(world,game,window,resources,"key:B");Step(world,game,window,resources,1080);Metrics(world,id);
    Check(Standing(world,id),id==10?"authored uneven moving support reaches real standing":id==11?"authored slope reaches real standing":"authored oblique uniform zone and rotated support reach real standing");
   }
  }
  if(mode=="blocked-recovery"||mode=="low-grip"){
   while(State(world).value("selected",2u)!=3u)Pulse(world,game,window,resources,"key:Tab");
   Pulse(world,game,window,resources,"key:P");Step(world,game,window,resources,120);Pulse(world,game,window,resources,mode=="blocked-recovery"?"key:G":"key:B");Step(world,game,window,resources,1080);
   if(mode=="low-grip"){Pulse(world,game,window,resources,"key:J");Step(world,game,window,resources,1080);}
   Metrics(world,13);Check(!Standing(world,13)&&!world.PhysicalBalanceOwnsJoints(13),mode=="blocked-recovery"?"ordinary low ceiling prevents physical standing and reaches finite failure":"ordinary frictionless material refuses disturbance recovery without hidden braking");
  }
  if(mode=="lifecycle"){
   const auto elapsed=world.PhysicalBalanceSnapshot(12)->controller.History().elapsed;
   const auto referenceTime=world.PhysicalBalanceSnapshot(12)->controller.History().referenceTime;
   for(int n=0;n<20;++n){world.UpdateUIScripts(&window.Input(),1.f/60);world.PresentationScripts(&window.Input(),1.f/60,.5f);}
   Check(world.PhysicalBalanceSnapshot(12)->controller.History().elapsed==elapsed&&world.PhysicalBalanceSnapshot(12)->controller.History().referenceTime==referenceTime,"zero-step UI/presentation frames do not advance native task clocks");
   SceneTransform joint;Check(world.JointPose(12,"LeftStrut","world",1,joint)&&world.RuntimeDefinition(80)&&glm::length(world.RuntimeDefinition(80)->transform.position-(joint.position+joint.rotation*(joint.scale*glm::vec3(.15f,0,0))))<1e-4f,"normal visual socket follows actual resolved physical skeleton");
   Pulse(world,game,window,resources,"key:L");Step(world,game,window,resources,5);Check(world.RuntimeAnimation(12)->layers.size()==1&&Standing(world,12),"ordinary masked rest layer coexists without taking physical actuator authority");
   Check(!world.ConfigureFullBodyIK(12,std::nullopt,error)&&world.PhysicalBalanceOwnsJoints(12),"shared IK cannot replace M77 owned actuator references");
   auto* balance=world.PhysicalBalanceSnapshot(12);const auto rootBody=balance->controller.Bodies()[0].body;
   const auto actualVelocity=world.Physics().GetLinearVelocity(rootBody);
   PhysicalAnimationRequest blocked;blocked.mode=PhysicalAnimationMode::Animation;blocked.resumeMotor=true;
   blocked.placement=PhysicalAnimationPlacement{{3,-5,0},glm::quat(1,0,0,0)};
   Check(!world.RequestPhysicalAnimation(12,blocked,error)&&world.PhysicalBalanceOwnsJoints(12)&&world.RagdollActive(12),"blocked motor return preserves physical authority");
   Pulse(world,game,window,resources,"key:N");Step(world,game,window,resources,2);
   Check(!world.RagdollActive(12)&&world.RuntimeCharacter(12)->settings.enabled&&!world.PhysicalBalanceOwnsJoints(12),"public separate return retires physics and resumes motor");
   Check(!world.Physics().IsBodyEnabled(rootBody),"retired root handle cannot remain an active physical body");
   Check(glm::length(world.RuntimeCharacter(12)->velocity-actualVelocity)<.5f,"motor return does not invent an impact velocity");
   Pulse(world,game,window,resources,"key:O");Step(world,game,window,resources,10);Check(world.RagdollActive(12)&&world.PhysicalAnimationSnapshot(12).mode==PhysicalAnimationMode::Partial&&world.RuntimeCharacter(12)->settings.enabled,"public partial-physical compatibility retains motor root ownership");
   Pulse(world,game,window,resources,"key:O");Step(world,game,window,resources,20);Check(!world.RagdollActive(12)&&world.RuntimeCharacter(12)->settings.enabled,"partial compatibility returns through ordinary visual handoff");
   Pulse(world,game,window,resources,"key:Z");Step(world,game,window,resources,20);const auto spawnedState=State(world);
   Check(spawnedState.contains("spawned")&&spawnedState["spawned"].size()==1,"public prefab spawn creates an independent actor reference");
   Pulse(world,game,window,resources,"key:P");Step(world,game,window,resources,120);Pulse(world,game,window,resources,"key:B");Step(world,game,window,resources,360);
   EntityId instance=0;for(const auto& o:world.ScriptObjects())if(o.ragdoll&&o.id>90&&!world.IsTransientEntity(o.id))instance=o.id;
   Check(instance&&Standing(world,instance)&&!world.PhysicalBalanceOwnsJoints(12),"prefab instance owns independent native task state");
   const auto stale=instance?world.RagdollBody(instance,world.RuntimeDefinition(instance)->ragdoll->bones[0].joint):EntityId{};
   Check(instance&&world.DestroyEntity(instance,&error)&&!world.PhysicalBalanceSnapshot(instance)&&!world.RuntimeDefinition(stale),"owner destruction retires task and all mapped transient bodies");
  }
  if(!write&&mode!="lifecycle"&&mode!="regions"&&mode!="fault"&&mode!="cookbook"){Pulse(world,game,window,resources,"key:P");auto selected=State(world).value("selected",2);auto id=selected==0?10:selected==1?11:selected==2?12:13;Check(!world.PhysicalBalanceOwnsJoints(id)&&world.RagdollActive(id),"cancel releases actuators and preserves physical articulation");}
 }
 Check(world.Scripts()&&(mode=="fault"?!world.Scripts()->Diagnostics().empty():world.Scripts()->Diagnostics().empty()),"ordinary lab callback health matches deliberate fault policy");if(world.Scripts())for(const auto& d:world.Scripts()->Diagnostics())std::cout<<"SCRIPT "<<d.message<<'\n';
 auto& renderer=host.GetRenderer();auto view=world.view->pose;renderer.BeginFrame(800,450);RenderWorldFrame(renderer,800,450,world,&game,glm::inverse(glm::translate(glm::mat4(1),view.position)*glm::mat4_cast(view.rotation)),glm::perspective(glm::radians(55.f),800.f/450,.08f,250.f),view.position,.5f);renderer.EndFrame();Check(true,"real final pose/skinning and diagnostic rendering path executes");
 std::ofstream(out/(mode+"-state.json"))<<State(world).dump(2)<<'\n';game.End();world.EndScripts();streaming.reset();world.Destroy();Check(!world.IsBuilt(),"Stop destroys the owned runtime world");std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
}
