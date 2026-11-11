#pragma once

#include <functional>
#include "ResourceTrace.h"

class EngineHost;
class RuntimeWorld;
class InteractivePlay;

// Optional automation/diagnostics for the ordinary application loop. Hooks
// supply events and observe real frames; they do not replace resource pumping,
// simulation, rendering or shutdown. A controlled frame clock makes assertions
// independent of machine speed. The executable uses the default null control.
struct ApplicationControl {
    ResourceTrace resourceTrace;
    bool hidden = false;
    std::function<void(EngineHost&)> hostReady;
    std::function<void(EngineHost&, RuntimeWorld&, InteractivePlay&)> worldReady;
    // Startup-only observation, after drawing the splash, before swap; gameplay is frozen.
    std::function<void(EngineHost&, RuntimeWorld&)> loadingFrame;
    std::function<void(EngineHost&, RuntimeWorld&, InteractivePlay&)> beforeFrame;
    // Runs after rendering, before buffer swap (readback sees this frame).
    std::function<void(EngineHost&, RuntimeWorld&, InteractivePlay&)> afterFrame;
    std::function<void(EngineHost&, RuntimeWorld&, InteractivePlay&)> beforeShutdown;
    std::function<float(float measuredSeconds)> frameSeconds;
};

// Milestone 28: the `judas` runtime's orchestration — resolve options,
// bring up the engine host, load the selected scene file, instantiate it,
// begin an InteractivePlay session, and run either the interactive loop or the scripted
// test harness over it. Owns no scene content and no engine feature; see
// docs/ARCHITECTURE.md, "Milestone 28, Application responsibility split."
class Application {
public:
    // Runs the application to completion. Returns a process exit code
    // (0 on clean shutdown, non-zero if startup failed).
    int Run(int argc, char** argv, ApplicationControl* control = nullptr);
};
