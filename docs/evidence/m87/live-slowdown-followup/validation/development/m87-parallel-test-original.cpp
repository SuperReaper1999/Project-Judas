// M87: parallel geometry produces the original ordered physical observations.
// No timing threshold, contact-quality change or special scene path is used.
#include "PhysicsWorld.h"
#include "JobSystem.h"
#include "PerformanceProfiler.h"
#include "CollisionAsset.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include "SaveArchive.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
unsigned checks=0,failures=0;
constexpr float dt=1.f/60;
void Check(bool value,const char* name){++checks;if(!value){++failures;std::printf("FAIL %s\n",name);}}
void Require(bool value,const std::string& name){if(!value)throw std::runtime_error(name);}
struct Hash {
    uint64_t value=14695981039346656037ull;
    void Word(uint32_t x){for(unsigned i=0;i<4;++i){value^=(x>>(8*i))&255u;value*=1099511628211ull;}}
    void Float(float x){uint32_t bits;std::memcpy(&bits,&x,sizeof(bits));Word(bits);}
    void Vector(glm::vec3 x){Float(x.x);Float(x.y);Float(x.z);}
    void Bytes(const std::string& x){Word(uint32_t(x.size()));for(unsigned char c:x){value^=c;value*=1099511628211ull;}}
};
uint64_t PhysicalState(PhysicsWorld& world,bool observations=true){
    Hash h;auto alive=world.AliveBodies();std::sort(alive.begin(),alive.end(),[](auto a,auto b){return a.id<b.id;});
    for(auto body:alive){h.Word(body.id);const auto pose=world.GetTransform(body);h.Vector(pose.position);
        h.Float(pose.rotation.w);h.Float(pose.rotation.x);h.Float(pose.rotation.y);h.Float(pose.rotation.z);
        h.Vector(world.GetLinearVelocity(body));h.Vector(world.GetAngularVelocity(body));
        SaveArchive settling;world.PersistSleep(settling,body);h.Bytes(settling.bytes);
    }
    if(observations)for(const auto& c:world.LastStepContacts()){h.Vector(c.point);h.Vector(c.normal);h.Float(c.penetration);}
    if(observations)for(const auto& e:world.LastStepTouchEvents()){h.Word(e.a.id);h.Word(e.b.id);h.Word(unsigned(e.phase));h.Word(e.sensor);
        h.Vector(e.point);h.Vector(e.normal);h.Vector(e.relativeVelocity);h.Word(e.impulseAvailable);h.Float(e.normalImpulse);}
    return h.value;
}
double Counter(const char* name){const auto history=PerformanceProfiler::Get().History();if(history.empty())return -1;
    for(const auto& counter:history.back().counters)if(counter.name==name)return counter.value;
    return -1;
}
void Measured(const std::function<void()>& step){ProfileFrame frame("parallel narrowphase test");step();}
void GravityStep(PhysicsWorld& world,glm::vec3 gravity={0,-9.81f,0}){
    for(auto body:world.AliveBodies())if(world.IsDynamicBody(body))world.ApplyLinearAcceleration(body,gravity,dt);
    world.Step(dt);
}
bool SameGeometryDiagnostics(const PhysicsWorld& a,const PhysicsWorld& b){const auto& x=a.LastStepStats();const auto& y=b.LastStepStats();
    return x.geometryPredicates==y.geometryPredicates&&x.geometryExactFallbacks==y.geometryExactFallbacks&&
        x.geometryUnresolved==y.geometryUnresolved&&x.geometryNumericGapFallbacks==y.geometryNumericGapFallbacks;
}
MeshData Cube(){MeshData mesh;for(int z:{-1,1})for(int y:{-1,1})for(int x:{-1,1}){MeshVertex v;v.position={x*.25f,y*.25f,z*.25f};mesh.vertices.push_back(v);}
    mesh.indices={0,4,6,0,6,2,1,3,7,1,7,5,0,1,5,0,5,4,2,6,7,2,7,3,0,2,3,0,3,1,4,5,7,4,7,6};return mesh;
}
struct Fixture {
    PhysicsWorld world;std::vector<BodyHandle> dynamic;BodyHandle floor,query;
    glm::quat rotation;glm::vec3 gravity;
    Fixture(JobSystem* jobs,unsigned lanes,bool rotated=false,bool mixed=false,bool spheres=true):
        rotation(rotated?glm::angleAxis(.67f,glm::normalize(glm::vec3(1,2,-3))):glm::quat(1,0,0,0)),gravity(rotation*glm::vec3(0,-9.81f,0)){
        world.Init();world.SetJobSystem(jobs,lanes);
        floor=world.CreateShape(Shape::Box({32,.25f,32}),{rotation*glm::vec3(0,-.25f,0),rotation},false,0,.6f,0);
        // 120 independent pairs: real body/body plus body/floor contacts,
        // comfortably above the generic 256-pair batching threshold.
        for(unsigned i=0;i<240;++i){const auto cell=i/2;const glm::vec3 p{float(cell%10)*2+(i%2?.25f:-.25f),.25f,float(cell/10)*2};
            auto shape=spheres&&i%3==0?Shape::Sphere(.25f):Shape::Box(glm::vec3(.25f));
            dynamic.push_back(world.CreateShape(shape,{rotation*p,rotation},true,1,.6f,0));
        }
        if(mixed){
            std::vector<CompoundBox> parts{{{-.15f,0,0},{.1f,.2f,.2f}},{{.15f,0,0},{.1f,.2f,.2f}}};
            dynamic.push_back(world.CreateShape(Shape::Compound(parts),{rotation*glm::vec3(-4,.2f,0),rotation},true,1,.6f,0));
            world.CreateShape(Shape::Capsule(.2f,.2f),{rotation*glm::vec3(-4,.4f,3),rotation},false,0,.6f,0);
            dynamic.push_back(world.CreateShape(Shape::Sphere(.25f),{rotation*glm::vec3(-4,1.05f,3),rotation},true,1,.6f,0));
            CollisionCookSettings settings;settings.convex=true;CollisionAsset convex;std::string error;
            Require(CookCollision(Cube(),settings,convex,error),"mixed convex cook: "+error);
            dynamic.push_back(world.CreateShape(Shape::Cooked(std::make_shared<CollisionAsset>(convex),"parallel convex"),{rotation*glm::vec3(-7,.25f,0),rotation},true,1,.6f,0));
            MeshData mesh;for(auto p:{glm::vec3(-1,0,-1),glm::vec3(1,0,-1),glm::vec3(1,0,1),glm::vec3(-1,0,1)}){MeshVertex v;v.position=p;mesh.vertices.push_back(v);}mesh.indices={0,2,1,0,3,2};
            settings.convex=false;CollisionAsset cooked;Require(CookCollision(mesh,settings,cooked,error),"mixed mesh cook: "+error);
            world.CreateShape(Shape::Cooked(std::make_shared<CollisionAsset>(cooked),"parallel static mesh"),{rotation*glm::vec3(-10,1,0),rotation},false,0,.6f,0);
            dynamic.push_back(world.CreateShape(Shape::Box(glm::vec3(.25f)),{rotation*glm::vec3(-10,1.25f,0),rotation},true,1,.6f,0));
            world.SetCollisionFilter(dynamic[0],1,CategoryBit(1));world.SetBodySensor(dynamic[2],true);world.SetBodyEnabled(dynamic[4],false);
            world.SetPairCollisionEnabled(dynamic[6],dynamic[7],false);world.SetBodyTags(dynamic[10],CategoryBit(9));
            query=world.CreateQueryCapsule(.15f,.2f,{rotation*glm::vec3(-12,.35f,0),rotation});
        }
    }
    void Tick(){GravityStep(world,gravity);}
};
void ExactParallel(JobSystem& jobs,unsigned lanes,bool mixed){
    Fixture serial(nullptr,1,mixed,mixed),parallel(&jobs,lanes,mixed,mixed);
    bool identical=true,diagnostics=true,positive=false,fallBack=false;
    for(unsigned step=0;step<8;++step){
        if(step==3){const auto impulse=serial.rotation*glm::vec3(.12f,.04f,.08f);serial.world.ApplyLinearImpulse(serial.dynamic[10],impulse);parallel.world.ApplyLinearImpulse(parallel.dynamic[10],impulse);}
        Measured([&]{serial.Tick();});Measured([&]{parallel.Tick();});
        identical&=PhysicalState(serial.world)==PhysicalState(parallel.world);diagnostics&=SameGeometryDiagnostics(serial.world,parallel.world);
        positive|=Counter("Physics initial geometry worker lanes")==lanes&&Counter("Physics parallel initial manifolds")>=256;
        if(mixed)fallBack|=Counter("Physics cooked convex tests")>0;
    }
    Check(identical,mixed?"mixed/rotated parallel paths retain exact physical/contact/sleep endpoints":"2/4-lane parallel trajectory and ordered contact observations match serial exactly");
    Check(diagnostics,"worker TLS geometry diagnostics are neither missing nor double-counted");
    Check(positive&&jobs.Stats().submitted>0,"real eligible collision workload submits bounded jobs and reports the active path");
    if(mixed){
        Check(fallBack,"cooked owner fallback genuinely performs contact geometry alongside parallel primitives");
        Check(glm::dot(parallel.world.GetTransform(parallel.dynamic[0]).position-serial.rotation*glm::vec3(-.25f,.25f,0),glm::normalize(serial.gravity))>.01f,"excluded collision layer really falls through support");
        bool suppressed=true;for(const auto& e:parallel.world.LastStepTouchEvents())if((e.a.id==parallel.dynamic[6].id&&e.b.id==parallel.dynamic[7].id)||(e.b.id==parallel.dynamic[6].id&&e.a.id==parallel.dynamic[7].id))suppressed=false;
        Check(suppressed,"generation-aware suppressed pair does not gain events from prefetch");
    }
    const auto oldA=serial.dynamic[12],oldB=parallel.dynamic[12];const auto pose=serial.world.GetTransform(oldA);
    serial.world.DestroyBody(oldA);parallel.world.DestroyBody(oldB);
    serial.dynamic[12]=serial.world.CreateShape(Shape::Sphere(.25f),{pose.position+serial.rotation*glm::vec3(0,2,0),pose.rotation},true,1,.6f,0);
    parallel.dynamic[12]=parallel.world.CreateShape(Shape::Sphere(.25f),{pose.position+parallel.rotation*glm::vec3(0,2,0),pose.rotation},true,1,.6f,0);
    Shape ignored;BodyTransform ignoredPose;
    Check(serial.dynamic[12].id!=oldA.id&&parallel.dynamic[12].id!=oldB.id&&!parallel.world.GetBodyShape(oldB,ignored,ignoredPose),"slot reuse cannot retain stale prefetched body/shape identity");
    for(unsigned i=0;i<4;++i){serial.Tick();parallel.Tick();identical&=PhysicalState(serial.world)==PhysicalState(parallel.world);}
    Check(identical,"reused-slot geometry rebuild preserves subsequent exact physical endpoints");
}
void Sleeping(JobSystem& jobs){
    Fixture serial(nullptr,1,false,false,false),parallel(&jobs,4,false,false,false);bool same=true;
    for(unsigned i=0;i<60;++i){serial.Tick();Measured([&]{parallel.Tick();});same&=PhysicalState(serial.world)==PhysicalState(parallel.world);}
    const auto asleep=parallel.world.LastStepStats().sleepingBodies;
    Check(same&&asleep==240,"all resting bodies naturally sleep with exactly the original quiet clocks");
    Check(Counter("Physics parallel initial manifolds")==0,"inactive sleeping assemblies do not receive unnecessary geometry jobs");
    const auto start=parallel.world.GetTransform(parallel.dynamic[10]).position;
    serial.world.ApplyLinearImpulse(serial.dynamic[10],{1,0,0});parallel.world.ApplyLinearImpulse(parallel.dynamic[10],{1,0,0});
    for(unsigned i=0;i<4;++i){serial.Tick();parallel.Tick();same&=PhysicalState(serial.world)==PhysicalState(parallel.world);}
    Check(same&&!parallel.world.IsSleeping(parallel.dynamic[10])&&glm::length(parallel.world.GetTransform(parallel.dynamic[10]).position-start)>.002f,"actual impulse wakes and moves the original physical contact island");
}
void ServiceBoundaries(){
    Fixture serial(nullptr,1),detached(nullptr,4);Measured([&]{serial.Tick();});Measured([&]{detached.Tick();});
    Check(PhysicalState(serial.world)==PhysicalState(detached.world)&&Counter("Physics initial geometry worker lanes")==0,"null borrowed service keeps the original serial result");
    JobSystem stopped(1);stopped.Shutdown();Fixture afterStop(&stopped,4),reference(nullptr,1);reference.Tick();Measured([&]{afterStop.Tick();});
    Check(PhysicalState(reference.world)==PhysicalState(afterStop.world)&&Counter("Physics initial geometry worker lanes")==0&&stopped.Stats().submitted==0,"already-stopped pool safely falls back without submitting or skipping physics");
    JobSystem jobs(1);Fixture one(&jobs,1),oneReference(nullptr,1);oneReference.Tick();Measured([&]{one.Tick();});
    Check(PhysicalState(oneReference.world)==PhysicalState(one.world)&&Counter("Physics initial geometry worker lanes")==0,"one-lane limit keeps the unbatched serial path");
    bool rejected=false;PhysicsWorld::VelocityCoupling material;material.iterations=10;material.begin=[&](float){try{one.world.SetJobSystem(nullptr);}catch(const std::logic_error&){rejected=true;}};
    one.world.Step(dt,&material);Check(rejected,"service ownership cannot change inside an authoritative step callback");
    one.world.SetJobSystem(nullptr);one.world.Step(dt);
    Check(jobs.Stats().running==0,"detaching at a completed boundary leaves no physics jobs alive");
}
void BusyPool(){
    JobSystem jobs(1);std::atomic<bool> release{false},finished{false},rescued{false};
    const auto resource=jobs.Submit([&](JobContext& ctx){while(!release.load()&&!ctx.CancelRequested())std::this_thread::yield();},JobPriority::Normal,"unrelated resource fixture");
    while(jobs.StateOf(resource)!=JobState::Running)std::this_thread::yield();
    Fixture serial(nullptr,1),parallel(&jobs,4);serial.Tick();
    std::thread watchdog([&]{const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!finished.load()&&std::chrono::steady_clock::now()<end)std::this_thread::yield();if(!finished.load()){rescued=true;release=true;}});
    try{Measured([&]{parallel.Tick();});}catch(...){finished=true;release=true;watchdog.join();throw;}
    finished=true;watchdog.join();
    Check(!rescued.load()&&jobs.StateOf(resource)==JobState::Running,"physics finishes while an unrelated resource job remains blocked; no WaitAll or queued-job stall");
    Check(jobs.Stats().cancelled>=1&&PhysicalState(serial.world)==PhysicalState(parallel.world),"only unnecessary queued physics work is cancelled; owner results remain exact");
    release=true;Require(jobs.Wait(resource)==JobState::Completed,"unrelated resource did not complete normally");jobs.Forget(resource);
    const auto failure=jobs.Submit([](JobContext&){throw std::runtime_error("independent resource failure");});
    Check(jobs.Wait(failure)==JobState::Failed,"negative control produces a real failed unrelated worker job");jobs.Forget(failure);
    serial.Tick();parallel.Tick();Check(PhysicalState(serial.world)==PhysicalState(parallel.world),"unrelated worker failure does not poison geometry or authoritative motion");
}
void ConcurrentShutdown(){
    JobSystem jobs(1);const auto blocker=jobs.Submit([](JobContext& ctx){while(!ctx.CancelRequested())std::this_thread::yield();ctx.ReportCancelled();});
    while(jobs.StateOf(blocker)!=JobState::Running)std::this_thread::yield();
    Fixture serial(nullptr,1),parallel(&jobs,4);serial.Tick();std::atomic<bool> sawSubmission{false};
    std::thread shutdown([&]{const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(jobs.Stats().submitted<2&&std::chrono::steady_clock::now()<end)std::this_thread::yield();sawSubmission=jobs.Stats().submitted>=2;jobs.Shutdown();});
    try{parallel.Tick();}catch(...){shutdown.join();throw;}shutdown.join();
    Check(sawSubmission&&jobs.IsShutDown()&&PhysicalState(serial.world)==PhysicalState(parallel.world),"pool shutdown around a submitted batch joins/cancels safely and preserves the actual endpoint");
    serial.Tick();parallel.Tick();Check(PhysicalState(serial.world)==PhysicalState(parallel.world),"subsequent steps continue serially after pool shutdown");
}
void ScratchBound(JobSystem& jobs,unsigned count,bool expectedParallel){
    PhysicsWorld serial,parallel;serial.Init();parallel.Init();parallel.SetJobSystem(&jobs,4);
    for(unsigned i=0;i<count;++i){const glm::vec3 p{float(i%6)*.01f,float((i/6)%6)*.01f,float(i/36)*.01f};serial.CreateDynamicSphere(p,.0005f,1,0,0);parallel.CreateDynamicSphere(p,.0005f,1,0,0);}
    // Fat broadphase boxes overlap; the actual spheres are independently spaced
    // ten radii apart. This exercises the cap without a destructive pile.
    serial.Step(dt);Measured([&]{parallel.Step(dt);});
    const auto pairs=std::size_t(count)*(count-1)/2;
    Check(parallel.LastStepStats().candidatePairs==pairs&&parallel.LastStepContacts().empty()&&PhysicalState(serial)==PhysicalState(parallel),"large candidate-only negative control retains real separation and exact physical state");
    Check(expectedParallel?Counter("Physics parallel initial manifolds")==double(pairs)&&Counter("Physics initial geometry scratch bytes")>0:Counter("Physics parallel initial manifolds")==0&&Counter("Physics initial geometry scratch bytes")==0,"16384-pair prefetch cap bounds storage and selects ordinary serial fallback above the cap");
}
Scene SaveScene(){Scene scene;scene.Settings().name="parallel geometry persistence";
    auto& ground=scene.CreateObject("support");ground.body=SceneBodyComponent{};ground.body->halfExtents={32,.25f,32};ground.body->restitution=0;ground.transform.position={0,-.25f,0};
    for(unsigned i=0;i<240;++i){const unsigned cell=i/2;auto& body=scene.CreateObject("ordinary physical instance");body.body=SceneBodyComponent{};body.body->motion=SceneBodyMotion::Dynamic;body.body->halfExtents=glm::vec3(.25f);body.body->radius=.25f;body.body->shape=i%3?SceneShape::Box:SceneShape::Sphere;body.body->restitution=0;body.transform.position={float(cell%10)*2+(i%2?.25f:-.25f),.25f,float(cell/10)*2};}return scene;
}
void PersistenceAndRuntimeWiring(JobSystem& jobs){
    ResourceManager resources(nullptr,nullptr,&jobs);RuntimeWorld serial,parallel,cold;serial.legacyGameplay=parallel.legacyGameplay=cold.legacyGameplay=false;
    const auto scene=SaveScene();std::string error;
    Require(serial.Build(scene,nullptr,error)&&parallel.Build(scene,&resources,error),"runtime build: "+error);
    for(unsigned i=0;i<4;++i){GravityStep(serial.Physics());Measured([&]{GravityStep(parallel.Physics());});}
    Check(Counter("Physics initial geometry worker lanes")==4&&PhysicalState(serial.Physics())==PhysicalState(parallel.Physics()),"ordinary RuntimeWorld borrows its ResourceManager job service and preserves real contacts");
    std::map<std::string,SaveChunk> serialChunks,chunks;Require(WorldPersistence::Capture(serial,serialChunks,error)&&WorldPersistence::Capture(parallel,chunks,error),"runtime capture: "+error);
    bool same=serialChunks.size()==chunks.size();for(const auto& [key,value]:serialChunks){auto found=chunks.find(key);same&=found!=chunks.end()&&found->second.version==value.version&&found->second.data==value.data;}
    Check(same,"job service, worker scheduling and derived geometry scratch do not change physical save participants");
    Scene saved;Require(WorldPersistence::SceneFrom(chunks,saved,error)&&cold.Build(saved,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(cold,error)&&WorldPersistence::Restore(cold,chunks,error),"cold restore: "+error);
    Check(PhysicalState(parallel.Physics(),false)==PhysicalState(cold.Physics(),false),"fresh runtime reconstructs actual saved physical endpoints independently of worker scratch");
    bool exact=true;for(unsigned i=0;i<4;++i){GravityStep(parallel.Physics());Measured([&]{GravityStep(cold.Physics());});exact&=PhysicalState(parallel.Physics())==PhysicalState(cold.Physics());}
    Check(exact&&Counter("Physics initial geometry worker lanes")==4,"restored runtime rebuilds geometry and continues actual physical motion with its ordinary job service");
}
}
int main(){
    auto& profiler=PerformanceProfiler::Get();profiler.Enable(true);profiler.Clear();
    try{JobSystem jobs(3);ExactParallel(jobs,2,false);ExactParallel(jobs,4,false);ExactParallel(jobs,2,true);ExactParallel(jobs,4,true);Sleeping(jobs);ServiceBoundaries();BusyPool();ConcurrentShutdown();ScratchBound(jobs,180,true);ScratchBound(jobs,184,false);PersistenceAndRuntimeWiring(jobs);
        Check(profiler.Diagnostics().droppedEvents==0&&profiler.Diagnostics().truncatedFrames==0,"bounded batch profiling does not emit per-pair scope floods");}
    catch(const std::exception& error){Check(false,error.what());}
    profiler.Enable(false);std::printf("Parallel narrowphase: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
