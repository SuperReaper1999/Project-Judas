#include "PhysicsWorld.h"
#include "JobSystem.h"
#include "PerformanceProfiler.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
bool Same(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }
bool Same(glm::vec3 a, glm::vec3 b) {
    return Same(a.x,b.x) && Same(a.y,b.y) && Same(a.z,b.z);
}
bool Same(glm::quat a, glm::quat b) {
    return Same(a.w,b.w) && Same(a.x,b.x) && Same(a.y,b.y) && Same(a.z,b.z);
}
struct Fixture {
    PhysicsWorld world;
    std::vector<BodyHandle> bodies;
    glm::quat rotation;
    explicit Fixture(bool rotated) : rotation(rotated ? glm::angleAxis(.63f,glm::normalize(glm::vec3(1,2,-3))) : glm::quat(1,0,0,0)) {
        world.Init();
        // Four hundred independent incoming pairs exceed the real parallel
        // query threshold. Grid spacing prevents accidental cross-pair hits.
        for (unsigned i=0;i<400;++i) for(unsigned side=0;side<2;++side) {
            auto shape = Shape::Box({.25f,.3f,.2f});
            const auto position = rotation * glm::vec3(float(i%20)*4 + float(side),float(i/20)*4,0);
            auto body = world.CreateShape(shape,{position,rotation},true,1,.35f,0);
            bodies.push_back(body);
            world.SetLinearVelocity(body,rotation*glm::vec3(side ? -24.f : 0.f,0,0));
            if(i%7==0) world.SetAngularVelocity(body,rotation*glm::vec3(.1f,.2f,-.15f));
        }
    }
    void Before(unsigned step,float dt) {
        if(step==3) world.ShiftOrigin({128,-64,32});
        if(step==4) for(unsigned i=1;i<bodies.size();i+=64) {
            const auto old=bodies[i]; const auto pose=world.GetTransform(old);
            world.DestroyBody(old);
            auto shape=Shape::Box({.25f,.3f,.2f});
            shape.pivotOffset={.1f,-.025f,.05f}; // deliberately unsupported fast path
            bodies[i]=world.CreateShape(shape,pose,true,1,.35f,0);
            Check(bodies[i].id!=old.id,"reused slots receive a new generation");
        }
        for(unsigned i=0;i<bodies.size();++i) {
            const auto gravity=rotation*(step<5?glm::vec3(0,-3,0):glm::vec3(0,0,2));
            world.ApplyLinearAcceleration(bodies[i],gravity,dt);
            if(step==2 && i%2) {
                const auto point=world.GetTransform(bodies[i]).position+rotation*glm::vec3(0,.15f,.1f);
                world.ApplyImpulseAtPoint(bodies[i],rotation*glm::vec3(3,0,0),point);
            }
        }
    }
};
void Compare(Fixture& serial,Fixture& jobs) {
    bool same=true;
    for(unsigned i=0;i<serial.bodies.size();++i) {
        const auto a=serial.bodies[i],b=jobs.bodies[i];
        const auto x=serial.world.GetTransform(a),y=jobs.world.GetTransform(b);
        same &= Same(x.position,y.position) && Same(x.rotation,y.rotation) &&
                Same(serial.world.GetLinearVelocity(a),jobs.world.GetLinearVelocity(b)) &&
                Same(serial.world.GetAngularVelocity(a),jobs.world.GetAngularVelocity(b));
    }
    Check(same,"serial and borrowed jobs preserve every physical pose and velocity bit");
}
}
int main() {
    // White-box checks of the MK1 TOI-event pipeline: pinned to MK1.
    setenv("JUDAS_PHYSICS_MK1", "1", 1);
    auto& profiler=PerformanceProfiler::Get();
    profiler.RegisterThread("impact batch owner"); profiler.Enable(true); profiler.Clear();
    bool batches=false,workers=false,bounds=false;
    for(bool rotated:{false,true}) {
        JobSystem jobs(3);
        Fixture serial(rotated),parallel(rotated);
        parallel.world.SetJobSystem(&jobs,4);
        for(unsigned step=0;step<10;++step) {
            const float dt=step<5?1.f/30:1.f/48;
            serial.Before(step,dt); parallel.Before(step,dt);
            serial.world.Step(dt);
            profiler.BeginFrame("impact batch continuation"); parallel.world.Step(dt); profiler.EndFrame();
            Compare(serial,parallel);
            if(step==0) Check(serial.world.LastStepStats().impactEvents>=300,
                             "incoming pairs exercise actual collision response, not empty query work");
            for(const auto& frame:profiler.History()) for(const auto& c:frame.counters) {
                if(c.name==std::string_view("Physics parallel CCD batches")) batches|=c.value>0;
                if(c.name==std::string_view("Physics worker CCD queries")) workers|=c.value>0;
                if(c.name==std::string_view("Physics CCD bound handoffs")) bounds|=c.value>0;
            }
        }
        jobs.Shutdown(); parallel.world.Step(1.f/60); serial.world.Step(1.f/60);
        Compare(serial,parallel); // stopped service safely continues natively
    }
    Check(batches&&workers,"real pending CCD batches execute on borrowed workers");
    Check(bounds,"immutable owner bounds are actually handed to query lanes");
    std::printf("Impact batch continuation: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
