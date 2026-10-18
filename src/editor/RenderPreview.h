#pragma once

#include "Scene.h"

class EditorDocument;
struct EditorPanelState;

// Runtime-only inspection preview. Authored scene state is never modified.
void DrawRuntimeAppearance(EditorDocument &doc, EditorPanelState &state);
void DrawRuntimeRenderPreview(EditorDocument &doc, SceneObjectId id, EditorPanelState &state);
