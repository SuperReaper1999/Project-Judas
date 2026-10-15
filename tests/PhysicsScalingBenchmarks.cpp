// M87 identical-workload physics measurements. This driver contains no alternate
// solver, quality adjustment, broadphase or scene-name branch. Timing gates are
// deliberately absent: physical checks and exact endpoint digests accompany raw
// costs so faster but missing simulation cannot be presented as an improvement.
#include "CollisionAsset.h"
#include "CollisionGeometry.h"
#include "PerformanceProfiler.h"
#include "PhysicsWorld.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <cstdlib>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <malloc.h>
#endif
#if defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#endif

// Test executable only: count requested heap bytes on the calling simulation
// thread, with no production allocator replacement. Counts intentionally exclude
// other worker threads; the current PhysicsWorld::Step is owner-thread serial.
namespace M87Allocation {
#if defined(M87_ALLOCATION_DIAGNOSTICS)
constexpr bool available=true;
#else
constexpr bool available=false;
#endif
thread_local bool enabled=false;
thread_local std::uint64_t calls=0,bytes=0;
void Count(std::size_t n){if(enabled){++calls;bytes+=n;}}
void* Allocate(std::size_t n){Count(n);if(auto* p=std::malloc(std::max<std::size_t>(n,1)))return p;throw std::bad_alloc();}
void* Aligned(std::size_t n,std::size_t alignment){
    Count(n);
#if defined(_WIN32)
    auto* p=_aligned_malloc(std::max<std::size_t>(n,1),alignment);
#else
    void* p=nullptr;if(posix_memalign(&p,alignment,std::max<std::size_t>(n,1)))p=nullptr;
#endif
    if(p)return p;
    throw std::bad_alloc();
}
void AlignedFree(void* p){
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
}
#if defined(M87_ALLOCATION_DIAGNOSTICS)
void* operator new(std::size_t n){return M87Allocation::Allocate(n);}
void* operator new[](std::size_t n){return M87Allocation::Allocate(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
void* operator new(std::size_t n,const std::nothrow_t&)noexcept{try{return M87Allocation::Allocate(n);}catch(...){return nullptr;}}
void* operator new[](std::size_t n,const std::nothrow_t&)noexcept{try{return M87Allocation::Allocate(n);}catch(...){return nullptr;}}
void operator delete(void* p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void* p,const std::nothrow_t&)noexcept{std::free(p);}
void* operator new(std::size_t n,std::align_val_t a){return M87Allocation::Aligned(n,std::size_t(a));}
void* operator new[](std::size_t n,std::align_val_t a){return M87Allocation::Aligned(n,std::size_t(a));}
void operator delete(void* p,std::align_val_t)noexcept{M87Allocation::AlignedFree(p);}
void operator delete[](void* p,std::align_val_t)noexcept{M87Allocation::AlignedFree(p);}
void operator delete(void* p,std::size_t,std::align_val_t)noexcept{M87Allocation::AlignedFree(p);}
void operator delete[](void* p,std::size_t,std::align_val_t)noexcept{M87Allocation::AlignedFree(p);}
void* operator new(std::size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return M87Allocation::Aligned(n,std::size_t(a));}catch(...){return nullptr;}}
void* operator new[](std::size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return M87Allocation::Aligned(n,std::size_t(a));}catch(...){return nullptr;}}
void operator delete(void* p,std::align_val_t,const std::nothrow_t&)noexcept{M87Allocation::AlignedFree(p);}
void operator delete[](void* p,std::align_val_t,const std::nothrow_t&)noexcept{M87Allocation::AlignedFree(p);}
#endif

namespace {
using Clock=std::chrono::steady_clock;
using Json=nlohmann::json;
constexpr float kDt=1.f/60.f;
struct Options {
    std::string workload="pile";
    std::filesystem::path output;
    unsigned count=100,warm=60,steps=120;
    bool profile=true,sleep=true,rotated=false,noContact=false,noBodyContact=false,freeze=false;
};
Options Arguments(int argc,char** argv){
    Options o;
    for(int i=1;i<argc;++i){
        const std::string key=argv[i];
        auto value=[&](){if(++i==argc)throw std::invalid_argument("missing argument for "+key);return std::string(argv[i]);};
        if(key=="--workload")o.workload=value();
        else if(key=="--output")o.output=value();
        else if(key=="--count")o.count=unsigned(std::stoul(value()));
        else if(key=="--machines")o.count=4*unsigned(std::stoul(value()));
        else if(key=="--warm")o.warm=unsigned(std::stoul(value()));
        else if(key=="--steps")o.steps=unsigned(std::stoul(value()));
        else if(key=="--profile")o.profile=std::stoul(value())!=0;
        else if(key=="--sleep")o.sleep=std::stoul(value())!=0;
        else if(key=="--rotated")o.rotated=true;
        else if(key=="--negative-no-contact")o.noContact=true;
        else if(key=="--negative-no-body-contact")o.noBodyContact=true;
        else if(key=="--negative-freeze")o.freeze=true;
        else throw std::invalid_argument("unknown argument "+key);
    }
    if(o.output.empty()||o.count<4||o.count>10000||!o.steps||o.steps>3600||o.warm>3600)
        throw std::invalid_argument("require --output DIR, count4..10000, steps1..3600, warm0..3600");
    if(o.workload!="free"&&o.workload!="pile"&&o.workload!="mesh"&&o.workload!="machines_active"&&o.workload!="machines_sleeping")
        throw std::invalid_argument("unknown workload");
    return o;
}
double Ms(Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();}
double Quantile(std::vector<double> values,double q){if(values.empty())return 0;std::sort(values.begin(),values.end());return values[std::size_t(q*double(values.size()-1))];}
Json Distribution(const std::vector<double>& v){return {{"median",Quantile(v,.5)},{"p95",Quantile(v,.95)},{"maximum",Quantile(v,1)}};}
std::uint64_t Resident(){
#if defined(__linux__)
    std::ifstream stream("/proc/self/statm");std::uint64_t total=0,resident=0;stream>>total>>resident;return resident*std::uint64_t(sysconf(_SC_PAGESIZE));
#else
    return 0; // Explicitly unavailable, never presented as measured zero bytes.
#endif
}
std::uint64_t PeakResident(){
#if defined(__linux__)
    struct rusage usage{};getrusage(RUSAGE_SELF,&usage);return std::uint64_t(usage.ru_maxrss)*1024;
#else
    return 0;
#endif
}
void Mix(std::uint64_t& digest,float value){std::uint32_t bits=0;std::memcpy(&bits,&value,sizeof bits);for(unsigned shift=0;shift<32;shift+=8){digest^=(bits>>shift)&255u;digest*=1099511628211ull;}}
Json Vector(glm::vec3 v){return Json::array({v.x,v.y,v.z});}
Json Quaternion(glm::quat q){return Json::array({q.w,q.x,q.y,q.z});}
struct Body {BodyHandle handle;glm::vec3 initial,half,initialVelocity{0};bool sphere=false;};
struct Fixture {
    PhysicsWorld world;
    std::vector<Body> bodies;
    std::vector<JointHandle> joints;
    std::shared_ptr<CollisionAsset> mesh;
    glm::quat rotation{1,0,0,0};
    glm::vec3 gravity{0,-9.81f,0};
    unsigned triangles=0,machines=0;
    bool hasFloor=true;
};
float Height(float x,float z){return .06f*std::sin(.31f*x)*std::cos(.27f*z);}
void CreateFixture(Fixture& f,const Options& o){
    if(!f.world.Init())throw std::runtime_error("physics initialization failed");
    f.world.SetSleepingEnabled(o.sleep);
    if(o.rotated)f.rotation=glm::angleAxis(.93f,glm::normalize(glm::vec3(.31f,.79f,-.23f)));
    f.gravity=f.rotation*glm::vec3(0,-9.81f,0);
    const bool machine=o.workload.find("machines_")==0;
    const unsigned columns=unsigned(std::ceil(std::sqrt(double(machine?(o.count+3)/4:o.count))));
    auto create=[&](glm::vec3 p,glm::vec3 half,bool sphere,float mass,glm::quat local=glm::quat(1,0,0,0)){
        const auto at=f.rotation*p;
        const auto h=sphere?f.world.CreateDynamicSphere(at,half.x,mass,.55f,.02f):f.world.CreateDynamicBox(at,half,mass,.55f,.02f);
        f.world.ResetBody(h,at,f.rotation*local);
        if(o.noContact)f.world.SetCollisionFilter(h,0,0);
        else if(o.noBodyContact)f.world.SetCollisionFilter(h,0,CategoryMask{1}<<1);
        f.bodies.push_back({h,p,half,glm::vec3(0),sphere});return h;
    };
    if(o.workload=="free"){
        f.hasFloor=false;f.gravity={0,0,0};
        for(unsigned i=0;i<o.count;++i){
            const glm::vec3 p{float(i%columns)*3,2,float(i/columns)*3};
            const auto h=create(p,{.22f,.22f,.22f},i%2==0,2);
            // All bodies translate identically, so none can begin colliding as N grows.
            f.bodies.back().initialVelocity={.15f,.07f,-.04f};
            f.world.SetLinearVelocity(h,f.rotation*f.bodies.back().initialVelocity);
        }
    }else if(o.workload=="mesh"){
        const unsigned cells=columns*3+6;const float spacing=.4f;
        MeshData m;
        for(unsigned z=0;z<=cells;++z)for(unsigned x=0;x<=cells;++x){MeshVertex v;const float px=(float(x)-3)*spacing,pz=(float(z)-3)*spacing;v.position={px,Height(px,pz),pz};m.vertices.push_back(v);}
        const auto at=[&](unsigned x,unsigned z){return std::uint32_t(z*(cells+1)+x);};
        for(unsigned z=0;z<cells;++z)for(unsigned x=0;x<cells;++x)m.indices.insert(m.indices.end(),{at(x,z),at(x,z+1),at(x+1,z+1),at(x,z),at(x+1,z+1),at(x+1,z)});
        f.mesh=std::make_shared<CollisionAsset>();std::string error;
        if(!CookCollision(m,{},*f.mesh,error))throw std::runtime_error("mesh cook: "+error);
        f.triangles=unsigned(f.mesh->faces.size());f.world.CreateShape(Shape::Cooked(f.mesh,"M87 wavy mesh"),{{0,0,0},f.rotation},false,1,.55f,.02f);
        for(unsigned i=0;i<o.count;++i){const float x=float(i%columns)*1.2f,z=float(i/columns)*1.2f;create({x,Height(x,z)+.36f,z},{.2f,.2f,.2f},false,1);}
    }else if(machine){
        f.machines=(o.count+3)/4;
        f.world.CreateStaticBox(f.rotation*glm::vec3(float(columns)*1.5f,-.5f,float(columns)*1.5f),f.rotation,{float(columns)*1.5f+3,.5f,float(columns)*1.5f+3},.55f,.02f);
        for(unsigned i=0;i<f.machines;++i){
            const glm::vec3 p{float(i%columns)*3,0,float(i/columns)*3};
            for(unsigned part=0;part<4&&f.bodies.size()<o.count;++part){
                const auto h=create(p+(part==0?glm::vec3(0,1.2f,0):part==1?glm::vec3(.7f,.6f,0):glm::vec3(part==2?-.2f:.2f,.201f,.7f)),part==0?glm::vec3(.35f,.3f,.08f):part==1?glm::vec3(.2f,.12f,.12f):glm::vec3(.2f),part>=2,part<2?3.f:1.f);
                if(part>=2)continue;
                JointSettings s;s.bodyA=h;s.type=part==0?JointType::Hinge:JointType::Slider;s.spring=true;s.stiffness=20;s.damping=4;s.limits=true;s.lower=part==0?-.65f:-.25f;s.upper=part==0?.65f:.25f;
                s.frameA=s.frameB=glm::quat(1,0,0,0);
                if(part==0){s.anchorA={0,.3f,0};s.anchorB=f.rotation*(p+glm::vec3(0,1.5f,0));}
                else s.anchorB=f.rotation*(p+glm::vec3(.7f,.6f,0));
                s.frameB=f.rotation;
                if(o.workload=="machines_active"&&part==0){s.motor=true;s.speed=.7f;s.maxForce=4;}
                const auto joint=f.world.CreateJoint(s);if(!joint.IsValid())throw std::runtime_error("machine joint creation rejected");f.joints.push_back(joint);
            }
        }
    }else{
        const unsigned stacks=(o.count+4)/5,stackColumns=unsigned(std::ceil(std::sqrt(double(stacks))));
        const auto floor=f.world.CreateStaticBox(f.rotation*glm::vec3(float(stackColumns)*.6f,-.5f,float(stackColumns)*.6f),f.rotation,{float(stackColumns)*.6f+3,.5f,float(stackColumns)*.6f+3},.55f,.02f);
        if(o.noBodyContact)f.world.SetCollisionFilter(floor,1,~CategoryMask{0});
        for(unsigned i=0;i<o.count;++i){const unsigned stack=i/5,layer=i%5;create({float(stack%stackColumns)*1.2f,.241f+float(layer)*.482f,float(stack/stackColumns)*1.2f},{.24f,.24f,.24f},false,1);}
    }
}
struct Observation {bool finite=true;double deepest=0,kinetic=0,maximumSpeed=0,maximumDisplacement=0,freeError=0,jointError=0;unsigned asleep=0;};
Observation Observe(Fixture& f,unsigned completedSteps,std::uint64_t& digest){
    Observation r;const auto inverse=glm::inverse(f.rotation);
    for(const auto& body:f.bodies){
        const auto t=f.world.GetTransform(body.handle);const auto v=f.world.GetLinearVelocity(body.handle),w=f.world.GetAngularVelocity(body.handle);
        const auto local=inverse*t.position;const auto q=inverse*t.rotation;
        for(float x:{t.position.x,t.position.y,t.position.z,t.rotation.w,t.rotation.x,t.rotation.y,t.rotation.z,v.x,v.y,v.z,w.x,w.y,w.z}){r.finite&=std::isfinite(x);Mix(digest,x);}
        r.kinetic+=.5*double(f.world.GetMass(body.handle))*glm::dot(v,v);
        r.maximumSpeed=std::max(r.maximumSpeed,double(glm::length(v)));
        r.maximumDisplacement=std::max(r.maximumDisplacement,double(glm::length(local-body.initial)));
        r.asleep+=f.world.IsSleeping(body.handle);
        if(!f.hasFloor){const auto expected=body.initial+body.initialVelocity*(float(completedSteps)*kDt);r.freeError=std::max(r.freeError,double(glm::length(local-expected)));continue;}
        if(body.sphere)r.deepest=std::max(r.deepest,double(body.half.x-local.y));
        else for(unsigned corner=0;corner<8;++corner){const glm::vec3 p=local+q*glm::vec3(corner&1?body.half.x:-body.half.x,corner&2?body.half.y:-body.half.y,corner&4?body.half.z:-body.half.z);const float surface=f.mesh?Height(p.x,p.z):0.f;r.deepest=std::max(r.deepest,double(surface-p.y));}
    }
    for(auto handle:f.joints){JointState j;if(!f.world.GetJoint(handle,j)){r.finite=false;continue;}const auto a=f.world.GetTransform(j.settings.bodyA);const auto pa=a.position+a.rotation*j.settings.anchorA;const auto delta=glm::inverse(a.rotation*j.settings.frameA)*(j.settings.anchorB-pa);const double error=j.settings.type==JointType::Slider?std::hypot(delta.y,delta.z):double(glm::length(delta));r.jointError=std::max(r.jointError,error);}
    return r;
}
int Run(const Options& o){
    std::filesystem::create_directories(o.output);
    if(std::filesystem::exists(o.output/"summary.json"))throw std::runtime_error("refusing to replace an existing summary");
    const auto setupStart=Clock::now();Fixture f;CreateFixture(f,o);const double setupMs=Ms(setupStart,Clock::now());
    const auto rssSetup=Resident();
    auto& profiler=PerformanceProfiler::Get();profiler.Enable(o.profile);profiler.Clear();profiler.RegisterThread("M87 physics benchmark");
    std::ofstream csv(o.output/"steps.csv");csv<<std::setprecision(12);
    csv<<"step,measured,input_and_step_ms,step_ms,broadphase_ms,narrowphase_ms,solver_ms,bodies,dynamic,awake,sleeping,islands,candidates,pairs,points,impact_events,impact_queries,impact_iterations,sampling_tests,geometry_allocations,geometry_allocated_bytes,geometry_cache_bytes,solver_frame_builds,motion_bytes,resident_bytes,step_owner_thread_new_calls,step_owner_thread_new_bytes\n";
    std::vector<double> external,total,broad,narrow,solver;std::uint64_t digest=1469598103934665603ull,contactSteps=0,candidates=0,allocationCount=0,allocationBytes=0;
    double deepest=0,jointError=0,maximumSpeed=0,maxFreeError=0,maximumTravel=0;unsigned finalAsleep=0;bool finite=true;std::string thrown;
    std::uint64_t residentMaximum=rssSetup,cacheMaximum=0,motionMaximum=0;
    unsigned completed=0;std::uint64_t heapCalls=0,heapBytes=0;
    std::vector<float> minimumCoordinate(f.joints.size(),std::numeric_limits<float>::infinity());
    std::vector<float> maximumCoordinate(f.joints.size(),-std::numeric_limits<float>::infinity());
    try{
        for(unsigned step=0;step<o.warm+o.steps;++step){
            if(step==o.warm)profiler.Clear();
            if(o.profile)profiler.BeginFrame("M87 physics benchmark",step<o.warm);
            // Periodic ordinary motor commands prevent a limited paddle from
            // merely sitting against its stop for the entire timed interval.
            // Driver policy cost is outside Step's allocation/timing counters.
            if(o.workload=="machines_active"&&step%60==0)for(auto h:f.joints){JointState j;if(f.world.GetJoint(h,j)&&j.settings.motor){j.settings.speed=(step/60)%2?-.7f:.7f;if(!f.world.SetJoint(h,j.settings))throw std::runtime_error("motor direction update rejected");}}
            const auto start=Clock::now();
            for(const auto& body:f.bodies)f.world.ApplyLinearAcceleration(body.handle,f.gravity,kDt);
            M87Allocation::calls=M87Allocation::bytes=0;
            if(!o.freeze){ProfileFixedStep fixed(kDt);M87Allocation::enabled=M87Allocation::available;try{f.world.Step(kDt);}catch(...){M87Allocation::enabled=false;throw;}M87Allocation::enabled=false;}
            const auto end=Clock::now();++completed;
            const auto stats=f.world.LastStepStats();
            if(step%30==0)residentMaximum=std::max(residentMaximum,Resident());
            const auto observed=Observe(f,completed,digest);finite&=observed.finite;deepest=std::max(deepest,observed.deepest);jointError=std::max(jointError,observed.jointError);maximumSpeed=std::max(maximumSpeed,observed.maximumSpeed);maximumTravel=std::max(maximumTravel,observed.maximumDisplacement);maxFreeError=std::max(maxFreeError,observed.freeError);finalAsleep=observed.asleep;
            contactSteps+=stats.collidingPairs;cacheMaximum=std::max(cacheMaximum,std::uint64_t(stats.geometryCacheBytes));motionMaximum=std::max(motionMaximum,std::uint64_t(stats.motionStorageBytes));
            const bool measured=step>=o.warm;
            if(measured)for(std::size_t i=0;i<f.joints.size();++i){JointState j;if(f.world.GetJoint(f.joints[i],j)&&j.settings.motor){minimumCoordinate[i]=std::min(minimumCoordinate[i],j.coordinate);maximumCoordinate[i]=std::max(maximumCoordinate[i],j.coordinate);}}
            csv<<step<<','<<measured<<','<<Ms(start,end)<<','<<stats.totalMilliseconds<<','<<stats.broadphaseMilliseconds<<','<<stats.narrowphaseMilliseconds<<','<<stats.solverMilliseconds<<','<<stats.bodies<<','<<stats.dynamicBodies<<','<<stats.awakeBodies<<','<<stats.sleepingBodies<<','<<stats.sleepingIslands<<','<<stats.candidatePairs<<','<<stats.collidingPairs<<','<<stats.contactPoints<<','<<stats.impactEvents<<','<<stats.impactQueries<<','<<stats.impactSearchIterations<<','<<stats.impactSamplingTests<<','<<stats.geometryCacheAllocations<<','<<stats.geometryCacheAllocatedBytes<<','<<stats.geometryCacheBytes<<','<<stats.solverFrameBuilds<<','<<stats.motionStorageBytes<<','<<(step%30==0?Resident():0)<<','<<M87Allocation::calls<<','<<M87Allocation::bytes<<'\n';
            if(measured){external.push_back(Ms(start,end));total.push_back(stats.totalMilliseconds);broad.push_back(stats.broadphaseMilliseconds);narrow.push_back(stats.narrowphaseMilliseconds);solver.push_back(stats.solverMilliseconds);candidates+=stats.candidatePairs;allocationCount+=stats.geometryCacheAllocations;allocationBytes+=stats.geometryCacheAllocatedBytes;heapCalls+=M87Allocation::calls;heapBytes+=M87Allocation::bytes;}
            if(o.profile)profiler.EndFrame();
        }
    }catch(const std::exception& error){thrown=error.what();if(o.profile)profiler.EndFrame();}
    std::string profileError;
    const bool profileSaved=!o.profile||profiler.Export((o.output/"profile.json").string(),profileError);
    profiler.Enable(false);
    // Independent live-mechanism control after timing: an impulse must wake an
    // inactive part and change its physical endpoint. No cached status suffices.
    bool wakeOkay=true;double wakeTravel=0,wakeAngle=0;
    if(o.workload=="machines_sleeping"&&!f.bodies.empty()){
        const auto h=f.bodies.back().handle;const auto before=f.world.GetTransform(h).position;
        f.world.ApplyLinearImpulse(h,f.rotation*glm::vec3(.3f,.5f,0));
        const bool awake=!f.world.IsSleeping(h);
        const auto hinged=f.bodies.front().handle;const auto beforeRotation=f.world.GetTransform(hinged).rotation;
        f.world.ApplyAngularImpulse(hinged,f.rotation*glm::vec3(.12f,0,0));const bool hingedAwake=!f.world.IsSleeping(hinged);
        try{for(unsigned i=0;i<6;++i){for(const auto& body:f.bodies)f.world.ApplyLinearAcceleration(body.handle,f.gravity,kDt);if(!o.freeze)f.world.Step(kDt);}wakeTravel=glm::length(f.world.GetTransform(h).position-before);const auto after=f.world.GetTransform(hinged).rotation;wakeAngle=2*std::acos(std::clamp(double(std::abs(glm::dot(beforeRotation,after))),0.,1.));wakeOkay=awake&&hingedAwake&&wakeTravel>.005&&wakeAngle>.005;}catch(const std::exception& e){wakeOkay=false;thrown=e.what();}
    }
    double minimumDrivenRange=std::numeric_limits<double>::infinity();unsigned driven=0;
    for(std::size_t i=0;i<f.joints.size();++i)if(std::isfinite(minimumCoordinate[i])){++driven;minimumDrivenRange=std::min(minimumDrivenRange,double(maximumCoordinate[i]-minimumCoordinate[i]));}
    if(!driven)minimumDrivenRange=0;
    const bool free=o.workload=="free",sleeping=o.workload=="machines_sleeping";
    double maximumUpperStackHeightLoss=0;
    if(o.workload=="pile")for(std::size_t i=0;i<f.bodies.size();++i){
        if(i%5!=4&&i+1!=f.bodies.size())continue;
        const auto& b=f.bodies[i];const auto local=glm::inverse(f.rotation)*f.world.GetTransform(b.handle).position;
        maximumUpperStackHeightLoss=std::max(maximumUpperStackHeightLoss,double(b.initial.y-local.y));
    }
    Json acceptance={{"completed",completed==o.warm+o.steps},{"finite",finite},{"no_exception",thrown.empty()},{"ground_penetration_below_0_12_m",free||deepest<.12},{"linear_speed_below_50_m_s",maximumSpeed<50},{"joint_anchor_error_below_0_03_m",jointError<.03},{"contacts_exercised",free||contactSteps>0},{"free_motion_error_below_0_002_m",!free||maxFreeError<.002},{"actual_motion_exercised",!free||maximumTravel>.01},{"sleeping_majority",!sleeping||!o.sleep||finalAsleep*2>=f.bodies.size()},{"impulse_wakes_and_moves",wakeOkay},{"profile_exported",profileSaved}};
    acceptance["every_driven_joint_physically_moves"]=o.workload!="machines_active"||(driven==f.machines&&minimumDrivenRange>.005);
    acceptance["upper_stacks_remain_load_bearing"]=o.workload!="pile"||maximumUpperStackHeightLoss<.12;
    bool pass=true;for(const auto& item:acceptance.items())pass&=item.value().get<bool>();
    Json endpoints=Json::array();for(const auto& body:f.bodies){const auto t=f.world.GetTransform(body.handle);endpoints.push_back({{"body",body.handle.id},{"position",Vector(t.position)},{"rotation",Quaternion(t.rotation)},{"velocity",Vector(f.world.GetLinearVelocity(body.handle))},{"angular_velocity",Vector(f.world.GetAngularVelocity(body.handle))},{"sleeping",f.world.IsSleeping(body.handle)}});}
    // Digest excludes the post-measurement wake control; endpoints explicitly
    // include it, making both the timed trajectory and the wake result auditable.
    std::ofstream(o.output/"endpoints.json")<<std::setw(2)<<endpoints<<'\n';
    std::ostringstream hex;hex<<std::hex<<std::setw(16)<<std::setfill('0')<<digest;
    Json result={{"schema","judas.m87.physics_benchmark.v1"},{"workload",o.workload},{"dynamic_bodies",f.bodies.size()},{"all_bodies",f.world.AliveBodyCount()},{"joints",f.joints.size()},{"machines",f.machines},{"triangles",f.triangles},{"fixed_dt_seconds",kDt},{"warm_steps",o.warm},{"measured_steps",o.steps},{"completed_steps",completed},{"profile_enabled",o.profile},{"sleep_enabled",o.sleep},{"rotated",o.rotated},{"negative_no_contact",o.noContact},{"negative_freeze",o.freeze},{"setup_ms",setupMs},{"input_and_step_ms",Distribution(external)},{"internal_step_ms",Distribution(total)},{"broadphase_ms",Distribution(broad)},{"narrowphase_ms",Distribution(narrow)},{"solver_ms",Distribution(solver)},{"measured_candidate_pair_steps",candidates},{"all_contact_pair_steps",contactSteps},{"measured_geometry_allocations",allocationCount},{"measured_geometry_allocated_bytes",allocationBytes},{"maximum_geometry_cache_bytes",cacheMaximum},{"maximum_motion_storage_bytes",motionMaximum},{"rss_after_setup_bytes",rssSetup},{"rss_maximum_observed_bytes",residentMaximum},{"process_peak_rss_bytes",PeakResident()},{"profiler_reserved_bytes",profiler.ReservedBytes()},{"maximum_penetration_m",deepest},{"maximum_joint_anchor_error_m",jointError},{"maximum_linear_speed_m_s",maximumSpeed},{"maximum_free_motion_error_m",maxFreeError},{"final_sleeping_bodies",finalAsleep},{"wake_control_travel_m",wakeTravel},{"trajectory_digest",hex.str()},{"exception",thrown},{"profile_error",profileError},{"acceptance",acceptance},{"pass",pass},{"memory_note","Linux process RSS includes allocator/profiler/collision cook; per-step resident0 means not sampled. No GPU/rendering. Costs exclude endpoint inspection and CSV/profile export; input_and_step includes gravity submission and fixed-step profiler scope."}};
    result["measured_step_owner_thread_new_calls"]=heapCalls;result["measured_step_owner_thread_new_bytes"]=heapBytes;
    result["owner_thread_allocation_diagnostics_available"]=M87Allocation::available;
    result["minimum_driven_coordinate_range_radians"]=minimumDrivenRange;result["driven_joints"]=driven;result["wake_control_hinge_angle_radians"]=wakeAngle;
    result["maximum_upper_stack_height_loss_m"]=maximumUpperStackHeightLoss;result["negative_no_body_contact"]=o.noBodyContact;
    result["allocation_note"]="Test-executable scoped new/new[] interception, ordinary/aligned/nothrow forms, requested bytes only during PhysicsWorld::Step on its calling thread. Not C malloc calls, deallocations, live bytes or worker-thread allocations.";
    std::ofstream(o.output/"summary.json")<<std::setw(2)<<result<<'\n';std::cout<<result.dump()<<'\n';
    return pass?0:1;
}
}
int main(int argc,char** argv){try{return Run(Arguments(argc,argv));}catch(const std::exception& error){std::cerr<<"M87 benchmark: "<<error.what()<<'\n';return 2;}}
