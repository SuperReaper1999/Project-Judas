#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include <SDL2/SDL.h>
#include <glm/glm.hpp>

struct EditorAutomationNativePoint {
    SDL_Window *window = nullptr;
    glm::ivec2 position{0};
};

// ImGui multi-viewport positions are desktop coordinates; SDL events and warp
// APIs are local to their owning native window. The fallback preserves the
// legacy single-window fixtures when docking is disabled.
inline EditorAutomationNativePoint EditorAutomationPointInWindow(glm::vec2 point, SDL_Window *fallback,
                                                                 bool docking, const char *panelName) {
    EditorAutomationNativePoint result{fallback, {int(point.x), int(point.y)}};
    if (!docking || !ImGui::GetCurrentContext())
        return result;
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    if (panelName)
        if (const auto *panel = ImGui::FindWindowByName(panelName))
            viewport = panel->Viewport;
    if (!viewport)
        return result;
    const auto id = Uint32(reinterpret_cast<intptr_t>(viewport->PlatformHandle));
    if (auto *owner = SDL_GetWindowFromID(id))
        result.window = owner;
    result.position = {int(point.x - viewport->Pos.x), int(point.y - viewport->Pos.y)};
    return result;
}
