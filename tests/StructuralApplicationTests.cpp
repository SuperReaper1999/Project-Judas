#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldPersistence.h"
#include "WorldStreaming.h"
#include "SaveService.h"
#include "ScreenshotWriter.h"
#include "PerformanceProfiler.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <thread>
#include <cstdio>
#include <algorithm>
static unsigned checks=0,failures=0;
static void check(bool v,const char* s){++checks;failures+=!v;std::printf("%s %s\n",v?"PASS":"FAIL",s);std::fflush(stdout);}
static Window* testWindow=nullptr;
static void key(SDL_Scancode k,bool down){testWindow->QueueTestPhysical(std::string("key:")+SDL_GetScancodeName(k),down?1.f:0.f);}
int main(int argc,char** argv){if(argc!=4)return 2;
 std::string project=argv[1],mode=argv[2],out=argv[3],error;std::filesystem::create_directories(out);
 unsigned frame=0,ready=0,phase=0,continuedAt=0,captureRetries=0;float restoredY=0;EntityId restoredPiece=0;int lastBroken=-1;bool frozen=false;RuntimeWorld* initial=nullptr;uint64_t epoch=0,region=0;EntityId spawned=0;GameSnapshot snapshot;std::vector<double> times;
 ApplicationControl c;c.hidden=true;c.frameSeconds=[&](float){return frozen?0.f:1.f/60;};
 c.hostReady=[&](EngineHost& h){testWindow=&h.GetWindow();h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string e;h.Audio().Init(e,true);};
 c.worldReady=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){initial=&w;};
 c.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){h.GetRenderer().ResetStats();
  if(mode=="read"&&phase==1){key(SDL_SCANCODE_F8,true);phase=2;}else if(mode=="read"&&phase==2)key(SDL_SCANCODE_F8,false);
  // The clock is frozen from the restored world's first frame. Its scripts
  // start at the next lifecycle boundary, so a capture refused for that
  // (retryable) reason is retried on the following frozen frames: the
  // compared physical state cannot have advanced.
  if(mode=="read"&&&w!=initial&&phase==2){frozen=true;std::map<std::string,SaveChunk> chunks;const bool captured=WorldPersistence::Capture(w,chunks,error);
   if(!captured&&error.find("retry after lifecycle boundary")!=std::string::npos&&++captureRetries<=10)return;
   check(chunks.count("deformables")&&chunks.at("deformables").data==snapshot.participants.at("deformables").data,"fresh-process restoration matches exact topology, physical poses and owned joint continuation");phase=3;}
 };
 c.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){++frame;
  auto* a=w.RuntimeDeformable(100,error);auto* b=w.RuntimeDeformable(200,error);auto* d=w.RuntimeDeformable(300,error);
  auto broken=[](const DeformableInstance* s){return s?int(std::count_if(s->fracture.broken.begin(),s->fracture.broken.end(),[](auto x){return x!=0;})):0;};
  bool healthy=w.Scripts()&&w.Scripts()->Diagnostics().empty();if(!healthy&&w.Scripts())for(auto& d:w.Scripts()->Diagnostics())std::printf("SCRIPT %s %s\n",d.callback.c_str(),d.message.c_str());
  if(!ready&&a&&b&&d&&w.FracturePartEntity(100,7)){ready=frame;epoch=a->generation;
   check(a->asset->fracture->structural&&b->asset->fracture->structural,"ordinary async structural resource publication");
   check(!w.RuntimeBody(100).IsValid(),"family has no duplicate body or mass");double mass=0;for(unsigned p=0;p<8;++p){auto id=w.FracturePartEntity(100,p);check(id&&w.Physics().IsDynamicBody(w.RuntimeBody(id)),"real safe dynamic fragment");mass+=w.RuntimeDefinition(id)->body->mass;}check(std::abs(mass-768)<.001,"independent density times volume mass oracle");
   if(mode=="read"){auto* service=w.SceneControl()->Saves(h.Resources());SaveStorage store(service->Root());bool recovered=false;check(store.Read("structure-current",snapshot,recovered,error),"read original on-disk structural save");phase=1;}
  }
  if(ready&&mode!="read"){
   unsigned t=frame-ready;
   if(broken(a)!=lastBroken){lastBroken=broken(a);std::printf("TOPOLOGY step=%u broken=%d revision=%llu\n",t,lastBroken,(unsigned long long)a->fracture.revision);}
   if(t==120){auto* rotated=w.RuntimeDeformable(400,error);check(rotated&&rotated->fracture.revision==1,"oblique independent structure carries gravity without world-up stress assumptions");
       auto def=w.RuntimeDefinition(400);auto expected=def->transform.rotation*glm::vec3(0,-9.81f,0);check(glm::length(w.Gravity().Sample(def->transform.position)-expected)<1e-4f,"ordinary authored oblique field rotates with its structural section");
       if(mode=="radial")check(w.Gravity().Sample({12,4,0}).x<-.5,"actual spatial radial acceleration used away from oblique zone");
   }
   if(t>20)times.push_back(play.LastFixedStepMilliseconds());
   if(t==120){check(a&&a->fracture.revision==1&&b->fracture.revision==1&&d->fracture.revision==1,"all bridges carry self weight and actual 20kg contact load");
    for(auto s:{a,b,d})std::printf("CALM peak_tension=%.9g broken=%d\n",*std::max_element(s->fracture.tensionDemand.begin(),s->fracture.tensionDemand.end()),broken(s));
    check(healthy,"public project JS including camera, input and UI runs");
    if(mode=="support"||mode=="write"||mode=="radial")key(SDL_SCANCODE_R,true);
    if(mode=="impact"){auto id=w.FracturePartEntity(100,4);auto body=w.RuntimeBody(id);auto pose=w.Physics().GetTransform(body);auto before=w.Physics().GetLinearVelocity(body);w.Physics().ApplyImpulseAtPoint(body,{0,-1200,0},pose.position+glm::vec3(.4f,.3f,0));check(glm::length(w.Physics().GetLinearVelocity(body)-before)>1,"real impact impulse at point");}
    if(mode=="overload")key(SDL_SCANCODE_B,true);
    if(mode=="lifecycle"){SceneTransform placement;placement.position={0,8,-7};spawned=w.SpawnPrefab("79797979797979797979797979790006",placement,error);check(spawned!=0,"public prefab path creates independent structure");region=w.SceneControl()->Streaming(w,error)->Request("annex",false,error);}
   }
   if(t==121&&mode=="overload")key(SDL_SCANCODE_B,false);
   if(t==121&&(mode=="support"||mode=="write"||mode=="radial"))key(SDL_SCANCODE_R,false);
   if(t==240&&(mode=="support"||mode=="write"||mode=="radial")){
    for(auto sim:{a,b,d})for(unsigned n=0;n<sim->fracture.broken.size();++n)std::printf("DEMAND family=%u bond=%u broken=%u tension=%.9g shear=%.9g\n",sim==a?100:sim==b?200:300,n,sim->fracture.broken[n],sim->fracture.tensionDemand[n],sim->fracture.shearDemand[n]);
    std::printf("REDISTRIBUTION weak=%d strong=%d middle=%d\n",broken(a),broken(b),broken(d));
    check(broken(a)>0&&std::any_of(a->fracture.broken.begin(),a->fracture.broken.end(),[](auto x){return x==1;}),"support loss alone causes physical failure under unchanged normal gravity");
    check(broken(b)<broken(a),"changing strength reduces failed connections under the same support loss");check(broken(d)<broken(a),"extra support reduces failed connections at the same strength");
    check(a->Mass()==768&&a->fracture.removed==std::vector<uint8_t>(8,0),"collapse retains original material mass with no decorative debris");
    double movement=0;for(unsigned p=0;p<8;++p){auto id=w.FracturePartEntity(100,p);movement=std::max(movement,double(4-w.Physics().GetTransform(w.RuntimeBody(id)).position.y));}
    check(movement>1,"detached colliders really fall and settle rather than disappearing or becoming decorative debris");
   }
   if(t==300&&(mode=="impact"||mode=="overload"))check(broken(a)>0,"applied physical load exceeds real material capacity");
   if(t==300&&mode=="lifecycle"){
    auto* f=w.RuntimeDeformable(spawned,error);check(f&&f->asset==a->asset&&f->generation!=a->generation,"prefab shares immutable cook with independent runtime state");
    auto* stream=w.SceneControl()->Streaming(w,error);auto annex=stream->Resolve("annex",1);check(annex&&w.FracturePartEntity(annex,0),"registered streamed region builds real structure");stream->Release(region);
    auto child=w.FracturePartEntity(spawned,4);auto body=w.RuntimeBody(child);w.DestroyEntity(spawned);check(!w.RuntimeDefinition(child)&&!w.Physics().IsDynamicBody(body),"owner destruction removes descendants and constraints safely");
   }
   if(t==340&&mode=="lifecycle"){auto* stream=w.SceneControl()->Streaming(w,error);check(stream->Regions().front().state=="active"&&!stream->Regions().front().pins.empty(),"live structure explicitly pins source residency");w.SceneControl()->Reload(error);phase=5;}
   if(t==360&&mode=="write"){key(SDL_SCANCODE_F5,true);phase=6;}
   if(t==361&&mode=="write")key(SDL_SCANCODE_F5,false);
   if(mode=="write"&&phase==6){auto* service=w.SceneControl()->Saves(h.Resources());for(uint64_t i=1;i<8;++i)if(auto* status=service->Status(i)){if(status->state=="completed"){check(true,"public JS saves collapsed structure");w.UI().RequestQuit();phase=9;}if(status->state=="failed"){std::printf("SAVE %s\n",status->error.c_str());check(false,"structural save");w.UI().RequestQuit();phase=9;}}}
   if(mode=="lifecycle"&&phase==5&&a->generation!=epoch){check(a->fracture.revision==1,"reload destroys all old topology and constraints");w.UI().RequestQuit();phase=9;}
   if(t==360&&mode!="write"&&mode!="lifecycle"){
    check(healthy&&!a->error.size()&&!a->fracture.error.size(),"simulation and public JS remain healthy");
    std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);WriteRgbPng((std::filesystem::path(out)/"lab.png").string(),h.GetWindow().Width(),h.GetWindow().Height(),pixels);
    std::sort(times.begin(),times.end());if(!times.empty())std::printf("PERF bodies=%zu fixed_median_ms=%.6f p95_ms=%.6f max_ms=%.6f draws=%u frames=%zu\n",w.Physics().AliveBodyCount(),times[times.size()/2],times[size_t(.95*(times.size()-1))],times.back(),h.GetRenderer().Stats().drawCalls,times.size());w.UI().RequestQuit();phase=9;
   }
  }
  if(mode=="read"&&phase==3){frozen=false;check(a&&broken(a)>0&&healthy,"restored physical collapse does not heal or replay invalid state");restoredPiece=w.FracturePartEntity(100,4);auto body=w.RuntimeBody(restoredPiece);restoredY=w.Physics().GetTransform(body).position.y;check(w.LoadFracturePart(100,4,{0,300,0},true),"restored real piece accepts a new ordinary impulse");continuedAt=frame;phase=4;}
  if(mode=="read"&&phase==4&&frame>=continuedAt+10){auto body=w.RuntimeBody(restoredPiece);check(w.Physics().GetTransform(body).position.y>restoredY+.1f,"cold-restored physics actually resumes, not merely cached topology status");check(a&&a->error.empty()&&healthy,"continued structural solve and scripts healthy");w.UI().RequestQuit();phase=9;}
  std::this_thread::sleep_for(std::chrono::milliseconds(1));if(frame>1000){check(false,"bounded deadline");w.UI().RequestQuit();}
 };
 Application app;char name[]="judas";char* args[]={name,project.data()};check(app.Run(2,args,&c)==0&&phase==9,"normal application startup and shutdown");std::printf("M79 %s checks=%u failures=%u\n",mode.c_str(),checks,failures);return failures?1:0;
}
