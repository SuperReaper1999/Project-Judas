// Candidate reuse and invalidation through actual PhysicsWorld::Step calls.
// Private access is confined to this fixture; no test-only engine API.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
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


using namespace JudasPhysicsDetail;

namespace {
constexpr float kDeltaTime = 1.f / 60;
unsigned checks = 0;
unsigned failures = 0;
using Pair = std::pair<unsigned, unsigned>;
using Pairs = std::vector<Pair>;

void Check(bool condition, const std::string &label) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL " << label << '\n';
    }
}

unsigned Slot(BodyHandle handle) {
    return handle.id & PhysicsWorld::Impl::kSlotMask;
}

Pair Key(BodyHandle a, BodyHandle b) {
    return {std::min(Slot(a), Slot(b)), std::max(Slot(a), Slot(b))};
}

bool Contains(const Pairs &pairs, BodyHandle a, BodyHandle b) {
    return std::binary_search(pairs.begin(), pairs.end(), Key(a, b));
}

// Independent exhaustive enumeration of fat-bound overlaps, not the engine's
// tree traversal or GenerateCandidatePairs/CanCollide/HasMotion helpers.
Pairs Oracle(const PhysicsWorld &physics) {
    const auto &world = *physics.m_impl;
    Pairs result;
    auto participates = [](const auto &body) {
        return (!body.simulationSuspended && (body.isDynamic || body.isKinematic)) ||
               body.sensor || body.queryOnly;
    };
    for (std::size_t i = 0; i < world.aliveSlots.size(); ++i) {
        const auto a = world.aliveSlots[i];
        const auto &first = world.bodies[a];
        if (!first.enabled)
            continue;
        for (std::size_t j = i + 1; j < world.aliveSlots.size(); ++j) {
            const auto b = world.aliveSlots[j];
            const auto &second = world.bodies[b];
            if (!second.enabled || (!participates(first) && !participates(second)))
                continue;
            if (!(first.collisionMask & (std::uint64_t(1) << second.collisionLayer)) ||
                !(second.collisionMask & (std::uint64_t(1) << first.collisionLayer)))
                continue;
            const auto handleA = world.MakeHandle(a).id;
            const auto handleB = world.MakeHandle(b).id;
            const auto handles = std::minmax(handleA, handleB);
            if (world.suppressedPairs.count({handles.first, handles.second}))
                continue;
            const auto &left = world.tree.FatAabb(first.proxy);
            const auto &right = world.tree.FatAabb(second.proxy);
            bool overlap = true;
            for (unsigned axis = 0; axis < 3; ++axis)
                overlap &= left.min[axis] <= right.max[axis] &&
                           right.min[axis] <= left.max[axis];
            if (overlap)
                result.emplace_back(a, b);
        }
    }
    // The oracle is canonical regardless of aliveSlots ordering. Enumeration
    // order must not accidentally duplicate the production ordering assumption.
    for (auto &pair : result)
        if (pair.second < pair.first)
            std::swap(pair.first, pair.second);
    std::sort(result.begin(), result.end());
    return result;
}

void CheckStationaryStep(PhysicsWorld &physics, const std::string &label) {
    const auto expected = Oracle(physics);
    physics.Step(kDeltaTime);
    Check(physics.m_impl->candidatePairs == expected, label + " exact sorted candidates");
    Check(physics.LastStepStats().proxyReinsertions == 0,
          label + " reaches the unchanged-tree certificate");
    Check(physics.m_impl->tree.Validate(), label + " retains valid tree ownership");
}

void MixedClassification() {
    PhysicsWorld physics;
    physics.Init();
    physics.SetSleepingEnabled(false);
    for (unsigned kind = 0; kind < 8; ++kind) {
        const float base = 3.f * kind;
        physics.CreateStaticSphere({base, 0, 0}, .1f, 0, 0);
        BodyHandle body;
        if (kind == 1 || kind == 2)
            body = physics.CreateStaticSphere({base + .25f, 0, 0}, .1f, 0, 0);
        else if (kind == 3)
            body = physics.CreateQueryCapsule(.1f, .1f, {{base + .25f, 0, 0}, {1, 0, 0, 0}});
        else
            body = physics.CreateDynamicSphere({base + .25f, 0, 0}, .1f, 1, 0, 0);
        if (kind == 2)
            physics.SetBodySensor(body, true);
        if (kind == 4)
            physics.SetMotionType(body, BodyMotionType::Kinematic);
        if (kind == 5 || kind == 7)
            physics.SetBodyEnabled(body, false);
        if (kind == 6)
            physics.SetCollisionFilter(body, 1, 0);
        if (kind == 7)
            physics.SetBodySimulationSuspended(body, true);
    }
    Check(Oracle(physics).size() == 4,
          "mixed fixture exercises dynamic, sensor, query and kinematic participation");
    CheckStationaryStep(physics, "mixed classification");
}

void WakeWithoutProxyChange() {
    PhysicsWorld physics;
    physics.Init();
    physics.SetSleepingEnabled(false);
    const auto sleeper = physics.CreateDynamicSphere({0, 0, 0}, .2f, 1, 0, 0);
    const auto moving = physics.CreateDynamicSphere({-.45f, 0, 0}, .2f, 1, 0, 0);
    physics.SetLinearVelocity(moving, {.1f, 0, 0});
    // Seed the sleep state directly so this fixture tests the actual prediction
    // Wake transition without a long, incidental settling prerequisite.
    physics.m_impl->Get(sleeper)->sleeping = true;
    Check(Contains(Oracle(physics), sleeper, moving), "wake fixture has predicted neighbor");
    CheckStationaryStep(physics, "wake transition");
    Check(!physics.IsSleeping(sleeper), "prediction really wakes the asleep participant");
}

void ForceInvalidatesCandidates() {
    for(float force:{7200.f,14400.f}) {
        PhysicsWorld physics;
        physics.Init();
        physics.SetSleepingEnabled(false);
        const auto mover = physics.CreateDynamicSphere({0, 0, 0}, .2f, 1, 0, 0);
        const auto wall = physics.CreateStaticBox({1.5f, 0, 0}, {.1f, 2, 2}, 0, 0);
        Check(!Contains(Oracle(physics), mover, wall),
              "force fixture initially excludes the remote wall candidate");
        // Force is retained over four native intervals, not converted into one
        // whole-dt pre-step kick. Independently check its actual free flight.
        PhysicsWorld free;free.Init();free.SetSleepingEnabled(false);
        const auto unblocked=free.CreateDynamicSphere({0,0,0},.2f,1,0,0);
        free.ApplyForce(unblocked,{force,0,0});free.Step(kDeltaTime);
        constexpr unsigned phases=4;
        const double h=double(kDeltaTime)/phases;
        const double freeX=double(force)*h*h*double(phases*(phases+1))/2;
        const double freeV=double(force)*double(kDeltaTime);
        Check(std::abs(double(free.GetTransform(unblocked).position.x)-freeX)<2e-6 &&
                  std::abs(double(free.GetLinearVelocity(unblocked).x)-freeV)<2e-5,
              "retained force matches the independent quarter-interval displacement and total impulse");
        physics.ApplyForce(mover, {force, 0, 0});
        physics.Step(kDeltaTime);
        Check(physics.LastStepStats().proxyReinsertions > 0,
              "integrated force genuinely changes the proxy reach");
        Check(Contains(physics.m_impl->candidatePairs, mover, wall),
              "changed-tree fallback discovers newly reachable wall candidate");
        constexpr double tolerance=2e-6;
        const double limit=double(1.5f)-double(.1f)-double(.2f);
        Check(freeX>limit+tolerance,"independent free flight really crosses the wall limit");
        for(const auto& segment:physics.GetBodyMotionSegments(mover))
            Check(segment.start.position.x<=limit+tolerance && segment.endPose.position.x<=limit+tolerance,
                  "every actual force-driven flight remains on the incident side of the wall");
        std::vector<PhysicsWorld::BodyContactSample> contacts;
        const bool sampled=physics.ReadBodyContactSample(mover,contacts);
        double normalImpulse=0;
        for(const auto& contact:contacts)normalImpulse+=contact.normalImpulse;
        const auto position=physics.GetTransform(mover).position;
        const auto velocity=physics.GetLinearVelocity(mover);
        Check(sampled && normalImpulse>0 &&
                  std::abs(normalImpulse-(freeV-double(velocity.x)))<2e-4,
              "the new force-expanded contact publishes its real momentum-changing normal reaction");
        Check(std::abs(double(position.x)-limit)<tolerance &&
                  physics.LastStepStats().impactSafetyFallback==0 && physics.m_impl->tree.Validate(),
              "force-driven contact reaches the wall without penetration, safety holds or invalid tree state");
        std::cout << "{\"kind\":\"force_candidate\",\"force\":" << force
                  << ",\"free_x\":" << free.GetTransform(unblocked).position.x
                  << ",\"wall_x\":" << position.x << ",\"velocity_x\":" << velocity.x
                  << ",\"normal_impulse\":" << normalImpulse
                  << ",\"impact_events\":" << physics.LastStepStats().impactEvents << "}\n";
        if(force==14400.f) {
            // This stronger input reaches the wall before the final native
            // boundary, so it retains the original in-step collision assertions.
            Check(physics.LastStepStats().impactEvents > 0,
                  "force fixture causes an actual physical collision episode");
            Check(physics.GetTransform(mover).position.x < 1.3f &&
                      std::abs(physics.GetLinearVelocity(mover).x) < 1.f,
                  "actual CCD prevents the forced body crossing the wall");
        } else {
            // The original 7200 force reaches a non-bouncing speculative
            // contact at the final native boundary. A CCD-event count is not
            // its contract: require actual reaction/containment above, then
            // verify complete stopping at the following force-free boundary.
            physics.Step(kDeltaTime);
            Check(std::abs(double(physics.GetTransform(mover).position.x)-limit)<tolerance &&
                      std::abs(physics.GetLinearVelocity(mover).x)<1e-5f &&
                      physics.LastStepStats().impactSafetyFallback==0,
                  "the original near-boundary force contact stops without penetration or lost continuation");
        }
    }
}

void BetweenStepIdentityAndPolicy() {
    PhysicsWorld physics;
    physics.Init();
    physics.SetSleepingEnabled(false);
    const auto anchor = physics.CreateStaticSphere({0, 0, 0}, .1f, 0, 0);
    const auto oldBody = physics.CreateDynamicSphere({.25f, 0, 0}, .1f, 1, 0, 0);
    CheckStationaryStep(physics, "original identity");
    physics.DestroyBody(oldBody);
    const auto replacement = physics.CreateDynamicSphere({.25f, 0, 0}, .1f, 1, 0, 0);
    Check(Slot(oldBody) == Slot(replacement) && oldBody.id != replacement.id &&
              physics.m_impl->Get(oldBody) == nullptr,
          "slot reuse changes generation and rejects original identity");
    physics.SetCollisionFilter(replacement, 2, 0);
    Check(Oracle(physics).empty(), "between-step filter change really removes the pair");
    CheckStationaryStep(physics, "replacement filtered out");
    physics.SetCollisionFilter(replacement, 2, kAllCategories);
    Check(Contains(Oracle(physics), anchor, replacement),
          "between-step filter restoration really restores the pair");
    CheckStationaryStep(physics, "replacement filter restored");
    physics.SetBodyEnabled(replacement, false);
    physics.SetBodySimulationSuspended(replacement, true);
    CheckStationaryStep(physics, "replacement suspended");
    physics.SetBodySimulationSuspended(replacement, false);
    physics.SetBodyEnabled(replacement, true);
    physics.ShiftOrigin({1000, -500, 250});
    CheckStationaryStep(physics, "replacement resumed after origin change");
}
void ProfiledCandidatePasses() {
    auto &profiler = PerformanceProfiler::Get();
    const bool enabled = profiler.Enabled();
    const bool frozen = profiler.Frozen();
    profiler.Enable(true);
    profiler.Freeze(false);
    profiler.Clear();
    {
        PhysicsWorld physics;
        physics.Init();
        physics.SetSleepingEnabled(false);
        const auto moving = physics.CreateDynamicSphere({0, 0, 0}, .2f, 1, 0, 0);
        const auto nearby = physics.CreateStaticSphere({.45f, 0, 0}, .2f, 0, 0);
        {
            ProfileFrame frame("candidate reuse unchanged proxy");
            physics.Step(kDeltaTime);
        }
        Check(physics.m_impl->work.active &&
                  physics.LastStepStats().proxyReinsertions == 0 &&
                  Contains(physics.m_impl->candidatePairs, moving, nearby) &&
                  physics.m_impl->work.candidatePasses == 1,
              "unchanged nonempty candidate list executes exactly one real broadphase pass");
    }
    {
        PhysicsWorld physics;
        physics.Init();
        physics.SetSleepingEnabled(false);
        const auto moving = physics.CreateDynamicSphere({0, 0, 0}, .2f, 1, 0, 0);
        const auto wall = physics.CreateStaticBox({1.5f, 0, 0}, {.1f, 2, 2}, 0, 0);
        physics.ApplyForce(moving, {7200, 0, 0});
        {
            ProfileFrame frame("candidate reuse changed proxy");
            physics.Step(kDeltaTime);
        }
        Check(physics.m_impl->work.active &&
                  physics.LastStepStats().proxyReinsertions > 0 &&
                  Contains(physics.m_impl->candidatePairs, moving, wall) &&
                  physics.m_impl->work.candidatePasses >= 2,
              "force-expanded proxy executes genuine second-pass candidate discovery");
    }
    profiler.Enable(enabled);
    profiler.Freeze(frozen);
    profiler.Clear();
}
} // namespace

int main() {
    MixedClassification();
    WakeWithoutProxyChange();
    ForceInvalidatesCandidates();
    BetweenStepIdentityAndPolicy();
    ProfiledCandidatePasses();
    std::cout << "{\"checks\":" << checks << ",\"failures\":" << failures << "}\n";
    return failures ? 1 : 0;
}
