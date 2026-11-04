#include "EditorAutomation.h"
#include "../authoring/AuthoringDocument.h"
#include "AnimationTimeline.h"
#include "AuthoringJSON.h"
#include "ComponentEditors.h"
#include "EditorApplication.h"
#include "EditorStabilization.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "ModAuthoring.h"
#include "ModelCook.h"
#include "PerformanceProfiler.h"
#include "Prefab.h"
#include "RetargetPanel.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "ScreenshotWriter.h"
#include "ScriptTools.h"
#include "SpriteSheetEditor.h"
#include "TerrainTools.h"
#include "TilemapEditor.h"
#include "UtilityAIEditor.h"
#include "VolumeTerrainTools.h"
#include "WorldStreaming.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
#include "EditorStabilization.h"
EditorAutomation::EditorAutomation(EditorApplication &application, EngineHost &engine)
    : app(application), host(engine), window(engine.GetWindow()), renderer(engine.GetRenderer()),
      initialWindowWidth(window.Width()), initialWindowHeight(window.Height()),
      autotest(std::getenv("JUDAS_EDITOR_AUTOTEST")),
      navigationAutotest(autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_PAN")),
      projectUnsavedAutotest(autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_PROJECT_UNSAVED")),
      polishAutotest(autotest && (std::getenv("JUDAS_EDITOR_AUTOTEST_POLISH") || projectUnsavedAutotest)),
      pickingAutotest(autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_PICKING")),
      workspaceAutotest(autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_WORKSPACE")),
      animationTimelineAutotest(autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_ANIMATION")),
      importRecipe(autotest ? std::getenv("JUDAS_EDITOR_AUTOTEST_IMPORT") : nullptr),
      terrainAutotest(autotest ? std::getenv("JUDAS_EDITOR_AUTOTEST_TERRAIN") : nullptr),
      volumeAutotest(autotest ? std::getenv("JUDAS_EDITOR_AUTOTEST_VOLUME") : nullptr),
      retargetAutotest(autotest ? std::getenv("JUDAS_EDITOR_AUTOTEST_RETARGET") : nullptr) {}

EditorAutomationNativePoint EditorAutomation::NativePoint(glm::vec2 point, const char *panelName) const {
    // The presenter supplies the current Scene native owner once it has drawn.
    auto result =
        EditorAutomationPointInWindow(point, window.NativeWindow(), app.m_panels.dockingEnabled, panelName);
    const auto &surface = app.m_shell.views.Scene();
    if (app.m_panels.dockingEnabled && panelName && std::string(panelName) == "Scene###Judas.Scene")
        if (auto *owner = SDL_GetWindowFromID(surface.windowId)) {
            result.window = owner;
            result.position = {int(point.x - surface.nativeOrigin.x), int(point.y - surface.nativeOrigin.y)};
        }
    return result;
}
SDL_Window *EditorAutomation::SceneWindow() const {
    return NativePoint(app.m_panels.workspace.viewport.position).window;
}
void EditorAutomation::WarpPoint(glm::vec2 point, const char *panelName) {
    const auto native = NativePoint(point, panelName);
    SDL_WarpMouseInWindow(native.window, native.position.x, native.position.y);
    // Pump the real native warp before queuing a synthetic button. PushEvent
    // alone does not update SDL mouse state used by the ImGui platform backend.
    SDL_PumpEvents();
}

void EditorAutomation::navigationCheck(bool ok, const char *label) {
    if (!ok)
        ++navigationFailures;
    std::fprintf(stderr, "[editor navigation] %s: %s\n", label, ok ? "PASS" : "FAIL");
}

void EditorAutomation::navigationButton(bool down) {
    if (down && !window.IsMouseCaptured())
        WarpPoint(app.m_panels.workspace.viewport.position + app.m_panels.workspace.viewport.size * .5f);
    const auto native =
        NativePoint(app.m_panels.workspace.viewport.position + app.m_panels.workspace.viewport.size * .5f);
    SDL_Event event{};
    event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    event.button.windowID = SDL_GetWindowID(native.window);
    event.button.button = SDL_BUTTON_MIDDLE;
    event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
    event.button.x = native.position.x;
    event.button.y = native.position.y;
    SDL_PushEvent(&event);
}

void EditorAutomation::navigationMotion(int dx, int dy) {
    if (!window.IsMouseCaptured())
        WarpPoint(app.m_panels.workspace.viewport.position + app.m_panels.workspace.viewport.size * .5f);
    const auto native =
        NativePoint(app.m_panels.workspace.viewport.position + app.m_panels.workspace.viewport.size * .5f);
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = SDL_GetWindowID(native.window);
    event.motion.x = native.position.x;
    event.motion.y = native.position.y;
    event.motion.xrel = dx;
    event.motion.yrel = dy;
    SDL_PushEvent(&event);
}

void EditorAutomation::navigationFocus(bool focused) {
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.windowID = SDL_GetWindowID(SceneWindow());
    event.window.event = focused ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST;
    SDL_PushEvent(&event);
}

void EditorAutomation::volumeCheck(bool ok, const char *label) {
    if (!ok)
        ++volumeFailures;
    std::fprintf(stderr, "[M84 editor volume] %s: %s\n", ok ? "PASS" : "FAIL", label);
}

void EditorAutomation::terrainCheck(bool value, const char *label) {
    if (!value)
        ++terrainFailures;
    std::fprintf(stderr, "[M75 terrain editor] %s: %s\n", value ? "PASS" : "FAIL", label);
}

void EditorAutomation::screenshot(const std::string &path) {
    std::vector<unsigned char> pixels;
    renderer.CaptureFrame(window.Width(), window.Height(), pixels);
    const bool written = WriteRgbPng(path, window.Width(), window.Height(), pixels);
    std::fprintf(stderr, "[editor autotest] %s: %s\n", written ? "wrote" : "FAILED", path.c_str());
}

bool EditorAutomation::Initialize() {
    if(navigationAutotest||pickingAutotest){
        app.m_panels.showSceneView=true;
        app.m_shell.views.RequestSceneFocus();
    }
    if (polishAutotest && std::getenv("JUDAS_EDITOR_AUTOTEST_PREFS"))
        navigationCheck(app.m_panels.assetBrowserHeight == 100.0f,
                        "saved asset height below 120 pixels survives preference loading");
    if (autotest) {
        if (const char *directory = std::getenv("JUDAS_EDITOR_AUTOTEST_NEW_PROJECT")) {
            std::string error;
            const char *twoD = std::getenv("JUDAS_EDITOR_AUTOTEST_NEW_2D");
            const int template2D = twoD ? (std::string(twoD) == "grid"        ? 3
                                           : std::string(twoD) == "side-view" ? 2
                                                                              : 1)
                                        : 0;
            const bool created = !fs::exists(fs::u8path(directory)) &&
                                 app.CreateProject(directory, template2D ? "SDK 2D Game" : "SDK First Game",
                                                   error, !template2D, template2D);
            std::fprintf(stderr, "[editor onboarding] packaged visible starter: %s %s\n",
                         created ? "PASS" : "FAIL", error.c_str());
            if (!created)
                return false;
        }
    }
    if (autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_LIQUID_BAKE")) {
        std::vector<SceneObjectId> ids;
        for (const auto &o : app.m_document.GetScene().Objects())
            if (o.liquidBasin)
                ids.push_back(o.id);
        for (auto id : ids) {
            bool ok = BakeEditorLiquid(app.m_document, id, app.m_panels);
            std::fprintf(stderr, "[editor autotest] liquid bake %llu: %s: %s\n",
                         static_cast<unsigned long long>(id), ok ? "PASS" : "FAIL",
                         app.m_panels.status.c_str());
        }
    }
    if (autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_NAV_BAKE")) {
        std::vector<SceneObjectId> surfaces;
        for (const auto &o : app.m_document.GetScene().Objects())
            if (o.navigationSurface)
                surfaces.push_back(o.id);
        for (auto id : surfaces) {
            bool ok = BakeEditorNavigation(app.m_document, id, app.m_panels);
            std::fprintf(stderr, "[editor autotest] navigation bake %llu: %s: %s\n",
                         static_cast<unsigned long long>(id), ok ? "PASS" : "FAIL",
                         app.m_panels.status.c_str());
        }
    }
    if (autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_DEFORMABLE")) {
        app.m_panels.deformableColumns = 9;
        app.m_panels.deformableRows = 13;
        app.m_panels.deformableSubdivision = 2;
        app.m_panels.deformableSize = {2, 3, 1};
        app.m_panels.deformableDestination = "deformables/editor-sheet.judasdeform";
        bool sheet = BakeEditorDeformable(app.m_document, 100, app.m_panels, 1);
        app.m_panels.deformableColumns = 4;
        app.m_panels.deformableRows = 2;
        app.m_panels.deformableSubdivision = 2;
        app.m_panels.deformableSize = {3, 1, 1};
        app.m_panels.deformableDestination = "deformables/editor-block.judasdeform";
        bool block = BakeEditorDeformable(app.m_document, 300, app.m_panels, 2);
        std::fprintf(stderr, "[editor autotest] deformable sheet/block authoring: %s / %s: %s\n",
                     sheet ? "PASS" : "FAIL", block ? "PASS" : "FAIL", app.m_panels.status.c_str());
        if (const char *source = std::getenv("JUDAS_EDITOR_AUTOTEST_DEFORMABLE_SOURCE")) {
            app.m_panels.deformableSource = source;
            app.m_panels.deformableDestination = "deformables/editor-import.judasdeform";
            bool imported = BakeEditorDeformable(app.m_document, 501, app.m_panels, 3);
            std::fprintf(stderr, "[editor autotest] deformable indexed import authoring: %s: %s\n",
                         imported ? "PASS" : "FAIL", app.m_panels.status.c_str());
        }
    }
    if (autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_FRACTURE")) {
        app.m_panels.deformableColumns = 3;
        app.m_panels.deformableRows = 1;
        app.m_panels.deformableSubdivision = 1;
        app.m_panels.deformableSize = {3, 1, 1};
        app.m_panels.deformableDestination = "deformables/editor-fracture-rigid.judasdeform";
        bool rigid = BakeEditorDeformable(app.m_document, 100, app.m_panels, 5);
        app.m_panels.deformableDestination = "deformables/editor-fracture-soft.judasdeform";
        bool soft = BakeEditorDeformable(app.m_document, 200, app.m_panels, 4);
        app.m_panels.deformableDestination = "deformables/editor-fracture-import.judasdeform";
        app.m_panels.deformableSource = "deformables/L-partition.source";
        bool imported = BakeEditorDeformable(app.m_document, 450, app.m_panels, 6);
        std::fprintf(stderr, "[editor autotest] fracture rigid/soft/partition authoring: %s / %s / %s: %s\n",
                     rigid ? "PASS" : "FAIL", soft ? "PASS" : "FAIL", imported ? "PASS" : "FAIL",
                     app.m_panels.status.c_str());
    }
    if (autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_BENDING")) {
        app.m_panels.deformableColumns = 4;
        app.m_panels.deformableRows = 1;
        app.m_panels.deformableSubdivision = 1;
        app.m_panels.deformableSize = {4, 1, 1};
        app.m_panels.deformableDestination = "deformables/editor-bendable.judasdeform";
        bool baked = BakeEditorDeformable(app.m_document, 300, app.m_panels, 7);
        const auto *owner = app.m_document.GetScene().Find(300);
        const auto assigned = owner && owner->deformable ? owner->deformable->asset : std::string();
        bool duplicate = BakeEditorDeformable(app.m_document, 300, app.m_panels, 7);
        const auto *retained = app.m_document.GetScene().Find(300);
        bool safe = !duplicate && retained && retained->deformable && retained->deformable->asset == assigned;
        std::fprintf(stderr,
                     "[M80 editor] bake bendable structural cook: %s; existing destination leaves assignment "
                     "unchanged: %s\n",
                     baked ? "PASS" : "FAIL", safe ? "PASS" : "FAIL");
        if (!baked || !safe)
            ++navigationFailures;
    }
    if (autotest && std::getenv("JUDAS_EDITOR_AUTOTEST_COLLISION")) {
        auto *object = app.m_document.GetScene().Find(100);
        auto source = object && object->render ? object->render->meshAsset : AssetId{};
        CollisionCookSettings settings;
        bool ok = BakeEditorCollision(app.m_document, 100, app.m_panels, source, settings,
                                      "collision/editor-check.judascollision");
        std::fprintf(stderr, "[editor autotest] shared collision cook/assign: %s: %s\n", ok ? "PASS" : "FAIL",
                     app.m_panels.status.c_str());
    }
    if (autotest)
        SaveSceneToString(app.m_document.GetScene(), autotestBaseline);
    if (importRecipe) {
        app.m_panels.showAssetBrowser = true;
        app.m_panels.modelRecipe = importRecipe;
        app.m_panels.importTask = QueueModelImport(host.Jobs(), importRecipe);
        app.m_panels.importPublished = false;
    }
    if (retargetAutotest) {
        std::string error;
        if (!BeginRetargetAutotest(app.m_panels, retargetAutotest, error)) {
            std::fprintf(stderr, "[M74 editor retarget] FAIL: %s\n", error.c_str());
            app.m_quit = true;
        }
    }
    if (const char *output = std::getenv("JUDAS_EDITOR_STABILIZATION")) {
        app.m_stabilization = std::make_unique<EditorApplication::StabilizationAutomation>();
        app.m_stabilization->output = output;
    }

    return true;
}

void EditorAutomation::BeforeRequests(EditorRequests &requests) {
    exportDestination = std::getenv("JUDAS_EDITOR_AUTOTEST_EXPORT");
    // Polish intentionally queues Escape at frame 14. Test export only
    // after that cancellation/release has completed, not in the same frame.
    exportRequestFrame = polishAutotest ? 17 : 14;
    automatedExport = autotest && autotestFrame == exportRequestFrame && exportDestination;
    if (automatedExport) {
        app.m_document.BeginEdit();
        app.m_document.GetScene().Settings().name += " [unsaved export]";
        app.m_document.CommitEdit();
        app.m_project.Settings().name += " [unsaved export]";
        app.m_panels.exportDestination = exportDestination;
        requests.exportProject = true;
    }
    if (importRecipe && autotestFrame == 1)
        requests.dropMeshAssetId = app.m_panels.importAccepted->assetId;
    if (polishAutotest && autotestFrame == 11)
        requests.newScene = true;
    if (projectUnsavedAutotest && autotestFrame == 11) {
        std::string error;
        app.m_document.Save(error);
        app.m_project.Settings().name += " - unsaved";
        requests.newScene = false;
        requests.quit = true;
    }
    if (autotest && exportDestination && app.m_projectWorkflow.documentActionPending) {
        if (auto *dialog = ImGui::FindWindowByName("Save before run or export"))
            ImGui::ActivateItemByID(dialog->GetID(std::getenv("JUDAS_EDITOR_AUTOTEST_EXPORT_SAVED")
                                                      ? "Use saved version"
                                                      : "Save all and continue"));
    }
}

void EditorAutomation::AfterRequests() {
    if (projectUnsavedAutotest && autotestFrame == 11)
        navigationCheck(app.m_project.IsDirty() && !app.m_document.IsDirty() &&
                            app.m_projectWorkflow.confirmationTitle == "Unsaved project" && !app.m_quit,
                        "project-only edits receive their own close guard");
    if (autotest && exportDestination && autotestFrame == exportRequestFrame + 5) {
        const bool success = app.m_panels.runProjectInfo.rfind("Exported ", 0) == 0;
        navigationCheck(success, "explicit export choice produces the requested package");
        if (success) {
            Project packaged;
            Scene scene;
            std::string error;
            const bool savedOnly = std::getenv("JUDAS_EDITOR_AUTOTEST_EXPORT_SAVED") != nullptr;
            const bool loaded =
                packaged.Load((fs::u8path(exportDestination) / "game.judasproj").string(), error) &&
                LoadSceneFromFile(packaged.StartupScenePath(), scene, error);
            navigationCheck(
                loaded &&
                    ((packaged.Settings().name.find("[unsaved export]") != std::string::npos) ==
                     !savedOnly) &&
                    ((scene.Settings().name.find("[unsaved export]") != std::string::npos) == !savedOnly),
                "exported scene and project match the explicit saved-content choice");
            navigationCheck(app.m_document.IsDirty() == savedOnly && app.m_project.IsDirty() == savedOnly,
                            "use-saved preserves live edits; save-all clears both dirty baselines");
        }
        // Authoring changed deliberately before export; Play/Stop must
        // preserve that current document, whichever disk version shipped.
        SaveSceneToString(app.m_document.GetScene(), autotestBaseline);
        std::fprintf(stderr, "[editor export choice] %s\n", app.m_panels.runProjectInfo.c_str());
    }
    if (polishAutotest) {
        if (autotestFrame == 11) {
            std::string actual;
            SaveSceneToString(app.m_document.GetScene(), actual);
            navigationCheck(app.m_projectWorkflow.documentActionPending &&
                                !app.m_projectWorkflow.documentActionApproved && actual == autotestBaseline,
                            "new-scene request preserves unsaved document until a choice");
        } else if (autotestFrame == 13) {
            navigationCheck(ImGui::IsPopupOpen(app.m_projectWorkflow.confirmationTitle.c_str()) &&
                                app.m_projectWorkflow.documentActionPending,
                            "unsaved-document confirmation remains open across frames");
        } else if (autotestFrame == 14) {
            navigationCheck(!app.m_projectWorkflow.documentActionPending &&
                                !app.m_projectWorkflow.documentActionApproved &&
                                !app.m_projectWorkflow.documentActionSaveAs,
                            "Escape cancels unsaved-document action");
        } else if (autotestFrame == 15) {
            const auto &viewport = app.m_panels.workspace.viewport;
            const auto size = renderer.RenderTargetSize(app.m_sceneViewport.target);
            navigationCheck(size == glm::ivec2(viewport.size),
                            "resized scene target follows workspace dimensions");
            glm::vec3 origin, direction;
            app.ViewportRay(static_cast<int>(viewport.position.x + viewport.size.x * .5f),
                            static_cast<int>(viewport.position.y + viewport.size.y * .5f), origin, direction);
            navigationCheck(glm::dot(direction, app.m_sceneViewport.camera.Forward()) > .99999f,
                            "scene picking uses the central viewport projection after resize");
        } else if (autotestFrame == 144) {
            navigationCheck(app.m_projectWorkflow.documentActionPending && !app.m_quit &&
                                !window.ShouldClose(),
                            "desktop close request waits for unsaved-document confirmation");
        } else if (autotestFrame == 145) {
            std::string actual;
            SaveSceneToString(app.m_document.GetScene(), actual);
            navigationCheck(!app.m_projectWorkflow.documentActionPending && !app.m_quit &&
                                actual == autotestBaseline,
                            "cancelled desktop close preserves authored state and editor session");
        }
    }
    if (automatedExport)
        std::fprintf(stderr, "[editor autotest] export project: %s\n", app.m_panels.runProjectInfo.c_str());
}

void EditorAutomation::AfterRender(EditorRequests &deferredRequests) {
    if (!autotest)
        return;
    RunTerrainAndAsyncChecks(deferredRequests);
    RunAssetsChecks();
    RunWorkspaceChecks();
    RunSimulationChecks(deferredRequests);
}
