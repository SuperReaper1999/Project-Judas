#include "EditorShortcuts.h"
#include "EditorAssetActions.h"
#include "EditorCamera.h"
#include "EditorContext.h"
#include "EditorDocument.h"
#include "EditorPanels.h"
#include "GizmoMath.h"
#include "imgui.h"

void RouteEditorShortcuts(EditorContext &context, const EditorCameraGestureState &gesture) {
    const auto &io = ImGui::GetIO();
    // Keyboard shortcuts (edit mode, no text field focused, not flying).
    if (context.panels.mode == EditorMode::Edit && !io.WantTextInput &&
        (!io.WantCaptureKeyboard || context.panels.sceneViewHovered)) {
        const bool ctrl = io.KeyCtrl;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S))
            context.requests.save = true;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z))
            context.requests.undo = true;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y))
            context.requests.redo = true;
        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D))
            context.requests.duplicateId = context.document.Selected();
        if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_F2) && gesture.Active() == EditorCameraGesture::None)
            BeginEditorRename(context.document, context.panels);
        if (ImGui::IsKeyPressed(ImGuiKey_F5))
            context.requests.play = true;
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && context.document.Selected() != kInvalidSceneObjectId) {
            std::string error;
            if (!context.document.DeleteSelection(error))
                context.panels.status = error;
        }
        if (gesture.Active() == EditorCameraGesture::None && !ctrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_F))
                context.requests.focusSelection = true;
            if (ImGui::IsKeyPressed(ImGuiKey_W))
                context.panels.gizmoMode = GizmoMode::Translate;
            if (ImGui::IsKeyPressed(ImGuiKey_E))
                context.panels.gizmoMode = GizmoMode::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_R))
                context.panels.gizmoMode = GizmoMode::Scale;
            if (ImGui::IsKeyPressed(ImGuiKey_X)) {
                context.panels.gizmoSpace =
                    context.panels.gizmoSpace == GizmoSpace::Local ? GizmoSpace::World : GizmoSpace::Local;
            }
        }
    } else if (context.panels.mode == EditorMode::Play && !io.WantTextInput &&
               !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ImGui::IsKeyPressed(ImGuiKey_F5)) {
        context.requests.stop = true;
    }

    if (context.panels.mode == EditorMode::Play && ImGui::IsKeyPressed(ImGuiKey_F8)) {
        context.panels.profilerInspect = !context.panels.profilerInspect;
        if (context.panels.profilerInspect)
            context.panels.showProfiler = true;
    }
}
