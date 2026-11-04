#pragma once
struct EditorPanelState;
struct EditorRequests;

// Safe authoring-frame tick, independent of panel visibility. Workers prepare;
// only this workflow publishes accepted generations after project/mode checks.
void PumpEditorAssetWorkflow(EditorPanelState &, EditorRequests &);
