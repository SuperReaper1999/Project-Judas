// Real renderer/input/UI/outer lifecycle over the ordinary Connected Workshop.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "NetworkSession.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {
using Json=nlohmann::json;unsigned checks=0,failures=0;
void Check(bool value,const char* message){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",message);std::fflush(stdout);}
Json State(RuntimeWorld& world){if(world.Scripts())for(const auto& row:world.Scripts()->Capture())if(row.entity==1)return Json::parse(row.json);return Json::object();}
void Quit(){SDL_Event event{};event.type=SDL_QUIT;SDL_PushEvent(&event);}
}

int main(int argc,char** argv){
 const std::string project=argc>1?argv[1]:"projects/connected_workshop/connected_workshop.judasproj";
 const auto output=std::filesystem::absolute(argc>2?argv[2]:".cache/m99/application");
 const std::string port=argc>3?argv[3]:"29899";std::filesystem::create_directories(output);
 ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};
 control.hostReady=[](EngineHost& host){host.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string error;host.Audio().Init(error,true);};
 unsigned frame=0,phase=0,at=0;bool completed=false,rehosted=false;RuntimeWorld* previous=nullptr;glm::vec3 initial;float initialCrateX=0;
 control.beforeFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay&){
  auto& window=host.GetWindow();auto key=[&](const char* name,float value){window.QueueTestPhysical(std::string("key:")+name,value);};
  const auto age=frame-at;
  if(phase==1){if(age==0)key("W",1);if(age==16)key("W",0);}
  if(phase==2){if(age==0)key("D",1);if(age==3){key("D",0);key("E",1);}if(age==94)key("E",0);}
  if(phase==7){if(age==0)key("H",1);if(age==1)key("H",0);}
  if(phase==5){
   if(age==0){world.UI().Layout(window.Width(),window.Height());const auto* target=world.UI().LayoutOf(world.UI().Find("connected"),"leave");Check(target&&target->visible,"ordinary Disconnect button has a live hit rectangle");if(target){const auto p=target->rect.position+target->rect.size*.5f;window.SetTestPointer(int(p.x),int(p.y));window.QueueTestPhysical("mouse:Left",1);}}
   if(age==1)window.QueueTestPhysical("mouse:Left",0);
  }
 };
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  auto& window=host.GetWindow();const auto document=world.UI().Find("connected");
  if(world.Scripts()&&!world.Scripts()->Diagnostics().empty()){
   for(const auto& error:world.Scripts()->Diagnostics())std::printf("SCRIPT %s %s\n",error.callback.c_str(),error.message.c_str());
   Check(false,"real project callbacks remain healthy");Quit();return;
  }
  auto screenshot=[&](const char* name){std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window.Width(),window.Height(),pixels);Check(WriteRgbPng((output/name).string(),window.Width(),window.Height(),pixels),"capture actual renderer plus authored HUD");};
  const auto age=frame-at;
  if(phase==0&&document&&world.Network().Status()=="listening"&&frame>12){
   Check(world.Network().Mode()=="host"&&world.Network().Port()==unsigned(std::stoul(port)),"generic runtime launch settings start the actual game listener");
   Check(!world.pointerCapture&&!window.IsMouseCaptured()&&!play.IsPaused(),"overview game starts playable with usable pointer/UI and no global pause");
   Check(world.view.has_value(),"script-owned presentation camera is independent of motors");
   Check(world.UI().Element(document,"title")->text=="CONNECTED WORKSHOP"&&world.UI().Element(document,"score")->text.find("HOST")!=std::string::npos,"authored HUD binds and reflects real session mode");
   Check(world.UI().Element(document,"address_label")->text.find(port)!=std::string::npos,"HUD displays the effective custom command-line port");
   InputSystem routed;std::string routeError;Check(routed.SetMap(window.Input().Map(),routeError),"actual project input map installs independently");routed.SetPhysical("pad:South",1);routed.ConsumeBindings({"ui_activate"});Check(routed.Action("interact").held,"hovered UI activation bindings cannot consume the distinct gamepad shove");
   unsigned cargo=0;for(EntityId id=200;id<212;++id)cargo+=world.Physics().IsDynamicBody(world.RuntimeBody(id));Check(cargo==12,"all twelve shared cargo entities are actual dynamic bodies");
   screenshot("host.png");initial=world.RuntimeDefinition(100)->transform.position;phase=1;at=frame+1;
  }else if(phase==1&&age==18){
   Check(world.RuntimeDefinition(100)->transform.position.z<initial.z-.7f,"real keyboard input drives the existing CharacterMotor through game JS");
   auto actor=world.RuntimeDefinition(100)->transform;actor.position={6.3f,.95f,0};Check(world.SetRuntimeTransform(100,actor),"arrange motor for controlled interaction fixture");
   auto crate=world.RuntimeDefinition(200)->transform;crate.position={8.2f,.56f,0};Check(world.SetRuntimeTransform(200,crate),"arrange ordinary cargo before public game action");
   initialCrateX=crate.position.x;phase=2;at=frame+1;
  }else if(phase==2&&age==12){
   Check(world.Physics().GetLinearVelocity(world.RuntimeBody(200)).x>.25f,"ordinary E action produces real hit-point impulse and body motion");
  }else if(phase==2&&age==96){
   Check(world.Physics().GetTransform(world.RuntimeBody(200)).position.x>initialCrateX+.8f&&State(world).value("delivered",0)>0,"actual physical cargo crosses delivery boundary and scores");
   Check(world.UI().Element(document,"score")->text.find("AMBER 1")!=std::string::npos,"project score is visible through ordinary runtime UI");
   screenshot("delivered.png");
   Check(world.UI().Focus(document,"address"),"existing editable address field takes ordinary UI focus");
   Check(!play.IsPaused(),"editing connection settings does not pause the shared physical world");
   SDL_SetWindowSize(window.NativeWindow(),720,1280);phase=3;at=frame+1;
  }else if(phase==3&&window.Width()==720&&window.Height()==1280&&age>2){
   Check(world.UI().Document(document)->reference==glm::vec2(720,1280),"project reflows the same public UI into readable portrait layout");
   world.UI().Layout(window.Width(),window.Height());bool contained=true,distinct=true;std::vector<UIRect> rectangles;
   for(const auto* id:{"host","join","leave","address","transfer"}){const auto* l=world.UI().LayoutOf(document,id);contained&=l&&l->visible&&l->rect.position.x>=0&&l->rect.position.y>=0&&l->rect.position.x+l->rect.size.x<=window.Width()+.1f&&l->rect.position.y+l->rect.size.y<=window.Height()+.1f;if(l)rectangles.push_back(l->rect);}
   for(size_t a=0;a<rectangles.size();++a)for(size_t b=a+1;b<rectangles.size();++b){const auto& x=rectangles[a];const auto& y=rectangles[b];distinct&=std::min(x.position.x+x.size.x,y.position.x+y.size.x)<=std::max(x.position.x,y.position.x)||std::min(x.position.y+x.size.y,y.position.y+y.size.y)<=std::max(x.position.y,y.position.y);}
   Check(contained&&distinct,"phone-facing connection fields and buttons remain in bounds without overlap");
   screenshot("portrait.png");world.UI().Focus(document,"leave");phase=5;at=frame+1;
  }else if(phase==5&&age==4){
   Check(world.Network().Mode()=="offline"&&!world.SceneControl()->networkActive,"actual Disconnect click closes transport and releases scene/save gate");
   Check(world.Network().Entries().empty()&&world.Network().Peers().empty(),"closing the session clears transient network entities and peer records");
   if(!rehosted){phase=7;at=frame+1;}
   else{std::string error;previous=&world;Check(world.SceneControl()->Reload(error),"normal scene reload is accepted after explicit session closure");phase=6;at=frame+1;}
  }else if(phase==7&&world.Network().Status()=="listening"&&age>3){
   Check(world.Network().Port()==unsigned(std::stoul(port)),"normal H action retains the effective custom port after disconnect");
   rehosted=true;phase=5;at=frame+1;
  }else if(phase==6&&&world!=previous&&age>2&&document){
   Check(world.Network().Mode()=="offline"&&world.Network().Entries().empty()&&world.Network().Peers().empty(),"new scene does not revive live sockets or old network identities");
   Check(State(world).value("delivered",99)==0&&world.RuntimeDefinition(200)->transform.position.x<0,"reload restores authored physical cargo and fresh gameplay score");
   Check(world.Scripts()&&world.Scripts()->Diagnostics().empty()&&!window.IsMouseCaptured(),"reload preserves healthy project lifecycle and pointer policy");
   screenshot("reloaded.png");completed=true;phase=99;Quit();
  }
  if(frame>1800&&!completed){Check(false,"bounded connected application workflow completes");Quit();}
  ++frame;SDL_Delay(1);
 };
 char name[]="judas",hostFlag[]="--host";char* args[]={name,const_cast<char*>(project.c_str()),hostFlag,const_cast<char*>(port.c_str())};
 Application app;Check(app.Run(4,args,&control)==0&&completed,"normal application startup, input, renderer, UI, close/reload and shutdown");
 std::printf("SUMMARY checks=%u failures=%u\n",checks,failures);return failures?1:0;
}
