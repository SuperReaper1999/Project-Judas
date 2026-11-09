#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "ProjectExporter.h"
#include "SceneSerialization.h"
#include <stdexcept>
#include <SDL2/SDL.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
int checks=0,failures=0;
void Check(bool ok,const char* name){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",name);}
int main(int argc,char** argv){
 namespace fs=std::filesystem;
 fs::path out=argc==3?argv[2]:"build/m43-tests";fs::create_directories(out);
 // The fixture owns only the scene/session contracts exercised below.
 fs::path fixture=fs::absolute(out/"project");fs::remove_all(fixture);
 Project authored;std::string why;
 if(!Project::CreateNew(fixture.string(),"Scene transitions",authored,why))throw std::runtime_error(why);
 auto asset=[&](const char* file,const char* identity,AssetType type){
  if(!AssetDatabase::WriteMeta((fixture/"Assets"/file).string()+".judasmeta",identity,type,"Original focused Judas regression fixture",why))throw std::runtime_error(why);
 };
 const char* events="42424242424242424242424242424201",*hud="42424242424242424242424242424202",*prefabId="42424242424242424242424242424203",*uiId="42424242424242424242424242424204";
 std::ofstream(fixture/"Assets/events.js")<<R"JS(import {scenes,session} from 'judas';
 export default class {onTriggerEnter(){if(scenes.current!=='Scenes/a.judas')return;
 session.set('hasKey',true);session.set('visits',(session.get('visits')||0)+1);
 scenes.load('Scenes/b.judas');session.set('callbackCompleted',true);}})JS";
 std::ofstream(fixture/"Assets/hud.js")<<R"JS(import {ui,input,world} from 'judas';
 export default class {start(){ui.get('game_ui').modal=false;input.pointerCapture=true;world.setView({position:{x:0,y:3,z:8},rotation:{w:1,x:0,y:0,z:0}},60);}})JS";
 asset("events.js",events,AssetType::Script);asset("hud.js",hud,AssetType::Script);
 UIDocument document;UIElement canvas;canvas.id="root";canvas.kind=UIKind::Canvas;document.elements.push_back(canvas);UIElement counter;counter.id="counter";counter.parent="root";counter.kind=UIKind::Text;counter.text="Scene fixture";document.elements.push_back(counter);
 if(!SaveUIDocument((fixture/"Assets/menu.judasui").string(),document,why))throw std::runtime_error(why);
 asset("menu.judasui",uiId,AssetType::UI);
 Scene prefab;auto& body=prefab.CreateObject("Trigger entrant");body.body=SceneBodyComponent{};body.body->motion=SceneBodyMotion::Dynamic;
 if(!SaveSceneToFile(prefab,(fixture/"Assets/participant.judasprefab").string(),why))throw std::runtime_error(why);
 asset("participant.judasprefab",prefabId,AssetType::Prefab);
 Scene a;
 auto& controller=a.CreateObject("Controller");controller.ui=SceneUIComponent{uiId,"game_ui"};controller.scripts.push_back({1,hud,true,"{}"});
 auto& camera=a.CreateObject("View");camera.transform.position={0,3,8};
 a.CreateObject("Marker");auto& prop=a.CreateObject("Authored movable");prop.transform.position={-4,1,0};prop.body=SceneBodyComponent{};prop.body->motion=SceneBodyMotion::Dynamic;prop.render=SceneRenderComponent{};
 a.CreateObject("Marker two");auto& gate=a.CreateObject("Transition trigger");gate.transform.position={0,1,0};gate.body=SceneBodyComponent{};gate.body->sensor=true;gate.body->halfExtents={1,1,1};gate.scripts.push_back({1,events,true,"{}"});
 authored.Settings().startupScene="Scenes/a.judas";
 if(!authored.Save(why)||!SaveSceneToFile(a,authored.Resolve("Scenes/a.judas"),why)||!SaveSceneToFile(a,authored.Resolve("Scenes/b.judas"),why))throw std::runtime_error(why);
 std::string project=authored.ProjectFile();
 ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};
 control.hostReady=[](EngineHost& host){std::string error;host.Audio().Init(error,true);};
 int frame=0,phase=0;EntityId spawned=0;const RuntimeWorld* preservedWorld=nullptr;std::shared_ptr<SceneSession> session;
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  ++frame;auto s=world.SceneControl();std::string error;
  if(phase==0){session=s;Check(s&&s->Current()=="Scenes/a.judas","initial registered scene A");
   Check(!s->Request("/etc/passwd",error)&&!s->Pending(),"absolute/unregistered request rejected without queuing");
   Check(!s->Set("bad","[1e999]",error),"nonfinite session data rejected");
   Check(s->Set("nested","{\"a\":[true,3,\"ok\",null]}",error),"bounded JSON session state accepted");
   SceneTransform t;t.position={0,1,0};spawned=world.SpawnPrefab("42424242424242424242424242424203",t,error);
   Check(spawned!=0,"ordinary runtime prefab triggers real sensor");phase=1;
  }else if(phase==1&&s->Current()=="Scenes/b.judas"){
   Check(s==session&&s->Get("callbackCompleted")=="true"&&s->Get("hasKey")=="true","M42 callback finished before A to B; session survives");
   Check(!world.FindEntity(spawned),"old runtime spawned hierarchy destroyed");
   Check(world.Scripts()&&world.Scripts()->Diagnostics().empty()&&world.UI().Find("game_ui"),"fresh scripts and scene UI start without faults");
   world.UI().Document(world.UI().Find("game_ui"))->modal=true;phase=2;
  }else if(phase==2&&s->Current()=="Scenes/b.judas"){
   Check(play.IsPaused()&&!host.GetWindow().IsMouseCaptured(),"outgoing modal UI owns the cursor");
   Check(s->Request("Scenes/a.judas",error)&&s->Request("Scenes/b.judas",error),"multiple valid requests accepted deterministically");
  }else if(phase==2&&s->Current()=="Scenes/a.judas"){
   Check(true,"B to A; first request wins");
   Check(!play.IsPaused()&&host.GetWindow().IsMouseCaptured(),"new unpaused scene recaptures cursor after modal scene");
   auto t=world.RuntimeDefinition(4)->transform;t.position.x=22;world.SetRuntimeTransform(4,t);
   SceneTransform spawn;spawn.position={-10,1,4};spawned=world.SpawnPrefab("42424242424242424242424242424203",spawn,error);
   Check(s->Reload(error),"reload queued");phase=3;
  }else if(phase==3){
   Check(!world.FindEntity(spawned),"reload removes spawned entities");
   Check(world.PresentedTransform(4,world.RuntimeDefinition(4)->transform,1).position.x<0,"reload restores authored transform");
   Check(s->Get("nested")=="{\"a\":[true,3,\"ok\",null]}"&&s->Get("visits")=="1","reload retains session but not trigger history");
   // A registered but malformed candidate must leave the live world untouched.
   Check(!s->Request("Scenes/missing.judas",error),"missing scene fails safely");
   Check(world.Scripts()->Diagnostics().empty(),"all JS lifecycle callbacks valid");
   auto h=world.UI().Find("game_ui");world.UI().Element(h,"counter")->text="LIVE WORLD MARKER";
   preservedWorld=&world;
   std::ofstream(fixture/"Scenes/b.judas")<<"INVALID SCENE";
   Check(s->Request("Scenes/b.judas",error),"registered malformed scene request deferred");phase=4;
  }else if(phase==4){
   Check(s->Current()=="Scenes/a.judas"&&!s->Pending(),"malformed candidate consumed without changing current scene");
   Check(&world==preservedWorld&&world.Scripts()&&!world.FindEntity(spawned)&&s->Get("hasKey")=="true","failed transition retains live systems and session");
   Check(world.UI().Element(world.UI().Find("game_ui"),"counter")->text=="LIVE WORLD MARKER","rejected candidate preserves live UI state");
   phase=5;SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);
  }
  if(frame>60){Check(false,"transition progress");SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}
 };
 char name[]="judas";char* args[]={name,project.data()};Application app;
 Check(app.Run(2,args,&control)==0&&phase==5,"ordinary Application transitions and shutdown");
 Check(session.use_count()==1,"no runtime world retained by project/session state");session.reset();
 std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
