#pragma once
#include "AssetDatabase.h"
#include <set>
// Uses normal typed loaders. This is discovery, not a replacement export linker.
bool DirectDocumentAssets(const std::string& kind,const std::string& text,
                          std::set<AssetId>& out,std::string& error);
bool DirectAssetDependencies(const AssetRecord&,std::set<AssetId>& out,std::string& error);
