#include "editor/ScriptTools.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "Project.h"
#include "ScreenshotWriter.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL2/SDL.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdio>
static int failures=0,checks=0;
static void Check(bool ok,const char* label){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
static void NewFrame(){ImGui_ImplOpenGL3_NewFrame();ImGui_ImplSDL2_NewFrame();ImGui::NewFrame();}
static void Key(SDL_Window* w,SDL_Scancode code,bool down){SDL_Event e{};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.windowID=SDL_GetWindowID(w);e.key.keysym.scancode=code;e.key.keysym.sym=SDL_GetKeyFromScancode(code);SDL_PushEvent(&e);}
int main(){
    EngineHost host;std::string error;Check(host.Init("M94 editor tools functional test",960,800,false,error),"real SDL/OpenGL editor host");
    auto& window=host.GetWindow();ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplSDL2_InitForOpenGL(window.NativeWindow(),window.NativeGLContext());ImGui_ImplOpenGL3_Init("#version 330 core");window.SetEventHook([](const SDL_Event& e){ImGui_ImplSDL2_ProcessEvent(&e);});
    Project project;Check(project.Load("projects/developer_lab/developer_lab.judasproj",error),"ordinary editable developer project");host.OpenProjectAssets(project.RootDir(),project.AssetsDir());
    Scene scene;Check(LoadSceneFromFile(project.StartupScenePath(),scene,error),"normal scene loading");auto& probe=scene.CreateObject("editor pause physical probe");auto probeId=probe.id;probe.body=SceneBodyComponent{};probe.body->motion=SceneBodyMotion::Dynamic;probe.body->mass=2;probe.transform.position={0,6,0};RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(scene,&host.Resources(),error),"normal world construction");
    auto probeBody=world.RuntimeBody(probeId);world.Physics().SetLinearVelocity(probeBody,{1,-3,0});
    auto& vm=world.PrepareScriptTools();ScriptTools tools;tools.visible=true;tools.enabled=true;tools.points={{"Assets/scripts/worker.js",10,true}};
    std::vector<ScriptDebugStop> stops;
    vm.SetDebugger([&](const ScriptDebugStop& stop){
        const auto physicalBefore=world.Physics().GetTransform(probeBody);const auto velocityBefore=world.Physics().GetLinearVelocity(probeBody);
        stops.push_back(stop);const auto states=vm.Capture();auto before=states;
        std::thread key([&]{SDL_Delay(90);Key(window.NativeWindow(),SDL_SCANCODE_F10,true);SDL_Delay(40);Key(window.NativeWindow(),SDL_SCANCODE_F10,false);});
        auto action=tools.Pause(stop,host);key.join();
        if(stops.size()==1){std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(window.Width(),window.Height(),pixels);Check(WriteRgbPng(".cache/m94/editor-stopped.png",window.Width(),window.Height(),pixels),"stopped editor framebuffer captured");}
        Check(world.Physics().GetTransform(probeBody).position==physicalBefore.position&&world.Physics().GetLinearVelocity(probeBody)==velocityBefore,"moving physical body remains unchanged throughout debugger UI pause");
        Check(action==ScriptDebugAction::Over,"actual SDL F10 resumes nested debugger UI with step-over");
        auto after=vm.Capture();bool same=after.size()==before.size();for(size_t i=0;same&&i<after.size();++i)same=after[i].entity==before[i].entity&&after[i].json==before[i].json;
        Check(same,"inspection UI leaves all declared script state unchanged while stopped");
        return stops.size()<2?action:ScriptDebugAction::Continue;
    });vm.SetBreakpoints(tools.points);
    // A separate bounded watchdog only closes this test window on failure.
    bool finished=false;std::mutex mutex;
    std::thread watchdog([&]{for(int i=0;i<200;++i){SDL_Delay(50);std::lock_guard<std::mutex> lock(mutex);if(finished)return;}SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);});
    NewFrame();world.UpdateScripts(&window.Input(),.016f);ImGui::EndFrame();
    Check(stops.size()==2&&stops[0].frames[0].line==10&&stops[1].frames[0].line==11,"real editor pause loop steps an actual project callback");
    Check(tools.clockReset&&!tools.stopRequested,"debugger requests clock reset and preserves Play");
    auto prior=world.Physics().GetTransform(probeBody).position;world.Physics().Step(1.f/60);Check(world.Physics().GetTransform(probeBody).position!=prior,"physical pause probe actually moves when authoritative physics is advanced");
    vm.SetDebugger({});NewFrame();tools.Panel(&world,host.Assets());ImGui::Render();ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());window.SwapBuffers();
    Check(vm.Diagnostics().empty(),"scene and developer panel remain fault-free after resuming");
    tools.reloadRequested=true;tools.preserve=true;tools.ApplyReload(world);world.UpdateScripts(&window.Input(),.016f);
    Check(vm.HasReloaded()&&vm.Diagnostics().empty(),"editor reload action replaces VM through the same tested boundary");
    {std::lock_guard<std::mutex> lock(mutex);finished=true;}watchdog.join();
    auto owned=std::filesystem::absolute(".cache/m94/editor-source-edit");std::filesystem::remove_all(owned);std::filesystem::create_directories(owned/"Assets");
    auto source=owned/"Assets/test.js";{std::ofstream(source)<<"export const oldValue=1;";}
    AssetDatabase editable;editable.Scan(owned.string(),(owned/"Assets").string());AssetRecord record;Check(editable.Track(source.string(),record,error),"tracked source edit fixture");
    tools.file="Assets/test.js";tools.sourcePath=source.string();tools.loadedBytes="export const oldValue=1;";std::string changed="export const newValue=2;";tools.edit.assign(changed.begin(),changed.end());tools.edit.push_back(0);
    Check(tools.SaveSourceEdits(editable),"editor saves source through owned staging and atomic replacement");
    {std::ofstream(source)<<"// externally edited\n";}
    Check(!tools.SaveSourceEdits(editable)&&tools.status.find("externally")!=std::string::npos,"editor rejects overwriting externally changed source");
    std::ifstream result(source);std::string bytes{std::istreambuf_iterator<char>(result),{}};Check(bytes=="// externally edited\n","external source bytes preserved on rejected edit");
    world.Destroy();window.SetEventHook(nullptr);ImGui_ImplOpenGL3_Shutdown();ImGui_ImplSDL2_Shutdown();ImGui::DestroyContext();host.Shutdown();
    std::printf("M94_EDITOR %d checks %d failures\n",checks,failures);return failures?1:0;
}
