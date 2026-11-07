#pragma once
#include "TerrainSource.h"
namespace TerrainV2 {
// Header/journal only. These functions intentionally leave sample arrays empty;
// extraction still requires a fully validated Source assembled from asset pages.
bool ValidateMetadata(const Source &, std::string &);
std::string SerializeMetadata(const Source &);
bool ParseMetadata(const std::string &, Source &, std::string &);
} // namespace TerrainV2
