#pragma once
#include "modding/ModRuntime.h"
#include "TerrainAuthoring.h"
#include "Project.h"
namespace JudasMods {
struct KitBinding {std::string kit,identity,mod;Contract contract;JSON identities=JSON::object();std::vector<std::string> dependencies;};
bool ExportKit(const Project&,const std::string& destination,const std::string& documentationRoot,std::string&);
bool NewModProject(const std::string& kit,const std::string& destination,const std::string& mod,Project&,std::string&);
bool ReadKitBinding(const Project&,KitBinding&,std::string&);
bool BindDependency(const Project&,const std::string& packagePath,std::string&);
bool OpenAuthorAssets(const Project&,AssetDatabase&,std::string&);
AssetId AuthorIdentity(const Project&,const AssetId& local);
bool QualifyTerrain(const Project&,TerrainSource&,std::string&);
bool AuthorPackage(const Project&,const Manifest& draft,Package&,std::string&);
bool ValidateAuthorManifest(const Project&,const Manifest&,std::string&);
}
