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
    {
        // Both anchors can be borrowed before either lane-local array exists.
        // Reset(scratch,world) below intentionally pre-sizes storage and cannot
        // cover the empty-lane ActualImpactBound regression on its own.
        const auto physicalState=[&] {
            SaveArchive state;
            for(auto handle:physics.AliveBodies()){
                auto pose=physics.GetTransform(handle),previous=physics.GetPreviousTransform(handle);
                auto velocity=physics.GetLinearVelocity(handle),spin=physics.GetAngularVelocity(handle);
                auto id=handle.id;state(id,pose.position,pose.rotation,previous.position,previous.rotation,velocity,spin);
                physics.PersistBodyForces(state,handle);physics.PersistSleep(state,handle);
                auto leaves=physics.GetBodyMotionSegments(handle);auto count=uint32_t(leaves.size());state(count);
                for(auto& leaf:leaves)state(leaf.begin,leaf.end,leaf.start.position,leaf.start.rotation,
                    leaf.endPose.position,leaf.endPose.rotation,leaf.linearVelocity,leaf.angularVelocity,
                    leaf.pivotOffset,leaf.movable,leaf.prescribed,leaf.exactRotation,
                    leaf.anchoredTranslation,leaf.translationOrigin,leaf.translationTime);
            }
            return state.bytes;
        };
        const auto beforePhysical=physicalState();
        const auto anchorA=world.impactAnchorSamples[Slot(a)],anchorB=world.impactAnchorSamples[Slot(b)];
        const auto serialTruth=world.PairTouches(Slot(a),Slot(b),0,0,.25);
        const auto serialManifold=world.PairAt(Slot(a),Slot(b),0,0,.25,0);
        Check(serialTruth&&serialManifold.count>0&&!serialManifold.uncertain,
              "empty-lane control uses a genuine accepted contact, not an empty query");
        World::CCDQueryStorage freshTruth;
        World::CCDQuery truthLane(world,freshTruth);
        const auto* first=&truthLane.ImpactPoseAt(Slot(a),.25);
        const auto* second=&truthLane.ImpactPoseAt(Slot(b),.25);
        Check(first==&world.impactAnchorSamples[Slot(a)]&&second==&world.impactAnchorSamples[Slot(b)]&&
                  truthLane.ownerAnchorBorrows==2&&freshTruth.impactPoseSamples.empty()&&
                  freshTruth.impactAnchorSamples.empty(),
              "both prepared owner anchors borrow with genuinely empty lane storage");
        const auto laneTruth=truthLane.PairTouches(Slot(a),Slot(b),0,0,.25);
        Check(laneTruth==serialTruth&&freshTruth.impactPoseSamples.size()==world.bodies.size()&&
                  freshTruth.impactAnchorSamples.size()==world.bodies.size()&&freshTruth.impactPoseEvaluations==0,
              "boolean bounds independently allocate private storage without recomputing borrowed poses");
        World::CCDQueryStorage freshManifold;
        Check(freshManifold.impactPoseSamples.empty()&&freshManifold.impactAnchorSamples.empty(),
              "manifold control starts with a separate untouched lane");
        World::CCDQuery manifoldLane(world,freshManifold);
        const auto laneManifold=manifoldLane.PairAt(Slot(a),Slot(b),0,0,.25,0);
        bool same=laneManifold.count==serialManifold.count&&laneManifold.uncertain==serialManifold.uncertain;
        for(int k=0;k<laneManifold.count&&k<serialManifold.count;++k){
            const auto& x=laneManifold.points[k];const auto& y=serialManifold.points[k];
            same&=x.hit==y.hit&&x.point==y.point&&x.normal==y.normal&&x.penetration==y.penetration&&
                x.signedSeparation==y.signedSeparation&&x.preciseNormal==y.preciseNormal&&
                x.localAnchorA==y.localAnchorA&&x.localAnchorB==y.localAnchorB&&
                x.localWitnessA==y.localWitnessA&&x.localWitnessB==y.localWitnessB&&
                x.hasLocalAnchors==y.hasLocalAnchors&&x.anchorAInWorldFrame==y.anchorAInWorldFrame&&
                x.anchorBInWorldFrame==y.anchorBInWorldFrame&&x.separationState==y.separationState;
        }
        Check(same&&manifoldLane.ownerAnchorBorrows>=2&&freshManifold.impactPoseEvaluations==0&&
                  freshManifold.impactPoseSamples.size()==world.bodies.size(),
              "fresh borrowed-anchor manifold matches all exact serial geometry fields");
        Check(world.impactAnchorSamples[Slot(a)].time==anchorA.time&&
                  world.impactAnchorSamples[Slot(b)].time==anchorB.time&&
                  world.impactAnchorSamples[Slot(a)].pose.position==anchorA.pose.position&&
                  world.impactAnchorSamples[Slot(b)].pose.position==anchorB.pose.position&&
                  world.impactAnchorSamples[Slot(a)].pose.orientation==anchorA.pose.orientation&&
                  world.impactAnchorSamples[Slot(b)].pose.orientation==anchorB.pose.orientation&&
                  beforePhysical==physicalState(),
              "empty-lane queries leave owner anchors and authoritative physical motion unchanged");
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
    // Structural sections are offset identity-child compound boxes. Check
    // actual-bound rejection against independent complete contact geometry,
    // including near-touching pairs, parent rotation and a nonzero COM pivot.
    PhysicsWorld compoundPhysics;compoundPhysics.Init();
    const auto section=compoundPhysics.CreateDynamicCompoundBoxes({0,0,0},
        {CompoundBox({.375f,-.125f,.25f},{.3f,.2f,.7f})},2,.4f,0);
    const auto obstacle=compoundPhysics.CreateStaticBox({0,0,0},{.4f,.3f,.6f},.4f,0);
    auto& cw=*compoundPhysics.m_impl;
    const unsigned cs=Slot(section), os=Slot(obstacle);
    auto& cb=cw.bodies[cs];cb.shape.pivotOffset={.0625f,-.03125f,.015625f};
    cb.boundingRadius=ShapeBoundingRadius(cb.shape);
    unsigned rejected=0;bool complete=true,enclosed=true;
    for(unsigned n=0;n<1200;++n) {
        cb.rigidBody.position={float(int(n%31)-15)*.07f,float(int(n%13)-6)*.08f,
                               float(int(n%23)-11)*.09f};
        const glm::vec3 origin=n%3?glm::vec3(0):glm::vec3(1048576,-524288,262144);
        cb.rigidBody.position+=origin;cw.bodies[os].rigidBody.position=origin;
        cb.rigidBody.orientation=glm::angleAxis(float(n%41)*.13f,
                                               glm::normalize(glm::vec3(1,2,3)));
        cw.InvalidateImpactPose(cs);cw.InvalidateImpactPose(os);
        const auto& cp=cw.ImpactPoseAt(cs,0);const auto& op=cw.ImpactPoseAt(os,0);
        const auto full=ComputeContacts(PrimitiveAt(cb.shape,cp.pose,0),
            PrimitiveAt(cw.bodies[os].shape,op.pose,0),0);
        const bool separated=cw.SimpleImpactBoundsSeparated(cs,os,cp,op);
        rejected+=separated;
        complete &= !separated || full.count==0;
        const auto bound=cw.ActualImpactBound(cs,cp);
        const auto& child=cb.shape.boxes[0];
        const auto rotation=ContactRotation(cp.pose.orientation);
        for(unsigned corner=0;corner<8;++corner) {
            glm::dvec3 local(child.localCenter-cb.shape.pivotOffset);
            for(unsigned axis=0;axis<3;++axis)
                local[axis]+=(corner&(1u<<axis))?child.halfExtents[axis]:-child.halfExtents[axis];
            const auto point=glm::dvec3(cp.pose.position)+rotation*local;
            enclosed &= glm::all(glm::greaterThanEqual(point,glm::dvec3(bound.min))) &&
                        glm::all(glm::lessThanEqual(point,glm::dvec3(bound.max)));
        }
    }
    Check(rejected>100,"offset compound controls exercise actual-bound rejection");
    Check(complete,"offset/rotated compound bound never rejects an actual contact");
    Check(enclosed,"offset compound bounds enclose all independently transformed corners");
    // Changing the event frontier must retain a reusable start observation
    // while independent trial times change; a new trajectory invalidates it.
    cw.impactQueryAnchorTime=.125;cw.InvalidateImpactPose(cs);
    const auto* frontier=&cw.ImpactPoseAt(cs,.125);
    cw.ImpactPoseAt(cs,.25);cw.ImpactPoseAt(cs,.375);
    Check(frontier==&cw.ImpactPoseAt(cs,.125) && frontier->time==.125,
          "nonzero event frontier survives intervening trial samples");
    cb.rigidBody.position={0,0,0};cb.rigidBody.orientation={1,0,0,0};cw.InvalidateImpactPose(cs);
    cw.bodies[os].rigidBody.position={0,0,0};cw.InvalidateImpactPose(os);
    cw.impactGeometryMemo.resize(1);cw.impactGeometryEpoch=1;cw.impactGeometryMemoActive=true;
    World::CCDQuery cached(cw);
    ResetContactGeometryDiagnostics();
    const auto once=cached.PairAt(cs,os,0,0,.125,.1f);
    const auto firstDiagnostics=GetContactGeometryDiagnostics();
    const auto twice=cached.PairAt(cs,os,0,0,.125,.1f);
    Check(once.count>0 && firstDiagnostics.predicates>0 && once.count==twice.count && firstDiagnostics.predicates==GetContactGeometryDiagnostics().predicates,
          "identical primitive/pose/margin observes the exact cached manifold");
    cached.PairAt(cs,os,0,0,.125,.2f);
    Check(cw.impactGeometryMemo[0].geometry.back()==.2f,
          "margin changes cannot borrow a different contact observation");
    ++cw.impactGeometryEpoch;
    cached.PairAt(cs,os,0,0,.125,.1f);
    Check(cw.impactGeometryMemo[0].epoch==cw.impactGeometryEpoch,
          "a new continuous pass invalidates geometry even at identical poses");
    cb.rigidBody.position.x+=3;cw.InvalidateImpactPose(cs);
    const auto changedPose=cached.PairAt(cs,os,0,0,.125,.1f);
    const auto independent=ComputeContacts(PrimitiveAt(cb.shape,cb.rigidBody,0),
        PrimitiveAt(cw.bodies[os].shape,cw.bodies[os].rigidBody,0),.1f,nullptr,nullptr,true);
    Check(changedPose.count==independent.count,
          "pose replacement and a deliberate hash collision recompute contact truth");
    World::CCDQueryStorage privateScratch;Reset(privateScratch,cw);
    World::CCDQuery privateLane(cw,privateScratch);
    Check(privateLane.GeometryMemo(cs,os,0,0,cw.ImpactPoseAt(cs,.125),
        cw.ImpactPoseAt(os,.125),0)==nullptr,"parallel query lane never mutates owner memo");
    cw.impactGeometryMemoActive=false;
    PhysicsWorld sweepPhysics;sweepPhysics.Init();
    const auto mover=sweepPhysics.CreateDynamicSphere({0,0,0},.25f,1,.4f,0);
    const auto wall=sweepPhysics.CreateStaticBox({3,0,0},{.1f,1,1},.4f,0);
    sweepPhysics.SetLinearVelocity(mover,{4,0,0});
    auto& sw=*sweepPhysics.m_impl;const auto ms=Slot(mover),ws=Slot(wall);
    auto& moving=sw.bodies[ms];moving.motion.Begin(sw.MakeHandle(ms).id,moving.rigidBody,1);
    sw.InvalidateImpactPose(ms);sw.impactGeometryEpoch=1;sw.impactGeometryMemoActive=true;
    sw.impactGeometryMemo.resize(1);sw.impactSearchMemo.resize(1);
    const auto firstHit=sw.ImpactTime(ms,ws,0,0,0,1,0);
    const auto reusedHit=sw.ImpactTime(ms,ws,0,0,0,1,0);
    Check(firstHit<1 && firstHit==reusedHit && sw.work.searchMemoReuses==1,
          "same exact interval and trajectories reuse the identical arrival time");
    moving.rigidBody.linearVelocity={-4,0,0};
    moving.motion.Begin(sw.MakeHandle(ms).id,moving.rigidBody,1);sw.InvalidateImpactPose(ms);
    const auto withdrawnHit=sw.ImpactTime(ms,ws,0,0,0,1,0);
    Check(withdrawnHit>1 && sw.work.searchMemoReuses==1,
          "a last-second trajectory reversal cannot reuse the old collision prediction");
    // Diagnostics must observe the same exact answer and restore transient
    // state even when the tool callback throws. They never own solver state.
    unsigned observed=0;
    sweepPhysics.SetCollisionSearchObserver([&](const auto& sample){
        ++observed;Check(sample.a==mover.id && sample.b==wall.id && sample.cacheHit,
            "observer identifies exact cached root without rescheduling physics");
    });
    Check(sw.ImpactTime(ms,ws,0,0,0,1,0)==withdrawnHit && observed==1,
        "optional observation preserves exact result");
    sweepPhysics.SetCollisionSearchObserver([](const auto&){throw std::runtime_error("observer control");});
    bool threw=false;try{sw.ImpactTime(ms,ws,0,0,0,1,0);}catch(const std::runtime_error&){threw=true;}
    Check(threw && sw.work.searchDetail==nullptr,"observer exception cannot leave a dangling detail pointer");
    threw=false;try{sw.ImpactTime(ms,ws,0,0,2,3,0);}catch(const std::out_of_range&){threw=true;}
    Check(threw && sw.work.searchDetail==nullptr,"failed geometry query restores the transient diagnostic scope");
    sweepPhysics.SetCollisionSearchObserver({});
    // Two different exact keys coexist in a set; touching one protects it
    // from least-recent replacement. An epoch never supplies old observations.
    struct Entry {std::uint64_t epoch=0,lastUse=0;unsigned key=0;};
    std::vector<Entry> entries(2);std::uint64_t clock=0;
    const auto get=[&](unsigned key,std::uint64_t epoch)->Entry& {
        return JudasPhysicsDetail::SelectImpactMemo(entries,0,epoch,clock,
            [&](const auto& entry){return entry.key==key;});
    };
    auto& zero=get(0,1);zero.key=0;zero.epoch=1;
    auto& one=get(1,1);one.key=1;one.epoch=1;
    const auto touched=get(0,1).lastUse;
    auto& replacement=get(2,1);
    Check(replacement.key==1 && get(0,1).lastUse>touched,
        "bounded set retention keeps the exact recently used key");
    replacement.key=2;replacement.epoch=1;
    auto& fresh=get(0,2);
    Check(fresh.epoch!=2,"new epoch cannot match an old set member");
    std::printf("Exact memo storage bytes: %zu\n",4096*(sizeof(World::ImpactGeometryMemo)+sizeof(World::ImpactSearchMemo)));
    sw.impactGeometryMemoActive=false;
    std::printf("CCD anchor reuse: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
