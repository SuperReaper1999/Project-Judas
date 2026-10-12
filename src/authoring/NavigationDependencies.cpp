#include "NavigationDependencies.h"
#include "NavigationAsset.h"
#include "AssetDependencies.h"
#include "AuthoringDocument.h"
#include "ModAuthoring.h"
#include "SceneSession.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "Prefab.h"
#include "AsyncFile.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <tuple>
namespace fs=std::filesystem;
namespace {
std::string Read(const fs::path& p){std::vector<uint8_t> bytes;std::string error;if(!ReadWholeFile(p.string(),bytes,error))throw std::runtime_error(error);return {bytes.begin(),bytes.end()};}
bool Uses(const Scene& scene,const AssetDatabase& assets,const AssetId& changed,std::string& error){
 if(changed.empty())return true;
 std::set<AssetId> pending,seen;CollectSceneAssetReferences(scene,pending);
 while(!pending.empty()){auto id=*pending.begin();pending.erase(pending.begin());if(id==changed)return true;if(!seen.insert(id).second)continue;auto* record=assets.Find(id);if(!record){error="Missing dependency "+id;return false;}std::set<AssetId> next;std::set<std::string> files;if(!CollectAssetDependencies(*record,next,files,error))return false;pending.insert(next.begin(),next.end());}return false;
}
}
bool InspectNavigationDependencies(const Project& p,const AssetDatabase& assets,const AssetId& changed,
 std::vector<NavigationDependency>& out,std::string& error){try{
 std::map<std::string,bool> scenes;SceneSession registered(p,p.StartupScenePath());for(auto& path:registered.Scenes())scenes[path]=false;
 if(fs::exists(fs::path(p.RootDir())/"mod-authoring.json")){JudasMods::KitBinding binding;if(!JudasMods::ReadKitBinding(p,binding,error))return false;auto kit=JudasMods::JSON::parse(Read(fs::path(p.Resolve(binding.kit))/"kit.json"));for(auto& row:kit.at("scenes"))scenes[(fs::path(binding.kit)/row.at("path").get<std::string>()).generic_string()]=true;}
 std::vector<NavigationDependency> next;
 for(auto& [path,readOnly]:scenes){Scene scene,resolved,flat;if(!LoadSceneFromFile(p.Resolve(path),scene,error))return false;std::string dependencyError;if(!Uses(scene,assets,changed,dependencyError)){if(!dependencyError.empty())next.push_back({path,dependencyError,0,true,readOnly});continue;}
 if(!ResolvePrefabs(scene,&assets,resolved,error)||!FlattenHierarchy(resolved,flat,error)){next.push_back({path,error,0,true,readOnly});continue;}
 for(auto& surface:flat.Objects())if(surface.navigationSurface&&surface.navigationSurface->enabled){NavigationDependency row;row.scene=path;row.surface=surface.id;row.readOnly=readOnly;NavigationGeometry geometry;NavigationData baked;auto* record=assets.Find(surface.navigationSurface->asset);std::string why;
 if(!CollectNavigationGeometry(flat,surface,p.Settings().navigation,geometry,why,&assets))row.reason="Cannot collect current geometry: "+why;
 else if(!record||!LoadNavigation(record->path,baked,why))row.reason="Missing/invalid bake: "+why;
 else if(geometry.fingerprint!=baked.fingerprint)row.reason="Geometry/settings fingerprint changed; explicit navigation rebuild required";
 row.stale=!row.reason.empty();if(row.stale&&readOnly)row.reason+="; published kit is read-only: fork/replace this scene in the owned project";next.push_back(std::move(row));}
 }out=std::move(next);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
bool RebuildNavigationDependencies(const Project& p,AssetDatabase& assets,const std::vector<NavigationDependency>& rows,
 std::string& error,const std::function<bool()>& cancelled){
 fs::path scratch;try{
 auto cancel=[&]{if(cancelled&&cancelled())throw std::runtime_error("Cancelled; last good navigation/scenes retained");};
 std::map<std::string,std::vector<SceneObjectId>> selected;for(auto& row:rows)if(row.stale&&!row.readOnly){if(!row.surface)throw std::runtime_error(row.scene+": "+row.reason);selected[row.scene].push_back(row.surface);}if(selected.empty()){error="No stale owned navigation surfaces; read-only kit findings require an explicit scene fork";return false;}
 scratch=fs::path(p.RootDir())/(".navigation-stage-"+MintAssetId());fs::create_directories(scratch);
 std::vector<AuthoringOutput> outputs;
 std::vector<std::tuple<std::string,SceneObjectId,std::string>> expectedGeometry;
 const auto projectBefore=Read(p.ProjectFile());
 for(auto& [path,ids]:selected){cancel();Scene scene,resolved,flat;auto original=Read(p.Resolve(path));if(!LoadSceneFromString(original,scene,error)||!ResolvePrefabs(scene,&assets,resolved,error)||!FlattenHierarchy(resolved,flat,error))throw std::runtime_error(error);
 for(auto id:ids){cancel();auto* source=flat.Find(id);auto* authored=scene.Find(id);if(!source||!authored||!authored->navigationSurface)throw std::runtime_error(path+": prefab navigation requires an owned authored surface override before rebuilding");NavigationData data;if(!BakeNavigation(flat,*source,p.Settings().navigation,data,error,&assets))throw std::runtime_error(error);cancel();
 auto local=MintAssetId(),identity=JudasMods::AuthorIdentity(p,local);auto target=fs::path(p.AssetsDir())/"navigation"/("rebake-"+local+".judasnav");auto temporary=scratch/(local+".judasnav");if(!SaveNavigation(temporary.string(),data,error))throw std::runtime_error(error);NavigationData verified;if(!LoadNavigation(temporary.string(),verified,error))throw std::runtime_error(error);
 expectedGeometry.emplace_back(path,id,data.fingerprint);
 auto metadata=temporary.string()+kAssetMetaExtension;if(!AssetDatabase::WriteMeta(metadata,identity,AssetType::Navigation,target.string(),error))throw std::runtime_error(error);
 outputs.push_back({target.string(),Read(temporary),{},false});outputs.push_back({target.string()+kAssetMetaExtension,Read(metadata),{},false});authored->navigationSurface->asset=identity;}
 std::string text;if(!SaveSceneToString(scene,text))throw std::runtime_error("Cannot serialize rebuilt scene");outputs.push_back({p.Resolve(path),text,original,true});}
 cancel();
 if(Read(p.ProjectFile())!=projectBefore)throw std::runtime_error("Project changed during dependent rebuild; prior outputs retained");
 for(auto& [path,id,fingerprint]:expectedGeometry){Scene current,resolved,flat;NavigationGeometry geometry;if(!LoadSceneFromFile(p.Resolve(path),current,error)||!ResolvePrefabs(current,&assets,resolved,error)||!FlattenHierarchy(resolved,flat,error))throw std::runtime_error(error);auto* surface=flat.Find(id);if(!surface||!CollectNavigationGeometry(flat,*surface,p.Settings().navigation,geometry,error,&assets)||geometry.fingerprint!=fingerprint)throw std::runtime_error("Navigation inputs changed during rebuild: "+path+"; prior outputs retained");}
 cancel();if(!PublishAuthoringOutputs(p.RootDir(),outputs,error))throw std::runtime_error(error);fs::remove_all(scratch);return JudasMods::OpenAuthorAssets(p,assets,error);
 }catch(const std::exception& e){if(!scratch.empty()){std::error_code ignored;fs::remove_all(scratch,ignored);}error=e.what();return false;}}
