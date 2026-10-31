#include <filesystem>
#include "FluidDiagnosticForceGate.h"
// Diagnostic-only driver. Links the existing engine unchanged and advances its
// authoritative StepPlayedWorld exactly once per request. No alternate solver.
#include "BuildIdentity.h"
#include "EngineHost.h"
#include "FluidHydrostatics.h"
#include "GameSession.h"
#include "ProductionFluidCoupling.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "ScreenshotWriter.h"
#include "Simulation.h"
#include "SimulationTiming.h"
#include "WorldPresentation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
constexpr float dt = SimulationTiming::kFixedTimestep;
void V(std::ostream& out, glm::vec3 v) { out << '[' << v.x << ',' << v.y << ',' << v.z << ']'; }
void Q(std::ostream& out, glm::quat q) { out << '[' << q.w << ',' << q.x << ',' << q.y << ',' << q.z << ']'; }
std::string Stamp() {
    const auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::ostringstream out;out<<std::put_time(std::gmtime(&now),"%Y-%m-%dT%H:%M:%SZ");return out.str();
}
void Bodies(std::ostream& out, RuntimeWorld& world) {
    out << '['; bool first=true;
    for(const auto& body:world.DynamicBodies()) {
        const auto h=body.Handle(); if(!world.Physics().IsDynamicBody(h))continue;
        if(!first)out<<',';
        first=false;
        const auto p=world.Physics().GetTransform(h);
        out<<"{\"handle\":"<<h.id<<",\"position\":";V(out,p.position);
        out<<",\"rotation_wxyz\":";Q(out,p.rotation);
        out<<",\"velocity\":";V(out,world.Physics().GetLinearVelocity(h));
        out<<",\"angular_velocity\":";V(out,world.Physics().GetAngularVelocity(h));
        out<<",\"mass\":"<<world.Physics().GetMass(h)<<'}';
    }out<<']';
}
void Particles(std::ostream& out, RuntimeWorld& world) {
    out<<'[';bool first=true;
    for(const auto& p:world.Fluid().Particles()) {
        if(!first)out<<',';
        first=false;
        out<<"{\"p\":";V(out,p.position);out<<",\"previous_p\":";V(out,p.previousPosition);
        out<<",\"v\":";V(out,p.velocity);out<<",\"a\":";V(out,p.acceleration);
        out<<",\"mass\":"<<p.mass<<'}';
    }out<<']';
}
void Player(std::ostream& out, const GameSession& session) {
    out << "{\"position\":"; V(out, session.Player().GetPosition());
    out << ",\"velocity\":"; V(out, session.Player().GetVelocity());
    out << ",\"orientation_wxyz\":"; Q(out, session.Player().GetOrientation()); out << '}';
}
std::string State(RuntimeWorld& world, const GameSession& session) {
    std::ostringstream s;s<<std::setprecision(17)<<world.SimulationTimeSeconds()<<' ';
    Bodies(s,world);Particles(s,world);Player(s,session);return s.str();
}
struct Expected { BodyHandle body; glm::vec3 force,torque; };
// Reconstruct the same diagnostic quadrature on this fixture. The first
// production field is empty; thereafter it is built at each particle endpoint.
// Equality against the production load is checked after the real step below.
std::vector<Expected> Samples(std::ostream& out,RuntimeWorld& world,const Scene& scene,std::size_t step) {
    FluidHydrostaticField field;
    if(step)field.Build(world.Fluid().Particles(),world.FluidSettingsUsed().restDensity);
    std::vector<Expected> expected;
    out<<'[';bool firstBody=true;
    for(const auto& object:scene.Objects()) {
        if(!object.body||object.body->motion!=SceneBodyMotion::Dynamic)continue;
        const auto h=world.RuntimeBody(object.id);
        if(!world.Physics().IsDynamicBody(h))continue;
        if(object.body->shape!=SceneShape::Box)throw std::runtime_error("viewer fixture requires box dynamic bodies");
        if(!object.body->fluidCavities.empty())throw std::runtime_error("viewer shadow trace does not implement cavity exclusion");
        const auto volume=MakeBoxFluidVolume(object.body->halfExtents);
        const auto pose=world.Physics().GetTransform(h);
        const auto load=EvaluateFluidHydrostatics(field,volume,pose,world.Gravity());
        expected.push_back({h,load.buoyancyForce,load.buoyancyTorque});
        double clearance=1e30;
        for(const auto& p:world.Fluid().Particles()) {
            const auto q=glm::abs(glm::conjugate(pose.rotation)*(p.position-pose.position))-object.body->halfExtents;
            const double distance=glm::length(glm::max(q,glm::vec3(0)))+std::min(std::max({q.x,q.y,q.z}),0.f)-world.FluidSettingsUsed().particleRadius;
            clearance=std::min(clearance,distance);
        }
        if(!firstBody)out<<',';
        firstBody=false;
        out<<"{\"entity\":"<<object.id<<",\"handle\":"<<h.id<<",\"name\":"<<std::quoted(object.name)
           <<",\"nearest_particle_box_clearance\":"<<clearance<<",\"body_bound_radius\":"<<volume.boundRadius
           <<",\"predicted_force\":";V(out,load.buoyancyForce);out<<",\"predicted_torque\":";V(out,load.buoyancyTorque);
        out<<",\"samples\":[";bool firstSample=true;
        for(const auto& sample:volume.samples) {
            const glm::vec3 offset=pose.rotation*sample.localPosition,point=pose.position+offset,g=world.Gravity().Sample(point);
            const auto up=glm::dot(g,g)>0?-glm::normalize(g):pose.rotation*glm::vec3(0,1,0);
            const auto localUp=glm::conjugate(pose.rotation)*up;
            const float height=glm::dot(glm::abs(glm::conjugate(sample.localRotation)*localUp),sample.localHalfExtents)+sample.localRadius;
            glm::vec3 tangent(0);float best=-1;
            for(int axis=0;axis<3;++axis) {glm::vec3 a(0);a[axis]=1;a=pose.rotation*a;const auto t=a-up*glm::dot(a,up);const float l=glm::dot(t,t);if(l>best){best=l;tangent=t;}}
            const FluidSolidQuery solid{volume,pose};
            const auto fluid=field.Query(point,up,height,volume.boundRadius,tangent,&solid,true);
            const glm::dvec3 force=-double(fluid.density)*double(sample.volume)*double(fluid.fraction)*(glm::dvec3(g)-glm::dvec3(fluid.acceleration));
            if(!firstSample)out<<',';
            firstSample=false;
            out<<"{\"p\":";V(out,point);out<<",\"g\":";V(out,g);out<<",\"up\":";V(out,up);
            out<<",\"half_height\":"<<height<<",\"volume\":"<<sample.volume<<",\"fraction\":"<<fluid.fraction
               <<",\"density\":"<<fluid.density<<",\"fluid_velocity\":";V(out,fluid.velocity);
            out<<",\"fluid_acceleration\":";V(out,fluid.acceleration);
            out<<",\"force_double\":["<<force.x<<','<<force.y<<','<<force.z<<"]}";
        }out<<"]}";
    }out<<']';return expected;
}
}
int main(int argc,char** argv) try {
    if(argc<3||argc>6)throw std::runtime_error("usage: viewer project.judasproj log.jsonl [--self-test] [--water-only-start]");
    bool test=false,waterOnlyStart=false,suppressBefore60=false;
    for(int i=3;i<argc;++i) {
        const std::string option=argv[i];
        if(option=="--self-test")test=true;
        else if(option=="--water-only-start")waterOnlyStart=true;
        else if(option=="--suppress-water-before-60")suppressBefore60=true;
        else throw std::runtime_error("unknown viewer option: "+option);
    }
    if(waterOnlyStart&&suppressBefore60)throw std::runtime_error("choose one diagnostic experiment at a time");
    Project project;Scene scene;std::string error;
    const bool explicitScene=std::filesystem::path(argv[1]).extension()==".judas";
    const auto projectPath=explicitScene?Project::FindProjectFileFor(argv[1]):std::string(argv[1]);
    if(!project.Load(projectPath,error)||!LoadSceneFromFile(explicitScene?std::string(argv[1]):project.StartupScenePath(),scene,error))throw std::runtime_error(error);
    EngineHost host;
    if(!host.Init("Fluid launch debug | armed",1280,720,!test,error))throw std::runtime_error(error);
    host.OpenProjectAssets(project.RootDir(),project.AssetsDir());host.Resources().SetBlockingMode(true);
    RuntimeWorld world;world.ConfigureProject(project.Settings());
    if(!world.Build(scene,&host.Resources(),error))throw std::runtime_error(error);
    if(!world.HasFluid())throw std::runtime_error("advanced particles unavailable; use Experimental with project opt-in");
    GameSession session;if(!session.Begin(world,error))throw std::runtime_error(error);
    auto& window=host.GetWindow();auto& renderer=host.GetRenderer();
    window.SetMouseCaptured(false);window.SetInputClaimed(true,true);
    bool q=false;
    window.SetEventHook([&](const SDL_Event& event){if(event.type==SDL_KEYDOWN&&event.key.keysym.scancode==SDL_SCANCODE_Q&&!event.key.repeat)q=true;});
    const glm::mat4 view=session.Player().GetViewMatrix(1,session.ViewMode());
    std::ofstream log(argv[2]);if(!log)throw std::runtime_error("cannot open diagnostic log");
    log<<std::setprecision(17);
    log<<"{\"event\":\"metadata\",\"utc\":"<<std::quoted(Stamp())<<",\"project\":"<<std::quoted(project.ProjectFile())
       <<",\"scene\":"<<std::quoted(explicitScene?std::string(argv[1]):project.StartupScenePath())<<",\"engine_revision\":"<<std::quoted(GetBuildIdentity().sourceRevision)
       <<",\"suppress_water_before_step\":"<<(suppressBefore60?60:0)<<",\"water_only_start_steps\":"<<(waterOnlyStart?2:0)<<",\"dt\":"<<dt<<",\"trap\":"<<std::quoted(suppressBefore60?"step 60: water-to-body force re-enable boundary":"first nonzero production buoyancy or drag")<<",\"particles\":"<<world.Fluid().Particles().size()<<"}\n";
    std::size_t step=0;bool paused=false;bool capture=false;std::size_t trappedAt=0;
    auto advance=[&] {
        log<<"{\"event\":\"before\",\"utc\":"<<std::quoted(Stamp())<<",\"step\":"<<step+1<<",\"time\":"<<world.SimulationTimeSeconds()<<",\"bodies\":";
        Bodies(log,world);log<<",\"player\":";Player(log,session);log<<",\"particles\":";Particles(log,world);log<<",\"recomputed_field\":";
        const auto expected=Samples(log,world,scene,step);log<<"}\n";
        FluidLaunchDiagnostic::suppress=suppressBefore60&&step+1<60;
        FluidLaunchDiagnostic::counts={};
        const auto fluidRevisionBefore=world.Fluid().Revision();
        window.Input().BeginFixedStep();FixedStepMeasurements measured;measured.measureFluid=true;
        if(waterOnlyStart&&step<2) {
            std::ostringstream heldBefore;heldBefore<<std::setprecision(17);
            Bodies(heldBefore,world);Player(heldBefore,session);
            // Deliberately skip ALL non-water simulation: no gravity, rigid
            // integration, player, scripts, or force preparation/catch-up.
            // Fluid boundaries remain at their actual frozen poses, with the
            // original configured particle cadence and normal contact filtering.
            world.FluidCoupling().AdvanceResolvedLiquid(world,dt);
            world.AdvanceSimulationTime(dt);
            std::ostringstream heldAfter;heldAfter<<std::setprecision(17);
            Bodies(heldAfter,world);Player(heldAfter,session);
            if(heldBefore.str()!=heldAfter.str())throw std::runtime_error("water-only startup changed a body or player");
        } else StepPlayedWorld(session,window,dt,&measured);
        ++step;
        bool loadStarted=false;
        log<<"{\"event\":\"after\",\"utc\":"<<std::quoted(Stamp())<<",\"step\":"<<step<<",\"time\":"<<world.SimulationTimeSeconds()<<",\"bodies\":";
        Bodies(log,world);log<<",\"player\":";Player(log,session);log<<",\"particles\":";Particles(log,world);log<<",\"loads\":[";
        bool first=true;
        for(const auto& load:world.FluidCoupling().Measurements().bodies) {
            if(!first)log<<',';
            first=false;
            const auto found=std::find_if(expected.begin(),expected.end(),[&](const auto& e){return e.body.id==load.body.id;});
            const bool matches=found!=expected.end()&&found->force==load.buoyancyForce;
            if(!matches)throw std::runtime_error("shadow reconstruction differs from production load; trace invalid");
            log<<"{\"handle\":"<<load.body.id<<",\"fraction\":"<<load.fraction<<",\"displaced_volume\":"<<load.displacedVolume<<",\"buoyancy\":";
            V(log,load.buoyancyForce);log<<",\"drag\":";V(log,load.dragForce);log<<",\"recomputed_force_exact_match\":true}";
            loadStarted|=glm::dot(load.buoyancyForce,load.buoyancyForce)>0||glm::dot(load.dragForce,load.dragForce)>0;
        }
        const auto& f=world.FluidCoupling().Measurements();
        log<<"],\"water_forces_suppressed\":"<<(FluidLaunchDiagnostic::suppress?"true":"false")<<",\"suppressed_force_calls\":"<<FluidLaunchDiagnostic::counts.forceCalls<<",\"suppressed_torque_calls\":"<<FluidLaunchDiagnostic::counts.torqueCalls<<",\"suppressed_swim_samples\":"<<FluidLaunchDiagnostic::counts.swimSamples<<",\"suppressed_reaction_rows\":"<<FluidLaunchDiagnostic::counts.reactionRows<<",\"water_only_phase\":"<<((waterOnlyStart&&step<=2)?"true":"false")<<",\"fluid_advanced\":"<<((world.Fluid().Revision()!=fluidRevisionBefore)?"true":"false")<<",\"fluid_dt\":"<<f.lastFluidDeltaTime<<",\"contact_batches\":"<<f.contactBatches<<",\"contact_iterations\":"<<f.contactIterations<<"}\n";
        if(!paused&&!FluidLaunchDiagnostic::suppress&&(loadStarted||(suppressBefore60&&step==60))){paused=true;capture=true;trappedAt=step;log<<"{\"event\":\"trap\",\"step\":"<<step<<",\"time\":"<<world.SimulationTimeSeconds()<<"}\n";}
        log.flush();if(!log)throw std::runtime_error("diagnostic log write failed");
        std::string title="FLUID DEBUG | "+std::string(paused?"PAUSED | Q = one fixed step":"ARMED")+" | step "+std::to_string(step);
        SDL_SetWindowTitle(window.NativeWindow(),title.c_str());
    };
    auto render=[&] {
        host.PumpResources();UpdateFluidSurface(renderer,world,1);
        const auto projection=glm::perspective(glm::radians(60.f),float(window.Width())/window.Height(),.1f,2000.f);
        RenderWorldFrame(renderer,window.Width(),window.Height(),world,&session,view,projection,session.Player().GetPosition(),1,0);
        renderer.BeginUIFrame(window.Width(),window.Height());renderer.DrawUIRect({10,10},{float(window.Width()-20),120},{0,0,0,.88f});
        renderer.DrawUIText(paused?(suppressBefore60?"FLUID DEBUG: PAUSED - water forces enabled":"FLUID DEBUG: PAUSED at first fluid load"):(suppressBefore60?"FLUID DEBUG: water forces blocked until step 60":"FLUID DEBUG: armed - unchanged physics"),{24,20},.65f,{1,.8f,.3f,1});
        renderer.DrawUIText("Step "+std::to_string(step)+" | time "+std::to_string(world.SimulationTimeSeconds())+" s | Q: ONE 1/60 s STEP",{24,60},.5f,{1,1,1,1});
        renderer.DrawUIText("All gameplay frozen between steps. Fixed observer camera. Close window to exit.",{24,92},.42f,{.8f,.9f,1,1});
        renderer.EndUIFrame();
        if(capture) {
            std::vector<unsigned char> pixels;renderer.CaptureFrame(window.Width(),window.Height(),pixels);
            if(!WriteRgbPng(std::string(argv[2])+".trap.png",window.Width(),window.Height(),pixels))
                throw std::runtime_error("trap screenshot failed");
            capture=false;
        }
        window.SwapBuffers();
    };
    while(!window.ShouldClose()) {
        window.PollEvents();if(window.ShouldClose())break;
        if(!paused||q){q=false;advance();}
        render();
        if(test&&paused) {
            const auto before=State(world,session);const auto count=step;
            for(int i=0;i<5;++i){window.PollEvents();render();}
            if(before!=State(world,session)||count!=step)throw std::runtime_error("paused render advanced authoritative state");
            SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_SCANCODE_Q;
            SDL_PushEvent(&event);window.PollEvents();if(!q)throw std::runtime_error("Q event did not request single step");
            q=false;advance();render();if(step!=count+1||!paused)throw std::runtime_error("single step did not remain paused");
            event.key.repeat=1;SDL_PushEvent(&event);window.PollEvents();if(q)throw std::runtime_error("key repeat requested extra step");
            const auto frozen=State(world,session);render();if(frozen!=State(world,session))throw std::runtime_error("post-step render advanced state");
            // Same original fixture and fixed path, uninterrupted, without any
            // diagnostic queries: establish that observing/pausing changed no endpoint.
            RuntimeWorld control;control.ConfigureProject(project.Settings());
            if(!control.Build(scene,&host.Resources(),error))throw std::runtime_error(error);
            GameSession baseline;if(!baseline.Begin(control,error))throw std::runtime_error(error);
            for(std::size_t n=0;n<step;++n) {
                FluidLaunchDiagnostic::suppress=suppressBefore60&&n+1<60;
                window.Input().BeginFixedStep();
                if(waterOnlyStart&&n<2) {
                    control.FluidCoupling().AdvanceResolvedLiquid(control,dt);
                    control.AdvanceSimulationTime(dt);
                } else StepPlayedWorld(baseline,window,dt);
            }
            if(State(control,baseline)!=State(world,session))throw std::runtime_error("diagnostic trajectory differs from uninterrupted control");
            log<<"{\"event\":\"self_test_pass\",\"trap_step\":"<<trappedAt<<",\"final_step\":"<<step<<",\"paused_state_unchanged\":true,\"Q_exactly_one_step\":true,\"repeat_ignored\":true,\"uninterrupted_endpoint_exact_match\":true}\n";
            std::cout<<"PASS: trap="<<trappedAt<<"; pause stable; Q exactly one step; key repeat ignored; uninterrupted endpoint exact match\n";
            baseline.End();break;
        }
        if(!paused&&step>=120)throw std::runtime_error("no fluid load found within 120 fixed steps");
        SDL_Delay(paused?16:8);
    }
    session.End();return 0;
} catch(const std::exception& e) {std::cerr<<"Fluid viewer: "<<e.what()<<'\n';return 1;}
