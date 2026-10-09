#pragma once

#include <vector>

#include "EditorDocument.h"

struct EditorPanelState;

// Edits a detached candidate; its caller publishes through authored undo or the
// runtime setter. Shared controls never mutate an imported material asset.
bool DrawMaterialOverrideControls(MaterialOverride&,const MaterialDefinition&,EditorPanelState&);

// Milestone 30: the inspector's component editor registry.
//
// Adding a component type to the editor means adding ONE entry to the
// table in ComponentEditors.cpp — its name, how to test for / add / remove
// it on a SceneObject, and the function that draws its fields — rather
// than touching a switch in the inspector, the add-component menu and the
// hierarchy indicators separately. The inspector iterates the registry;
// so does "Add component..."; so do the hierarchy's component letters.
//
// This is a static table over the fixed component set in src/Scene.h, not
// a reflection system and not a dynamic component model (see
// docs/ARCHITECTURE.md, "Milestone 30, Deliberately not implemented").
struct ComponentEditor {
    const char* name;      // header/menu label
    char indicator;        // one letter for the hierarchy row
    bool (*has)(const SceneObject&);
    void (*add)(SceneObject&);
    void (*remove)(SceneObject&);
    void (*draw)(EditorDocument& doc, SceneObject& object, EditorPanelState& state);
};

const std::vector<ComponentEditor>& ComponentEditorRegistry();

// The Transform block, drawn before the registered components.
void DrawTransformEditor(EditorDocument& doc, SceneObject& object);

// A short string of indicator letters for the components an object has
// (e.g. "RB" for render + body), in registry order.
std::string ComponentIndicators(const SceneObject& object);

void DrawCategoryLayer(EditorDocument&,const char*,unsigned&,const CategoryRegistry&);
void DrawCategoryMask(EditorDocument&,const char*,CategoryMask&,const CategoryRegistry&,bool allowAll=true);

// Shared inspector Bake command and editor automation entry point.
bool BakeEditorNavigation(EditorDocument&,SceneObjectId,EditorPanelState&);

bool BakeEditorLiquid(EditorDocument&,SceneObjectId,EditorPanelState&);

// Same ordinary create/import command used by the inspector and editor smoke.
// action: 1 sheet, 2 tetrahedral block, 3 indexed cloth import.
bool BakeEditorDeformable(EditorDocument&,SceneObjectId,EditorPanelState&,int action);
