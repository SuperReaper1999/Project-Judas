#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "SaveService.h"
#include "WorldPersistence.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <iostream>
using J=nlohmann::json;
namespace {unsigned checks=0,failures=0;void check(bool b,const std::string& s){++checks;failures+=!b;std::cout<<(b?"PASS ":"FAIL ")<<s<<std::endl;}void quit(){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}std::string read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}}
int main(int argc,char** argv){if(argc!=4)return 2;std::string project=argv[1],mode=argv[2];std::filesystem::path out=argv[3];std::filesystem::create_directories(out);bool writing=mode=="write",sameContent=mode=="same-content"||mode=="current-v3",negative=mode=="exact"||mode=="failure"||mode=="incompatible"||mode=="mods"||mode=="native-failure";unsigned frame=0,phase=0,at=0;uint64_t request=0,token=0,built=0;RuntimeWorld* original=nullptr;std::string expected,sourceBytes,slot="old-progress";ApplicationControl control;control.hidden=true;control.hostReady=[](EngineHost& host){host.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string e;host.Audio().Init(e,true);};control.frameSeconds=[](float){return 1.f/60;};
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& w,InteractivePlay&){++frame;std::string error;auto session=w.SceneControl();auto* service=session->Saves(host.Resources());auto* stream=session->Streaming(w,error);auto owner=stream?stream->Resolve("quarry0",1):0;auto* terrain=owner?w.LiveTerrain().Status(owner):nullptr;
  if(w.Scripts()&&!w.Scripts()->Diagnostics().empty()){for(auto& d:w.Scripts()->Diagnostics())std::cout<<"SCRIPT "<<d.callback<<' '<<d.message<<std::endl;check(false,"project scripts healthy");quit();return;}
  if(phase==0&&terrain&&terrain->ready&&!terrain->busy&&!service->Content().empty()){
   original=&w;
   if(writing){
    auto records=w.Scripts()->Capture();for(auto& r:records)if(r.entity==10){auto json=J::parse(r.json);json["salvage"]=7;json["unknownQuest"]={{"opened",true}};r.json=json.dump();}check(w.RestoreScriptState(records,error),"real v1 progress installed in ordinary script state");check(session->Set("mission","{\"step\":3,\"unfamiliar\":true}",error),"project-owned session quest progress");
    auto transform=w.RuntimeDefinition(owner)->transform;check(w.LiveTerrain().Brush(owner,transform.position+transform.rotation*glm::vec3(0,-3.5f,7),VolumeBrush{VolumeBrushKind::Remove,1.8f,1.f},error),"v1 edits actual terrain");phase=1;
   }else{
    SaveStorage storage(service->Root());GameSnapshot saved;bool recovered=false;if(mode=="read-v3")slot="v2-progress";if(mode=="current-v3")slot="upgraded-progress";if(!storage.Read(slot,saved,recovered,error)){check(false,"read existing cold fixture: "+error);quit();return;}
    sourceBytes=read(std::filesystem::path(service->Root())/(slot+".save"));
    if(mode=="mods"||mode=="native-failure"){
     if(mode=="mods")saved.modContext=J({{"format",1},{"signature","missing-pack"},{"contractSha256",std::string(64,'1')},{"active",J::array({{{"id","missing.cosmetics"},{"version","1.0.0"},{"sha256",std::string(64,'2')}}})}}).dump();else saved.participants["motors"].version=999;
     slot=mode;check(storage.Write(slot,saved,error),"isolated adversarial save fixture");sourceBytes=read(std::filesystem::path(service->Root())/(slot+".save"));
    }
    request=service->Request("load",slot,"","{}",error,mode!="exact");check(request!=0,"queue public save-service load: "+error);phase=5;
   }
  }else if(phase==1&&terrain&&!terrain->busy&&terrain->revision>=2){
   if(!built){SceneTransform placement;placement.position={2,2,7};built=w.SpawnPrefab("447760bfcc12bf1c58786519546688af",placement,error);if(!built&&error=="loading")return;check(built&&stream->Adopt(built,"quarry0",error),"v1 real prefab construction adopted by region");check(session->Set("built",J(stream->PersistentKey(built)).dump(),error),"persist qualified construction reference");}
   check(w.CaptureRuntimeTerrain(owner,expected,error),"v1 exact excavated field");std::ofstream(out/"expected-terrain.bin",std::ios::binary)<<expected;
   auto records=w.Scripts()->Capture();for(auto& r:records)if(r.entity==10){auto j=J::parse(r.json);j["quarry"]=1;r.json=j.dump();}check(w.RestoreScriptState(records,error),"script selects destination region");
   auto t=w.RuntimeDefinition(10)->transform;t.position={50,-3.6f,11};check(w.SetRuntimeTransform(10,t),"travel away normally before cold save");token=stream->Request("quarry1",false,error);check(token!=0,"request destination region");phase=2;
  }else if(phase==2){bool unloaded=false;for(auto& r:stream->Regions())if(r.id=="quarry0")unloaded=r.state=="unloaded";if(unloaded&&stream->SaveReady()){check(!w.RuntimeDefinition(owner),"v1 excavated region genuinely suspended before save");request=service->Request("save","old-progress","v1 original","{}",error);check(request!=0,"write older content through existing service");phase=3;}}
  else if(phase==3){auto* status=service->Status(request);if(status&&(status->state=="completed"||status->state=="failed")){check(status->state=="completed","older-game durable save: "+status->error);SaveStorage storage(service->Root());GameSnapshot saved;bool recovered=false;check(storage.Read("old-progress",saved,recovered,error)&&saved.contentVersion==0,"genuine legacy-format source identity");check(saved.participants.at("streaming").version==9,"v1 cold save includes retained physical terrain");phase=20;}}
  else if(phase==5){auto* status=service->Status(request);if(status&&(status->state=="completed"||status->state=="failed")){
   std::cout<<"LOAD_RESULT "<<status->state<<" migration_ms="<<status->migrationMs<<" worker_ms="<<status->workerMs<<" restore_ms="<<status->restoreMs<<" error="<<status->error<<" report="<<status->migrationReport<<std::endl;
   check(sourceBytes==read(std::filesystem::path(service->Root())/(slot+".save")),"load/migration preserves original source bytes");
   if(negative){check(status->state=="failed"&&&w==original,"failed candidate leaves original world published");if(mode=="mods")check(status->error.find("missing.cosmetics")!=std::string::npos,"missing mod diagnosis names exact saved requirement");auto transform=w.RuntimeDefinition(10)->transform;transform.position.z+=1;check(w.SetRuntimeTransform(10,transform),"live world remains usable after failure");phase=20;}
   else{check(status->state=="completed"&&&w!=original&&status->migrated!=sameContent,"cold version upgrade publishes only validated new world");if(status->state=="failed"){phase=20;return;}
    bool found=false;for(auto& r:w.Scripts()->Capture())if(r.entity==10){auto j=J::parse(r.json);found=(mode=="same-content"?j.at("salvage")==7:j.at("inventory").at("ore")==7&&!j.contains("salvage"))&&j.at("unknownQuest").at("opened")==true;if(mode=="read-v3"||mode=="direct-v3"||mode=="current-v3")found&=j.at("chapter")=="arrival";}check(found,"actual restored script inventory and unknown quest retained");check(J::parse(session->Get("mission")).at("step")==3,"actual restored session quest retained");
    if(!sameContent){auto rejected=service->Request("save",slot,"","{}",error);check(!rejected&&error.find("protected")!=std::string::npos,"upgraded world cannot overwrite source slot");}
    auto records=w.Scripts()->Capture();for(auto& r:records)if(r.entity==10){auto j=J::parse(r.json);j["quarry"]=0;r.json=j.dump();}check(w.RestoreScriptState(records,error),"restored script selects return region");
    auto transform=w.RuntimeDefinition(10)->transform;transform.position={0,-3.6f,11};check(w.SetRuntimeTransform(10,transform),"return to edited region after upgrade");token=stream->Request("quarry0",false,error);phase=6;at=frame;
   }
  }}else if(phase==6&&terrain&&terrain->ready&&!terrain->busy&&frame>at+30){
   std::string actual;check(w.CaptureRuntimeTerrain(owner,actual,error)&&actual==read(out/"expected-terrain.bin"),"actual suspended terrain field survives content upgrade");auto t=w.RuntimeDefinition(owner)->transform;auto hit=w.Physics().Raycast(t.position+t.rotation*glm::vec3(0,-3.4f,8),t.rotation*glm::vec3(0,0,-1),16);check(hit.hit&&hit.body.id==w.RuntimeBody(owner).id,"restored edited geometry has actual collision");
   const auto builtKey=J::parse(session->Get("built")).get<std::string>();const auto restoredBlock=stream->ResolvePersistentKey(builtKey);auto body=w.RuntimeBody(restoredBlock);check(body.IsValid()&&w.Physics().IsDynamicBody(body),"actual constructed prefab returns as an independent physical body");
   if(mode=="read-v3"||mode=="direct-v3"||mode=="current-v3")check(w.RuntimeDefinition(10)->name=="Upgraded survivor"&&w.RuntimeDefinition(70)->render->color==glm::vec3(.25f,.4f,.32f),"saved entity/component evolution is visible in the actual restored world");
   request=service->Request("save",sameContent?"same-content-copy":mode=="read-v2"?"v2-progress":"upgraded-progress","upgraded progress","{}",error);check(request!=0,"commit upgraded progress to new slot");phase=7;
  }else if(phase==7){auto* s=service->Status(request);if(s&&(s->state=="completed"||s->state=="failed")){check(s->state=="completed","upgraded new-format save durable: "+s->error);phase=20;}}
  if(phase==20){std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(host.GetWindow().Width(),host.GetWindow().Height(),pixels);WriteRgbPng((out/(mode+".png")).string(),host.GetWindow().Width(),host.GetWindow().Height(),pixels);check(w.Scripts()&&w.Scripts()->Diagnostics().empty(),"project lifecycle remains healthy");quit();phase=21;}
  if(frame>12000&&phase!=21){check(false,"bounded evolution workflow timeout phase "+std::to_string(phase));quit();phase=21;}
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 };
 Application app;char name[]="judas";char* args[]={name,const_cast<char*>(project.c_str())};check(app.Run(2,args,&control)==0,"application shutdown");std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
}
