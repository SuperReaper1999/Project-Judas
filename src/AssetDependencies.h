#pragma once
#include "AssetDatabase.h"
#include "Scene.h"
#include <set>
// Shared typed references used by authoring and export. Script strings are not
// linked or guessed; projects explicitly declare runtime roots for dynamic IDs.
void CollectSceneAssetReferences(const Scene&,std::set<AssetId>&);
bool CollectAssetDependencies(const AssetRecord&,std::set<AssetId>&,
                              std::set<std::string>& auxiliaryFiles,std::string& error);
