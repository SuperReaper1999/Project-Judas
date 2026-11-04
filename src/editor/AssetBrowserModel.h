#pragma once
#include "AssetDatabase.h"
#include <string>
#include <vector>

// Pure browser projection over registered assets. It never moves files,
// assigns IDs or replaces the AssetDatabase's ownership/validation rules.
std::string EditorAssetBrowserPath(const AssetRecord &, const std::string &assetsDirectory);
std::vector<std::string> EditorAssetBrowserDirectories(const AssetDatabase &);
bool EditorAssetBrowserContains(const std::string &path, const std::string &directory, bool recursive);
std::vector<const AssetRecord *> EditorAssetBrowserRecords(const AssetDatabase &,
                                                           const std::string &directory, bool recursive,
                                                           int type = -1, bool missingOnly = false);
