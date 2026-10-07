#pragma once
#include "MeshData.h"
#include "SkeletalAnimation.h"
#include <functional>
#include <map>
#include <string>
struct ModelImportSettings {
 double sourceUnitMeters=0; // 0: source metadata (glTF assumes metres)
 double sampleRate=60;
 glm::quat basisRotation{1,0,0,0}; // explicit rotation after source-metadata normalization
 bool allowBaseMesh=false; // explicit lossy selection for unsupported deformers
 std::map<std::string,std::string> dependencyRemaps;
 std::map<std::string,std::string> jointRemaps;
 std::function<bool()> cancelled;
};
struct ModelImportDiagnostic {std::string severity,code,source,node,action,message;int face=-1,vertex=-1;glm::vec3 point{0};};
struct ModelImportReport {
 size_t sourceBones=0,hierarchyNodes=0,skinJoints=0,parts=0,vertices=0;
 double sourceUnitMeters=1;
 std::vector<std::string> dependencies,uvSets;
 std::vector<ModelImportDiagnostic> diagnostics;
};
// Authoring service only. CPU work, approved source-relative dependencies.
bool ImportModelSource(const std::string&,const ModelImportSettings&,MeshData&,ModelImportReport&,std::string& error);
bool ImportCompatibleMotion(const std::string&,const ModelImportSettings&,const Skeleton&,std::vector<AnimationClip>&,ModelImportReport&,std::string& error);

bool GatherModelDependencies(const std::string&,std::vector<std::string>&,std::string&,const ModelImportSettings* settings=nullptr);
