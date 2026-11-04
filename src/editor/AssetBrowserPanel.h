#pragma once
#include <string>
struct AssetRecord;
class AssetDatabase;
class EditorDocument;
struct EditorPanelState;
struct EditorRequests;

// Browser-owned navigation state. Stable asset selection is shared with other
// authoring tools; folder/view choices do not belong to the project or scene.
struct EditorAssetBrowserState {
    std::string directory;
    std::string projectRoot;
    bool grid = false;
    bool recursive = false;
};
void DrawAssetList(EditorDocument &, EditorPanelState &, EditorRequests &, const AssetDatabase &);
void DrawAssetDetails(EditorDocument &, EditorPanelState &, EditorRequests &, const AssetDatabase &);
void DrawAssetImportTools(EditorDocument &, EditorPanelState &, EditorRequests &, const AssetDatabase &,
                          bool revealModelTools = false);
void DrawUIAssetDocument(const AssetRecord &, EditorPanelState &);
