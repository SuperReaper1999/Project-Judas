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
unsigned checks = 0, failures = 0;
void Check(bool condition, const char* label) {
    ++checks; failures += !condition;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", label);
}

void Exercise(bool bridgeTouching, bool mechanicalBridge) {
    PhysicsWorld physics;
    physics.Init(); physics.SetSleepingEnabled(false);
    auto& world = *physics.m_impl;
    constexpr float gap = .03125f;
    std::array<BodyHandle, 4> handles{};
    std::array<unsigned, 4> slots{};
    std::array<Contact, 3> geometry{};
    const std::array<float, 4> x{0, 1, bridgeTouching ? 2.f : 2.f + gap,
                               bridgeTouching ? 3.f : 3.f + gap};
    for (unsigned i = 0; i < 4; ++i) {
        handles[i] = physics.CreateShape(Shape::Sphere(.5f),
            {{x[i], 0, 0}, {1, 0, 0, 0}}, true, 1, .6f, 0);
        slots[i] = handles[i].id & PhysicsWorld::Impl::kSlotMask;
    }
    physics.SetLinearVelocity(handles[0], {2, 0, 0});
    if (mechanicalBridge) {
        JointSettings settings;
        settings.type = JointType::Ball;
        settings.bodyA = handles[1]; settings.bodyB = handles[2];
        settings.anchorA = {.5f, 0, 0};
        settings.anchorB = {bridgeTouching ? -.5f : -.5f - gap, 0, 0};
        Check(physics.CreateJoint(settings).IsValid(), "ordinary mechanical bridge created");
    }
    world.solver.Clear();
    for (unsigned pair = 0; pair < 3; ++pair) {
        const auto a = slots[pair], b = slots[pair + 1];
        const auto actual = world.PairAt(a, b, 0, 0, 0, 0);
        const auto proximity = world.PairAt(a, b, 0, 0, 0, .04f);
        Check(proximity.count == 1 && !proximity.uncertain,
              "native geometry produces one resolved authored pair");
        if (proximity.count != 1) return;
        geometry[pair] = proximity.points[0];
        if (pair == 1 && !bridgeTouching) {
            Check(actual.count == 0 && proximity.points[0].signedSeparation > 0,
                  "speculative bridge has genuine positive gap and no actual contact");
        } else {
            Check(actual.count == 1 && actual.points[0].signedSeparation <= 0,
                  "actual edge is independently confirmed at zero margin");
        }
        const auto index = world.solver.Constraints().size();
        const float warm = pair == 2 ? 1.25f : pair == 1 ? .75f : 0;
        const glm::vec3 tangent = pair == 2 ? glm::vec3(0, .125f, 0) : glm::vec3(0);
        const JudasPhysicsDetail::ContactKey key{
            a, b, world.bodies[a].generation, world.bodies[b].generation, 0, 0};
        const JudasPhysicsDetail::CachedContact cached{
            key, proximity.points[0].localAnchorA, proximity.points[0].normal,
            warm, tangent, float(proximity.points[0].signedSeparation)};
        if (pair != 0) world.contactCache.push_back(cached);
        world.pendingCache.push_back(cached);
        world.solver.AddContact(world.bodies[a].rigidBody, world.bodies[b].rigidBody,
                               proximity.points[0], .6f, 0, warm, tangent);
        world.startContacts.push_back({a, b, index, .6f, warm, tangent, pair == 0});
    }
    Check(PhysicsWorld::Impl::NormalSpeed(
              world.PairAt(slots[0], slots[1], 0, 0, 0, 0).points[0],
              world.bodies[slots[0]].rigidBody, world.bodies[slots[1]].rigidBody) < -.5,
          "selected initial pair has a genuine new closing impact");
    world.ResolveInitialImpacts(1.f / 60);
    Check(world.stats.impactEvents == 1, "one initial impact component resolved");
    Check(world.solver.Constraints().size() == 3 && world.startContacts.size() == 3,
          "all actual and speculative ordinary rows remain available");
    if (!bridgeTouching && !mechanicalBridge) {
        Check(world.startContacts[1].warmNormal == .75f,
              "positive-gap ordinary row retains its prior iterative guess");
        Check(world.startContacts[2].warmNormal == 1.25f &&
              world.startContacts[2].warmTangent == glm::vec3(0, .125f, 0),
              "disconnected resting contact retains exact normal and tangent guesses");
        Check(physics.GetLinearVelocity(handles[2]) == glm::vec3(0) &&
              physics.GetLinearVelocity(handles[3]) == glm::vec3(0),
              "instantaneous impact does not mutate disconnected resting cluster");
        Check(physics.GetLinearVelocity(handles[0]).x > .9f &&
              physics.GetLinearVelocity(handles[1]).x > .9f,
              "real seed impact transfers momentum within its actual component");
        // ResolveInitialImpacts precedes ordinary Prepare in the real step.
        // Compare the ensuing ten sweeps against an independent zero-guess
        // ordinary solve from the SAME actual post-impact physical endpoint.
        std::array<RigidBody, 4> referenceBodies{};
        for (unsigned i = 0; i < 4; ++i)
            referenceBodies[i] = world.bodies[slots[i]].rigidBody;
        ContactSolver reference;
        reference.EnableBlockNormals(world.solver.BlockNormalsEnabled());
        for (unsigned i = 0; i < 3; ++i)
            reference.AddContact(referenceBodies[i], referenceBodies[i + 1], geometry[i], .6f, 0);
        reference.Prepare(1.f / 60);
        reference.SolveVelocities(ContactSolver::kVelocityIterations);
        world.solver.Prepare(1.f / 60);
        world.solver.SolveVelocities(ContactSolver::kVelocityIterations);
        glm::vec3 momentum(0);
        double kinetic = 0;
        bool sameEndpoint = true;
        for (unsigned i = 0; i < 4; ++i) {
            const auto& actualBody = world.bodies[slots[i]].rigidBody;
            sameEndpoint &= glm::length(actualBody.linearVelocity - referenceBodies[i].linearVelocity) < 1e-5f &&
                            glm::length(actualBody.angularVelocity - referenceBodies[i].angularVelocity) < 1e-5f;
            momentum += actualBody.linearVelocity;
            kinetic += .5 * glm::dot(glm::dvec3(actualBody.linearVelocity), glm::dvec3(actualBody.linearVelocity));
            // Radius .5, mass1 => I=.1, so angular kinetic is .05*omega^2.
            kinetic += .05 * glm::dot(glm::dvec3(actualBody.angularVelocity), glm::dvec3(actualBody.angularVelocity));
        }
        Check(sameEndpoint, "retained speculative guesses cancel to independent zero-guess physical endpoint");
        Check(glm::length(momentum - glm::vec3(2, 0, 0)) < 1e-5f,
              "post-Prepare ordinary solve preserves actual total linear momentum");
        Check(kinetic <= 1.00001,
              "retained guesses add no post-impact physical energy after ten ordinary sweeps");
        bool removed = true;
        for (unsigned i : {1u, 2u}) {
            const auto& row = world.solver.Constraints()[i];
            removed &= std::abs(row.normalImpulse) < 1e-6f && glm::length(row.tangentImpulse) < 1e-6f;
        }
        Check(removed, "separated and unforced resting guesses are removed rather than replayed as impact loads");
    } else {
        Check(world.startContacts[2].warmNormal == 0 &&
              world.startContacts[2].warmTangent == glm::vec3(0),
              "actual or mechanical bridge still invalidates reached contact guesses");
        Check(physics.GetLinearVelocity(handles[2]).x > .1f,
              "real connected assembly still propagates physical impact");
        if (!bridgeTouching)
            Check(world.startContacts[1].warmNormal == .75f,
                  "mechanically joined endpoints do not turn a geometric gap into an impulse row");
    }
}

void RestitutionIsolation(float restitution) {
    PhysicsWorld physics;
    physics.Init(); physics.SetSleepingEnabled(false);
    auto& world = *physics.m_impl;
    std::array<BodyHandle, 4> handles{};
    std::array<unsigned, 4> slots{};
    const std::array<float, 4> positions{0, 1, 2.03125f, 3.03125f};
    for (unsigned i = 0; i < 4; ++i) {
        handles[i] = physics.CreateShape(Shape::Sphere(.5f),
            {{positions[i], 0, 0}, {1, 0, 0, 0}}, true, 1, 0,
            i < 2 ? restitution : 0);
        slots[i] = handles[i].id & PhysicsWorld::Impl::kSlotMask;
    }
    physics.SetLinearVelocity(handles[0], {2, 0, 0});
    world.solver.Clear();
    for (unsigned i = 0; i < 3; ++i) {
        const auto a = slots[i], b = slots[i + 1];
        const auto actual = world.PairAt(a, b, 0, 0, 0, 0);
        const auto proximity = world.PairAt(a, b, 0, 0, 0, .04f);
        Check(proximity.count == 1 && !proximity.uncertain &&
                  (i == 1 ? actual.count == 0 : actual.count == 1),
              "restitution fixture independently proves two actual clusters and separating gap");
        if (proximity.count != 1) return;
        const auto row = world.solver.Constraints().size();
        const float warm = i == 0 ? 0.f : i == 1 ? .75f : 1.25f;
        world.solver.AddContact(world.bodies[a].rigidBody, world.bodies[b].rigidBody,
                               proximity.points[0], 0, 0, warm);
        world.startContacts.push_back({a, b, row, 0, warm, glm::vec3(0), i == 0});
    }
    world.ResolveInitialImpacts(1.f / 60);
    // Equal masses, initial velocities2 and0: J=(1+e), resulting velocities
    // 1-e and1+e. This analytic oracle does not depend on solver internals.
    Check(std::abs(physics.GetLinearVelocity(handles[0]).x - (1 - restitution)) < 1e-6f &&
          std::abs(physics.GetLinearVelocity(handles[1]).x - (1 + restitution)) < 1e-6f,
          "separated resting cluster cannot change isolated material restitution");
    Check(physics.GetLinearVelocity(handles[2]) == glm::vec3(0) &&
          physics.GetLinearVelocity(handles[3]) == glm::vec3(0),
          "elastic response does not transmit across speculative gap");
    const double va = physics.GetLinearVelocity(handles[0]).x;
    const double vb = physics.GetLinearVelocity(handles[1]).x;
    Check(std::abs(va + vb - 2) < 1e-6 &&
          std::abs(.5 * (va * va + vb * vb) - (1 + double(restitution) * restitution)) < 1e-6,
          "isolated restitution matches independent momentum and kinetic energy oracle");
}
}
int main() {
    Exercise(false, false);
    Exercise(true, false);
    Exercise(false, true);
    for (float restitution : {0.f, .5f, 1.f}) RestitutionIsolation(restitution);
    std::printf("SUMMARY checks=%u failures=%u\n", checks, failures);
    return failures ? 1 : 0;
}
