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
void WorkspaceSplitter(const char *id, const EditorWorkspaceRect &rectangle, bool vertical, float &dimension,
                       float direction, float minimum, float maximum) {
    if (rectangle.size.x <= 0 || rectangle.size.y <= 0)
        return;
    PlaceWorkspacePanel(rectangle);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
    ImGui::Begin(id, nullptr,
                 kWorkspacePanelFlags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavInputs |
                     ImGuiWindowFlags_NoNavFocus);
    ImGui::InvisibleButton("##resize", {rectangle.size.x, rectangle.size.y});
    const bool active = ImGui::IsItemActive();
    if (active || ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        const auto color = ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive : ImGuiCol_SeparatorHovered);
        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), color);
    }
    if (active) {
        const auto delta = ImGui::GetIO().MouseDelta;
        dimension = std::clamp(dimension + direction * (vertical ? delta.x : delta.y), minimum, maximum);
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}
} // namespace

void UpdateEditorWorkspaceLayout(EditorPanelState &state, glm::vec2 displaySize) {
    if (state.dockingEnabled)
        return; // Dock host and content panels publish actual rectangles.
    const bool rails = state.mode == EditorMode::Edit || state.playPaused;
    state.workspace = CalculateEditorWorkspaceLayout(
        displaySize, state.hierarchyWidth, state.inspectorWidth, state.assetBrowserHeight,
        state.showAssetBrowser && state.mode == EditorMode::Edit, rails, ImGui::GetIO().FontGlobalScale);
}

void DrawEditorWorkspaceChrome(EditorDocument &doc, EditorPanelState &state, EditorRequests &requests,
                               const std::function<void()> &workspaceSelector,
                               const std::function<void()> &workflowSelector) {
    const bool editing = state.mode == EditorMode::Edit;
    const auto display = ImGui::GetIO().DisplaySize;
    PlaceWorkspacePanel(state.workspace.toolbar);
    const float uiScale = ImGui::GetIO().FontGlobalScale;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10 * uiScale, 6 * uiScale));
    ImGui::Begin("##workspaceToolbar", nullptr,
                 kWorkspacePanelFlags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse);
    if (workflowSelector) {
        workflowSelector();
        if (workspaceSelector) {
            const float right = ImGui::GetWindowContentRegionMax().x - 170.f * uiScale;
            if (right > ImGui::GetCursorPosX() + 12.f * uiScale) {
                ImGui::SameLine(right);
                workspaceSelector();
            } else {
                ImGui::SameLine();
                if (ImGui::SmallButton("Layout..."))
                    ImGui::OpenPopup("workflowLayouts");
                if (ImGui::BeginPopup("workflowLayouts")) {
                    workspaceSelector();
                    ImGui::EndPopup();
                }
            }
        }
        ImGui::Separator();
        ImGui::Spacing();
    }
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
    if (EditorVisuals::ActionButton(editing ? "Play" : "Stop",
                                    editing ? EditorVisuals::Icon::Play : EditorVisuals::Icon::Stop, true)) {
        if (editing)
            requests.play = true;
        else
            requests.stop = true;
    }
    ImGui::PopStyleColor(2);
    ToolbarHint("F5: play this scene / restore the authored scene");
    ImGui::SameLine();
    ImGui::BeginDisabled(!editing);
    if (EditorVisuals::ActionButton("Save", EditorVisuals::Icon::File))
        requests.save = true;
    ToolbarHint("Ctrl+S: save the scene");
    ImGui::SameLine();
    if (EditorVisuals::ActionButton("Create...", EditorVisuals::Icon::Plus))
        ImGui::OpenPopup("toolbarCreate");
    ToolbarHint("Add objects, a first/third-person player, lights or a pause menu");
    if (ImGui::BeginPopup("toolbarCreate")) {
        DrawEditorCreationItems(state, requests);
        ImGui::EndPopup();
    }
    const float logicalWidth = display.x / std::max(.85f, ImGui::GetIO().FontGlobalScale);
    if (logicalWidth >= 800) {
        ImGui::SameLine();
        ImGui::BeginDisabled(!doc.CanUndo());
        if (ImGui::Button("Undo"))
            requests.undo = true;
        ImGui::EndDisabled();
        ToolbarHint("Ctrl+Z: undo the last scene edit");
        ImGui::SameLine();
        ImGui::BeginDisabled(!doc.CanRedo());
        if (ImGui::Button("Redo"))
            requests.redo = true;
        ImGui::EndDisabled();
        ToolbarHint("Ctrl+Y: redo the scene edit");
    }
    ImGui::SameLine();
    if (logicalWidth >= 1000) {
        const auto tool = [&](const char *label, GizmoMode mode, EditorVisuals::Icon icon, const char *hint) {
            const bool selected = state.gizmoMode == mode;
            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (EditorVisuals::ActionButton(label, icon, selected))
                state.gizmoMode = mode;
            if (selected)
                ImGui::PopStyleColor();
            ToolbarHint(hint);
            ImGui::SameLine();
        };
        tool("Move", GizmoMode::Translate, EditorVisuals::Icon::Move, "W: move selected objects");
        tool("Rotate", GizmoMode::Rotate, EditorVisuals::Icon::Rotate, "E: rotate selected objects");
        tool("Scale", GizmoMode::Scale, EditorVisuals::Icon::Scale, "R: scale selected objects");
    }
    if (ImGui::Button(logicalWidth >= 1000 ? "Options..." : "Transform..."))
        ImGui::OpenPopup("toolbarTransform");
    ToolbarHint("Transform tools, world/local axes and snapping; W / E / R / X shortcuts remain available");
    if (ImGui::BeginPopup("toolbarTransform")) {
        if (ImGui::MenuItem("Move", "W", state.gizmoMode == GizmoMode::Translate))
            state.gizmoMode = GizmoMode::Translate;
        if (ImGui::MenuItem("Rotate", "E", state.gizmoMode == GizmoMode::Rotate))
            state.gizmoMode = GizmoMode::Rotate;
        if (ImGui::MenuItem("Scale", "R", state.gizmoMode == GizmoMode::Scale))
            state.gizmoMode = GizmoMode::Scale;
        ImGui::Separator();
        if (ImGui::MenuItem("Local axes", "X", state.gizmoSpace == GizmoSpace::Local))
            state.gizmoSpace = state.gizmoSpace == GizmoSpace::Local ? GizmoSpace::World : GizmoSpace::Local;
        ImGui::MenuItem("Snap", "hold Ctrl", &state.gizmoSnap);
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("View..."))
        ImGui::OpenPopup("toolbarView");
    if (logicalWidth >= 750) {
        ImGui::SameLine();
        ImGui::BeginDisabled(!editing);
        if (ImGui::Button(state.view2D ? "2D view" : "3D view"))
            state.view2D = !state.view2D;
        ToolbarHint("Switch the editing view; gameplay cameras are unchanged.");
        ImGui::EndDisabled();
    }
    if (ImGui::BeginPopup("toolbarView")) {
        ImGui::MenuItem("2D scene view", nullptr, &state.view2D, editing);
        ImGui::MenuItem("Scene settings", nullptr, &state.showSceneSettings, editing);
        ImGui::MenuItem("Asset Browser", nullptr, &state.showAssetBrowser);
        ImGui::MenuItem("Editor preferences / layouts", nullptr, &state.showEditorPreferences);
        ImGui::MenuItem("Navigation and shortcuts", nullptr, &state.showNavigationHelp);
        ImGui::EndPopup();
    }
    if (!workflowSelector && workspaceSelector) {
        const float selectorWidth = 166.f * uiScale;
        const float right = ImGui::GetWindowContentRegionMax().x - selectorWidth;
        const float current = ImGui::GetCursorPosX();
        if (right > current + 12.f * uiScale) {
            ImGui::SameLine(right);
            ImGui::PushStyleColor(ImGuiCol_Border, EditorSecondaryAccent(.65f));
            workspaceSelector();
            ImGui::PopStyleColor();
        } else {
            ImGui::SameLine();
            if (ImGui::Button("Layout..."))
                ImGui::OpenPopup("compactWorkspaceSelector");
            if (ImGui::BeginPopup("compactWorkspaceSelector")) {
                ImGui::TextUnformatted("Workspace");
                workspaceSelector();
                ImGui::EndPopup();
            }
        }
    } else if (logicalWidth >= 1100) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s%s", doc.GetScene().Settings().name.c_str(), doc.IsDirty() ? " *" : "");
    }
    ImGui::End();
    ImGui::PopStyleVar();

    if (!state.dockingEnabled && state.workspace.showRails) {
        WorkspaceSplitter("##hierarchySplitter", state.workspace.leftSplitter, true, state.hierarchyWidth,
                          1.0f, 150.0f, std::max(150.0f, display.x * 0.40f));
        WorkspaceSplitter("##inspectorSplitter", state.workspace.rightSplitter, true, state.inspectorWidth,
                          -1.0f, 220.0f, std::max(220.0f, display.x * 0.46f));
        WorkspaceSplitter("##assetsSplitter", state.workspace.bottomSplitter, false, state.assetBrowserHeight,
                          -1.0f, 80.0f, std::max(80.0f, display.y * 0.55f));
    }
    if (!state.dockingEnabled)
        DrawEditorNavigationHelp(state, requests);
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
            ImGui::MenuItem("Volumetric terrain - dig / build", nullptr, &state.showVolumeTerrain);
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
        if (ImGui::MenuItem("Reset workspace layout")) {
            state.hierarchyWidth = 250;
            state.inspectorWidth = 340;
            state.assetBrowserHeight = 200;
            state.showAssetBrowser = true;
        }
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
    if (ImGui::BeginMenu("World", !editing)) {
        ImGui::TextDisabled("%s", state.worldStatePath.empty() ? "(no world-state path: save the scene first)"
                                                               : state.worldStatePath.c_str());
        if (ImGui::MenuItem("Save world state", "F6", false, !state.worldStatePath.empty()))
            requests.saveWorldState = true;
        if (ImGui::MenuItem("Delete world state (pristine baseline next Play)", "F7", false,
                            !state.worldStatePath.empty()))
            requests.deleteWorldState = true;
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 4));
    ImGui::Begin("##status", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
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
