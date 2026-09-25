#include "Application.h"

#include <SDL2/SDL.h>

#include <cstdio>
#include <string>
#include <vector>

#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeDiagnostics.h"
#include "RuntimeOptions.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "TestHarness.h"
#include "WorldCoordinates.h"
#include "WorldState.h"
#include "WorldPresentation.h"
#include "ScreenshotWriter.h"

namespace {
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;
}  // namespace

int Application::Run(int argc, char** argv) {
    RuntimeOptions options;
    std::string error;
    if (!ParseRuntimeOptions(argc, argv, options, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    // The scene is parsed before any engine system exists so a bad file
    // is reported and exits cleanly rather than leaving partially built
    // state behind — the same fail-early rule every asset load follows.
    Scene scene;
    if (!LoadSceneFromFile(options.scenePath, scene, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    const WorldCoordinates worldCoordinates(
        options.worldOriginOverride ? *options.worldOriginOverride : scene.Settings().worldOrigin);

    EngineHost host;
    if (!host.Init("Project Judas", kWindowWidth, kWindowHeight, !options.IsTestRun(), error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    Window& window = host.GetWindow();
    Renderer& renderer = host.GetRenderer();
    // Milestone 30: the project's assets are the only ones a scene can
    // reference; a scene outside any project resolves nothing.
    if (options.project.IsLoaded()) {
        host.OpenProjectAssets(options.project.RootDir(), options.project.AssetsDir());
        for (const AssetProblem& problem : host.Assets().Problems()) {
            std::fprintf(stderr, "Asset problem: %s: %s\n", problem.path.c_str(), problem.message.c_str());
        }
    }

    RuntimeWorld world;
    if (!world.Build(scene, &host.Resources(), error)) {
        std::fprintf(stderr, "Scene '%s' could not be instantiated: %s\n", options.scenePath.c_str(),
                     error.c_str());
        return 1;
    }
    // Milestone 29: the saved world-state delta (if any) layers over the
    // freshly built baseline before the session begins.
    bool worldStateApplied = false;
    if (!ApplyWorldStateFileIfPresent(world, options.worldStatePath, worldStateApplied, error)) {
        std::fprintf(stderr, "World state '%s' could not be applied: %s\n", options.worldStatePath.c_str(),
                     error.c_str());
        return 1;
    }
    InteractivePlay play;
    if (!play.Begin(world, worldCoordinates, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    play.SetWorldStatePath(options.worldStatePath, worldStateApplied);
    if (!options.worldStatePath.empty()) {
        std::fprintf(stderr, "World state: %s (%s)\n", options.worldStatePath.c_str(),
                     worldStateApplied ? "loaded" : "none saved; F6 saves, F7 deletes");
    }
    std::fprintf(stderr, "Project: %s (%s)\nScene: %s (%s)\nWorld origin (m): %.3f, %.3f, %.3f\n",
                 options.project.IsLoaded() ? options.project.Settings().name.c_str() : "(none)",
                 options.project.IsLoaded() ? options.project.ProjectFile().c_str() : "scene outside a project",
                 scene.Settings().name.c_str(), options.scenePath.c_str(), worldCoordinates.Origin().x,
                 worldCoordinates.Origin().y, worldCoordinates.Origin().z);

    if (options.IsTestRun()) {
        // A harness screenshot shows exactly what the real game renders.
        const auto drawScene = [&](Renderer& r, float alpha) {
            r.SetLighting(glm::normalize(world.Settings().sunDirection), world.Settings().sunColor,
                          world.Settings().ambientColor);
            UpdateFluidSurface(r, world, alpha);
            DrawWorldGeometry(r, world, &play.Session(), alpha, WorldDrawOptions{});
            DrawWorldTransparents(r, world, &play.Session(), alpha);
        };
        return RunTestHarness(window, renderer, play.Session(), drawScene, options.testScriptPath);
    }

    RuntimeDiagnostics diagnostics(options.fluidDiagnostics, options.atmosphereDiagnostics,
                                   options.fireDiagnostics, options.liveTelemetry);
    play.SetFixedStepMeasurementFlags(options.atmosphereDiagnostics, options.fireDiagnostics,
                                      options.fluidDiagnostics);
    play.SetFixedStepObserver([&](const FixedStepMeasurements& m, double ms) {
        diagnostics.RecordFixedStep(play.Session(), m, ms);
    });

    bool screenshotWritten = false;
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    while (!window.ShouldClose() && !play.QuitRequested()) {
        window.PollEvents();
        const Uint64 currentCounter = SDL_GetPerformanceCounter();
        const float frameDeltaTime = static_cast<float>(currentCounter - previousCounter) / static_cast<float>(frequency);
        previousCounter = currentCounter;

        play.Frame(window, renderer, frameDeltaTime);
        if (play.ConsumeResetOccurred()) screenshotWritten = false;
        diagnostics.RecordTelemetryFrame(play.Session());
        diagnostics.RecordFrame(play.Session(), play.LastSurfaceMilliseconds(), play.LastSceneMilliseconds(),
                                frameDeltaTime);

        // Opt-in visual validation of a settled scene after ten seconds of
        // fixed-step simulation (M25), read back through the renderer.
        if (!options.terrainScreenshotPath.empty() && !screenshotWritten && play.FixedStepsSinceReset() >= 600) {
            std::vector<unsigned char> pixels;
            renderer.CaptureFrame(window.Width(), window.Height(), pixels);
            const bool written = WriteRgbPng(options.terrainScreenshotPath, window.Width(), window.Height(), pixels);
            std::fprintf(stderr, "Scene screenshot %s: %s\n", written ? "written" : "failed",
                         options.terrainScreenshotPath.c_str());
            screenshotWritten = true;
        }
        window.SwapBuffers();
    }
    return 0;
}
