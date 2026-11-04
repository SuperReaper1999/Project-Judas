#include "EditorPlaySession.h"
#include "AssetBrowserPanel.h"
#include "EditorContext.h"
#include "EditorDocument.h"
#include "EditorPanels.h"
#include "EditorSceneViewport.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "ModAuthoring.h"
#include "ModelArchive.h"
#include "PerformanceProfiler.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "ScriptTools.h"
#include "WorldCoordinates.h"
#include "WorldDebugView.h"
#include "WorldState.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
#include "EditorSceneViewport.h"
EditorPlaySession::EditorPlaySession() = default;
EditorPlaySession::~EditorPlaySession() = default;
bool EditorPlaySession::StartPlay(EditorContext &context, EditorSceneViewport &viewport,
                                  std::unique_ptr<ScriptTools> &scriptTools, std::string &outError) {
    JUDAS_PROFILE_SCOPE("Editor Play startup");
    {
        JUDAS_PROFILE_SCOPE("Editor Play model preflight");
        // Each Play rechecks current bytes. Repeated scene instances share
        // only this check, never a freshness decision from an earlier Play.
        std::unordered_set<std::string> checkedModels;
        for (const auto &object : context.document.GetScene().Objects()) {
            if (!object.render || object.render->meshAsset.empty())
                continue;
            const auto *asset = context.host.Assets().Find(object.render->meshAsset);
            if (!asset || asset->readOnly || fs::path(asset->path).extension() != ".judasmodel" ||
                !checkedModels.insert(asset->path).second)
                continue;
            if (!VerifyTerrainModelFreshForPlay(context.project.RootDir(), asset->path, outError)) {
                outError = asset->relativePath + ": " + outError;
                return false;
            }
        }
        JUDAS_PROFILE_COUNTER("Editor Play model archives checked", double(checkedModels.size()),
                              ProfileCounterMode::Latest);
    }
    Project playedProject = context.project;
    if (!JudasMods::PrepareSession(playedProject, false, outError))
        return false;
    Scene playedScene = context.document.GetScene();
    if (playedProject.ModSession()) {
        const auto logical = context.project.MakeRelative(context.document.Path());
        if (playedProject.ModSession()->sceneFiles.count(logical) &&
            !LoadSceneFromFile(playedProject.Resolve(logical), playedScene, outError))
            return false;
        ResetEditorAssetThumbnails(context.panels);
        context.host.OpenContentView(playedProject.ModSession()->assets);
        context.project.SetModSession(playedProject.ModSession());
    }
    viewport.CancelCameraNavigation(context);
    world = std::make_unique<RuntimeWorld>();
    world->legacyGameplay = context.project.Settings().legacyGameplay;
    world->audioGroups = context.project.Settings().audio;
    world->ConfigureProject(context.project.Settings());
    world->SetSceneControl(std::make_shared<SceneSession>(context.project, context.document.Path()));
    world->SceneControl()->SetEditorSaveIsolation(true);
    if (!world->Build(playedScene, &context.host.Resources(), outError,
                      &context.project.Settings().classification, &context.project.Settings().navigation)) {
        StopPlay(context, viewport);
        return false;
    }
    // Milestone 29: a saved world-state delta for this scene file layers
    // over the freshly instantiated baseline, exactly as the runtime does.
    context.panels.worldStatePath =
        (context.document.Path().empty() ? std::string()
         : context.project.IsLoaded()    ? context.project.WorldStatePathForScene(context.document.Path())
                                         : DefaultWorldStatePath(context.document.Path()));
    bool stateApplied = false;
    if (world->IsComposed())
        context.panels.worldStatePath.clear();
    if (!ApplyWorldStateFileIfPresent(*world, context.panels.worldStatePath, stateApplied, outError)) {
        StopPlay(context, viewport);
        return false;
    }
    if (context.project.IsLoaded() &&
        !context.host.GetWindow().Input().SetMap(context.project.Settings().input, outError)) {
        StopPlay(context, viewport);
        return false;
    }
    if (!scriptTools)
        scriptTools = std::make_unique<ScriptTools>();
    scriptTools->Attach(*world, *(&context.host));
    play = std::make_unique<InteractivePlay>();
    if (!play->Begin(*world, WorldCoordinates(playedScene.Settings().worldOrigin), outError)) {
        play.reset();
        StopPlay(context, viewport);
        return false;
    }
    viewport.BeginObservation(context.document.GetScene().Settings().worldOrigin);
    play->SetScriptFaultNoticeEnabled(true);
    play->SetWorldStatePath(context.panels.worldStatePath, stateApplied);
    // The debug view over the played world, inside the 3D frame.
    play->SetWorldOverlay([this, panels = &context.panels](Renderer &renderer, float alpha) {
        if (!panels->debug.AnyEnabled() || !world)
            return;
        debugLines.Clear();
        BuildWorldDebugLines(*world, &play->Session(), alpha, panels->debug, debugLines);
        renderer.DrawDebugLines(debugLines.Lines());
    });
    context.panels.runtime = world.get();
    context.runtime = world.get();
    context.panels.renderPreviewHistory.reset();
    context.panels.mode = EditorMode::Play;
    context.panels.status = world->legacyGameplay
                                ? "Playing historical controls: Escape pauses; Stop restores authored state"
                                : "Playing project scripts; Stop restores authored state";
    // Keys pressed while editing (F to focus, R, Space, ...) must not fire
    // as gameplay requests on the first played frame, and a gizmo drag in
    // progress is abandoned.
    viewport.drag = GizmoDrag{};
    viewport.hoverAxis = GizmoAxis::None;
    if (context.document.EditInProgress())
        context.document.CancelEdit();
    context.host.GetWindow().ClearPendingRequests();
    context.host.GetWindow().SetMouseCaptured(world->legacyGameplay);
    return true;
}

void EditorPlaySession::StopPlay(EditorContext &context, EditorSceneViewport &viewport) {
    context.host.GetWindow().ClearInputRegion();
    viewport.CancelCameraNavigation(context);
    viewport.EndObservation(context);
    if (play)
        play->End();
    context.host.GetWindow().Input().StopRumble();
    play.reset();
    world.reset(); // the authored Scene was never written; nothing to revert
    if (context.project.ModSession()) {
        auto retiringContent = context.project.ModSession(); // ResourceManager release joins late readers
        ResetEditorAssetThumbnails(context.panels);
        context.host.OpenProjectAssets(context.project.RootDir(), context.project.AssetsDir());
        context.project.SetModSession({});
        std::string error;
        if (!JudasMods::OpenAuthorAssets(context.project, context.host.Assets(), error))
            context.panels.status = error;
    }
    context.panels.runtime = nullptr;
    context.panels.renderPreviewHistory.reset();
    context.panels.mode = EditorMode::Edit;
    context.panels.runtimeInfo.clear();
    context.panels.status = "Stopped: authored scene restored";
    context.host.GetWindow().SetMouseCaptured(false);
    context.host.GetWindow().Input().DiscardStickHistory();
}

bool EditorPlaySession::Frame(EditorContext &context, std::unique_ptr<ScriptTools> &scriptTools,
                              float deltaSeconds, bool render, bool allowCapture,
                              const RenderTargetHandle *destination) {
    auto &window = context.host.GetWindow();
    auto &renderer = context.host.GetRenderer();
    auto &host = context.host;
    bool clockReset = false;
    std::string error;
    if (!scriptTools)
        scriptTools = std::make_unique<ScriptTools>();
    scriptTools->Attach(*world, host);
    scriptTools->ApplyReload(*world);
    play->SetPointerCaptureAllowed(allowCapture);
    // The identical frame the runtime runs. Escape opens the M13
    // pause menu, which releases the mouse for the editor panels.
    lastAlpha = play->Frame(window, renderer, deltaSeconds, /*drawHud=*/true, render, destination);
    if (scriptTools->clockReset) {
        clockReset = true;
        scriptTools->clockReset = false;
    }
    if (scriptTools->stopRequested) {
        context.requests.stop = true;
        scriptTools->stopRequested = false;
    }
    if (scriptTools->quitRequested) {
        context.requests.quit = true;
        scriptTools->quitRequested = false;
    }
    if (auto scenes = world->SceneControl()) {
        scenes->AdvanceStreaming(*world, play->IsPaused());
        auto *previousWorld = world.get();
        scenes->AdvanceSaves(world, *play, host.Resources(), error);
        // Clear the Load edge without turning a held confirm button into a new jump.
        if (previousWorld != world.get())
            window.Input().DiscardPending();
        if (!error.empty()) {
            std::fprintf(stderr, "Save service: %s\n", error.c_str());
            error.clear();
        }
        if (!scenes->Apply(world, *play, host.Resources(), error))
            std::fprintf(stderr, "Scene transition: %s\n", error.c_str());
        if (previousWorld != world.get())
            window.Input().DiscardStickHistory();
        context.panels.runtime = world.get();
        context.runtime = world.get();
        context.panels.worldStatePath = play->WorldStatePath();
    }
    context.panels.playPaused = play->IsPaused();
    if (play->QuitRequested())
        context.requests.stop = true;
    const GameSession &session = play->Session();
    const RuntimeWorld::LifecycleCounts counts = world->CountLifecycle();
    char info[200];
    std::snprintf(info, sizeof(info),
                  "entities %zu full / %zu coarse / %zu dormant / %zu destroyed | %zu physics bodies | %zu "
                  "particles | %s",
                  counts.full, counts.coarse, counts.dormant, counts.destroyed, counts.physicsBodies,
                  world->HasFluid() ? world->Fluid().Particles().size() : std::size_t{0},
                  session.IsPiloting()            ? "piloting"
                  : session.Player().IsGrounded() ? "grounded"
                                                  : "airborne");
    context.panels.runtimeInfo = info;

    return clockReset;
}
