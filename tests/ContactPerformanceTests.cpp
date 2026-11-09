// FTFT4A-P performance workloads. This same driver is compiled against each
// complete source snapshot. It contains no alternate physics implementation.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <sys/resource.h>
#include <glm/gtc/quaternion.hpp>
#include "PhysicsWorld.h"

namespace {
using Clock = std::chrono::steady_clock;
double Milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
struct Counters {
    std::uint64_t predicates=0, fallbacks=0, unresolved=0;
    std::uint64_t orientationHits=0, orientationRebuilds=0, boundHits=0, boundRebuilds=0;
    std::uint64_t shapeRebuilds=0, allocations=0;
    std::size_t peakCacheBytes=0;
};
template<class T, class=void> struct Geometry {
    static void Add(const T&, Counters&) {}
    static constexpr bool available=false;
};
template<class T> struct Geometry<T, std::void_t<decltype(std::declval<T>().geometryPredicates)>> {
    static void Add(const T& s, Counters& c) {
        c.predicates+=s.geometryPredicates; c.fallbacks+=s.geometryExactFallbacks; c.unresolved+=s.geometryUnresolved;
    }
    static constexpr bool available=true;
};
template<class T, class=void> struct Cache {
    static void Add(const T&, Counters&) {}
    static constexpr bool available=false;
};
template<class T> struct Cache<T, std::void_t<decltype(std::declval<T>().orientationCacheHits)>> {
    static void Add(const T& s, Counters& c) {
        c.orientationHits+=s.orientationCacheHits; c.orientationRebuilds+=s.orientationCacheRebuilds;
        c.boundHits+=s.boundCacheHits; c.boundRebuilds+=s.boundCacheRebuilds;
        c.shapeRebuilds+=s.shapeCacheRebuilds;
        c.allocations+=s.geometryCacheAllocations; c.peakCacheBytes=std::max(c.peakCacheBytes,s.geometryCacheBytes);
    }
    static constexpr bool available=true;
};
void HashFloat(std::uint64_t& h,float value) {
    std::uint32_t bits=0; std::memcpy(&bits,&value,sizeof(bits));
    for(int k=0;k<4;++k) { h^=(bits>>(8*k))&255u; h*=1099511628211ull; }
}
bool State(PhysicsWorld& world,const std::vector<BodyHandle>& bodies,const char* name,int step,
           std::uint64_t& checksum,bool trace) {
    bool finite=true;
    if(trace) std::printf("{\"kind\":\"state\",\"fixture\":\"%s\",\"step\":%d,\"bodies\":[",name,step);
    for(std::size_t i=0;i<bodies.size();++i) {
        const auto p=world.GetTransform(bodies[i]);
        const auto v=world.GetLinearVelocity(bodies[i]);
        const auto w=world.GetAngularVelocity(bodies[i]);
        const float a[]={p.position.x,p.position.y,p.position.z,p.rotation.w,p.rotation.x,p.rotation.y,p.rotation.z,
                         v.x,v.y,v.z,w.x,w.y,w.z};
        if(trace) std::printf("%s[%u",i?",":"",bodies[i].id);
        for(float x:a) { finite=finite&&std::isfinite(x); HashFloat(checksum,x); if(trace)std::printf(",%.17g",double(x)); }
        if(trace) std::printf("]");
    }
    if(trace) std::printf("]}\n");
    return finite;
}
bool Workload(const char* name,int kind,bool trace) {
    const auto setupStart=Clock::now();
    PhysicsWorld world; world.Init();
    const auto floor=world.CreateStaticBox({0,-.5f,0},{16,.5f,12},.6f,.05f);
    std::vector<BodyHandle> bodies;
    std::vector<CompoundBox> parts={{{-.31f,0,.11f},{.19f,.25f,.22f}},
                                    {{.27f,-.08f,-.13f},{.23f,.17f,.19f}},
                                    {{.04f,.24f,.03f},{.11f,.1f,.16f}}};
    for(int i=0;i<32;++i) {
        const glm::vec3 p{float(i%8)*1.6f-5.6f,.65f+.05f*float(i%3),float(i/8)*1.7f-2.5f};
        BodyHandle h;
        if(kind==1 || (kind==3 && i%3==0)) h=world.CreateDynamicCompoundBoxes(p,parts,3.f,.6f,.05f);
        else if(kind==2 && i%4==0) h=world.CreateDynamicSphere({p.x,.375f,p.z},.375f,2.f,.6f,0.f);
        else h=world.CreateDynamicBox(p,{.3f+.02f*float(i%3),.35f,.28f},3.f,.6f,.05f);
        if(kind==2 && i%4!=0) {
            // Exact touching and nearly parallel quaternions exercise both the
            // ordinary filter and exact fallback; no engine policy is changed.
            const glm::quat q = i%2 ? glm::quat(1,0,0,0) : glm::quat(1,0,1e-12f,0);
            world.ResetBody(h,{p.x,.35f,p.z},q);
        } else if(kind!=2) {
            const auto axis=glm::normalize(glm::vec3(.3f+float(i%3),.7f,-.4f));
            world.ResetBody(h,p,glm::angleAxis(.07f*float(i+1),axis));
            world.SetAngularVelocity(h,axis*(.3f+.05f*float(i%4)));
            world.SetLinearVelocity(h,{.02f*float(i%3),0,.015f*float(i%5)});
        }
        bodies.push_back(h);
    }
    const double setupMs=Milliseconds(setupStart);
    constexpr int steps=120; constexpr float dt=1.f/60.f;
    Counters counters; double external=0,total=0,broad=0,narrow=0,solver=0;
    std::size_t candidates=0,pairs=0,points=0; std::uint64_t checksum=1469598103934665603ull;
    bool finite=true;
    for(int step=0;step<steps;++step) {
        const auto begin=Clock::now();
        if(kind==3) {
            const float t=float(step)*dt;
            world.ResetBody(floor,{.08f*std::sin(2*t),-.5f,.04f*std::cos(t)},
                            glm::angleAxis(.035f*std::sin(t),glm::normalize(glm::vec3(.3f,1,.2f))));
        }
        for(auto h:bodies)world.ApplyLinearAcceleration(h,{0,-9.81f,0},dt);
        world.Step(dt);
        external+=Milliseconds(begin);
        const auto s=world.LastStepStats();
        total+=s.totalMilliseconds;broad+=s.broadphaseMilliseconds;narrow+=s.narrowphaseMilliseconds;solver+=s.solverMilliseconds;
        candidates+=s.candidatePairs;pairs+=s.collidingPairs;points+=s.contactPoints;
        Geometry<PhysicsWorld::StepStats>::Add(s,counters);Cache<PhysicsWorld::StepStats>::Add(s,counters);
        finite=State(world,bodies,name,step,checksum,trace&&(step%10==0||step==steps-1))&&finite;
    }
    struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
    const bool pass=finite&&counters.unresolved==0&&pairs>0&&points>0;
    std::printf("{\"kind\":\"workload\",\"fixture\":\"%s\",\"steps\":%d,\"bodies\":33,\"dynamic_bodies\":32,"
                "\"setup_ms\":%.9g,\"external_frame_ms\":%.9g,\"total_ms\":%.9g,\"broadphase_ms\":%.9g,"
                "\"narrowphase_ms\":%.9g,\"solver_ms\":%.9g,\"candidate_pair_steps\":%zu,\"colliding_pair_steps\":%zu,"
                "\"contact_point_steps\":%zu,\"geometry_available\":%s,\"predicates\":%llu,\"exact_fallbacks\":%llu,"
                "\"unresolved\":%llu,\"cache_available\":%s,\"orientation_hits\":%llu,\"orientation_rebuilds\":%llu,"
                "\"bound_hits\":%llu,\"bound_rebuilds\":%llu,\"shape_rebuilds\":%llu,\"solver_frames\":%llu,"
                "\"cache_allocations\":%llu,\"peak_cache_bytes\":%zu,\"process_peak_rss_kib\":%ld,"
                "\"state_checksum\":\"%016llx\",\"finite\":%s,\"pass\":%s}\n",
                name,steps,setupMs,external/steps,total/steps,broad/steps,narrow/steps,solver/steps,candidates,pairs,points,
                Geometry<PhysicsWorld::StepStats>::available?"true":"false",(unsigned long long)counters.predicates,
                (unsigned long long)counters.fallbacks,(unsigned long long)counters.unresolved,
                Cache<PhysicsWorld::StepStats>::available?"true":"false",(unsigned long long)counters.orientationHits,
                (unsigned long long)counters.orientationRebuilds,(unsigned long long)counters.boundHits,
                (unsigned long long)counters.boundRebuilds,(unsigned long long)counters.shapeRebuilds,
                (unsigned long long)counters.allocations,counters.peakCacheBytes,
                usage.ru_maxrss,(unsigned long long)checksum,finite?"true":"false",pass?"true":"false");
    return pass;
}
} // namespace
int main(int argc,char**argv) {
    const bool trace=argc==2&&std::string(argv[1])=="--trace";
    bool pass=true;
    const char* names[]={"rotating_boxes","offset_compounds","near_parallel_and_exact_touch","moving_static_support"};
    for(int k=0;k<4;++k)pass=Workload(names[k],k,trace)&&pass;
    return pass?0:1;
}
