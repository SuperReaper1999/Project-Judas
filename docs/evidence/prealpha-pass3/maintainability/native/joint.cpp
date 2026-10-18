#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
int checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
int main(int argc,char** argv){
 namespace fs=std::filesystem;
 fs::path out=argc==3?argv[2]:"build/m45-application";fs::create_directories(out);
 fs::path fixture=out/"project";fs::remove_all(fixture);fs::copy("projects/joint_demo",fixture,fs::copy_options::recursive);
 // Assertions are project JS in a disposable fixture. All calls use the real
 // VM/runtime/physics bridge and the normal Application loop.
 fs::rename(fixture/"Assets/scripts/queries.js",fixture/"Assets/scripts/demo.js");
 fs::copy_file(fixture/"Assets/scripts/queries.js.judasmeta",fixture/"Assets/scripts/demo.js.judasmeta");
 std::ofstream(fixture/"Assets/scripts/demo.js.judasmeta")<<"JudasAssetMeta 1\nid \"44444444444444444444444444444403\"\ntype script\nsource \"\"\n";
 std::ofstream(fixture/"Assets/scripts/queries.js")<<R"JS(
import Demo from './demo.js';import {world,physics} from 'judas';
export default class extends Demo {
 start(){super.start();const assert=(x,s)=>{if(!x)throw Error(s);};
 const joint=physics.joint(world.entity('31'));assert(joint&&joint.valid&&joint.state.active,'live joint');
 joint.setEnabled(false);assert(!joint.state.active,'disabled joint');joint.setEnabled(true);
 joint.setLimits(-.7,.7);joint.setMotor(-.5,10);joint.setSpring(0,2,1);
 let rejected=false;try{joint.setMotor(1,-1);}catch(e){rejected=true;}assert(rejected,'invalid motor rejected');
 const root=world.spawnPrefab('44444444444444444444444444444402',{position:{x:10,y:4,z:0},rotation:{w:1,x:0,y:0,z:0},scale:{x:1,y:1,z:1}});
 let owned=null;for(const child of root.children){const h=physics.joint(child);if(h)owned=h;}
 assert(owned&&owned.valid,'spawned prefab joint');root.destroy();assert(!owned.valid,'stale joint fails safely');
 this.state.checked=true;
 }
})JS";
 const bool visibleCapture=std::getenv("JUDAS_CAPTURE_VISIBLE")!=nullptr;
 ApplicationControl control;control.hidden=!visibleCapture;control.frameSeconds=[](float){return 1.f/60;};
 control.hostReady=[](EngineHost& host){std::string error;host.Audio().Init(error,true);};
 int frames=0;
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  if(++frames==3){if(world.Scripts())for(const auto& observed:world.Scripts()->Capture())std::printf("TRACE_JOINT_SCRIPT %s\n",observed.json.c_str());JointState observedJoint;if(world.Physics().GetJoint(world.RuntimeJoint(31),observedJoint))std::printf("TRACE_JOINT_PHYSICAL %.9g %.9g\n",observedJoint.coordinate,observedJoint.motorImpulse);
   Check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),"actual JS joint callbacks have no faults");
   if(world.Scripts()){for(auto& fault:world.Scripts()->Diagnostics())std::printf("FAULT %s\n",fault.message.c_str());
    auto states=world.Scripts()->Capture();Check(!states.empty()&&states[0].json.find("\"checked\":true")!=std::string::npos,"JS controls limits/motors/springs and rejects stale/invalid handles");}
   auto doc=world.UI().Find("game_ui");Check(doc&&world.UI().Element(doc,"counter")->text.find("Door angle:")!=std::string::npos,"joint state drives authored HUD");
   Check(host.GetWindow().Input().Map().Find("spawn_prefab")!=nullptr,"project spawn binding loaded");
   // Reproduce backend capture loss while logical gameplay capture remains requested.
   SDL_SetRelativeMouseMode(SDL_FALSE);
   SDL_Event focus{};focus.type=SDL_WINDOWEVENT;focus.window.windowID=SDL_GetWindowID(host.GetWindow().NativeWindow());focus.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;SDL_PushEvent(&focus);
   SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_SCANCODE_P;SDL_PushEvent(&event);
  }else if(frames==4){
   if(visibleCapture)Check(SDL_GetRelativeMouseMode()==SDL_TRUE,"refocus restores actual SDL relative mouse capture");
   auto states=world.Scripts()->Capture();Check(states[0].json.find("\"spawns\":2")!=std::string::npos,"physical P activates demo prefab spawn");
   SDL_Event event{};event.type=SDL_KEYUP;event.key.keysym.scancode=SDL_SCANCODE_P;SDL_PushEvent(&event);
   event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_SCANCODE_ESCAPE;SDL_PushEvent(&event);
  }else if(frames==5){
   auto doc=world.UI().Find("game_ui");Check(play.IsPaused()&&world.UI().Element(doc,"pause")->visible,"physical Escape displays authored pause panel");
   if(visibleCapture)Check(SDL_GetRelativeMouseMode()==SDL_FALSE,"pause releases actual SDL capture");
   SDL_Event event{};event.type=SDL_KEYUP;event.key.keysym.scancode=SDL_SCANCODE_ESCAPE;SDL_PushEvent(&event);
  }else if(frames==6){
   SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_SCANCODE_ESCAPE;SDL_PushEvent(&event);
  }else if(frames==7){
   auto doc=world.UI().Find("game_ui");Check(!play.IsPaused()&&!world.UI().Element(doc,"pause")->visible,"physical Escape resumes demo");
   if(visibleCapture)Check(SDL_GetRelativeMouseMode()==SDL_TRUE,"resume restores actual SDL capture");
   SDL_Event event{};event.type=SDL_QUIT;SDL_PushEvent(&event);
  }
 };
 std::string project=(fixture/"joint_demo.judasproj").string();char name[]="judas";char* args[]={name,project.data()};Application app;
 Check(app.Run(2,args,&control)==0&&frames>=7,"normal Application startup, rendering and shutdown");
 std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
