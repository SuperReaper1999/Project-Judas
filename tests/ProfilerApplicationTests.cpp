#include "Application.h"
#include "PerformanceProfiler.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "Project.h"
#include "SceneFingerprint.h"
#include "ScriptSystem.h"
#include "editor/ProfilerView.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <cstdio>
namespace fs=std::filesystem;
int main(int argc,char** argv){
 fs::path out=argc>2&&std::string(argv[1])=="--output"?argv[2]:argc>1?argv[1]:"build/m56-application";fs::create_directories(out);
 unsigned checks=0,failures=0;auto check=[&](bool v,const char* m){++checks;failures+=!v;std::printf("%s %s\n",v?"PASS":"FAIL",m);};
 std::ofstream metrics(out/"paired.csv");metrics<<"project,trial,mode,frame,interval_ms,active_ms,fixed_ms,ui_ms,steps\n";
 auto& profiler=PerformanceProfiler::Get();
 for(const char* projectName:{"shooter_game","liquid_surface_demo"}){
  const bool liquid=std::string(projectName)=="liquid_surface_demo";
  std::string baseline;unsigned trial=0;double acquired=0;
  // Interleaved equal deterministic inputs, same cameras and simulation cadence.
  for(int mode:{0,1,2,1,0,2}){
   ++trial;profiler.Freeze(false);profiler.Clear();setenv("JUDAS_PROFILE",mode?"1":"0",1);unsetenv("JUDAS_PROFILE_OUTPUT");
   Project project;std::string error;std::string path="projects/"+std::string(projectName)+"/"+projectName+".judasproj";check(project.Load(path,error),"ordinary integration project");
   unsigned frame=0,stepCount=0;std::uint64_t start=0,previous=0;std::string state;double bucketVolume=0;bool cursorReleased=true,intentUnchanged=true;
   std::vector<double> times,steps,uis;ApplicationControl control;control.hidden=true;control.frameSeconds=[](float){return 1.f/60;};
   control.hostReady=[&](EngineHost& host){
    SDL_GL_SetSwapInterval(0);IMGUI_CHECKVERSION();ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::StyleColorsDark();ImGui_ImplSDL2_InitForOpenGL(host.GetWindow().NativeWindow(),host.GetWindow().NativeGLContext());ImGui_ImplOpenGL3_Init("#version 330");
    host.OpenProjectAssets(project.RootDir(),project.AssetsDir());
    for(const auto& [id,record]:host.Assets().Records())switch(record.type){case AssetType::Mesh:host.Resources().RequestMesh(id);break;case AssetType::Texture:host.Resources().RequestTexture(id);break;case AssetType::Navigation:host.Resources().RequestNavigation(id);break;case AssetType::Liquid:host.Resources().RequestLiquid(id);break;case AssetType::Audio:host.Resources().RequestAudio(id);break;default:break;}
    host.Resources().WaitForAll();
   };
   control.worldReady=[&](EngineHost& host,RuntimeWorld&,InteractivePlay&){host.GetWindow().SetTestInputMode(true);host.Resources().WaitForAll();};
   control.beforeFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
    play.SetPointerCaptureAllowed(mode!=2);
    // Desktop mouse motion must not reach deterministic trials (issue #4); queue some through real SDL.
    if(frame<5)for(int i=0;i<4;++i){SDL_Event motion{};motion.type=SDL_MOUSEMOTION;motion.motion.xrel=3;motion.motion.yrel=2;SDL_PushEvent(&motion);}
    const auto now=PerformanceProfiler::Now();double interval=previous?double(now-previous)/1e6:0;previous=now;start=now;
    if(frame>30)times.push_back(interval);
    auto& input=host.GetWindow().Input();input.SetPhysical("key:W",!liquid&&frame<45?1:0);
    if(liquid&&frame>=35&&frame<75){auto pose=world.RuntimeDefinition(200)->transform;pose.position={0,1.2f,-12};pose.rotation={1,0,0,0};world.SetRuntimeTransform(200,pose);auto body=world.RuntimeBody(200);world.Physics().SetLinearVelocity(body,{});world.Physics().SetAngularVelocity(body,{});}
    if(liquid&&frame>=75&&frame<90){auto pose=world.RuntimeDefinition(200)->transform;pose.position={0,4,0};pose.rotation={1,0,0,0};world.SetRuntimeTransform(200,pose);auto body=world.RuntimeBody(200);world.Physics().SetLinearVelocity(body,{});world.Physics().SetAngularVelocity(body,{});}
    if(liquid&&frame>=90){auto pose=world.RuntimeDefinition(200)->transform;pose.position={1.5f,3.15f,-2.7f};pose.rotation=glm::angleAxis(-glm::half_pi<float>(),glm::vec3(1,0,0));world.SetRuntimeTransform(200,pose);auto body=world.RuntimeBody(200);world.Physics().SetLinearVelocity(body,{});world.Physics().SetAngularVelocity(body,{});}
   };
   control.afterFrame=[&](EngineHost& host,RuntimeWorld& world,InteractivePlay& play){
    if(mode==2){cursorReleased&=!host.GetWindow().IsMouseCaptured();intentUnchanged&=world.pointerCapture;}
    double active=double(PerformanceProfiler::Now()-start)/1e6;double ui=0;
    if(mode==2){auto a=PerformanceProfiler::Now();JUDAS_PROFILE_SCOPE("Profiler UI");RendererProfileScope gpu(host.GetRenderer(),"Profiler UI");ImGui_ImplOpenGL3_NewFrame();ImGui_ImplSDL2_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowSize(ImVec2(850,650));ImGui::Begin("Profiler");DrawIntegratedProfilerView();ImGui::End();ImGui::Render();ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());ui=double(PerformanceProfiler::Now()-a)/1e6;}
    ++frame;stepCount+=play.LastFixedStepsThisFrame();if(frame>31){steps.push_back(play.LastFixedStepMilliseconds());uis.push_back(ui);metrics<<projectName<<','<<trial<<','<<mode<<','<<frame<<','<<(times.empty()?0:times.back())<<','<<active<<','<<play.LastFixedStepMilliseconds()<<','<<ui<<','<<play.LastFixedStepsThisFrame()<<'\n';}
    if(liquid){auto h=world.Liquids().Handle(200);if(auto* s=world.Liquids().Get(h))bucketVolume=std::max(bucketVolume,s->volume);}
    if(frame==120){
     std::ostringstream data;data<<std::setprecision(17);
     for(const auto& o:world.ScriptObjects()){EntityPhysicalState s;if(!world.GetEntityState(o.id,s))continue;data<<o.id<<' '<<s.position.x<<' '<<s.position.y<<' '<<s.position.z<<' '<<s.rotation.w<<' '<<s.rotation.x<<' '<<s.rotation.y<<' '<<s.rotation.z<<' '<<s.linearVelocity.x<<' '<<s.linearVelocity.y<<' '<<s.linearVelocity.z<<' '<<s.angularVelocity.x<<' '<<s.angularVelocity.y<<' '<<s.angularVelocity.z<<'\n';}
     if(auto* scripts=world.Scripts())for(auto s:scripts->Capture())data<<s.entity<<' '<<s.slot<<' '<<s.json<<'\n';
     for(auto& [id,s]:world.Liquids().States()){data<<id<<' '<<s.volume<<'\n';if(s.dynamicSurface)for(auto& c:s.dynamicSurface->cells)data<<c.volume<<' '<<c.q<<' '<<c.velocity.x<<' '<<c.velocity.y<<' '<<c.velocity.z<<'\n';}
     state=SceneFingerprintSha256(data.str());std::ofstream(out/(std::string(projectName)+"-"+std::to_string(trial)+"-state.txt"))<<data.str();
     check(!world.Scripts()||world.Scripts()->Diagnostics().empty(),"profiled project scripts remain fault-free");check(world.Liquids().Errors().empty(),"profiled liquid path retains accepted solve behaviour");world.UI().RequestQuit();
    }
   };
   control.beforeShutdown=[&](EngineHost&,RuntimeWorld&,InteractivePlay&){ImGui_ImplOpenGL3_Shutdown();ImGui_ImplSDL2_Shutdown();ImGui::DestroyContext();};
   char* arguments[]={const_cast<char*>("judas"),path.data()};Application app;check(app.Run(2,arguments,&control)==0,"real outer application lifecycle");
   check(frame==120&&stepCount==120,"same actual fixed-step input count for every mode");
   if(mode==2){check(cursorReleased,"live profiler inspection releases cursor while fixed simulation continues");check(intentUnchanged,"editor cursor routing preserves script pointer-capture intent");}
   if(baseline.empty())baseline=state;else check(state==baseline,"capture/UI do not alter deterministic physics and script state");
   auto print=[&](const char* label,std::vector<double> values){std::sort(values.begin(),values.end());std::printf("M56 %s trial %u mode %d %s median %.5f p95 %.5f max %.5f ms\n",projectName,trial,mode,label,values[values.size()/2],values[size_t((values.size()-1)*.95)],values.back());};print("outer interval incl collection/swap",times);print("fixed",steps);print("profiler UI",uis);
   if(mode){auto history=profiler.History();bool fixed=false,gpu=false,storage=false,water=false;for(const auto& f:history){fixed|=!f.fixed.empty();for(const auto& g:f.gpu)gpu|=!g.pending;for(const auto& s:f.scopes){storage|=s.name=="Solid excluded storage rebuild";water|=s.name=="Water optical paths";}}
    check(fixed&&gpu,"outer frames contain fixed identities and completed delayed GPU results");if(liquid)check(storage&&water,"moving storage is distinguishable from water presentation");
    bool submissions=false,boundedSubmissions=true;for(const auto& f:history)for(const auto& c:f.counters)if(c.name=="Submitted draws all passes"){submissions=true;boundedSubmissions&=c.value>0&&c.value<500&&c.mode==ProfileCounterMode::Sum;}check(submissions&&boundedSubmissions,"submission counters describe each frame rather than cumulative standalone totals");
    if(trial==2||trial==3){check(profiler.Export((out/(std::string(projectName)+(mode==2?"-UI.json":"-capture.json"))).string(),error),"bounded structured integration capture");}
    auto d=profiler.Diagnostics();std::printf("M56 capacity %zu bytes drops %llu truncated %llu GPU dropped %llu\n",profiler.ReservedBytes(),(unsigned long long)d.droppedEvents,(unsigned long long)d.truncatedFrames,(unsigned long long)d.gpuDropped);
   }
   acquired=std::max(acquired,bucketVolume);
  }
  if(liquid)check(acquired>.002,"captured workload actually acquires liquid through physical bucket opening");
 }
 std::printf("M56 application %u checks %u failures\n",checks,failures);return failures?1:0;
}
