#include "EditorApplication.h"
#include "EditorPlatformPlacement.h"
#include "../ExperimentalBrowser.h"
#include "AnimationTimeline.h"
#include "BrowserExportWorkflow.h"
#include "BuildIdentity.h"
#include "EditorAssetActions.h"
#include "EditorAutomation.h"
#include "EditorShortcuts.h"
#include "EditorToolPanels.h"
#include "EngineHost.h"
#include "EnginePaths.h"
#include "ModTools.h"
#include "PerformanceProfiler.h"
#include "RenderPreview.h"
#include "RetargetPanel.h"
#include "ScriptTools.h"
#include "SpriteSheetEditor.h"
#include "TerrainV2Editor.h"
#include "TilemapEditor.h"
#include "UtilityAIEditor.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
} // namespace

// Opt-in application integration fixture. Requests use the editor's ordinary
// authoring operations; Play advances only through InteractivePlay::Frame in
// the normal loop below. No test-specific simulation or resource path exists.

EditorProjectServices EditorApplication::ProjectServices() {
    return {m_sceneViewport,
            m_playSession,
            m_preferences,
            m_milestoneNotice,
            m_terrain,
            m_utilityAIEditor,
            m_spriteSheetEditor,
            m_quit,
            [this](std::string &error) { return StartPlay(error); },
            [this]() { StopPlay(); }};
}
EditorContext EditorApplication::Context(EditorRequests &requests) {
    return {m_document,
            m_project,
            m_panels,
            requests,
            *m_host,
            m_host->Assets(),
            m_playSession.world.get(),
            m_panels.mode == EditorMode::Edit ? EditorPanelPhase::Edit
            : m_panels.playPaused             ? EditorPanelPhase::Paused
                                              : EditorPanelPhase::Play};
}
void EditorApplication::RefreshProjectLists() {
    EditorRequests requests;
    auto context = Context(requests);
    m_projectWorkflow.RefreshProjectLists(context);
}
bool EditorApplication::OpenProject(const std::string &projectFile, std::string &outError) {
    EditorRequests requests;
    auto context = Context(requests);
    auto services = ProjectServices();
    return m_projectWorkflow.OpenProject(context, services, projectFile, outError);
}
bool EditorApplication::CreateProject(const std::string &directory, const std::string &name,
                                      std::string &outError, bool visibleStarter, int twoDTemplate) {
    EditorRequests requests;
    auto context = Context(requests);
    auto services = ProjectServices();
    return m_projectWorkflow.CreateProject(context, services, directory, name, outError, visibleStarter,
                                           twoDTemplate);
}
std::string EditorApplication::ResolveScenePath(const std::string &input) {
    EditorRequests requests;
    auto context = Context(requests);
    return m_projectWorkflow.ResolveScenePath(context, input);
}

bool EditorApplication::OpenScene(const std::string &path, std::string &outError) {
    EditorRequests requests;
    auto context = Context(requests);
    auto services = ProjectServices();
    return m_projectWorkflow.OpenScene(context, services, path, outError);
}
bool EditorApplication::RunProject(std::string &outMessage) {
    EditorRequests requests;
    auto context = Context(requests);
    return m_projectWorkflow.RunProject(context, outMessage);
}
bool EditorApplication::StartPlay(std::string &error) {
    if (m_terrain && !m_terrain->Guard(m_panels)) {
        error = m_panels.status;
        return false;
    }

    EditorRequests requests;
    auto context = Context(requests);
    const bool started = m_playSession.StartPlay(context, m_sceneViewport, m_scriptTools, error);
    if (started) {
        m_panels.showGameView = true;
        m_shell.views.RequestGameFocus();
    }
    return started;
}
void EditorApplication::StopPlay() {
    EditorRequests requests;
    auto context = Context(requests);
    m_playSession.StopPlay(context, m_sceneViewport);
    m_panels.showSceneView = true;
    m_shell.views.RequestSceneFocus();
}
void EditorApplication::HandleRequests(EditorRequests &requests) {
    auto context = Context(requests);
    auto services = ProjectServices();
    m_projectWorkflow.HandleRequests(context, services, requests);
}
void EditorApplication::ViewportRay(int x, int y, glm::vec3 &origin, glm::vec3 &direction) {
    EditorRequests requests;
    auto context = Context(requests);
    m_sceneViewport.ViewportRay(context, x, y, origin, direction);
}
void EditorApplication::PickAtPixel(int x, int y) {
    EditorRequests requests;
    auto context = Context(requests);
    m_sceneViewport.PickAtPixel(context, x, y);
}
void EditorApplication::UpdateGizmo(bool allow) {
    EditorRequests requests;
    auto context = Context(requests);
    m_sceneViewport.UpdateGizmo(context, allow);
}
void EditorApplication::DrawEditOverlay(Renderer &renderer, const Scene &scene) {
    EditorRequests requests;
    auto context = Context(requests);
    m_sceneViewport.DrawEditOverlay(context, renderer, scene);
}
void EditorApplication::CancelCameraNavigation() {
    EditorRequests requests;
    auto context = Context(requests);
    m_sceneViewport.CancelCameraNavigation(context);
}
void EditorApplication::UpdateCameraNavigation(const EditorCameraGestureInput &input, int x, int y, float dt,
                                               bool fast) {
    EditorRequests requests;
    auto context = Context(requests);
    m_sceneViewport.UpdateCameraNavigation(context, input, x, y, dt, fast);
}
void EditorApplication::FrameEditMode(float dt) {
    EditorRequests requests;
    auto context = Context(requests);
    m_sceneViewport.FrameEditMode(context, dt, m_terrain.get(), m_tilemapEditor.get());
}
void EditorApplication::DrawModelImportPreview(float dt) {
    EditorRequests requests;
    auto context = Context(requests);
    m_assetPreview.DrawModelImportPreview(context, dt);
}
void EditorApplication::RefreshAssetDemand() {
    EditorRequests requests;
    auto context = Context(requests);
    m_assetPreview.RefreshAssetDemand(context);
}
void EditorApplication::DrawUIAuthoringPreview() {
    EditorRequests requests;
    auto context = Context(requests);
    m_assetPreview.DrawUIAuthoringPreview(context);
}

void EditorApplication::LoadEditorPreferences() {
    m_preferences.LoadUserSettings(m_panels);
}
void EditorApplication::SaveEditorPreferences() {
    m_preferences.SaveUserSettings(m_panels);
}
void EditorApplication::CollectProfilerData(float dt) {
    EditorRequests r;
    auto c = Context(r);
    m_profilerSnapshot.Collect(c, m_playSession, dt);
}

EditorApplication::EditorApplication() = default;
EditorApplication::~EditorApplication() {
    Shutdown();
}

int EditorApplication::Run(int argc, char **argv) {
    ProfileRun profileRun("editor");
    ProfileFrame startupProfile("editor startup", true);
    std::vector<char *> launchArguments;
    std::string launchError;
    if (!ParseExperimentalBrowserArguments(argc, argv, m_panels.experimentalBrowserSupport, launchArguments,
                                           launchError)) {
        std::fprintf(stderr, "%s\n", launchError.c_str());
        return 1;
    }
    argc = int(launchArguments.size());
    argv = launchArguments.data();
    if (argc > 2) {
        std::fprintf(stderr,
                     "usage: judas_editor [-ExpBrowserSupport=1] [project.judasproj | scene.judas]\n");
        return 1;
    }
    std::string error;
    m_ownedHost = std::make_unique<EngineHost>();
    EngineHost &host = *m_ownedHost;
    const auto editorTitle = std::string("Judas Editor V2 — ") + GetBuildIdentity().releaseName + " " +
                             GetBuildIdentity().version + " [" + GetBuildIdentity().channel + "] " +
                             GetBuildIdentity().buildId;
    if (!host.Init(editorTitle.c_str(), kWindowWidth, kWindowHeight, true, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    m_host = &host;
    Window &window = host.GetWindow();
    Renderer &renderer = host.GetRenderer();
    SDL_SetWindowMinimumSize(window.NativeWindow(), 640, 480);
    window.SetMouseCaptured(false);
    // An explicit desktop launch option, also used for full-resolution visual
    // qualification. The window manager owns work-area/maximization behavior.
    if (const char *maximized = std::getenv("JUDAS_EDITOR_MAXIMIZED");
        maximized && std::string(maximized) == "1")
        SDL_MaximizeWindow(window.NativeWindow());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    m_guiContextOwned = true;
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    const auto editorFont = ResolveEngineDataPath("assets/fonts/DejaVuSans.ttf");
    if (fs::is_regular_file(editorFont))
        io.Fonts->AddFontFromFileTTF(editorFont.c_str(), 15.0f);
    const auto editorHeading = ResolveEngineDataPath("assets/editor/DejaVuSans-Bold.ttf");
    if (fs::is_regular_file(editorHeading))
        io.Fonts->AddFontFromFileTTF(editorHeading.c_str(), 16.0f);
    LoadEditorPreferences();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    const char *driver = SDL_GetCurrentVideoDriver();
    if (driver && (std::string(driver) == "x11" || std::string(driver) == "windows"))
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    m_shell.RegisterPanels(m_sceneViewport, m_playSession,
                           {m_scriptTools, m_terrain, m_modTools, m_animationTimeline,
                            m_utilityAIEditor, m_spriteSheetEditor, m_tilemapEditor});
    if (!m_scriptTools)
        m_scriptTools = std::make_unique<ScriptTools>();
    m_scriptTools->drawStoppedShell = [this] {
        EditorRequests requests;
        auto c = Context(requests);
        m_shell.DrawStopped(c, m_preferences);
    };
    m_scriptTools->presentStoppedShell = [this, &host, &window] {
        m_shell.Present(host);
        window.SwapBuffers();
    };

    m_preferences.ApplyPending(m_panels);
    if (m_preferences.automaticUpdates && !std::getenv("JUDAS_EDITOR_AUTOTEST"))
        m_milestoneNotice.Check(false, m_preferences.updateChannel);
    m_guiPlatformReady = ImGui_ImplSDL2_InitForOpenGL(window.NativeWindow(), window.NativeGLContext());
    m_guiRendererReady = m_guiPlatformReady && ImGui_ImplOpenGL3_Init("#version 330 core");
    if (!m_guiRendererReady) {
        std::fprintf(stderr, "Editor UI backend initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    InstallEditorPlatformPlacement();
    window.SetEventHook([this,
                         editorWindowId = SDL_GetWindowID(window.NativeWindow())](const SDL_Event &event) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_MOUSEMOTION &&
            event.motion.windowID ==
                (m_shell.views.Scene().windowId ? m_shell.views.Scene().windowId : editorWindowId)) {
            m_sceneViewport.mouseDeltaX += event.motion.xrel;
            m_sceneViewport.mouseDeltaY += event.motion.yrel;
        } else if (event.type == SDL_WINDOWEVENT &&
                   event.window.windowID ==
                       (m_shell.views.Scene().windowId ? m_shell.views.Scene().windowId : editorWindowId)) {
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                m_sceneViewport.focused = false;
                if (m_panels.mode == EditorMode::Edit)
                    CancelCameraNavigation();
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                m_sceneViewport.focused = true;
        }
    });

    // --- Milestone 30: open a project, then a scene ---
    std::string argument = argc == 2 ? argv[1] : std::string();
    std::string projectFile;
    std::string sceneArgument;
    if (!argument.empty() && argument.size() > 10 &&
        argument.compare(argument.size() - 10, 10, ".judasproj") == 0) {
        projectFile = argument;
    } else if (!argument.empty()) {
        sceneArgument = argument;
        projectFile = Project::FindProjectFileFor(argument);
    } else {
        projectFile = Project::FindProjectFileFor(".");
    }
    if (const char *mode = std::getenv("JUDAS_RESOURCE_MODE")) {
        if (std::string(mode) == "blocking")
            host.Resources().SetBlockingMode(true);
    }
    RefreshProjectLists();
    if (!projectFile.empty()) {
        if (!OpenProject(projectFile, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        m_panels.status = "Opened project " + m_project.Settings().name;
    }
    if (!sceneArgument.empty()) {
        m_panels.status =
            OpenScene(sceneArgument, error) ? "Opened " + sceneArgument : "Open failed: " + error;
    } else if (m_project.IsLoaded() && !m_project.Settings().startupScene.empty()) {
        if (!OpenScene(m_project.Settings().startupScene, error))
            m_panels.status = "Startup scene failed: " + error;
        else
            m_panels.status =
                "Opened project " + m_project.Settings().name + " at " + m_project.Settings().startupScene;
    } else {
        m_document.NewScene();
        m_panels.status = m_project.IsLoaded() ? "Project has no startup scene; new scene. Create > objects; "
                                                 "right-drag to look, middle-drag to pan."
                                               : "No project. File > New project or Open project. Right-drag "
                                                 "to look, middle-drag to pan, WASD/QE to fly.";
    }
    if (m_panels.status.rfind("Opened", 0) == 0) {
        m_panels.status += ". Wheel zooms; right-drag looks; middle-drag pans; WASD/QE fly; click selects; "
                           "W/E/R transform; X local/world; Ctrl snaps.";
    }

    m_panels.showBrowserExport = m_panels.experimentalBrowserSupport;
    EditorAutomation automation(*this, host);
    if (!automation.Initialize())
        return 1;
    EditorRequests deferredRequests;
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    startupProfile.End();
    while (!window.ShouldClose() && !m_quit) {
        ProfileFrame outerProfile(m_panels.mode == EditorMode::Play ? "editor Play" : "editor edit");
        // Last frame's UI focus decides whether the engine's own key/mouse
        // reading is suppressed this frame (a text field must not walk the
        // player or fly the camera).
        if (m_panels.mode == EditorMode::Play)
            m_shell.views.ConfigureInput(window, true);
        else
            window.SetInputClaimed(io.WantCaptureKeyboard && !m_panels.sceneViewHovered,
                                   io.WantCaptureMouse && !m_panels.sceneViewHovered);
        window.SetHostTextInput(io.WantTextInput);
        m_sceneViewport.mouseDeltaX = m_sceneViewport.mouseDeltaY = 0;
        {
            JUDAS_PROFILE_SCOPE("Input events");
            window.PollEvents();
        }
        const bool closeRequested = window.ShouldClose();
        // A desktop close is handled like File > Quit, including the document
        // guard. The game runtime still exits on its own normal close request.
        if (closeRequested)
            window.CancelCloseRequest();
        host.PumpResources(); // M31: GPU upload of finished loads, budget eviction
        const Uint64 currentCounter = SDL_GetPerformanceCounter();
        if (m_scriptTools && m_scriptTools->clockReset) {
            previousCounter = currentCounter;
            m_scriptTools->clockReset = false;
        }
        const float deltaSeconds =
            static_cast<float>(currentCounter - previousCounter) / static_cast<float>(frequency);
        previousCounter = currentCounter;
        renderer.ResetStats();

        m_preferences.ApplyPending(m_panels);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        PollBrowserExportWorkflow(m_panels);
        EditorRequests requests;
        requests.quit = closeRequested;
        {
            auto shortcuts = Context(requests);
            RouteEditorShortcuts(shortcuts, m_sceneViewport.gesture);
        }

        static ProfileLabel editorBuildLabel("Editor UI build");
        ProfileScope editorBuildScope(editorBuildLabel);
        auto context = Context(requests);
        m_assetPreview.Pump(context);
        PumpEditorScriptTools(context, m_scriptTools);
        DrawEditorImportReview(m_panels, requests);
        m_preferences.Draw(m_panels);
        m_shell.Draw(context, m_preferences);
        if (m_preferences.ConsumeSaveRequest())
            SaveEditorPreferences();
        m_milestoneNotice.Draw(m_panels.status, m_preferences.updateChannel);
        editorBuildScope.End();
        m_shell.views.ConfigureInput(window, m_panels.mode == EditorMode::Play);
        if (m_panels.mode == EditorMode::Play) {
            const bool render = m_shell.views.ShouldRenderGame();
            const bool sceneVisible = m_shell.views.Scene().visible;
            const bool capture = m_shell.views.Game().focused &&
                                 (m_shell.views.Game().hovered || window.IsMouseCaptured()) &&
                                 !io.WantTextInput && !(m_panels.showProfiler && m_panels.profilerInspect);
            const bool resumed = m_playSession.Frame(context, m_scriptTools, deltaSeconds, render, capture,
                                                     &m_shell.views.GameTarget(), render || !sceneVisible);
            if (sceneVisible) {
                auto liveContext = Context(requests);
                m_sceneViewport.RenderLive(liveContext, m_playSession, deltaSeconds, !render);
            }
            if (resumed) {
                previousCounter = SDL_GetPerformanceCounter();
                auto refreshed = Context(requests);
                m_shell.Draw(refreshed, m_preferences);
            }
        } else if (m_shell.views.Scene().visible) {
            m_sceneViewport.focused = m_shell.views.Scene().focused;
            FrameEditMode(deltaSeconds);
        } else
            CancelCameraNavigation();
        CollectProfilerData(deltaSeconds);

        // Requests raised by the automation hook on the previous frame,
        // after that frame's UI had already been rendered.
        if (deferredRequests.play)
            requests.play = true;
        if (deferredRequests.stop)
            requests.stop = true;
        deferredRequests = EditorRequests{};
        // Export automation submits the same UI request during an active ImGui
        // frame. HandleRequests also owns modal UI and cannot run after Render.
        automation.BeforeRequests(requests);
        HandleRequests(requests);
        automation.AfterRequests();

        DrawModelImportPreview(deltaSeconds);
        UpdateRetargetPreview(m_panels, renderer, deltaSeconds);
        DrawUIAuthoringPreview();
        ImGui::Render();
        // Scene and Game have rendered into their owned targets. The shell
        // composes those images and panels into the main and floating windows.
        {
            JUDAS_PROFILE_SCOPE("Editor UI submission");
            RendererProfileScope uiGPU(renderer, "Editor UI");
            m_shell.Present(host);
        }

        m_assetPreview.DrawTextPreview(context);

        automation.AfterRender(deferredRequests);
        {
            JUDAS_PROFILE_WAIT("Swap present wait");
            window.SwapBuffers();
        }
    }

    Shutdown(true);
    return automation.Failed() || (m_tilemapEditor && m_tilemapEditor->TestFailures())
               ? 1
               : 0;
}

void EditorApplication::Shutdown(bool savePreferences) {
    if (!m_host)
        return;
    auto &host = *m_host;
    auto &window = host.GetWindow();
    auto &renderer = host.GetRenderer();
    SDL_GL_MakeCurrent(window.NativeWindow(), window.NativeGLContext());
    if (m_playSession.world)
        StopPlay();
    else
        CancelCameraNavigation();
    window.ClearInputRegion();
    window.SetEventHook(nullptr);
    if (m_scriptTools) {
        m_scriptTools->drawStoppedShell = {};
        m_scriptTools->presentStoppedShell = {};
    }
    m_shell.views.Destroy(renderer);
    m_sceneViewport.Destroy(renderer);
    if (m_terrain) {
        m_terrain->Shutdown(renderer, m_panels.importJobs);
        m_terrain.reset();
    }

    ShutdownRetargetPreview(m_panels, renderer);
    EditorRequests requests;
    auto context = Context(requests);
    m_assetPreview.Destroy(context);
    m_panels.importTask.reset();
    if (savePreferences && m_guiContextOwned)
        SaveEditorPreferences();
    if (m_guiRendererReady)
        ImGui_ImplOpenGL3_Shutdown();
    if (m_guiPlatformReady)
        ImGui_ImplSDL2_Shutdown();
    if (m_guiContextOwned)
        ImGui::DestroyContext();
    m_guiRendererReady = m_guiPlatformReady = m_guiContextOwned = false;
    m_host = nullptr;
}
