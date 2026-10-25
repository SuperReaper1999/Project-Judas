#include "EditorAssetActions.h"
#include "EditorPanels.h"
#include "imgui.h"
#include <cstdio>

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

void DrawEditorImportReview(EditorPanelState& state, EditorRequests& requests) {
    if (state.reviewAssetImport) {
        ImGui::OpenPopup("Import asset");
        state.reviewAssetImport = false;
    }
    ImGui::SetNextWindowSize({520, 0}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Import asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Copy this file into the project and register it as an asset. The original file is kept.");
        ImGui::SeparatorText("Source");
        ImGui::TextWrapped("%s", state.importSourceInput.c_str());
        char destination[512]; std::snprintf(destination, sizeof(destination), "%s", state.importDestinationInput.c_str());
        ImGui::TextUnformatted("Destination inside Assets (optional)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##importDestination", "Leave blank to keep the file name", destination, sizeof(destination)))
            state.importDestinationInput = destination;
        ImGui::TextWrapped("For FBX/glTF models and optimized runtime assets, use Asset Browser > Import & tools.");
        ImGui::BeginDisabled(state.mode != EditorMode::Edit || state.importSourceInput.empty());
        if (ImGui::Button("Import file") && state.mode == EditorMode::Edit && !state.importSourceInput.empty()) {
            requests.importSource = state.importSourceInput; requests.importDestination = state.importDestinationInput;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Choose another...")) { requests.browseImport = true; ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
