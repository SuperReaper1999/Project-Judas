#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldPersistence.h"
#include "WorldStreaming.h"
#include "SaveService.h"
#include "ScreenshotWriter.h"
#include "UniformGravity.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <thread>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <fstream>
static unsigned checks=0,failures=0;
static void check(bool v,const char* s){++checks;failures+=!v;std::printf("%s %s\n",v?"PASS":"FAIL",s);std::fflush(stdout);}
int main(int argc,char** argv){if(argc!=4)return 2;
 std::string project=argv[1],mode=argv[2],out=std::filesystem::absolute(argv[3]).string(),error;std::filesystem::create_directories(out);
 unsigned frame=0,ready=0,phase=0;bool frozen=false;RuntimeWorld* original=nullptr;uint64_t epoch=0,region=0;EntityId oldAnnex=0,spawned=0;GameSnapshot snapshot;std::vector<double> times,frameTimes;auto frameStart=std::chrono::steady_clock::now();std::vector<glm::dvec3> retained;std::string retainedMaterial;
 Window* window=nullptr;auto key=[&](SDL_Scancode k,bool held){window->QueueTestPhysical(std::string("key:")+SDL_GetScancodeName(k),held?1.f:0.f);};
 ApplicationControl c;c.hidden=true;c.frameSeconds=[&](float){return frozen?0.f:1.f/60;};
 c.hostReady=[&](EngineHost& h){window=&h.GetWindow();window->SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string e;h.Audio().Init(e,true);};
 c.worldReady=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){original=&w;};
 c.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){frameStart=std::chrono::steady_clock::now();h.GetRenderer().ResetStats();
  if(mode=="read"&&phase==1){key(SDL_SCANCODE_F8,true);phase=2;}else if(mode=="read"&&phase==2)key(SDL_SCANCODE_F8,false);
  // Publication precedes the restored script lifecycle boundary. Freeze the
  // physical clock, then capture after that zero-dt frame: full-world Capture
  // deliberately rejects uninitialized script instances even when the native
  // material participant has restored correctly.
  if(mode=="read"&&&w!=original&&phase==2){frozen=true;phase=3;}

 };
 c.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){++frame;
  auto* a=w.RuntimeDeformable(100,error);auto* b=w.RuntimeDeformable(200,error);auto* f=w.RuntimeDeformable(300,error);auto* panel=w.RuntimeDeformable(400,error);
  bool healthy=w.Scripts()&&w.Scripts()->Diagnostics().empty();if(!healthy&&w.Scripts())for(auto& d:w.Scripts()->Diagnostics())std::printf("SCRIPT %s\n",d.message.c_str());
  if(!ready&&a&&b&&f&&panel){ready=frame;epoch=a->generation;check(!a->asset->fracture&&f->asset->fracture&&!f->asset->fracture->rigid&&f->asset->fracture->structural,"normal async solid and flexible structural assets publish");check(!w.RuntimeBody(100).IsValid()&&!w.RuntimeBody(300).IsValid(),"deforming material has one nodal mass owner, no undeformed duplicate rigid body");if(mode=="read"){auto* service=w.SceneControl()->Saves(h.Resources());SaveStorage store(service->Root());bool recovered=false;check(store.Read("bending-current",snapshot,recovered,error),"fresh process reads accepted deformable save bytes");phase=1;}}
  if(ready&&mode!="read"){
   auto t=frame-ready;if(t>20){times.push_back(play.LastFixedStepMilliseconds());frameTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStart).count());}
   if(t==30){for(auto id:{100,200}){auto* sim=w.RuntimeDeformable(id,error);auto pose=w.RuntimeDefinition(id)->transform;double rest=0;for(unsigned t=0;t<sim->plasticRest.size();++t)for(int k=0;k<3;++k)rest=std::max(rest,glm::length(sim->plasticRest[t][k]-(glm::mat3_cast(glm::dquat(pose.rotation))*sim->asset->solids[t].rest)[k]));std::printf("CALM id=%d plastic_rest_m=%.9g\n",id,rest);}check(healthy,"public JS camera, motor and UI execute");if(mode=="probe"||mode=="write"||mode=="radial")key(SDL_SCANCODE_B,true);if(mode=="stream"){SceneTransform placement;placement.position={0,6,-5};spawned=w.SpawnPrefab("80808080808080808080808000000008",placement,error);check(spawned!=0,"ordinary deformable prefab spawns");region=w.SceneControl()->Streaming(w,error)->Request("annex",false,error);}}
   if(t==31)key(SDL_SCANCODE_B,false);
   if(t==120&&(mode=="probe"||mode=="write"||mode=="radial")){double deformation=0;const auto pose=w.RuntimeDefinition(100)->transform;for(unsigned n=0;n<a->positions.size();++n)deformation=std::max(deformation,glm::length(a->positions[n]-(glm::dvec3(pose.position)+glm::dquat(pose.rotation)*a->asset->nodes[n])));std::printf("LOADED deformation_m=%.9g minJ=%.9g\n",deformation,a->stats.minimumJacobian);check(deformation>.01&&a->error.empty(),"script force changes authoritative beam geometry");key(SDL_SCANCODE_B,true);}
   if(t==121)key(SDL_SCANCODE_B,false);
   if(t==160&&mode=="write"){key(SDL_SCANCODE_H,true);}if(t==161)key(SDL_SCANCODE_H,false);
   if(t==200&&mode=="write"){key(SDL_SCANCODE_H,true);}if(t==201)key(SDL_SCANCODE_H,false);
   if(t==230&&mode=="write"){key(SDL_SCANCODE_H,true);}
   if(t==231){key(SDL_SCANCODE_H,false);}
   if(t==260){for(auto id:{100,200,300,400}){auto* sim=w.RuntimeDeformable(id,error);auto pose=w.RuntimeDefinition(id)->transform;double shape=0,plastic=0;for(unsigned n=0;n<sim->positions.size();++n)shape=std::max(shape,glm::length(sim->positions[n]-(glm::dvec3(pose.position)+glm::dquat(pose.rotation)*sim->asset->nodes[n])));for(unsigned tet=0;tet<sim->plasticRest.size();++tet)for(int k=0;k<3;++k)plastic=std::max(plastic,glm::length(sim->plasticRest[tet][k]-(glm::mat3_cast(glm::dquat(pose.rotation))*sim->asset->solids[tet].rest)[k]));std::printf("SPECIMEN id=%d shape_m=%.9g plastic_rest_m=%.9g contacts=%zu error=%s\n",id,shape,plastic,sim->stats.contacts,sim->error.c_str());if(id==400)check(shape>.01&&plastic>.001&&sim->error.empty(),"ordinary physical press dents the actual panel and leaves permanent material rest");}}
   if(t==280&&mode=="write"){auto moved=w.RuntimeDefinition(200)->transform;moved.rotation=glm::angleAxis(.4f,glm::normalize(glm::vec3(1,2,3)));check(w.SetRuntimeTransform(200,moved)&&b->Release("left"),"logical owner may rotate while existing material retains its historical frame");check(std::any_of(f->fracture.broken.begin(),f->fracture.broken.end(),[](auto x){return x!=0;}),"increasing actual JS load fails flexible structural material before cold save");key(SDL_SCANCODE_F5,true);phase=6;}if(t==281)key(SDL_SCANCODE_F5,false);
   if(mode=="write"&&phase==6){auto* service=w.SceneControl()->Saves(h.Resources());for(uint64_t i=1;i<8;++i)if(auto* s=service->Status(i)){if(s->state=="completed"){check(true,"public JS commits deformed and yielded state");w.UI().RequestQuit();phase=9;}if(s->state=="failed"){std::printf("SAVE %s\n",s->error.c_str());check(false,"deformable save transaction succeeds");w.UI().RequestQuit();phase=9;}}}
   if(mode=="stream"&&t>40){auto* stream=w.SceneControl()->Streaming(w,error);auto status=stream->Regions().front();if(phase==0&&status.state=="active"){oldAnnex=stream->Resolve("annex",1);auto* sim=w.RuntimeDeformable(oldAnnex,error);if(sim){sim->Impulse("right",{0,0,1});sim->Step(1./60,w.Physics(),UniformGravity({0,0,0}),{});auto moved=w.RuntimeDefinition(oldAnnex)->transform;moved.rotation=glm::angleAxis(.4f,glm::normalize(glm::vec3(1,2,3)));check(w.SetRuntimeTransform(oldAnnex,moved)&&sim->Release("left"),"streamed owner changes pose without resetting existing material");retained=sim->positions;SaveArchive retain;sim->Persist(retain);retainedMaterial=std::move(retain.bytes);stream->Release(region);frozen=true;phase=1;}}if(phase==1&&status.state=="unloaded"){check(!w.RuntimeDefinition(oldAnnex)&&status.retained>0,"ordinary plain-solid region suspends geometry and invalidates identity");region=stream->Request("annex",false,error);phase=2;}if(phase==2&&status.state=="active"){auto current=stream->Resolve("annex",1);auto* sim=w.RuntimeDeformable(current,error);SaveArchive restoredMaterial;if(sim)sim->Persist(restoredMaterial);check(sim&&current!=oldAnnex&&sim->positions==retained&&restoredMaterial.bytes==retainedMaterial,"stream revisit restores deformed geometry and original material basis with fresh identity");auto* p=w.RuntimeDeformable(spawned,error);check(p&&p->asset==a->asset&&p->generation!=a->generation,"prefab shares immutable topology with independent nodal state");w.DestroyEntity(spawned);check(!w.RuntimeDefinition(spawned),"destroy removes prefab simulation owner");frozen=false;w.SceneControl()->Reload(error);phase=4;}}
   if(mode=="stream"&&phase==4&&a->generation!=epoch){check(a->error.empty()&&healthy,"reload cleans previous instances and script handles");w.UI().RequestQuit();phase=9;}
   if(t==320&&(mode=="probe"||mode=="radial")){check(a->error.empty()&&b->error.empty()&&f->error.empty()&&panel->error.empty()&&healthy,"ordinary gameplay, physical solids and contacts remain healthy");if(mode=="radial")check(w.Gravity().Sample({6,2,0}).x<-.1,"real RadicalGravity field has spatial acceleration");std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);WriteRgbPng((std::filesystem::path(out)/"lab.png").string(),window->Width(),window->Height(),pixels);std::sort(times.begin(),times.end());std::printf("PERF bodies=%zu nodes=%zu fixed_median_ms=%.6f p95_ms=%.6f max_ms=%.6f frames=%zu\n",w.Physics().AliveBodyCount(),a->positions.size()+b->positions.size()+f->positions.size()+panel->positions.size()+w.RuntimeDeformable(450,error)->positions.size(),times[times.size()/2],times[size_t(.95*(times.size()-1))],times.back(),times.size());std::sort(frameTimes.begin(),frameTimes.end());std::printf("FRAME_WORK median_ms=%.6f p95_ms=%.6f max_ms=%.6f\n",frameTimes[frameTimes.size()/2],frameTimes[size_t(.95*(frameTimes.size()-1))],frameTimes.back());w.UI().RequestQuit();phase=9;}
  }
  if(mode=="read"&&phase==3){
   std::map<std::string,SaveChunk> chunks;
   if(WorldPersistence::Capture(w,chunks,error)){
    check(play.FixedStepsSinceReset()==0,"restore observation precedes any resumed physical step");
    check(chunks.count("deformables")&&chunks.at("deformables").data==snapshot.participants.at("deformables").data,"cold restore matches exact deformed geometry, plastic rest and fracture topology");
    for(auto name:{"saved","restored"}){std::ofstream file(std::filesystem::path(out)/(std::string(name)+"-deformables.bin"),std::ios::binary);const auto& data=std::string(name)=="saved"?snapshot.participants.at("deformables").data:chunks.at("deformables").data;file.write(data.data(),data.size());}
    frozen=false;retained=b->positions;check(b->error.empty()&&healthy,"restored yielded material and scripts healthy");b->Impulse("right",{0,0,1});phase=4;ready=frame;
   }else if(error!="loading"&&error.find("required script instance not initialized")==std::string::npos){std::printf("RESTORE_CAPTURE %s\n",error.c_str());check(false,"restored world accepts capture after lifecycle initialization");w.UI().RequestQuit();phase=9;}
  }
  if(mode=="read"&&phase==4&&frame>ready+12){check(b->positions!=retained&&b->error.empty()&&healthy,"cold-restored material physically resumes after new impulse");w.UI().RequestQuit();phase=9;}
  std::this_thread::sleep_for(std::chrono::milliseconds(1));if(frame>1000){check(false,"bounded application deadline");w.UI().RequestQuit();}
 };
 Application app;char name[]="judas";char* args[]={name,project.data()};check(app.Run(2,args,&c)==0&&phase==9,"normal application startup and shutdown");std::printf("M80 %s checks=%u failures=%u\n",mode.c_str(),checks,failures);return failures?1:0;
}
