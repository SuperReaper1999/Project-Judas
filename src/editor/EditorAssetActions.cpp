#include "EditorAssetActions.h"
#include "EditorPanels.h"
#include "imgui.h"

bool RequestEditorScriptEdit(const AssetRecord& asset, EditorPanelState& state) {
    if (asset.type != AssetType::Script) return false;
    if (asset.missing || asset.readOnly) {
        state.status = asset.missing ? "Script source is missing. Restore the file or choose another script."
                                     : "This script is read-only. Copy it into your project's assets to edit it.";
        return false;
    }
    state.editScriptAsset = asset.id;
    return true;
}

bool BeginEditorRename(const EditorDocument& doc, EditorPanelState& state) {
    const auto* object = doc.GetScene().Find(doc.Selected());
    if (state.mode != EditorMode::Edit || doc.Selection().size() != 1 || !object) return false;
    state.renamingId = object->id;
    state.renameBuffer = object->name;
    return true;
}

void HandleHierarchyRenameShortcut(const EditorDocument& doc, EditorPanelState& state) {
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::GetIO().WantTextInput &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
        ImGui::IsKeyPressed(ImGuiKey_F2)) {
        BeginEditorRename(doc, state);
    }
}
