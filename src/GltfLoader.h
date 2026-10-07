#pragma once
#include "MeshData.h"
#include <string>
#include <map>
bool ParseGltfMesh(const void* bytes,size_t size,MeshData& mesh,std::string& error);

// External resources stay in the source directory or use explicitly approved remaps.
bool ParseGltfMeshSource(const void*,size_t,const std::string& sourcePath,MeshData&,std::string& error,std::vector<std::string>* dependencies=nullptr,bool animationOnly=false,const std::map<std::string,std::string>* remaps=nullptr);

bool GatherGltfDependencies(const std::string& source,std::vector<std::string>&,std::string& error,const std::map<std::string,std::string>* remaps=nullptr);
