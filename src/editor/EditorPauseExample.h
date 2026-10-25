#pragma once
#include "Scene.h"
class EditorDocument;
class Project;
class AssetDatabase;
SceneObjectId CreateEditorPauseExample(EditorDocument&,const Project&,AssetDatabase&,std::string& error,
                                     SceneObjectId parent = kInvalidSceneObjectId);
