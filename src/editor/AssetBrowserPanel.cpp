#include "AssetBrowserPanel.h"
#include "EditorPanels.h"
#include "EditorVisuals.h"
#include "EditorWorkspace.h"
#include "Project.h"
#include "imgui.h"

void DrawAssetBrowserPanel(EditorDocument &doc, EditorPanelState &state, EditorRequests &requests) {
    if (state.dockingEnabled ? !state.showAssetBrowser : !state.workspace.showAssets)
        return;
    if (!BeginEditorPanel("Asset Browser", &state.showAssetBrowser, state, &state.workspace.assets)) {
        ImGui::End();
        return;
    }
    const bool hasProject = state.project && state.project->IsLoaded() && state.assets;
    if (!hasProject) {
        ImGui::Spacing();
        EditorVisuals::IconText(EditorVisuals::Icon::Folder, "Your project's content", 22);
        ImGui::TextWrapped("Open or create a project to import models, images, scripts and audio. "
                           "Assets stay with your project and can be dragged into the scene.");
        ImGui::Spacing();
        if (ImGui::Button("Create project..."))
            requests.newProject = true;
        ImGui::SameLine();
        if (ImGui::Button("Open project..."))
            requests.openProject = true;
        ImGui::End();
        return;
    }
    const AssetDatabase &db = *state.assets;
    if (ImGui::BeginTabBar("##assetBrowserTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        if (ImGui::BeginTabItem("Assets")) {
            DrawAssetList(doc, state, requests, db);
            ImGui::EndTabItem();
        }
        const bool inspectRequested = state.selectAssetDetails;
        state.selectAssetDetails = false;
        if (ImGui::BeginTabItem("Selected asset", nullptr,
                                inspectRequested ? ImGuiTabItemFlags_SetSelected : 0)) {
            DrawAssetDetails(doc, state, requests, db);
            ImGui::EndTabItem();
        }
        const bool openModelTools = state.selectModelImportTools;
        if (ImGui::BeginTabItem("Import & tools", nullptr,
                                openModelTools ? ImGuiTabItemFlags_SetSelected : 0)) {
            state.selectModelImportTools = false;
            DrawAssetImportTools(doc, state, requests, db, openModelTools);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
