#include "NetworkSession.h"
#include "PerformanceProfiler.h"
#include "Application.h"
#include "AppIcon.h"
#include "modding/ModRuntime.h"

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
#include "WorldPresentation.h"
#include "ScreenshotWriter.h"

int Application::Run(int argc, char** argv, ApplicationControl* control) {
    ProfileRun profileRun("standalone");
    ProfileFrame startupProfile("standalone startup",true);
    RuntimeOptions options;
    std::string error;
    if (!ParseRuntimeOptions(argc, argv, options, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

#ifdef __EMSCRIPTEN__
    if(!options.project.IsLoaded()||options.project.Settings().advancedFluidDynamics) {
        std::fprintf(stderr,"Experimental browser games require a project with script-owned gameplay and Advanced Fluid Dynamics disabled\n");
        return 1;
    }
#endif

    if (control && options.IsTestRun()) {
        std::fprintf(stderr, "ApplicationControl requires the normal async loop, not JUDAS_TEST_SCRIPT.\n");
        return 1;
    }

    // The scene is parsed before any engine system exists so a bad file
    // is reported and exits cleanly rather than leaving partially built
    // state behind — the same fail-early rule every asset load follows.
    const auto originalScene=options.project.MakeRelative(options.scenePath);
    if(!JudasMods::PrepareSession(options.project,options.noMods,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    if(options.project.ModSession())options.scenePath=options.project.Resolve(originalScene);
    Scene scene;
    if (!LoadSceneFromFile(options.scenePath, scene, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    const WorldCoordinates worldCoordinates(
        options.worldOriginOverride ? *options.worldOriginOverride : scene.Settings().worldOrigin);

    EngineHost host;
    if (!host.Init(options.project.IsLoaded() ? options.project.Settings().name.c_str() : "Project Judas", options.windowWidth, options.windowHeight, !options.IsTestRun() && !(control && control->hidden), error,
                   control ? control->resourceTrace : ResourceTrace{})) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    Window& window = host.GetWindow();
    Renderer& renderer = host.GetRenderer();
    // Milestone 30: the project's assets are the only ones a scene can
    // reference; a scene outside any project resolves nothing.
    if (options.project.IsLoaded()) {
        if(options.project.ModSession())host.OpenContentView(options.project.ModSession()->assets);
        else host.OpenProjectAssets(options.project.RootDir(), options.project.AssetsDir());

        host.Resources().ConfigureVisualStreaming(uint64_t(options.project.Settings().visualBudgetMiB)*1024*1024);
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
    sceneControl->EnableSaves(options.project.IsLoaded());
    if(options.project.IsLoaded())worldOwner->audioGroups=options.project.Settings().audio;
    if(options.project.IsLoaded())worldOwner->ConfigureProject(options.project.Settings());
    worldOwner->SetSceneControl(sceneControl);
    RuntimeWorld& world=*worldOwner;
    if (!world.Build(scene, &host.Resources(), error, options.project.IsLoaded()?&options.project.Settings().classification:nullptr, options.project.IsLoaded()?&options.project.Settings().navigation:nullptr)) {
        std::fprintf(stderr, "Scene '%s' could not be instantiated: %s\n", options.scenePath.c_str(),
                     error.c_str());
        return 1;
    }
    if(!options.networkMode.empty())world.Network().SetLaunch(options.networkMode,options.networkAddress,options.networkPort,false);
    InteractivePlay play;
    if (!play.Begin(world, worldCoordinates, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
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

    RuntimeDiagnostics diagnostics(options.fluidDiagnostics,
                                   options.fireDiagnostics, options.liveTelemetry);
    play.SetFixedStepMeasurementFlags(options.fireDiagnostics,
                                      options.fluidDiagnostics);
    play.SetFixedStepObserver([&](const FixedStepMeasurements& m, double ms) {
        diagnostics.RecordFixedStep(play.World(), m, ms);
    });

    const Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    startupProfile.End();
    bool applicationSuspended=false;
    while (!window.ShouldClose() && !play.QuitRequested()) {
        ProfileFrame outerProfile("standalone");
        RuntimeWorld& world=*worldOwner;
        if (control && control->beforeFrame) control->beforeFrame(host, world, play);
        RendererProfileScope wholeGPU(renderer,"Whole runtime frame");
        { JUDAS_PROFILE_SCOPE("Input events"); window.PollEvents(); }
        if(window.ShouldClose())break;
        if(window.ConsumeLifecycleClockReset())previousCounter=SDL_GetPerformanceCounter();
        const bool suspended=!window.ApplicationActive();
        if(suspended!=applicationSuspended){host.Audio().SetApplicationSuspended(suspended);applicationSuspended=suspended;}
        if(suspended){
            // No physics catch-up, rendering or input leakage while backgrounded.
            // Dispatched immutable save writes may finish on workers. Defer
            // owner publication/load/resource work until a foreground boundary.
            previousCounter=SDL_GetPerformanceCounter();
            SDL_Delay(16);continue;
        }
        // Milestone 31: finished background loads are installed here, on
        // the GL thread, before the frame that will draw them.
        host.PumpResources();
        const Uint64 currentCounter = SDL_GetPerformanceCounter();
        float frameDeltaTime = static_cast<float>(currentCounter - previousCounter) / static_cast<float>(frequency);
        previousCounter = currentCounter;
        if (control && control->frameSeconds) frameDeltaTime = control->frameSeconds(frameDeltaTime);

        play.Frame(window, renderer, frameDeltaTime);
        diagnostics.RecordTelemetryFrame(play.World());
        diagnostics.RecordFrame(play.World(), play.LastSurfaceMilliseconds(), play.LastSceneMilliseconds(),
                                frameDeltaTime);

        if (control && control->afterFrame) control->afterFrame(host, world, play);
        { JUDAS_PROFILE_WAIT("Swap present wait"); window.SwapBuffers(); }
        if(!sceneControl->AdvanceOuter(worldOwner,play,host.Resources(),window.Input(),error)||!error.empty()){
            std::fprintf(stderr,"Runtime outer services: %s\n",error.c_str());error.clear();
        }
    }
    if (control && control->beforeShutdown) control->beforeShutdown(host, *worldOwner, play);
    return 0;
}
