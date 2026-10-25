#pragma once
#include "Scene.h"
class EditorDocument;
class Project;
class AssetDatabase;

// Ordinary reusable OBJ and cooked convex assets, not new runtime shape kinds.
SceneObjectId CreateEditorPrimitive(EditorDocument&,const Project&,AssetDatabase&,
    const std::string& kind,glm::vec3 position,SceneObjectId parent,std::string& error);
