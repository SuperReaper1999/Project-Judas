#include "NavigationDependencies.h"
#include "ModAuthoring.h"
#include "TerrainAuthoring.h"
#include "NavigationAsset.h"
#include "ProjectExporter.h"
#include "SceneSerialization.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
int main(int argc,char** argv){if(argc!=3){std::cerr<<"usage: judas_navigation_dependency_tests owned-copy.judasproj terrain-source-relative\n";return 2;}int count=0;auto check=[&](bool ok,const char* name){++count;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';if(!ok)throw std::runtime_error(name);};try{
 Project p;AssetDatabase assets;std::string error;check(p.Load(argv[1],error)&&JudasMods::OpenAuthorAssets(p,assets,error),"open ordinary owned project with published kit");TerrainDraft draft;check(draft.Open(p.Resolve(argv[2]),error),"open owned terrain");std::vector<NavigationDependency> rows;
 check(InspectNavigationDependencies(p,assets,draft.source.meshId,rows,error),"discover shared terrain dependents");unsigned owned=0;for(auto& row:rows)if(!row.readOnly){++owned;check(!row.stale,"initial owned navigation is fresh");}check(owned>=2,"two independently authored scenes share geometry");
 draft.source.heights[draft.source.heights.size()/2]+=.25f;draft.MarkChanged();check(draft.Save(error),"save genuine terrain sculpt");TerrainCookResult result;check(CookTerrain(draft.source,draft.Path(),p.RootDir(),assets,result,error)&&result.geometryChanged,"publish changed geometry through normal cook");check(JudasMods::OpenAuthorAssets(p,assets,error)&&InspectNavigationDependencies(p,assets,draft.source.meshId,rows,error),"inspect all dependents after cook");owned=0;for(auto& row:rows)if(!row.readOnly){++owned;check(row.stale,"both scenes report stale bake");}
 // A failure after one bounded surface bake still cannot publish half a batch.
 std::vector<NavigationDependency> broken;for(auto& row:rows)if(!row.readOnly)broken.push_back(row);broken.back().surface=99999999;
 check(!RebuildNavigationDependencies(p,assets,broken,error),"late invalid surface preserves last-good batch");
 check(InspectNavigationDependencies(p,assets,draft.source.meshId,rows,error),"failed batch remains inspectable");for(auto& row:rows)if(!row.readOnly)check(row.stale,"failed batch publishes neither dependent scene");
 unsigned cancellationCalls=0;check(!RebuildNavigationDependencies(p,assets,rows,error,[&]{return ++cancellationCalls>=3;}),"cancel after actual bounded bake preserves last-good batch");
 check(InspectNavigationDependencies(p,assets,draft.source.meshId,rows,error),"post-bake cancellation remains inspectable");for(auto& row:rows)if(!row.readOnly)check(row.stale,"post-bake cancellation publishes neither scene");
 std::vector<NavigationDependency> one;for(auto& row:rows)if(!row.readOnly){one.push_back(row);break;}
 check(!RebuildNavigationDependencies(p,assets,rows,error,[]{return true;}),"cancel preserves all last-good bakes");check(InspectNavigationDependencies(p,assets,draft.source.meshId,rows,error),"cancelled batch remains inspectable");for(auto& row:rows)if(!row.readOnly)check(row.stale,"cancel did not publish scene or navigation");
 check(RebuildNavigationDependencies(p,assets,one,error),"explicit first-scene-only rebuild");check(InspectNavigationDependencies(p,assets,draft.source.meshId,rows,error),"inspect partly rebuilt project");unsigned fresh=0,stale=0;for(auto& row:rows)if(!row.readOnly)(row.stale?stale:fresh)++;check(fresh==1&&stale>=1,"deliberately unrepaired second scene stays stale");
 for(auto& row:rows)if(!row.readOnly&&row.stale){Scene scene;check(LoadSceneFromFile(p.Resolve(row.scene),scene,error),"load deliberately unrepaired scene");check(!ValidateSceneContent(scene,assets,p.Settings().classification,p.Settings().navigation,error)&&error.find("Stale navigation")!=std::string::npos,"normal package validation rejects stale second-scene bake");}
 check(RebuildNavigationDependencies(p,assets,rows,error),"integrated stale-owned-scenes rebuild");check(InspectNavigationDependencies(p,assets,draft.source.meshId,rows,error),"recheck batch");for(auto& row:rows)if(!row.readOnly)check(!row.stale,"every owned dependent scene fresh");
 for(auto& row:rows)if(!row.readOnly){Scene scene;check(LoadSceneFromFile(p.Resolve(row.scene),scene,error)&&ValidateSceneContent(scene,assets,p.Settings().classification,p.Settings().navigation,error),"normal package scene validation accepts refreshed dependent");}
 draft.source.textureSize=draft.source.textureSize==256?512:256;draft.MarkChanged(false);check(draft.Save(error)&&CookTerrain(draft.source,draft.Path(),p.RootDir(),assets,result,error)&&!result.geometryChanged,"appearance-only cook does not change geometry");check(JudasMods::OpenAuthorAssets(p,assets,error)&&InspectNavigationDependencies(p,assets,draft.source.meshId,rows,error),"inspect appearance-only cook");for(auto& row:rows)if(!row.readOnly)check(!row.stale,"appearance-only cook retains fresh navigation");
 std::cout<<count<<" navigation dependency checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
