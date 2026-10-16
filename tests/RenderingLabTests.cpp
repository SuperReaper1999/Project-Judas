#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "WorldPresentation.h"
#include "WorldPersistence.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "Project.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
static unsigned checks=0,failures=0;
static void Check(bool yes,const char* label){++checks;failures+=!yes;std::printf("%s %s\n",yes?"PASS":"FAIL",label);}
int main(int argc,char**argv){std::setvbuf(stdout,nullptr,_IOLBF,0);std::filesystem::path out=argc>1?argv[1]:".cache/m92/lab";std::filesystem::create_directories(out);std::string error;std::string project=argc>2?argv[2]:"projects/rendering_lab/rendering_lab.judasproj";
 if(argc<=2){auto copy=std::filesystem::absolute(out/"project");std::filesystem::remove_all(copy);std::filesystem::create_directories(copy);std::filesystem::copy("projects/rendering_lab",copy,std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);project=(copy/"rendering_lab.judasproj").string();}
 ApplicationControl control;control.hidden=true;unsigned frames=0,worlds=0;control.frameSeconds=[](float){return 1.f/60;};
 control.hostReady=[](EngineHost& h){h.GetWindow().SetTestInputMode(true);SDL_GL_SetSwapInterval(0);};
 control.worldReady=[&](EngineHost& h,RuntimeWorld&,InteractivePlay&){h.Resources().WaitForAll();++worlds;};
 control.beforeFrame=[&](EngineHost& h,RuntimeWorld&,InteractivePlay&){h.GetRenderer().ResetStats();
  for(auto [frame,key]:{std::pair<unsigned,const char*>{15,"key:L"},{30,"key:L"},{45,"key:H"},{60,"key:V"},{80,"key:V"},{95,"key:Escape"},{105,"key:Escape"},{115,"key:F6"},{130,"key:H"},{145,"key:F7"}})if(frames==frame||frames==frame+1)h.GetWindow().QueueTestPhysical(key,frames==frame?1:0);
 };
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){++frames;
  if(frames==10){Check(w.Scripts()&&w.Scripts()->Diagnostics().empty(),"registered rendering lab script starts without faults");Check(w.StaticLights().size()==128&&h.GetRenderer().Stats().dynamicLights>5,"ordinary project submits many actual independent light sources");Check(h.GetRenderer().Stats().instancedDraws>0,"ordinary project uses actual GPU instancing");}
  if(frames==20)Check(!w.RuntimeDefinition(10000)->light->enabled,"logical L toggles individual lights through public JS");
  if(frames==35)Check(w.RuntimeDefinition(10000)->light->enabled,"logical L restores light source contribution");
  if(frames==50)Check(!w.RuntimeDefinition(10000)->light->castsShadow,"logical H changes ordinary per-light shadow request");
  if(frames==70)Check(w.PlayersIfLoaded()&&w.PlayersIfLoaded()->HasViews()&&h.GetRenderer().Stats().offscreenPasses>=2,"logical V renders independent split-screen views");
  if(frames==90)Check(!w.PlayersIfLoaded()->HasViews(),"split views can be left without retaining active cameras");
  if(frames==100)Check(w.UI().Paused(),"pause UI receives input focus");
  if(frames==110)Check(!w.UI().Paused(),"pause resumes cleanly");
  if(frames==135)Check(w.RuntimeDefinition(10000)->light->castsShadow,"post-save change establishes a different live light state");
  if(frames==180){Check(w.Scripts()->Diagnostics().empty(),"all tested light/view/save callbacks remain fault-free");Check(!w.RuntimeDefinition(10000)->light->castsShadow,"F7 restores the saved light state after a different live edit");std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);WriteRgbPng((out/"gallery.png").string(),h.GetWindow().Width(),h.GetWindow().Height(),pixels);std::printf("LAB draws=%u instances=%u lights=%u shadow_bytes=%zu light_bytes=%zu fixed_ms=%.6f\n",h.GetRenderer().Stats().drawCalls,h.GetRenderer().Stats().instances,h.GetRenderer().Stats().dynamicLights,h.GetRenderer().Stats().shadowBytes,h.GetRenderer().Stats().lightBytes,play.LastFixedStepMilliseconds());w.UI().RequestQuit();}
 };
 std::string name="judas";char* args[]={name.data(),project.data()};Application app;Check(app.Run(2,args,&control)==0&&frames==180,"ordinary application startup, rendering and Stop");std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}
