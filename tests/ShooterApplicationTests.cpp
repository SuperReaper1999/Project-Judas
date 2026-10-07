// Ordinary outer-loop input, UI, camera, reload and asynchronous resources.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "ScreenshotWriter.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <vector>
int checks=0,failures=0;
void Check(bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
void Key(SDL_Scancode key,bool down){SDL_Event e{};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.keysym.scancode=key;e.key.keysym.sym=SDL_GetKeyFromScancode(key);SDL_PushEvent(&e);}
void Click(bool down){SDL_Event e{};e.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_LEFT;SDL_PushEvent(&e);}
std::string State(RuntimeWorld& w){if(w.Scripts())for(auto& x:w.Scripts()->Capture())if(x.entity==10)return x.json;return {};}
int main(int argc,char**argv){if(argc<3){std::fprintf(stderr,"usage: %s <project.judasproj> <output-dir> [perf]\n",argv[0]);return 2;}std::filesystem::create_directories(argv[2]);const bool perf=argc>3;int frame=0;std::size_t paused=0;int shots=0;
 using Clock=std::chrono::steady_clock;Clock::time_point begin,last;std::vector<double> ms,fixed;
 std::ofstream csv(std::filesystem::path(argv[2])/(perf?"performance.csv":"application.csv"));csv<<"frame,cpu_ms,render_submit_ms,fixed_steps,wall_ms\n";
 ApplicationControl control;control.hidden=true;if(!perf)control.frameSeconds=[](float){return 1.f/60;};
 control.hostReady=[](EngineHost& h){std::string error;h.Audio().Init(error,true);Check(!h.Resources().BlockingMode(),"normal asynchronous resources");};
 control.beforeFrame=[&](EngineHost&,RuntimeWorld&,InteractivePlay& play){begin=Clock::now();if(perf)play.SetFixedStepMeasurementFlags(false,false,true);play.SetFixedStepObserver([&](const FixedStepMeasurements&,double value){if(perf&&frame>=30)fixed.push_back(value);});
  if(perf)return;
  if(frame==40||frame==65||frame==90||frame==111)Click(true);
  if(frame==41||frame==66||frame==91||frame==112)Click(false);
  if(frame==60)Key(SDL_SCANCODE_V,true);
  if(frame==61)Key(SDL_SCANCODE_V,false);
  if(frame==85)Key(SDL_SCANCODE_ESCAPE,true);
  if(frame==86)Key(SDL_SCANCODE_ESCAPE,false);
  if(frame==105)Key(SDL_SCANCODE_RETURN,true);
  if(frame==106)Key(SDL_SCANCODE_RETURN,false);
  if(frame==140)Key(SDL_SCANCODE_R,true);
  if(frame==141)Key(SDL_SCANCODE_R,false);
 };
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){const auto now=Clock::now();double cpu=std::chrono::duration<double,std::milli>(now-begin).count(),wall=frame?std::chrono::duration<double,std::milli>(now-last).count():0;last=now;
  if(perf&&frame>=30)ms.push_back(wall);
  csv<<frame<<','<<cpu<<','<<play.LastSceneMilliseconds()<<','<<play.LastFixedStepsThisFrame()<<','<<wall<<'\n';
  auto screen=[&](const char* name){std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);Check(WriteRgbPng((std::filesystem::path(argv[2])/name).string(),h.GetWindow().Width(),h.GetWindow().Height(),pixels),"actual GL world plus UI screenshot");};
  if(!perf){auto s=State(w);
   if(frame==35){Check(w.pointerCapture&&!play.IsPaused(),"normal standalone starts in playable captured view");screen("first.png");}
   if(frame==42)Check(s.find("\"score\":100")!=std::string::npos,"normal application fire scores on real target");
   if(frame==62){Check(s.find("\"third\":true")!=std::string::npos,"normal application camera toggle");screen("third.png");}
   if(frame==86){Check(play.IsPaused()&&play.LastFixedStepsThisFrame()==0,"pause shows UI before authoritative simulation");paused=play.FixedStepsSinceReset();auto p=s.find("\"shots\":");shots=p==std::string::npos?-1:std::atoi(s.c_str()+p+8);screen("pause.png");}
   if(frame==99){Check(play.FixedStepsSinceReset()==paused,"paused frames never step gameplay");Check(s.find("\"shots\":"+std::to_string(shots))!=std::string::npos,"paused fire does not leak");}
   if(frame==108)Check(!play.IsPaused()&&w.pointerCapture,"keyboard Resume restores game and capture");
   if(frame==149)Check(State(w).find("\"score\":0")!=std::string::npos&&State(w).find("\"shots\":0")!=std::string::npos,"queued outer-boundary reload genuinely reconstructs game");
  }
  if(frame==(perf?239:155)){Check(w.Scripts()&&w.Scripts()->Diagnostics().empty(),"no script faults in actual application");if(perf){double sum=0;for(auto x:ms)sum+=x;double step=0;for(auto x:fixed)step+=x;std::printf("M52_PERF frames=%zu mean_frame_ms=%.6f fps=%.3f mean_fixed_ms=%.6f bodies=%zu\n",ms.size(),sum/ms.size(),1000/(sum/ms.size()),fixed.empty()?0:step/fixed.size(),w.Physics().AliveBodies().size());}SDL_Event quit{};quit.type=SDL_QUIT;SDL_PushEvent(&quit);}
  ++frame;
 };
 char name[]="judas";char* args[]={name,argv[1]};Application app;Check(app.Run(2,args,&control)==0,"normal standalone shutdown");std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
