#pragma once

#include <vector>

#include "EditorDocument.h"

struct EditorPanelState;

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
