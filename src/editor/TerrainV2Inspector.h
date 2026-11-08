#pragma once
#include "ComponentEditors.h"
// Native landscapes require an authored asset/anchor, so creation belongs to
// the terrain workflow rather than a blank, invalid Add Component entry.
ComponentEditor TerrainV2ComponentEditor();
void DrawTerrainV2Navigation(EditorDocument &, SceneObject &, EditorPanelState &);
