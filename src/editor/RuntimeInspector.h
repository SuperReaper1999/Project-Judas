#pragma once
#include "Scene.h"
class RuntimeWorld;
struct EditorPanelState;
struct EditorRequests;

// Optional editor-only observations: no gameplay state, demand or VM serialization.
void DrawRuntimeEntity(RuntimeWorld&, SceneObjectId,EditorPanelState* panels=nullptr);
void DrawRuntimeInspector(EditorPanelState&, EditorRequests&);
