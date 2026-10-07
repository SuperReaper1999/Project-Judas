#pragma once
#include "Scene.h"
#include "Prefab.h"
#include "MeshData.h"
#include "JobSystem.h"
struct RecipeElement {std::string key;SceneObjectId object=0;PrefabProperties generated;};
struct WorldRecipe {
 std::string id="recipe",type="linear",prefabAsset,renderAsset,collisionAsset,proxyAsset,geometryRevision;
 SceneObjectId templateId=0;SceneTransform frame;
 unsigned count=8,segments=24,collisionSegments=0;float spacing=2,radius=4,startDegrees=0,endDegrees=90,twistDegrees=0;
 bool smooth=false,closed=true;
 std::vector<glm::vec3> path{{0,0,0},{0,0,-10}};
 std::vector<glm::vec2> profile{{-2,-.15f},{2,-.15f},{2,.15f},{-2,.15f}};
 std::vector<RecipeElement> elements;
};
struct RecipeProduct {Scene scene;WorldRecipe recipe;MeshData mesh,collisionMesh;uint64_t documentGeneration=0;std::string sourceRevision;std::vector<SceneObjectId> removed;};
bool ParseWorldRecipe(const std::string&,WorldRecipe&,std::string&);
std::string SerializeWorldRecipe(const WorldRecipe&);
// Pure CPU worker operation. Product publication is owner-thread and revision checked.
bool EvaluateWorldRecipe(const WorldRecipe&,const Scene&,const AssetDatabase*,RecipeProduct&,std::string&,const JobContext* context=nullptr);
bool BakeRecipeGeometry(const RecipeProduct&,const std::string& meshPath,const std::string& collisionPath,std::string&);
