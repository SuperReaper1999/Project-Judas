#include "ModAuthoring.h"
#include "PlatformServices.h"
#include "AuthoringDocument.h"
#include "AssetDependencies.h"
#include "SceneSerialization.h"
#include "ProjectExporter.h"
#include "ModelArchive.h"
#include "ScriptSystem.h"
#include "SceneFingerprint.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
namespace fs=std::filesystem;
namespace JudasMods {
namespace {
using AuthoringJSON::require;using AuthoringJSON::keys;
std::string Read(const fs::path& p,size_t limit=Limits::Metadata){std::string bytes,error;require(ReadRegularContentFile(p.u8string(),bytes,limit,error),error);return bytes;}
void Write(const fs::path& p,const std::string& bytes){fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary|std::ios::trunc);out.write(bytes.data(),std::streamsize(bytes.size()));out.close();require(bool(out),"cannot write kit/mod source: "+p.u8string());}
JSON Kit(const fs::path& root){
 auto j=AuthoringJSON::parse(Read(root/"kit.json"));
 keys(j,{"format","tool","contract","identity","project","assets","scenes","sources","inclusions","sdk"},"kit");
 require(j.at("format")==1&&j.at("tool")=="Judas-M76-1","kit/tool format mismatch");
 require(j.at("identity")==SceneFingerprintSha256(j.at("contract").dump()+":"+j.at("project").get<std::string>()),"kit contract/registry identity mismatch");
 std::map<std::string,std::string> verified;
 for(auto name:{"assets","scenes","sources"}){
  require(j.at(name).is_array()&&j.at(name).size()<=Limits::Files,"kit index count bound");
  for(const auto& entry:j.at(name)){
   auto path=entry.at("path").get<std::string>();std::string key,error;
   require(PortablePath(path,key,error),error);
   auto hash=SceneFingerprintSha256(Read(root/fs::u8path(path),Limits::File));
   require(entry.at("sha256")==hash,"published kit input changed: "+path);
   auto prior=verified.emplace(key,hash);require(prior.second||prior.first->second==hash,"kit index alias collision");
  }
 }
 return j;
}
// A curated kit is bounded and copied as regular files, never by following a
// directory iterator's link targets. This also protects the offline docs copy.
void CopyKit(const fs::path& from,const fs::path& to){
 size_t count=0,total=0;std::set<std::string> names;
 for(const auto& entry:fs::recursive_directory_iterator(from)){
  auto status=entry.symlink_status();require(status.type()==fs::file_type::directory||status.type()==fs::file_type::regular,"kit contains link/nonregular entry");
  auto relative=entry.path().lexically_relative(from).generic_u8string();std::string key,error;
  require(PortablePath(relative,key,error)&&names.insert(key).second,error.empty()?"kit path alias":error);
  if(status.type()==fs::file_type::directory)continue;
  require(++count<=Limits::Files*2,"kit file count exceeds 8192");auto bytes=Read(entry.path(),Limits::File);
  require(total<=Limits::Storage-bytes.size(),"kit copy exceeds 512 MiB");total+=bytes.size();Write(to/fs::u8path(relative),bytes);
 }
}

std::string HashFile(const fs::path& p){std::string hash,error;require(SceneFingerprintSha256File(p.u8string(),hash,error),error);return hash;}
void Portable(const std::string& p){std::string key,error;require(PortablePath(p,key,error),error);}
}
bool ExportKit(const Project& project,const std::string& destination,const std::string& docs,std::string& error){std::string staging;try{
 Contract contract;require(ParseContract(project.Settings().modContract,contract,error)&&contract.enabled,error.empty()?"project has not opted into modding":error);require(contract.registries==RegistryIdentity(project),"contract registries differ from game");AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());require(assets.Problems().empty(),"inconsistent base assets");std::set<AssetId> published;std::set<std::string> scenes;for(auto& [_,r]:contract.published){if(r.kind=="scene")scenes.insert(r.value);else published.insert(r.value);}for(auto& [_,r]:contract.replacements){if(r.target.kind=="scene")scenes.insert(r.target.value);else published.insert(r.target.value);}
 // A curated kit must not silently reveal private dependencies. The game author
 // explicitly publishes the needed dependency closure in its contract.
 std::set<AssetId> required=published;for(auto& id:published){auto* a=assets.Find(id);require(a&&!a->missing,"missing published asset: "+id);std::set<AssetId> refs;std::set<std::string> extra;require(CollectAssetDependencies(*a,refs,extra,error),error);require(extra.empty(),"publish cooked models, not private external raw dependencies");for(auto& dep:refs)require(published.count(dep),"published asset needs an unpublished dependency: "+id+" -> "+dep);}
 for(auto& path:scenes){Portable(path);Scene scene;require(LoadSceneFromFile(project.Resolve(path),scene,error),error);std::set<AssetId> refs;CollectSceneAssetReferences(scene,refs);for(auto& dep:refs)require(published.count(dep),"published scene needs an unpublished dependency: "+path+" -> "+dep);}
 auto target=fs::absolute(fs::u8path(destination));require(!fs::exists(target),"kit destination exists; choose a new versioned kit directory");fs::create_directories(target.parent_path());staging=CreateExportStagingDirectory(target.parent_path());auto root=fs::u8path(staging);JSON as=JSON::array(),ss=JSON::array(),sources=JSON::array(),inclusions=JSON::array();
 auto copy=[&](const std::string& path,const std::string& reason){Portable(path);auto bytes=Read(fs::u8path(project.Resolve(path)),Limits::File);Write(root/fs::u8path(path),bytes);inclusions.push_back({{"path",path},{"reason",reason},{"sha256",SceneFingerprintSha256(bytes)}});};
 for(auto& id:required){const auto& record=*assets.Find(id);copy(record.relativePath,"explicit published base reference/replacement target");std::string e;require(AssetDatabase::WriteMeta((root/fs::u8path(record.relativePath)).u8string()+kAssetMetaExtension,id,record.type,"published-kit-reference",e),e);inclusions.push_back({{"path",record.relativePath+kAssetMetaExtension},{"reason","generated published identity/type metadata"},{"sha256",HashFile(root/fs::u8path(record.relativePath+kAssetMetaExtension))}});as.push_back({{"id",id},{"type",AssetTypeName(record.type)},{"path",record.relativePath},{"sha256",HashFile(root/fs::u8path(record.relativePath))}});}
 for(auto& path:scenes){copy(path,"explicit published map/replacement authoring reference");ss.push_back({{"path",path},{"sha256",HashFile(root/fs::u8path(path))}});}
 for(auto& path:contract.editableSources){copy(path,"explicit permitted editable source; fork before changing");sources.push_back({{"path",path},{"sha256",HashFile(root/fs::u8path(path))}});}
 // Offline docs are developer tooling, not redistributed private game sources.
 auto documentation=[&](const fs::path& source,const fs::path& relative){auto bytes=Read(source,Limits::File);Write(root/"Documentation"/relative,bytes);inclusions.push_back({{"path",(fs::path("Documentation")/relative).generic_u8string()},{"reason","matching offline author reference/tooling"},{"sha256",SceneFingerprintSha256(bytes)}});};
 for(auto& path:{"JUDASJS.md","judas.d.ts","MODDING.md","M76.md","judasmod.example.json"})documentation(fs::u8path(docs)/path,path);
 for(auto& entry:fs::recursive_directory_iterator(fs::u8path(docs)/"judasjs"))if(entry.is_regular_file()&&(entry.path().extension()==".md"||entry.path().extension()==".js"))documentation(entry.path(),fs::path("judasjs")/entry.path().lexically_relative(fs::u8path(docs)/"judasjs"));
 auto settings=project.Settings();settings.modContract.clear();settings.worldManifest.clear();settings.saveIdentity.clear();settings.runtimeAssets.clear();settings.exportScenes.clear();settings.excludeScenes.clear();settings.startupScene="";auto bytes=Project::SerializeToString(settings);JSON kit={{"format",1},{"tool","Judas-M76-1"},{"sdk","Matching Judas M76 Release SDK; no C++ compilation required"},{"contract",ToJSON(contract)},{"project",bytes},{"identity",SceneFingerprintSha256(ToJSON(contract).dump()+":"+bytes)},{"assets",as},{"scenes",ss},{"sources",sources},{"inclusions",inclusions}};Write(root/"kit.json",kit.dump(2)+"\n");Write(root/"README.txt","Curated Judas M76 kit. Base references are read-only. Create a mod project with judas_mod new; fork published terrain before sculpting. Runtime packages contain cooked content, not these authoring sources. See Documentation/JUDASJS.md and the matching SDK modding guide.\n");fs::rename(root,target);staging.clear();error.clear();return true;
 }catch(const std::exception& e){if(!staging.empty()){std::error_code ignored;fs::remove_all(fs::u8path(staging),ignored);}error="kit export: "+std::string(e.what());return false;}}
bool NewModProject(const std::string& kitPath,const std::string& destination,const std::string& mod,Project& out,std::string& error){std::string staging;try{
 require(ValidIdentity(mod)&&mod!="base"&&mod!="judas","invalid/reserved mod identity");auto kitRoot=fs::absolute(fs::u8path(kitPath));auto kit=Kit(kitRoot);Contract contract;require(ParseContract(kit.at("contract").dump(),contract,error),error);require(!fs::exists(fs::u8path(destination)),"mod project destination already exists");auto target=fs::absolute(fs::u8path(destination));fs::create_directories(target.parent_path());staging=CreateExportStagingDirectory(target.parent_path());fs::remove(fs::u8path(staging));Project project;require(Project::CreateNew(staging,mod,project,error),error);ProjectSettings registry;require(Project::ParseFromString(kit.at("project"),registry,error),error);auto identity=project.Settings().saveIdentity;project.Settings()=registry;project.Settings().name=mod;project.Settings().saveIdentity=identity;project.Settings().assetsDir="Assets";project.Settings().scenesDir="Scenes";project.Settings().savesDir="Saves";project.Settings().startupScene="Scenes/main.judas";require(project.Save(error),error);
 // Copy only the curated kit so moving the authoring project remains offline.
 CopyKit(kitRoot,fs::u8path(project.RootDir())/"Kit");Write(fs::u8path(project.RootDir())/"mod-authoring.json",JSON({{"format",1},{"kit","Kit"},{"identity",kit.at("identity")},{"mod",mod},{"identities",JSON::object()}}).dump(2)+"\n");Manifest m;m.id=m.name=mod;m.game=contract.game;m.gameVersion=contract.gameVersion;m.contractVersion=contract.version;m.version="1.0.0";m.author="Mod author";m.license="Specify the license for your content";Write(fs::u8path(project.RootDir())/"mod.json",ToJSON(m).dump(2)+"\n");Scene scene;require(SaveSceneToFile(scene,project.StartupScenePath(),error),error);auto filename=fs::u8path(project.ProjectFile()).filename();fs::rename(fs::u8path(staging),target);staging.clear();require(out.Load((target/filename).u8string(),error),error);error.clear();return true;
 }catch(const std::exception& e){if(!staging.empty()){std::error_code ignored;fs::remove_all(fs::u8path(staging),ignored);}error="new mod project: "+std::string(e.what());return false;}}
bool ReadKitBinding(const Project& p,KitBinding& out,std::string& error){try{auto j=AuthoringJSON::parse(Read(fs::u8path(p.RootDir())/"mod-authoring.json"));keys(j,{"format","kit","identity","mod","identities","dependencies"},"mod authoring binding");require(j.at("format")==1,"unsupported mod authoring binding");KitBinding b;b.kit=j.at("kit");Portable(b.kit);auto kit=Kit(fs::u8path(p.Resolve(b.kit)));b.identity=j.at("identity");b.mod=j.at("mod");b.identities=j.at("identities");if(j.contains("dependencies")){b.dependencies=j.at("dependencies").get<std::vector<std::string>>();require(b.dependencies.size()<=Limits::Mods,"author dependency count");for(const auto& path:b.dependencies)Portable(path);}require(ValidIdentity(b.mod)&&kit.at("identity")==b.identity&&b.identities.is_object()&&b.identities.size()<=Limits::Files,"kit binding identity/count");require(ParseContract(kit.at("contract").dump(),b.contract,error),error);require(b.contract.registries==RegistryIdentity(p),"mod registry changes are unsupported; restore kit registries");for(auto i=b.identities.begin();i!=b.identities.end();++i)require(IsValidAssetId(i.key())&&i.key()==QualifiedAsset(b.contract.game,b.mod,i.value()),"invalid authoring identity ledger");out=std::move(b);error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
AssetId AuthorIdentity(const Project& p,const AssetId& local){auto id=local;std::string error;require(AssignAssetIdentity(p.RootDir(),id,error),error);return id;}
bool QualifyTerrain(const Project& p,TerrainSource& t,std::string& error){if(!fs::exists(fs::u8path(p.RootDir())/"mod-authoring.json"))return true;try{t.identity=AuthorIdentity(p,t.identity);t.meshId=AuthorIdentity(p,t.meshId);t.textureId=AuthorIdentity(p,t.textureId);t.collisionId=AuthorIdentity(p,t.collisionId);t.productDirectory=(fs::u8path(p.Settings().assetsDir)/"Terrain"/t.identity).generic_u8string();error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
bool BindDependency(const Project& project,const std::string& packagePath,std::string& error){try{
 KitBinding binding;require(ReadKitBinding(project,binding,error),error);Package package;
 require(ReadPackage(packagePath,package,error),error);
 require(package.manifest.game==binding.contract.game&&VersionMatches(binding.contract.gameVersion,package.manifest.gameVersion)&&VersionMatches(binding.contract.version,package.manifest.contractVersion),"dependency uses another game/contract");
 require(package.manifest.id!=binding.mod,"cannot bind self dependency");
 auto source=fs::u8path(project.RootDir())/"mod-authoring.json";auto previous=Read(source);auto j=AuthoringJSON::parse(previous);
 auto path="Dependencies/"+package.hash+".judasmod";std::string bytes;require(EncodePackage(package,bytes,error),error);
 ContentStorage storage((fs::u8path(project.RootDir())/"Dependencies").u8string());std::vector<std::string> existing;
 require(storage.List(existing,Limits::Mods+1,error),error);
 if(std::find(existing.begin(),existing.end(),package.hash+".judasmod")==existing.end())require(storage.Publish(package.hash+".judasmod",bytes,error,true),error);
 auto paths=binding.dependencies;paths.erase(std::remove_if(paths.begin(),paths.end(),[&](const auto& old){Package prior;require(ReadPackage(project.Resolve(old),prior,error),error);return prior.manifest.id==package.manifest.id;}),paths.end());paths.push_back(path);
 require(paths.size()<=Limits::Mods,"author dependency count");j["dependencies"]=paths;
 require(PublishAuthoringOutputs(project.RootDir(),{{source.u8string(),j.dump(2)+"\n",previous,true}},error),error);
 error.clear();return true;
 }catch(const std::exception& e){error="bind dependency: "+std::string(e.what());return false;}}
namespace {
Manifest AuthorDraft(const Project& p){Manifest draft;std::string error;require(ParseManifest(Read(fs::u8path(p.RootDir())/"mod.json"),draft,error,true),error);return draft;}
std::vector<Selection> AuthorDependencies(const Project& project,const KitBinding& binding,const Manifest& draft){
 std::map<std::string,std::shared_ptr<Package>> available;std::string error;
 for(const auto& path:binding.dependencies){auto package=std::make_shared<Package>();require(ReadPackage(project.Resolve(path),*package,error),error);require(available.emplace(package->manifest.id,package).second,"multiple bound versions of dependency; bind one exact version");}
 std::vector<Selection> result;std::set<std::string> seen;
 std::function<void(const Dependency&)> visit=[&](const auto& dep){
  auto item=available.find(dep.id);require(item!=available.end()&&VersionMatches(item->second->manifest.version,dep.version),"missing/incompatible author dependency: "+dep.id);
  if(!seen.insert(dep.id).second){return;}
  auto package=item->second;result.push_back({package,package->manifest.executable?package->hash:std::string()});
  require(result.size()<=Limits::Mods,"author dependency count exceeds 16");for(const auto& child:package->manifest.dependencies)visit(child);
 };
 for(const auto& dep:draft.dependencies){visit(dep);}
 return result;
}
}

bool OpenAuthorAssets(const Project& p,AssetDatabase& out,std::string& error){try{
 out.Scan(p.RootDir(),p.AssetsDir());
 if(!fs::exists(fs::u8path(p.RootDir())/"mod-authoring.json"))return true;
 KitBinding binding;require(ReadKitBinding(p,binding,error),error);auto kit=Kit(fs::u8path(p.Resolve(binding.kit)));
 auto draft=AuthorDraft(p);auto dependencies=AuthorDependencies(p,binding,draft);
 auto records=out.Records();for(auto& [_,record]:records)record.owner=binding.mod;
 JSON exports=JSON::object(),baseExports=JSON::object();
 for(const auto& [name,ref]:binding.contract.published)baseExports[name]={{"kind",ref.kind},{"value",ref.value}};
 exports["base"]=baseExports;
 for(const auto& entry:kit.at("assets")){
  auto id=entry.at("id").get<std::string>(),path=entry.at("path").get<std::string>();AssetType type{};
  require(AssetKind(entry.at("type"),type),"unknown kit asset type");
  auto physical=fs::u8path(p.Resolve(binding.kit))/fs::u8path(path);
  AssetRecord r{id,type,physical.u8string(),"Base/"+path,"kit",false};r.readOnly=true;
  require(records.emplace(id,r).second,"owned asset collides with base; assign identities before cooking");
 }
 if(!dependencies.empty()){
  Store cache((fs::u8path(p.RootDir())/".AuthoringCache").u8string());Snapshot snapshot;
  require(cache.Preview(binding.contract,dependencies,snapshot,error),error);
  for(const auto& selected:snapshot.resolution.ordered){const auto& m=selected.package->manifest;
   exports[m.id]=snapshot.resolution.catalog.at("exports").at(m.id);
   for(const auto& asset:m.assets){AssetType type{};require(AssetKind(asset.type,type),"unknown dependency type");
    AssetRecord r{asset.id,type,snapshot.files.at(m.id+":"+asset.path),"Mods/"+m.id+"/"+asset.path,"dependency",false};r.owner=m.id;r.readOnly=true;
    require(records.emplace(r.id,r).second,"dependency identity collision");
   }
  }
 }
 std::map<std::string,std::set<std::string>> permissions;
 for(const auto& import:draft.imports){
  auto ref=exports.at(import.owner).at(import.name);
  require(ref.at("kind")==import.reference.kind&&ref.at("value")==import.reference.value,"unpublished/changed author import");
  if(import.reference.kind=="script"){auto record=records.find(import.reference.value);require(record!=records.end(),"author module import missing");permissions[binding.mod].insert(record->second.relativePath);}
 }
 for(const auto& selected:dependencies)for(const auto& import:selected.package->manifest.imports)if(import.reference.kind=="script"){
  auto record=records.find(import.reference.value);require(record!=records.end(),"dependency module import missing");permissions[selected.package->manifest.id].insert(record->second.relativePath);
 }
 JSON catalog={{"active",JSON::array()},{"contributions",JSON::array()},{"exports",exports}};
 require(out.SetEffectiveRecords(std::move(records),catalog.dump(),std::move(permissions),error),error);
 out.SetIdentityQualifier([p](const AssetId& local){return AuthorIdentity(p,local);});error.clear();return true;
 }catch(const std::exception& e){error="author assets: "+std::string(e.what());return false;}}

bool AuthorPackage(const Project& project,const Manifest& draft,Package& out,std::string& error){try{
 KitBinding binding;require(ReadKitBinding(project,binding,error),error);require(draft.id==binding.mod&&draft.game==binding.contract.game,"manifest does not match bound kit/mod");AssetDatabase assets;require(OpenAuthorAssets(project,assets,error),error);require(assets.Problems().empty(),"mod asset database problems");Package p;p.manifest=draft;p.manifest.assets.clear();p.manifest.files.clear();p.manifest.scenes.clear();
 auto add=[&](const std::string& relative){Portable(relative);auto physical=fs::u8path(project.Resolve(relative));auto bytes=Read(physical,Limits::File);require(p.files.emplace(relative,bytes).second,"duplicate payload path");p.manifest.files.push_back({relative,SceneFingerprintSha256(bytes),bytes.size()});};
 for(auto& [id,record]:assets.Records())if(!record.readOnly){require(binding.identities.contains(id),"asset was not assigned a mod-owned identity before cooking: "+record.relativePath);require(AssetDatabase::ValidateAssetFile(record.path,record.type,error),error);if(record.type==AssetType::Mesh)if(!VerifyImportedModelFresh(project.RootDir(),record.path,error))throw std::runtime_error("stale mod mesh: "+error);add(record.relativePath);p.manifest.assets.push_back({binding.identities.at(id),id,AssetTypeName(record.type),record.relativePath});}
 for(auto& entry:fs::recursive_directory_iterator(fs::u8path(project.ScenesDir())))if(entry.is_regular_file()&&entry.path().extension()==".judas"){auto relative=project.MakeRelative(entry.path().u8string());Scene scene;if(!LoadSceneFromFile(entry.path().u8string(),scene,error)||!ValidateSceneContent(scene,assets,project.Settings().classification,project.Settings().navigation,error))throw std::runtime_error(relative+": "+error);p.manifest.scenes.push_back(relative);add(relative);}
 for(const auto& n:p.manifest.notices){add(n);}
 std::string bytes;require(EncodePackage(p,bytes,error),error);require(DecodePackage(bytes,p,error),error);
 auto selected=AuthorDependencies(project,binding,p.manifest);
 selected.push_back({std::make_shared<Package>(p),p.manifest.executable?p.hash:std::string()});
 Store cache((fs::u8path(project.RootDir())/".AuthoringCache").u8string());Snapshot snapshot;
 require(cache.Preview(binding.contract,selected,snapshot,error),error);
 AssetDatabase base=assets;std::map<AssetId,AssetRecord> published;
 for(const auto& [id,a]:assets.Records())if(a.readOnly&&a.owner.empty())published[id]=a;
 require(base.SetEffectiveRecords(std::move(published),"",{},error),error);
 auto kit=Kit(fs::u8path(project.Resolve(binding.kit)));std::map<std::string,std::string> baseScenes;
 for(const auto& scene:kit.at("scenes")){auto logical=scene.at("path").get<std::string>();baseScenes[logical]=(fs::u8path(project.Resolve(binding.kit))/fs::u8path(logical)).u8string();}
 Session validated;require(ComposeContent(project,binding.contract,snapshot,base,baseScenes,validated,error),error);
 out=std::move(p);error.clear();return true;
 }catch(const std::exception& e){error="mod publication: "+std::string(e.what());return false;}}
bool ValidateAuthorManifest(const Project& p,const Manifest& m,std::string& e){Package package;return AuthorPackage(p,m,package,e);}
}
