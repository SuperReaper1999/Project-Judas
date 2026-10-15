// M88 ordinary game through the real Application/RuntimeWorld/JS/physics path.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "RuntimeUI.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "ScreenshotWriter.h"
#include "ScriptSystem.h"
#include "../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <cmath>
#include <set>
using J=nlohmann::json;
unsigned checks=0,failures=0;
void Check(bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);std::fflush(stdout);}
J State(RuntimeWorld& world){if(world.Scripts())for(const auto& r:world.Scripts()->Capture())if(r.entity==50)return J::parse(r.json);return J::object();}
int main(){
 std::filesystem::create_directories(".cache/m88/application");
#ifdef _WIN32
 _putenv_s("JUDAS_WORLD_STATE",".cache/m88/application/legacy.worldstate");
#else
 setenv("JUDAS_WORLD_STATE",".cache/m88/application/legacy.worldstate",1);
#endif
 Application app;ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};
 unsigned frame=0,pausedSteps=0,finishFrame=0;std::uint64_t request=0;bool saved=false,loaded=false,reloaded=false;RuntimeWorld* savedWorld=nullptr;
 unsigned gamePhase=0,phaseStart=0,placedAt=0;EntityId target=0;bool sawWide=false,sawMulti=false;std::set<EntityId> drops;
 J savedState;std::string error;Window* window=nullptr;float beforeX=0;bool injected=false,hit=false;
 control.hostReady=[&](EngineHost& host){window=&host.GetWindow();window->SetTestInputMode(true);SDL_GL_SetSwapInterval(0);std::string ignored;host.Audio().Init(ignored,true);};
 control.beforeFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
  ++frame;
  if(frame==30)window->QueueTestPhysical("key:Escape",1);
  if(frame==32)window->QueueTestPhysical("key:Escape",0);
  if(frame==35)Check(play.IsPaused(),"logical Back/Escape opens menu once without immediately closing it");
  if(frame==36)window->QueueTestPhysical("key:Escape",1);
  if(frame==38)window->QueueTestPhysical("key:Escape",0);
  if(frame==40)Check(!play.IsPaused(),"second logical Back resumes without input leakage");
  if(frame==41)window->QueueTestPhysical("key:Escape",1);
  if(frame==42)window->QueueTestPhysical("key:Escape",0);
  if(frame==44)window->QueueTestPhysical("key:Space",1);
  if(frame==45)window->QueueTestPhysical("key:Space",0);
  if(frame==48)Check(!play.IsPaused()&&State(world).value("phase","")=="ready","menu confirm consumes its shared launch binding");
  if(frame==49)host.GetRenderer().ResetStats(); // Stats are caller-reset cumulative counters.
  if(frame==50){Check(host.GetRenderer().Stats().shadowPasses==(host.GetRenderer().ShadowsEnabled()?1u:0u),"renderer shadow quality changes submission only");Check(world.QueryEntities(CategoryBit(3)).size()==42,"42 real physical blocks published");Check(State(world).value("phase","")=="ready","JS begins ready with a physical ball");
   pausedSteps=play.FixedStepsSinceReset();SDL_Event event{};event.type=SDL_APP_WILLENTERBACKGROUND;SDL_PushEvent(&event);}
  if(frame==54)Check(play.FixedStepsSinceReset()==pausedSteps,"OS background freezes authoritative physics");
  if(frame==55){SDL_Event event{};event.type=SDL_APP_DIDENTERFOREGROUND;SDL_PushEvent(&event);}
  if(frame==60){Check(play.FixedStepsSinceReset()>pausedSteps&&play.FixedStepsSinceReset()<pausedSteps+10,"resume continues without catch-up spiral");beforeX=world.Physics().GetTransform(world.RuntimeBody(world.QueryEntities(CategoryBit(1)).at(0))).position.x;window->Input().SetTouch(9,3,.7f,.7f,1,true);}
  if(frame==90){auto paddle=world.QueryEntities(CategoryBit(1)).at(0);Check(world.Physics().GetTransform(world.RuntimeBody(paddle)).position.x>beforeX+.5f,"normalized touch drives real kinematic paddle");window->Input().SetTouch(9,3,0,0,0,false);window->QueueTestPhysical("key:Space",1);}
  if(frame==92)window->QueueTestPhysical("key:Space",0);
  if(frame==96){Check(State(world).value("phase","")=="playing","logical launch starts the physical ball");Check(State(world).value("score",-1)==0,"miss/no impact cannot score");}
  if(frame==100){auto balls=world.QueryEntities(CategoryBit(2));Check(!balls.empty(),"ball survived ordinary launch");if(!balls.empty()){
   auto id=balls[0];auto b=world.RuntimeBody(id);auto t=world.Physics().GetTransform(b);t.position={0,2.1f,0};world.SetRuntimeTransform(id,{t.position,t.rotation,{1,1,1}});world.Physics().SetLinearVelocity(b,{0,15,0});injected=true;}}
  if(frame==120){hit=State(world).value("hits",0)>0;Check(injected&&hit,"actual native sphere/block contact delivers JS destruction and score");Check(world.QueryEntities(CategoryBit(3)).size()<42,"hit removes real block collision");
   world.UI().Document(world.UI().Find("breakwater"))->modal=true;savedState=State(world);
   request=world.SceneControl()->Saves(*world.Resources())->Request("save","m88-test","M88 test","{}",error);Check(request!=0,"normal project save accepted");}
  if(frame>120&&!loaded){SDL_Delay(1);if(frame%100==0){const auto* s=world.SceneControl()->Saves(*world.Resources())->Status(request);if(s)std::printf("SAVE %s %s\n",s->state.c_str(),s->error.c_str());}}
  if(frame>120&&request&&!saved){auto* status=world.SceneControl()->Saves(*world.Resources())->Status(request);if(status&&status->state=="completed"){
   saved=true;savedWorld=&world;request=world.SceneControl()->Saves(*world.Resources())->Request("load","m88-test","","{}",error);Check(request!=0,"normal project load accepted");}}
  if(saved&&!loaded&&savedWorld!=&world){loaded=true;world.Scripts()->Synchronize(world.ScriptSlots());Check(State(world).value("score",-1)==savedState.value("score",-2),"save restores actual gameplay score");Check(world.QueryEntities(CategoryBit(3)).size()==42-unsigned(State(world).value("hits",0)),"save restores actual remaining collision population");Check(world.QueryEntities(CategoryBit(1)).size()==1,"save restores one paddle rather than spawning a duplicate");}
  if(loaded&&!reloaded&&frame>200){window->QueueTestPhysical("key:R",1);reloaded=true;finishFrame=frame+15;}
  if(reloaded&&frame>=finishFrame-10)window->QueueTestPhysical("key:R",0);
  if(finishFrame&&frame==finishFrame-2){Check(State(world).value("score",-1)==0&&world.QueryEntities(CategoryBit(3)).size()==42,"normal scene reload resets round and physical blocks");}
  if(gamePhase==1){
   if(frame==phaseStart+2)window->QueueTestPhysical("key:Space",0);
   if(State(world).value("phase","")=="playing"){
    auto blocks=world.QueryEntities(CategoryBit(3)),balls=world.QueryEntities(CategoryBit(2));sawMulti|=balls.size()>1;
    auto paddles=world.QueryEntities(CategoryBit(1));if(!paddles.empty()){
     Shape shape;BodyTransform pose;world.Physics().GetBodyShape(world.RuntimeBody(paddles[0]),shape,pose);sawWide|=shape.halfExtents.x>2.5f;
     for(auto drop:world.QueryEntities(CategoryBit(4)))if(drops.insert(drop).second){world.SetRuntimeTransform(drop,{{pose.position.x,-8.79f,0},{1,0,0,0},{1,1,1}});world.Physics().SetLinearVelocity(world.RuntimeBody(drop),{0,-3,0});}
    }
    if(!blocks.empty()&&!balls.empty()&&(!world.HasTag(target,3)||frame>placedAt+12)){
     target=blocks.back();placedAt=frame;const auto p=world.Physics().GetTransform(world.RuntimeBody(target)).position;
     world.SetRuntimeTransform(balls[0],{{p.x,p.y-1.05f,0},{1,0,0,0},{1,1,1}});world.Physics().SetLinearVelocity(world.RuntimeBody(balls[0]),{0,15,0});
    }
   }
   if(State(world).value("phase","")=="won"){
    Check(world.QueryEntities(CategoryBit(3)).empty(),"winning round removes all real collision blocks");
    Check(State(world).value("score",0)>=4200&&play.IsPaused(),"winning score and modal end screen");
    Check(State(world).value("powers",0)>=3,"physical capsule/paddle contacts award all three power-up types");
    Check(sawWide,"wide power-up changes actual collider geometry");Check(sawMulti,"multiball creates independently simulated physical balls");
    gamePhase=2;phaseStart=frame;window->QueueTestPhysical("key:R",1);
   }
  }else if(gamePhase==2){
   if(frame==phaseStart+2)window->QueueTestPhysical("key:R",0);
   if(State(world).value("phase","")=="ready"&&frame>phaseStart+3){gamePhase=3;phaseStart=frame;window->QueueTestPhysical("key:Space",1);}
  }else if(gamePhase==3){
   if(frame>=phaseStart+2)window->QueueTestPhysical("key:Space",0);
   if(State(world).value("phase","")=="playing"){
    for(auto ball:world.QueryEntities(CategoryBit(2)))world.SetRuntimeTransform(ball,{{0,-13.2f,0},{1,0,0,0},{1,1,1}});
   }else if(State(world).value("phase","")=="ready"&&frame>phaseStart+4){phaseStart=frame;window->QueueTestPhysical("key:Space",1);}
   else if(State(world).value("phase","")=="lost"){
    Check(State(world).value("lives",-1)==0&&play.IsPaused(),"three physical misses produce one loss screen with zero lives");
    Check(world.Scripts()->Diagnostics().empty(),"no faults through complete winning and losing rounds");world.UI().RequestQuit();gamePhase=4;
   }
  }
  if(frame>5000){Check(false,"bounded application completion");world.UI().RequestQuit();}
 };
 control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay&){
  float audioFrames[1600];host.Audio().ReadWithoutDevice(audioFrames,800);
  if(frame==80){std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window->Width(),window->Height(),pixels);Check(WriteRgbPng(".cache/m88/application/desktop.png",window->Width(),window->Height(),pixels),"capture real desktop game frame");}
  if(finishFrame&&frame==finishFrame){for(const auto& d:world.Scripts()->Diagnostics())std::printf("SCRIPT %s %s\n",d.callback.c_str(),d.message.c_str());Check(world.Scripts()->Diagnostics().empty(),"no script exceptions after input/contact/save/reload");Check(saved&&loaded&&reloaded,"save/load/reload lifecycle completed");gamePhase=1;phaseStart=frame;window->QueueTestPhysical("key:Space",1);finishFrame=0;}
 };
 const char* args[]={"judas","projects/breakwater/breakwater.judasproj"};int result=app.Run(2,const_cast<char**>(args),&control);Check(result==0,"ordinary game application exits cleanly");std::printf("%u checks %u failures\n",checks,failures);return failures?1:0;
}
