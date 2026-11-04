#include "AssetBrowserPanel.h"
#include "EditorPanels.h"
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
        ImGui::TextDisabled("No project open. File > New project / Open project.");
        ImGui::End();
        return;
    }
    const AssetDatabase &db = *state.assets;
    if (ImGui::BeginTabBar("##assetBrowserTabs")) {
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
