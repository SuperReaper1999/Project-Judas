#pragma once
#include "AssetDatabase.h"
#include "../AuthoringJSON.h"
#include <memory>
#include <array>
#include <set>

// Policy is authored by the game, never supplied by a mod. JSON is retained only
// for detached descriptors and explicitly declared property schemas, not runtime
// pointers or an alternate scene representation.
namespace JudasMods {
using JSON=AuthoringJSON::J;
struct Limits {
 static constexpr size_t Metadata=1024*1024,File=8*1024*1024,Package=64*1024*1024;
 static constexpr size_t Files=4096,Mods=16,Edges=128,Path=512,Storage=512*1024*1024;
};
struct Reference {std::string kind,value;}; // asset type + ID, or scene + portable path
struct Replacement {Reference target;std::string revision;JSON interface=JSON::object();};
struct Extension {std::set<std::string> kinds;size_t maximum=64;};
struct Contract {
 unsigned format=1;bool enabled=false,executable=false;
 std::string game,gameVersion,version,registries;
 std::set<std::string> additions;
 std::map<std::string,Reference> published;
 std::map<AssetId,JSON> behaviors; // published base property schemas, no module evaluation
 std::map<std::string,Replacement> replacements;
 std::map<std::string,Extension> extensions;
 std::vector<std::string> editableSources;
};
struct Inventory {std::string path,hash;uint64_t size=0;};
struct Asset {std::string local,id,type,path;};
struct Dependency {std::string id,version;};
struct Import {std::string owner,name;Reference reference;};
struct Contribution {std::string point,name;Reference reference;JSON data=JSON::object();};
struct RequestReplacement {std::string target,revision;Reference provider;};
struct Manifest {
 unsigned format=1;std::string id,name,version,game,gameVersion,contractVersion,author,license;
 bool executable=false;
 std::vector<std::string> notices;
 std::vector<Inventory> files;
 std::vector<Asset> assets;
 std::vector<std::string> scenes;
 std::vector<Dependency> dependencies;
 std::vector<Import> imports;
 std::map<std::string,Reference> exports;
 std::vector<Contribution> contributions;
 std::vector<RequestReplacement> replacements;
};
struct Package {Manifest manifest;std::map<std::string,std::string> files;std::string hash;};
struct Selection {std::shared_ptr<const Package> package;std::string trustedHash;};
struct Resolution {std::vector<Selection> ordered;std::string signature;JSON catalog=JSON::object();};
bool ValidIdentity(const std::string&);
bool ParseVersion(const std::string&,std::array<unsigned,3>&);
bool VersionMatches(const std::string& actual,const std::string& required);
// Valid UTF-8 paths; canonical NFC + Unicode casefold used to reject aliases.
bool PortablePath(const std::string&,std::string& collisionKey,std::string& error);
std::string QualifiedAsset(const std::string& game,const std::string& mod,const std::string& local);
JSON ToJSON(const Contract&);JSON ToJSON(const Manifest&);
bool ParseContract(const std::string&,Contract&,std::string&);
bool ParseManifest(const std::string&,Manifest&,std::string&,bool authorDraft=false);
bool EncodePackage(const Package&,std::string&,std::string&);
bool DecodePackage(const std::string&,Package&,std::string&);
bool ReadPackage(const std::string&,Package&,std::string&);
bool WritePackage(const std::string&,const Package&,std::string&);
// Whole selected set is validated, including declared imports and trust. No VM
// is constructed, no module is evaluated and no partially valid set is returned.
bool Resolve(const Contract&,const std::vector<Selection>&,Resolution&,std::string&);
bool AssetKind(const std::string&,AssetType&);
bool ValidateReferences(const Contract&,const Resolution&,const AssetDatabase& base,
                        const std::map<std::string,std::string>& physicalFiles,std::string&);
}
