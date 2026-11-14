#include "PhysicsWorld.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks = 0;

void Check(bool condition, const char* description) {
    ++checks;
    if (!condition) throw std::runtime_error(description);
}

void Timing(unsigned iterations, float dt) {
    PhysicsWorld world;
    world.Init();
    const auto body = world.CreateDynamicBox({0, 0, 0}, {.1f, .1f, .1f}, 2, 0, 0);
    world.SetLinearVelocity(body, {2, 0, 0});
    unsigned calls = 0, begins = 0, finishes = 0, count = 0, completed = 0;
    auto participant = PhysicsWorld::VelocityCoupling::ForEachSubstep(
        iterations, [&](const auto& step) {
            count = step.count;
            Check(step.stepDeltaTime == dt, "adapter retains the outer Step interval");
            Check(step.count > 0 && step.deltaTime == dt / float(step.count),
                  "native substep duration is explicit");
            Check(step.index == calls / iterations && step.iteration == calls % iterations &&
                      step.iterations == iterations,
                  "chronological and iterative indices are distinct");
            Check(step.beginTime == step.index * step.deltaTime,
                  "substep time is relative to this Step boundary");
            Check(std::abs(world.GetTransform(body).position.x - 2 * step.beginTime) < 1e-6f,
                  "reported time matches the actual native body pose");
            ++calls;
        }, [&](const auto& step) {
            Check(step.index == completed && step.iteration == step.iterations,
                  "completed boundary follows all requested solves");
            Check(calls == (completed + 1) * iterations,
                  "completion runs before the next native substep");
            Check(std::abs(world.GetTransform(body).position.x -
                           2 * (step.beginTime + step.deltaTime)) < 1e-6f,
                  "completed callback observes the actual advanced pose");
            ++completed;
        });
    participant.begin = [&](float h) { Check(h == dt, "begin keeps the legacy interval"); ++begins; };
    participant.finish = [&](float h) { Check(h == dt, "finish keeps the legacy interval"); ++finishes; };
    world.Step(dt, &participant);
    Check(calls == count * iterations && calls == participant.iterations,
          "every native substep receives the requested number of solves");
    Check(completed == count, "every native substep publishes its completion");
    Check(begins == 1 && finishes == 1, "begin/finish are not repeated by the adapter");
    Check(std::abs(world.GetTransform(body).position.x - 2 * dt) < 1e-6f,
          "observation-only adapter preserves free-flight endpoint");
}

void NormalSupport(float angle) {
    const auto frame = glm::angleAxis(angle, glm::normalize(glm::vec3(1, -2, 3)));
    const auto up = frame * glm::vec3(0, 1, 0);
    constexpr float dt = 1.f / 60, mass = 80;
    PhysicsWorld world;
    world.Init();
    const auto body = world.CreateDynamicBox({0, 0, 0}, {.1f, .1f, .1f}, mass, 0, 0);
    double normalReaction = 0;
    bool completedBeforeNextGravity = true;
    auto participant = PhysicsWorld::VelocityCoupling::ForEachSubstep(1, [&](const auto&) {
        const float incoming = glm::dot(world.GetLinearVelocity(body), up);
        if (incoming < 0) {
            const auto impulse = -mass * incoming * up;
            world.ApplyLinearImpulse(body, impulse);
            normalReaction += glm::dot(impulse, up);
        }
    }, [&](const auto&) {
        completedBeforeNextGravity &= glm::length(world.GetLinearVelocity(body)) < 1e-5f;
    });
    for (unsigned tick = 0; tick < 120; ++tick) {
        world.ApplyLinearAcceleration(body, -9.81f * up, dt);
        world.Step(dt, &participant);
    }
    Check(glm::length(world.GetTransform(body).position) < 1e-5f &&
              glm::length(world.GetLinearVelocity(body)) < 1e-5f,
          "all chronological acceleration increments receive a physical reaction");
    Check(completedBeforeNextGravity, "completion precedes the next external acceleration increment");
    Check(std::abs(normalReaction - mass * 9.81 * dt * 120) < .002,
          "constraint reaction balances actual external momentum");

    // A missing participant must be detected by the observed physical endpoint.
    PhysicsWorld absent;
    absent.Init();
    const auto falling = absent.CreateDynamicBox({0, 0, 0}, {.1f, .1f, .1f}, mass, 0, 0);
    absent.ApplyLinearAcceleration(falling, -9.81f * up, dt);
    absent.Step(dt);
    Check(glm::dot(absent.GetTransform(falling).position, up) < -.002f,
          "absent constraint negative control actually falls");

    PhysicsWorld legacy;
    legacy.Init();
    const auto partial = legacy.CreateDynamicBox({0, 0, 0}, {.1f, .1f, .1f}, mass, 0, 0);
    PhysicsWorld::VelocityCoupling once;
    unsigned legacyCalls = 0;
    once.iterations = 1;
    once.solve = [&](float h, unsigned round) {
        Check(h == dt && round == 0, "legacy callback timing remains unchanged");
        ++legacyCalls;
        legacy.ApplyLinearImpulse(partial, -mass * legacy.GetLinearVelocity(partial));
    };
    legacy.ApplyLinearAcceleration(partial, -9.81f * up, dt);
    legacy.Step(dt, &once);
    Check(legacyCalls == 1 && glm::dot(legacy.GetTransform(partial).position, up) < -.0001f,
          "one legacy solve is not silently expanded across native substeps");
}

void CoupledIntervals() {
    PhysicsWorld world;
    world.Init();
    const auto body = world.CreateDynamicBox({0, 0, 0}, {.1f, .1f, .1f}, 2, 0, 0);
    world.SetLinearVelocity(body, {1, 0, 0});
    constexpr float dt = 1.f / 60;
    unsigned calls = 0, count = 0;
    auto participant = PhysicsWorld::VelocityCoupling::ForEachSubstep(2, [&](const auto& step) {
        Check(step.stepDeltaTime == dt / 3, "adapter describes the actual coupled interval");
        count = step.count;
        ++calls;
    });
    world.StepCoupled(dt, 3, {}, {}, &participant);
    Check(calls == 3 * count * 2, "nested coupled intervals retain all native substeps");
    Check(std::abs(world.GetTransform(body).position.x - dt) < 1e-6f &&
              world.GetPreviousTransform(body).position == glm::vec3(0),
          "coupled adapter preserves endpoint and macro presentation history");
}


void RawPhaseTiming(unsigned iterations,unsigned externalIntervals) {
    PhysicsWorld world;
    world.Init();
    const auto body=world.CreateDynamicBox({0,0,0},{.1f,.1f,.1f},2,0,0);
    world.SetLinearVelocity(body,{2,0,0});
    constexpr float dt=1.f/60;
    const float localDt=dt/float(externalIntervals),h=localDt/4;
    unsigned external=0,phase=0,begins=0,finishes=0,totalCalls=0,callsInStep=0;
    unsigned beforeCalls=0,afterCalls=0;
    std::array<unsigned,4> phaseCalls{};
    const unsigned quotient=iterations/4,remainder=iterations%4;
    PhysicsWorld::VelocityCoupling participant;
    participant.iterations=iterations;
    participant.begin=[&](float interval) {
        Check(interval==localDt&&phase==0&&callsInStep==0,
              "raw begin observes the current outer Step interval once before its solves");
        ++begins;
    };
    participant.solve=[&](float interval,unsigned round) {
        Check(phase<4&&interval==localDt&&round==callsInStep,
              "raw callback retains its Step duration and contiguous global round number");
        const unsigned prefix=phase*quotient+std::min(phase,remainder);
        Check(round==prefix+phaseCalls[phase],
              "raw callback remainder rounds retain their chronological phase");
        Check(std::abs(world.GetTransform(body).position.x-
                       2*(float(external)*localDt+float(phase)*h))<1e-6f,
              "raw callback executes at its actual physical phase pose");
        ++phaseCalls[phase];++callsInStep;++totalCalls;
    };
    participant.afterSubstep=[&](const auto& context) {
        Check(phase<4&&context.stepDeltaTime==localDt&&context.deltaTime==h&&
                  context.index==phase&&context.count==4&&context.beginTime==float(phase)*h,
              "raw completion has four correctly timed physical phase contexts");
        const unsigned share=quotient+(phase<remainder?1u:0u);
        Check(context.iteration==share&&context.iterations==share&&phaseCalls[phase]==share,
              "raw iteration remainder is front-loaded without manufacturing zero-share solves");
        Check(std::abs(world.GetTransform(body).position.x-
                       2*(float(external)*localDt+float(phase+1)*h))<1e-6f,
              "raw completion observes the advanced pose before the next phase");
        ++phase;
    };
    participant.finish=[&](float interval) {
        Check(interval==localDt&&phase==4&&callsInStep==iterations,
              "raw finish occurs once after every requested solve and physical phase");
        ++finishes;
    };
    if(externalIntervals==1)world.Step(dt,&participant);
    else world.StepCoupled(dt,externalIntervals,[&](float interval,unsigned index) {
        Check(interval==localDt&&index==beforeCalls,
              "external interval preparation retains its own clock and ordering");
        external=index;phase=0;callsInStep=0;phaseCalls.fill(0);++beforeCalls;
    },[&](float interval,unsigned index) {
        Check(interval==localDt&&index==afterCalls&&phase==4&&callsInStep==iterations,
              "external completion follows its four physical phases");
        ++afterCalls;
    },&participant);
    Check(begins==externalIntervals&&finishes==externalIntervals&&
              totalCalls==externalIntervals*iterations,
          "raw callbacks run exactly once per requested round and begin/finish once per Step");
    Check(externalIntervals==1||(beforeCalls==externalIntervals&&afterCalls==externalIntervals),
          "external interval callbacks are not multiplied by physical contact phases");
    Check(std::abs(world.GetTransform(body).position.x-2*dt)<1e-6f&&
              world.GetPreviousTransform(body).position==glm::vec3(0),
          "raw/external phases preserve total elapsed motion and the macro presentation origin");
}

void ForceAndImpulseOwnership(unsigned externalIntervals) {
    PhysicsWorld world;
    world.Init();
    constexpr float dt=1.f/60;
    const auto body=world.CreateDynamicSphere({0,0,0},.5f,2,0,0);
    world.ApplyLinearImpulse(body,{2,0,0});
    world.ApplyAngularImpulse(body,{0,0,.2f});
    world.ApplyLinearAcceleration(body,{3,0,0},dt);
    world.ApplyForce(body,{4,0,0});
    world.ApplyTorque(body,{0,0,2});
    if(externalIntervals==1)world.Step(dt);
    else world.StepCoupled(dt,externalIntervals,{},{});
    const auto velocity=world.GetLinearVelocity(body),spin=world.GetAngularVelocity(body);
    Check(std::abs(velocity.x-(1+5*dt))<2e-6f&&
              glm::length(glm::vec2(velocity.y,velocity.z))<1e-6f,
          "force and sampled acceleration integrate once while the initial impulse is never divided or replayed");
    Check(glm::length(spin-glm::vec3(0,0,1+10*dt))<5e-6f,
          "torque and initial angular impulse retain their independent physical ownership");
    if(externalIntervals==1)world.Step(dt);
    else world.StepCoupled(dt,externalIntervals,{},{});
    Check(glm::length(world.GetLinearVelocity(body)-velocity)<2e-6f&&
              glm::length(world.GetAngularVelocity(body)-spin)<5e-6f,
          "absent force/torque and stale last-acceleration state add no momentum on the next Step");
}

void EmptyWorld() {
    PhysicsWorld world;
    world.Init();
    unsigned solves = 0, completed = 0, count = 0;
    double elapsed = 0;
    constexpr float dt = 1.f / 60;
    auto participant = PhysicsWorld::VelocityCoupling::ForEachSubstep(1,
        [&](const auto& step) { ++solves; count = step.count; },
        [&](const auto& step) { ++completed; elapsed += step.deltaTime; });
    world.Step(dt, &participant);
    Check(solves == count && count > 0 && completed == count,
          "a material clock does not require an awake rigid body");
    Check(std::abs(elapsed - dt) < 1e-9,
          "empty-world participant advances exactly the requested interval");
}
} // namespace

int main() {
    try {
        for (unsigned iterations : {1u, 2u, 5u})
            for (float dt : {1.f / 60, .01f}) Timing(iterations, dt);
        for (float angle : {0.f, .37f, 1.31f}) NormalSupport(angle);
        CoupledIntervals();
        for(unsigned iterations:{1u,5u,10u})
            for(unsigned externalIntervals:{1u,3u})RawPhaseTiming(iterations,externalIntervals);
        for(unsigned externalIntervals:{1u,3u})ForceAndImpulseOwnership(externalIntervals);
        EmptyWorld();
        for (unsigned count : {0u, std::numeric_limits<unsigned>::max()}) {
            bool rejected = false;
            try { PhysicsWorld::VelocityCoupling::ForEachSubstep(count, [](const auto&) {}); }
            catch (const std::invalid_argument&) { rejected = true; }
            Check(rejected, "invalid substep iteration count rejected before simulation");
        }
        bool rejected = false;
        try { PhysicsWorld::VelocityCoupling::ForEachSubstep(1, {}); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "missing substep callback rejected before simulation");
        std::printf("SUMMARY checks=%u failures=0\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
