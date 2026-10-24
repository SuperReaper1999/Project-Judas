#include "PhysicsWorld.h"
#include "PerformanceProfiler.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks = 0;
void Check(bool okay, const std::string& message) {
    ++checks;
    if (!okay) throw std::runtime_error(message);
    std::printf("PASS %s\n", message.c_str());
}
struct Result {
    PhysicsWorld::StepStats stats;
    std::vector<glm::vec3> poses, velocities;
    std::size_t heldPaths = 0;
    double restitutionFallbacks = 0;
};
Result Run(unsigned count, float speed, unsigned intervals, bool recording) {
    PhysicsWorld world;
    world.Init();
    std::vector<BodyHandle> balls;
    for (unsigned i = 0; i < count; ++i) {
        const float z = 3.f * i;
        world.CreateStaticBox({.6f, 0, z}, {.05f, 1, 1}, 0, 1);
        world.CreateStaticBox({-.6f, 0, z}, {.05f, 1, 1}, 0, 1);
        const auto ball = speed > 1
            ? world.CreateDynamicBox({0, 0, z}, {.08f, .2f, .05f}, 1, 0, 1)
            : world.CreateDynamicSphere({0, 0, z}, .05f, 1, 0, 1);
        if (speed > 1) world.SetAngularVelocity(ball, {0, 0, 100});
        world.SetLinearVelocity(ball, {speed, 0, 0});
        balls.push_back(ball);
    }
    auto& profiler = PerformanceProfiler::Get();
    profiler.Enable(recording);
    if (recording) { profiler.Clear(); profiler.BeginFrame("MK2 reporting fixture"); }
    world.StepCoupled(1.f / 60, intervals, {}, {});
    Result result;
    result.stats = world.LastStepStats();
    for (auto ball : balls) {
        bool held = false;
        for (const auto& segment : world.GetBodyMotionSegments(ball))
            held |= segment.begin < segment.end && segment.linearVelocity == glm::vec3(0);
        result.heldPaths += held;
        result.poses.push_back(world.GetTransform(ball).position);
        result.velocities.push_back(world.GetLinearVelocity(ball));
    }
    const auto& s = result.stats;
    Check(s.impactSafetyFallback == s.impactBodyCapHolds + s.impactStepCapHolds + s.impactResponseHolds,
          "hold reasons partition actual hold operations");
    Check(s.impactEventCapFallback == s.impactBodyCapHolds + s.impactStepCapHolds,
          "aggregate cap count agrees with detailed reasons");
    Check(s.impactResponseAttempts == s.impactResponsesApplied + s.impactMissingManifolds + s.impactEnergyRejections,
          "every attempted continuous response has a recorded outcome");
    if (recording) {
        profiler.EndFrame();
        const auto history = profiler.History();
        Check(!history.empty(), "completed profiler snapshot exists");
        const auto counter = [&](const char* label, std::size_t expected) {
            for (const auto& c : history.back().counters)
                if (std::string(c.name.c_str()) == label) {
                    Check(c.value == double(expected), std::string(label) + " matches completed public-step totals");
                    return;
                }
            Check(false, std::string("missing actual profiler counter: ") + label);
        };
        counter("Physics CCD path holds", s.impactSafetyFallback);
        counter("Physics CCD body limit holds", s.impactBodyCapHolds);
        counter("Physics CCD step limit holds", s.impactStepCapHolds);
        counter("Physics CCD response holds", s.impactResponseHolds);
        counter("Physics CCD response attempts", s.impactResponseAttempts);
        counter("Physics CCD responses applied", s.impactResponsesApplied);
        counter("Physics CCD missing manifolds", s.impactMissingManifolds);
        counter("Physics CCD energy rejections", s.impactEnergyRejections);
        counter("Physics impact queries", s.impactQueries);
        for (const char* name : {"Physics restitution frictionless retries", "Physics restitution inelastic fallbacks", "Physics restitution coupled fallbacks"}) {
            bool found = false;
            for (const auto& c : history.back().counters) {
                if (std::string(c.name.c_str()) != name) continue;
                found = true;
                Check(c.mode == ProfileCounterMode::Sum && c.value >= 0,
                      std::string(name) + " uses explicit frame-sum cadence");
                result.restitutionFallbacks += c.value;
            }
            Check(found, std::string(name) + " is present even when zero");
        }
    }
    profiler.Enable(false);
    std::printf("CASE count=%u speed=%g intervals=%u heldPaths=%zu operations=%zu bodyCap=%zu stepCap=%zu responseHold=%zu attempts=%zu applied=%zu missing=%zu energy=%zu\n",
        count, speed, intervals, result.heldPaths, s.impactSafetyFallback, s.impactBodyCapHolds,
        s.impactStepCapHolds, s.impactResponseHolds, s.impactResponseAttempts,
        s.impactResponsesApplied, s.impactMissingManifolds, s.impactEnergyRejections);
    return result;
}
}
int main() {
    try {
        const auto calm = Run(1, 1, 1, true);
        Check(!calm.heldPaths && !calm.stats.impactSafetyFallback && !calm.stats.impactResponseAttempts && calm.restitutionFallbacks == 0,
              "free slow motion independently reports no CCD hold or response");
        const auto bounded = Run(1, 100, 1, true);
        Check(bounded.heldPaths && bounded.stats.impactSafetyFallback,
              "a stationary remaining ledger segment with retained velocity reports a real hold");
        Check(bounded.stats.impactBodyCapHolds > 0 && bounded.stats.impactStepCapHolds == 0,
              "one fast bouncing body reaches the per-body limit, not the global limit");
        Check(glm::length(bounded.velocities.front()) > 1 && std::abs(bounded.poses.front().x) < .56f,
              "safety hold keeps physical speed and collision-bounded endpoint");
        const auto many = Run(300, 100, 1, true);
        Check(many.heldPaths > 0 && many.stats.impactStepCapHolds > 0,
              "many independent fast bodies exercise the unchanged global safety bound");
        Check(many.restitutionFallbacks > 0,
              "overloaded contacts actually exercise restitution fallback reporting");
        const auto unrecorded = Run(300, 100, 1, false);
        Check(many.poses == unrecorded.poses && many.velocities == unrecorded.velocities,
              "recording leaves actual physical endpoints and velocities exactly unchanged");
        Run(300, 100, 2, true);
        // Fresh world and frame must not retain the previous overload's counters.
        const auto after = Run(1, 1, 1, true);
        Check(!after.stats.impactSafetyFallback, "zero-work frame resets overloaded-frame reporting");
        std::printf("MK2 diagnostics: %u checks, 0 failures\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL %s\n", error.what());
        return 1;
    }
}
