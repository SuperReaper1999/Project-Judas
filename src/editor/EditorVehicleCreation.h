#pragma once
#include "Scene.h"
class EditorDocument;
class Project;
class AssetDatabase;
SceneObjectId CreateEditorVehicle(EditorDocument&,Project&,AssetDatabase&,const std::string& kind,
                                 glm::vec3 position,SceneObjectId parent,std::string& error);
