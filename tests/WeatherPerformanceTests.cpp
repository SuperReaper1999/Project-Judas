#include "RuntimeWorld.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;
constexpr unsigned bodyCount=64,warmSteps=100,measuredSteps=700;
constexpr float dt=1.f/60;
unsigned checks=0,failures=0;
void Check(bool value,const char* message){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",message);}
double Percentile(std::vector<double> values,double q){if(values.empty())return 0;std::sort(values.begin(),values.end());return values[size_t((values.size()-1)*q)];}
Scene Content(unsigned fields,bool responses){
    Scene scene;scene.Settings().name="M86 matched weather CPU workload";
    auto& ground=scene.CreateObject("ordinary ground");ground.body=SceneBodyComponent{};ground.body->halfExtents={500,.5f,500};ground.transform.position={0,-.5f,0};
    auto& gravity=scene.CreateObject("ordinary uniform field");gravity.gravity=SceneGravityComponent{};gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->magnitude=9.81f;gravity.gravity->regionRadius=10000;
    for(unsigned i=0;i<bodyCount;++i){auto& body=scene.CreateObject("ordinary wind prop");body.body=SceneBodyComponent{};body.body->motion=SceneBodyMotion::Dynamic;body.body->halfExtents={.25f,.25f,.25f};body.body->mass=8;body.body->friction=.15f;body.transform.position={float(i%8)*20-70,.27f,float(i/8)*20-70};if(responses)body.weatherResponse=WeatherResponseSettings{true,1,1,30,~uint64_t{0}};}
    for(unsigned i=0;i<fields;++i){auto& field=scene.CreateObject("ordinary weather field");field.weather=WeatherSettings{};field.weather->seed=86000+i;field.weather->priority=int(i);field.weather->profile.wind={12,0,3};field.weather->profile.gust=2;}
    return scene;
}
struct Result {
    std::vector<double> weather,physics;
    std::vector<EntityPhysicalState> endpoints;
    uint64_t queries=0,forces=0;
    bool okay=false;
};
Result Run(unsigned fields,bool responses,bool enabled,unsigned quality){
    const Scene scene=Content(fields,responses);RuntimeWorld world;world.legacyGameplay=false;world.weatherQuality=quality;std::string error;Result result;
    result.okay=world.Build(scene,nullptr,error);Check(result.okay,"ordinary headless weather workload builds");if(!result.okay){std::printf("BUILD %s\n",error.c_str());return result;}
    std::vector<EntityId> bodies;
    for(const auto& o:scene.Objects()){if(o.weather)world.RuntimeWeather(o.id)->SetEnabled(enabled);if(o.body&&o.body->motion==SceneBodyMotion::Dynamic)bodies.push_back(o.id);}
    RuntimeWorld::WeatherWork before;
    for(unsigned step=0;step<warmSteps+measuredSteps;++step){
        if(step==warmSteps)before=world.WeatherStats();
        auto start=Clock::now();world.UpdateWeather(dt);const auto afterWeather=Clock::now();world.Physics().Step(dt);const auto afterPhysics=Clock::now();
        if(step>=warmSteps){result.weather.push_back(std::chrono::duration<double,std::milli>(afterWeather-start).count());result.physics.push_back(std::chrono::duration<double,std::milli>(afterPhysics-afterWeather).count());}
    }
    const auto after=world.WeatherStats();result.queries=after.forceQueries-before.forceQueries;result.forces=after.forces-before.forces;
    for(auto id:bodies){EntityPhysicalState state;result.okay&=world.GetEntityState(id,state);result.endpoints.push_back(state);}
    const bool bounded=result.queries<=uint64_t(measuredSteps)*bodyCount;
    Check(bounded,"actual wind query work remains at most one cast per opted-in body per fixed step");
    if(!enabled||!responses)Check(result.queries==0&&result.forces==0,"disabled fields or absent opt-in produce no wind casts/forces");
    else Check(result.queries>0&&result.forces>0,"enabled workload exercises real collision queries and body force accumulation");
    std::printf("MEASURE weather_cpu fields=%u bodies=%u opt_in=%u enabled=%u visual_quality=%u steps=%u weather_p50_ms=%.9f weather_p95_ms=%.9f weather_max_ms=%.9f physics_p50_ms=%.9f physics_p95_ms=%.9f physics_max_ms=%.9f casts=%llu forces=%llu\n",fields,bodyCount,responses?bodyCount:0,enabled?1u:0u,quality,measuredSteps,Percentile(result.weather,.5),Percentile(result.weather,.95),Percentile(result.weather,1),Percentile(result.physics,.5),Percentile(result.physics,.95),Percentile(result.physics,1),static_cast<unsigned long long>(result.queries),static_cast<unsigned long long>(result.forces));
    return result;
}
void SamePhysicalEndpoints(const Result& a,const Result& b){
    double position=0,velocity=0;
    bool same=a.okay&&b.okay&&a.endpoints.size()==b.endpoints.size();
    if(same)for(size_t i=0;i<a.endpoints.size();++i){position=std::max(position,double(glm::length(a.endpoints[i].position-b.endpoints[i].position)));velocity=std::max(velocity,double(glm::length(a.endpoints[i].linearVelocity-b.endpoints[i].linearVelocity)));}
    Check(same&&position<1e-7&&velocity<1e-7&&a.queries==b.queries&&a.forces==b.forces,"visual quality off/high preserves actual physical endpoints and bounded work");
    std::printf("MEASURE quality_invariance maximum_position_difference_m=%.12g maximum_velocity_difference_m_s=%.12g\n",position,velocity);
}
}
int main(){
    std::printf("WORKLOAD native CPU only; same initial scene and fixed60Hz; 100 warmup + 700 measured steps; PhysicsWorld::Step timed separately from RuntimeWorld::UpdateWeather. No rendering/GPU claim.\n");
    for(unsigned fields:{1u,4u}){
        const auto off=Run(fields,true,false,0);
        const auto fieldsOnly=Run(fields,false,true,0);
        const auto low=Run(fields,true,true,0);
        const auto high=Run(fields,true,true,2);
        SamePhysicalEndpoints(low,high);
        double moved=0;for(size_t i=0;i<off.endpoints.size()&&i<low.endpoints.size();++i)moved=std::max(moved,double(glm::length(off.endpoints[i].position-low.endpoints[i].position)));
        Check(off.okay&&low.okay&&moved>.1,"enabled wind changes real body motion relative to the matched disabled field");
        std::printf("MEASURE weather_reference fields=%u disabled_p95_ms=%.9f fields_only_p95_ms=%.9f full_p95_ms=%.9f displacement_vs_disabled_m=%.9f\n",fields,Percentile(off.weather,.95),Percentile(fieldsOnly.weather,.95),Percentile(low.weather,.95),moved);
    }
    std::printf("NOTE fields-only and full timings are separate real workloads; their difference is not claimed as an exact isolated force profiler. Disabled means SetEnabled(false) on the same authored regions; no response bodies or physics quality are removed.\n");
    std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}
