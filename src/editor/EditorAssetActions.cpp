#include "EditorAssetActions.h"
#include "EditorPanels.h"
#include "ModelCook.h"
#include "Project.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>

namespace {
namespace fs = std::filesystem;

std::string ImportExtension(const std::string& source) {
    auto extension = fs::u8path(source).extension().u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return extension;
}

bool IsModelSource(const std::string& extension) {
    return extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".obj";
}

bool StartModelImport(EditorPanelState& state, std::string& error) {
    if (!state.project || !state.project->IsLoaded() || !state.importJobs ||
        EditorModelImportPending(state) || state.mode != EditorMode::Edit) {
        error = "Open a project in Edit mode and wait for any current model import to finish.";
        return false;
    }
    const auto relative = fs::u8path(state.modelImportReviewOutput);
    if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
        relative.extension() != ".judasmodel") {
        error = "Enter a .judasmodel destination relative to Assets.";
        return false;
    }
    std::error_code ec;
    const auto assets = fs::weakly_canonical(fs::u8path(state.project->AssetsDir()), ec);
    if (ec) { error = ec.message(); return false; }
    const auto destination = fs::weakly_canonical(assets / relative, ec);
    if (ec) { error = ec.message(); return false; }
    const auto assetRelative = destination.lexically_relative(assets);
    if (assetRelative.empty() || assetRelative.is_absolute() || *assetRelative.begin() == "..") {
        error = "The cooked model must stay inside this project's Assets folder.";
        return false;
    }
    const auto output = (fs::u8path(state.project->Settings().assetsDir) / assetRelative).generic_u8string();
    const auto recipePath = fs::u8path(state.project->RootDir()) / "Imports" /
                            (destination.stem().u8string() + ".judasimport");
    if (fs::exists(recipePath, ec)) {
        error = "That model name already has an import recipe. Choose a new name, or reimport the existing recipe under Import & tools.";
        return false;
    }
    if (ec) { error = ec.message(); return false; }
    std::string recipe;
    if (!CreateModelRecipe(state.project->RootDir(), state.importSourceInput, output, recipe, error))
        return false;

    state.modelSource = state.importSourceInput;
    state.modelOutput = output;
    state.modelRecipe = std::move(recipe);
    state.showAssetBrowser = true;
    state.selectModelImportTools = true;
    QueueEditorModelCook(state);
    return true;
}
} // namespace

bool EditorModelImportPending(const EditorPanelState& state) {
    return state.importTask && (!state.importPublished || state.importTask->job.IsValid());
}

bool QueueEditorModelCook(EditorPanelState& state) {
    if (state.mode != EditorMode::Edit || !state.importJobs || state.modelRecipe.empty() ||
        EditorModelImportPending(state))
        return false;
    auto task = QueueModelImport(*state.importJobs, state.modelRecipe);
    if (!task->job.IsValid()) {
        state.status = "Import worker unavailable. The recipe is saved. Use Import / reimport / retry in Import & tools.";
        return false;
    }
    state.importTask = std::move(task);
    state.importPublished = false;
    state.importScaleConfirmed.reset();
    state.importScalePrompted.reset();
    state.status = "Cooking model. Preview and placement will be available under Import & tools.";
    return true;
}

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
        state.modelImportReviewError.clear();
        if (IsModelSource(ImportExtension(state.importSourceInput))) {
            auto name = fs::u8path(state.importSourceInput).stem();
            name += ".judasmodel";
            state.modelImportReviewOutput = (fs::path("models") / name).generic_u8string();
        }
        ImGui::OpenPopup("Import asset");
        state.reviewAssetImport = false;
    }
    ImGui::SetNextWindowSize({520, 0}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Import asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto extension = ImportExtension(state.importSourceInput);
        const bool model = IsModelSource(extension);
        bool imported = false;
        ImGui::SeparatorText("Source");
        ImGui::TextWrapped("%s", state.importSourceInput.c_str());
        const bool editing = state.mode == EditorMode::Edit && !state.importSourceInput.empty();
        if (model) {
            ImGui::TextWrapped("Create a game-ready model and keep the original source. "
                               "Preview, place or adjust the saved recipe under Import & tools.");
            char output[512];
            std::snprintf(output, sizeof(output), "%s", state.modelImportReviewOutput.c_str());
            ImGui::TextUnformatted("Cooked model inside Assets");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##modelOutput", output, sizeof(output))) {
                state.modelImportReviewOutput = output;
                state.modelImportReviewError.clear();
            }
            const bool pending = EditorModelImportPending(state);
            if (pending) ImGui::TextWrapped("A model import is still finishing. Check Import & tools for progress.");
            ImGui::BeginDisabled(!editing || pending || !state.importJobs || !state.project);
            if (ImGui::Button("Cook & import model")) imported = StartModelImport(state, state.modelImportReviewError);
            ImGui::EndDisabled();
            if (!state.modelImportReviewError.empty())
                ImGui::TextWrapped("%s", state.modelImportReviewError.c_str());
        }
        AssetType type;
        if (!model || (!imported && AssetTypeForExtension(extension, type))) {
            const bool showDirect = !model || ImGui::TreeNode("Register source only");
            if (showDirect) {
                ImGui::TextWrapped("Copy and register the selected file as-is. The original is kept.");
                if (model)
                    ImGui::TextWrapped("Source-only skips model unit normalization and scale review. "
                                       "Use Cook & import model to inspect metre dimensions and possible 100x mismatches.");
                char destination[512];
                std::snprintf(destination, sizeof(destination), "%s", state.importDestinationInput.c_str());
                ImGui::TextUnformatted("Destination inside Assets (optional)");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputTextWithHint("##importDestination", "Leave blank to keep the file name", destination, sizeof(destination)))
                    state.importDestinationInput = destination;
                ImGui::BeginDisabled(!editing);
                if (ImGui::Button("Import file") && editing) {
                    requests.importSource = state.importSourceInput;
                    requests.importDestination = state.importDestinationInput;
                    imported = true;
                }
                ImGui::EndDisabled();
                if (model) ImGui::TreePop();
            }
        }
        if (imported) ImGui::CloseCurrentPopup();
        ImGui::Separator();
        if (ImGui::Button("Choose another...")) { requests.browseImport = true; ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
