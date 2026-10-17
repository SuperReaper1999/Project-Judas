#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "SaveService.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
static int checks=0,failures=0;
static void Check(bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
int main(){
 for(const char* name:{"shooter_game","boundary_demo","asset_lab","streamed_range"}){
    const auto launched=std::chrono::steady_clock::now();
    int frame=0,stops=0;std::string error;bool reloaded=false,busyControl=false;ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};
    control.hostReady=[&](EngineHost& host){host.Audio().Init(error,true);};
    control.worldReady=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){auto& scripts=world.PrepareScriptTools();scripts.SetDebugger([&](const auto& stop){++stops;Check(!stop.frames.empty()&&stop.entity!=0,"existing consumer pauses at actual project source with owner identity");return ScriptDebugAction::Continue;});scripts.BreakOnNextInstruction();};
    control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay&){
        ++frame;if(frame==1)std::printf("CONSUMER %s startup_to_first_frame_ms=%.4f\n",name,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-launched).count());auto* scripts=world.Scripts();if(!scripts||!scripts->Diagnostics().empty()){
            if(scripts){for(auto& d:scripts->Diagnostics())std::printf("CONSUMER_ERROR %s %s\n",d.asset.c_str(),d.message.c_str());}
            Check(false,"existing consumer callbacks remain healthy");SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);return;
        }
        if(frame>=4&&!reloaded&&(!world.SceneControl()||!world.SceneControl()->OperationInProgress())){
            if(world.SceneControl()&&world.SceneControl()->StreamingIfLoaded()&&!busyControl){
                auto* saves=world.SceneControl()->Saves(host.Resources());auto request=saves?saves->Request("list","","","{}",error):0;
                Check(request!=0,"real asynchronous save-list operation queues for negative reload control");
                auto before=scripts->Capture();const bool accepted=scripts->Reload(true,error);auto after=scripts->Capture();bool same=before.size()==after.size();for(size_t i=0;same&&i<before.size();++i)same=before[i].json==after[i].json;
                Check(!accepted&&error.find("pending")!=std::string::npos&&same,"busy save operation rejects reload before changing VM state");busyControl=true;return;
            }
            const auto owner=world.ScriptKeys().front();UIDocument document;UIElement canvas;canvas.id="canvas";canvas.kind=UIKind::Canvas;document.elements.push_back(canvas);
            auto ownedUI=world.UI().Add(document,"m94-slot-owned",owner.first,error,owner.second);
            auto independentUI=world.UI().Add(document,"m94-independent",0,error);
            Check(ownedUI&&independentUI,"consumer exposes slot-owned and independent UI lifetime controls");
            auto* streaming=world.SceneControl()?world.SceneControl()->StreamingIfLoaded():nullptr;uint64_t owned=0,independent=0;
            if(streaming){owned=streaming->Request("gallery-0",false,error,10,1);independent=streaming->Request("gallery-0",false,error);Check(owned&&independent,"real streamed consumer holds slot-owned and independent region requests");}
            const auto count=world.Physics().AliveBodyCount();auto begin=std::chrono::steady_clock::now();reloaded=scripts->Reload(true,error);
            std::printf("CONSUMER %s reload_ms=%.4f bodies=%zu error=%s\n",name,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count(),count,error.c_str());
            Check(reloaded,"existing consumer can deliberately replace script session with JSON state");
            Check(!world.UI().Document(ownedUI)&&world.UI().Document(independentUI),"reload invalidates old slot UI while independent UI survives");world.UI().Unload(independentUI);
            if(streaming){Check(!streaming->Status(owned)&&streaming->Status(independent).has_value(),"reload retires old slot requests while independent streaming demand survives");streaming->Release(independent);}Check(world.Physics().AliveBodyCount()==count,"consumer reload does not reconstruct or drop physical bodies");
        }
        if(frame>=12&&reloaded){Check(stops==1&&reloaded,"consumer completed debug/reload/resume application frames");SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}
        if(frame>2000){Check(false,"consumer reload completes within bounded asynchronous workload");SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}
    };
    std::string project=std::string("projects/")+name+"/"+name+".judasproj";std::string executable="judas";char* args[]={executable.data(),project.data()};Application app;Check(app.Run(2,args,&control)==0,"existing consumer clean startup and shutdown");
 }
 {
    namespace fs=std::filesystem;auto root=fs::absolute(".cache/m94/lab-iteration");fs::remove_all(root);fs::create_directories(root);fs::copy("projects/developer_lab",root,fs::copy_options::recursive);
    int frame=0;std::vector<std::string> observedGains;bool observedFault=false,edited=false;
    ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};
    control.hostReady=[](EngineHost& h){std::string e;h.Audio().Init(e,true);};
    control.worldReady=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){auto& vm=w.PrepareScriptTools();vm.SetDebugger([&](const ScriptDebugStop& stop){for(auto& v:stop.frames.front().variables)if(v.name=="local gain")observedGains.push_back(v.value);return ScriptDebugAction::Continue;});vm.SetBreakpoints({{"Assets/scripts/calibration.js",5,true}});};
    auto key=[](bool down){SDL_Event e{};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.keysym.scancode=SDL_SCANCODE_E;e.key.keysym.sym=SDLK_e;SDL_PushEvent(&e);};
    control.beforeFrame=[&](EngineHost&,RuntimeWorld&,InteractivePlay&){if(frame==1||frame==5)key(true);if(frame==2||frame==6)key(false);};
    control.afterFrame=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){++frame;auto* vm=w.Scripts();
        if(frame==3){observedFault=vm&&vm->Diagnostics().size()==1&&vm->Diagnostics()[0].message.find("Calibration gain")!=std::string::npos;Check(observedFault,"lab reproduces real helper fault while unrelated presentation stays alive");
            auto path=root/"Assets/scripts/calibration.js";std::ifstream input(path);std::string source{std::istreambuf_iterator<char>(input),{}};input.close();auto at=source.find("gain: 0");if(at!=std::string::npos){source.replace(at,7,"gain: 2");std::ofstream(path)<<source;edited=true;}
            std::string error;Check(edited&&vm->Reload(false,error),"external source repair and deliberate fresh reload recover faulted game without restart");
        }
        if(frame==10){bool repaired=false;if(vm)for(const auto& r:vm->Capture())if(r.entity==30)repaired=r.json.find("\"calibrated\":true")!=std::string::npos;
            Check(observedFault&&edited&&repaired&&vm->Diagnostics().empty(),"same E input now executes repaired calibration and worker continues");
            Check(std::find(observedGains.begin(),observedGains.end(),"0")!=observedGains.end()&&std::find(observedGains.begin(),observedGains.end(),"2")!=observedGains.end(),"real project breakpoints inspect failing and corrected local values");
            SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);
        }
    };
    std::string executable="judas",project=(root/"developer_lab.judasproj").string();char* args[]={executable.data(),project.data()};Application app;Check(app.Run(2,args,&control)==0,"lab edit-debug-fix-reload cycle shuts down cleanly");
 }
 std::printf("M94_CONSUMERS %d checks %d failures\n",checks,failures);return failures?1:0;
}
