#include "PerformanceProfiler.h"
#include "Application.h"
#include "AppIcon.h"

#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <stdexcept>

#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeDiagnostics.h"
#include "RuntimeOptions.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "WorldStreaming.h"
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

int Application::Run(int argc, char** argv, ApplicationControl* control) {
    ProfileRun profileRun("standalone");
    ProfileFrame startupProfile("standalone startup",true);
    RuntimeOptions options;
    std::string error;
    if (!ParseRuntimeOptions(argc, argv, options, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    if (control && options.IsTestRun()) {
        std::fprintf(stderr, "ApplicationControl requires the normal async loop, not JUDAS_TEST_SCRIPT.\n");
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
    if (!host.Init(options.project.IsLoaded() ? options.project.Settings().name.c_str() : "Project Judas", kWindowWidth, kWindowHeight, !options.IsTestRun() && !(control && control->hidden), error,
                   control ? control->resourceTrace : ResourceTrace{})) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    Window& window = host.GetWindow();
    Renderer& renderer = host.GetRenderer();
    // Milestone 30: the project's assets are the only ones a scene can
    // reference; a scene outside any project resolves nothing.
    if (options.project.IsLoaded()) {
        host.OpenProjectAssets(options.project.RootDir(), options.project.AssetsDir());

        const auto& iconId = options.project.Settings().iconAsset;
        if (!iconId.empty()) {
            const auto* icon = host.Assets().Find(iconId);
            std::string iconError;
            if (!icon || icon->missing || icon->type != AssetType::Texture)
                std::fprintf(stderr, "Missing project app icon %s; using Judas icon.\n", iconId.c_str());
            else if (!ApplyAppIcon(window.NativeWindow(), icon->path, iconError))
                std::fprintf(stderr, "Project app icon: %s; using Judas icon.\n", iconError.c_str());
        }
        if(!host.GetWindow().Input().SetMap(options.project.Settings().input,error)){std::fprintf(stderr,"Input map: %s\n",error.c_str());return 1;}
        for (const AssetProblem& problem : host.Assets().Problems()) {
            std::fprintf(stderr, "Asset problem: %s: %s\n", problem.path.c_str(), problem.message.c_str());
        }
    }

    // Milestone 31: assets load asynchronously and presentation shows a
    // placeholder until they are Ready. The scripted harness and
    // JUDAS_RESOURCE_MODE=blocking use the synchronous path instead (a
    // deterministic screenshot, and the M31 synchronous-versus-asynchronous
    // measurement).
    const char* resourceMode = std::getenv("JUDAS_RESOURCE_MODE");
    if (options.IsTestRun() || (resourceMode && std::string(resourceMode) == "blocking")) {
        host.Resources().SetBlockingMode(true);
    }

    if (control && control->hostReady) control->hostReady(host);

    auto worldOwner=std::make_unique<RuntimeWorld>();
    auto sceneControl=std::make_shared<SceneSession>(options.project,options.scenePath);
    sceneControl->EnableSaves(!options.worldStatePath.empty());
    if(options.project.IsLoaded())worldOwner->audioGroups=options.project.Settings().audio;
    worldOwner->legacyGameplay = !options.project.IsLoaded() || options.project.Settings().legacyGameplay;
    worldOwner->SetSceneControl(sceneControl);
    RuntimeWorld& world=*worldOwner;
    if (!world.Build(scene, &host.Resources(), error, options.project.IsLoaded()?&options.project.Settings().classification:nullptr, options.project.IsLoaded()?&options.project.Settings().navigation:nullptr)) {
        std::fprintf(stderr, "Scene '%s' could not be instantiated: %s\n", options.scenePath.c_str(),
                     error.c_str());
        return 1;
    }
    // Milestone 29: the saved world-state delta (if any) layers over the
    // freshly built baseline before the session begins.
    bool worldStateApplied = false;
    if (!world.IsComposed()&&!ApplyWorldStateFileIfPresent(world, options.worldStatePath, worldStateApplied, error)) {
        std::fprintf(stderr, "World state '%s' could not be applied: %s\n", options.worldStatePath.c_str(),
                     error.c_str());
        return 1;
    }
    InteractivePlay play;
    if (!play.Begin(world, worldCoordinates, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    play.SetWorldStatePath(world.IsComposed()?std::string{}:options.worldStatePath, worldStateApplied);
    if(world.IsComposed())std::fprintf(stderr,"World state: legacy .judasstate (F6/F7) saves disabled for additive compositions; save slots remain available\n");
    else if (!options.worldStatePath.empty()) {
        std::fprintf(stderr, "World state: %s (%s)\n", options.worldStatePath.c_str(),
                     worldStateApplied ? "loaded" : "none saved");
    }
    std::fprintf(stderr, "Project: %s (%s)\nScene: %s (%s)\nWorld origin (m): %.3f, %.3f, %.3f\n",
                 options.project.IsLoaded() ? options.project.Settings().name.c_str() : "(none)",
                 options.project.IsLoaded() ? options.project.ProjectFile().c_str() : "scene outside a project",
                 scene.Settings().name.c_str(), options.scenePath.c_str(), worldCoordinates.Origin().x,
                 worldCoordinates.Origin().y, worldCoordinates.Origin().z);

    if (options.IsTestRun()) {
        startupProfile.End();
        return RunTestHarness(window, renderer, play, options.testScriptPath,
            [&]{host.PumpResources();},
            [&]{std::string boundaryError;sceneControl->AdvanceOuter(worldOwner,play,host.Resources(),window.Input(),boundaryError);
                if(!boundaryError.empty())throw std::runtime_error(boundaryError);
                // Workers progress on wall time even with a synthetic test clock.
                if(sceneControl->Pending())SDL_Delay(1);
            },[&](const std::string& service){
                if(service=="scene")return !sceneControl->Pending();
                if(service=="save")return !sceneControl->Saves(host.Resources())->Busy();
                if(service=="stream"){auto* s=sceneControl->StreamingIfLoaded();return !s||(s->SaveReady()&&s->Stats().pending==0);}
                throw std::runtime_error("unknown service wait: "+service);
            });
    }

    if (control && control->worldReady) control->worldReady(host, world, play);

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
    startupProfile.End();
    while (!window.ShouldClose() && !play.QuitRequested()) {
        ProfileFrame outerProfile("standalone");
        RuntimeWorld& world=*worldOwner;
        if (control && control->beforeFrame) control->beforeFrame(host, world, play);
        { JUDAS_PROFILE_SCOPE("Input events"); window.PollEvents(); }
        // Milestone 31: finished background loads are installed here, on
        // the GL thread, before the frame that will draw them.
        host.PumpResources();
        const Uint64 currentCounter = SDL_GetPerformanceCounter();
        float frameDeltaTime = static_cast<float>(currentCounter - previousCounter) / static_cast<float>(frequency);
        previousCounter = currentCounter;
        if (control && control->frameSeconds) frameDeltaTime = control->frameSeconds(frameDeltaTime);

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
        if (control && control->afterFrame) control->afterFrame(host, world, play);
        { JUDAS_PROFILE_WAIT("Swap present wait"); window.SwapBuffers(); }
        if(!sceneControl->AdvanceOuter(worldOwner,play,host.Resources(),window.Input(),error)||!error.empty()){
            std::fprintf(stderr,"Runtime outer services: %s\n",error.c_str());error.clear();
        }
    }
    if (control && control->beforeShutdown) control->beforeShutdown(host, *worldOwner, play);
    return 0;
}
