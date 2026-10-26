#include "Application.h"
#include "ToolProcess.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "ScriptSystem.h"
#include "WorldStreaming.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include "RuntimeUI.h"
#include <algorithm>
#include <SDL2/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <functional>
using Json = nlohmann::json;
namespace {
int checks=0, failures=0;
void Check(bool ok,const char* name){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<name<<std::endl;}
Json State(RuntimeWorld& world,EntityId id){if(world.Scripts())for(const auto& row:world.Scripts()->Capture())if(row.entity==id)return Json::parse(row.json);return Json::object();}
Json Vector(glm::vec3 p){return {p.x,p.y,p.z};}
glm::vec3 Vector(const Json& p){return {p[0].get<float>(),p[1].get<float>(),p[2].get<float>()};}
void PlacePlayer(RuntimeWorld& world,glm::vec3 p){auto t=world.RuntimeDefinition(100)->transform;t.position=p;world.SetRuntimeTransform(100,t);world.RuntimeCharacter(100)->velocity={};}
void Quit(){SDL_Event event{};event.type=SDL_QUIT;SDL_PushEvent(&event);}
}
int main(int argc,char** argv){
 if(argc!=4)return 2;
 std::string mode=argv[2];auto proof=std::filesystem::absolute(argv[3]);
 if(mode=="cycle") {
  if(std::filesystem::exists(proof)){std::cerr<<"Use a fresh proof directory\n";return 2;}
  auto executable=std::filesystem::absolute(argv[0]);std::string output,error;
  if(!CaptureToolOutput(executable,{std::filesystem::absolute(argv[1]).string(),"write",proof.string()},output,error,std::chrono::seconds(45),65536)){std::cerr<<output<<error;return 1;}
  std::cout<<output;
  std::string slot;{std::ifstream path(proof/"slot-path.txt");std::getline(path,slot);}
  auto bytes=[&](){std::ifstream f(slot,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};auto original=bytes();
  if(original.empty()){std::cerr<<"No original saved bytes\n";return 1;}
  if(!CaptureToolOutput(executable,{std::filesystem::absolute(argv[1]).string(),"read",proof.string()},output,error,std::chrono::seconds(45),65536)){std::cerr<<output<<error;return 1;}
  std::cout<<output;if(bytes()!=original){std::cerr<<"Read changed original save\n";return 1;}
  std::cout<<"PASS fresh-process lesson cycle preserves source save bytes\n";return 0;
 }
 std::filesystem::create_directories(proof);
 unsigned frame=0,phase=0,wait=0;uint64_t request=0;bool done=false,aimApplied=false,layoutChecked=false;EntityId lastBlue=0;glm::vec3 start{};Json expected;std::vector<std::string> pulse,held;
 ApplicationControl control;control.hidden=true;
 control.hostReady=[](EngineHost& host){host.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string error;host.Audio().Init(error,true);};
 control.frameSeconds=[](float){return 1.f/90;};
 control.beforeFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay&){
  for(auto& entry:host.GetWindow().Input().Map().entries)for(auto& binding:entry.bindings){float value=0;if(binding.scale>0&&(std::find(pulse.begin(),pulse.end(),entry.name)!=pulse.end()||std::find(held.begin(),held.end(),entry.name)!=held.end()))value=1;host.GetWindow().QueueTestPhysical(binding.control,value);}
  pulse.clear();
  if(phase==2&&!aimApplied){host.GetWindow().QueueTestPhysical("mouse:dy",90);aimApplied=true;}
  if(mode=="write"&&phase==12){auto* service=world.SceneControl()->SavesIfLoaded();if(service)for(uint64_t id=1;id<10;++id){auto* status=service->Status(id);if(status&&status->operation=="save"&&(status->state=="writing"||status->state=="completed")){request=id;expected={{"root",State(world,200)},{"carPosition",Vector(world.Physics().GetTransform(world.RuntimeBody(7000)).position)},{"carState",State(world,7000)}};std::ofstream(proof/"expected.json")<<expected.dump(2);phase=13;break;}}}
 };
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay&){++frame;try{
  if(world.Scripts()&&!world.Scripts()->Diagnostics().empty())throw std::runtime_error(world.Scripts()->Diagnostics().front().message);
  if(++wait<6)return;
  if(!layoutChecked&&State(world,100).contains("yaw")) {
   auto doc=world.UI().Find("lesson");auto* layout=world.UI().LayoutOf(doc,"lesson");
   Check(layout&&layout->visible&&layout->rect.position.x>=0&&layout->rect.position.y>=0&&layout->rect.position.x+layout->rect.size.x<=host.GetWindow().Width()&&layout->rect.position.y+layout->rect.size.y<=host.GetWindow().Height(),"lesson HUD actually lies inside the viewport");
   std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(host.GetWindow().Width(),host.GetWindow().Height(),pixels);WriteRgbPng((proof/(mode+"-visible.png")).string(),host.GetWindow().Width(),host.GetWindow().Height(),pixels);layoutChecked=true;
  }
  auto next=[&](unsigned p){phase=p;wait=0;};
  auto* stream=world.SceneControl()->StreamingIfLoaded();
  const auto region=stream?stream->Regions():std::vector<RegionStatus>{};
  const auto state=region.empty()?"":region[0].state;
  const auto blue=stream?stream->Resolve("yard",10):0;
  if(mode=="incompatible"){
   if(phase==0&&State(world,100).contains("driving")){std::string error;request=world.SceneControl()->Saves(*world.Resources())->Request("load","lesson","","{}",error);Check(request!=0,"incompatible load is handled as an ordinary asynchronous request");next(1);}
   else if(phase==1){auto* status=world.SceneControl()->SavesIfLoaded()->Status(request);if(status->state=="failed"){Check(status->error.find("Content fingerprint changed")!=std::string::npos,"actual refused load names changed content fingerprint");Check(status->error.find("matching game build")!=std::string::npos,"actual refused load offers safe recovery");Check(State(world,200).value("pushes",0)==0&&!State(world,100).value("driving",false),"refused load leaves the original current gameplay world intact");done=true;Quit();}else if(status->state=="completed")throw std::runtime_error("incompatible save incorrectly loaded");}
  }else if(mode=="read"){
   if(phase==0&&State(world,100).contains("driving")){std::string error;request=world.SceneControl()->Saves(*world.Resources())->Request("load","lesson","","{}",error);Check(request!=0,"fresh process requests ordinary saved lesson");next(1);}
   else if(phase==1){auto* status=world.SceneControl()->SavesIfLoaded()->Status(request);if(status->state=="failed")throw std::runtime_error(status->error);if(status->state=="completed"&&State(world,100).value("driving",false)){std::ifstream(proof/"expected.json")>>expected;Check(State(world,200)==expected["root"],"cold root crate counter restored exactly");Check(glm::distance(world.Physics().GetTransform(world.RuntimeBody(7000)).position,Vector(expected["carPosition"]))<.03f,"cold physical vehicle endpoint restored");Check(!world.RuntimeCharacter(100)->settings.enabled,"cold driver lease disables actual motor");start=world.Physics().GetTransform(world.RuntimeBody(7000)).position;held={"move_y"};if(world.UI().Paused())pulse={"pause"};next(2);}}
   else if(phase==2&&wait>180){held.clear();Check(glm::distance(start,world.Physics().GetTransform(world.RuntimeBody(7000)).position)>1,"cold restored vehicle drives physically again");pulse={"region"};next(3);}
   else if(phase==3&&blue&&State(world,blue).contains("pushes")){Check(State(world,blue)["pushes"]==1,"cold regional snapshot preserves exact project counter");Check(world.RuntimeBody(blue).IsValid(),"cold regional crate regains actual collision");done=true;Quit();}
  }else{
   if(phase==0&&State(world,100).contains("yaw")){Check(world.RuntimeBody(200).IsValid(),"ordinary root physical crate published");start=world.RuntimeCharacter(100)->position;held={"move_y"};next(1);}
   else if(phase==1&&wait>30){held.clear();Check(glm::distance(start,world.RuntimeCharacter(100)->position)>1,"lesson motor responds to actual WASD input");next(2);}
   else if(phase==2){pulse={"interact"};next(3);}
   else if(phase==3&&wait>30){Check(State(world,200).value("pushes",0)==1,"actual ray interaction increments project-owned root counter");pulse={"pause"};next(4);}
   else if(phase==4){Check(world.UI().Paused(),"ordinary pause blocks physical gameplay");pulse={"region"};next(5);}
   else if(phase==5&&state=="prepared"){Check(blue==0,"paused preparation is not mistaken for published collision");pulse={"pause"};next(14);}
   else if(phase==14&&state=="active"&&blue){lastBlue=blue;Check(world.RuntimeBody(blue).IsValid(),"requested regional collision genuinely publishes");if(mode=="missing-script"){Check(State(world,blue).empty(),"disabled script has no gameplay instance despite native body");Check(world.Scripts()->InspectPhase(blue,1)==ScriptInstancePhase::Absent,"runtime diagnostic distinguishes missing instance");Check(world.UI().Element(world.UI().Find("lesson"),"lesson")->text.find("not registered")!=std::string::npos,"lesson does not mistake active region for project registration");done=true;Quit();}else if(State(world,blue).contains("pushes")){Check(world.UI().Element(world.UI().Find("lesson"),"lesson")->text.find("registered")!=std::string::npos,"project registration is visible independently of native state");PlacePlayer(world,{8,.1f,4});next(6);}}
   else if(phase==6){pulse={"interact"};next(7);}
   else if(phase==7&&wait>60){Check(State(world,blue).value("pushes",0)==1,"actual regional ray interaction reaches registered script");pulse={"region"};next(8);}
   else if(phase==8&&state=="unloaded"){Check(!world.RuntimeBody(lastBlue).IsValid(),"regional crate actually unloads");PlacePlayer(world,{6,.1f,4});pulse={"interact"};next(9);}
   else if(phase==9&&State(world,100).value("driving",false)){start=world.Physics().GetTransform(world.RuntimeBody(7000)).position;held={"move_y"};next(10);}
   else if(phase==10&&wait>180){held.clear();Check(glm::distance(start,world.Physics().GetTransform(world.RuntimeBody(7000)).position)>1,"native wheels and powertrain move vehicle under project input");pulse={"pause"};next(11);}
   else if(phase==11){Check(world.UI().Paused(),"save captures paused physical boundary");pulse={"save"};next(12);}
   else if(phase==13){auto* status=world.SceneControl()->SavesIfLoaded()->Status(request);if(status->state=="failed")throw std::runtime_error(status->error);if(status->state=="completed"){Check(true,"ordinary native save commits complete lesson");std::ofstream(proof/"slot-path.txt")<<world.SceneControl()->SavesIfLoaded()->Root()+"/lesson.save";done=true;Quit();}}
  }
  if(frame>4500)throw std::runtime_error("bounded lesson test timed out at phase "+std::to_string(phase));
 }catch(const std::exception& error){Check(false,error.what());done=true;Quit();}};
 auto project=std::filesystem::absolute(argv[1]).string();std::string exe="workflow-observer";char* args[]={exe.data(),project.data()};int result=Application{}.Run(2,args,&control);Check(result==0&&done,"ordinary application exits cleanly");std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
