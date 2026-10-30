#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "WorldPersistence.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "TestInput.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <cstdio>
#include <chrono>
#include <algorithm>
using Json=nlohmann::json;
namespace fs=std::filesystem;
unsigned checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
Json State(RuntimeWorld& w){for(const auto& r:w.Scripts()->Capture())if(r.entity==1)return Json::parse(r.json);throw std::runtime_error("missing player state");}
int main(int argc,char** argv){try{
 if(argc!=3&&argc!=4)return 2;
 const fs::path output=fs::absolute(argv[2]),project=output/"project";
 const std::string mode=argc==4?argv[3]:"win";const bool coldWon=mode=="cold-win",cold=mode=="cold"||coldWon,loss=mode=="loss";
 if(!cold){if(fs::exists(project))throw std::runtime_error("output must be fresh");fs::create_directories(output);fs::copy(fs::path(argv[1]).parent_path(),project,fs::copy_options::recursive);}
 const std::string userData=(output/"user").string();SDL_setenv("XDG_DATA_HOME",userData.c_str(),1);
 std::string projectFile=(project/"Beacon_Grove.judasproj").string(),error;unsigned frame=0,phase=0,phaseAt=0;uint64_t request=0;RuntimeWorld* prior=nullptr;bool complete=false;glm::vec3 expected{},pausePosition{};double pauseTime=0;
 Project definition;Scene authored;Check(definition.Load(projectFile,error)&&LoadSceneFromFile(definition.StartupScenePath(),authored,error),"open fresh editable game");
 SceneObjectId gateId=0;for(const auto& o:authored.Objects())if(o.name=="Beacon gate - real static collider")gateId=o.id;
 std::chrono::steady_clock::time_point began;std::vector<double> timings;
 ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};
 control.hostReady=[](EngineHost& h){std::string e;h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);h.Audio().Init(e,true);};
 control.worldReady=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){prior=&w;};
 auto quit=[](){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);};
 control.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){
  began=std::chrono::steady_clock::now();
  if(phase==3&&&w!=prior){const auto p=w.Physics().GetTransform(w.RuntimeBody(1)).position;Check(glm::length(p-expected)<1e-6f,"real load restores exact native physical endpoint before stepping");
   phase=coldWon?9:4;phaseAt=frame;}
  if(phase==8&&&w!=prior){Check(glm::length(w.Physics().GetTransform(w.RuntimeBody(1)).position-expected)<1e-6f,"completed-save physical endpoint matches before stepping");Check(!w.RuntimeBody(gateId).IsValid(),"completed-save gate collider stays removed");phase=9;}
  if(!cold&&!loss){
   if(frame==5||frame==15)QueueTestKey(h.GetWindow(),SDL_SCANCODE_ESCAPE,true);
   if(frame==6||frame==16)QueueTestKey(h.GetWindow(),SDL_SCANCODE_ESCAPE,false);
   if(frame==8)QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,true);
   if(frame==12)QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,false);
  }
  if(loss){
   if(phase==0&&frame==10)QueueTestKey(h.GetWindow(),SDL_SCANCODE_A,true);
   if(phase==0&&w.Physics().GetTransform(w.RuntimeBody(1)).position.x < -2.7f){QueueTestKey(h.GetWindow(),SDL_SCANCODE_A,false);phase=10;}
   if(phase==11&&frame==phaseAt+2)QueueTestKey(h.GetWindow(),SDL_SCANCODE_R,true);
   if(phase==11&&frame==phaseAt+3)QueueTestKey(h.GetWindow(),SDL_SCANCODE_R,false);
   return;
  }
  if(!cold){
   if(frame==20)QueueTestKey(h.GetWindow(),SDL_SCANCODE_E,true);
   if(frame==21)QueueTestKey(h.GetWindow(),SDL_SCANCODE_E,false);
   if(phase==0&&frame==60){auto state=State(w);Check(!state["charged"].get<bool>()&&state["pulses"].get<int>()==1,"animation marker without cells/proximity cannot open the gate");QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,true);}
   if(phase==0&&w.Physics().GetTransform(w.RuntimeBody(1)).position.x>4.1f){QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,false);phase=1;phaseAt=frame;}
  }
  if(phase==4&&frame==phaseAt+15)QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,true);
  if(phase==4&&w.Physics().GetTransform(w.RuntimeBody(1)).position.x>6.1f){QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,false);phase=12;phaseAt=frame;}
  if(phase==12&&frame==phaseAt+15)QueueTestKey(h.GetWindow(),SDL_SCANCODE_E,true);
  if(phase==12&&frame==phaseAt+16)QueueTestKey(h.GetWindow(),SDL_SCANCODE_E,false);
  if(phase==12&&frame==phaseAt+50){const auto state=State(w);Check(state["cells"].size()==3&&!state["nearBeacon"].get<bool>()&&!state["charged"].get<bool>(),"all resources and a real animation marker still cannot activate from outside beacon proximity");QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,true);phase=13;}
  if(phase==13&&w.Physics().GetTransform(w.RuntimeBody(1)).position.x>7.8f){QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,false);phase=5;phaseAt=frame;}
  if(phase==5&&frame==phaseAt+15)QueueTestKey(h.GetWindow(),SDL_SCANCODE_E,true);
  if(phase==5&&frame==phaseAt+16)QueueTestKey(h.GetWindow(),SDL_SCANCODE_E,false);
  if(phase==6&&frame==phaseAt+1)QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,true);
 };
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){
  ++frame;if(frame>10&&(phase==0||phase==4||phase==5||phase==6))timings.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count());
  if(w.Scripts()&&!w.Scripts()->Diagnostics().empty()){Check(false,w.Scripts()->Diagnostics().front().message.c_str());quit();return;}
  auto state=State(w);
  auto capture=[&](const char* name){std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);Check(WriteRgbPng((output/name).string(),h.GetWindow().Width(),h.GetWindow().Height(),pixels),"capture actual rendered game");};
  if(frame==30)capture("start.png");
  if(!cold&&!loss&&frame==7){pausePosition=w.Physics().GetTransform(w.RuntimeBody(1)).position;pauseTime=state["time"];}
  if(!cold&&!loss&&frame==14){Check(w.Physics().GetTransform(w.RuntimeBody(1)).position==pausePosition&&state["time"]==pauseTime,"authored pause menu freezes native motion/game time and consumes held input");capture("pause.png");}
  if(phase==4&&frame==phaseAt+1)Check(state["cells"].size()==2&&!state["charged"].get<bool>(),"partial objective and unconsumed gate survive first restored script boundary");
  auto* saves=w.SceneControl()->Saves(h.Resources());
  if(cold&&phase==0){std::ifstream f(output/(coldWon?"expected-win.json":"expected.json"));Json e;f>>e;expected={e["x"],e["y"],e["z"]};prior=&w;request=saves->Request("load",coldWon?"grove-win":"grove-test","","{}",error);Check(request!=0,"fresh process requests real gameplay save");phase=3;}
  if(loss){
   if(phase==10&&state["lost"].get<bool>()){Check(state["health"].get<int>()==0&&!state["won"].get<bool>(),"real hazard overlap causes loss without fabricated damage");prior=&w;phase=11;phaseAt=frame;}
   if(phase==11&&&w!=prior){auto fresh=State(w);Check(fresh["health"]==3&&fresh["cells"].empty()&&!fresh["lost"].get<bool>(),"R reload really resets failed game state");capture("loss.png");complete=true;quit();}
  }else{
   if(phase==1&&frame>phaseAt+20){Check(state["cells"].size()==2,"actual movement and native triggers collect exactly two cells");request=saves->Request("save","grove-test","Partial grove","{}",error);Check(request!=0,"save played progress through production service");phase=2;}
   if(phase==2){const auto* status=saves->Status(request);if(status&&status->state=="completed"){
    GameSnapshot snapshot;bool recovered=false;SaveStorage storage(saves->Root());Scene scene;Check(storage.Read("grove-test",snapshot,recovered,error)&&WorldPersistence::SceneFrom(snapshot.participants,scene,error),"read committed snapshot independently");expected=scene.Find(1)->transform.position;
    std::ofstream(output/"expected.json")<<Json{{"x",expected.x},{"y",expected.y},{"z",expected.z}};
    prior=&w;request=saves->Request("load","grove-test","","{}",error);phase=3;
   }}
   if(phase==5&&state["charged"].get<bool>()){Check(state["cells"].size()==3,"all three actual collectibles required for activation");const bool gate=w.RuntimeBody(gateId).IsValid();
    Check(!gate,"timed activation removes actual gate collider");phase=6;phaseAt=frame;}
   if(phase==6&&state["won"].get<bool>()){Check(w.Physics().GetTransform(w.RuntimeBody(1)).position.x>11,"native player crosses former physical gate to exit");Check(!state["lost"].get<bool>(),"complete playable objective loop");capture("win.png");request=saves->Request("save","grove-win","Completed grove","{}",error);Check(request!=0,"save completed objective and destroyed gate");phase=7;}
  }
  if(phase==7){const auto* status=saves->Status(request);if(status&&status->state=="completed"){
   GameSnapshot snapshot;bool recovered=false;SaveStorage storage(saves->Root());Scene scene;
   Check(storage.Read("grove-win",snapshot,recovered,error)&&WorldPersistence::SceneFrom(snapshot.participants,scene,error),"read independently committed completed-game save");expected=scene.Find(1)->transform.position;
   std::ofstream(output/"expected-win.json")<<Json{{"x",expected.x},{"y",expected.y},{"z",expected.z}};
   prior=&w;request=saves->Request("load","grove-win","","{}",error);phase=8;
  }}
  if(phase==9){Check(state["won"].get<bool>()&&state["cells"].size()==3&&!w.RuntimeBody(gateId).IsValid(),"completed objectives and removed collider survive real save restoration");complete=true;quit();}
  if((phase==2||phase==3||phase==7||phase==8)&&request){const auto* status=saves->Status(request);if(status&&status->state=="failed"){Check(false,status->error.c_str());quit();}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  if(frame>1000){std::printf("TIMEOUT phase=%u state=%s\n",phase,state.dump().c_str());Check(false,"bounded game workflow completes");quit();}
 };
 Application app;char program[]="judas";char* args[]={program,projectFile.data()};Check(app.Run(2,args,&control)==0&&complete,"real rendered application completes and shuts down");
 std::sort(timings.begin(),timings.end());if(!timings.empty())std::printf("SOFTWARE_RENDERED_FRAME n=%zu median_ms=%.6f p95_ms=%.6f worst_ms=%.6f\n",timings.size(),timings[timings.size()/2],timings[size_t(std::ceil(timings.size()*.95))-1],timings.back());
 std::printf("BEACON_GROVE mode=%s checks=%u failures=%u\n",mode.c_str(),checks,failures);return failures?1:0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
