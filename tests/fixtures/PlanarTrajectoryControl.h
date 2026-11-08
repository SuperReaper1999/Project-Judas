#pragma once
#include "PhysicsWorld.h"
#include "SaveArchive.h"
#include <stdexcept>
#include <vector>

// Same public-API 3D workload in the unmodified source and both experiments.
inline uint64_t PlanarStage0Trajectory3D() {
    PhysicsWorld world;
    world.Init();
    world.CreateStaticBox({0, -.5f, 0}, {8, .5f, 8}, .6f, .2f);
    std::vector<BodyHandle> bodies;
    for (int i = 0; i < 6; ++i) {
        auto h = world.CreateDynamicBox({float(i) - 2, 1.f + .5f * i, .1f * i}, {.3f, .4f, .2f}, 3, .6f, .2f);
        world.SetAngularVelocity(h, {.3f * i, -.4f, 2.f});
        bodies.push_back(h);
    }
    JointSettings hinge;
    hinge.bodyA = bodies[4];
    hinge.bodyB = bodies[5];
    hinge.type = JointType::Hinge;
    hinge.anchorA = {.5f, .25f, .05f};
    hinge.anchorB = {-.5f, -.25f, -.05f};
    if(!world.CreateJoint(hinge).IsValid())throw std::runtime_error("3D control hinge");
    uint64_t hash = UINT64_C(14695981039346656037);
    constexpr float dt=1.f/60;
    for (int step = 0; step < 300; ++step) {
        for (auto h : bodies) world.ApplyLinearAcceleration(h, {0, -9.81f, 0}, dt);
        if (step == 130) world.ApplyImpulseAtPoint(bodies[1], {1, 3, 2}, world.GetTransform(bodies[1]).position + glm::vec3(.1f, .3f, .2f));
        world.Step(dt);
        SaveArchive samples;
        for (auto h : bodies) {
            auto p = world.GetTransform(h);
            auto v = world.GetLinearVelocity(h), w = world.GetAngularVelocity(h);
            samples(p.position, p.rotation, v, w);
        }
        for (const unsigned char byte : samples.bytes) { hash ^= byte; hash *= UINT64_C(1099511628211); }
    }
    return hash;
}
