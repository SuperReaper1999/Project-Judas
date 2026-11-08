// Sprout Patch: drives the real rendered application through the complete
// farming loop with synthetic touch (joystick + on-screen buttons) and keys,
// then saves, independently reads the committed snapshot and loads it back.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "WorldPersistence.h"
#include "SceneSerialization.h"
#include "TestInput.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <filesystem>
#include <functional>
#include <thread>
#include <cstdio>
#include <cmath>
#include <string>
using Json=nlohmann::json;
namespace fs=std::filesystem;
unsigned checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
Json State(RuntimeWorld& w){for(const auto& r:w.Scripts()->Capture())if(r.entity==1)return Json::parse(r.json);throw std::runtime_error("missing farmer state");}
glm::vec3 Position(RuntimeWorld& w){return w.Physics().GetTransform(w.RuntimeBody(1)).position;}
// Cell in front of the farmer's feet, mirroring game.js target().
std::pair<int,int> Target(RuntimeWorld& w){const auto p=Position(w);const auto f=State(w)["facing"];
 return {int(std::floor(p.x+f["x"].get<float>()*.8f)),int(std::floor(p.y-.45f+f["y"].get<float>()*.8f))};}
int main(int argc,char** argv){try{
 if(argc!=3)return 2;
 const fs::path output=fs::absolute(argv[2]),project=output/"project";
 if(fs::exists(project))throw std::runtime_error("output must be fresh");
 fs::create_directories(output);fs::copy(fs::path(argv[1]).parent_path(),project,fs::copy_options::recursive);
 const std::string userData=(output/"user").string();SDL_setenv("XDG_DATA_HOME",userData.c_str(),1);
 std::string projectFile=(project/"Sprout_Patch.judasproj").string(),error;
 unsigned frame=0,phase=0,phaseAt=0,grown=0;uint64_t request=0;RuntimeWorld* prior=nullptr;bool complete=false;glm::vec3 expected{};
 ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};
 control.hostReady=[](EngineHost& h){std::string e;h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);h.Audio().Init(e,true);};
 control.worldReady=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){prior=&w;};
 auto quit=[](){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);};
 // Normalized centre of a HUD element anchored at a corner, using the same
 // aspect-matched 720-unit-high canvas that game.js installs.
 auto button=[](EngineHost& h,glm::vec2 anchor,glm::vec2 offset,glm::vec2 size){
  const float ref=720.f*float(h.GetWindow().Width())/float(h.GetWindow().Height());
  const glm::vec2 corner=anchor*glm::vec2(ref,720)+offset;const glm::vec2 centre=corner-anchor*size+size*.5f;
  return centre/glm::vec2(ref,720);};
 const glm::vec2 corner{1,1};
 auto hoe=[&](EngineHost& h){return button(h,corner,{-264,-188},{180,64});};
 auto seeds=[&](EngineHost& h){return button(h,corner,{-264,-114},{180,64});};
 auto water=[&](EngineHost& h){return button(h,corner,{-264,-40},{180,64});};
 auto use=[&](EngineHost& h){return button(h,corner,{-40,-40},{200,200});};
 auto save=[&](EngineHost& h){return button(h,{1,0},{-24,118},{130,56});};
 // Tap with finger 2 (finger 1 is reserved for the joystick).
 std::function<glm::vec2(EngineHost&)> tapTarget;unsigned tapAt=0;
 auto tap=[&](std::function<glm::vec2(EngineHost&)> where){tapTarget=where;tapAt=frame+1;}; // pressed on the next frame
 auto key=[&](EngineHost& h,SDL_Scancode code,unsigned at){if(frame==at)QueueTestKey(h.GetWindow(),code,true);if(frame==at+2)QueueTestKey(h.GetWindow(),code,false);};
 control.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){
  if(tapTarget){const auto p=tapTarget(h);
   if(frame==tapAt)h.GetWindow().SetTestTouch(2,p.x,p.y,1,true);
   if(frame==tapAt+3){h.GetWindow().SetTestTouch(2,p.x,p.y,0,false);tapTarget=nullptr;}}
  if(phase==20&&&w!=prior){Check(glm::length(Position(w)-expected)<1e-6f,"real load restores the farmer's exact native position before stepping");phase=21;phaseAt=frame;}
  // Phase 0: steer down with the touch joystick (finger 1, left half).
  if(phase==0&&frame==20)h.GetWindow().SetTestTouch(1,.2f,.6f,1,true);
  if(phase==0&&frame==22)h.GetWindow().SetTestTouch(1,.2f,.78f,1,true);
  if(phase==0&&frame>22&&Position(w).y<2.f){h.GetWindow().SetTestTouch(1,.2f,.78f,0,false);phase=1;phaseAt=frame;}
  if(phase==1&&frame==phaseAt+30){
   const auto [x,y]=Target(w);Check(x==0&&y==0,"touch joystick moves the native farmer and faces the field cell (0,0)");
   tap(hoe);phase=2;phaseAt=frame;}
  if(phase==2&&frame==phaseAt+10){tap(use);phase=3;phaseAt=frame;}
  if(phase==4&&frame==phaseAt+5){tap(seeds);}
  if(phase==4&&frame==phaseAt+15){tap(use);phase=5;phaseAt=frame;}
  if(phase==6&&frame==phaseAt+5)tap(water);
  if(phase==6&&frame==phaseAt+15){tap(use);phase=7;phaseAt=frame;}
  // Phase 8: keyboard walk west to the pond and refill.
  if(phase==8&&frame==phaseAt+5)QueueTestKey(h.GetWindow(),SDL_SCANCODE_A,true);
  if(phase==8&&frame>phaseAt+5&&Position(w).x<-5.f){QueueTestKey(h.GetWindow(),SDL_SCANCODE_A,false);phase=9;phaseAt=frame;}
  if(phase==9)key(h,SDL_SCANCODE_SPACE,phaseAt+30);
  // Phase 11: walk back east, then face down onto the planted cell.
  if(phase==11&&frame==phaseAt+5)QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,true);
  if(phase==11&&frame>phaseAt+5&&Position(w).x>.35f){QueueTestKey(h.GetWindow(),SDL_SCANCODE_D,false);phase=12;phaseAt=frame;}
  if(phase==12)key(h,SDL_SCANCODE_S,phaseAt+30);
  if(phase==13)key(h,SDL_SCANCODE_SPACE,phaseAt+5);
  if(phase==15)key(h,SDL_SCANCODE_SPACE,phaseAt+5);
  if(phase==17&&frame==phaseAt+5)tap(save);
 };
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){
  ++frame;
  if(w.Scripts()&&!w.Scripts()->Diagnostics().empty()){Check(false,w.Scripts()->Diagnostics().front().message.c_str());quit();return;}
  auto state=State(w);
  auto capture=[&](const char* name){std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);Check(WriteRgbPng((output/name).string(),h.GetWindow().Width(),h.GetWindow().Height(),pixels),name);};
  auto plot=[&]()->Json{auto& p=state["plots"];return p.contains("0,0")?p["0,0"]:Json();};
  if(frame==15)capture("start.png");
  if(phase==3&&frame==phaseAt+15){Check(state["tool"]=="hoe"&&plot().is_object()&&plot()["stage"]==-1,"HOE button + USE button (second finger) till the targeted field cell");phase=4;phaseAt=frame;}
  if(phase==5&&frame==phaseAt+15){Check(plot()["stage"]==0&&state["seeds"]==5,"SEEDS + USE plant a seed and spend one");phase=6;phaseAt=frame;}
  if(phase==7&&frame==phaseAt+15){Check(state["tool"]=="water"&&!plot()["wet"].get<bool>()&&state["water"]==0,"an empty watering can cannot water");capture("planted.png");phase=8;phaseAt=frame;}
  if(phase==9&&frame==phaseAt+45){const auto [x,y]=Target(w);Check(x<=-7&&y>=1&&y<=4,"native pond collision stops the farmer facing pond water");
   Check(state["water"]==5,"USE at the pond refills the watering can");phase=11;phaseAt=frame;}
  if(phase==12&&frame==phaseAt+50){const auto [x,y]=Target(w);Check(x==0&&y==0,"keyboard steering returns to face the seeded cell");phase=13;phaseAt=frame;}
  if(phase==13&&frame==phaseAt+15){Check(plot()["wet"].get<bool>()&&state["water"]==4,"watering wets the seeded soil");phase=14;phaseAt=frame;}
  if(phase==14){
   const int stage=plot()["stage"];
   if(stage==3){Check(grown==2&&!plot()["wet"].get<bool>(),"crop reached ripe after watering between each stage");capture("ripe.png");phase=15;phaseAt=frame;}
   else if(stage>int(grown)){
    Check(!plot()["wet"].get<bool>(),"each growth stage dries the soil");++grown;
    if(grown==1){// A dry crop does not grow: wait a full stage time before rewatering.
     phase=30;phaseAt=frame;}
    else{QueueTestKey(h.GetWindow(),SDL_SCANCODE_SPACE,true);phase=31;phaseAt=frame;}
   }
  }
  if(phase==30&&frame==phaseAt+420){Check(plot()["stage"]==1,"a crop in dry soil does not grow");QueueTestKey(h.GetWindow(),SDL_SCANCODE_SPACE,true);phase=31;phaseAt=frame;}
  if(phase==31&&frame==phaseAt+2){QueueTestKey(h.GetWindow(),SDL_SCANCODE_SPACE,false);}
  if(phase==31&&frame==phaseAt+10){Check(plot()["wet"].get<bool>(),"rewatering resumes growth");phase=14;}
  if(phase==15&&frame==phaseAt+15){Check(state["harvested"]==1&&state["seeds"]==6&&plot()["stage"]==-1,"harvesting a ripe turnip yields it and a seed and leaves tilled soil");capture("harvested.png");phase=17;phaseAt=frame;}
  auto* saves=w.SceneControl()->Saves(h.Resources());
  if(phase==17&&frame==phaseAt+10){request=state["request"].is_null()?0:state["request"].get<uint64_t>();Check(request!=0,"SAVE button requests a real save");phase=18;}
  if(phase==18){const auto* status=saves->Status(request);
   if(status&&status->state=="failed"){Check(false,status->error.c_str());quit();return;}
   if(status&&status->state=="completed"){
    GameSnapshot snapshot;bool recovered=false;SaveStorage storage(saves->Root());Scene scene;
    Check(storage.Read("sprout-patch",snapshot,recovered,error)&&WorldPersistence::SceneFrom(snapshot.participants,scene,error),"read committed farm snapshot independently");
    expected=scene.Find(1)->transform.position;
    bool soil=false,crop=false;
    for(const auto& [key,text]:scene.Settings().authoringRecipes)if(key.rfind("tilemap.",0)==0){
     const Json recipe=Json::parse(text); // keep the parsed recipe alive while iterating its cells
     for(const auto& c:recipe["data"]["cells"]){if(c["x"]==0&&c["y"]==0&&c["layer"]=="soil")soil=c["frame"]=="cell-1-1";if(c["x"]==0&&c["y"]==0&&c["layer"]=="crops")crop=true;}}
    Check(soil&&!crop,"saved tilemap retains tilled soil and the harvested (empty) crop cell");
    prior=&w;request=saves->Request("load","sprout-patch","","{}",error);Check(request!=0,"request real load");phase=20;
   }else std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  if(phase==21&&frame==phaseAt+5){Check(state["harvested"]==1&&plot()["stage"]==-1&&state["water"]==2,"loaded farm state matches the saved progress");complete=true;quit();}
  if(frame>4000){std::printf("TIMEOUT phase=%u state=%s\n",phase,state.dump().c_str());Check(false,"bounded farming workflow completes");quit();}
 };
 Application app;char program[]="judas";char* args[]={program,projectFile.data()};Check(app.Run(2,args,&control)==0&&complete,"real rendered application completes and shuts down");
 std::printf("SPROUT_PATCH checks=%u failures=%u\n",checks,failures);return failures?1:0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
