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

void Check(bool condition, const std::string& label) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL " << label << '\n';
    }
}

bool Bits(float a, float b) {
    return std::memcmp(&a, &b, sizeof(float)) == 0;
}

bool Same(const glm::dvec3& a, const glm::dvec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool IsPair(const ContactKey& key, BodyHandle a, BodyHandle b) {
    const auto first = a.id & PhysicsWorld::Impl::kSlotMask;
    const auto second = b.id & PhysicsWorld::Impl::kSlotMask;
    return key.slotA == std::min(first, second) && key.slotB == std::max(first, second);
}

struct Fixture {
    PhysicsWorld world;
    BodyHandle floor;
    BodyHandle target;
    BodyHandle shot;

    explicit Fixture(bool sleeping = false) {
        world.Init();
        world.SetSleepingEnabled(sleeping);
        floor = world.CreateStaticBox({0, -.5f, 0}, {100, .5f, 100}, .5f, 0);
        target = world.CreateDynamicBox({0, .5f, 0}, {.5f, .5f, .5f}, 8, .5f, 0);
    }

    void Step(bool coupled = false) {
        world.ApplyLinearAcceleration(target, {0, -9.81f, 0}, kDeltaTime);
        if (shot.IsValid())
            world.ApplyLinearAcceleration(shot, {0, -9.81f, 0}, kDeltaTime);
        if (coupled) {
            PhysicsWorld::VelocityCoupling coupling;
            world.Step(kDeltaTime, &coupling);
        } else {
            world.Step(kDeltaTime);
        }
    }

    void Warm() {
        for (unsigned i = 0; i < 120; ++i)
            Step();
    }

    void Shoot(bool offset = false) {
        const glm::vec3 position{-2.f, offset ? .91f : .55f, offset ? .35f : 0.f};
        shot = world.CreateDynamicSphere(position, .2f, 2, .2f, 0);
        world.SetLinearVelocity(shot, {80, 0, 0});
    }
};

unsigned PositiveCache(PhysicsWorld& physics, BodyHandle a, BodyHandle b) {
    unsigned count = 0;
    for (const auto& cached : physics.m_impl->contactCache)
        count += IsPair(cached.key, a, b) && cached.normalImpulse > 0;
    return count;
}

void CheckNonzeroGuessesHaveOrdinaryProducer(Fixture& fixture) {
    auto& world = *fixture.world.m_impl;
    for (const auto& cached : world.contactCache) {
        if (!(cached.normalImpulse > 0))
            continue;
        bool fromOrdinary = false;
        for (const auto& contact : world.startContacts) {
            const auto& row = world.solver.Constraints()[contact.constraint];
            if (contact.a != cached.key.slotA || contact.b != cached.key.slotB)
                continue;
            fromOrdinary |= Bits(cached.normalImpulse, row.normalImpulse) &&
                            cached.tangentImpulse == row.tangentImpulse &&
                            Same(cached.localAnchorA, row.localAnchorA) &&
                            cached.normal == row.normal;
        }
        Check(fromOrdinary,
              "positive warm guess has exact ordinary-step producer, never TOI impulse");
    }
}

void OrdinaryOwnership() {
    Fixture fixture;
    fixture.Warm();
    Check(PositiveCache(fixture.world, fixture.floor, fixture.target) > 0,
          "warm fixture has real nonzero support history");
    fixture.Shoot();
    fixture.Step();

    auto& world = *fixture.world.m_impl;
    Check(world.stats.impactEvents > 0, "real fixed step contains separated physical impact");
    Check(fixture.world.GetLinearVelocity(fixture.target).x > .5f,
          "impact changes supported body's represented velocity");

    const auto floorSlot = fixture.floor.id & PhysicsWorld::Impl::kSlotMask;
    const auto targetSlot = fixture.target.id & PhysicsWorld::Impl::kSlotMask;
    unsigned produced = 0;
    unsigned preserved = 0;
    for (const auto& contact : world.startContacts) {
        const bool support = (contact.a == floorSlot && contact.b == targetSlot) ||
                             (contact.b == floorSlot && contact.a == targetSlot);
        if (!support)
            continue;
        const auto& row = world.solver.Constraints()[contact.constraint];
        if (!(row.normalImpulse > 0))
            continue;
        ++produced;
        for (const auto& cached : world.contactCache) {
            if (IsPair(cached.key, fixture.floor, fixture.target) &&
                Same(cached.localAnchorA, row.localAnchorA) && cached.normal == row.normal &&
                Bits(cached.normalImpulse, row.normalImpulse) &&
                cached.tangentImpulse == row.tangentImpulse) {
                ++preserved;
                break;
            }
        }
    }
    Check(produced > 0, "current ordinary solve actually produced the inspected support guesses");
    Check(preserved == produced,
          "eligible velocity-changing impact preserves ordinary numerical guesses");

    bool impactObserved = false;
    bool newIdentity = false;
    for (const auto& observed : world.completedContactObservations)
        impactObserved |= IsPair(observed.key, fixture.target, fixture.shot) &&
                          observed.normalImpulse > 0;
    for (const auto& cached : world.contactCache) {
        if (!IsPair(cached.key, fixture.target, fixture.shot))
            continue;
        newIdentity = true;
        Check(cached.normalImpulse == 0 && cached.tangentImpulse == glm::vec3(0),
              "new impact identity stores zero warm load, never its actual impact impulse");
    }
    Check(impactObserved,
          "completed observations separately contain actual projectile-target reaction");
    Check(newIdentity, "new physical impact publishes next-step contact identity");
    CheckNonzeroGuessesHaveOrdinaryProducer(fixture);
    std::cout << "ordinary_support produced=" << produced << " preserved=" << preserved
              << " events=" << world.stats.impactEvents
              << " target_vx=" << fixture.world.GetLinearVelocity(fixture.target).x << '\n';
}

void ScopeFallback(bool coupled) {
    Fixture fixture;
    fixture.Warm();
    if (!coupled) {
        JointSettings joint;
        joint.type = JointType::Slider;
        joint.bodyA = fixture.target;
        joint.anchorB = {0, .5f, 0};
        Check(fixture.world.CreateJoint(joint).IsValid(), "hard-joint fallback fixture authored");
    }
    fixture.Shoot();
    fixture.Step(coupled);
    auto& world = *fixture.world.m_impl;
    Check(world.stats.impactEvents > 0, "fallback fixture contains actual separated impact");
    Check(PositiveCache(fixture.world, fixture.floor, fixture.target) == 0,
          coupled ? "external-coupling fallback retains original warm invalidation"
                  : "hard-joint fallback retains original warm invalidation");
}

void ElasticFallback() {
    PhysicsWorld physics;
    physics.Init();
    physics.SetSleepingEnabled(false);
    const auto wall = physics.CreateStaticBox({1, 0, 0}, {.1f, 4, 4}, 0, 1);
    const auto ball = physics.CreateDynamicSphere({0, 0, 0}, .2f, 1, 0, 1);
    physics.SetLinearVelocity(ball, {60, 0, 0});
    physics.Step(kDeltaTime);
    Check(physics.LastStepStats().impactEvents > 0, "elastic fixture contains actual impact");
    Check(physics.GetLinearVelocity(ball).x < 0,
          "elastic fallback retains real restitution departure");
    Check(PositiveCache(physics, wall, ball) == 0,
          "elastic flight never inherits warm support impulse");
    bool actual = false;
    for (const auto& observed : physics.m_impl->completedContactObservations)
        actual |= IsPair(observed.key, wall, ball) && observed.normalImpulse > 0;
    Check(actual, "elastic actual reaction remains independently observable");
}

void RotatingFeatureAndRemoval() {
    Fixture fixture(true);
    fixture.Warm();
    fixture.Shoot(true);
    bool rotated = false;
    bool finite = true;
    bool departed = false;
    for (unsigned i = 0; i < 60; ++i) {
        fixture.Step();
        const auto pose = fixture.world.GetTransform(fixture.target);
        const auto localUp = glm::inverse(pose.rotation) * glm::vec3(0, 1, 0);
        rotated |= std::abs(localUp.y) < .8f;
        const auto velocity = fixture.world.GetLinearVelocity(fixture.target);
        const auto angular = fixture.world.GetAngularVelocity(fixture.target);
        finite &= FiniteVector(velocity) && FiniteVector(angular);
        if (velocity.y > .03f) {
            departed = true;
            Check(!fixture.world.IsSleeping(fixture.target),
                  "impact-launched body cannot sleep from retained support guess");
        }
    }
    Check(rotated, "off-centre physical impact actually changes contacted face orientation");
    Check(finite, "changed feature/normal contact remains finite");

    // Removal invalidates the handle immediately. Disposable cache rows can
    // remain until the next step; alive/generation checks reject their ownership.
    const auto floor = fixture.floor;
    fixture.world.DestroyBody(floor);
    Check(fixture.world.m_impl->Get(floor) == nullptr,
          "destroyed support handle cannot resolve to a physical body");
    const auto initialY = fixture.world.GetTransform(fixture.target).position.y;
    for (unsigned i = 0; i < 60; ++i) {
        fixture.Step();
        if (i == 0) {
            for (const auto& cached : fixture.world.m_impl->contactCache) {
                Check(cached.key.slotA != (floor.id & PhysicsWorld::Impl::kSlotMask) &&
                          cached.key.slotB != (floor.id & PhysicsWorld::Impl::kSlotMask),
                      "next ordinary step removes destroyed support cache rows");
            }
        }
    }
    Check(!fixture.world.IsSleeping(fixture.target),
          "body without remaining support stays physically active under gravity");
    Check(fixture.world.GetTransform(fixture.target).position.y < initialY,
          "removed support produces real downward travel");
    std::cout << "feature rotated=" << rotated
              << " upward_departure_observed=" << departed << '\n';
}
} // namespace

int main() {
    std::cout << std::setprecision(17);
    OrdinaryOwnership();
    ScopeFallback(false);
    ScopeFallback(true);
    ElasticFallback();
    RotatingFeatureAndRemoval();
    std::cout << "{\"checks\":" << checks << ",\"failures\":" << failures << "}\n";
    return failures ? 1 : 0;
}
