// Real Application/ResourceManager/QuickJS/physics/save/region boundaries.
#include "Application.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "InteractivePlay.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "SaveService.h"
#include "WorldPersistence.h"
#include "TestInput.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include "modding/ModRuntime.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <iostream>
#include <algorithm>
#include <chrono>
namespace fs=std::filesystem;using JSON=JudasMods::JSON;
namespace {
unsigned checks=0,failures=0;
void Check(bool ok,const char* what){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<what<<'\n';std::cout.flush();}
std::string State(RuntimeWorld& world){for(const auto& record:world.Scripts()->Capture())if(record.entity==10&&record.slot==1)return record.json;return "{}";}
void Quit(){SDL_Event event{};event.type=SDL_QUIT;SDL_PushEvent(&event);}
JSON Distribution(std::vector<double> values){if(values.empty())return nullptr;std::sort(values.begin(),values.end());double sum=0;for(auto v:values)sum+=v;return {{"samples",values.size()},{"mean_ms",sum/values.size()},{"p95_ms",values[size_t((values.size()-1)*.95)]},{"max_ms",values.back()}};}
}
int main(int argc,char** argv){if(argc!=4)return 2;std::string file=argv[1],mode=argv[2],error;fs::path output=fs::u8path(argv[3]);fs::create_directories(output);unsigned frame=0,readyFrame=0,phase=0;bool ready=false,done=false,freeze=false,droppedContact=false;uint64_t request=0,demand=0;RuntimeWorld* original=nullptr;BodyHandle retired;EntityId previous=0;std::vector<double> frames,fixed;double measured=0,total=0;size_t bodies=0;JSON expected;
 auto begin=std::chrono::steady_clock::now();double readiness=0;ApplicationControl control;control.hidden=true;control.hostReady=[&](EngineHost& host){SDL_GL_SetSwapInterval(0);host.GetWindow().SetTestInputMode(true);std::string e;host.Audio().Init(e,true);PerformanceProfiler::Get().Enable(true);};
 control.frameSeconds=[&](float elapsed){measured=double(elapsed)*1000;return freeze?0.f:(mode=="performance"||mode=="zero"||mode=="empty")?elapsed:1.f/60;};
 control.beforeFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay&){if((mode=="probe"||mode=="write")&&ready&&phase==0){auto n=frame-readyFrame;if(n==(mode=="probe"?100u:2u)||n==(mode=="probe"?102u:10u))QueueTestKey(host.GetWindow(),SDL_SCANCODE_E,n==(mode=="probe"?100u:2u));if(mode=="probe"&&(n==15||n==90))QueueTestKey(host.GetWindow(),SDL_SCANCODE_W,n==15);}if(mode=="read"&&phase==1&&original!=&world){freeze=true;phase=2;}};
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){++frame;host.Audio().AdvanceWithoutDevice(128);if(done)return;
  if(world.Scripts()&&!world.Scripts()->Diagnostics().empty()){for(const auto& diagnostic:world.Scripts()->Diagnostics())std::cerr<<diagnostic.callback<<": "<<diagnostic.message<<'\n';Check(false,"mod public JS healthy");done=true;Quit();return;}
  auto* motor=world.RuntimeCharacter(10);
  if(!ready&&motor&&motor->result.supported&&host.Resources().Stats().loading==0&&world.BaselineFingerprint().size()==64){ready=true;readiness=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();readyFrame=frame;bodies=world.Physics().AliveBodyCount();original=&world;
   Check(world.RuntimeBody(1).IsValid(),"required composed terrain/body ready through normal resource path");Check(world.UI().Find("modding_lab")!=0,"ordinary authored UI published");
   std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(host.GetWindow().Width(),host.GetWindow().Height(),pixels);Check(WriteRgbPng((output/"runtime.png").u8string(),host.GetWindow().Width(),host.GetWindow().Height(),pixels),"normal renderer capture");
   if(mode!="zero"&&mode!="empty")Check(!host.Assets().ModCatalog().empty(),"frozen catalog reaches normal script/resource database");
   if(mode=="probe"||mode=="write"||mode=="read")Check(motor->position.y>.8f,"motor grounded above actual terrain");
  }
  if(!ready){std::this_thread::sleep_for(std::chrono::milliseconds(1));if(frame>1600){Check(false,"bounded application readiness");done=true;Quit();}return;}
  auto count=frame-readyFrame;
  if(mode=="probe"&&phase==0)for(const auto& script:world.Scripts()->Capture())if(script.entity!=10){auto body=world.RuntimeBody(script.entity),ground=world.RuntimeBody(1);for(const auto& contact:world.Physics().LastStepTouchEvents())if(!contact.sensor&&contact.phase!=PhysicsWorld::TouchPhase::Exit&&((contact.a.id==body.id&&contact.b.id==ground.id)||(contact.b.id==body.id&&contact.a.id==ground.id)))droppedContact=true;}
  if(mode=="performance"||mode=="zero"||mode=="empty"){
   total+=measured;if(total>1500&&frames.size()<32768){frames.push_back(measured);if(play.LastFixedStepsThisFrame())fixed.push_back(play.LastFixedStepMilliseconds());}
   if(total>4500){JSON report={{"startup_to_grounded_resource_ready_ms",readiness},{"outer_frame",Distribution(frames)},{"fixed_step",Distribution(fixed)},{"bodies",bodies},{"resident_bytes",host.Resources().Stats().bytesResident},{"mode",mode},{"note","Actual uncapped hidden Release application; inclusive M56 timings overlap, no test sleep after readiness."}};std::ofstream(output/"performance.json")<<report.dump(2);Check(PerformanceProfiler::Get().Export((output/"profile.json").u8string(),error),"M56 profiler export");done=true;Quit();}
  }else if(mode=="probe"){
   if(phase==0&&count==165){auto point=motor->position;std::cout<<"MOTOR "<<point.x<<","<<point.y<<","<<point.z<<" support="<<motor->result.supported<<"\n";PhysicsQueryFilter filter;filter.ignoredBodies.push_back(motor->observationBody);auto hit=world.Physics().Raycast(point+glm::vec3(0,4,0),{0,-1,0},10,filter);Check(motor->result.supported&&point.y>1.8f&&hit.hit&&hit.point.y>1,"script-driven motor walked onto actual sculpted collision height");}
   if(phase==0&&count>170){Check(world.Physics().AliveBodyCount()>bodies,"game spawns contributed prefab through public JS");bool declared=false,dropped=false;for(const auto& script:world.Scripts()->Capture()){if(script.entity!=10&&script.json.find("\"value\":7")!=std::string::npos)declared=true;auto body=world.RuntimeBody(script.entity);if(script.entity!=10&&body.IsValid())dropped=true;}Check(dropped&&droppedContact,"ordinary spawned body makes authoritative contact with sculpted collision geometry");if(host.Assets().ModCatalog().find("common")!=std::string::npos)Check(declared,"published dependency module executed with independent same-filename ownership");auto state=State(world);Check(JSON::parse(state).value("received",0)>0,"ordinary signal changes saved mod fact");
    auto hit=world.Physics().Raycast({0,15,0},{0,-1,0},30);Check(hit.hit&&hit.point.y>1,"query sees edited terrain, no hidden flat collider");
    auto path=world.Navigation().FindPath({-10,0,0},{10,0,0});Check(path.status==NavigationPath::Status::Complete,"normal Detour routes over composed terrain bake");
    auto* stream=world.SceneControl()->Streaming(world,error);demand=stream?stream->Request("landscape",false,error):0;Check(demand!=0,"ordinary additive region demand accepted");phase=1;
   }
   auto* stream=world.SceneControl()->Streaming(world,error);
   if(phase==1&&stream&&(previous=stream->Resolve("landscape",1))){retired=world.RuntimeBody(previous);auto hit=world.Physics().Raycast({48,15,0},{0,-1,0},30);Check(retired.IsValid()&&hit.hit&&hit.point.y>1,"region uses permitted replacement collision");Check(stream->Release(demand)&&stream->Unload("landscape"),"normal region unload requested");phase=2;}
   if(phase==2&&stream&&!stream->Resolve("landscape",1)){Check(!world.Physics().IsBodyEnabled(retired),"region unload retires old collider handle");demand=stream->Request("landscape",false,error);phase=3;}
   if(phase==3&&stream&&stream->Resolve("landscape",1)){auto current=world.RuntimeBody(stream->Resolve("landscape",1));Check(current.IsValid()&&(current.id!=retired.id||current.world!=retired.world),"revisit uses safe remapped body identity");stream->Release(demand);stream->Unload("landscape");Check(world.SceneControl()->Reload(error),"normal whole scene reload queued");original=&world;phase=4;}
   if(phase==4&&original!=&world){Check(world.RuntimeCharacter(10)&&world.Physics().AliveBodyCount()==bodies,"reload cleans contributed runtime instances");done=true;Quit();}
  }else {
   auto* service=world.SceneControl()->Saves(host.Resources());
   if(phase==0&&count>20){freeze=true;original=&world;
    if(mode=="write"){expected={{"state",State(world)},{"position",{motor->position.x,motor->position.y,motor->position.z}}};std::ofstream(output/"expected.json")<<expected.dump();request=service->Request("save","m76-lab","Modding Lab","{}",error);}
    else {std::ifstream(output/"expected.json")>>expected;request=service->Request("load","m76-lab","","{}",error);}Check(request!=0,"normal modern save/load request");phase=1;
   }
   if(request){auto* status=service->Status(request);if(frame%1000==0){std::string prepared;WorldPersistence::Prepare(world,prepared);std::cout<<"SAVE-PROBE "<<(status?status->state:"missing")<<" content="<<service->Content()<<" prepared="<<prepared<<"\n";std::cout.flush();}if(status&&status->state=="failed"){Check(mode=="mismatch"&&original==&world&&status->error.find("mod loadout")!=std::string::npos,"changed loadout refused before live-world replacement");std::cout<<"DIAGNOSTIC "<<status->error<<'\n';done=true;Quit();}
    else if(status&&status->state=="completed"&&(mode=="write"||phase==2)){
     Check(mode!="mismatch","same exact loadout completes modern transaction");if(mode=="read"){Check(State(world)==expected.at("state"),"fresh process restores saved mod fact without replayed signal");auto point=expected.at("position");Check(glm::length(motor->position-glm::vec3(point[0],point[1],point[2]))<1e-4f,"fresh process restores authoritative character pose");}
     Check(!service->Content().empty(),"effective-content fingerprint established");done=true;Quit();
    }}std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if(frame>12000){Check(false,"bounded test completion");done=true;Quit();}
 };
 std::vector<char*> arguments{argv[0],argv[1]};std::string no="--no-mods";if(mode=="zero")arguments.push_back(no.data());auto result=Application{}.Run(int(arguments.size()),arguments.data(),&control);Check(result==0,"normal Application shutdown");std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:result;
}
