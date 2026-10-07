#pragma once
#include "Project.h"
#include <cstdint>

struct ProjectExportOptions {
    std::string destination; // exact package directory, not its parent
    std::string runtimeExecutable; // empty: sibling Release judas
    std::string engineDataRoot; // empty: resolve installed/development engine data
};
struct ProjectExportResult {
    std::string packageDirectory;
    std::size_t assetCount = 0, sceneCount = 0;
    std::uintmax_t bytes = 0;
    std::uintmax_t assetBytes=0,deduplicatedBytes=0,excludedAssetBytes=0;
    double seconds = 0;
};
// Linux desktop baseline. Copies a verified Release runtime, all registered
// assets by default, or opt-in dependency closure with explicit runtime roots. Transactional
// staging: existing packages are replaced, arbitrary directories are refused.
bool ExportProject(const Project& project, const ProjectExportOptions& options,
                   ProjectExportResult& result, std::string& error);
