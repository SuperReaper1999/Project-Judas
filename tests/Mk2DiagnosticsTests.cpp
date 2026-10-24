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
    double energyRatio = 0;
};
Result Run(unsigned count, float speed, unsigned intervals, bool recording, bool capFixture = false) {
    PhysicsWorld world;
    world.Init();
    std::vector<BodyHandle> balls;
    for (unsigned i = 0; i < count; ++i) {
        const float z = 3.f * i;
        world.CreateStaticBox({.6f, 0, z}, {.05f, 1, 1}, 0, 1);
        world.CreateStaticBox({-.6f, 0, z}, {.05f, 1, 1}, 0, 1);
        const auto ball = speed > 1 && !capFixture
            ? world.CreateDynamicBox({0, 0, z}, {.08f, .2f, .05f}, 1, 0, 1)
            : world.CreateDynamicSphere({0, 0, z}, .05f, 1, 0, 1);
        if (speed > 1 && !capFixture) world.SetAngularVelocity(ball, {0, 0, 100});
        world.SetLinearVelocity(ball, {speed, 0, 0});
        balls.push_back(ball);
    }
    const auto energy = [&] {
        double total=0;
        for (const auto body:balls) {
            const glm::dvec3 v(world.GetLinearVelocity(body)),w(world.GetAngularVelocity(body));
            total+=.5*glm::dot(v,v)+.5*glm::dot(w,glm::dmat3(world.GetInertiaWorld(body))*w);
        }
        return total;
    };
    const double incoming=energy();
    auto& profiler = PerformanceProfiler::Get();
    profiler.Enable(recording);
    if (recording) { profiler.Clear(); profiler.BeginFrame("MK2 reporting fixture"); }
    world.StepCoupled(1.f / 60, intervals, {}, {});
    Result result;
    result.stats = world.LastStepStats();
    result.energyRatio = energy()/incoming;
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
        // Preserve the original spinning-box fixture as an energy regression.
        // Its old excessive response is no longer a valid cap-positive control.
        const auto repaired = Run(1, 100, 1, true);
        Check(repaired.energyRatio<=1.0001,"original spinning-box fixture creates no energy");
        // At 1000 m/s, a centered elastic sphere must traverse more than four
        // corridor widths in 1/60 s. This independently demands the body cap.
        const auto bounded = Run(1, 1000, 1, true, true);
        Check(bounded.heldPaths && bounded.stats.impactSafetyFallback,
              "a stationary remaining ledger segment with retained velocity reports a real hold");
        Check(bounded.stats.impactBodyCapHolds > 0 && bounded.stats.impactStepCapHolds == 0,
              "one fast bouncing body reaches the per-body limit, not the global limit");
        Check(glm::length(bounded.velocities.front()) > 1 && std::abs(bounded.poses.front().x) < .56f,
              "safety hold keeps physical speed and collision-bounded endpoint");
        const auto many = Run(300, 1000, 1, true, true);
        Check(many.heldPaths > 0 && many.stats.impactStepCapHolds > 0,
              "many independent fast bodies exercise the unchanged global safety bound");
        Check(many.energyRatio<=1.0001 && many.restitutionFallbacks==0,
              "independent elastic cap control neither gains energy nor hides work with inelastic fallback");
        const auto unrecorded = Run(300, 1000, 1, false, true);
        Check(many.poses == unrecorded.poses && many.velocities == unrecorded.velocities,
              "recording leaves actual physical endpoints and velocities exactly unchanged");
        Run(300, 1000, 2, true, true);
        {
            PhysicsWorld world;world.Init();
            const auto left=world.CreateStaticBox({-.1f,0,0},{.05f,1,1},0,1);
            const auto right=world.CreateStaticBox({.1f,0,0},{.05f,1,1},0,1);
            world.SetMotionType(left,BodyMotionType::Kinematic);
            world.SetMotionType(right,BodyMotionType::Kinematic);
            world.SetKinematicVelocity(left,{1,0,0},{0,0,0});
            world.SetKinematicVelocity(right,{-1,0,0},{0,0,0});
            world.CreateDynamicSphere({0,0,0},.05f,1,0,1);
            auto& profiler=PerformanceProfiler::Get();profiler.Enable(true);profiler.Clear();
            profiler.BeginFrame("infeasible prescribed closure");world.Step(1.f/60);profiler.EndFrame();
            double fallback=0;
            const auto history=profiler.History();
            for(const auto& counter:history.back().counters)
                if(std::string(counter.name.c_str())=="Physics restitution coupled fallbacks") fallback=counter.value;
            profiler.Enable(false);
            Check(fallback>0,"infeasible powered closure genuinely exercises bounded coupled fallback reporting");
        }
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
