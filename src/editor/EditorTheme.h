#pragma once

#include "imgui.h"

// Editor-only chrome. Scene materials and project runtime UI keep their own
// authored appearance; this palette never changes game content.
inline void ApplyEditorTheme() {
    ImGui::StyleColorsDark();
    auto& s = ImGui::GetStyle();
    s.WindowPadding = {12, 10};
    s.FramePadding = {7, 5};
    s.ItemSpacing = {8, 7};
    s.ItemInnerSpacing = {6, 5};
    s.WindowRounding = 5;
    s.ChildRounding = 4;
    s.FrameRounding = 4;
    s.PopupRounding = 5;
    s.GrabRounding = 3;
    s.TabRounding = 4;
    s.WindowBorderSize = 1;
    s.FrameBorderSize = 1;
    s.ScrollbarSize = 14;
    s.GrabMinSize = 12;
    auto& c = s.Colors;
    c[ImGuiCol_Text] = {0.86f, 0.91f, 0.94f, 1};
    c[ImGuiCol_TextDisabled] = {0.56f, 0.64f, 0.70f, 1};
    c[ImGuiCol_WindowBg] = {0.075f, 0.091f, 0.115f, 1};
    c[ImGuiCol_ChildBg] = {0.061f, 0.075f, 0.095f, 1};
    c[ImGuiCol_PopupBg] = {0.094f, 0.113f, 0.140f, 1};
    c[ImGuiCol_Border] = {0.19f, 0.23f, 0.28f, 1};
    c[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
    c[ImGuiCol_FrameBg] = {0.13f, 0.16f, 0.20f, 1};
    c[ImGuiCol_FrameBgHovered] = {0.18f, 0.25f, 0.29f, 1};
    c[ImGuiCol_FrameBgActive] = {0.18f, 0.31f, 0.34f, 1};
    c[ImGuiCol_TitleBg] = {0.075f, 0.091f, 0.115f, 1};
    c[ImGuiCol_TitleBgActive] = {0.10f, 0.14f, 0.17f, 1};
    c[ImGuiCol_MenuBarBg] = {0.056f, 0.070f, 0.087f, 1};
    c[ImGuiCol_ScrollbarBg] = {0.061f, 0.075f, 0.095f, 1};
    c[ImGuiCol_ScrollbarGrab] = {0.23f, 0.28f, 0.33f, 1};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.31f, 0.39f, 0.43f, 1};
    c[ImGuiCol_ScrollbarGrabActive] = {0.35f, 0.49f, 0.52f, 1};
    c[ImGuiCol_CheckMark] = {0.37f, 0.77f, 0.75f, 1};
    c[ImGuiCol_SliderGrab] = {0.32f, 0.67f, 0.67f, 1};
    c[ImGuiCol_SliderGrabActive] = {0.43f, 0.84f, 0.80f, 1};
    c[ImGuiCol_Button] = {0.14f, 0.22f, 0.26f, 1};
    c[ImGuiCol_ButtonHovered] = {0.20f, 0.35f, 0.39f, 1};
    c[ImGuiCol_ButtonActive] = {0.20f, 0.44f, 0.46f, 1};
    c[ImGuiCol_Header] = {0.13f, 0.21f, 0.25f, 1};
    c[ImGuiCol_HeaderHovered] = {0.18f, 0.32f, 0.36f, 1};
    c[ImGuiCol_HeaderActive] = {0.20f, 0.40f, 0.42f, 1};
    c[ImGuiCol_Separator] = {0.18f, 0.23f, 0.28f, 1};
    c[ImGuiCol_SeparatorHovered] = {0.31f, 0.62f, 0.63f, 1};
    c[ImGuiCol_SeparatorActive] = {0.37f, 0.77f, 0.75f, 1};
    c[ImGuiCol_ResizeGrip] = {0.25f, 0.46f, 0.49f, 0.3f};
    c[ImGuiCol_ResizeGripHovered] = {0.31f, 0.62f, 0.63f, 0.7f};
    c[ImGuiCol_ResizeGripActive] = {0.37f, 0.77f, 0.75f, 1};
    c[ImGuiCol_Tab] = {0.09f, 0.13f, 0.17f, 1};
    c[ImGuiCol_TabHovered] = {0.18f, 0.32f, 0.36f, 1};
    c[ImGuiCol_TabSelected] = {0.16f, 0.27f, 0.31f, 1};
    c[ImGuiCol_TextSelectedBg] = {0.25f, 0.54f, 0.57f, 0.45f};
    c[ImGuiCol_NavCursor] = {0.37f, 0.77f, 0.75f, 1};
    c[ImGuiCol_ModalWindowDimBg] = {0.025f, 0.035f, 0.045f, 0.75f};
}
