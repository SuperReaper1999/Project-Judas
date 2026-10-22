// Exact event-time geometry and isolated CCD query-batch controls.
// Private access stays in this fixture; no public test API or solver tuning.
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

namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* label) {
    ++checks;
    if(!value){++failures;std::printf("FAIL %s\n",label);}
}
using World=PhysicsWorld::Impl;
unsigned Slot(BodyHandle handle){return handle.id&World::kSlotMask;}
void Reset(World::CCDQueryStorage& scratch,const World& world) {
    scratch.Reset();
    scratch.impactPoseSamples.resize(world.bodies.size());
    scratch.impactAnchorSamples.resize(world.bodies.size());
}
}
int main() {
    PhysicsWorld physics; physics.Init();
    const auto a=physics.CreateDynamicBox({0,0,0},{.5f,.5f,.5f},1,.4f,0);
    const auto b=physics.CreateDynamicSphere({3,0,0},.5f,1,.4f,0);
    physics.SetLinearVelocity(b,{-8,0,0});
    auto& world=*physics.m_impl;
    world.stepDuration=1;world.impactQueryAnchorTime=.25;
    for(auto slot:world.aliveSlots) {
        auto& body=world.bodies[slot];
        body.motion.Begin(world.MakeHandle(slot).id,body.rigidBody,1);
        world.InvalidateImpactPose(slot);
        world.ImpactPoseAt(slot,.25);
    }
    World::CCDQueryStorage scratch;Reset(scratch,world);
    World::CCDQuery lane(world,scratch);
    const auto* borrowed=&lane.ImpactPoseAt(Slot(b),.25);
    Check(borrowed==&world.impactAnchorSamples[Slot(b)],"worker borrows the exact immutable owner anchor");
    Check(lane.ownerAnchorBorrows==1 && scratch.impactPoseEvaluations==0,
          "borrow avoids new pose/orientation preparation");
    const auto before=world.impactAnchorSamples[Slot(b)];
    const auto actual=lane.PairAt(Slot(a),Slot(b),0,0,.25,0);
    const auto oracle=ComputeContacts(PrimitiveAt(world.bodies[Slot(a)].shape,world.GeometryPoseAt(Slot(a),.25),0),
                                      PrimitiveAt(world.bodies[Slot(b)].shape,world.GeometryPoseAt(Slot(b),.25),0),0);
    Check(actual.count>0 && actual.count==oracle.count && actual.uncertain==oracle.uncertain,
          "borrowed actual contact agrees with independent geometry");
    bool geometry=true;
    for(int i=0;i<actual.count && i<oracle.count;++i)
        geometry &= actual.points[i].point==oracle.points[i].point &&
                    actual.points[i].normal==oracle.points[i].normal &&
                    actual.points[i].signedSeparation==oracle.points[i].signedSeparation;
    Check(geometry,"contact witnesses and separations are unchanged");
    Check(world.impactAnchorSamples[Slot(b)].pose.position==before.pose.position &&
          world.impactAnchorSamples[Slot(b)].time==before.time,
          "worker geometry leaves owner anchor unchanged");
    const auto* otherTime=&lane.ImpactPoseAt(Slot(b),.375);
    Check(otherTime!=borrowed && otherTime->pose.position==world.GeometryPoseAt(Slot(b),.375).position,
          "non-anchor bisection time uses its own authoritative pose");
    auto& body=world.bodies[Slot(b)];body.rigidBody.linearVelocity={4,0,0};
    body.motion.ChangeVelocity(body.rigidBody,.25);world.InvalidateImpactPose(Slot(b));
    Reset(scratch,world);World::CCDQuery changed(world,scratch);
    Check(&changed.ImpactPoseAt(Slot(b),.25)!=&world.impactAnchorSamples[Slot(b)],
          "velocity invalidation rejects the old owner anchor");
    Check(changed.ImpactPoseAt(Slot(b),.5).pose.position==world.GeometryPoseAt(Slot(b),.5).position,
          "post-impulse trial follows the replacement motion segment");
    world.ImpactPoseAt(Slot(b),.25);
    auto& anchor=world.impactAnchorSamples[Slot(b)];
    anchor.generation^=1;Reset(scratch,world);World::CCDQuery stale(world,scratch);
    Check(&stale.ImpactPoseAt(Slot(b),.25)!=&anchor,"wrong-generation anchor is never borrowed");
    anchor.generation=body.generation;
    anchor.pose.orientation=glm::angleAxis(.2f,glm::vec3(1,0,0));
    Reset(scratch,world);World::CCDQuery mismatch(world,scratch);
    Check(&mismatch.ImpactPoseAt(Slot(b),.25)!=&anchor,
          "orientation-cache mismatch rejects an inconsistent owner sample");
    world.InvalidateImpactPose(Slot(b));world.ImpactPoseAt(Slot(b),.25);
    physics.ShiftOrigin({32,-16,8});
    Reset(scratch,world);World::CCDQuery shifted(world,scratch);
    Check(shifted.ImpactPoseAt(Slot(b),.25).pose.position==world.GeometryPoseAt(Slot(b),.25).position,
          "origin shift never exposes an old-chart owner pose");
    std::printf("CCD anchor reuse: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
