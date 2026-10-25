#pragma once
#include "Scene.h"

struct EditorPanelState;
struct EditorRequests;

// Shared by the main Create menu and hierarchy context menus.
void DrawEditorCreationItems(const EditorPanelState&, EditorRequests&,
                             SceneObjectId parent = kInvalidSceneObjectId);
// Call inside the hierarchy/list window; existing row context menus take priority.
void DrawHierarchyCreationPopup(const EditorPanelState&, EditorRequests&);
