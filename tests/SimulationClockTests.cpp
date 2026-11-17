#include "EngineHost.h"
#include "InteractivePlay.h"
#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include "SimulationTiming.h"
#include <cmath>
#include <cstdio>

int main() {
    unsigned failures = 0;
    auto check = [&](bool ok, const char* label) {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
        failures += !ok;
    };
    PerformanceProfiler::Get().Enable(false);
    EngineHost host;
    std::string error;
    if (!host.Init("Clock observation contract", 64, 64, false, error)) {
        std::puts(error.c_str()); return 1;
    }
    Scene scene;
    auto& object = scene.CreateObject("moving body");
    object.body = SceneBodyComponent{};
    object.body->motion = SceneBodyMotion::Dynamic;
    object.body->managed = true;
    object.body->initialLinearVelocity = {1, 0, 0};
    RuntimeWorld world;
    if (!world.Build(scene, &host.Resources(), error)) { std::puts(error.c_str()); return 1; }
    InteractivePlay play;
    if (!play.Begin(world, WorldCoordinates{}, error)) return 1;
    play.Frame(host.GetWindow(), host.GetRenderer(), .001f, false);
    check(play.LastFixedStepsThisFrame() == 0 &&
          std::abs(play.LastClockState().accumulatorSeconds - .001f) < 1e-6,
          "sub-step backlog remains visible with profiling disabled");
    const auto before = world.Physics().GetTransform(world.RuntimeBody(object.id)).position;
    play.Frame(host.GetWindow(), host.GetRenderer(), 1.f, false);
    const auto clock = play.LastClockState();
    const double advanced = SimulationTiming::kMaxPhysicsStepsPerFrame *
                            double(SimulationTiming::kFixedTimestep);
    check(play.LastFixedStepsThisFrame() == SimulationTiming::kMaxPhysicsStepsPerFrame && clock.capReached,
          "observes the actual unchanged catch-up cap");
    check(std::abs(clock.incomingClippedSeconds - .75) < 1e-6 &&
          std::abs(clock.discardedSeconds - (.251 - advanced)) < 1e-6,
          "both clipping and discarded backlog are independently accounted");
    const auto after = world.Physics().GetTransform(world.RuntimeBody(object.id)).position;
    check(std::abs((after.x - before.x) - advanced) < 1e-5,
          "body advances only the reported eight real fixed steps");
    play.Frame(host.GetWindow(), host.GetRenderer(), 0.f, false);
    check(play.LastFixedStepsThisFrame() == 0 && !play.LastClockState().capReached &&
          play.LastClockState().discardedSeconds == 0 && play.LastClockState().incomingClippedSeconds == 0,
          "next frame does not replay cumulative loss counters");
    play.End(); world.Destroy(); host.Shutdown();
    return failures ? 1 : 0;
}
