#pragma once
#include "Scene.h"

struct EditorPanelState;
struct EditorRequests;
class EditorDocument;
class AssetDatabase;
SceneObjectId CreateEditorSpriteFromImage(EditorDocument&,const AssetDatabase&,const std::string& image,
                                         glm::vec3 position,SceneObjectId parent,std::string& error);

// Shared by the main Create menu and hierarchy context menus.
void DrawEditorCreationItems(const EditorPanelState&, EditorRequests&,
                             SceneObjectId parent = kInvalidSceneObjectId);
// Call inside the hierarchy/list window; existing row context menus take priority.
void DrawHierarchyCreationPopup(const EditorPanelState&, EditorRequests&);
