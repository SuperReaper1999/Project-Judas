#include "EditorOverlayMenu.h"
#include "EditorVisuals.h"
#include "EditorAssetActions.h"
#include "ComponentEditors.h"
#include "EditorPanelWidgets.h"
#include "EditorPanels.h"
#include "EditorTheme.h"
#include "EditorWidgets.h"
#include "EditorWorkspace.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <set>
#include <sstream>

using namespace EditorPanelWidgets;
#include "EditorCreationMenu.h"
#include "SceneSession.h"
#include "WorldStreaming.h"

namespace {
} // namespace

void DrawEditorWorkspaceChrome(EditorDocument &doc, EditorPanelState &state, EditorRequests &requests,
                               const std::function<void()> &workspaceSelector,
                               const std::function<void()> &workflowSelector) {
    const bool editing = state.mode == EditorMode::Edit;
    const auto display = ImGui::GetIO().DisplaySize;
    const float scale = ImGui::GetIO().FontGlobalScale;
    const float logicalWidth = display.x / std::max(.85f, scale);
    PlaceWorkspacePanel(state.workspace.toolbar);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {14 * scale, 8 * scale});
    ImGui::Begin("##workspaceToolbar", nullptr,
                 kWorkspacePanelFlags | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse);
    auto *draw = ImGui::GetWindowDrawList();
    const auto position = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
    const auto upper = ImGui::GetColorU32(EditorVisuals::SurfaceColor(.035f));
    const auto lower = ImGui::GetColorU32(EditorVisuals::SurfaceColor(-.015f));
    draw->AddRectFilledMultiColor(position, {position.x + size.x, position.y + size.y}, upper, upper, lower,
                                  lower);
    if (workflowSelector) {
        workflowSelector();
        if (workspaceSelector) {
            const float right = ImGui::GetWindowContentRegionMax().x - 180 * scale;
            if (right > ImGui::GetCursorPosX() + 12 * scale) {
                ImGui::SameLine(right);
                workspaceSelector();
            } else {
                ImGui::SameLine();
                if (EditorDesign::ActionButton("Workspace...", EditorDesign::Tone::Quiet))
                    ImGui::OpenPopup("workflowLayouts");
                if (ImGui::BeginPopup("workflowLayouts")) {
                    workspaceSelector();
                    ImGui::EndPopup();
                }
            }
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }
    const float actionHeight = 30 * scale;
    ImGui::BeginDisabled(!editing);
    if (EditorVisuals::ActionButton("Play", EditorVisuals::Icon::Play, true, {96 * scale, actionHeight}))
        requests.play = true;
    ToolbarHint("F5: play the scene using your project's scripts");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(editing);
    if (EditorVisuals::ActionButton("Stop", EditorVisuals::Icon::Stop, false, {90 * scale, actionHeight}))
        requests.stop = true;
    ToolbarHint("F5: stop and restore your authored scene");
    ImGui::EndDisabled();
    ImGui::SameLine(0, 22 * scale);
    ImGui::BeginDisabled(!editing);
    if (logicalWidth >= 1200) {
        const auto tool = [&](const char *label, GizmoMode mode, EditorVisuals::Icon icon, const char *hint) {
            if (EditorVisuals::ActionButton(label, icon, state.gizmoMode == mode, {0, actionHeight}))
                state.gizmoMode = mode;
            ToolbarHint(hint);
            ImGui::SameLine();
        };
        tool("Move", GizmoMode::Translate, EditorVisuals::Icon::Move, "W: move selected objects");
        tool("Rotate", GizmoMode::Rotate, EditorVisuals::Icon::Rotate, "E: rotate selected objects");
        tool("Scale", GizmoMode::Scale, EditorVisuals::Icon::Scale, "R: resize selected objects");
        {
        EditorDesign::Field transformSpaceField;
        ImGui::SetNextItemWidth(86 * scale);
        if (ImGui::BeginCombo("##transformSpace",
                              state.gizmoSpace == GizmoSpace::World ? "World" : "Local")) {
            if (ImGui::Selectable("World", state.gizmoSpace == GizmoSpace::World))
                state.gizmoSpace = GizmoSpace::World;
            if (ImGui::Selectable("Local", state.gizmoSpace == GizmoSpace::Local))
                state.gizmoSpace = GizmoSpace::Local;
            ImGui::EndCombo();
        }
        }
        ToolbarHint("X: switch world/local transform axes");
        ImGui::SameLine();
        EditorDesign::Toggle("Snap", &state.gizmoSnap);
        ToolbarHint("Hold Ctrl temporarily or enable snapping here");
    } else {
        if (EditorDesign::ActionButton("Transform...", EditorDesign::Tone::Secondary, {0, actionHeight}))
            ImGui::OpenPopup("toolbarTransform");
        if (ImGui::BeginPopup("toolbarTransform")) {
            if (ImGui::MenuItem("Move", "W", state.gizmoMode == GizmoMode::Translate))
                state.gizmoMode = GizmoMode::Translate;
            if (ImGui::MenuItem("Rotate", "E", state.gizmoMode == GizmoMode::Rotate))
                state.gizmoMode = GizmoMode::Rotate;
            if (ImGui::MenuItem("Scale", "R", state.gizmoMode == GizmoMode::Scale))
                state.gizmoMode = GizmoMode::Scale;
            ImGui::Separator();
            if (ImGui::MenuItem("Local axes", "X", state.gizmoSpace == GizmoSpace::Local))
                state.gizmoSpace =
                    state.gizmoSpace == GizmoSpace::Local ? GizmoSpace::World : GizmoSpace::Local;
            ImGui::MenuItem("Snap", "hold Ctrl", &state.gizmoSnap);
            ImGui::EndPopup();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0, 22 * scale);
    if (EditorDesign::ActionButton("View...", EditorDesign::Tone::Quiet, {0, actionHeight}))
        ImGui::OpenPopup("toolbarView");
    if (ImGui::BeginPopup("toolbarView")) {
        ImGui::MenuItem("2D scene view", nullptr, &state.view2D, editing);
        ImGui::MenuItem("Scene settings", nullptr, &state.showSceneSettings, editing);
        ImGui::MenuItem("Asset Browser", nullptr, &state.showAssetBrowser);
        ImGui::MenuItem("Editor preferences / layouts", nullptr, &state.showEditorPreferences);
        ImGui::MenuItem("Navigation and shortcuts", nullptr, &state.showNavigationHelp);
        ImGui::EndPopup();
    }
    // File/Edit/Create menus retain all project commands and shortcuts. Keep
    // only the small, everyday document actions beside the scene tool group.
    if (logicalWidth >= 850) {
        ImGui::SameLine(0, 18 * scale);
        ImGui::BeginDisabled(!editing);
        if (EditorVisuals::ActionButton("Save", EditorVisuals::Icon::File, false, {0, actionHeight}))
            requests.save = true;
        ToolbarHint("Ctrl+S: save scene. File contains project and export commands.");
        ImGui::SameLine();
        ImGui::BeginDisabled(!doc.CanUndo());
        if (EditorDesign::ActionButton("Undo", EditorDesign::Tone::Quiet, {0, actionHeight}))
            requests.undo = true;
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!doc.CanRedo());
        if (EditorDesign::ActionButton("Redo", EditorDesign::Tone::Quiet, {0, actionHeight}))
            requests.redo = true;
        ImGui::EndDisabled();
        ImGui::EndDisabled();
    }
    if (!workflowSelector && workspaceSelector) {
        ImGui::SameLine();
        if (EditorDesign::ActionButton("Workspace...", EditorDesign::Tone::Quiet))
            ImGui::OpenPopup("compactWorkspaceSelector");
        if (ImGui::BeginPopup("compactWorkspaceSelector")) {
            workspaceSelector();
            ImGui::EndPopup();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();


}

void DrawEditorNavigationHelp(EditorPanelState &state, EditorRequests &requests) {
    if (!state.showNavigationHelp)
        return;

    ImGui::SetNextWindowSize({440, 340}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Editor help", &state.showNavigationHelp)) {
        ImGui::TextUnformatted("SCENE NAVIGATION");
        ImGui::Separator();
        ImGui::TextWrapped("Wheel: zoom in / out. Middle drag: pan. Right drag: look; hold WASD / Q "
                           "/ E to fly. Shift flies faster. F frames selection.");
        ImGui::Spacing();
        ImGui::TextWrapped("Start a camera drag inside the scene view. Release, Escape or leaving "
                           "the app ends capture.");
        ImGui::Spacing();
        ImGui::TextUnformatted("AUTHORING");
        ImGui::Separator();
        ImGui::TextWrapped("Right-click hierarchy space to create an object, or a row to create a "
                           "child. Player presets include an editable JavaScript controller and "
                           "camera; V switches first/third person.");
        ImGui::TextWrapped("Select an object, then use Create script & attach or Attach existing script in "
                           "the "
                           "Inspector. Edit script opens its source. Save source and scene before Play.");
        ImGui::TextWrapped("W / E / R: move / rotate / scale. X: local / world axes. Ctrl: snap. "
                           "Left click selects; Ctrl adds; Shift selects a range in the hierarchy.");
        ImGui::Spacing();
        ImGui::TextWrapped("Ctrl+S: save. Ctrl+Z / Ctrl+Y: undo / redo. Ctrl+D: duplicate. F5: Play "
                           "/ Stop. Drag the dividers to resize this workspace.");
        ImGui::Separator();
        if (ImGui::Button("JudasJS reference"))
            requests.openJudasJS = true;
        ImGui::SameLine();
        if (ImGui::Button("First project guide"))
            requests.openQuickStart = true;
        if (ImGui::Button("Ownership, readiness and saves"))
            requests.openWorkflowGuide = true;
    }
    ImGui::End();
}

void DrawEditorMainMenu(EditorDocument &doc, EditorPanelState &state, EditorRequests &requests,
                        const std::function<void()> &windowMenu) {
    if (!doc.ValidationError().empty())
        state.status = "Edit rejected; previous document restored: " + doc.ValidationError();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10, 7});
    if (!ImGui::BeginMainMenuBar()) {
        ImGui::PopStyleVar();
        return;
    }
    const auto brandAt = ImGui::GetCursorScreenPos();
    const float brandSize = ImGui::GetFontSize() * 1.15f;
    EditorVisuals::BrandMark(ImGui::GetWindowDrawList(), {brandAt.x, brandAt.y + 1}, brandSize);
    ImGui::Dummy({brandSize + 5, brandSize});
    ImGui::SameLine();
    ImGui::PushFont(EditorVisuals::HeadingFont());
    ImGui::TextUnformatted("Judas Editor V2");
    ImGui::PopFont();
    ImGui::SameLine(0, 20 * ImGui::GetIO().FontGlobalScale);
    const bool editing = state.mode == EditorMode::Edit;
    const bool hasProject = state.project && state.project->IsLoaded();
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New project...", nullptr, false, editing))
            requests.newProject = true;
        if (ImGui::MenuItem("Open project...", nullptr, false, editing))
            requests.openProject = true;
        if (ImGui::MenuItem("Project settings", nullptr, state.showProjectSettings, hasProject)) {
            state.showProjectSettings = !state.showProjectSettings;
        }
        if (ImGui::MenuItem("Import assets...", nullptr, false, editing && hasProject))
            requests.browseImport = true;
        if (ImGui::MenuItem("Build & Export...", nullptr, false, editing && hasProject)) {
            state.showBuildExport = true;
            ImGui::SetWindowFocus("Build & Export");
        }
        if (state.experimentalBrowserSupport &&
            ImGui::MenuItem("Export browser game (Experimental)...", nullptr, false, editing && hasProject)) {
            state.showBrowserExport = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("New scene", nullptr, false, editing))
            requests.newScene = true;
        if (ImGui::MenuItem("Open scene...", nullptr, false, editing))
            requests.open = true;
        if (ImGui::MenuItem("Save scene", "Ctrl+S", false, editing))
            requests.save = true;
        if (ImGui::MenuItem("Save scene as...", nullptr, false, editing))
            requests.saveAs = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Quit"))
            requests.quit = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, editing && doc.CanUndo()))
            requests.undo = true;
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, editing && doc.CanRedo()))
            requests.redo = true;
        ImGui::Separator();
        const bool selected = doc.Selected() != kInvalidSceneObjectId;
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, editing && selected))
            requests.duplicateId = doc.Selected();
        if (ImGui::MenuItem("Rename", "F2", false, editing && doc.Selection().size() == 1))
            BeginEditorRename(doc, state);
        if (ImGui::MenuItem("Focus selection", "F", false, selected))
            requests.focusSelection = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Translate gizmo", "W", state.gizmoMode == GizmoMode::Translate))
            state.gizmoMode = GizmoMode::Translate;
        if (ImGui::MenuItem("Rotate gizmo", "E", state.gizmoMode == GizmoMode::Rotate))
            state.gizmoMode = GizmoMode::Rotate;
        if (ImGui::MenuItem("Scale gizmo", "R", state.gizmoMode == GizmoMode::Scale))
            state.gizmoMode = GizmoMode::Scale;
        if (ImGui::MenuItem("Local space", "X", state.gizmoSpace == GizmoSpace::Local)) {
            state.gizmoSpace = state.gizmoSpace == GizmoSpace::Local ? GizmoSpace::World : GizmoSpace::Local;
        }
        ImGui::MenuItem("Snap (hold Ctrl)", nullptr, &state.gizmoSnap);
        ImGui::Separator();
        ImGui::MenuItem("Editor preferences", nullptr, &state.showEditorPreferences);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Create", editing)) {
        DrawEditorCreationItems(state, requests);
        ImGui::EndMenu();
    }
    if (windowMenu && ImGui::BeginMenu("Window")) {
        windowMenu();
        ImGui::Separator();
        ImGui::MenuItem("2D scene view", nullptr, &state.view2D, editing);
        ImGui::MenuItem("Editor preferences / layouts", nullptr, &state.showEditorPreferences);
        ImGui::EndMenu();
    }
    if (!windowMenu && ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Editor preferences (appearance / layouts)", nullptr, &state.showEditorPreferences);
        ImGui::Separator();
        ImGui::MenuItem("Asset Browser", nullptr, &state.showAssetBrowser);
        ImGui::MenuItem("Scene settings", nullptr, &state.showSceneSettings, editing);
        ImGui::MenuItem("JudasJS developer tools", nullptr, &state.showScriptTools);
        ImGui::MenuItem("AI actions and decisions", nullptr, &state.showUtilityAI);
        if (ImGui::BeginMenu("Terrain and world tools")) {
            ImGui::MenuItem("Terrain sculpt / paint", nullptr, &state.showTerrain);
            ImGui::MenuItem("World building / named source", nullptr, &state.showWorldBuilding);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("2D authoring")) {
            ImGui::MenuItem("Tilemap painter", nullptr, &state.showTilemapEditor);
            ImGui::MenuItem("Sprite sheets and frame animation", nullptr, &state.showSpriteSheetEditor);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Animation tools")) {
            ImGui::MenuItem("Animation timeline", nullptr, &state.showAnimationTimeline);
            ImGui::MenuItem("Sprite sheets and frame animation", nullptr, &state.showSpriteSheetEditor);

            ImGui::MenuItem("Animation retargeting", nullptr, &state.showRetarget);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Diagnostics and extensions")) {
            ImGui::MenuItem("Profiler", nullptr, &state.showProfiler);
            ImGui::MenuItem("Runtime Inspector (ownership / readiness / saves)", nullptr,
                            &state.showRuntimeInspector);
            ImGui::MenuItem("Local mods / game kits", nullptr, &state.showMods);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::MenuItem("Navigation and shortcuts", nullptr, &state.showNavigationHelp);
        if (ImGui::MenuItem("Check this channel for updates"))
            requests.checkMilestones = true;
        if (ImGui::MenuItem("JudasJS reference"))
            requests.openJudasJS = true;
        if (ImGui::MenuItem("Making your first project"))
            requests.openQuickStart = true;
        if (ImGui::MenuItem("Ownership, readiness and saves guide"))
            requests.openWorkflowGuide = true;
        if (ImGui::MenuItem("Reset workspace layout"))
            requests.resetWorkspace = true;
        ImGui::Separator();
        ImGui::TextDisabled("Scene navigation (press in the viewport)");
        ImGui::TextUnformatted("Mouse wheel: zoom toward / away from focus");
        ImGui::TextUnformatted("Middle drag: pan in the view plane");
        ImGui::TextUnformatted("Right drag: look; hold WASD / Q / E to fly");
        ImGui::TextUnformatted("Shift: faster flight; F: focus selection");
        ImGui::TextUnformatted("Release or Escape: finish camera drag");
        ImGui::EndMenu();
    }
    if (windowMenu && ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("Navigation and shortcuts", nullptr, &state.showNavigationHelp);
        if (ImGui::MenuItem("Making your first project"))
            requests.openQuickStart = true;
        if (ImGui::MenuItem("JudasJS reference"))
            requests.openJudasJS = true;
        if (ImGui::MenuItem("Ownership, readiness and saves guide"))
            requests.openWorkflowGuide = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Check this channel for updates"))
            requests.checkMilestones = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Debug")) {
        DrawEditorOverlayMenu(state.debug);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Play")) {
        if (ImGui::MenuItem(editing ? "Play scene" : "Stop", "F5")) {
            if (editing)
                requests.play = true;
            else
                requests.stop = true;
        }
        if (ImGui::MenuItem("Run project", nullptr, false, hasProject && editing))
            requests.runProject = true;
        ImGui::EndMenu();
    }
    const char *projectName = hasProject ? state.project->Settings().name.c_str() : "No project";
    const std::string context = std::string(projectName) + "  /  " +
                                (editing            ? "EDIT"
                                 : state.playPaused ? "PLAY PAUSED"
                                                    : "PLAY");
    const float contextWidth = ImGui::CalcTextSize(context.c_str()).x;
    if (ImGui::GetContentRegionAvail().x > contextWidth + 20) {
        ImGui::SameLine(ImGui::GetWindowWidth() - contextWidth - 14.0f);
        ImGui::TextDisabled("%s", context.c_str());
    }
    ImGui::EndMainMenuBar();
    ImGui::PopStyleVar();
}

void DrawStatusBar(EditorDocument &doc, EditorPanelState &state) {
    PlaceWorkspacePanel(state.workspace.status);
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 4));
    ImGui::Begin("##status", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
    const std::string message = std::string(doc.IsDirty() ? "Unsaved edits  |  " : "") + state.status;
    ImGui::TextUnformatted(message.c_str());
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s\n%s", doc.Path().empty() ? "Unsaved scene" : doc.Path().c_str(),
                          state.status.c_str());
    if (!state.runtimeInfo.empty() &&
        state.workspace.status.size.x - ImGui::CalcTextSize(message.c_str()).x > 480) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", state.runtimeInfo.c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
