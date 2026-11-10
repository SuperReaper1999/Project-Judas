#pragma once
#include "CrashRig.h"
#include <filesystem>
class AssetDatabase;
// Optional development authoring only. Writes an ordinary editable project
// exclusively under the supplied private proof directory; no runtime special case.
void AuthorCrashProject(const std::filesystem::path&,Scene&,const std::vector<CrashRig>&,
                        const std::array<MeshData,4>&,const std::array<MeshData,4>&,AssetDatabase&);
