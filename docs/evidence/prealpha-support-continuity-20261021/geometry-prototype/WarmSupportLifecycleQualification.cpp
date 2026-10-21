// Source-only private lifecycle negatives. Compile the SAME source against the
// frozen original engine and the retained-ordinary-support candidate separately.
// No cache/policy switches or extra engine hooks are required by this fixture.
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

namespace {
constexpr float dt = 1.f / 60.f;
constexpr float gravity = 9.81f;
unsigned checks = 0, failures = 0;
std::string variant = "unlabelled";

void Check(bool condition, const std::string &label) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL " << variant << ' ' << label << '\n';
    }
}
bool Finite(glm::vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

struct Warm {
    unsigned rows = 0, positive = 0;
    double normal = 0;
};

struct Fixture {
    PhysicsWorld physics;
    BodyHandle support, body, projectile;
    glm::quat rotation;
    glm::vec3 up, right, forward;
    std::string name;

    Fixture(std::string label, bool rotated)
        : rotation(rotated ? glm::quat(.7071067811865475f, 0, 0, .7071067811865475f)
                           : glm::quat(1, 0, 0, 0)),
          up(rotation * glm::vec3(0, 1, 0)), right(rotation * glm::vec3(1, 0, 0)),
          forward(rotation * glm::vec3(0, 0, 1)),
          name(std::move(label) + (rotated ? ":rotated" : ":flat")) {
        physics.Init();
        support = physics.CreateShape(Shape::Box({1, .5f, 1}),
                                      {-.5f * up, rotation}, false, 0, .3f, 0);
        body = physics.CreateShape(Shape::Box({.25f, .25f, .25f}),
                                   {.25f * up, rotation}, true, 2, .3f, 0);
        for (unsigned i = 0; i < 180; ++i)
            Step();
        const auto warm = WarmPair(body, support);
        Check(warm.positive > 0 && warm.normal > 0, name + " genuinely warmed load-bearing pair");
        Check(Finite(physics.GetTransform(body).position), name + " finite settled body");
        Trace("warmed", 0);
    }

    unsigned Slot(BodyHandle handle) const { return handle.id & PhysicsWorld::Impl::kSlotMask; }
    unsigned Generation(BodyHandle handle) const {
        return handle.id >> PhysicsWorld::Impl::kSlotBits;
    }
    Warm WarmPair(BodyHandle one, BodyHandle two) const {
        Warm result;
        if (!one.IsValid() || !two.IsValid())
            return result;
        if (Slot(one) > Slot(two))
            std::swap(one, two);
        for (const auto &cached : physics.m_impl->contactCache)
            if (cached.key.slotA == Slot(one) && cached.key.slotB == Slot(two) &&
                cached.key.generationA == Generation(one) &&
                cached.key.generationB == Generation(two)) {
                ++result.rows;
                result.positive += cached.normalImpulse > 0;
                result.normal += cached.normalImpulse;
            }
        return result;
    }
    void Step() {
        physics.ApplyLinearAcceleration(body, -gravity * up, dt);
        if (projectile.IsValid())
            physics.ApplyLinearAcceleration(projectile, -gravity * up, dt);
        physics.Step(dt);
        Check(Finite(physics.GetTransform(body).position) && Finite(physics.GetLinearVelocity(body)) &&
                  Finite(physics.GetAngularVelocity(body)), name + " finite actual endpoint");
        for (const auto &cached : physics.m_impl->contactCache)
            Check(std::isfinite(cached.normalImpulse) && cached.normalImpulse >= 0 &&
                      Finite(cached.tangentImpulse), name + " finite unilateral warm history");
    }
    void Trace(const char *phase, unsigned step) const {
        const auto pose = physics.GetTransform(body);
        const auto velocity = physics.GetLinearVelocity(body);
        const auto warm = WarmPair(body, support);
        const auto &impl = *physics.m_impl;
        std::cout << std::setprecision(17) << variant << ',' << name << ',' << phase << ',' << step
                  << ',' << glm::dot(pose.position, right) << ',' << glm::dot(pose.position, up)
                  << ',' << glm::dot(pose.position, forward) << ',' << glm::dot(velocity, right)
                  << ',' << glm::dot(velocity, up) << ',' << physics.IsSleeping(body)
                  << ',' << impl.bodies[Slot(body)].simulationSuspended << ',' << warm.rows << ','
                  << warm.positive << ',' << warm.normal << ',' << physics.LastStepStats().impactEvents
                  << '\n';
    }
    bool CompletelyPastSupport() const {
        Shape shape;
        BodyTransform pose;
        if (!physics.GetBodyShape(body, shape, pose))
            return false;
        // Bound along the scene-authored lateral axis, independent of world Y.
        // This direction also remains the launch direction after rotated setup.
        const float half = physics.GetBodySupportDistance(body, -right);
        return glm::dot(pose.position, right) - half > 1.05f;
    }
    void FreeFlight(const char *phase) {
        const glm::dvec3 initialPosition(physics.GetTransform(body).position);
        const glm::dvec3 initialVelocity(physics.GetLinearVelocity(body));
        const glm::dvec3 acceleration(-gravity * up);
        unsigned asleep = 0, suspended = 0;
        for (unsigned step = 1; step <= 60; ++step) {
            Step();
            asleep += physics.IsSleeping(body);
            suspended += physics.m_impl->bodies[Slot(body)].simulationSuspended;
            if (step == 1 || step == 15 || step == 30 || step == 60)
                Trace(phase, step);
        }
        const double time = 60.0 * double(dt);
        const glm::dvec3 expectedVelocity = initialVelocity + acceleration * time;
        const glm::dvec3 expectedPosition = initialPosition + initialVelocity * time +
            acceleration * (double(dt) * double(dt) * (60.0 * 61.0 * .5));
        const glm::dvec3 actualPosition(physics.GetTransform(body).position);
        const glm::dvec3 actualVelocity(physics.GetLinearVelocity(body));
        Check(asleep == 0, name + " unsupported gravity flight never sleeps");
        Check(suspended == 0, name + " unsupported body is not simulation-suspended");
        Check(glm::length(actualVelocity - expectedVelocity) < .001,
              name + " actual velocity follows sixty unchanged gravity kicks");
        Check(glm::length(actualPosition - expectedPosition) < .001,
              name + " actual position follows ordinary semi-implicit free flight");
        Check(WarmPair(body, support).rows == 0,
              name + " departed/invalid support history is absent after real steps");
    }
};

void LaunchOffSupport(bool rotated) {
    Fixture fixture("impact_departure", rotated);
    fixture.projectile = fixture.physics.CreateDynamicSphere(
        -.9f * fixture.right + .25f * fixture.up, .15f, 8, .3f, 0);
    fixture.physics.SetLinearVelocity(fixture.projectile, 50.f * fixture.right);
    fixture.Step();
    const auto events = fixture.physics.LastStepStats().impactEvents;
    fixture.Trace("after_actual_impact", 0);
    Check(events > 0, fixture.name + " fixture causes an actual solver TOI event");
    // Remove only the authored projectile after its real collision, isolating
    // the launched body's subsequent support departure and gravity flight.
    fixture.physics.DestroyBody(fixture.projectile);
    fixture.projectile = {};
    unsigned departureStep = 0;
    while (!fixture.CompletelyPastSupport() && departureStep < 30) {
        fixture.Step();
        ++departureStep;
    }
    fixture.Trace("geometric_departure", departureStep);
    Check(fixture.CompletelyPastSupport(), fixture.name + " actual collision launches body past finite support");
    Check(glm::dot(fixture.physics.GetLinearVelocity(fixture.body), fixture.right) > 0,
          fixture.name + " departure preserves real outgoing lateral motion");
    if (fixture.CompletelyPastSupport())
        fixture.FreeFlight("post_departure_flight");
}

enum class Mutation { MoveSupport, DisableSupport, MaskSupport, SensorSupport, SuppressPair,
                      ReuseSupportSlot, ResetBodyAway };

const char *MutationName(Mutation mutation) {
    switch (mutation) {
    case Mutation::MoveSupport: return "move_support";
    case Mutation::DisableSupport: return "disable_support";
    case Mutation::MaskSupport: return "mask_support";
    case Mutation::SensorSupport: return "sensor_support";
    case Mutation::SuppressPair: return "suppress_pair";
    case Mutation::ReuseSupportSlot: return "reuse_support_slot";
    case Mutation::ResetBodyAway: return "reset_body_away";
    }
    return "unknown";
}

void RemoveSupport(Mutation mutation, bool rotated) {
    Fixture fixture(MutationName(mutation), rotated);
    const auto oldSupport = fixture.support;
    switch (mutation) {
    case Mutation::MoveSupport:
        fixture.physics.ResetBody(fixture.support, 100.f * fixture.right - .5f * fixture.up,
                                  fixture.rotation);
        break;
    case Mutation::DisableSupport:
        Check(fixture.physics.SetBodyEnabled(fixture.support, false), fixture.name + " mutation accepted");
        break;
    case Mutation::MaskSupport:
        Check(fixture.physics.SetCollisionFilter(fixture.support, 0, 0), fixture.name + " mutation accepted");
        break;
    case Mutation::SensorSupport:
        Check(fixture.physics.SetBodySensor(fixture.support, true), fixture.name + " mutation accepted");
        break;
    case Mutation::SuppressPair:
        Check(fixture.physics.SetPairCollisionEnabled(fixture.body, fixture.support, false),
              fixture.name + " mutation accepted");
        break;
    case Mutation::ReuseSupportSlot:
        fixture.physics.DestroyBody(fixture.support);
        fixture.support = fixture.physics.CreateShape(Shape::Box({1, .5f, 1}),
            {100.f * fixture.right - .5f * fixture.up, fixture.rotation}, false, 0, .3f, 0);
        Check(fixture.Slot(fixture.support) == fixture.Slot(oldSupport) &&
                  fixture.support.id != oldSupport.id,
              fixture.name + " fixture actually reuses the slot with a new generation");
        Check(fixture.WarmPair(fixture.body, fixture.support).rows == 0,
              fixture.name + " replacement generation never matches old support warm rows");
        break;
    case Mutation::ResetBodyAway:
        fixture.physics.ResetBody(fixture.body, 5.f * fixture.right + .25f * fixture.up,
                                  fixture.rotation);
        break;
    }
    fixture.Trace("after_mutation_before_step", 0);
    fixture.FreeFlight("after_support_loss");
}
} // namespace

int main(int argc, char **argv) {
    if (argc > 1)
        variant = argv[1];
    std::cout << "variant,case,phase,step,right_m,up_m,forward_m,right_velocity,up_velocity,"
                 "sleeping,suspended,warm_rows,warm_positive,warm_normal,impact_events\n";
    for (bool rotated : {false, true}) {
        LaunchOffSupport(rotated);
        for (auto mutation : {Mutation::MoveSupport, Mutation::DisableSupport, Mutation::MaskSupport,
                              Mutation::SensorSupport, Mutation::SuppressPair,
                              Mutation::ReuseSupportSlot, Mutation::ResetBodyAway})
            RemoveSupport(mutation, rotated);
    }
    std::cerr << "WARM_LIFECYCLE variant=" << variant << " checks=" << checks
              << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
