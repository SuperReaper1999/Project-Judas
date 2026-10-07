#pragma once
#include "WorldBuilder.h"
#include "AuthoringDocument.h"
#include "Project.h"
struct RecipePreviewTask {
 ~RecipePreviewTask();
 std::map<std::string,std::string> sourceHashes;JobHandle job;RecipeProduct product;uint64_t generation=0;std::string project,parameters,error,stage,meshBytes,proxyBytes,collisionBytes;};
std::shared_ptr<RecipePreviewTask> PreviewRecipe(JobSystem&,const WorldRecipe&,const Scene&,const AssetDatabase&,uint64_t generation,const std::string& project);
bool PublishRecipeResources(const RecipePreviewTask&,const Project&,AssetDatabase&,std::string& error);
