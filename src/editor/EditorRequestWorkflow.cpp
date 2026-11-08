#include "AssetBrowserPanel.h"
#include "BrowserExportWorkflow.h"
#include "ComponentEditors.h"
#include "EditorAssetActions.h"
#include "EditorContext.h"
#include "EditorCreationMenu.h"
#include "EditorDocument.h"
#include "EditorMilestoneNotice.h"
#include "EditorPauseExample.h"
#include "EditorPlaySession.h"
#include "EditorPreferences.h"
#include "EditorPrimitiveAssets.h"
#include "EditorProjectWorkflow.h"
#include "EditorSceneGeometry.h"
#include "EditorSceneViewport.h"
#include "EditorScriptWorkflow.h"
#include "EditorWidgets.h"
#include "EngineHost.h"
#include "EnginePaths.h"
#include "InteractivePlay.h"
#include "ModAuthoring.h"
#include "ModelCook.h"
#include "PlatformServices.h"
#include "Prefab.h"
#include "Project.h"
#include "ProjectExporter.h"
#include "RetargetPanel.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SpriteSheetEditor.h"
#include "TerrainTools.h"
#include "UtilityAIEditor.h"
#include "VolumeTerrainTools.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <SDL2/SDL.h>
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
using EditorSceneGeometry::BuildSelectionLines;
using EditorSceneGeometry::PickRadius;

namespace fs = std::filesystem;
namespace {
constexpr const char *kProjectFileHint =
    "Relative to the working directory or absolute, e.g. projects/tiny_game/tiny_game.judasproj";
}
void EditorProjectWorkflow::HandleRequests(EditorContext &context, EditorProjectServices &services,
                                           EditorRequests &r) {
    if (r.resetWorkspace)
        services.preferences.ResetLayout();
    if (r.checkMilestones) {
        services.updates.Check(true, services.preferences.updateChannel);
        context.panels.status = "Checking this build channel for updates...";
    }
    std::string error;
    bool approvedDocumentAction = false;
    if (documentActionApproved) {
        r = pendingDocumentAction;
        pendingDocumentAction = {};
        documentActionApproved = documentActionPending = false;
        approvedDocumentAction = true;
    }
    if (r.quit && (context.document.IsDirty() || context.project.IsDirty()) &&
        context.panels.mode == EditorMode::Play)
        services.stopPlay();
    const bool terrainReplaces = r.quit || r.newScene || r.open || r.newProject || r.openProject || r.play ||
                                 r.runProject || r.exportProject || r.exportBrowser ||
                                 !r.openSceneRelative.empty();
    if (terrainReplaces && services.utilityAI && !services.utilityAI->Guard(context.panels))
        return;
    if (terrainReplaces && services.spriteSheets && services.spriteSheets->Dirty()) {
        context.panels.showSpriteSheetEditor = true;
        context.panels.status = "Save or discard the sprite-sheet draft before leaving, playing or exporting";
        return;
    }
    if (terrainReplaces && services.terrain && !services.terrain->Guard(context.panels))
        return;
    if (terrainReplaces && services.volumeTerrain && !services.volumeTerrain->Guard(context.panels))
        return;
    const bool replacesDocument =
        r.quit || r.newScene || r.open || r.newProject || r.openProject || !r.openSceneRelative.empty();
    const bool replacesProject = r.quit || r.newProject || r.openProject;
    const bool shipping = r.runProject || r.exportProject || r.exportBrowser;
    const bool sceneDirty = context.document.IsDirty() && (replacesDocument || shipping);
    const bool projectDirty = context.project.IsDirty() && (replacesProject || shipping);
    if ((sceneDirty || projectDirty) && !approvedDocumentAction) {
        pendingDocumentAction = r;
        actionNeedsScene = sceneDirty;
        actionNeedsProject = projectDirty;
        confirmationTitle = shipping     ? "Save before run or export"
                            : sceneDirty ? (projectDirty ? "Unsaved changes" : "Unsaved scene")
                                         : "Unsaved project";
        documentActionPending = true;
        r = {};
        services.viewport.CancelCameraNavigation(context);
    }
    auto savePending = [&]() {
        if (actionNeedsScene && context.document.IsDirty()) {
            if (context.document.Path().empty()) {
                documentActionSaveAs = true;
                return false;
            }
            if (!context.document.Save(error)) {
                context.panels.status = "Save failed: " + error;
                return false;
            }
            RefreshProjectLists(context);
        }
        if (actionNeedsProject && context.project.IsDirty() && !context.project.Save(error)) {
            context.panels.status = "Save failed: " + error;
            return false;
        }
        documentActionApproved = true;
        return true;
    };
    if (documentActionPending && !documentActionApproved && !documentActionSaveAs)
        ImGui::OpenPopup(confirmationTitle.c_str());
    if (documentActionSaveAs && !fileBrowser.Active() && !ImGui::IsPopupOpen("Save scene as"))
        r.saveAs = true;
    if (r.quit)
        services.quit = true;
    if (r.play && context.panels.mode == EditorMode::Edit) {
        if (!services.startPlay(error))
            context.panels.status = "Play failed: " + error;
    }
    if (r.stop && context.panels.mode == EditorMode::Play)
        services.stopPlay();
    if (context.panels.mode == EditorMode::Play && services.session.play) {
        std::string message;
        if (r.saveWorldState) {
            services.session.play->SaveWorldStateNow(message);
            context.panels.status = message;
        }
        if (r.deleteWorldState) {
            services.session.play->DeleteWorldStateNow(message);
            context.panels.status = message;
        }
    }
    if (r.openJudasJS || r.openQuickStart)
        r.openExternalPath =
            ResolveEngineDataPath(r.openQuickStart ? "docs/judasjs/getting-started.md" : "docs/JUDASJS.md");
    if (r.openWorkflowGuide)
        r.openExternalPath = ResolveEngineDataPath("docs/DEVELOPER_WORKFLOWS.md");
    if (!r.openExternalPath.empty()) {
        std::error_code ec;
        const auto path = fs::absolute(r.openExternalPath, ec);
        if (ec || !fs::is_regular_file(path, ec))
            context.panels.status = "File unavailable: " + r.openExternalPath;
        else {
            // SDL hands a local URI to the user's default application. Escape
            // path bytes rather than interpolating a filename into shell code.
            const auto generic = path.generic_string();
            std::string uri = generic.empty() || generic.front() != '/' ? "file:///" : "file://";
            constexpr char hex[] = "0123456789ABCDEF";
            for (const unsigned char c : generic) {
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' ||
                    c == ':' || c == '-' || c == '_' || c == '.' || c == '~')
                    uri += static_cast<char>(c);
                else {
                    uri += '%';
                    uri += hex[c >> 4];
                    uri += hex[c & 15];
                }
            }
            services.viewport.CancelCameraNavigation(context);
            context.panels.status =
                SDL_OpenURL(uri.c_str()) == 0
                    ? "Requested open: " + path.string()
                    : "Could not open file; copy its path instead: " + std::string(SDL_GetError());
        }
    }
    if (context.panels.mode != EditorMode::Edit)
        return;

    // --- Project ---
    if (r.browseImport) {
        fileBrowserParent.clear();
        fileBrowser.Open(EditorFileBrowser::Target::Import, EditorFileBrowser::Mode::OpenFile,
                         context.panels.importSourceInput);
    }
    if (r.browseExport) {
        fileBrowserParent.clear();
        fileBrowser.Open(EditorFileBrowser::Target::Export, EditorFileBrowser::Mode::Directory,
                         context.panels.exportDestination);
    }
    if (r.newProject)
        ImGui::OpenPopup("New project");
    if (r.openProject)
        ImGui::OpenPopup("Open project");
    if (r.saveProject)
        context.panels.status = context.project.Save(error) ? "Saved project " + context.project.ProjectFile()
                                                            : "Project save failed: " + error;
    if (r.exportBrowser && context.panels.experimentalBrowserSupport && context.panels.browserExport) {
        context.panels.browserExport->Start(context.project, false);
    }
    if (r.browseBrowserExport && context.panels.experimentalBrowserSupport && context.panels.browserExport) {
        fileBrowserParent.clear();
        fileBrowser.Open(EditorFileBrowser::Target::BrowserExport, EditorFileBrowser::Mode::Directory,
                         context.panels.browserExport->destination);
    }
    if (r.exportProject) {
        ProjectExportOptions options;
        options.destination = context.panels.exportDestination;
        ProjectExportResult result;
        Project saved;
        const bool exported =
            saved.Load(context.project.ProjectFile(), error) && ExportProject(saved, options, result, error);
        context.panels.runProjectInfo = exported ? "Exported " + result.packageDirectory + " (" +
                                                       std::to_string(result.assetCount) + " assets)"
                                                 : error;
        context.panels.status = context.panels.runProjectInfo;
    }
    if (r.runProject) {
        std::string message;
        RunProject(context, message);
        context.panels.status = message;
        context.panels.runProjectInfo = message;
    }
    if (!r.openSceneRelative.empty()) {
        context.panels.status = OpenScene(context, services, r.openSceneRelative, error)
                                    ? "Opened " + r.openSceneRelative
                                    : "Open failed: " + error;
    }

    // --- Assets ---
    bool assetsChanged = false;
    if (!r.importSource.empty()) {
        AssetRecord record;
        if (context.host.Assets().Import(r.importSource, r.importDestination, record, error)) {
            context.panels.status =
                "Imported " + record.relativePath + " as " + AssetTypeName(record.type) + " " + record.id;
            context.panels.browserSelection = record.id;
            context.panels.importSourceInput.clear();
            context.panels.importDestinationInput.clear();
        } else {
            context.panels.status = "Import failed: " + error;
        }
        assetsChanged = true;
    }
    if (!r.trackAssetPath.empty()) {
        AssetRecord record;
        context.panels.status = context.host.Assets().Track(r.trackAssetPath, record, error)
                                    ? "Tracked " + record.relativePath + " as " + record.id
                                    : "Track failed: " + error;
        assetsChanged = true;
    }
    if (!r.moveAssetId.empty()) {
        // The GPU copy keyed by this id stays valid: the id did not change.
        context.panels.status = context.host.Assets().Move(r.moveAssetId, r.moveAssetTo, error)
                                    ? "Moved asset to " + r.moveAssetTo
                                    : "Move failed: " + error;
        assetsChanged = true;
    }
    if (!r.removeAssetId.empty()) {
        context.host.Resources().Invalidate(r.removeAssetId);
        context.panels.status = context.host.Assets().Remove(r.removeAssetId, error)
                                    ? "Removed asset"
                                    : "Remove failed: " + error;
        context.panels.browserSelection.clear();
        assetsChanged = true;
    }
    if (r.rescanAssets || assetsChanged) {
        if (r.rescanAssets) {
            ResetEditorAssetThumbnails(context.panels);
            context.host.OpenProjectAssets(context.project.RootDir(), context.project.AssetsDir());
            std::string assetError;
            if (!JudasMods::OpenAuthorAssets(context.project, context.host.Assets(), assetError))
                context.panels.status = assetError;
        }
        RefreshProjectLists(context);
    }

    // --- Scene ---
    if (r.newScene) {
        context.document.NewScene();
        ResetEditorHierarchyView(context.panels);
        context.panels.status = "New scene";
    }
    if (r.open)
        ImGui::OpenPopup("Open scene");
    if (r.saveAs)
        ImGui::OpenPopup("Save scene as");
    if (r.save) {
        if (context.document.Path().empty())
            ImGui::OpenPopup("Save scene as");
        else {
            context.panels.status =
                context.document.Save(error) ? "Saved " + context.document.Path() : "Save failed: " + error;
            RefreshProjectLists(context);
        }
    }
    if (r.undo) {
        if (services.volumeTerrain && services.volumeTerrain->loaded && services.volumeTerrain->editing)
            services.volumeTerrain->draft.Undo();
        else if (services.terrain && services.terrain->loaded && services.terrain->editing)
            services.terrain->draft.Undo();
        else
            context.document.Undo();
    }
    if (r.redo) {
        if (services.volumeTerrain && services.volumeTerrain->loaded && services.volumeTerrain->editing)
            services.volumeTerrain->draft.Redo();
        else if (services.terrain && services.terrain->loaded && services.terrain->editing)
            services.terrain->draft.Redo();
        else
            context.document.Redo();
    }
    if (!r.createKind.empty()) {
        SceneObjectId createdId = kInvalidSceneObjectId;
        std::string defaultMesh;
        for (const auto &[id, record] : context.host.Assets().Records()) {
            if (record.type == AssetType::Mesh && !record.missing) {
                defaultMesh = id;
                break;
            }
        }
        if (r.createKind == "planar-player" || r.createKind == "planar-topdown-player" ||
            r.createKind == "planar-grid-player") {
            std::string error;
            createdId = CreateEditorPlanarPlayer(context.document, context.project, context.host.Assets(),
                                                r.createPosition, r.createParent, error,
                                                r.createKind == "planar-player",
                                                r.createKind == "planar-grid-player");
            context.panels.status = error.empty() ? "Created 2D player. Set scene gravity for platformers; "
                                                    "edit its project script in Assets."
                                                  : error;
        } else if (r.createKind == "first-person-player" || r.createKind == "third-person-player") {
            std::string error;
            const auto id = CreateEditorFirstPersonPlayer(
                context.document, context.project, context.host.Assets(), r.createPosition, r.createParent,
                error, r.createKind == "third-person-player");
            createdId = id;
            context.panels.status =
                id ? "Player created. Save scene, then Play: WASD / mouse / Space; V switches camera. Escape "
                     "releases mouse. Unedited starter camera disabled."
                   : error;
            if (id)
                context.panels.revealComponent = "Character motor";
        } else if (r.createKind == "pause-menu-example") {
            std::string error;
            const auto id = CreateEditorPauseExample(context.document, context.project, context.host.Assets(),
                                                     error, r.createParent);
            createdId = id;
            context.panels.status = id ? "Optional pause menu created. Save scene; Escape opens Pause / "
                                         "Settings / Controls during Play."
                                       : error;
            if (id)
                context.panels.revealComponent = "Scripts";
        } else if (r.createKind == "planar-circle" || r.createKind == "planar-capsule") {
            std::string error;
            const auto id =
                CreateEditorPlanarPrimitive(context.document, context.project, context.host.Assets(),
                                            r.createKind, r.createPosition, r.createParent, error);
            createdId = id;
            context.panels.status =
                id ? "Created native 2D collision with a matching editable sprite image" : error;
        } else if (r.createKind == "capsule" || r.createKind == "cylinder") {
            std::string error;
            const auto id = CreateEditorPrimitive(context.document, context.project, context.host.Assets(),
                                                  r.createKind, r.createPosition, r.createParent, error);
            createdId = id;
            context.panels.status =
                id ? "Created " + r.createKind +
                         " with editable mesh and convex collision. Body > Motion selects static/dynamic."
                   : error;
        } else {
            const auto created = CreateObjectOfKind(context.document, r.createKind, r.createPosition,
                                                    r.createKind == "sprite" ? std::string{} : defaultMesh,
                                                    nullptr, r.createParent);
            createdId = created;
            context.panels.status =
                created ? "Created " + r.createKind : "Creation failed: parent no longer exists";
        }
        // Report the published object, not the previous selection or merely the
        // requested kind. Failure diagnostics above remain untouched.
        if (const auto *created = context.document.GetScene().Find(createdId)) {
            const auto *parent = context.document.GetScene().Find(created->parent);
            const std::string placement = parent ? " as a child of '" + parent->name + "'. "
                                                  : " at the scene root. ";
            context.panels.status = "Created '" + created->name + "'" + placement + context.panels.status;
            std::fprintf(stderr,
                         "[editor creation] requested_kind=%s requested_parent=%llu result_id=%llu "
                         "actual_parent=%llu\n",
                         r.createKind.c_str(), static_cast<unsigned long long>(r.createParent),
                         static_cast<unsigned long long>(createdId),
                         static_cast<unsigned long long>(created->parent));
        }
    }
    if (!r.dropMeshAssetId.empty()) {
        std::optional<SceneAnimationComponent> animation;
        if (auto rig = context.host.Resources().TryGetSkeletal(r.dropMeshAssetId)) {
            animation.emplace();
            animation->clip = rig->clips.empty() ? "" : rig->clips.front().name;
        }
        CreateObjectOfKind(context.document, "mesh", r.createPosition, r.dropMeshAssetId,
                           animation ? &*animation : nullptr);
        context.panels.status = "Placed mesh asset from the Asset Browser";
    }
    if (r.duplicateId != kInvalidSceneObjectId) {
        if (DuplicateObject(context.document, r.duplicateId) != kInvalidSceneObjectId)
            context.panels.status = "Duplicated object";
    }
    if (r.focusSelection) {
        Scene flattened;
        if (FlattenHierarchy(context.document.GetScene(), flattened, error))
            if (const SceneObject *o = flattened.Find(context.document.Selected())) {
                services.viewport.camera.LookAt(o->transform.position, std::max(4.0f, PickRadius(*o) * 3.0f));
            }
    }

    // Path popups (the editor has no OS file dialog; a path field is enough
    // — see docs/ARCHITECTURE.md, "Milestone 30, Limitations").
    const auto pathPopup = [&](const char *title, const char *button, const char *label, const char *hint,
                               auto &&action) {
        ImGui::SetNextWindowSizeConstraints({440, 0},
                                            {std::max(440.0f, ImGui::GetIO().DisplaySize.x - 48), 600});
        if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;
        char buffer[512];
        std::strncpy(buffer, context.panels.pathInput.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        ImGui::TextUnformatted(label);
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1);
        const bool enter =
            ImGui::InputText("##path", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue);
        context.panels.pathInput = buffer;
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", hint);
        ImGui::PopTextWrapPos();
        if ((ImGui::Button(button) || enter) && !context.panels.pathInput.empty()) {
            if (action(context.panels.pathInput))
                ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            fileBrowserParent = title;
            const bool saving = std::strcmp(title, "Save scene as") == 0;
            const bool project = std::strcmp(title, "Open project") == 0;
            auto start = context.panels.pathInput;
            if (start.empty())
                start = context.project.IsLoaded() ? context.project.RootDir() : ".";
            else if (context.project.IsLoaded() && !fs::u8path(start).is_absolute())
                start = context.project.Resolve(start);
            fileBrowser.Open(EditorFileBrowser::Target::Document,
                             saving ? EditorFileBrowser::Mode::SaveFile : EditorFileBrowser::Mode::OpenFile,
                             start, project ? ".judasproj" : ".judas");
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if (documentActionSaveAs && std::strcmp(title, "Save scene as") == 0) {
                documentActionSaveAs = documentActionPending = false;
                pendingDocumentAction = {};
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", context.panels.status.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    };
    pathPopup("Open scene", "Open", "Path (.judas)", "Relative to the project root, e.g. Scenes/main.judas",
              [&](const std::string &path) {
                  std::string e;
                  const bool opened = OpenScene(context, services, path, e);
                  context.panels.status = opened ? "Opened " + path : "Open failed: " + e;
                  return opened;
              });
    pathPopup("Save scene as", "Save", "Path (.judas)",
              "Relative to the project root, e.g. Scenes/level2.judas", [&](const std::string &path) {
                  std::string e;
                  const std::string resolved = context.project.IsLoaded() && !fs::path(path).is_absolute()
                                                   ? context.project.Resolve(path)
                                                   : path;
                  const bool saved = context.document.SaveAs(resolved, e);
                  context.panels.status = saved ? "Saved " + resolved : "Save failed: " + e;
                  RefreshProjectLists(context);
                  if (saved && documentActionSaveAs) {
                      documentActionSaveAs = false;
                      savePending();
                  }
                  return saved;
              });
    pathPopup("Open project", "Open", "Project file (.judasproj)", kProjectFileHint,
              [&](const std::string &path) {
                  std::string e;
                  if (!OpenProject(context, services, path, e)) {
                      context.panels.status = "Open project failed: " + e;
                      return false;
                  }
                  context.panels.status = "Opened project " + context.project.Settings().name;
                  if (!context.project.Settings().startupScene.empty()) {
                      if (!OpenScene(context, services, context.project.Settings().startupScene, e))
                          context.panels.status = "Project opened; startup scene failed: " + e;
                  } else {
                      context.document.NewScene();
                  }
                  return true;
              });
    if (ImGui::BeginPopupModal("New project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        char directory[4096], name[128];
        std::snprintf(directory, sizeof(directory), "%s", newProjectDirectory.c_str());
        std::snprintf(name, sizeof(name), "%s", newProjectName.c_str());
        if (ImGui::InputTextWithHint("Directory", "Choose a new project folder", directory,
                                     sizeof(directory)))
            newProjectDirectory = directory;
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            fileBrowserParent = "New project";
            fileBrowser.Open(EditorFileBrowser::Target::NewProject, EditorFileBrowser::Mode::Directory,
                             newProjectDirectory);
        }
        if (ImGui::InputTextWithHint("Name", "My Game", name, sizeof(name)))
            newProjectName = name;
        ImGui::Combo("Template", &newProjectTemplate,
                     "3D script starter\0Empty 3D scene\0New 2D game - top-down\0New 2D game - "
                     "side-view\0New 2D game - grid RPG\0");
        ImGui::TextWrapped(
            newProjectTemplate == 4 ? "Cardinal cell-by-cell movement, native collision queries and an "
                                      "editable kinematic controller for tile RPGs."
            : newProjectTemplate >= 2
                ? "Native 2D collision, an orthographic camera and editable project controller. Side-view "
                  "adds gravity and jumping; top-down uses planar movement."
            : newProjectTemplate == 0
                ? "A rotating object, an ordinary JavaScript camera and an editable script. No hidden "
                  "gameplay controller."
                : "A floor, light and gravity. Add your own scripts, player and camera.");
        if (newProjectTemplate >= 2)
            ImGui::TextWrapped("Next: press Play to try movement, then Stop to edit. Select the player in "
                               "Hierarchy and open its attached Assets/game.js script. 2D collision size is "
                               "edited in Body, independently of the sprite.");
        const bool missingProjectFields = newProjectDirectory.empty() || newProjectName.empty();
        if (missingProjectFields)
            ImGui::TextDisabled("Choose a project directory and enter a name to continue.");
        ImGui::BeginDisabled(missingProjectFields);
        if (ImGui::Button("Create")) {
            std::string e;
            if (CreateProject(context, services, newProjectDirectory, newProjectName, e,
                              newProjectTemplate == 0,
                              newProjectTemplate >= 2 ? newProjectTemplate - 1 : 0)) {
                context.panels.status = "Created project " + context.project.ProjectFile();
                ImGui::CloseCurrentPopup();
            } else
                context.panels.status = "New project failed: " + e;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::TextWrapped("%s", context.panels.status.c_str());
        ImGui::EndPopup();
    }
    ImGui::SetNextWindowSize({460, 0}, ImGuiCond_Appearing);
    if (documentActionPending && !confirmationTitle.empty() &&
        ImGui::BeginPopupModal(confirmationTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const bool shipping = pendingDocumentAction.runProject || pendingDocumentAction.exportProject ||
                              pendingDocumentAction.exportBrowser;
        ImGui::TextUnformatted(shipping ? "Run and export use saved project and scene files."
                                        : "Save your changes before continuing?");
        if (actionNeedsScene)
            ImGui::BulletText("Scene: %s",
                              context.document.Path().empty() ? "Untitled" : context.document.Path().c_str());
        if (actionNeedsProject)
            ImGui::BulletText("Project settings: %s", context.project.Settings().name.c_str());
        if (ImGui::Button(shipping ? "Save all and continue" : "Save and continue")) {
            if (savePending() || documentActionSaveAs)
                ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(shipping ? "Use saved version" : "Discard")) {
            if (shipping || !actionNeedsProject || context.project.DiscardSettings(error)) {
                documentActionApproved = true;
                ImGui::CloseCurrentPopup();
            } else
                context.panels.status = "Discard failed: " + error;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            documentActionPending = documentActionApproved = documentActionSaveAs = false;
            pendingDocumentAction = {};
            ImGui::CloseCurrentPopup();
        }
        if (context.panels.status.rfind("Save failed:", 0) == 0)
            ImGui::TextWrapped("%s", context.panels.status.c_str());
        ImGui::EndPopup();
    }
    if (auto selected = fileBrowser.Draw()) {
        if (!selected->path.empty())
            switch (selected->target) {
            case EditorFileBrowser::Target::Document:
                context.panels.pathInput = selected->path;
                break;
            case EditorFileBrowser::Target::NewProject:
                newProjectDirectory = selected->path;
                break;
            case EditorFileBrowser::Target::Import:
                context.panels.importSourceInput = selected->path;
                context.panels.reviewAssetImport = true;
                break;
            case EditorFileBrowser::Target::Export:
                context.panels.exportDestination = selected->path;
                break;
            case EditorFileBrowser::Target::BrowserExport:
                if (context.panels.browserExport)
                    context.panels.browserExport->destination =
                        (fs::path(selected->path) /
                         (fs::path(context.project.RootDir()).filename().string() + "-web"))
                            .string();
                break;
            }
        if (!fileBrowserParent.empty()) {
            ImGui::OpenPopup(fileBrowserParent.c_str());
            fileBrowserParent.clear();
        }
    }
}
