#include "EditorWorkspace.h"
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
    return ImGui::Begin(name, open, flags);
}

void EditorWorkspace::BeginFrame(EditorPanelState &state, EditorPreferences &preferences) {
    if (!state.dockingEnabled)
        return;
    const auto *viewport = ImGui::GetMainViewport();
    const float scale = std::max(.85f, ImGui::GetIO().FontGlobalScale);
    const float toolbarHeight = std::min(42.f * scale, viewport->WorkSize.y);
    const float statusHeight =
        std::min(25.f * scale, std::max(0.f, viewport->WorkSize.y - toolbarHeight));
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
                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(3);
    dockspace = ImGui::GetID("Judas.Workspace.Dockspace");
    std::string preset;
    const bool reset = preferences.ConsumeDockRebuild(preset);
    if (reset || (firstFrame && !ImGui::DockBuilderGetNode(dockspace)))
        BuildDefault(reset ? preset : preferences.WorkspaceName(), size);
    firstFrame = false;
    ImGui::DockSpace(dockspace, {0, 0}, ImGuiDockNodeFlags_None);
    ImGui::End();
    RecoverFloatingWindows();
}

void EditorWorkspace::BuildDefault(const std::string &workspace, ImVec2 size) {
    ImGui::DockBuilderRemoveNode(dockspace);
    ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace, size);
    ImGuiID center = dockspace;
    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, .18f, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, .28f, nullptr, &center);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, .29f, nullptr, &center);
    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Scene###Judas.Scene", center);
    ImGui::DockBuilderDockWindow("Game###Judas.Game", center);
    ImGui::DockBuilderDockWindow("Asset Browser", bottom);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Profiler", bottom);
    ImGui::DockBuilderDockWindow("Judas Tools", right);
    ImGui::DockBuilderDockWindow("JudasJS developer tools", bottom);
    if (workspace == "Scripting") {
        ImGui::DockBuilderDockWindow("JudasJS developer tools", center);
    } else if (workspace == "Animation") {
        ImGui::DockBuilderDockWindow("Animation timeline", bottom);
        ImGui::DockBuilderDockWindow("Animation retargeting | IMPORT > MAP > PREVIEW > BAKE",
                                     right);
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
    }
}

void EditorWorkspace::RecoverFloatingWindows() {
    auto *context = ImGui::GetCurrentContext();
    if (!context)
        return;
    const auto *main = ImGui::GetMainViewport();
    const auto &monitors = ImGui::GetPlatformIO().Monitors;
    const auto intersects = [](ImVec2 position, ImVec2 size, ImVec2 workPosition, ImVec2 workSize) {
        return position.x < workPosition.x + workSize.x &&
               position.y < workPosition.y + workSize.y && position.x + size.x > workPosition.x &&
               position.y + std::min(size.y, 32.f) > workPosition.y;
    };
    for (auto *window : context->Windows) {
        if (window->DockId ||
            (window->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup |
                              ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoSavedSettings)))
            continue;
        bool visible = false;
        if (monitors.empty())
            visible = intersects(window->Pos, window->Size, main->WorkPos, main->WorkSize);
        else
            for (const auto &monitor : monitors)
                visible |= intersects(window->Pos, window->Size, monitor.WorkPos, monitor.WorkSize);
        if (!visible || !std::isfinite(window->Pos.x) || !std::isfinite(window->Pos.y)) {
            ImGui::SetWindowPos(window, {main->WorkPos.x + 35, main->WorkPos.y + 35},
                                ImGuiCond_Always);
            ImGui::SetWindowSize(
                window,
                {std::min(std::max(200.f, window->Size.x), std::max(200.f, main->WorkSize.x - 70)),
                 std::min(std::max(120.f, window->Size.y), std::max(120.f, main->WorkSize.y - 70))},
                ImGuiCond_Always);
        }
    }
}

void DrawEditorWorkspaceSelector(EditorPreferences &preferences, EditorPanelState &) {
    ImGui::SetNextItemWidth(150 * ImGui::GetIO().FontGlobalScale);
    if (ImGui::BeginCombo("##workspacePreset", preferences.WorkspaceName().c_str())) {
        for (const auto &name : EditorPreferences::WorkspaceNames())
            if (ImGui::Selectable(name.c_str(), preferences.WorkspaceName() == name))
                preferences.QueueWorkspace(name);
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Workspace: arrange tools for this task. Your scene is unchanged.");
}
