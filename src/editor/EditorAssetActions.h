#pragma once
class EditorDocument;
struct EditorPanelState;
struct AssetRecord;
struct EditorRequests;

// Request source editing through the existing editor-owned source panel.
bool RequestEditorScriptEdit(const AssetRecord&, EditorPanelState&);
bool BeginEditorRename(const EditorDocument&, EditorPanelState&);
// Called inside the hierarchy window so keyboard navigation can capture input.
void HandleHierarchyRenameShortcut(const EditorDocument&, EditorPanelState&);
void DrawEditorImportReview(EditorPanelState&, EditorRequests&);
