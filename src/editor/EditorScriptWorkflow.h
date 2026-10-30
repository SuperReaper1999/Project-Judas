#pragma once
#include "Scene.h"
#include "AssetDatabase.h"
class EditorDocument;
class Project;
struct EditorPanelState;

bool AttachEditorScript(EditorDocument&, SceneObjectId, const AssetDatabase&, const AssetId&, std::string& error,
                        const std::string& properties = "{}");
bool CreateAndAttachEditorScript(EditorDocument&, SceneObjectId, const Project&, AssetDatabase&,
                                const std::string& fileName, AssetRecord&, std::string& error,
                                const std::string& sourceTemplate = {}, const std::string& properties = "{}");
void DrawEditorScriptWorkflow(EditorDocument&, SceneObject&, EditorPanelState&);
SceneObjectId CreateEditorFirstPersonPlayer(EditorDocument&, Project&, AssetDatabase&,
                                          glm::vec3 position, SceneObjectId parent, std::string& error,
                                          bool thirdPerson = false);
SceneObjectId CreateEditorPlanarPlayer(EditorDocument&,Project&,AssetDatabase&,glm::vec3,
                                      SceneObjectId,std::string& error,bool sideView,bool grid=false);
