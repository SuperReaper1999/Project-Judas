#include "PhysicsEventDebugger.h"
#include "PhysicsWorld.h"
#include "PerformanceProfiler.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "Simulation.h"
#include "Window.h"
#include "Renderer.h"
#include "JobSystem.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include "../third_party/nlohmann/json.hpp"
namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* text){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",text);}
struct State {std::vector<glm::vec3> positions,velocities;std::vector<glm::quat> rotations;bool operator==(const State& b)const{return positions==b.positions&&velocities==b.velocities&&rotations==b.rotations;}};
struct Result {State state;std::vector<PhysicsWorld::DebugBoundary> trace;double wall=0,cpu=0;std::uint64_t excluded=0;};
Result Crash(unsigned count,bool observer,bool wait,bool chain,bool symmetric=false){
    PhysicsWorld world;world.Init();world.SetSleepingEnabled(false);JobSystem jobs(3);world.SetJobSystem(&jobs,4);
    std::vector<BodyHandle> actors;
    if(symmetric){
        for(float x:{-4.f,0.f,4.f})actors.push_back(world.CreateDynamicSphere({x,0,0},.2f,1,0,1));
        world.SetLinearVelocity(actors.front(),{1200,0,0});world.SetLinearVelocity(actors.back(),{-1200,0,0});
    }else if(chain){
        for(unsigned n=0;n<3;++n)actors.push_back(world.CreateDynamicSphere({n?float(n-1): -4.f,0,0},.2f,1,0,1));
        world.SetLinearVelocity(actors[0],{1200,0,0});
    }else{
        for(unsigned n=0;n<count;++n){
            actors.push_back(world.CreateDynamicBox({-4,float(n)*4,0},{.1,.15,.2},1,0,.5));
            BodyTransform pose;pose.position={0,float(n)*4,0};world.CreateShape(Shape::Box({.025,1,1}),pose,false,0,0,.5);
            world.SetLinearVelocity(actors.back(),{1200,0,0});
        }
    }
    Result result;bool slept=false;
    if(observer)world.SetDebugObserver([&](const auto& point){
        result.trace.push_back(point);
        if(point.phase==PhysicsWorld::DebugPhase::BeforeImpact){
            const auto bodies=world.CaptureDebugBodies(point.localTime);Check(!bodies.empty()&&point.contacts>0,"accepted response breakpoint exposes pre-commit ledger geometry and actual contact rows");
            bool recursive=false;try{world.Step(1.f/60);}catch(const std::logic_error&){recursive=true;}Check(recursive,"in-flight debugger cannot re-enter authoritative stepping");
            if(wait&&!slept){slept=true;std::this_thread::sleep_for(std::chrono::milliseconds(35));}
        }
    });
    auto& profiler=PerformanceProfiler::Get();profiler.Enable(true);profiler.Clear();
    const auto began=std::chrono::steady_clock::now();
    {ProfileFrame frame("physics-debugger-test");ProfileFixedStep fixed(1./60);world.Step(1.f/60);}
    result.wall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();result.cpu=world.LastStepStats().totalMilliseconds;
    const auto snapshot=profiler.Snapshot(profiler.FrameId());if(!snapshot.fixed.empty())result.excluded=snapshot.fixed[0].debuggerWaitNanoseconds;
    for(auto actor:actors){const auto pose=world.GetTransform(actor);result.state.positions.push_back(pose.position);result.state.rotations.push_back(pose.rotation);result.state.velocities.push_back(world.GetLinearVelocity(actor));}
    world.SetDebugObserver({});return result;
}
}
int main(){try{
    const auto reference=Crash(1,false,false,false),observed=Crash(1,true,true,false);
    Check(reference.state==observed.state,"inspection and a human-duration stop preserve exact impact state");
    Check(observed.excluded>=30000000&&observed.wall-observed.cpu>=30,"native work and fixed profiler records exclude inspection wait, not simulation time");
    std::size_t before=0,published=0,searches=0;for(const auto& p:observed.trace){before+=p.phase==PhysicsWorld::DebugPhase::BeforeImpact;published+=p.phase==PhysicsWorld::DebugPhase::TrajectoriesPublished;searches+=p.phase==PhysicsWorld::DebugPhase::SearchCompleted;}
    Check(before>0&&published>=before&&searches>0,"trace exposes actual searches, accepted commits and retimed trajectories");
    const auto multiple=Crash(8,true,false,false),multipleReference=Crash(8,false,false,false);
    Check(multiple.state==multipleReference.state,"parallel query preparation with debugging preserves exact simultaneous-impact results");
    std::vector<PhysicsWorld::DebugBoundary> accepted;for(auto p:multiple.trace)if(p.phase==PhysicsWorld::DebugPhase::BeforeImpact)accepted.push_back(p);
    bool grouped=accepted.size()>=8;for(unsigned n=1;grouped&&n<8;++n)grouped=accepted[n].macroTime==accepted[0].macroTime&&accepted[n].timeGroup==accepted[0].timeGroup&&accepted[n].event>accepted[n-1].event;
    Check(grouped,"same-time independent impacts share a time group and retain ordered event identities");
    const auto chain=Crash(1,true,false,true),chainReference=Crash(1,false,false,true);
    Check(chain.state==chainReference.state,"changed trajectories and recurrent contact response remain exact under inspection");
    std::size_t invalid=0;for(const auto& p:chain.trace)invalid+=p.phase==PhysicsWorld::DebugPhase::PredictionDiscarded;
    Check(invalid>0,"invalidated predictions are reported as discarded rather than accepted impact breakpoints");
    const auto opposed=Crash(1,true,false,false,true),opposedReference=Crash(1,false,false,false,true);
    bool coupled=false;for(const auto& p:opposed.trace)coupled|=p.phase==PhysicsWorld::DebugPhase::BeforeImpact&&p.coupled&&p.participants>=3;
    Check(coupled&&opposed.state==opposedReference.state,"recurrent simultaneous contact batches stop before atomic publication and retain exact physics");
    {
        Scene scene;auto& actor=scene.CreateObject("Debugger free body");const auto id=actor.id;actor.body=SceneBodyComponent{};actor.body->motion=SceneBodyMotion::Dynamic;
        RuntimeWorld world;std::string error;if(!world.Build(scene,nullptr,error))throw std::runtime_error(error);
        InteractivePlay play;WorldCoordinates coordinates;if(!play.Begin(world,coordinates,error))throw std::runtime_error(error);
        auto debugger=std::make_shared<PhysicsEventDebugger>();using Command=PhysicsEventDebugger::Command;
        debugger->SetHandler([](const auto&){return Command::Hold;});play.SetPhysicsDebugger(debugger);
        Window window;Renderer renderer;const auto initial=world.Physics().GetTransform(world.RuntimeBody(id));
        for(unsigned n=0;n<10;++n)play.Frame(window,renderer,100.f,false);
        Check(debugger->Count("FixedBegin")==1,"idle inspection polls cannot flood or evict the physical trace");
        Check(world.SimulationTimeSeconds()==0&&play.FixedStepsSinceReset()==0&&initial.position==world.Physics().GetTransform(world.RuntimeBody(id)).position,"paused wall-clock frames never accumulate or advance simulation");
        debugger->Request(Command::Fixed);play.Frame(window,renderer,100.f,false);
        Check(play.LastFixedStepsThisFrame()==1&&play.FixedStepsSinceReset()==1&&std::abs(world.SimulationTimeSeconds()-1./60)<1e-7,"manual fixed step advances exactly one normal fixed interval despite large wall delta");
        for(unsigned n=0;n<10;++n)play.Frame(window,renderer,100.f,false);
        Check(play.FixedStepsSinceReset()==1&&play.ConsumeDiagnosticClockReset(),"remaining paused time cannot create a catch-up sequence on resume");
        std::map<std::string,SaveChunk> saved;if(!WorldPersistence::Capture(world,saved,error))throw std::runtime_error(error);
        const auto point=world.Physics().GetTransform(world.RuntimeBody(id));
        debugger->Request(Command::Fixed);play.Frame(window,renderer,1,false);
        play.End();Scene restored;if(!WorldPersistence::SceneFrom(saved,restored,error)||!world.Build(restored,nullptr,error)||!WorldPersistence::Prepare(world,error)||!WorldPersistence::Restore(world,saved,error))throw std::runtime_error(error);WorldPersistence::Published(world);
        Check(point.position==world.Physics().GetTransform(world.RuntimeBody(id)).position,"ordinary persistence restores a completed pre-impact debug checkpoint exactly");
        if(!play.Begin(world,coordinates,error))throw std::runtime_error(error);
        Check(world.Physics().HasDebugObserver(),"world replacement rebinds the debugger without retaining old event predictions");
        bool validGaps=true;unsigned phaseStops=0;
        debugger->SetHandler([&](const auto& snapshot){validGaps&=snapshot.activeMilliseconds>=0;if(snapshot.inFlight){++phaseStops;std::this_thread::sleep_for(std::chrono::milliseconds(3));return Command::Phase;}return Command::Hold;});
        debugger->Request(Command::Phase);play.Frame(window,renderer,100.f,false);
        Check(validGaps&&phaseStops>=2,"phase CPU gaps remain nonnegative after repeated inspection waits");
        play.SetPhysicsDebugger({});play.End();
    }
    {
        Scene scene;const auto actor=scene.CreateObject("Replay projectile").id;
        auto* body=scene.Find(actor);body->body=SceneBodyComponent{};body->body->motion=SceneBodyMotion::Dynamic;
        body->body->shape=SceneShape::Sphere;body->body->radius=.2f;body->body->initialLinearVelocity={1200,0,0};body->transform.position={-4,0,0};
        const auto wall=scene.CreateObject("Replay barrier").id;scene.Find(wall)->body=SceneBodyComponent{};scene.Find(wall)->body->halfExtents={.025f,2,2};
        RuntimeWorld source,restored;std::string error;std::map<std::string,SaveChunk> chunks;
        if(!source.Build(scene,nullptr,error)||!WorldPersistence::Prepare(source,error)||!WorldPersistence::Capture(source,chunks,error))throw std::runtime_error(error);
        Scene saved;if(!WorldPersistence::SceneFrom(chunks,saved,error)||!restored.Build(saved,nullptr,error)||!WorldPersistence::Prepare(restored,error)||!WorldPersistence::Restore(restored,chunks,error))throw std::runtime_error(error);
        WorldPersistence::Published(restored);
        std::vector<double> originalTimes,replayTimes;
        source.Physics().SetDebugObserver([&](const auto& p){if(p.phase==PhysicsWorld::DebugPhase::BeforeImpact)originalTimes.push_back(p.macroTime);});
        restored.Physics().SetDebugObserver([&](const auto& p){if(p.phase==PhysicsWorld::DebugPhase::BeforeImpact)replayTimes.push_back(p.macroTime);});
        source.Physics().Step(1.f/60);restored.Physics().Step(1.f/60);
        Check(!originalTimes.empty()&&originalTimes==replayTimes,"completed pre-impact checkpoint reproduces the accepted CCD event timeline");
        Check(source.Physics().GetTransform(source.RuntimeBody(actor)).position==restored.Physics().GetTransform(restored.RuntimeBody(actor)).position&&source.Physics().GetLinearVelocity(source.RuntimeBody(actor))==restored.Physics().GetLinearVelocity(restored.RuntimeBody(actor)),"checkpoint replay reproduces exact post-impact pose and momentum");
    }
    {
        auto& profiler=PerformanceProfiler::Get();profiler.Clear();profiler.Enable(true);
        {ProfileFrame frame("inspector-recording");{ProfileScope parent(profiler.Intern("outer"));{ProfileRecordingPause quiet;for(unsigned n=0;n<10000;++n){ProfileScope suppressed(profiler.Intern("inspector draw"));}}}}
        const auto f=profiler.Snapshot(profiler.FrameId());Check(!f.incomplete&&!f.capReached&&f.diagnostics.droppedEvents==0&&f.scopes.size()==1,"long inspector rendering cannot overflow or invalidate live profiler scopes");
    }
    std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}catch(const std::exception& e){std::fprintf(stderr,"physics debugger test: %s\n",e.what());return 1;}}
