#include "EditorPlatformPlacement.h"
#include "imgui_internal.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {
bool Finite(ImVec2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }

ImVec2 AdmittedDragPosition(ImGuiViewportP* viewport, ImVec2 requested) {
    auto& context = *ImGui::GetCurrentContext();
    const auto& monitors = ImGui::GetPlatformIO().Monitors;
    int index = viewport->PlatformMonitor;
    float nearest = std::numeric_limits<float>::max();
    if (ImGui::IsMousePosValid()) for (int i = 0; i < monitors.Size; ++i) {
        const auto& monitor = monitors[i];
        if (!Finite(monitor.MainPos) || !Finite(monitor.MainSize) ||
            monitor.MainSize.x <= 0 || monitor.MainSize.y <= 0) continue;
        const float dx = context.IO.MousePos.x - std::clamp(context.IO.MousePos.x,
            monitor.MainPos.x, monitor.MainPos.x + monitor.MainSize.x);
        const float dy = context.IO.MousePos.y - std::clamp(context.IO.MousePos.y,
            monitor.MainPos.y, monitor.MainPos.y + monitor.MainSize.y);
        const float distance = dx * dx + dy * dy;
        if (distance < nearest) { nearest = distance; index = i; }
    }
    if (index < 0 || index >= monitors.Size) return requested;
    const auto& monitor = monitors[index];
    if (!Finite(monitor.WorkPos) || !Finite(monitor.WorkSize) ||
        monitor.WorkSize.x <= 0 || monitor.WorkSize.y <= 0) return requested;
    // KWin/XWayland admits the entire native client rectangle in this work
    // area. Project the drag before Begin() builds the header and hit boxes,
    // rather than repeatedly requesting a position the compositor rejects.
    const ImVec2 maximum{
        monitor.WorkPos.x + std::max(0.f, monitor.WorkSize.x - viewport->Window->Size.x),
        monitor.WorkPos.y + std::max(0.f, monitor.WorkSize.y - viewport->Window->Size.y)};
    return {std::clamp(requested.x, monitor.WorkPos.x, maximum.x),
            std::clamp(requested.y, monitor.WorkPos.y, maximum.y)};
}
} // namespace

void ReconcileEditorPlatformPlacement(ImGuiViewport* publicViewport) {
    auto* context = ImGui::GetCurrentContext();
    if (!context || !publicViewport || publicViewport == ImGui::GetMainViewport()) return;
    auto* viewport = static_cast<ImGuiViewportP*>(publicViewport);
    auto* owner = viewport->Window;
    if (!owner || !owner->ViewportOwned) return;
    if (owner->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) return;
    const bool moving = context->MovingWindow &&
        context->MovingWindow->RootWindowDockTree == owner->RootWindowDockTree;
    auto position = owner->Pos;
    if (moving) {
        position = AdmittedDragPosition(viewport, position);
    } else {
        // A release can arrive after the last ConfigureNotify. ImGui's moving
        // window path overwrites that admitted position while dragging, so
        // explicitly adopt it before building the now-idle window controls.
        const auto getPosition = ImGui::GetPlatformIO().Platform_GetWindowPos;
        if (!viewport->PlatformWindowCreated || !getPosition ||
            ((context->NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasPos) && !owner->DockNodeAsHost)) return;
        // Respect explicit application/recovery placement until the platform
        // has received it. A stale drag has matching logical window/viewport
        // positions and a different native position, not this pending move.
        if (!viewport->PlatformRequestMove &&
            (owner->Pos.x != viewport->Pos.x || owner->Pos.y != viewport->Pos.y)) return;
        position = getPosition(viewport);
    }
    if (!Finite(position)) return;
    ImGui::SetWindowPos(owner, position, ImGuiCond_Always);
    viewport->Pos = owner->Pos;
    viewport->UpdateWorkRect();
    if (owner->DockNodeAsHost && (context->NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasPos))
        context->NextWindowData.PosVal = owner->Pos;
}

void InstallEditorPlatformPlacement() {
    const char* driver = SDL_GetCurrentVideoDriver();
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    if (!driver || std::strcmp(driver, "x11") || !wayland || !*wayland) return;
    auto& platform = ImGui::GetPlatformIO();
    // SDL2 leaves this pre-Begin viewport hook unused. Do not take over a
    // different platform owner's hook; native X11, Windows and Wayland paths
    // are otherwise unchanged.
    if (!platform.Platform_OnChangedViewport)
        platform.Platform_OnChangedViewport = ReconcileEditorPlatformPlacement;
}
