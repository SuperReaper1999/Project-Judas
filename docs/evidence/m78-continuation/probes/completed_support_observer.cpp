#include "PhysicsWorld.h"
#include <cstdio>
#include <vector>
#include <numeric>

// Narrow physical fixture, no balance controller, motor, animation or gains.
// A previously supported ordinary box receives a real incoming TOI impact.
// The start/contact and event solvers supply the observations; casts do not.
void Observe(const char* phase,PhysicsWorld& world,BodyHandle box,BodyHandle ground) {
    std::vector<PhysicsWorld::BodyContactSample> sample;
    const bool ok=world.ReadBodyContactSample(box,sample,64,128);
    unsigned floorRows=0;double load=0;
    for(const auto& row:sample)if(row.other.id==ground.id){++floorRows;load+=row.normalImpulse;}
    std::printf("%s valid=%d floor_rows=%u floor_impulse=%.9g impact_events=%zu box_y=%.9g vy=%.9g\n",
        phase,ok,floorRows,load,world.LastStepStats().impactEvents,
        world.GetTransform(box).position.y,world.GetLinearVelocity(box).y);
}
int main() {
    PhysicsWorld world;world.Init();
    const auto floor=world.CreateStaticBox({0,-.5f,0},{3,.5f,3},.8f,0);
    const auto box=world.CreateDynamicBox({0,.5f,0},{.5f,.5f,.5f},4,.8f,0);
    for(int i=0;i<4;++i){world.ApplyLinearAcceleration(box,{0,-9.81f,0},1.f/60);world.Step(1.f/60);}
    Observe("settled",world,box,floor);
    const auto ball=world.CreateDynamicSphere({0,1.30f,0},.15f,2,.8f,0);
    world.SetLinearVelocity(ball,{0,-20,0});
    world.ApplyLinearAcceleration(box,{0,-9.81f,0},1.f/60);
    world.ApplyLinearAcceleration(ball,{0,-9.81f,0},1.f/60);world.Step(1.f/60);
    Observe("after_real_impact",world,box,floor);
    world.SetBodyEnabled(box,false);Observe("disabled",world,box,floor);
    world.SetBodyEnabled(box,true);Observe("re_enabled_before_solve",world,box,floor);
    world.DestroyBody(floor);
    const auto replacement=world.CreateStaticBox({0,-.5f,0},{3,.5f,3},.8f,0);
    Observe("support_slot_reused_before_solve",world,box,replacement);
}
