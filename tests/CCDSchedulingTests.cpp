// Current production CCD scheduling: exact serial/worker physical contracts.
// No third-party assets, renderer, tolerance widening or timing authority.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cfenv>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#define private public
#include "physics/WorldInternal.h"
#undef private
#include <cstdlib>

namespace {
constexpr float dt=1.f/60;
unsigned checks=0;
void Check(bool condition,const std::string& label){
    ++checks;if(!condition)throw std::runtime_error(label);
}
void Environment(const char* name,const char* value){
#ifdef _WIN32
    if(_putenv_s(name,value?value:"")!=0)throw std::runtime_error("test environment update failed");
#else
    const int result=value?setenv(name,value,1):unsetenv(name);
    if(result)throw std::runtime_error("test environment update failed");
#endif
}
struct SavedEnvironment {
    std::string name,value;bool present=false;
    SavedEnvironment(const char* key,const char* replacement):name(key){
        if(const auto* old=std::getenv(key)){present=true;value=old;}
        Environment(key,replacement);
    }
    ~SavedEnvironment(){
        try{Environment(name.c_str(),present?value.c_str():nullptr);}catch(...){}
    }
};
struct Words {
    std::vector<std::uint32_t> values;
    void Word(std::uint32_t value){values.push_back(value);}
    void Float(float value){
        Check(std::isfinite(value),"physical state is finite");
        std::uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));Word(bits);
    }
    void Double(double value){
        Check(std::isfinite(value),"motion time is finite");
        std::uint64_t bits;std::memcpy(&bits,&value,sizeof(bits));
        Word(std::uint32_t(bits));Word(std::uint32_t(bits>>32));
    }
    void Vector(glm::vec3 value){Float(value.x);Float(value.y);Float(value.z);}
    void Pose(const BodyTransform& value){Vector(value.position);Float(value.rotation.w);
        Float(value.rotation.x);Float(value.rotation.y);Float(value.rotation.z);}
    template<class Bytes>void Serialized(const Bytes& bytes){Word(std::uint32_t(bytes.size()));for(auto byte:bytes)Word(static_cast<unsigned char>(byte));}
    std::uint64_t Hash()const{
        std::uint64_t result=14695981039346656037ull;
        for(auto value:values)for(unsigned i=0;i<4;++i){result^=(value>>(i*8))&255u;result*=1099511628211ull;}
        return result;
    }
};
Words Snapshot(PhysicsWorld& world){
    Words out;auto bodies=world.AliveBodies();
    std::sort(bodies.begin(),bodies.end(),[](auto a,auto b){return a.id<b.id;});
    out.Word(std::uint32_t(bodies.size()));
    for(auto body:bodies){
        out.Word(body.id);out.Pose(world.GetTransform(body));out.Pose(world.GetPreviousTransform(body));
        out.Vector(world.GetLinearVelocity(body));out.Vector(world.GetAngularVelocity(body));
        SaveArchive sleep;world.PersistSleep(sleep,body);out.Serialized(sleep.bytes);
        const auto segments=world.GetBodyMotionSegments(body);out.Word(std::uint32_t(segments.size()));
        for(const auto& leaf:segments){
            out.Double(leaf.begin);out.Double(leaf.end);out.Pose(leaf.start);out.Pose(leaf.endPose);
            out.Vector(leaf.linearVelocity);out.Vector(leaf.angularVelocity);out.Vector(leaf.pivotOffset);
            out.Word(leaf.movable);out.Word(leaf.prescribed);out.Word(leaf.exactRotation);
            out.Word(leaf.anchoredTranslation);out.Vector(leaf.translationOrigin);out.Double(leaf.translationTime);
        }
        std::vector<PhysicsWorld::BodyContactSample> samples;
        Check(world.ReadBodyContactSample(body,samples,64,128),"fixture contact observations fit the declared budget");
        out.Word(std::uint32_t(samples.size()));
        for(const auto& sample:samples){out.Word(sample.other.id);out.Vector(sample.point);
            out.Vector(sample.normal);out.Vector(sample.supportVelocity);out.Float(sample.normalImpulse);
            out.Float(sample.separation);out.Word(sample.sleeping);}
    }
    out.Word(std::uint32_t(world.LastStepContacts().size()));
    for(const auto& contact:world.LastStepContacts()){
        out.Vector(contact.point);out.Vector(contact.normal);out.Float(contact.penetration);
    }
    out.Word(std::uint32_t(world.LastStepTouchEvents().size()));
    for(const auto& event:world.LastStepTouchEvents()){
        out.Word(event.a.id);out.Word(event.b.id);out.Word(unsigned(event.phase));out.Word(event.sensor);
        out.Vector(event.point);out.Vector(event.normal);out.Vector(event.relativeVelocity);
        out.Word(event.impulseAvailable);out.Float(event.normalImpulse);
    }
    // Persisted touch history is observable next-step state, not diagnostic work.
    // Runtime slot/generation ID zero is valid; persistence reserves zero for
    // an absent project identity. This injective mapping retains generation.
    const auto identity=[](BodyHandle body){return std::uint64_t(body.id)+1;};
    const auto unusedResolver=[](std::uint64_t){return BodyHandle{};};
    SaveArchive touches;world.PersistTouches(touches,identity,unusedResolver);out.Serialized(touches.bytes);
    SaveArchive observations;world.PersistCompletedContactObservations(observations,identity,unusedResolver);
    out.Serialized(observations.bytes);
    const auto& stats=world.LastStepStats();
    out.Word(std::uint32_t(stats.impactEvents));out.Word(std::uint32_t(stats.impactSafetyFallback));
    out.Word(std::uint32_t(stats.impactResponseAttempts));out.Word(std::uint32_t(stats.impactResponsesApplied));
    return out;
}
double Counter(const char* name){
    const auto history=PerformanceProfiler::Get().History();
    if(!history.empty())for(const auto& counter:history.back().counters)
        if(counter.name==name)return counter.value;
    return 0;
}
struct Scene {
    PhysicsWorld world;std::vector<BodyHandle> bodies,receivers;
    std::vector<std::pair<BodyHandle,BodyHandle>> recontacts;
    std::vector<std::pair<BodyHandle,BodyHandle>> nearMisses;
    glm::quat frame=glm::angleAxis(.47f,glm::normalize(glm::vec3(1,2,-3)));
    Scene(JobSystem& jobs,unsigned kind){
        Check(world.Init(),"procedural query world initializes");world.SetJobSystem(&jobs,4);
        if(kind>=4)frame=glm::quat(1,0,0,0); // exact axis symmetry in tie controls
        if(kind==8){
            bodies.push_back(world.CreateStaticBox({0,-.5f,0},{3,.5f,3},0,0));
            const auto rotation=glm::angleAxis(-.7853981633974483f,glm::vec3(0,0,1));
            const auto matrix=ContactRotation(rotation);
            const glm::vec3 half(.6f,.08f,.1f);
            const float height=std::nextafter(float(std::abs(matrix[0].y)*half.x+
                std::abs(matrix[1].y)*half.y),-std::numeric_limits<float>::infinity());
            const auto panel=world.CreateShape(Shape::Box(half),{{0,height,0},rotation},true,1,0,0);
            bodies.push_back(panel);world.SetAngularVelocity(panel,{0,0,300});
            const auto m=BoxVsBoxManifold({0,height,0},rotation,half,{0,-.5f,0},glm::quat(1,0,0,0),{3,.5f,3});
            Check(m.count>0&&!m.uncertain,"rotating continuation starts in actual zero-margin contact");
            for(int k=0;k<m.count;++k){
                const auto r=matrix*m.points[k].localAnchorA;
                const auto v=glm::cross(glm::dvec3(0,0,300),r);
                Check(glm::dot(v,glm::dvec3(m.points[k].normal))>=0,
                    "touching rotating control is initially nonapproaching before later deepening");
            }
            return;
        }
        const auto create=[&](glm::vec3 center,unsigned index,glm::quat localRotation,float mass,float restitution){
            const auto rotation=frame*localRotation;
            const glm::vec3 half{.22f,.18f,.16f};
            Shape shape=Shape::Box(half);glm::vec3 offset(0);
            if(index%2&&kind<4){offset={.17f,-.09f,.13f};CompoundBox child;child.localCenter=offset;
                child.halfExtents=half;child.key=index+1;shape=Shape::Compound({child});}
            const auto handle=world.CreateShape(shape,{frame*center-rotation*offset,rotation},true,mass,.4f,restitution);
            bodies.push_back(handle);return handle;
        };
        // Independent corridors provide many simultaneous root queries without
        // depending on the generic 256-manifold worker path. The compounds are
        // genuine offset-child descriptors, rather than renamed centred boxes.
        for(unsigned lane=0;lane<24;++lane){
            // Keep the independent cases isolated even after energetic grazing
            // responses. Case 3 instead packs shared neighbours deliberately.
            const float z=kind==3?(float(lane%6)-2.5f)*.7f:float(lane)*128;
            const float y=kind==3?(float(lane/6)-1.5f)*.7f:0;
            if(kind==4){
                const auto left=create({-.95f,0,z},3*lane,glm::quat(1,0,0,0),1,1);
                create({0,0,z},3*lane+1,glm::quat(1,0,0,0),1,1);
                const auto right=create({.95f,0,z},3*lane+2,glm::quat(1,0,0,0),1,1);
                world.SetLinearVelocity(left,{90,0,0});world.SetLinearVelocity(right,{-90,0,0});
            }else if(kind==5){
                // The struck body and touching receiver have lower keys than
                // the incoming striker: propagated immediate contact reorders.
                create({0,0,z},3*lane,glm::quat(1,0,0,0),1,1);
                receivers.push_back(create({.44f,0,z},3*lane+1,glm::quat(1,0,0,0),1,1));
                const auto striker=create({-.95f,0,z},3*lane+2,glm::quat(1,0,0,0),1,1);
                world.SetLinearVelocity(striker,{90,0,0});
            }else if(kind==2){
                const auto striker=create({-.95f,0,z},3*lane,glm::quat(1,0,0,0),1,1);
                create({0,0,z},3*lane+1,glm::quat(1,0,0,0),1,1);
                receivers.push_back(create({.95f,0,z},3*lane+2,glm::quat(1,0,0,0),1,1));
                world.SetLinearVelocity(striker,frame*glm::vec3(90,0,0));
            }else{
                const bool nearMiss=kind==1&&lane%4==3;
                const float lateral=nearMiss?.9f:kind==1&&lane%4==1?.27f:0.f;
                const auto rotation=kind==1||kind==3?glm::angleAxis(.11f*float(lane%3),glm::vec3(0,0,1)):glm::quat(1,0,0,0);
                const float x=kind==6?.2f:1.2f;
                const float restitution=kind==7?1.f:.2f;
                const auto a=create({-x,y,z},2*lane,rotation,1,restitution);
                const auto b=create({x,y+lateral,z},2*lane+1,rotation,kind==7?1.f:1.3f,restitution);
                world.SetLinearVelocity(a,frame*glm::vec3(70,0,0));
                world.SetLinearVelocity(b,frame*glm::vec3(-70,0,0));
                if(kind==1||kind==3){world.SetAngularVelocity(a,frame*glm::vec3(3,float(7+lane%5),40));
                    world.SetAngularVelocity(b,frame*glm::vec3(-4,-11,-35));}
                if(nearMiss)nearMisses.push_back({a,b});
                if(kind==7)recontacts.push_back({a,b});
            }
        }
    }
};
struct Result {
    std::vector<Words> steps;std::size_t helperJobs=0,helperRoots=0,submitted=0;
    std::size_t lazyDeferred=0,lazyFlushes=0;
    unsigned actualLanes=0,recontactEnters=0;bool response=false,receiverMoved=false;
};
Result Run(JobSystem& jobs,unsigned lanes,unsigned kind){
    const auto laneText=std::to_string(lanes);
    SavedEnvironment laneMode("JUDAS_MK2_QUERY_LANES",laneText.c_str());
    auto& profiler=PerformanceProfiler::Get();profiler.ConfigureEnvironment("CCD scheduling correctness");
    profiler.Clear();profiler.Enable(true);
    const auto before=jobs.Stats();Scene scene(jobs,kind);Result result;
    // Dense pair sharing needs fewer intervals; the independent cases also
    // cover origin translation and generation-reused body slots.
    const unsigned stepCount=kind==3?10:kind==8?4:24;
    for(unsigned step=0;step<stepCount;++step){
        if(step==6&&kind==7)for(const auto& pair:scene.recontacts)for(auto body:{pair.first,pair.second})
            scene.world.ApplyLinearImpulse(body,-2.f*scene.world.GetMass(body)*scene.world.GetLinearVelocity(body));
        if(step==6&&kind!=7)scene.world.ApplyAngularImpulse(scene.bodies[0],scene.frame*glm::vec3(.03f,.02f,-.04f));
        if(step==8)scene.world.ShiftOrigin({128.5,-64.25,32.125});
        if(step==12){
            const auto old=scene.bodies[0];const auto at=scene.world.GetTransform(old);
            scene.world.DestroyBody(old);
            scene.bodies[0]=scene.world.CreateDynamicBox(at.position+scene.frame*glm::vec3(0,1,0),{.2f,.17f,.14f},1,.4f,.2f);
            scene.world.SetLinearVelocity(scene.bodies[0],scene.frame*glm::vec3(4,-3,2));
            Check(scene.bodies[0].id!=old.id&&!scene.world.IsDynamicBody(old),"reused slot rejects stale query identity");
        }
        {ProfileFrame frame("CCD query worker correctness");scene.world.Step(dt);}
        result.helperJobs+=std::size_t(std::max(0.,Counter("Physics CCD query helper jobs")));
        result.helperRoots+=std::size_t(std::max(0.,Counter("Physics CCD query helper roots")));
        result.actualLanes=std::max(result.actualLanes,unsigned(std::max(0.,Counter("Physics CCD query worker lanes"))));
        result.lazyDeferred+=std::size_t(std::max(0.,Counter("Physics CCD lazy deferred futures")));
        result.lazyFlushes+=std::size_t(std::max(0.,Counter("Physics CCD lazy immediate flushes")));
        result.response|=scene.world.LastStepStats().impactResponsesApplied>0||scene.world.LastStepContactCount()>0;
        if(!scene.recontacts.empty())for(const auto& event:scene.world.LastStepTouchEvents()){
            const auto pair=scene.recontacts.front();
            if(event.phase==PhysicsWorld::TouchPhase::Enter&&
               ((event.a.id==pair.first.id&&event.b.id==pair.second.id)||
                (event.a.id==pair.second.id&&event.b.id==pair.first.id)))++result.recontactEnters;
        }
        for(const auto& event:scene.world.LastStepTouchEvents())for(const auto& pair:scene.nearMisses)
            Check(!((event.a.id==pair.first.id&&event.b.id==pair.second.id)||
                    (event.a.id==pair.second.id&&event.b.id==pair.first.id)),"a certified near miss never becomes a collision event");
        for(auto receiver:scene.receivers)result.receiverMoved|=glm::length(scene.world.GetLinearVelocity(receiver))>1;
        result.steps.push_back(Snapshot(scene.world));
    }
    const auto after=jobs.Stats();result.submitted=std::size_t(after.submitted-before.submitted);
    Check(after.running==0&&after.queued==0,"all query helpers are joined before the completed boundary");
    Check(after.failed==before.failed,"no worker query failed silently");
    Check(result.response,"the fixture produces real physical collision responses");
    if(kind==2||kind==5)Check(result.receiverMoved,"newly accelerated bodies reach receivers beyond their old stationary futures");
    if(kind==7)Check(result.recontactEnters>=2,"physical separation and later recontact both occurred");
    const auto diagnostics=profiler.Diagnostics();
    Check(diagnostics.droppedEvents==0&&diagnostics.truncatedFrames==0,
          "bounded fixture profiling retains actual helper diagnostics");
    std::printf("CCD-SCHEDULING case=%u lanes=%u actual=%u helper_jobs=%zu helper_roots=%zu deferred=%zu flushes=%zu final_hash=%016llx\n",
        kind,lanes,result.actualLanes,result.helperJobs,result.helperRoots,result.lazyDeferred,result.lazyFlushes,
        static_cast<unsigned long long>(result.steps.back().Hash()));
    return result;
}

using FrozenWorld=PhysicsWorld::Impl;
unsigned FrozenSlot(BodyHandle handle){return handle.id&FrozenWorld::kSlotMask;}
struct FrozenResult {
    Words state;std::size_t deferred=0,flushes=0;
};
FrozenResult FrozenControl(JobSystem& jobs,unsigned lanes,unsigned kind){
    const auto laneText=std::to_string(lanes);
    SavedEnvironment laneMode("JUDAS_MK2_QUERY_LANES",laneText.c_str());
    auto& profiler=PerformanceProfiler::Get();profiler.ConfigureEnvironment("CCD frozen scheduling correctness");
    profiler.Clear();profiler.Enable(true);
    PhysicsWorld physics;Check(physics.Init(),"frozen procedural world initializes");
    physics.SetJobSystem(&jobs,4);
    auto& world=*physics.m_impl;
    const auto box=[&](glm::vec3 position,glm::vec3 velocity,float restitution=1.f){
        const auto body=physics.CreateDynamicBox(position,{.22f,.18f,.16f},1,0,restitution);
        physics.SetLinearVelocity(body,velocity);return body;
    };
    BodyHandle separatedA{},separatedB{},receiver{},rotatingPanel{},rotatingFloor{};
    if(kind==0){
        // A one-binary32-ULP gap is a real, very near future arrival. The
        // unrelated later event must never be chosen before this deferred root.
        separatedA=box({0,0,128},{0,0,0});
        separatedB=box({std::nextafter(.44f,std::numeric_limits<float>::infinity()),0,128},{-70,0,0});
        box({0,0,256},{0,0,0});box({.66f,0,256},{-70,0,0});
        box({0,0,0},{70,0,0});box({.43f,0,0},{-70,0,0});
    }else if(kind==1){
        // New lower-key pair (center,receiver) becomes approaching at time0
        // only after higher-key striker/center response has propagated load.
        box({0,0,0},{0,0,0});receiver=box({.44f,0,0},{0,0,0});
        box({-.44f,0,0},{70,0,0});
        box({0,0,128},{0,0,0});box({.66f,0,128},{-70,0,0});
    }else if(kind==2){
        // Both arrivals are initially immediate. The first impulse changes the
        // other queued pair, requiring its exact new trajectory observation.
        box({-.44f,0,0},{70,0,0});box({0,0,0},{0,0,0});box({.44f,0,0},{-70,0,0});
        box({0,0,128},{0,0,0});box({.66f,0,128},{-70,0,0});
    }else if(kind==3){
        // A touching but initially opening rotating box later deepens. Its
        // continuation cannot disappear when an unrelated immediate pair runs.
        rotatingFloor=physics.CreateStaticBox({0,-.5f,0},{3,.5f,3},0,0);
        const auto rotation=glm::angleAxis(-.7853981633974483f,glm::vec3(0,0,1));
        const auto matrix=ContactRotation(rotation);const glm::vec3 half(.6f,.08f,.1f);
        const float height=std::nextafter(float(std::abs(matrix[0].y)*half.x+
            std::abs(matrix[1].y)*half.y),-std::numeric_limits<float>::infinity());
        const auto panel=physics.CreateShape(Shape::Box(half),{{0,height,0},rotation},true,1,0,0);
        rotatingPanel=panel;physics.SetAngularVelocity(panel,{0,0,300});
        const auto m=BoxVsBoxManifold({0,height,0},rotation,half,{0,-.5f,0},glm::quat(1,0,0,0),{3,.5f,3});
        Check(m.count>0&&!m.uncertain,"frozen rotating control begins touching");
        for(int k=0;k<m.count;++k)
            Check(glm::dot(glm::cross(glm::dvec3(0,0,300),matrix*m.points[k].localAnchorA),
                  glm::dvec3(m.points[k].normal))>=0,"frozen rotating contact initially opens");
        box({0,0,128},{70,0,0});box({.43f,0,128},{-70,0,0});
    }else{
        // A redirected body's newly possible arrival precedes every previously
        // queued future event, despite that pair's earlier stationary future.
        box({0,0,0},{0,0,0});receiver=box({.4401f,0,0},{0,0,0});
        box({-.44f,0,0},{70,0,0});
        box({0,0,128},{0,0,0});box({.66f,0,128},{-70,0,0});
    }
    // Deliberately begin at the CCD boundary, bypassing native initial rows
    // which would otherwise consume these precise immediate/future controls.
    world.work={};world.work.active=true;world.stats={};world.stepDuration=dt;
    for(auto slot:world.aliveSlots){
        auto& body=world.bodies[slot];const auto initial=body.rigidBody;
        body.motion.Begin(world.MakeHandle(slot).id,initial,dt,true);
        body.rigidBody=body.motion.Evaluate(dt);world.InvalidateImpactPose(slot);
    }
    if(kind==0){
        const auto a=FrozenSlot(separatedA),b=FrozenSlot(separatedB);
        Check(!world.PairTouches(a,b,0,0,0),"one-ULP gap is genuinely separated at the current timestamp");
        const auto hit=world.ImpactTime(a,b,0,0,0,dt);
        Check(hit>0&&hit<dt,"represented near-future root is strictly after now and inside this interval");
        std::printf("FROZEN-NEAR-FUTURE hit=%.17g phase_dt=%.17g\n",hit,double(dt));
    }
    {
        ProfileFrame frame("CCD frozen scheduling correctness");
        world.Mk2Continuous(dt);world.FinishTouches();
    }
    Check(world.stats.impactResponsesApplied>0,"frozen controls perform actual collision response");
    if(receiver.IsValid())Check(glm::length(physics.GetLinearVelocity(receiver))>1,
        "changed future actually reaches the previously stationary receiver");
    if(rotatingPanel.IsValid()){
        const auto a=std::min(FrozenSlot(rotatingPanel),FrozenSlot(rotatingFloor));
        const auto b=std::max(FrozenSlot(rotatingPanel),FrozenSlot(rotatingFloor));
        Check(std::any_of(world.mk2ImpactObservations.begin(),world.mk2ImpactObservations.end(),
            [&](const auto& row){return row.key.slotA==a&&row.key.slotB==b&&row.normalImpulse>0;}),
            "initially opening rotation later produces its own physical contact impulse");
    }
    FrozenResult result;result.state=Snapshot(physics);
    // Direct CCD controls have not run outer native publication, so include
    // their actual impulse observations explicitly instead of losing them.
    result.state.Word(std::uint32_t(world.mk2ImpactObservations.size()));
    for(const auto& row:world.mk2ImpactObservations){
        result.state.Word(row.key.slotA);result.state.Word(row.key.slotB);
        result.state.Word(row.key.generationA);result.state.Word(row.key.generationB);
        result.state.Word(std::uint32_t(row.key.partA));result.state.Word(std::uint32_t(row.key.partB));
        result.state.Double(row.localAnchorA.x);result.state.Double(row.localAnchorA.y);result.state.Double(row.localAnchorA.z);
        result.state.Vector(row.normal);result.state.Float(row.normalImpulse);
        result.state.Vector(row.tangentImpulse);result.state.Float(row.separation);
    }
    result.deferred=std::size_t(std::max(0.,Counter("Physics CCD lazy deferred futures")));
    result.flushes=std::size_t(std::max(0.,Counter("Physics CCD lazy immediate flushes")));
    const auto diagnostics=profiler.Diagnostics();
    Check(diagnostics.droppedEvents==0&&diagnostics.truncatedFrames==0,"frozen control diagnostics are complete");
    const auto jobState=jobs.Stats();Check(jobState.running==0&&jobState.queued==0,"frozen helpers join before observation");
    std::printf("FROZEN-SCHEDULING case=%u lanes=%u deferred=%zu flushes=%zu hash=%016llx\n",
        kind,lanes,result.deferred,result.flushes,static_cast<unsigned long long>(result.state.Hash()));
    return result;
}
}
int main(){try{
    // Current production algorithms are authoritative. No historical eager
    // solver or contact-cadence switch is retained just for this regression.
    SavedEnvironment depth("JUDAS_PROFILE_DEPTH","1");
    JobSystem jobs(3);
    std::array<std::size_t,5> helperCoverage{};
    std::size_t lazyDeferred=0,lazyFlushes=0;
    for(unsigned kind=0;kind<9;++kind){
        const auto reference=Run(jobs,1,kind);
        Check(reference.helperJobs==0&&reference.helperRoots==0,"one lane retains serial geometry publication");
        lazyDeferred+=reference.lazyDeferred;lazyFlushes+=reference.lazyFlushes;
        for(unsigned lanes:{2u,4u})for(unsigned repeat=0;repeat<2;++repeat){
            const auto current=Run(jobs,lanes,kind);
            Check(current.steps.size()==reference.steps.size(),"matched runs cover the same fixed intervals");
            for(std::size_t step=0;step<reference.steps.size();++step){
                if(current.steps[step].values!=reference.steps[step].values)
                    throw std::runtime_error("query workers changed exact physical/contact/motion/history state at case "+
                        std::to_string(kind)+" step "+std::to_string(step)+" lanes "+std::to_string(lanes));
                Check(current.steps[step].Hash()==reference.steps[step].Hash(),"exact per-step physical hash matches current serial scheduling");
            }
            if(current.helperJobs&&current.helperRoots&&current.submitted&&current.actualLanes==lanes)
                ++helperCoverage[lanes];
            lazyDeferred+=current.lazyDeferred;lazyFlushes+=current.lazyFlushes;
        }
    }
    for(unsigned kind=0;kind<5;++kind){
        const auto reference=FrozenControl(jobs,1,kind);
        lazyDeferred+=reference.deferred;lazyFlushes+=reference.flushes;
        for(unsigned lanes:{2u,4u})for(unsigned repeat=0;repeat<2;++repeat){
            const auto current=FrozenControl(jobs,lanes,kind);
            Check(current.state.values==reference.state.values,
                "frozen immediate/future ordering preserves exact physical and impulse state across workers");
            lazyDeferred+=current.deferred;lazyFlushes+=current.flushes;
        }
    }
    // Overlap-only controls need not expose independent work. Require genuine
    // helpers in representative suite cases, rather than in every tiny scene.
    for(unsigned lanes:{2u,4u})
        Check(helperCoverage[lanes]>0,"actual helper roots cover two/four-lane query configurations");
    Check(lazyDeferred>0&&lazyFlushes>0,"production scheduling actually retained future roots while immediate arrivals ran");
    std::printf("SUMMARY checks=%u failures=0 deferred=%zu immediate_flushes=%zu\n",checks,lazyDeferred,lazyFlushes);return 0;
}catch(const std::exception& error){std::fprintf(stderr,"FAIL after %u checks: %s\n",checks,error.what());return 1;}}
