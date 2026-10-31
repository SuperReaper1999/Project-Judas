#include <filesystem>
// Bounded cause tracing: unchanged StepPlayedWorld, observed body loads/touches.
#include "FluidDiagnosticForceGate.h"
#include "EngineHost.h"
#include "GameSession.h"
#include "ProductionFluidCoupling.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

void Vec(std::ostream& out,glm::vec3 v){out<<'['<<v.x<<','<<v.y<<','<<v.z<<']';}
int main(int argc,char** argv) try {
    if(argc!=4&&argc!=5)throw std::runtime_error("trace project-or-scene output.jsonl steps [lake-height-offset]");
    Project project;Scene scene;std::string error;
    const bool explicitScene=std::filesystem::path(argv[1]).extension()==".judas";
    const auto projectPath=explicitScene?Project::FindProjectFileFor(argv[1]):std::string(argv[1]);
    if(!project.Load(projectPath,error)||!LoadSceneFromFile(explicitScene?std::string(argv[1]):project.StartupScenePath(),scene,error))throw std::runtime_error(error);
    if(argc==5){auto* lake=scene.Find(8);if(!lake||!lake->fluidVolume)throw std::runtime_error("fixture lake missing");lake->transform.position.y+=std::stof(argv[4]);}
    EngineHost host;if(!host.Init("Fluid transfer trace",640,360,false,error))throw std::runtime_error(error);
    host.OpenProjectAssets(project.RootDir(),project.AssetsDir());host.Resources().SetBlockingMode(true);
    host.GetWindow().SetInputClaimed(true,true);
    std::ofstream out(argv[2]);out<<std::setprecision(17);
    for(bool suppress:{false,true}) {
        RuntimeWorld world;world.ConfigureProject(project.Settings());
        if(!world.Build(scene,&host.Resources(),error))throw std::runtime_error(error);
        GameSession session;if(!session.Begin(world,error))throw std::runtime_error(error);
        FluidLaunchDiagnostic::suppress=suppress;
        for(int step=1;step<=std::stoi(argv[3]);++step) {
            host.GetWindow().Input().BeginFixedStep();
            StepPlayedWorld(session,host.GetWindow(),1.f/60);
            out<<"{\"step\":"<<step<<",\"water_forces_suppressed\":"<<(suppress?"true":"false")<<",\"bodies\":[";
            bool first=true;
            for(std::size_t i=0;i<world.DynamicBodies().size();++i) {
                const auto h=world.DynamicBodies()[i].Handle();
                if(!world.Physics().IsDynamicBody(h))continue;
                if(!first)out<<',';
                first=false;
                out<<"{\"handle\":"<<h.id<<",\"name\":"<<std::quoted(world.DynamicVisuals()[i].name)<<",\"velocity\":";
                Vec(out,world.Physics().GetLinearVelocity(h));out<<'}';
            }
            out<<"],\"loads\":[";first=true;
            for(const auto& load:world.FluidCoupling().Measurements().bodies) {
                if(!first)out<<',';
                first=false;
                out<<"{\"handle\":"<<load.body.id<<",\"fraction\":"<<load.fraction<<",\"buoyancy\":";
                Vec(out,load.buoyancyForce);out<<",\"drag\":";Vec(out,load.dragForce);out<<'}';
            }
            out<<"],\"touches\":[";first=true;
            for(const auto& event:world.Physics().LastStepTouchEvents()) {
                if(!first)out<<',';
                first=false;
                out<<"{\"a\":"<<event.a.id<<",\"b\":"<<event.b.id<<",\"phase\":"<<int(event.phase)<<",\"impulse_available\":"<<(event.impulseAvailable?"true":"false")<<",\"normal_impulse\":"<<event.normalImpulse<<'}';
            }
            out<<"]}\n";
        }
        session.End();
    }
    if(!out)throw std::runtime_error("trace write failure");
    return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
