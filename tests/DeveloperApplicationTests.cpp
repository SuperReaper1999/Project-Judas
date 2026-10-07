#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "WorldPersistence.h"
#include "WorldStreaming.h"
#include "Ragdoll.h"
#include <glm/gtc/matrix_transform.hpp>
#include "PerformanceProfiler.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <thread>
#include <cstdio>
namespace {unsigned checks=0,failures=0;void check(bool ok,const std::string& message){++checks;failures+=!ok;printf("%s %s\n",ok?"PASS":"FAIL",message.c_str());fflush(stdout);}}
int main(int argc,char** argv){if(argc!=4){std::fprintf(stderr,"usage: %s <project.judasproj> <mode> <output-dir>\n",argv[0]);return 2;}std::string mode=argv[2],out=argv[3],error;std::filesystem::create_directories(out);unsigned frame=0,phase=0;uint64_t request=0,token=0;RuntimeWorld* original=nullptr;bool frozen=false,restored=false;EntityId adopted=0;BodyHandle stale;glm::vec3 initial{0};
 setenv("JUDAS_PROFILE","1",1);ApplicationControl control;control.hidden=true;control.hostReady=[](EngineHost& h){h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string e;h.Audio().Init(e,true);};control.frameSeconds=[&](float){return frozen?0.f:1.f/60;};control.worldReady=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){original=&w;};
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){++frame;for(auto& d:w.Scripts()->Diagnostics()){check(false,"script "+d.callback+": "+d.message);w.UI().RequestQuit();return;}
 if(mode.rfind("bench-",0)==0||mode.rfind("pose-",0)==0){
  if(frame==5){unsigned count=unsigned(std::stoul(mode.substr(mode.rfind("pose-",0)==0?5:6)));for(unsigned i=200;i<205;++i)w.DestroyHierarchy(i,error);auto* record=h.Assets().FindByRelativePath(mode.rfind("pose-",0)==0?"Assets/prefabs/posed.judasprefab":"Assets/prefabs/rider.judasprefab");check(record!=nullptr,"shared articulation prefab for performance");if(record)for(unsigned i=0;i<count;++i){SceneTransform t;t.position={-12+float(i%10)*2.4f,1,-10-float(i/10)*4};check(w.SpawnPrefab(record->id,t,error)!=0,"normal articulated prefab spawn");}if(mode.find("reference")!=std::string::npos)w.Physics().SetSleepingEnabled(false);}
  if(frame==2200){unsigned asleep=0,total=0;for(auto b:w.Physics().AliveBodies())if(w.Physics().IsDynamicBody(b)){total++;asleep+=w.Physics().IsSleeping(b);}printf("WORKLOAD %s dynamic=%u sleeping=%u \n",mode.c_str(),total,asleep);if(mode.find("reference")==std::string::npos&&mode.rfind("pose-",0)!=0)check(asleep>=unsigned(std::stoul(mode.substr(mode.rfind("pose-",0)==0?5:6)))*4,"settled connected bodies sleep without lowering cadence");w.UI().RequestQuit();}return;
 }
 if(mode=="read"){
  if(frame==3){request=w.SceneControl()->Saves(h.Resources())->Request("load","m65-current","","{}",error);check(request!=0,"cold-process load through ordinary save service");}
  if(&w!=original&&!restored){restored=true;frozen=true;check(w.RuntimeDefinition(101)->animation->limbs.size()==2,"cold load retains authored/runtime IK targets");check(w.RuntimeDefinition(102)->socket&&w.RuntimeDefinition(102)->socket->target==101,"cold load restores stable socket identity");check(w.RuntimeJoint(401).IsValid(),"cold load restores script-created joint");check(w.RuntimeDefinition(400)->body->physicalMaterialOverride&&w.RuntimeDefinition(400)->body->friction==.9f,"cold load retains shared identity and explicit physical coefficients");SceneTransform hand;check(w.JointPose(101,"Hand","world",1,hand),"cold load resolved skeleton available");w.UI().RequestQuit();return;}
 }else if(mode=="stream"){
  if(frame==3){token=w.SceneControl()->Streaming(w,error)->Request("annex",false,error);check(token!=0,"normal streaming request");}
  auto* coordinator=w.SceneControl()->Streaming(w,error);auto status=coordinator->Status(token);
  if(status&&status->state=="active"&&phase==0){auto entity=coordinator->Resolve("annex",1);auto socket=coordinator->Resolve("annex",2);auto joint=coordinator->Resolve("annex",4);check(entity&&socket&&joint,"streamed skeleton/socket/joint published atomically");check(w.RuntimeDefinition(socket)->socket->target==entity,"stream socket reference remaps");check(w.RuntimeJoint(joint).IsValid(),"streamed physical joint active");stale=w.RuntimeBody(coordinator->Resolve("annex",3));coordinator->Release(token);coordinator->Unload("annex");phase=1;}
  if(phase==1&&frame>30){auto statuses=coordinator->Regions();bool pinned=false;for(auto& status:statuses)for(auto& pin:status.pins)pinned|=pin.find("pose/articulation")!=std::string::npos;check(pinned,"animated region honestly pins unsuspended pose state");adopted=coordinator->Resolve("annex",1);check(coordinator->Adopt(adopted,"root",error),"complete hierarchical socket/reference assembly adopts");coordinator->Unload("annex");phase=2;}
  if(phase==2&&coordinator->Resolve("annex",3)==0){check(!w.Physics().IsBodyEnabled(stale),"region unload retires body generation and runtime joint");check(w.RuntimeDefinition(adopted)!=nullptr,"adopted rig survives unload");token=coordinator->Request("annex",false,error);phase=3;}
  if(phase==3&&coordinator->Resolve("annex",3)){auto body=coordinator->Resolve("annex",3),joint=coordinator->Resolve("annex",4);check(w.RuntimeBody(body).id!=stale.id&&w.RuntimeJoint(joint).IsValid(),"revisit reconstructs independent body and stable joint reference");check(coordinator->Resolve("annex",1)==0,"adopted original does not duplicate on revisit");coordinator->Release(token);coordinator->Unload("annex");phase=4;}
  if(phase==4&&coordinator->Resolve("annex",3)==0){bool socket=false;for(auto& o:w.ScriptObjects())socket|=o.socket&&o.socket->target==adopted;check(socket&&w.RuntimeDefinition(adopted),"adopted final-pose socket survives repeated suspension");w.UI().RequestQuit();}

 }else{
  if(frame==10){check(SDL_GetWindowTitle(h.GetWindow().NativeWindow())==std::string("Judas Developer Integration"),"runtime window uses project title");initial=w.RuntimeDefinition(10)->transform.position;h.GetWindow().QueueTestPhysical("key:W",1);}
  if(frame==20){h.GetWindow().QueueTestPhysical("key:W",0);check(glm::length(w.RuntimeDefinition(10)->transform.position-initial)>.5f,"real M35 input drives project motor script");}
  if(frame==30){
   auto& renderer=h.GetRenderer();MaterialDefinition mat;mat.model=MaterialModel::Unlit;mat.maps[0].embedded={2,1,{0,0,0,255,255,255,255,255}};auto material=renderer.CreateMaterial(mat);std::vector<unsigned char> samples[2];
   for(int i=0;i<2;++i){MaterialOverride uv;uv.uvScale=glm::vec2(i?3:1);uv.uvOffset=glm::vec2(i?.2f:0);renderer.SetSceneAppearance(false,1,{},1,{1,0,0,0},false,{.2,.1,.3});renderer.BeginFrame(h.GetWindow().Width(),h.GetWindow().Height());renderer.SetCamera(glm::lookAt(glm::vec3(0,0,4),glm::vec3(0),glm::vec3(0,1,0)),glm::perspective(glm::radians(45.f),float(h.GetWindow().Width())/h.GetWindow().Height(),.1f,20.f));renderer.SetMaterialBindings({{material,uv}});renderer.DrawBox({0,0,0},{1,0,0,0},{1,1,.25f},{1,1,1});renderer.EndFrame();renderer.CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),samples[i]);}
   check(samples[0]!=samples[1],"actual GL instance UV transform changes texture sampling");check(std::abs(int(samples[0][0])-51)<2&&std::abs(int(samples[0][1])-26)<2&&std::abs(int(samples[0][2])-77)<2,"actual GL authored clear colour without HDR environment");renderer.DestroyMaterial(material);
   SceneTransform foot,hand;check(w.JointPose(101,"LeftFoot","world",1,foot)&&w.JointPose(101,"Hand","world",1,hand),"resolved joint queries through real resource pipeline");const auto* d=w.RuntimeDefinition(101);auto k=d->animation->limbs[0];check(glm::length(foot.position-k.target)<.025f,"moving board foot target reached without transitionFraction guess");check(glm::length(w.PresentedTransform(102,{},1).position-(hand.position+hand.rotation*glm::vec3(.15,0,0)))<1e-4,"visual socket follows resolved hand");auto* a=w.RuntimeAnimation(101);check((a->mixer.CrossFade(a->playback,"Wave",.3),a->mixer.CrossFade(a->playback,"Lean",.15),a->mixer.Transitioning()),"interrupted crossfade retains IK/socket resolver path");(void)a;
   auto old=w.RuntimeDefinition(10)->transform;old.position={-4,1,2};w.SetRuntimeTransform(10,old);}
  if(frame==34){bool observed=false;for(auto r:w.Scripts()->Capture())if(r.entity==11)observed=r.json.find("\"sensorEnter\":1")!=std::string::npos;check(observed,"ordinary sensor checkpoint callback detects motor");}
  if(frame==36){auto t=w.RuntimeDefinition(10)->transform;t.position={0,1,10};w.SetRuntimeTransform(10,t);}
  if(frame==40)h.GetWindow().QueueTestPhysical("key:J",1);
  if(frame==41)h.GetWindow().QueueTestPhysical("key:J",0);
  if(frame==45){check(w.RuntimeJoint(401).IsValid(),"public JS creates normal runtime joint");h.GetWindow().QueueTestPhysical("key:K",1);}
  if(frame==46)h.GetWindow().QueueTestPhysical("key:K",0);
  if(frame==50){JointState state;check(w.Physics().GetJoint(w.RuntimeJoint(401),state)&&std::abs(state.settings.anchorB.y-3)<1e-4,"public JS reanchors owner-local world joint");h.GetWindow().QueueTestPhysical("key:M",1);}
  if(frame==51)h.GetWindow().QueueTestPhysical("key:M",0);
  if(frame==55){std::string identity;float f,r;check(w.Physics().GetPhysicalMaterial(w.RuntimeBody(400),identity,f,r)&&f==.9f&&!identity.empty(),"shared physical material identity and instance control");}
  if(frame==65){check(w.EnterRagdoll(101,error),"ragdoll takes resolved IK/layer pose");}
  if(frame==75){SceneTransform hand;check(w.JointPose(101,"Hand","world",1,hand)&&glm::length(w.PresentedTransform(102,{},1).position-(hand.position+hand.rotation*glm::vec3(.15,0,0)))<.001f,"socket follows physical pose through same resolver");check(w.LeaveRagdoll(101,.25,error),"return contribution preserves visual pose");}
  if(frame==100){check(!w.RagdollActive(101),"visual return retires physical articulation");}
  if(frame==210&&mode=="write"){frozen=true;request=w.SceneControl()->Saves(h.Resources())->Request("save","m65-current","M65 current state","{}",error);check(request!=0,"save runtime IK/socket/joint/material state");}
  if(frame==2000&&mode=="probe"){unsigned asleep=0,total=0;for(auto b:w.Physics().AliveBodies())if(w.Physics().IsDynamicBody(b)){total++;asleep+=w.Physics().IsSleeping(b);}printf("SETTLED sleeping=%u/%u\n",asleep,total);for(auto b:w.Physics().AliveBodies())if(w.Physics().IsDynamicBody(b))printf("MOTION %u %.5f %.5f %d\n",b.id,glm::length(w.Physics().GetLinearVelocity(b)),glm::length(w.Physics().GetAngularVelocity(b)),w.Physics().IsSleeping(b));check(asleep>10,"ragdoll/debris demonstration settles connected physical bodies");w.UI().RequestQuit();}
 }
 if(request){auto* status=w.SceneControl()->Saves(h.Resources())->Status(request);if(status&&status->state=="failed"){check(false,"save/load: "+status->error);w.UI().RequestQuit();}if(status&&status->state=="completed"&&mode=="write"){check(true,"save completed with new normal participant state");w.UI().RequestQuit();}}
 if(frame>2400){check(false,"application proof timeout");w.UI().RequestQuit();}if(play.IsPaused()&&mode!="read")check(false,"unexpected pause");std::this_thread::sleep_for(std::chrono::milliseconds(1));};
 control.beforeShutdown=[&](EngineHost&,RuntimeWorld&,InteractivePlay&){check(PerformanceProfiler::Get().Export(out+"/profile.json",error),"normal M56 profiling export");};auto result=Application{}.Run(2,argv,&control);printf("SUMMARY %u checks %u failures\n",checks,failures);return result?result:failures?1:0;}
