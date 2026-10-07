// Normal Application, public project scripts, real renderer and separate-process saves.
#include "Application.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "WorldStreaming.h"
#include "SaveService.h"
#include "WorldPersistence.h"
#include "TestInput.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <iostream>
namespace fs=std::filesystem;using J=nlohmann::json;
int checks=0,failures=0;
void Check(bool value,const std::string& label){++checks;failures+=!value;std::cout<<(value?"PASS ":"FAIL ")<<label<<std::endl;}
std::string Region(WorldStreaming& s,const std::string& name){for(auto& r:s.Regions())if(r.id==name)return r.state;return "missing";}
void Quit(){SDL_Event event{};event.type=SDL_QUIT;SDL_PushEvent(&event);}
J State(RuntimeWorld& w){for(auto& s:w.Scripts()->Capture())if(s.entity==3)return J::parse(s.json);return J::object();}
int main(int argc,char** argv){
    if(argc!=4)return 2;
    std::string project=argv[1],mode=argv[2],error;fs::path out=argv[3];fs::create_directories(out);
    unsigned frame=0,phase=0,at=0;bool done=false,ready=false,freeze=false;EntityId originalTraveller=0;uint64_t request=0;RuntimeWorld* before=nullptr;J expected;
    ApplicationControl control;control.hidden=true;
    control.hostReady=[](EngineHost& host){SDL_GL_SetSwapInterval(0);host.GetWindow().SetTestInputMode(true);};
    control.frameSeconds=[&](float){return freeze?0.f:1.f/60;};
    control.beforeFrame=[&](EngineHost& host,RuntimeWorld& w,InteractivePlay&){
        if(ready&&phase==0){for(auto [offset,key]:{std::pair<unsigned,SDL_Scancode>{3,SDL_SCANCODE_Z},{6,SDL_SCANCODE_V},{9,SDL_SCANCODE_L},{12,SDL_SCANCODE_ESCAPE},{15,SDL_SCANCODE_ESCAPE}})if(frame-at==offset||frame-at==offset+1)QueueTestKey(host.GetWindow(),key,frame-at==offset);}
        if(mode=="read"&&phase==7&&before!=&w&&State(w).contains("spawns")){
            auto now=State(w);Check(now.value("adopted","")==expected.value("adopted","")&&now.value("spawns",0)==expected.value("spawns",0),"cold save restores script-owned traveller references and spawn state");
            EntityId restored=0;Check((restored=w.SceneControl()->Streaming(w,error)->ResolvePersistentKey(now.value("adopted","")))!=0&&w.RuntimeDefinition(restored),"cold saved adopted traveller resolves safely");
            Check(w.view&&w.view->farPlane==2000,"cold process retains configured camera projection");phase=8;
        }
    };
    control.afterFrame=[&](EngineHost& host,RuntimeWorld& w,InteractivePlay&){
        ++frame;if(done)return;
        if(w.Scripts()&&!w.Scripts()->Diagnostics().empty()){for(auto& d:w.Scripts()->Diagnostics())std::cout<<"FAULT "<<d.message<<'\n';Check(false,"workshop ordinary JS callbacks");done=true;Quit();return;}
        auto* rig=w.RuntimeAnimation(6);
        if(!ready&&rig&&rig->asset&&w.BaselineFingerprint().size()==64){ready=true;at=frame;Check(w.ScriptObjects().size()>=364,"normal named workshop startup with several hundred objects");Check(host.Resources().TryGetModelParts(w.RuntimeDefinition(6)->render->meshAsset)&&host.Resources().TryGetModelParts(w.RuntimeDefinition(6)->render->meshAsset)->size()>=6,"approved original multipart fixture loads through normal M66 resources");}
        if(!ready){std::this_thread::sleep_for(std::chrono::milliseconds(2));if(frame>1500){Check(false,"bounded asset readiness");done=true;Quit();}return;}
        if(phase==0&&frame-at==13)Check(w.UI().Paused(),"opening pause remains modal across focus/back events");
        auto* stream=w.SceneControl()->Streaming(w,error);auto* saves=w.SceneControl()->Saves(host.Resources());
        if(mode=="read"&&phase==0&&frame-at>20){std::ifstream(out/"expected.json")>>expected;freeze=true;before=&w;request=saves->Request("load","m67","","{}",error);Check(request!=0,"normal cold load queued");phase=7;}
        if(mode!="read"&&phase==0&&frame-at>35){
            auto state=State(w);Check(state.value("spawns",0)==1&&state.value("third",false),"real logical input drives initialized prefab and same-player third-person camera");
            Check(w.Localization().Locale()=="ar","ordinary runtime localization switches to Arabic");
            Check(!w.UI().Paused(),"pause/resume releases modal state without gameplay input leak");
            SceneTransform socket;Check(w.JointPose(6,"Tip-A","world",1,socket),"actual posed skeleton stable socket resolves");
            Check(rig->asset->skeleton.names.size()>=6&&w.RuntimeDefinition(6)->animation->limbs.size()==1,"same pose resolver applies authored non-humanoid limb target");
            auto t=w.RuntimeDefinition(3)->transform;t.position={0,2,-45};w.SetRuntimeTransform(3,t);phase=1;
        }
        if(phase==1&&State(w).contains("adopted")){
            auto state=State(w);Check(Region(*stream,"east")=="active","first region activated normally");
            Check(!state.value("adopted","").empty(),"project JS adopted and qualified a referenced traveller");
            originalTraveller=stream->Resolve("east",2); // adopted member is now root-owned and no longer indexed here.
            auto t=w.RuntimeDefinition(3)->transform;t.position={0,2,-95};w.SetRuntimeTransform(3,t);at=frame;phase=2;
        }
        if(phase==2&&frame-at>90&&Region(*stream,"west")=="active"){
            Check(Region(*stream,"east")!="active","first region retires outside retention interest");
            auto t=w.RuntimeDefinition(3)->transform;t.position={0,2,-45};w.SetRuntimeTransform(3,t);at=frame;phase=3;
        }
        if(phase==3&&frame-at>40&&Region(*stream,"east")=="active"){
            EntityId traveller=0;auto state=State(w);Check((traveller=stream->ResolvePersistentKey(state.value("adopted","")))!=0&&w.RuntimeDefinition(traveller),"cross/revisit retains adopted traveller and qualified save reference");
            (void)originalTraveller;freeze=true;
            if(mode=="write"){
                expected=state;std::ofstream(out/"expected.json")<<expected.dump(2);request=saves->Request("save","m67","World Workshop","{}",error);Check(request!=0,"normal save queued after region crossing/revisit");phase=6;
            }else{
                before=&w;Check(w.SceneControl()->Reload(error),"ordinary reload queued");phase=5;
            }
        }
        if(phase==5&&before!=&w&&w.RuntimeAnimation(6)&&w.RuntimeAnimation(6)->asset){Check(State(w).value("spawns",0)==0,"scene reload removes spawned/mutable state");phase=8;}
        if(request){auto* status=saves->Status(request);if(status&&status->state=="failed"){Check(false,"save/load: "+status->error);done=true;Quit();}else if(status&&status->state=="completed"&&phase==6){Check(true,"separate-process snapshot saved");phase=8;}}
        if(phase==8){
            Check(w.Scripts()->Diagnostics().empty(),"reload/cold-load callbacks leave no stale handles");
            std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(host.GetWindow().Width(),host.GetWindow().Height(),pixels);WriteRgbPng((out/(mode+".png")).string(),host.GetWindow().Width(),host.GetWindow().Height(),pixels);
            PerformanceProfiler::Get().Export((out/(mode+"-profile.json")).string(),error);done=true;Quit();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));if(frame>3000){Check(false,"bounded normal-loop completion phase "+std::to_string(phase));done=true;Quit();}
    };
    Application app;char executable[]="judas";char* args[]={executable,project.data()};Check(app.Run(2,args,&control)==0&&done,"normal application lifecycle");std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures frames="<<frame<<'\n';return failures?1:0;
}
