#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "Project.h"
#include "SceneSession.h"
#include "SceneSerialization.h"
#include "ScriptSystem.h"
#include "ScreenshotWriter.h"
#include "WorldPersistence.h"
#include "SaveStorage.h"
#include "PerformanceProfiler.h"
#include <glad/gl.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <fstream>
#include "ToolProcess.h"
#include <numeric>
namespace {
int checks=0,failures=0;
void Check(bool v,const std::string& s){++checks;failures+=!v;std::cout<<(v?"PASS ":"FAIL ")<<s<<'\n';}
}
int main(int argc,char** argv){
    if(argc==3&&std::string(argv[1])=="--cold-cycle"){
        const std::filesystem::path directory=argv[2];
        if(std::filesystem::exists(directory)){std::cerr<<"cold cycle requires fresh output\n";return 1;}
        std::string log,error;const auto executable=std::filesystem::absolute(argv[0]);
        if(!CaptureToolOutput(executable,{"--cold-write",directory.string()},log,error,std::chrono::seconds(30),65536)){std::cerr<<log<<error;return 1;}
        std::cout<<log;auto read=[&](){std::ifstream f(directory/"water.save",std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
        const auto before=read();if(before.empty())return 1;
        if(!CaptureToolOutput(executable,{"--cold-read",directory.string()},log,error,std::chrono::seconds(30),65536)){std::cerr<<log<<error;return 1;}
        std::cout<<log;if(read()!=before){std::cerr<<"cold read changed save bytes\n";return 1;}
        std::cout<<"PASS actual fresh-process water continuation and unchanged source save\n";return 0;
    }

    std::string error;Project project;Check(project.Load("projects/basic_water_lab/basic_water_lab.judasproj",error),"load ordinary basic-water project: "+error);
    EngineHost host;if(!host.Init("Basic water workflow",960,540,false,error)){std::cerr<<error<<'\n';return 1;}
    std::cout<<"GL "<<glGetString(GL_RENDERER)<<" | "<<glGetString(GL_VERSION)<<'\n';
    host.OpenProjectAssets(project.RootDir(),project.AssetsDir());host.Resources().SetBlockingMode(true);
    auto& window=host.GetWindow();window.SetTestInputMode(true);Check(window.Input().SetMap(project.Settings().input,error),"project logical input map");
    const std::filesystem::path out=argc>2?argv[2]:"docs/evidence/prealpha-project-settings-water/application";std::filesystem::create_directories(out);
    const bool writing=argc>1&&std::string(argv[1])=="--cold-write",reading=argc>1&&std::string(argv[1])=="--cold-read";
    const bool profiling=argc>1&&std::string(argv[1])=="--profile";
    auto& profiler=PerformanceProfiler::Get();if(profiling)profiler.Enable(true);
    float finalWet=0,finalDry=0;
    for(unsigned mode=0;mode<(writing||reading?1u:3u);++mode){
        auto settings=project.Settings();settings.waterSwimming=mode!=1;settings.basicWater=mode!=2;
        auto owner=std::make_unique<RuntimeWorld>();auto& world=*owner;world.ConfigureProject(settings);
        world.SetSceneControl(std::make_shared<SceneSession>(project,project.Settings().startupScene));
        Scene scene;GameSnapshot snapshot;bool recovered=false;
        if(reading){SaveStorage storage(out.string());Check(storage.Read("water",snapshot,recovered,error)&&WorldPersistence::SceneFrom(snapshot.participants,scene,error),"fresh process reads preserved basic-water snapshot: "+error);}
        else Check(LoadSceneFromFile(project.StartupScenePath(),scene,error),"load authored swimming scene: "+error);
        Check(world.Build(scene,&host.Resources(),error,&settings.classification,&settings.navigation,reading),"ordinary world construction: "+error);
        host.Resources().WaitForAll();
        if(reading)Check(WorldPersistence::Prepare(world,error)&&WorldPersistence::Restore(world,snapshot.participants,error),"cold-process restore before publication: "+error);
        EntityId swimmer=0;for(const auto& o:scene.Objects())if(o.characterMotor)swimmer=o.id;
        Check(swimmer!=0&&!world.HasFluid()&&world.Liquids().States().empty(),"default scene has a real motor and no advanced fluid owners");
        InteractivePlay play;Check(play.Begin(world,WorldCoordinates(world.Settings().worldOrigin),error),"ordinary InteractivePlay begins: "+error);
        if(failures)return 1;
        if(reading){auto* motor=world.RuntimeCharacter(swimmer);Check(motor&&std::abs(motor->position.y+1)<.001f&&glm::length(motor->velocity)<.001f,"fresh process restores the actual saved motor pose and velocity");}
        if(profiling)profiler.Clear();
        std::vector<double> steps,frames;size_t pauseStart=0;bool healthy=true;unsigned glErrors=0;
        for(unsigned frame=0;frame<240;++frame){
            window.Input().BeginFrame();host.PumpResources();
            if(frame==20&&!reading){EntityPhysicalState state;world.GetEntityState(swimmer,state);state.position={0,-1,0};state.linearVelocity={0,0,0};Check(world.SetEntityState(swimmer,state),"place real motor in authored pool without injecting any water sample");}
            if(frame==120||frame==131)window.Input().SetPhysical("key:Escape",1);
            if(frame==121||frame==132)window.Input().SetPhysical("key:Escape",0);
            if(profiling)profiler.BeginFrame("Basic water workflow");
            const auto start=std::chrono::steady_clock::now();play.Frame(window,host.GetRenderer(),1.f/60,true,true);
            frames.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            if(play.LastFixedStepsThisFrame())steps.push_back(play.LastFixedStepMilliseconds());
            if(frame==120){pauseStart=play.FixedStepsSinceReset();Check(play.IsPaused(),"logical pause action opens the authored menu");}
            if(frame==129)Check(play.IsPaused()&&play.FixedStepsSinceReset()==pauseStart,"pause stops gameplay and physics without leaking input");
            if(frame==133)Check(!play.IsPaused(),"logical pause resumes cleanly");
            if(frame==12||frame==30){std::vector<unsigned char> rgb;host.GetRenderer().CaptureFrame(960,540,rgb);Check(WriteRgbPng((out/(std::string(mode?"dry":"basic")+(frame==12?"-overview.png":"-underwater.png"))).string(),960,540,rgb),"capture actual water renderer and HUD");}
            if(world.Scripts()&&!world.Scripts()->Diagnostics().empty()){for(const auto& d:world.Scripts()->Diagnostics())std::cerr<<d.callback<<": "<<d.message<<'\n';healthy=false;break;}
            glErrors+=glGetError()!=GL_NO_ERROR;
            if(profiling)profiler.EndFrame();
        }
        Check(glErrors==0,"all 240 water render frames have no GL error");
        Check(healthy,"ordinary project scripts remain healthy");
        auto* motor=world.RuntimeCharacter(swimmer);if(!motor)return 1;
        if(mode==0)finalWet=motor->position.y;else if(mode==1)finalDry=motor->position.y;
        Check(world.Liquids().States().empty()&&world.Liquids().StepTimes().total==0&&!world.HasFluid(),"rendering/swimming/pausing never activates an advanced solver");
        std::cout<<"ENDPOINT mode="<<mode<<" y="<<motor->position.y<<" vy="<<motor->velocity.y<<"\n";
        if(!reading){if(mode==0){
            // For the documented centre-line quadrature, 950/1000 immersion
            // puts the mean centre at -0.81 m, with 1.8/17 m resolution.
            const float expected=-.9f*(2*.95f-1),resolution=1.8f/17;
            Check(std::abs(motor->position.y-expected)<resolution+.02f&&motor->position.y>-.95f,"public basic-water sample produces the density-consistent floating endpoint through JS");}else Check(motor->position.y< -2.f,"negative control: disabling swimming makes the same motor sink to the physical bottom");}
        auto report=[&](std::vector<double> values,const char* label){std::sort(values.begin(),values.end());if(values.empty())return;std::cout<<"MEASURE "<<(mode==0?"basic-water":mode==1?"swimming-off":"water-off")<<' '<<label<<" n="<<values.size()<<" mean="<<std::accumulate(values.begin(),values.end(),0.)/values.size()<<" p95="<<values[std::min(values.size()-1,size_t(.95*values.size()))]<<" worst="<<values.back()<<" ms\n";};report(steps,"fixed-step");report(frames,"render-and-frame");
        if(writing){EntityPhysicalState state;world.GetEntityState(swimmer,state);state.position={0,-1,0};state.linearVelocity={0,0,0};world.SetEntityState(swimmer,state);
            snapshot.project="basic-water-test";snapshot.scene=project.Settings().startupScene;snapshot.content=world.BaselineFingerprint();snapshot.displayName="Basic water cold proof";
            Check(WorldPersistence::Capture(world,snapshot.participants,error),"capture real authored/script/motor state for a new process: "+error);SaveStorage storage(out.string());Check(storage.Write("water",snapshot,error),"write native save slot atomically: "+error);}
        if(profiling){glFinish();profiler.BeginFrame("GPU result drain");host.GetRenderer().PollProfileGPU();profiler.EndFrame();
            Check(profiler.Export((out/(std::string("profile-")+std::to_string(mode)+".json")).string(),error),"export bounded CPU/GPU water evidence");}
        play.End();owner.reset();
    }
    if(!writing&&!reading)Check(finalWet-finalDry>.9f,"positive and negative endpoints differ by more than the motor half-span (radius + halfHeight)");
    std::cout<<"BasicWaterApplicationTests: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
