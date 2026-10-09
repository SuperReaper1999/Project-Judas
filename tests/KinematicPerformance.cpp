#include "PhysicsWorld.h"
#include "CharacterMotor.h"
#include "UniformGravity.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <vector>

// One bounded workload, with the stationary control and commanded case run
// through the same executable/build. Retain every sample: first movement,
// first impact and authority handoffs are part of the measured distribution.
int main(int argc,char** argv){
    constexpr float dt=1.f/60;
    std::ofstream csv;if(argc>1)csv.open(argv[1]);
    if(csv)csv<<"commanded,step,whole_fixed_ms,physics_ms,contacts,impact_events,impact_queries,ccd_iterations,motion_segments\n";
    for(bool commanded:{false,true}){
        PhysicsWorld p;p.Init();UniformGravity zero({0,0,0});
        std::vector<BodyHandle> supports,crates;std::vector<CharacterMotor> riders(4);
        for(int i=0;i<4;++i){
            auto h=p.CreateShape(Shape::Box({2,.25f,2}),{{float(i*8),-.25f,0},{1,0,0,0}},false,1,.9f,0);
            p.SetMotionType(h,BodyMotionType::Kinematic);supports.push_back(h);
            for(int j=0;j<2;++j)crates.push_back(p.CreateDynamicBox({float(i*8+j)-.5f,.3f,.7f},{.3f,.3f,.3f},1,.9f,0));
            riders[i].settings.gravityScale=0;riders[i].Reset({float(i*8),.92f,-.6f},{1,0,0,0});
        }
        auto pusher=p.CreateShape(Shape::Box({.25f,.25f,.25f}),{{-2,5,0},{1,0,0,0}},false,1,0,0);
        p.SetMotionType(pusher,BodyMotionType::Kinematic);p.CreateDynamicSphere({0,5,0},.15f,1,0,0);
        std::vector<double> times;std::size_t contactTotal=0,eventTotal=0,queryTotal=0;int firstImpact=-1;
        for(int step=0;step<360;++step){
            auto start=std::chrono::steady_clock::now();
            if(step==60&&commanded){p.MoveKinematic(pusher,{{2,5,0},{1,0,0,0}});for(auto h:supports)p.SetKinematicVelocity(h,{.4f,.05f,0},{0,.3f,0});}
            if(step==180){p.SetMotionType(supports[0],BodyMotionType::Dynamic,40);}
            if(step==181){p.SetMotionType(supports[0],BodyMotionType::Kinematic,40);if(commanded)p.SetKinematicVelocity(supports[0],{-.4f,.05f,0},{0,-.3f,0});}
            if(step==240&&commanded)for(auto h:supports)p.StopKinematic(h);
            for(auto h:crates)p.ApplyLinearAcceleration(h,{0,-9.81f,0},dt);
            p.Step(dt);for(auto& m:riders)m.Step(p,zero,dt);
            auto whole=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();times.push_back(whole);
            const auto& s=p.LastStepStats();contactTotal+=s.contactPoints;eventTotal+=s.impactEvents;queryTotal+=s.impactQueries;
            if(firstImpact<0&&s.impactEvents)firstImpact=step;
            if(csv)csv<<commanded<<','<<step<<','<<whole<<','<<s.totalMilliseconds<<','<<s.contactPoints<<','<<s.impactEvents<<','<<s.impactQueries<<','<<s.impactSearchIterations<<','<<s.motionSegments<<'\n';
            if(step==60||step==180||step==181||step==240||step==firstImpact)
                std::printf("PHASE commanded=%d step=%d whole_fixed_ms=%.9g physics_ms=%.9g contacts=%zu impacts=%zu queries=%zu ccd_iterations=%zu\n",commanded,step,whole,s.totalMilliseconds,s.contactPoints,s.impactEvents,s.impactQueries,s.impactSearchIterations);
        }
        auto sorted=times;std::sort(sorted.begin(),sorted.end());
        std::printf("PERF commanded=%d samples=%zu bodies=%zu motors=4 median_ms=%.9g p95_ms=%.9g max_ms=%.9g first_impact_step=%d contact_points_total=%zu impact_events_total=%zu impact_queries_total=%zu\n",commanded,times.size(),p.AliveBodyCount(),sorted[sorted.size()/2],sorted[std::size_t(.95*(sorted.size()-1))],sorted.back(),firstImpact,contactTotal,eventTotal,queryTotal);
    }
    return 0;
}
