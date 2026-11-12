// M60 normal rendered project frames, actual device startup, public JS controls,
// scene replacement and ordinary exporter. Listening acceptance belongs to the operator.
#include "GraphicsApi.h"
#include "Application.h"
#include "TestInput.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "ProjectExporter.h"
#include "ScreenshotWriter.h"
#include "PerformanceProfiler.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <cstdio>
#include <thread>
#include <chrono>
#include <algorithm>
namespace {int checks=0,failures=0;void Check(bool b,const char* s){++checks;failures+=!b;std::printf("%s %s\n",b?"PASS":"FAIL",s);}void Quit(){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}}
int main(int argc,char** argv){std::string project=argc>1?argv[1]:"projects/audio_lab/audio_lab.judasproj";std::filesystem::path out=argc>2?argv[2]:"build/m60-app";bool device=argc>3&&std::string(argv[3])=="device";bool doorCheck=argc>3&&std::string(argv[3]).rfind("door",0)==0;bool isolateDoor=doorCheck&&std::string(argv[3])=="door-isolate";std::filesystem::create_directories(out);std::string error;int frames=0;AudioVoiceHandle oldMusic;double cursor=0;bool pendingReload=false;std::vector<double> work;auto start=std::chrono::steady_clock::now();ApplicationControl c;c.hidden=true;c.frameSeconds=[](float){return 1.f/60;};
 c.hostReady=[&](EngineHost& h){GraphicsSetSwapInterval(0);h.GetWindow().SetTestInputMode(true);Check(h.Audio().Init(error,!device),device?"actual desktop audio backend initialized":"production PCM graph without device");};
 c.beforeFrame=[&](EngineHost& h,RuntimeWorld&,InteractivePlay&){start=std::chrono::steady_clock::now();if(frames==60||frames==61||frames==95||frames==96)QueueTestKey(h.GetWindow(),SDL_SCANCODE_ESCAPE,frames==60||frames==95);if(frames==110||frames==111)QueueTestKey(h.GetWindow(),SDL_SCANCODE_P,frames==110);if(frames==120||frames==121)QueueTestKey(h.GetWindow(),SDL_SCANCODE_B,frames==120);if(frames==130||frames==131)QueueTestKey(h.GetWindow(),SDL_SCANCODE_E,frames==130);};
 c.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){++frames;work.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());if(!device)h.Audio().AdvanceWithoutDevice(800);else std::this_thread::sleep_for(std::chrono::milliseconds(16));
  AudioVoiceHandle music;for(auto& e:w.AudioEmitters())if(e.id==20)music=e.voice;AudioVoiceSnapshot s;h.Audio().Snapshot(music,s);
  if(frames==50){Check(s.ready&&s.state==AudioPlaybackState::Playing&&s.positionSeconds>0,"long original music starts incrementally in actual project");oldMusic=music;cursor=s.positionSeconds;Check(w.Scripts()&&w.Scripts()->Diagnostics().empty(),"normal JS/UI/motor callbacks healthy");Check(h.Audio().Seek(music,45),"normal stream seek control");}
  if(frames==80){Check(s.positionSeconds>=45&&music.id==oldMusic.id,"seek preserves independent live voice and advances");auto doc=w.UI().Find("audio_lab");Check(doc&&w.UI().Document(doc)->modal,"logical pause opens authored UI controls");Check(h.Audio().SetGroup("effects",{.7f,false,true}),"effects pause while music keeps playing");}
  if(frames==100){Check(s.positionSeconds>cursor&&s.state==AudioPlaybackState::Playing,"persistent music is independent from effects pause");h.Audio().SetGroup("effects",{.7f,false,false});Check(!w.UI().Document(w.UI().Find("audio_lab"))->modal,"logical resume closes menu");}
  if(frames==160){Check(w.RuntimeDefinition(21)->audioEmitter->doppler==1,"authored source remains immutable under JS setting changes");Check(w.Scripts()->Diagnostics().empty(),"Doppler/bypass/physical-door controls leave callbacks healthy");std::vector<unsigned char> pixels;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),pixels);Check(WriteRgbPng((out/"lab.png").string(),h.GetWindow().Width(),h.GetWindow().Height(),pixels),"actual rendered listening lab screenshot");Check(w.SceneControl()->Reload(error),"normal outer-boundary scene reload");pendingReload=true;}
  if(frames==210){Check(pendingReload&&!h.Audio().Snapshot(oldMusic,s),"reload invalidates old stream voice");Check(w.Scripts()&&w.Scripts()->Diagnostics().empty()&&h.Audio().Diagnostics().pendingRetirements==0,"replacement leaves no ghost callback or decoder retirement");}
  if(frames>=240){std::sort(work.begin()+30,work.end());auto begin=work.begin()+30;auto n=work.size()-30;std::printf("APPLICATION frame_work_median_ms=%.4f p95_ms=%.4f max_ms=%.4f device=%d voices=%zu streams=%zu PCM=%zu underruns=%llu rays=%llu detach_max_ms=%.4f\n",begin[n/2],begin[n*95/100],work.back(),device,h.Audio().VoiceCount(),h.Audio().Diagnostics().streams,h.Audio().Diagnostics().streamBytes,(unsigned long long)h.Audio().Diagnostics().underruns,(unsigned long long)w.AudioQueryCount(),h.Audio().Diagnostics().maximumDetachMilliseconds);Quit();}
  if(!device)std::this_thread::sleep_for(std::chrono::milliseconds(2));
 };
 c.beforeShutdown=[&](EngineHost&,RuntimeWorld& w,InteractivePlay&){Check(w.Scripts()&&w.Scripts()->Diagnostics().empty(),"runtime script destroy ordering has a live audio service");};
 // Human follow-up: prove actual motion, not merely a healthy toggle callback.
 if(doorCheck){
  c.beforeFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){
   if(frames==0&&isolateDoor)for(auto id:{1,30,31,32,42})w.Physics().SetBodyEnabled(w.RuntimeBody(id),false);
   if(frames==30||frames==31||frames==210||frames==211||frames==390||frames==391||frames==570||frames==571)QueueTestKey(h.GetWindow(),SDL_SCANCODE_E,frames==30||frames==210||frames==390||frames==570);
  };
  c.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){
   ++frames;h.Audio().AdvanceWithoutDevice(800);
   JointState state;bool valid=w.Physics().GetJoint(w.RuntimeJoint(34),state);
   if(frames==30||frames==90||frames==150||frames==210||frames==390||frames==570||frames==750){
    auto pose=w.Physics().GetTransform(w.RuntimeBody(33));
    std::printf("DOOR frame=%d valid=%d angle=%.6f motor=%d speed=%.6f impulse=%.6f position=%.4f,%.4f,%.4f contacts=%zu\n",frames,valid,state.coordinate,state.settings.motor,state.settings.speed,state.motorImpulse,pose.position.x,pose.position.y,pose.position.z,w.Physics().LastStepContactCount());
    for(const auto& e:w.Physics().LastStepTouchEvents())if(e.a.id==w.RuntimeBody(33).id||e.b.id==w.RuntimeBody(33).id)std::printf("DOOR contact other=%llu phase=%d impulse=%.6f\n",(unsigned long long)w.EntityIdOfBody(e.a.id==w.RuntimeBody(33).id?e.b:e.a),int(e.phase),e.normalImpulse);
   }
   if(frames==30)Check(valid&&std::abs(state.coordinate)<.05f,"physical door starts closed");
   if(frames==210||frames==570){Check(valid&&state.coordinate<-1.3f,"E opens real hinged door to intended angle");Check(w.Scripts()->Diagnostics().empty(),"door input has no script failure");}
   if(frames==390||frames==750)Check(valid&&std::abs(state.coordinate)<.05f,"second E physically closes door");
   if(!isolateDoor&&(frames==30||frames==210||frames==390||frames==570||frames==750)){
    auto ray=w.Physics().Raycast({-8,1.5,3},{0,0,-1},8);
    bool open=frames==210||frames==570;
    Check(open?!ray.hit:(ray.hit&&ray.body.id==w.RuntimeBody(33).id),open?"open doorway clears real obstruction ray":"closed door blocks real obstruction ray");
   }
   if(frames==750)Quit();
   std::this_thread::sleep_for(std::chrono::milliseconds(2));
  };
 }
 Application app;char name[]="judas";char* args[]={name,project.data()};Check(app.Run(2,args,&c)==0,"normal standalone exits and drains safely");
 if(!device&&!doorCheck){Project p;ProjectExportOptions options;ProjectExportResult result;Check(p.Load(project,error),"export ordinary registered project");options.destination=(std::filesystem::path(".cache/m60-exports")/out.filename()).string();options.runtimeExecutable=std::filesystem::absolute("build/judas").string();Check(ExportProject(p,options,result,error),"ordinary export includes streamed music/environment assets");if(!error.empty())std::puts(error.c_str());std::printf("EXPORT assets=%zu bytes=%llu seconds=%.3f package=%s\n",result.assetCount,(unsigned long long)result.bytes,result.seconds,result.packageDirectory.c_str());}
 std::printf("SUMMARY checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
