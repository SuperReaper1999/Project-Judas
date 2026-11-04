#pragma once
#include "EditorWorkspaceLayout.h"
#include "imgui.h"
#include <cstring>
#include <string>

namespace EditorPanelWidgets {
inline constexpr ImGuiWindowFlags kWorkspacePanelFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                                         ImGuiWindowFlags_NoCollapse |
                                                         ImGuiWindowFlags_NoSavedSettings;
inline void NextActionOnRow(const char *nextLabel) {
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const float needed = ImGui::CalcTextSize(nextLabel).x + 2 * ImGui::GetStyle().FramePadding.x;
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + needed <= right)
        ImGui::SameLine();
}

inline void PlaceWorkspacePanel(const EditorWorkspaceRect &rectangle) {
    ImGui::SetNextWindowPos({rectangle.position.x, rectangle.position.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({rectangle.size.x, rectangle.size.y}, ImGuiCond_Always);
}

inline void ToolbarHint(const char *hint) {
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", hint);
}

inline void CopyToBuffer(const std::string &s, char *buffer, std::size_t size) {
    if (!size)
        return;
    std::strncpy(buffer, s.c_str(), size - 1);
    buffer[size - 1] = '\0';
}
} // namespace EditorPanelWidgets
