// Private integration fixture. Compile THIS TU with -fno-access-control only:
// the engine/public headers stay byte-identical; no public inspection API added.
#include "physics/WorldInternal.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <cstring>

using namespace JudasPhysicsDetail;
namespace {
constexpr float dt = 1.f / 60;
unsigned checks = 0, failures = 0;
void Check(bool condition, const std::string& label) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
bool Bits(float a, float b) { return std::memcmp(&a,&b,sizeof(float)) == 0; }
bool Same(const glm::dvec3& a, const glm::dvec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
bool IsPair(const ContactKey& key, BodyHandle a, BodyHandle b) {
    const auto sa = a.id & PhysicsWorld::Impl::kSlotMask;
    const auto sb = b.id & PhysicsWorld::Impl::kSlotMask;
    return key.slotA == std::min(sa,sb) && key.slotB == std::max(sa,sb);
}
struct Fixture {
    PhysicsWorld world;
    BodyHandle floor, target, shot;
    Fixture(bool sleeping = false) {
        world.Init(); world.SetSleepingEnabled(sleeping);
        floor = world.CreateStaticBox({0,-.5f,0},{100,.5f,100},.5f,0);
        target = world.CreateDynamicBox({0,.5f,0},{.5f,.5f,.5f},8,.5f,0);
    }
    void Step(bool coupled = false) {
        world.ApplyLinearAcceleration(target,{0,-9.81f,0},dt);
        if (shot.IsValid()) world.ApplyLinearAcceleration(shot,{0,-9.81f,0},dt);
        if (coupled) { PhysicsWorld::VelocityCoupling coupling; world.Step(dt,&coupling); }
        else world.Step(dt);
    }
    void Warm() { for (unsigned i=0;i<120;++i) Step(); }
    void Shoot(bool offset = false) {
        shot = world.CreateDynamicSphere({-2.f,offset?.91f:.55f,offset?.35f:0.f},.2f,2,.2f,0);
        world.SetLinearVelocity(shot,{80,0,0});
    }
};
unsigned PositiveCache(PhysicsWorld& world, BodyHandle a, BodyHandle b) {
    unsigned count = 0;
    for (const auto& c : world.m_impl->contactCache)
        count += IsPair(c.key,a,b) && c.normalImpulse > 0;
    return count;
}
void CheckNonzeroGuessesHaveOrdinaryProducer(Fixture& f) {
    auto& w = *f.world.m_impl;
    for (const auto& cached : w.contactCache) {
        if (!(cached.normalImpulse > 0)) continue;
        bool fromOrdinary = false;
        for (const auto& c : w.startContacts) {
            const auto& row = w.solver.Constraints()[c.constraint];
            if (c.a != cached.key.slotA || c.b != cached.key.slotB) continue;
            fromOrdinary |= Bits(cached.normalImpulse,row.normalImpulse) &&
                            cached.tangentImpulse == row.tangentImpulse &&
                            Same(cached.localAnchorA,row.localAnchorA) && cached.normal == row.normal;
        }
        Check(fromOrdinary,"positive warm guess has exact ordinary-step producer, never TOI impulse");
    }
}
void OrdinaryOwnership() {
    Fixture f; f.Warm();
    Check(PositiveCache(f.world,f.floor,f.target)>0,"warm fixture has real nonzero support history");
    f.Shoot(); f.Step();
    auto& w = *f.world.m_impl;
    Check(w.stats.impactEvents>0,"real fixed step contains separated physical impact");
    Check(f.world.GetLinearVelocity(f.target).x>.5f,"impact changes supported body's represented velocity");
    unsigned produced=0,preserved=0;
    for (const auto& start : w.startContacts) {
        if (!((start.a==(f.floor.id&PhysicsWorld::Impl::kSlotMask) && start.b==(f.target.id&PhysicsWorld::Impl::kSlotMask)) ||
              (start.b==(f.floor.id&PhysicsWorld::Impl::kSlotMask) && start.a==(f.target.id&PhysicsWorld::Impl::kSlotMask)))) continue;
        const auto& row = w.solver.Constraints()[start.constraint];
        if (!(row.normalImpulse>0)) continue;
        ++produced;
        for (const auto& cached : w.contactCache)
            if (IsPair(cached.key,f.floor,f.target) && Same(cached.localAnchorA,row.localAnchorA) &&
                cached.normal==row.normal && Bits(cached.normalImpulse,row.normalImpulse) &&
                cached.tangentImpulse==row.tangentImpulse) { ++preserved; break; }
    }
    Check(produced>0,"current ordinary solve actually produced the inspected support guesses");
    Check(preserved==produced,"eligible velocity-changing impact preserves ordinary numerical guesses");
    bool impactObserved=false,newIdentity=false;
    for (const auto& c : w.completedContactObservations)
        impactObserved |= IsPair(c.key,f.target,f.shot) && c.normalImpulse>0;
    for (const auto& c : w.contactCache)
        if (IsPair(c.key,f.target,f.shot)) {
            newIdentity=true;
            Check(c.normalImpulse==0 && c.tangentImpulse==glm::vec3(0),
                  "new impact identity stores zero warm load, never its actual impact impulse");
        }
    Check(impactObserved,"completed observations separately contain actual projectile-target reaction");
    Check(newIdentity,"new physical impact publishes next-step contact identity");
    CheckNonzeroGuessesHaveOrdinaryProducer(f);
    std::cout << "ordinary_support produced="<<produced<<" preserved="<<preserved
              <<" events="<<w.stats.impactEvents<<" target_vx="<<f.world.GetLinearVelocity(f.target).x<<'\n';
}
void ScopeFallback(bool coupled) {
    Fixture f; f.Warm();
    if (!coupled) {
        JointSettings joint; joint.type=JointType::Slider; joint.bodyA=f.target; joint.anchorB={0,.5f,0};
        Check(f.world.CreateJoint(joint).IsValid(),"hard-joint fallback fixture authored");
    }
    f.Shoot(); f.Step(coupled);
    auto& w=*f.world.m_impl;
    Check(w.stats.impactEvents>0,"fallback fixture contains actual separated impact");
    Check(PositiveCache(f.world,f.floor,f.target)==0,
          coupled?"external-coupling fallback retains original warm invalidation":"hard-joint fallback retains original warm invalidation");
}
void ElasticFallback() {
    PhysicsWorld world;world.Init();world.SetSleepingEnabled(false);
    auto wall=world.CreateStaticBox({1,0,0},{.1f,4,4},0,1);
    auto ball=world.CreateDynamicSphere({0,0,0},.2f,1,0,1);
    world.SetLinearVelocity(ball,{60,0,0});world.Step(dt);
    Check(world.LastStepStats().impactEvents>0,"elastic fixture contains actual impact");
    Check(world.GetLinearVelocity(ball).x<0,"elastic fallback retains real restitution departure");
    Check(PositiveCache(world,wall,ball)==0,"elastic flight never inherits warm support impulse");
    bool actual=false;for(const auto& c:world.m_impl->completedContactObservations)
        actual|=IsPair(c.key,wall,ball)&&c.normalImpulse>0;
    Check(actual,"elastic actual reaction remains independently observable");
}
void RotatingFeatureAndRemoval() {
    Fixture f(true);f.Warm();f.Shoot(true);
    bool rotated=false,finite=true,departed=false;
    for(unsigned i=0;i<60;++i) {
        f.Step();
        const auto pose=f.world.GetTransform(f.target);
        const auto localUp=glm::inverse(pose.rotation)*glm::vec3(0,1,0);
        rotated|=std::abs(localUp.y)<.8f;
        const auto v=f.world.GetLinearVelocity(f.target),a=f.world.GetAngularVelocity(f.target);
        finite &= FiniteVector(v)&&FiniteVector(a);
        if(v.y>.03f) { departed=true; Check(!f.world.IsSleeping(f.target),"impact-launched body cannot sleep from retained support guess"); }
    }
    Check(rotated,"off-centre physical impact actually changes contacted face orientation");
    Check(finite,"changed feature/normal contact remains finite");
    // Removing the real support invalidates every associated guess; no stale
    // positive cache row may confer a fictional gravity support/anchor.
    const auto floor=f.floor;
    f.world.DestroyBody(floor);
    for(const auto& c:f.world.m_impl->contactCache)
        Check(c.key.slotA!=(floor.id&PhysicsWorld::Impl::kSlotMask) &&
              c.key.slotB!=(floor.id&PhysicsWorld::Impl::kSlotMask),"destroyed support removes warm-history ownership");
    const auto initialY=f.world.GetTransform(f.target).position.y;
    for(unsigned i=0;i<60;++i)f.Step();
    Check(!f.world.IsSleeping(f.target),"body without remaining support stays physically active under gravity");
    Check(f.world.GetTransform(f.target).position.y<initialY,"removed support produces real downward travel");
    std::cout<<"feature rotated="<<rotated<<" upward_departure_observed="<<departed<<'\n';
}
} // namespace
int main() {
    std::cout<<std::setprecision(17);
    OrdinaryOwnership(); ScopeFallback(false); ScopeFallback(true); ElasticFallback(); RotatingFeatureAndRemoval();
    std::cout<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<"}\n";
    return failures?1:0;
}
