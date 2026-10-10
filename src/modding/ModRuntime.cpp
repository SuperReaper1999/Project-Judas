#include "ModRuntime.h"
#include "Project.h"
#include "ProjectExporter.h"
#include "SceneFingerprint.h"
#include "SceneSerialization.h"
#include "NavigationAsset.h"
#include "CollisionAsset.h"
#include "Prefab.h"
#include "PlatformServices.h"
#include "ScriptSystem.h"
#include <filesystem>
#include <algorithm>
namespace JudasMods {
std::string RegistryIdentity(const Project& p){const auto& s=p.Settings();return SceneFingerprintSha256("Judas.ModRegistries.1:"+s.input.Serialize()+":"+s.classification.Serialize()+":"+s.navigation.Serialize()+":"+s.localization.Encode()+":"+s.audio.Serialize());}
bool AssignAssetIdentity(const std::string& root,AssetId& id,std::string& error){try{
 namespace fs=std::filesystem;using AuthoringJSON::require;
 auto path=fs::u8path(root)/"mod-authoring.json";
 if(!fs::exists(path)){error.clear();return true;}
 std::string previous;require(ReadRegularContentFile(path.u8string(),previous,Limits::Metadata,error),error);
 auto binding=AuthoringJSON::parse(previous);auto kitPath=binding.at("kit").get<std::string>();std::string key;
 require(PortablePath(kitPath,key,error),error);std::string kitBytes;
 require(ReadRegularContentFile((fs::u8path(root)/fs::u8path(kitPath)/"kit.json").u8string(),kitBytes,Limits::Metadata,error),error);
 auto kit=AuthoringJSON::parse(kitBytes);Contract contract;
 require(ParseContract(kit.at("contract").dump(),contract,error),error);
 require(kit.at("identity")==binding.at("identity")&&kit.at("identity")==SceneFingerprintSha256(kit.at("contract").dump()+":"+kit.at("project").get<std::string>()),"authoring kit identity changed");
 Project project;require(project.Load(Project::FindProjectFileFor(root),error),error);
 require(RegistryIdentity(project)==contract.registries,"mod registries differ from kit");
 auto mod=binding.at("mod").get<std::string>();require(ValidIdentity(mod)&&IsValidAssetId(id),"invalid mod/local asset identity");
 auto qualified=QualifiedAsset(contract.game,mod,id);binding["identities"][qualified]=id;
 require(binding.at("identities").size()<=Limits::Files,"authoring identity count exceeds 4096");
 // This is one explicit source publication. Pin the destination and publish via
 // the same no-link storage primitive used for installed immutable content.
 ContentStorage storage(root);std::string current;
 require(storage.Read("mod-authoring.json",current,Limits::Metadata,error)&&current==previous,"authoring identity ledger changed externally");
 require(storage.Publish("mod-authoring.json",binding.dump(2)+"\n",error),error);
 id=qualified;error.clear();return true;
 }catch(const std::exception& e){error="asset ownership: "+std::string(e.what());return false;}}
bool ComposeContent(const Project& project,const Contract& contract,const Snapshot& snapshot,const AssetDatabase& base,const std::map<std::string,std::string>& baseScenes,Session& session,std::string& error){try{
 using AuthoringJSON::require;session.snapshot=snapshot;
 session.assets=base;require(session.assets.Problems().empty(),"base asset database is inconsistent");
 for(auto& [name,reference]:contract.published)if(reference.kind!="scene"){auto* record=session.assets.Find(reference.value);AssetType type{};require(AssetKind(reference.kind,type)&&record&&record->type==type&&!record->missing,"published base reference unavailable: "+name);}
 for(auto& [name,replacement]:contract.replacements){std::string path;if(replacement.target.kind=="scene"){auto it=baseScenes.find(replacement.target.value);path=it==baseScenes.end()?project.Resolve(replacement.target.value):it->second;}else{auto* record=session.assets.Find(replacement.target.value);require(record&&!record->missing,"replacement base target missing: "+name);path=record->path;}std::string revision;require(SceneFingerprintSha256File(path,revision,error)&&revision==replacement.revision,"replacement contract requires another base revision: "+name);}
require(ValidateReferences(contract,session.snapshot.resolution,session.assets,session.snapshot.files,error),error);
 std::map<AssetId,AssetRecord> records=session.assets.Records();std::map<std::string,std::set<std::string>> permissions;
 for(auto& selection:session.snapshot.resolution.ordered){auto& m=selection.package->manifest;for(auto& a:m.assets){AssetType t{};require(AssetKind(a.type,t),"invalid asset type");AssetRecord record{a.id,t,session.snapshot.files.at(m.id+":"+a.path),"Mods/"+m.id+"/"+a.path,"mod:"+m.id,false};record.readOnly=true;record.owner=m.id;require(records.emplace(a.id,record).second,"asset namespace collision: "+m.id+"/"+a.id);}
 for(auto& s:m.scenes)session.sceneFiles.emplace("Mods/"+m.id+"/"+s,session.snapshot.files.at(m.id+":"+s));
 for(auto& i:m.imports)if(i.reference.kind=="script"){auto record=records.find(i.reference.value);require(record!=records.end(),"published script import not registered");permissions[m.id].insert(record->second.relativePath);}
 for(auto& r:m.replacements){const auto& target=contract.replacements.at(r.target).target;if(target.kind=="scene"){session.sceneFiles[target.value]=session.snapshot.files.at(m.id+":"+r.provider.value);}else {auto provider=records.at(r.provider.value);auto prior=records.find(target.value);require(prior!=records.end()&&prior->second.type==provider.type,"replacement target missing/incompatible");provider.id=target.value;records[target.value]=provider;}}
 }
 // A replacement's script dependencies and module paths retain provider ownership.
 require(session.assets.SetEffectiveRecords(std::move(records),session.snapshot.resolution.catalog.dump(),std::move(permissions),error),error);
 // Eagerly validate all registered mod scenes and navigation geometry before any
 // world or project script starts. Ordinary immutable decodes remain resource jobs.
 for(auto& [logical,path]:session.sceneFiles){Scene scene;if(!LoadSceneFromFile(path,scene,error)||!ValidateSceneContent(scene,session.assets,project.Settings().classification,project.Settings().navigation,error))throw std::runtime_error(logical+": "+error);}
 for(auto& selection:session.snapshot.resolution.ordered)for(auto& a:selection.package->manifest.assets)if(a.type=="prefab"){Scene scene;auto* record=session.assets.Find(a.id);if(!record||!LoadSceneFromFile(record->path,scene,error)||!ValidateSceneContent(scene,session.assets,project.Settings().classification,project.Settings().navigation,error))throw std::runtime_error(selection.package->manifest.id+"/"+a.path+": "+error);}

 // Resolved prefab contents must meet the requesting package's script policy too.
 // A published prefab is not permission to conceal a private executable slot.
 auto inspect=[&](const Scene& authored,const Manifest& manifest){
  Scene resolved;require(ResolvePrefabs(authored,&session.assets,resolved,error),error);
  if(!manifest.executable)for(const auto& object:resolved.Objects())for(const auto& slot:object.scripts){
   auto* record=session.assets.Find(slot.asset);
   require(record&&record->owner.empty(),"hidden executable dependency in data-only content: "+manifest.id);
   auto schema=contract.behaviors.find(slot.asset);
   require(schema!=contract.behaviors.end(),"unpublished base behaviour in resolved prefab: "+manifest.id);
   std::vector<ScriptProperty> properties;
   require(ScriptSystem::ReadProperties(schema->second.dump(),slot.properties,properties,error),error);
  }
 };
 for(const auto& selected:session.snapshot.resolution.ordered){
  const auto& manifest=selected.package->manifest;
  for(const auto& logical:manifest.scenes){Scene scene;require(LoadSceneFromFile(session.snapshot.files.at(manifest.id+":"+logical),scene,error),error);inspect(scene,manifest);}
  for(const auto& asset:manifest.assets)if(asset.type=="prefab"){Scene scene;require(LoadSceneFromFile(session.assets.Find(asset.id)->path,scene,error),error);inspect(scene,manifest);}
 }

 error.clear();return true;
 }catch(const std::exception& e){error="effective mod content: "+std::string(e.what());return false;}}
bool InstallForGame(const Project& project,const Package& package,std::string& error){try{
 using AuthoringJSON::require;Contract contract;
 require(ParseContract(project.Settings().modContract,contract,error),error);
 require(contract.registries==RegistryIdentity(project),"game mod registry contract changed");
 std::string root;require(GameModDirectory(contract,root,error),error);Store store(root);
 std::vector<Installed> installed;std::vector<Loadout> loadout;require(store.List(installed,loadout,error),error);
 // Installation validates code by compilation only. This synthetic validation
 // selection is never written as user trust or used to execute a script.
 std::vector<Selection> selected;std::set<std::string> seen;
 std::function<void(std::shared_ptr<const Package>)> visit=[&](auto p){
  if(!seen.insert(p->manifest.id).second)return;
  selected.push_back({p,p->manifest.executable?p->hash:std::string()});
  require(selected.size()<=Limits::Mods,"install dependency count exceeds 16");
  for(const auto& dependency:p->manifest.dependencies){
   auto choice=std::find_if(loadout.begin(),loadout.end(),[&](const auto& v){return v.id==dependency.id;});
   require(choice!=loadout.end(),"select an installed required dependency before installing: "+dependency.id);
   auto item=std::find_if(installed.begin(),installed.end(),[&](const auto& v){return v.id==choice->id&&v.version==choice->version&&v.hash==choice->hash;});
   require(item!=installed.end(),"selected dependency is missing: "+dependency.id);
   auto dep=std::make_shared<Package>();require(ReadPackage((std::filesystem::u8path(root)/"Installed"/item->file).u8string(),*dep,error),error);
   require(dep->hash==item->hash,"installed dependency payload changed");visit(dep);
  }
 };
 visit(std::make_shared<Package>(package));Snapshot snapshot;require(store.Preview(contract,selected,snapshot,error),error);
 AssetDatabase base;base.Scan(project.RootDir(),project.AssetsDir());Session validated;
 require(ComposeContent(project,contract,snapshot,base,{},validated,error),error);
 require(store.Install(contract,package,error),error);error.clear();return true;
 }catch(const std::exception& e){error="mod installation validation: "+std::string(e.what());return false;}}
bool PrepareSession(Project& project,bool noMods,std::string& error){try{
 using AuthoringJSON::require;project.SetModSession({});
 if(noMods||project.Settings().modContract.empty()){error.clear();return true;}
 Contract contract;require(ParseContract(project.Settings().modContract,contract,error),error);if(!contract.enabled){error.clear();return true;}
 require(contract.registries==RegistryIdentity(project),"mod contract registries differ from the authoritative game");std::string root;require(GameModDirectory(contract,root,error),error);Store store(root);auto session=std::make_shared<Session>();require(store.Freeze(contract,session->snapshot,error),error);
 // An opted-in but empty loadout preserves the ordinary asset/save path.
 if(session->snapshot.resolution.ordered.empty()){error.clear();return true;}
 AssetDatabase base;base.Scan(project.RootDir(),project.AssetsDir());require(ComposeContent(project,contract,session->snapshot,base,{},*session,error),error);
 project.SetModSession(session);error.clear();return true;
 }catch(const std::exception& e){error="mod session: "+std::string(e.what());return false;}}
}
