#pragma once
#include "ModStore.h"
class Project;
namespace JudasMods {
struct Session {
 Snapshot snapshot;
 AssetDatabase assets;
 std::map<std::string,std::string> sceneFiles;
};
// Returns without creating a store/scanning Mods when the game is not opted in,
// or --no-mods is selected. All selected policy/reference checks precede the VM.
// Shared eager validation for runtime, install and kit-bound publication.
bool ComposeContent(const Project&,const Contract&,const Snapshot&,const AssetDatabase&,
 const std::map<std::string,std::string>& baseScenes,Session&,std::string&);
bool InstallForGame(const Project&,const Package&,std::string&);
bool PrepareSession(Project&,bool noMods,std::string& error);
// Assigns identities before cooking, with no change for an ordinary project.
bool AssignAssetIdentity(const std::string& projectRoot,AssetId&,std::string&);
std::string RegistryIdentity(const Project&);
}
