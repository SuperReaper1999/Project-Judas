#include "EditorVisuals.h"
#include "EditorOverlayMenu.h"
#include <cstdio>
#include "EditorViewportPresenter.h"
#include "EditorContext.h"
#include "EditorPanels.h"
#include "EditorSceneViewport.h"
#include "EditorPlaySession.h"
#include "EngineHost.h"
#include "Window.h"
#include "Renderer.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
namespace {
void Surface(EditorViewSurface &surface) {
    const auto p = ImGui::GetCursorScreenPos(), s = ImGui::GetContentRegionAvail();
    surface.bounds = {{p.x, p.y}, {std::max(1.f, s.x), std::max(1.f, s.y)}};
    const auto *viewport = ImGui::GetWindowViewport();
    surface.nativeOrigin = {viewport->Pos.x, viewport->Pos.y};
    surface.windowId = Uint32(reinterpret_cast<intptr_t>(viewport->PlatformHandle));
    surface.visible = s.x > 1 && s.y > 1;
    surface.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
}
void Image(EditorViewSurface &surface, Renderer &renderer, RenderTargetHandle &target, bool frozen,
           std::string &status) {
    std::string error;
    if (!frozen && !renderer.ResizeRenderTarget(target, int(surface.bounds.size.x),
                                                int(surface.bounds.size.y), error, /*display=*/true)) {
        status = "Viewport unavailable: " + error;
        ImGui::TextWrapped("%s", status.c_str());
        return;
    }
    if (target.IsValid()) {
        const auto token = renderer.EditorImageToken(renderer.RenderTargetTexture(target));
        ImGui::Image(static_cast<ImTextureID>(token), {surface.bounds.size.x, surface.bounds.size.y}, {0, 1},
                     {1, 0});
        surface.hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                          ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        // ImGui normally focuses windows on a left press only. Editor camera
        // gestures also begin with right/middle, but must not steal a widget's
        // active drag, a text edit or a popup's pointer ownership.
        if (surface.hovered && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
            (ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Middle))) {
            ImGui::SetWindowFocus();
            surface.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        }
    } else
        ImGui::Dummy({surface.bounds.size.x, surface.bounds.size.y});
}
} // namespace
void EditorViewportPresenter::BeginFrame() {
    scene.visible = scene.hovered = game.visible = game.hovered = false;
}
void EditorViewportPresenter::DrawScene(EditorContext &context, EditorSceneViewport &viewport) {
    if (focusScene) {
        ImGui::SetNextWindowFocus();
        focusScene = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    if (ImGui::Begin("Scene###Judas.Scene", &context.panels.showSceneView,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        Surface(scene);
        context.panels.workspace.viewport = scene.bounds;
        Image(scene, context.host.GetRenderer(), viewport.target,
              context.phase == EditorPanelPhase::DebugStopped, context.panels.status);
        const auto afterImage = ImGui::GetCursorScreenPos();
        const float scale = ImGui::GetIO().FontGlobalScale;
        bool overlayOwns = false;
        ImGui::SetCursorScreenPos(
            {scene.bounds.position.x + 12 * scale, scene.bounds.position.y + 12 * scale});
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {9 * scale, 5 * scale});
        ImGui::BeginDisabled(context.phase != EditorPanelPhase::Edit);
        if (EditorVisuals::ActionButton(context.panels.view2D ? "2D Orthographic" : "Perspective",
                                        EditorVisuals::Icon::Cube))
            ImGui::OpenPopup("sceneProjection");
        overlayOwns |= ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()) ||
                       ImGui::IsItemActive();
        if (ImGui::BeginPopup("sceneProjection")) {
            if (ImGui::MenuItem("Perspective (3D)", nullptr, !context.panels.view2D))
                context.panels.view2D = false;
            if (ImGui::MenuItem("Orthographic (2D)", nullptr, context.panels.view2D))
                context.panels.view2D = true;
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
        const float toolsWidth = 185 * scale;
        if (scene.bounds.size.x > 400 * scale) {
            ImGui::SetCursorScreenPos({scene.bounds.position.x + scene.bounds.size.x - toolsWidth,
                                       scene.bounds.position.y + 12 * scale});
            ImGui::BeginDisabled(context.phase == EditorPanelPhase::DebugStopped);
            if (EditorVisuals::ActionButton("Overlays", EditorVisuals::Icon::Light))
                ImGui::OpenPopup("sceneOverlays");
            overlayOwns |= ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()) ||
                           ImGui::IsItemActive();
            if (ImGui::BeginPopup("sceneOverlays")) {
                DrawEditorOverlayMenu(context.panels.debug);
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(context.phase != EditorPanelPhase::Edit ||
                                 !context.document.SelectedObject());
            if (EditorVisuals::ActionButton("Frame", EditorVisuals::Icon::Search))
                context.requests.focusSelection = true;
            overlayOwns |= ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()) ||
                           ImGui::IsItemActive();
            ImGui::EndDisabled();
            ImGui::EndDisabled();
        }
        ImGui::PopStyleVar();
        if (const auto *selected = context.document.SelectedObject()) {
            const auto position = ImVec2(scene.bounds.position.x + 12 * scale,
                                         scene.bounds.position.y + scene.bounds.size.y - 31 * scale);
            char status[512];
            std::snprintf(status, sizeof(status), "%s    |    %s axes", selected->name.c_str(),
                          context.panels.gizmoSpace == GizmoSpace::Local ? "Local" : "World");
            auto background = EditorVisuals::SurfaceColor();
            background.w = .92f;
            const auto textSize = ImGui::CalcTextSize(status);
            const float width = std::min(textSize.x + 20 * scale, scene.bounds.size.x - 24 * scale);
            auto *draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(position, {position.x + width, position.y + 25 * scale},
                                ImGui::GetColorU32(background), 5 * scale);
            draw->PushClipRect(position, {position.x + width, position.y + 25 * scale}, true);
            draw->AddText({position.x + 9 * scale, position.y + 5 * scale}, ImGui::GetColorU32(ImGuiCol_Text),
                          status);
            draw->PopClipRect();
        }
        // Overlay widgets own their physical rectangles even while disabled.
        // Submit the image's drop target only after those owners are known;
        // otherwise dropping over a toolbar control creates an object below it.
        const ImRect imageRect(
            {scene.bounds.position.x, scene.bounds.position.y},
            {scene.bounds.position.x + scene.bounds.size.x, scene.bounds.position.y + scene.bounds.size.y});
        if (context.phase == EditorPanelPhase::Edit && !overlayOwns &&
            ImGui::BeginDragDropTargetCustom(imageRect, ImGui::GetID("SceneAssetDrop"))) {
            if (const auto *payload = ImGui::AcceptDragDropPayload(kAssetDragPayload)) {
                viewport.pendingDropAsset.assign(static_cast<const char *>(payload->Data), payload->DataSize);
                viewport.pendingDropDelivered = true;
                viewport.pendingDropPosition = {ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y};
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SetCursorScreenPos(afterImage);
        ImGui::Dummy({0, 0});
        scene.hovered &= !overlayOwns;
        context.panels.sceneViewHovered = scene.hovered;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
void EditorViewportPresenter::DrawGame(EditorContext &context, EditorPlaySession &session) {
    if (focusGame) {
        ImGui::SetNextWindowFocus();
        focusGame = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    if (ImGui::Begin("Game###Judas.Game", &context.panels.showGameView,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (session.world) {
            Surface(game);
            Image(game, context.host.GetRenderer(), gameTarget,
                  context.phase == EditorPanelPhase::DebugStopped, context.panels.status);
        } else {
            ImGui::Spacing();
            ImGui::TextUnformatted("Play your scene");
            ImGui::TextWrapped("Press Play or F5. Stop returns to your unchanged authored scene.");
            if (ImGui::Button("Play scene"))
                context.requests.play = true;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
void EditorViewportPresenter::ConfigureInput(Window &window, bool playing) {
    const auto &surface = playing ? game : scene;
    if (!surface.visible) {
        if (!playing)
            window.ClearInputRegion();
        window.SetInputClaimed(true, true);
        window.SetMouseCaptured(false);
        return;
    }
    WindowInputRegion region{surface.windowId,
                             {int(surface.bounds.position.x - surface.nativeOrigin.x),
                              int(surface.bounds.position.y - surface.nativeOrigin.y),
                              int(surface.bounds.size.x), int(surface.bounds.size.y)},
                             int(surface.bounds.size.x),
                             int(surface.bounds.size.y)};
    window.SetInputRegion(region);
    const bool keyboard = surface.focused && !ImGui::GetIO().WantTextInput;
    const bool pointer = keyboard && (surface.hovered || window.IsMouseCaptured());
    window.SetInputClaimed(!keyboard, !pointer && !window.IsMouseCaptured());
}

bool EditorViewportPresenter::BeginGameTarget(Renderer &renderer, bool renderHidden) {
    return (ShouldRenderGame() || (renderHidden && gameTarget.IsValid())) &&
           renderer.BeginRenderTarget(gameTarget);
}
void EditorViewportPresenter::EndGameTarget(Renderer &renderer) {
    renderer.EndRenderTarget();
}
void EditorViewportPresenter::Destroy(Renderer &renderer) {
    if (gameTarget.IsValid())
        renderer.DestroyRenderTarget(gameTarget);
    gameTarget = {};
}
