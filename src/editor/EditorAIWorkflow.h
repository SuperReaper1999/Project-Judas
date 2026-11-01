#pragma once
#include "AssetDatabase.h"
#include "Scene.h"
class EditorDocument;
class Project;
struct EditorPanelState;

struct EditorAIBehaviourAssets {
    AssetRecord definition, behaviour;
    SceneObjectId patrolPoint=0, returnPoint=0, coverPoint=0;
};
// Creates ordinary assets and one undoable scene edit. On failure, only files
// exclusively created by this invocation are removed. Existing assets survive.
bool CreateAndAttachEditorAI(EditorDocument&, SceneObjectId, const Project&, AssetDatabase&,
                            const std::string& name, EditorAIBehaviourAssets&, std::string& error);
// Resolve explicit metadata from a behaviour without evaluating any project code.
// False with an empty error means this is not a linked AI behaviour.
bool FindEditorAIDefinition(const AssetDatabase&, const AssetRecord& behaviour,
                            AssetRecord& definition, std::string& error);
void DrawEditorAIWorkflow(EditorDocument&, SceneObject&, EditorPanelState&);
void DrawSelectedAIShortcuts(const SceneObject&, EditorPanelState&);
