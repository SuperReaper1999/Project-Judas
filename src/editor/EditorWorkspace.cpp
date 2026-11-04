#include "EditorWorkspace.h"
#include "EditorVisuals.h"
#include "EditorPanels.h"
#include "EditorPreferences.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>

bool BeginEditorPanel(const char *name, bool *open, EditorPanelState &state,
                      const EditorWorkspaceRect *legacy, ImGuiWindowFlags flags) {
    if (!state.dockingEnabled && legacy) {
        ImGui::SetNextWindowPos({legacy->position.x, legacy->position.y}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({legacy->size.x, legacy->size.y}, ImGuiCond_Always);
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoSavedSettings;
    }
    ImGui::PushFont(EditorVisuals::HeadingFont());
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {12, 7});
    const bool visible = ImGui::Begin(name, open, flags);
    ImGui::PopStyleVar();
    ImGui::PopFont();
    return visible;
}

void EditorWorkspace::BeginFrame(EditorPanelState &state, EditorPreferences &preferences) {
    if (!state.dockingEnabled)
        return;
    // New dock windows can focus themselves on their creation frame. Apply
    // the chosen preset once they exist; never override a loaded/user layout.
    const auto selectTab = [](const std::string &name) {
        if (auto *window = ImGui::FindWindowByName(name.c_str()))
            if (window->DockNode && window->DockNode->TabBar)
                window->DockNode->TabBar->NextSelectedTabId = window->TabId;
    };
    if (!pendingBottomTab.empty()) {
        selectTab(pendingBottomTab);
        pendingBottomTab.clear();
    }
    if (!pendingDefaultFocus.empty()) {
        selectTab(pendingDefaultFocus);
        ImGui::SetWindowFocus(pendingDefaultFocus.c_str());
        pendingDefaultFocus.clear();
    }
    const auto *viewport = ImGui::GetMainViewport();
    const float scale = std::max(.85f, ImGui::GetIO().FontGlobalScale);
    const float toolbarHeight = std::min(100.f * scale, viewport->WorkSize.y);
    const float statusHeight = std::min(25.f * scale, std::max(0.f, viewport->WorkSize.y - toolbarHeight));
    const ImVec2 position{viewport->WorkPos.x, viewport->WorkPos.y + toolbarHeight};
    const ImVec2 size{viewport->WorkSize.x,
                      std::max(1.f, viewport->WorkSize.y - toolbarHeight - statusHeight)};
    state.workspace.toolbar = {{viewport->WorkPos.x, viewport->WorkPos.y},
                               {viewport->WorkSize.x, toolbarHeight}};
    state.workspace.status = {
        {viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - statusHeight},
        {viewport->WorkSize.x, statusHeight}};
    state.workspace.showRails = true;
    state.workspace.showAssets = state.showAssetBrowser;
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::Begin("##JudasWorkspaceDockHost", nullptr,
                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(3);
    dockspace = ImGui::GetID("Judas.Workspace.Dockspace");
    std::string preset;
    const bool reset = preferences.ConsumeDockRebuild(preset);
    if (reset || (firstFrame && !ImGui::DockBuilderGetNode(dockspace)))
        BuildDefault(reset ? preset : preferences.WorkspaceName(), size);
    firstFrame = false;
    // Keep one close button per panel tab. The dock-node close button is a
    // duplicate; tabs still support close, drag, split and their context menu.
    const auto tidyDock = [&](auto &&self, ImGuiDockNode *node) -> void {
        if (!node)
            return;
        node->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton | ImGuiDockNodeFlags_NoWindowMenuButton;
        self(self, node->ChildNodes[0]);
        self(self, node->ChildNodes[1]);
    };
    tidyDock(tidyDock, ImGui::DockBuilderGetNode(dockspace));
    ImGui::PushFont(EditorVisuals::HeadingFont());
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {12, 7});
    ImGui::DockSpace(dockspace, {0, 0}, ImGuiDockNodeFlags_None);
    ImGui::PopStyleVar();
    ImGui::PopFont();
    ImGui::End();
    RecoverFloatingWindows();
}

void EditorWorkspace::BuildDefault(const std::string &workspace, ImVec2 size) {
    ImGui::DockBuilderRemoveNode(dockspace);
    ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace, size);
    // Fresh layouts must grow their property rails with readable UI text.
    // Loaded/user dock trees are retained verbatim and never resized here.
    const float scale = ImGui::GetIO().FontGlobalScale;
    const float leftWidth = std::min(size.x * .27f, 250.f * scale);
    const float rightWidth = std::min(size.x * .38f, 340.f * scale);
    ImGuiID center = dockspace;
    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, leftWidth / size.x, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(
        center, ImGuiDir_Right, rightWidth / std::max(1.f, size.x - leftWidth), nullptr, &center);
    const float contentRatio = std::clamp(.34f + .1f * (scale - 1.f), .30f, .42f);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, contentRatio, nullptr, &center);
    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Scene###Judas.Scene", center);
    ImGui::DockBuilderDockWindow("Game###Judas.Game", center);
    ImGui::DockBuilderDockWindow("Asset Browser", bottom);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Profiler", bottom);
    // Judas Tools starts as a small movable utility, not another inspector tab.
    ImGui::DockBuilderDockWindow("JudasJS developer tools", bottom);
    if (workspace == "Scripting") {
        ImGui::DockBuilderDockWindow("JudasJS developer tools", center);
    } else if (workspace == "Animation") {
        ImGui::DockBuilderDockWindow("Animation timeline", bottom);
        ImGui::DockBuilderDockWindow("Animation retargeting | IMPORT > MAP > PREVIEW > BAKE", right);
    } else if (workspace == "UI") {
        ImGui::DockBuilderDockWindow("Asset Browser", center);
    } else if (workspace == "Audio") {
        ImGui::DockBuilderDockWindow("Project Settings", center);
    }
    ImGui::DockBuilderFinish(dockspace);
    if (auto *node = ImGui::DockBuilderGetNode(center)) {
        const char *selected = workspace == "Scripting" ? "JudasJS developer tools"
                               : workspace == "UI"      ? "Asset Browser"
                               : workspace == "Audio"   ? "Project Settings"
                                                        : "Scene###Judas.Scene";
        node->SelectedTabId = ImHashStr(selected);
        pendingDefaultFocus = selected;
    }
    pendingBottomTab = workspace == "Scripting" ? "Console" : "Asset Browser";
    if (auto *node = ImGui::DockBuilderGetNode(bottom))
        node->SelectedTabId = ImHashStr(pendingBottomTab.c_str());
}

void EditorWorkspace::RecoverFloatingWindows() {
    auto *context = ImGui::GetCurrentContext();
    if (!context)
        return;
    const auto *main = ImGui::GetMainViewport();
    const auto &monitors = ImGui::GetPlatformIO().Monitors;
    const auto intersects = [](ImVec2 position, ImVec2 size, ImVec2 workPosition, ImVec2 workSize) {
        return position.x < workPosition.x + workSize.x && position.y < workPosition.y + workSize.y &&
               position.x + size.x > workPosition.x && position.y + std::min(size.y, 32.f) > workPosition.y;
    };
    for (auto *window : context->Windows) {
        if (window->DockId || (window->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup |
                                                ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoSavedSettings)))
            continue;
        bool visible = false;
        if (monitors.empty())
            visible = intersects(window->Pos, window->Size, main->WorkPos, main->WorkSize);
        else
            for (const auto &monitor : monitors)
                visible |= intersects(window->Pos, window->Size, monitor.WorkPos, monitor.WorkSize);
        if (!visible || !std::isfinite(window->Pos.x) || !std::isfinite(window->Pos.y)) {
            ImGui::SetWindowPos(window, {main->WorkPos.x + 35, main->WorkPos.y + 35}, ImGuiCond_Always);
            ImGui::SetWindowSize(
                window,
                {std::min(std::max(200.f, window->Size.x), std::max(200.f, main->WorkSize.x - 70)),
                 std::min(std::max(120.f, window->Size.y), std::max(120.f, main->WorkSize.y - 70))},
                ImGuiCond_Always);
        }
    }
}

void DrawEditorWorkspaceSelector(EditorPreferences &preferences, EditorPanelState &state) {
    EditorDesign::Field workspaceField;
    ImGui::SetNextItemWidth(150 * ImGui::GetIO().FontGlobalScale);
    if (ImGui::BeginCombo("##workspacePreset", preferences.WorkspaceName().c_str())) {
        for (const auto &name : EditorPreferences::WorkspaceNames()) {
            const bool unavailable = state.mode != EditorMode::Edit && (name == "Audio" || name == "UI");
            ImGui::BeginDisabled(unavailable);
            if (ImGui::Selectable(name.c_str(), preferences.WorkspaceName() == name))
                preferences.QueueWorkspace(name);
            ImGui::EndDisabled();
            if (unavailable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Stop Play to use this authoring workspace.");
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Workspace: arrange tools for this task. Your scene is unchanged.");
}
