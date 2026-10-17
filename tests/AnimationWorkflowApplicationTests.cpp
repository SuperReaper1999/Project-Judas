#include "Application.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "InteractivePlay.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <filesystem>
#include <cstdio>
static int checks=0,failures=0;
static void Check(bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
void Key(SDL_Scancode code,bool down){SDL_Event e{};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.keysym.scancode=code;e.key.keysym.sym=SDL_GetKeyFromScancode(code);SDL_PushEvent(&e);}
int main(){std::filesystem::create_directories(".cache/m95/application");ApplicationControl c;c.hidden=true;c.frameSeconds=[](float){return 1.f/60;};c.hostReady=[](EngineHost& h){std::string e;h.Audio().Init(e,true);};int frame=0,ticks=0;bool done=false,rotated=false;glm::vec3 start{0},targetStart{0};float high=0;bool saving=false,loading=false;int saveFrame=0,loadFrame=0;RuntimeWorld* savedWorld=nullptr;
 c.afterFrame=[&](EngineHost& host,RuntimeWorld& w,InteractivePlay& play){++frame;
  if(w.Scripts()&&!w.Scripts()->Diagnostics().empty()){for(auto& e:w.Scripts()->Diagnostics())std::printf("FAULT %s\n",e.message.c_str());Check(false,"ordinary gameplay callbacks remain healthy");SDL_Event q{};q.type=SDL_QUIT;SDL_PushEvent(&q);return;}
  auto* a=w.RuntimeAnimation(10);if(a&&a->asset&&w.Scripts()){
   ++ticks;auto records=w.Scripts()->Capture();nlohmann::json state;for(auto& r:records)if(r.entity==90)state=nlohmann::json::parse(r.json);
   if(ticks==3){start=w.RuntimeCharacter(10)->position;targetStart=w.Physics().GetTransform(w.RuntimeBody(30)).position;Key(SDL_SCANCODE_W,true);}
   if(ticks==100){Key(SDL_SCANCODE_W,false);Check(w.RuntimeCharacter(10)->position.z<start.z-2.5,"JS fixed root motion actually moves character through normal application");Check(state.value("footsteps",0)>=2,"authored footstep markers reach ordinary JS game logic");Key(SDL_SCANCODE_E,true);}
   if(ticks==101)Key(SDL_SCANCODE_E,false);
   if(ticks==150){Check(state.value("strikes",0)==1,"contact marker triggers exactly one game-authored impulse");Check(glm::length(w.Physics().GetTransform(w.RuntimeBody(30)).position-targetStart)>.15,"real physical target moves after animation-timed hit");Key(SDL_SCANCODE_Q,true);}
   if(ticks==151)Key(SDL_SCANCODE_Q,false);
   if(ticks==225){Check(state.value("interactions",0)==1,"interaction marker drives authored state");Key(SDL_SCANCODE_SPACE,true);}
   if(ticks==226)Key(SDL_SCANCODE_SPACE,false);
   if(ticks>=225&&ticks<285)high=std::max(high,w.RuntimeCharacter(10)->position.y);
   if(ticks==285){Check(high>.5,"JS jump and animated airborne pose coexist with root-motion motor");auto& renderer=host.GetRenderer();std::vector<unsigned char> pixels;renderer.CaptureFrame(host.GetWindow().Width(),host.GetWindow().Height(),pixels);Check(WriteRgbPng(".cache/m95/application/studio.png",host.GetWindow().Width(),host.GetWindow().Height(),pixels),"actual gameplay screenshot");Key(SDL_SCANCODE_ESCAPE,true);}
   if(ticks==286)Key(SDL_SCANCODE_ESCAPE,false);
   if(ticks==290){Check(play.IsPaused(),"authored pause halts normal gameplay");Key(SDL_SCANCODE_RETURN,true);}
   if(ticks==291)Key(SDL_SCANCODE_RETURN,false);
   if(ticks==300){Check(!play.IsPaused(),"authored pause resumes through logical UI activation");Key(SDL_SCANCODE_F2,true);}
   if(ticks==301)Key(SDL_SCANCODE_F2,false);
   if(ticks>305&&w.SceneControl()->Current()=="Scenes/rotated.judas"){if(!rotated){rotated=true;Check(true,"normal M43 transition loads rotated authored scene");Key(SDL_SCANCODE_W,true);}if(ticks==365){Key(SDL_SCANCODE_W,false);auto* m=w.RuntimeCharacter(10);Check(m&&m->result.supported&&m->result.up.x<-.99f,"same JS motor and animation remain supported under rotated gravity");Check(m&&m->position.z<3,"rotated root motion moves along rotated studio plane");saving=true;saveFrame=frame;savedWorld=&w;Key(SDL_SCANCODE_F6,true);}}
  }
  if(saving){
   if(frame==saveFrame+1)Key(SDL_SCANCODE_F6,false);
   auto* service=w.SceneControl()?w.SceneControl()->Saves(host.Resources()):nullptr;
   if(!loading&&frame>saveFrame+3&&service&&!service->Busy()){
    bool saved=false;for(const auto& slot:service->List())saved|=slot.id=="motion-studio"&&slot.status=="compatible";
    if(saved){Check(true,"real F6 game input commits a normal SaveService slot");loading=true;loadFrame=frame;Key(SDL_SCANCODE_F7,true);}
   }
   if(loading&&frame==loadFrame+1)Key(SDL_SCANCODE_F7,false);
   if(loading && &w!=savedWorld){Check(w.RuntimeAnimation(10)&&w.RuntimeAnimation(10)->asset&&w.RuntimeCharacter(10),"real F7 game input restores animated motor through normal save-service publication");done=true;saving=false;SDL_Event q{};q.type=SDL_QUIT;SDL_PushEvent(&q);}
   SDL_Delay(1);
  }
  if(frame>1600){Check(false,"workflow completes within bounded application frames");SDL_Event q{};q.type=SDL_QUIT;SDL_PushEvent(&q);}
 };
 char name[]="judas",path[]="projects/animation_studio/animation_studio.judasproj";char* args[]={name,path};Application app;Check(app.Run(2,args,&c)==0&&done,"ordinary application lifecycle / safe scene replacement / shutdown");std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
