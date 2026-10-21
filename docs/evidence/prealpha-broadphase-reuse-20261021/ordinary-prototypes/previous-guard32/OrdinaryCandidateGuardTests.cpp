// Internal ownership regression through actual PhysicsWorld::Step calls.
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
    PhysicsWorld physics;
    physics.Init();
    physics.SetSleepingEnabled(false);
    const auto mover = physics.CreateDynamicSphere({0, 0, 0}, .2f, 1, 0, 0);
    const auto wall = physics.CreateStaticBox({1.5f, 0, 0}, {.1f, 2, 2}, 0, 0);
    Check(!Contains(Oracle(physics), mover, wall),
          "force fixture initially excludes the remote wall candidate");
    physics.ApplyForce(mover, {7200, 0, 0});
    physics.Step(kDeltaTime);
    Check(physics.LastStepStats().proxyReinsertions > 0,
          "integrated force genuinely changes the proxy reach");
    Check(Contains(physics.m_impl->candidatePairs, mover, wall),
          "changed-tree fallback discovers newly reachable wall candidate");
    Check(physics.LastStepStats().impactEvents > 0,
          "force fixture causes an actual physical collision episode");
    Check(physics.GetTransform(mover).position.x < 1.3f &&
              std::abs(physics.GetLinearVelocity(mover).x) < 1.f,
          "actual CCD prevents the forced body crossing the wall");
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
} // namespace

int main() {
    MixedClassification();
    WakeWithoutProxyChange();
    ForceInvalidatesCandidates();
    BetweenStepIdentityAndPolicy();
    std::cout << "{\"checks\":" << checks << ",\"failures\":" << failures << "}\n";
    return failures ? 1 : 0;
}
