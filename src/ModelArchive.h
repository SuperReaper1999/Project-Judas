#pragma once
#include "MeshData.h"
#include <string>
#include <vector>
// Versioned normalized runtime payload. No FBX parser or source files at runtime.
bool EncodeModelArchive(const MeshData&,std::vector<uint8_t>&,std::string& error);
bool DecodeModelArchive(const void*,size_t,MeshData&,std::string& error);

bool VerifyImportedModelFresh(const std::string& projectRoot,const std::string& modelPath,std::string& error);
