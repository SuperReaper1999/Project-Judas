// Compile the identical source against each revision's own headers/libraries.
// Fixed frame time matches simulated work; it is not a real-time FPS claim.
#include "Application.h"
#include "BuildIdentity.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "ProductionFluidCoupling.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include "SceneFingerprint.h"
#include "../../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;
using Json=nlohmann::json;
struct Frame {
    unsigned index=0,steps=0,particles=0,draws=0,triangles=0;
    std::uint64_t profile=0;
    double cpu=0,surface=0,render=0;
    std::string state;
};
struct Step {unsigned index;double fixed,particles,coupling,rigid;bool executed;};
std::string State(const RuntimeWorld &world) {
    std::string bytes;
    auto number=[&](float x){bytes.append(reinterpret_cast<const char*>(&x),sizeof(x));};
    auto vec=[&](glm::vec3 v){number(v.x);number(v.y);number(v.z);};
    for(const auto &p:world.Fluid().Particles()) {
        vec(p.position);vec(p.previousPosition);vec(p.velocity);number(p.mass);vec(p.acceleration);
    }
    for(auto body:world.Physics().AliveBodies()) {
        const auto pose=world.Physics().GetTransform(body);vec(pose.position);
        number(pose.rotation.x);number(pose.rotation.y);number(pose.rotation.z);number(pose.rotation.w);
        vec(world.Physics().GetLinearVelocity(body));vec(world.Physics().GetAngularVelocity(body));
    }
    return SceneFingerprintSha256(bytes);
}
}
int main(int argc,char **argv)try {
    if(HandleBuildIdentityArguments(argc,argv,"fluid_render_probe"))return 0;
    if(argc!=4)throw std::runtime_error("FluidRenderProbe PROJECT NEW_OUTPUT FRAMES (fixed 1/60 s; 1280x720)");
    const std::filesystem::path output=std::filesystem::absolute(argv[2]);
    const unsigned count=unsigned(std::stoul(argv[3]));
    if(count<12||count>3000)throw std::runtime_error("frames must be 12..3000");
    if(!std::filesystem::create_directories(output))throw std::runtime_error("output must be a new directory");
    std::vector<Frame> frames;std::vector<Step> steps;frames.reserve(count);steps.reserve(count*2);
    std::map<std::uint64_t,ProfileGPURecord> gpu;
    Json environment;
    Clock::time_point started;
    InteractivePlay::FixedStepObserver observer;
    auto &profiler=PerformanceProfiler::Get();profiler.Enable(true);
    auto collect=[&](Renderer &r) {
        r.PollProfileGPU();
        for(const auto &frame:profiler.History())
            for(const auto &record:frame.gpu)if(!record.pending)gpu[record.token]=record;
    };
    ApplicationControl control;control.hidden=true;
    control.frameSeconds=[](float){return 1.f/60;};
    control.hostReady=[&](EngineHost &host) {
        auto &window=host.GetWindow();std::string error;
        if(!window.Resize(1280,720,error))throw std::runtime_error(error);
        SDL_GL_SetSwapInterval(0);host.Audio().Init(error,true);
        environment={{"vendor",reinterpret_cast<const char*>(glGetString(GL_VENDOR))},
                     {"renderer",reinterpret_cast<const char*>(glGetString(GL_RENDERER))},
                     {"GL",reinterpret_cast<const char*>(glGetString(GL_VERSION))},
                     {"width",window.Width()},{"height",window.Height()},
                     {"frame_seconds",1./60},{"simulation_source","unchanged ordinary Application/InteractivePlay"}};
        if(std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER))).find("GTX 970")==std::string::npos)
            throw std::runtime_error("this matched run requires the actual GTX 970, not software GL");
#ifdef JUDAS_FLUID_PROBE_V2
        environment["modern_tier"]=host.GetRenderer().Capabilities().modernTier;
#endif
    };
    control.worldReady=[&](EngineHost&,RuntimeWorld &world,InteractivePlay &play) {
        if(!world.HasFluid()||world.Fluid().Particles().empty())throw std::runtime_error("fixture has no enabled advanced fluid");
        environment["initial_particles"]=world.Fluid().Particles().size();
        environment["bodies"]=world.Physics().AliveBodies().size();
        environment["fluid_hz"]=world.Settings().fluidUpdateRateHz;
        environment["linear_rendering"]=world.Settings().linearRendering;
        play.SetFixedStepMeasurementFlags(false,false,true);
        observer=[&,active=&play](const FixedStepMeasurements&,double milliseconds) {
            auto &current=active->Session().World();
            const auto &m=current.FluidCoupling().Measurements();
            steps.push_back({unsigned(steps.size()),milliseconds,m.particleMilliseconds,m.totalMilliseconds,
                             current.Physics().LastStepStats().totalMilliseconds,m.executed});
        };
    };
    control.beforeFrame=[&](EngineHost &host,RuntimeWorld&,InteractivePlay &play){
        play.SetFixedStepObserver(observer);play.SetFixedStepMeasurementFlags(false,false,true);
        host.GetRenderer().ResetStats();started=Clock::now();
    };
    control.afterFrame=[&](EngineHost &host,RuntimeWorld &world,InteractivePlay &play) {
        const double cpu=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
        const auto stats=host.GetRenderer().Stats();
        frames.push_back({unsigned(frames.size()),unsigned(play.LastFixedStepsThisFrame()),unsigned(world.Fluid().Particles().size()),
                          stats.drawCalls,unsigned(stats.triangles),profiler.FrameId(),cpu,
                          play.LastSurfaceMilliseconds(),play.LastSceneMilliseconds(),State(world)});
        collect(host.GetRenderer()); // outside recorded frame CPU/submission time
#ifdef JUDAS_FLUID_PROBE_V2
        environment["visual_gpu_particle_draws_last_frame"]=stats.gpuParticleDraws;
#endif
        if(frames.size()>=count) {
            MeshData surface;
            if(!host.GetRenderer().ReadMeshForDiagnostics(world.FluidMesh(),surface))throw std::runtime_error("cannot read uploaded fluid geometry");
            environment["surface_vertices"]=surface.vertices.size();
            std::string vertices;for(const auto &v:surface.vertices)for(float x:{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z})vertices.append(reinterpret_cast<const char*>(&x),sizeof(x));
            environment["surface_sha256"]=SceneFingerprintSha256(vertices);
            std::vector<unsigned char> image;host.GetRenderer().CaptureFrame(1280,720,image);
            WriteRgbPng((output/"final.png").string(),1280,720,image);
            SDL_Event quit{};quit.type=SDL_QUIT;SDL_PushEvent(&quit);
        }
    };
    control.beforeShutdown=[&](EngineHost &host,RuntimeWorld&,InteractivePlay&) {
        host.GetRenderer().FinishForDiagnostics();
        {ProfileFrame drain("Fluid rendering timestamp drain");collect(host.GetRenderer());}
        environment["gpu_dropped"]=profiler.Diagnostics().gpuDropped;
        std::ofstream(output/"environment.json")<<environment.dump(2)<<'\n';
        std::string error;profiler.Export((output/"profile.json").string(),error);
    };
    char name[]="judas";char *args[]={name,argv[1]};Application app;const int result=app.Run(2,args,&control);
    std::ofstream csv(output/"frames.csv");csv<<"frame,profile_frame,cpu_ms,surface_cpu_ms,render_cpu_ms,fixed_steps,particles,draws,triangles,state_sha256\n"<<std::setprecision(10);
    for(const auto &f:frames)csv<<f.index<<','<<f.profile<<','<<f.cpu<<','<<f.surface<<','<<f.render<<','<<f.steps<<','<<f.particles<<','<<f.draws<<','<<f.triangles<<','<<f.state<<'\n';
    std::ofstream fixed(output/"steps.csv");fixed<<"step,total_ms,particle_ms,coupling_ms,rigid_ms,fluid_executed\n"<<std::setprecision(10);
    for(const auto &s:steps)fixed<<s.index<<','<<s.fixed<<','<<s.particles<<','<<s.coupling<<','<<s.rigid<<','<<s.executed<<'\n';
    std::ofstream stamps(output/"gpu.csv");stamps<<"frame,token,pass,milliseconds\n"<<std::setprecision(10);
    for(const auto &[token,g]:gpu)stamps<<g.frame<<','<<token<<','<<g.pass.c_str()<<','<<g.milliseconds<<'\n';
    return result;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 2;}
