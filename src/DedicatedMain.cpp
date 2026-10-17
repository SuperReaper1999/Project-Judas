#include "BuildIdentity.h"
#include "AssetDatabase.h"
#include "GameSession.h"
#include "InputSystem.h"
#include "JobSystem.h"
#include "LocalMultiplayer.h"
#include "NetworkSession.h"
#include "PerformanceProfiler.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "Simulation.h"
#include "SimulationTiming.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
volatile std::sig_atomic_t stopRequested = 0;

void RequestStop(int) {
    stopRequested = 1;
}

unsigned ParsePort(const char* text) {
    const std::string value(text);
    unsigned port = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), port);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || !port || port > 65535)
        throw std::invalid_argument("port must be an integer in 1..65535");
    return port;
}

double ParseDuration(const char* text) {
    char* end = nullptr;
    const double seconds = std::strtod(text, &end);
    if (end == text || *end || !std::isfinite(seconds) || seconds < 0)
        throw std::invalid_argument("duration must be finite nonnegative seconds (0 runs until interrupted)");
    return seconds;
}

int RunDedicated(const std::string& projectPath, unsigned port, double duration) {
    using Clock = std::chrono::steady_clock;
    ProfileRun profile("dedicated");
    Project project;
    std::string error;
    if (!project.Load(projectPath, error)) throw std::runtime_error("Project: " + error);
    if (project.Settings().legacyGameplay)
        throw std::runtime_error("dedicated hosting requires legacy-gameplay false");
    if (!project.Settings().worldManifest.empty())
        throw std::runtime_error("dedicated hosting of composed/streamed worlds is not supported yet");
    if (!project.Settings().modContract.empty() || project.ModSession())
        throw std::runtime_error("dedicated hosting of mod-enabled projects is not supported yet");
    if (project.StartupScenePath().empty())
        throw std::runtime_error("dedicated hosting requires an authored startup scene");

    Scene scene;
    if (!LoadSceneFromFile(project.StartupScenePath(), scene, error))
        throw std::runtime_error("Scene: " + error);
    AssetDatabase assets;
    assets.Scan(project.RootDir(), project.AssetsDir());
    for (const auto& problem : assets.Problems())
        std::fprintf(stderr, "Asset problem: %s: %s\n", problem.path.c_str(), problem.message.c_str());

    // Deliberately no EngineHost, Window, SDL initialization, renderer, graphics
    // context or audio device. Required collision/script resources still use
    // the normal asset database and owner-thread worker publication service.
    // Declaration order keeps worlds and script teardown ahead of resources,
    // and resources ahead of the worker pool even if startup throws.
    JobSystem jobs(2);
    ResourceManager resources(nullptr, &assets, &jobs);
    auto control = std::make_shared<SceneSession>(project, project.StartupScenePath());
    RuntimeWorld world;
    world.legacyGameplay = false;
    world.audioGroups = project.Settings().audio;
    world.SetSceneControl(control);
    if (!world.Build(scene, &resources, error, &project.Settings().classification,
                     &project.Settings().navigation))
        throw std::runtime_error("World: " + error);
    world.Network().SetLaunch("host", "0.0.0.0", port, true);
    GameSession session;
    if (!session.Begin(world, error)) throw std::runtime_error("Session: " + error);
    InputSystem input;
    if (!input.SetMap(project.Settings().input, error))
        throw std::runtime_error("Input: " + error);
    std::signal(SIGINT, RequestStop);
    std::signal(SIGTERM, RequestStop);
    std::printf("Judas dedicated initialized: project=%s port=%u fixedHz=60 duration=%.3f\n",
                project.ProjectFile().c_str(), port, duration);
    std::printf("No display, renderer or audio device; project scripts own host startup and gameplay.\n");
    std::fflush(stdout);

    const auto started = Clock::now();
    auto previous = started;
    double accumulator = 0;
    std::uint64_t steps = 0;
    double discardedSeconds = 0;
    while (!stopRequested) {
        const auto now = Clock::now();
        const double elapsed = std::chrono::duration<double>(now - started).count();
        if (duration > 0 && elapsed >= duration) break;
        ProfileFrame frame("dedicated");
        const auto delta = std::chrono::duration<double>(now - previous).count();
        previous = now;
        const auto frameSeconds = std::min(delta, double(SimulationTiming::kMaxFrameDeltaTime));
        input.BeginFrame();
        resources.Pump();
        world.PumpNetwork(elapsed);
        world.UpdateScripts(&input, static_cast<float>(frameSeconds));
        accumulator += frameSeconds;
        int stepsThisFrame = 0;
        while (accumulator >= SimulationTiming::kFixedTimestep &&
               stepsThisFrame < SimulationTiming::kMaxPhysicsStepsPerFrame) {
            input.BeginFixedStep();
            if (auto* players = world.PlayersIfLoaded()) players->BeginFixedStep();
            StepAuthoritativeWorld(session, input, SimulationTiming::kFixedTimestep);
            accumulator -= SimulationTiming::kFixedTimestep;
            ++stepsThisFrame;
            ++steps;
        }
        const bool capped = stepsThisFrame == SimulationTiming::kMaxPhysicsStepsPerFrame;
        const auto discarded = capped ? accumulator : 0;
        PerformanceProfiler::Get().FixedState(accumulator, capped, discarded, false);
        if (capped) {
            discardedSeconds += accumulator;
            accumulator = 0;
        }
        // Wall-time pacing matches the ordinary 60 Hz simulation. Never run a
        // busy synthetic-time server or accumulate an unbounded catch-up queue.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    world.EndScripts();
    session.End();
    world.Destroy();
    resources.Shutdown();
    jobs.Shutdown();
    std::printf("Judas dedicated stopped: fixedSteps=%llu discardedCatchupSeconds=%.6f\n",
                static_cast<unsigned long long>(steps), discardedSeconds);
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    if (HandleBuildIdentityArguments(argc, argv, "server")) return 0;
    if (argc < 2 || argc > 4) {
        std::fprintf(stderr, "Usage: judas_server project.judasproj [port=27999] [seconds=0]\n");
        return 1;
    }
    try {
        return RunDedicated(argv[1], argc >= 3 ? ParsePort(argv[2]) : 27999,
                            argc >= 4 ? ParseDuration(argv[3]) : 0);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Judas dedicated: %s\n", error.what());
        return 1;
    }
}
