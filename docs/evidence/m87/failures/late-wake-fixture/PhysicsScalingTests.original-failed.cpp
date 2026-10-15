// M87: scaling must not change the established sleeping/contact semantics.
// These are physical endpoint and lifetime checks, not timing acceptance gates.
#include "PhysicsWorld.h"
#include "PerformanceProfiler.h"
#include "SaveArchive.h"

#include <cmath>
#include <cstdio>
#include <tuple>
#include <vector>

namespace {
unsigned checks=0,failures=0;
constexpr float dt=1.f/60;
void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}}
struct SleepState {bool asleep=false;float quiet=0;glm::vec3 acceleration{0};bool deferred=false;};
SleepState Sleep(PhysicsWorld& world,BodyHandle body){
    SaveArchive written;world.PersistSleep(written,body);SaveArchive read(written.bytes);SleepState state;
    read(state.asleep,state.quiet,state.acceleration,state.deferred);read.Finish();return state;
}
void ClockAndEligibility(){
    PhysicsWorld world;world.Init();
    auto still=world.CreateDynamicSphere({0,0,0},.1f,1,0,0);
    auto drifting=world.CreateDynamicSphere({4,0,0},.1f,1,0,0);world.SetLinearVelocity(drifting,{.001f,0,0});
    auto sensor=world.CreateDynamicSphere({8,0,0},.1f,1,0,0);world.SetBodySensor(sensor,true);
    auto disabled=world.CreateDynamicSphere({12,0,0},.1f,1,0,0);world.SetBodyEnabled(disabled,false);
    float expected=0;bool sawThreshold=false;
    for(unsigned step=0;step<60;++step){
        expected+=dt;world.Step(dt);const auto state=Sleep(world,still);
        Check(state.quiet==expected&&state.asleep==(expected>=.75f),"original binary32 quiet clock and 0.75-second sleep boundary preserved");
        if(expected>=.75f)sawThreshold=true;
    }
    Check(sawThreshold&&world.IsSleeping(still),"truly stationary zero-gravity body sleeps");
    Check(!world.IsSleeping(drifting)&&world.GetTransform(drifting).position.x>4,"small genuine unsupported motion is never frozen by a quiet velocity threshold");
    Check(!world.IsSleeping(sensor)&&!world.IsSleeping(disabled),"sensor and disabled dynamic bodies are excluded from sleep islands");
    world.SetSleepingEnabled(false);world.Step(dt);
    Check(!world.IsSleeping(still)&&world.LastStepStats().sleepingIslands==0,"disabling sleeping wakes and retains ordinary active simulation");
    world.SetSleepingEnabled(true);world.Step(dt);
    Check(Sleep(world,still).quiet==dt&&!world.IsSleeping(still),"re-enabling sleeping starts a fresh quiet interval");
}
struct Machine {BodyHandle base,arm;JointHandle anchor,link;glm::vec3 position;};
void IndependentAssemblies(){
    PhysicsWorld world;world.Init();std::vector<Machine> machines;
    const auto q=glm::angleAxis(.81f,glm::normalize(glm::vec3(1,2,-3)));
    const auto gravity=q*glm::vec3(0,-9.81f,0);
    for(unsigned i=0;i<32;++i){
        Machine m;m.position=q*glm::vec3(float(i%8)*4,3,float(i/8)*4);
        m.base=world.CreateShape(Shape::Box({.2f,.2f,.2f}),{m.position,q},true,2,0,0);
        m.arm=world.CreateShape(Shape::Box({.2f,.2f,.2f}),{m.position+q*glm::vec3(1,0,0),q},true,1,0,0);
        JointSettings s;s.type=JointType::Fixed;s.bodyA=m.base;s.anchorB=m.position;s.frameB=q;m.anchor=world.CreateJoint(s);
        s={};s.type=JointType::Fixed;s.bodyA=m.base;s.bodyB=m.arm;s.anchorA={.5f,0,0};s.anchorB={-.5f,0,0};m.link=world.CreateJoint(s);
        Check(m.anchor.IsValid()&&m.link.IsValid(),"independent physical assembly constructed");machines.push_back(m);
    }
    auto step=[&](){for(const auto& m:machines){world.ApplyLinearAcceleration(m.base,gravity,dt);world.ApplyLinearAcceleration(m.arm,gravity,dt);}world.Step(dt);};
    for(unsigned i=0;i<180;++i)step();
    bool asleep=true,physical=true;
    for(const auto& m:machines){asleep&=world.IsSleeping(m.base)&&world.IsSleeping(m.arm);physical&=glm::length(world.GetTransform(m.base).position-m.position)<.005f&&glm::length(world.GetTransform(m.arm).position-m.position-q*glm::vec3(1,0,0))<.005f;}
    Check(asleep&&world.LastStepStats().sleepingIslands==machines.size(),"independent anchored articulations sleep as separate islands under arbitrary gravity");
    Check(physical,"actual solved positions remain at their physical anchors");
    world.ApplyLinearImpulse(machines[3].arm,q*glm::vec3(2,0,0));
    Check(!world.IsSleeping(machines[3].base)&&!world.IsSleeping(machines[3].arm),"impulse wakes the complete connected articulation");
    bool independent=true;for(unsigned i=0;i<machines.size();++i)if(i!=3)independent&=world.IsSleeping(machines[i].base)&&world.IsSleeping(machines[i].arm);
    Check(independent,"sharing one physics world does not wake unrelated physical assemblies");
    for(unsigned i=0;i<180;++i)step();
    Check(world.IsSleeping(machines[3].base)&&world.IsSleeping(machines[3].arm),"physically restrained disturbed assembly resettles normally");
    auto start=world.GetTransform(machines[3].base).position;
    Check(world.DestroyJoint(machines[3].anchor),"world attachment can be removed");
    for(unsigned i=0;i<12;++i)step();
    Check(!world.IsSleeping(machines[3].base)&&!world.IsSleeping(machines[3].arm)&&glm::dot(world.GetTransform(machines[3].base).position-start,glm::normalize(gravity))>.05f,"support removal restores real gravity-driven motion of the connected assembly");
    independent=true;for(unsigned i=0;i<machines.size();++i)if(i!=3)independent&=world.IsSleeping(machines[i].base)&&world.IsSleeping(machines[i].arm);
    Check(independent,"removing one support preserves other sleeping assemblies");
}
void DrivenAndDisabledJoints(){
    PhysicsWorld world;world.Init();auto body=world.CreateDynamicBox({0,0,0},{.2f,.2f,.2f},1,0,0);
    JointSettings s;s.type=JointType::Hinge;s.bodyA=body;s.motor=true;s.speed=1;s.maxForce=1;auto joint=world.CreateJoint(s);
    for(unsigned i=0;i<120;++i)world.Step(dt);
    Check(!world.IsSleeping(body)&&std::abs(world.GetAngularVelocity(body).x)>.5f,"a genuinely driven hinge remains active");
    s.motor=false;world.SetJoint(joint,s);world.SetAngularVelocity(body,{0,0,0});
    for(unsigned i=0;i<120;++i)world.Step(dt);
    Check(world.IsSleeping(body),"removing actuator intent permits an actually quiet hinge to sleep");
    s.enabled=false;world.SetJoint(joint,s);world.ApplyLinearAcceleration(body,{0,-9.81f,0},dt);world.Step(dt);
    Check(!world.IsSleeping(body)&&world.GetLinearVelocity(body).y<-.1f,"disabled joints cannot supply support or suppress real falling motion");
}
void ContactAndSlotLifetime(){
    PhysicsWorld world;world.Init();
    const auto floor=world.CreateStaticBox({0,-.5f,0},{2,.5f,2},.5f,0);
    const auto lower=world.CreateDynamicBox({0,.25f,0},{.25f,.25f,.25f},1,.5f,0);
    const auto upper=world.CreateDynamicBox({0,.75f,0},{.25f,.25f,.25f},1,.5f,0);
    for(unsigned i=0;i<240;++i){world.ApplyLinearAcceleration(lower,{0,-9.81f,0},dt);world.ApplyLinearAcceleration(upper,{0,-9.81f,0},dt);world.Step(dt);}
    Check(world.IsSleeping(lower)&&world.IsSleeping(upper)&&world.LastStepStats().sleepingIslands==1,"real resting manifold support connects a stacked physical island");
    std::vector<PhysicsWorld::BodyContactSample> contacts;
    Check(world.ReadBodyContactSample(lower,contacts)&&!contacts.empty(),"sleeping contact island retains genuine completed physical support observations");
    world.ApplyLinearImpulse(upper,{.01f,0,0});
    Check(!world.IsSleeping(lower)&&!world.IsSleeping(upper),"cached positive support rows propagate a real contact-island wake");
    for(unsigned i=0;i<180;++i){world.ApplyLinearAcceleration(lower,{0,-9.81f,0},dt);world.ApplyLinearAcceleration(upper,{0,-9.81f,0},dt);world.Step(dt);}
    world.DestroyBody(floor);world.CreateStaticBox({100,-.5f,0},{2,.5f,2},.5f,0);
    for(unsigned i=0;i<12;++i){world.ApplyLinearAcceleration(lower,{0,-9.81f,0},dt);world.ApplyLinearAcceleration(upper,{0,-9.81f,0},dt);world.Step(dt);}
    Check(!world.IsSleeping(lower)&&world.GetTransform(lower).position.y<.1f,"destroyed and reused support slot cannot retain a stale anchor");
    PhysicsWorld churn;churn.Init();std::vector<BodyHandle> old;
    for(unsigned i=0;i<128;++i)old.push_back(churn.CreateDynamicSphere({float(i)*2,0,0},.1f,1,0,0));
    for(unsigned i=0;i<90;++i)churn.Step(dt);
    Check(churn.LastStepStats().sleepingIslands==128,"large zero-gravity population establishes real independent islands");
    for(auto body:old)churn.DestroyBody(body);
    auto fresh=churn.CreateDynamicSphere({0,0,0},.1f,1,0,0);churn.Step(dt);
    Check(fresh.id!=old.back().id&&!churn.IsDynamicBody(old.back())&&!churn.IsSleeping(fresh)&&Sleep(churn,fresh).quiet==dt,"reused DSU/member capacity carries no prior generation or quiet time");
    Check(churn.LastStepStats().bodies==1&&churn.LastStepStats().awakeBodies==1&&churn.LastStepStats().sleepingIslands==0,"unloaded slots are absent from physical island publication");
}
void BoundedDiagnostics(){
    auto& profiler=PerformanceProfiler::Get();profiler.Enable(false);profiler.Clear();
    auto run=[&](bool record){
        profiler.Enable(record);PhysicsWorld world;world.Init();
        world.CreateStaticBox({0,-.5f,0},{4,.5f,4},.5f,0);
        auto body=world.CreateDynamicBox({0,.6f,0},{.2f,.2f,.2f},1,.5f,0);
        world.SetLinearVelocity(body,{1,-3,0});
        for(unsigned i=0;i<120;++i){ProfileFrame frame("M87 diagnostic invariance");world.ApplyLinearAcceleration(body,{0,-9.81f,0},dt);world.Step(dt);}
        const auto pose=world.GetTransform(body);return std::make_tuple(pose.position,pose.rotation,world.GetLinearVelocity(body),world.GetAngularVelocity(body),Sleep(world,body).quiet);
    };
    const auto off=run(false),on=run(true);
    Check(off==on,"profiler stages and bounded CCD aggregate timers do not change actual physical endpoint or quiet clock");
    bool prediction=false,preparation=false,endpoint=false,graph=false,geometry=false;
    for(const auto& frame:profiler.History()){
        for(const auto& scope:frame.scopes){prediction|=scope.name=="Physics prediction and wake";preparation|=scope.name=="Physics contact row preparation";endpoint|=scope.name=="Physics endpoint contact publication";}
        for(const auto& c:frame.counters){graph|=c.name=="Physics sleep islands";geometry|=c.name=="Physics CCD geometry inclusive ms";}
    }
    Check(prediction&&preparation&&endpoint&&graph&&geometry,"M56 exposes prediction, preparation, publication, sleep graph and bounded CCD cost");
    const auto d=profiler.Diagnostics();Check(!d.droppedEvents&&!d.truncatedFrames&&!d.openScopes,"physics diagnostics fit bounded profiler lanes and close every scope");
    profiler.Enable(false);profiler.Clear();
}
void SimultaneousImpactDiagnostics(){
    // More independent impact islands than the profiler's per-frame record
    // budget: an event-by-event preparation scope would necessarily overflow.
    constexpr unsigned count=ProfileLimits::FrameEvents+64;
    using Endpoint=std::tuple<glm::vec3,glm::quat,glm::vec3,glm::vec3,float>;
    struct Result {std::vector<Endpoint> endpoints;std::size_t impacts=0;bool stopped=true;};
    auto& profiler=PerformanceProfiler::Get();profiler.Enable(false);profiler.Clear();
    auto run=[&](bool record){
        PhysicsWorld world;world.Init();std::vector<BodyHandle> bodies;
        world.CreateStaticBox({0,-.5f,0},{32,.5f,32},0,0);
        for(unsigned i=0;i<count;++i){
            auto body=world.CreateDynamicSphere({float(i%48)-24,.125f,float(i/48)-24},.125f,1,0,0);
            world.SetLinearVelocity(body,{0,-2,0});bodies.push_back(body);
        }
        profiler.Enable(record);
        {ProfileFrame frame("M87 simultaneous independent impacts");world.Step(dt);}
        profiler.Enable(false);Result result;result.impacts=world.LastStepStats().impactEvents;
        for(auto body:bodies){const auto pose=world.GetTransform(body);const auto velocity=world.GetLinearVelocity(body);
            result.stopped&=pose.position.y>=.12499f&&glm::length(velocity)<1e-5f;
            result.endpoints.emplace_back(pose.position,pose.rotation,velocity,world.GetAngularVelocity(body),Sleep(world,body).quiet);
        }
        return result;
    };
    const auto off=run(false);profiler.Clear();const auto before=profiler.Diagnostics();const auto on=run(true);
    Check(off.impacts==count&&on.impacts==count&&off.stopped&&on.stopped,"thousands of independent actual ground impacts stop incoming physical bodies");
    Check(off.endpoints==on.endpoints,"aggregated impact diagnostics preserve every physical endpoint and quiet clock exactly");
    const auto history=profiler.History();bool preparations=false;
    if(history.size()==1)for(const auto& c:history.front().counters)if(c.name=="Physics joint preparation calls")preparations=c.value>=count;
    Check(preparations,"bounded aggregate records every independent impact preparation rather than omitting the expensive path");
    Check(history.size()==1&&history.front().scopes.size()<100,"physics profiler scope count stays bounded across thousands of independent impact events");
    const auto after=profiler.Diagnostics();
    Check(after.droppedEvents==before.droppedEvents&&after.truncatedFrames==before.truncatedFrames&&after.incompleteScopes==before.incompleteScopes&&!after.openScopes,"simultaneous-impact diagnostics neither flood nor leave unfinished profiler scopes");
    profiler.Enable(false);profiler.Clear();
}
void LateWakeVelocityCoupling(){
    // A new finite material may first touch a sleeping articulation during a
    // velocity round. Compare that boundary with explicit pre-step waking and
    // a genuinely undisturbed sleeping control, using actual physical outputs.
    PhysicsWorld worlds[3];BodyHandle bases[3],arms[3];JointHandle anchors[3];
    for(unsigned trial=0;trial<3;++trial){auto& world=worlds[trial];world.Init();
        bases[trial]=world.CreateDynamicBox({0,3,0},{.1f,.1f,.1f},2,0,0);
        arms[trial]=world.CreateDynamicBox({1,3,0},{.1f,.1f,.1f},1,0,0);
        JointSettings s;s.type=JointType::Fixed;s.bodyA=bases[trial];s.anchorB={0,3,0};anchors[trial]=world.CreateJoint(s);
        s={};s.type=JointType::Fixed;s.bodyA=bases[trial];s.bodyB=arms[trial];s.anchorA={.5f,0,0};s.anchorB={-.5f,0,0};world.CreateJoint(s);
        for(unsigned step=0;step<180;++step){world.ApplyLinearAcceleration(bases[trial],{0,-9.81f,0},dt);world.ApplyLinearAcceleration(arms[trial],{0,-9.81f,0},dt);world.Step(dt);}
        Check(world.IsSleeping(bases[trial])&&world.IsSleeping(arms[trial]),"late-wake fixture begins with a genuinely sleeping physical articulation");
        world.ApplyLinearAcceleration(bases[trial],{0,-9.81f,0},dt);world.ApplyLinearAcceleration(arms[trial],{0,-9.81f,0},dt);
    }
    bool beganSleeping[2]={false,false};unsigned rounds[2]={0,0};
    for(unsigned trial=0;trial<2;++trial){PhysicsWorld::VelocityCoupling material;material.iterations=10;
        material.begin=[&,trial](float){beganSleeping[trial]=worlds[trial].IsSleeping(bases[trial])&&worlds[trial].IsSleeping(arms[trial]);};
        material.solve=[&,trial](float,unsigned round){++rounds[trial];if(trial==1&&round==0)worlds[trial].ApplyLinearImpulse(arms[trial],{2,0,0});};
        worlds[trial].Step(dt,&material);
    }
    worlds[2].Wake(bases[2]);worlds[2].ApplyLinearImpulse(arms[2],{2,0,0});worlds[2].Step(dt);
    Check(beganSleeping[0]&&beganSleeping[1]&&rounds[0]==10&&rounds[1]==10,"material callback reaches unchanged velocity rounds before its authored late impulse");
    Check(worlds[0].IsSleeping(bases[0])&&worlds[0].IsSleeping(arms[0])&&worlds[0].GetLinearVelocity(arms[0])==glm::vec3(0),"no-wake material control leaves genuinely inactive mechanisms sleeping");
    bool restrained=true,matched=true;
    for(unsigned member=0;member<2;++member){const auto late=member?arms[1]:bases[1],early=member?arms[2]:bases[2];const glm::vec3 initial=member?glm::vec3(1,3,0):glm::vec3(0,3,0);
        restrained&=!worlds[1].IsSleeping(late)&&glm::length(worlds[1].GetTransform(late).position-initial)<.001f&&glm::length(worlds[1].GetLinearVelocity(late))<.02f;
        matched&=glm::length(worlds[1].GetTransform(late).position-worlds[2].GetTransform(early).position)<2e-5f&&glm::length(worlds[1].GetLinearVelocity(late)-worlds[2].GetLinearVelocity(early))<2e-5f;
    }
    JointState anchor;worlds[1].GetJoint(anchors[1],anchor);
    Check(restrained&&anchor.reactionImpulse.y>.45f,"late-woken articulation receives deferred gravity and actual joint restraint in the same physical boundary");
    Check(matched,"late wake matches ordinary pre-step wake at actual body endpoints without replaying warm-start rows");
}

}
int main(){
    try{ClockAndEligibility();IndependentAssemblies();DrivenAndDisabledJoints();ContactAndSlotLifetime();BoundedDiagnostics();SimultaneousImpactDiagnostics();LateWakeVelocityCoupling();}
    catch(const std::exception& e){Check(false,e.what());}
    std::printf("Physics scaling semantics: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
