// Normal application/GL lifecycle. This target never links terrain authoring.
#include "Application.h"
#include "InteractivePlay.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "SaveService.h"
#include "TestInput.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <iostream>
#include <algorithm>
namespace fs=std::filesystem;using Json=nlohmann::json;
int checks=0,failures=0;
void Check(bool ok,const char* label){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';std::cout.flush();}
void Quit(){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}
Json Distribution(std::vector<double> values){if(values.empty())return Json();std::sort(values.begin(),values.end());double sum=0;for(auto v:values)sum+=v;return Json{{"samples",values.size()},{"mean_ms",sum/values.size()},{"p95_ms",values[size_t((values.size()-1)*.95)]},{"max_ms",values.back()}};}
int main(int argc,char** argv){if(argc!=4)return 2;std::string project=argv[1],mode=argv[2],error;fs::path out=argv[3];fs::create_directories(out);bool done=false,ready=false,freeze=false;unsigned frame=0,at=0,phase=0;size_t authoredBodies=0,terrainTriangles=0;uint64_t request=0,demand=0,uploads=0;BodyHandle retired;EntityId previous=0;RuntimeWorld* before=nullptr;Json expected;std::string restoreBytes,restorePath;std::vector<double> frameMs,fixedMs;double measuredFrameMs=0,performanceElapsed=0;uint64_t draws=0,triangles=0;bool measuring=false;
 ApplicationControl control;control.hidden=true;control.hostReady=[&](EngineHost& h){SDL_GL_SetSwapInterval(0);h.GetWindow().SetTestInputMode(true);std::string e;h.Audio().Init(e,true);PerformanceProfiler::Get().Enable(true);};control.frameSeconds=[&](float measured){measuredFrameMs=double(measured)*1000;return freeze?0.f:(mode=="performance"||mode=="diagnostic"||mode=="zero")?measured:1.f/60;};
 auto position=[](RuntimeWorld& w){auto p=w.RuntimeCharacter(10)->position;return Json::array({p.x,p.y,p.z});};
 control.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){if(mode=="probe"&&ready&&phase==0){auto n=frame-at;if(n==2||n==35)QueueTestKey(h.GetWindow(),SDL_SCANCODE_W,n==2);if(n==40||n==41)QueueTestKey(h.GetWindow(),SDL_SCANCODE_SPACE,n==40);if(n==45||n==46)QueueTestKey(h.GetWindow(),SDL_SCANCODE_E,n==45);}if(mode=="read"&&phase==1&&before!=&w){freeze=true;auto now=position(w);float distance=0;for(int i=0;i<3;++i)distance+=std::abs(now[i].get<float>()-expected[i].get<float>());Check(distance<1e-4,"fresh-process modern load restores exact motor position");Check(w.RuntimeBody(1).IsValid(),"fresh-process load resolves required terrain collision");phase=2;}};
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){++frame;if(done)return;auto* motor=w.RuntimeCharacter(10);if(w.Scripts()&&!w.Scripts()->Diagnostics().empty()){for(auto& d:w.Scripts()->Diagnostics())std::cerr<<d.callback<<": "<<d.message<<'\n';Check(false,"public terrain game JS healthy");done=true;Quit();return;}
 if(!ready&&motor&&motor->result.supported&&w.BaselineFingerprint().size()==64&&h.Resources().Stats().loading==0){ready=true;at=frame;authoredBodies=w.Physics().AliveBodyCount();Check(w.RuntimeBody(1).IsValid()&&w.Physics().AliveBodyCount()>=5,"required terrain and ordinary bodies ready through normal resource path");auto* definition=w.RuntimeDefinition(1);if(definition&&definition->render){auto* parts=h.Resources().TryGetModelParts(definition->render->meshAsset);if(parts)for(auto& part:*parts)terrainTriangles+=part.count/3;}Check(mode=="zero"||h.GetRenderer().Stats().triangles>terrainTriangles,"normal GL draws terrain and surrounding physical objects");std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);Check(WriteRgbPng((out/"runtime.png").string(),h.GetWindow().Width(),h.GetWindow().Height(),pixels),"actual ordinary renderer capture");expected=position(w);}
 if(!ready){std::this_thread::sleep_for(std::chrono::milliseconds(1));if(frame>1500){Check(false,"bounded readiness");Quit();}return;}
 if(mode=="performance"||mode=="diagnostic"||mode=="zero"){
  if((frame-at)%30==0)std::cerr<<"PERF frame "<<frame-at<<" wall "<<measuredFrameMs<<" fixed "<<play.LastFixedStepMilliseconds()<<" ms\n";
  performanceElapsed+=measuredFrameMs;
  const bool diagnostic=mode=="diagnostic";
  if(!measuring&&(diagnostic||performanceElapsed>=2000)){measuring=true;uploads=h.Resources().Stats().uploads;draws=h.GetRenderer().Stats().drawCalls;triangles=h.GetRenderer().Stats().triangles;frameMs.clear();fixedMs.clear();}
  if(measuring){if(frameMs.size()<32768)frameMs.push_back(measuredFrameMs);if(play.LastFixedStepsThisFrame())fixedMs.push_back(play.LastFixedStepMilliseconds());}
  if((diagnostic&&frame-at==30)||(!diagnostic&&performanceElapsed>=5000)){Check(h.Resources().Stats().uploads==uploads,"static terrain has zero steady resource uploads");Json report={{"outer_frame",Distribution(frameMs)},{"terrain_triangles",terrainTriangles},{"bodies",w.Physics().AliveBodyCount()},{"draw_calls_per_frame",double(h.GetRenderer().Stats().drawCalls-draws)/std::max(size_t(1),frameMs.size())},{"submitted_triangles_per_frame",double(h.GetRenderer().Stats().triangles-triangles)/std::max(size_t(1),frameMs.size())},{"fixed_step",Distribution(fixedMs)},{"elapsed_ms",performanceElapsed},{"resource_bytes",h.Resources().Stats().bytesResident},{"note","Measured hidden uncapped Release wall-clock application; no test sleep during steady workload. Inclusive M56 scopes overlap."}};std::ofstream(out/"performance.json")<<report.dump(2)<<'\n';Check(PerformanceProfiler::Get().Export((out/"profile.json").string(),error),"actual M56 performance capture");done=true;Quit();}
 }else if(mode=="probe"){
  auto* stream=w.SceneControl()->Streaming(w,error);
  if(phase==0&&frame-at>=80){Check(glm::length(motor->position-glm::vec3(expected[0],expected[1],expected[2]))>1,"public logical input moves motor on edited terrain");Check(w.Physics().AliveBodyCount()>authoredBodies,"game JS spawns real dropped prefab body");SceneTransform placement;placement.position={-96,0,0};auto spawned=w.SpawnPrefab("29d6bfaf62bde2b067ed096cd5f30a36",placement,error);auto prefabHit=w.Physics().Raycast({-96,20,0},{0,-1,0},30);Check(spawned&&prefabHit.hit&&prefabHit.body.id==w.RuntimeBody(spawned).id,"ordinary terrain prefab publishes shared coherent collider");Check(spawned&&w.DestroyEntity(spawned,&error)&&!w.Physics().Raycast({-96,20,0},{0,-1,0},30).hit,"terrain prefab destruction removes independent physical instance");demand=stream?stream->Request("annex",false,error):0;Check(demand!=0,"ordinary terrain annex demand");phase=1;}
  if(phase==1&&stream&&(previous=stream->Resolve("annex",1))){retired=w.RuntimeBody(previous);auto hit=w.Physics().Raycast({96,20,0},{0,-1,0},30);Check(retired.IsValid()&&hit.hit&&hit.body.id==retired.id,"region active boundary has real terrain collider");Check(stream->Release(demand)&&stream->Unload("annex"),"ordinary region release/unload");phase=2;}
  if(phase==2&&stream&&!stream->Resolve("annex",1)){Check(!w.Physics().IsBodyEnabled(retired)&&!w.Physics().Raycast({96,20,0},{0,-1,0},30).hit,"unload removes collider and rejects stale handle");demand=stream->Request("annex",false,error);phase=3;}
  if(phase==3&&stream&&stream->Resolve("annex",1)){auto next=w.RuntimeBody(stream->Resolve("annex",1));Check(next.IsValid()&&(next.id!=retired.id||next.world!=retired.world),"revisit publishes generation-safe replacement body");stream->Release(demand);stream->Unload("annex");Check(w.SceneControl()->Reload(error),"ordinary scene reload queued");before=&w;phase=4;}
  if(phase==4&&before!=&w){Check(w.RuntimeCharacter(10)&&w.RuntimeBody(1).IsValid()&&w.Physics().AliveBodyCount()==authoredBodies,"reload reconstructs clean terrain/body/motor state");done=true;Quit();}
 }else{
  auto* saves=w.SceneControl()->Saves(h.Resources());
  if(phase==0&&frame-at>15){freeze=true;before=&w;
   if(mode=="write"){expected=position(w);std::ofstream(out/"expected.json")<<expected.dump();request=saves->Request("save","m75-terrain","Terrain proof","{}",error);}
   else {std::ifstream(out/"expected.json")>>expected;if(mode=="changed"||mode=="missing"){auto* d=w.RuntimeDefinition(1);auto id=mode=="changed"?d->render->meshAsset:d->body->collisionAsset;auto* a=h.Assets().Find(id);restorePath=a->path;std::ifstream f(restorePath,std::ios::binary);restoreBytes={std::istreambuf_iterator<char>(f),{}};f.close();if(mode=="missing")fs::rename(restorePath,restorePath+".m75-hidden");else {std::ofstream f2(restorePath,std::ios::binary|std::ios::app);f2.put('x');}}request=saves->Request("load","m75-terrain","","{}",error);}
   Check(request!=0,"normal modern save/load request");phase=1;
  }
  if(request){auto* status=saves->Status(request);if(status&&status->state=="failed"){if(mode=="changed"||mode=="missing"){Check(before==&w&&w.RuntimeBody(1).IsValid(),"changed/missing required terrain fails before replacing live world");std::cout<<"EXPECTED REJECTION "<<status->error<<'\n';done=true;Quit();}else{Check(false,status->error.c_str());done=true;Quit();}}else if(status&&status->state=="completed"&&(mode=="write"||phase==2)){Check(true,"modern slot transaction completes");done=true;Quit();}}
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 if(frame>(mode=="performance"||mode=="zero"?32768u:2000u)){Check(false,"bounded application completion");done=true;Quit();}
 };
 Application app;char name[]="judas";char* args[]={name,project.data()};auto result=app.Run(2,args,&control);if(!restorePath.empty()){if(mode=="missing")fs::rename(restorePath+".m75-hidden",restorePath);else{std::ofstream f(restorePath,std::ios::binary|std::ios::trunc);f<<restoreBytes;}}
 Check(result==0&&done,"normal application startup and shutdown");std::cout<<checks<<" checks / "<<failures<<" failures\n";return failures?1:0;}
