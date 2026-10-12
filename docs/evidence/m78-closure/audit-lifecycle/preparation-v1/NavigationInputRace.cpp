#include "FixtureSupport.h"
#include "NavigationDependencies.h"
#include "NavigationAsset.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "Prefab.h"
#include "ProjectExporter.h"
#include <set>
using namespace LifecycleAudit;
namespace {
std::map<std::string,std::string> PublishedBytes(const Project& p,const std::string& excluded){std::map<std::string,std::string> rows;for(const auto& e:fs::recursive_directory_iterator(p.RootDir()))if(e.is_regular_file()){auto rel=e.path().lexically_relative(p.RootDir()).generic_string();if(rel!=excluded&&rel.rfind(".navigation-stage-",0)!=0)rows[rel]=Read(e.path());}return rows;}
}
int main(int argc,char** argv){Receipt r;fs::path out;try{out=FreshOutput(argc,argv);std::string error;Project project;AssetDatabase db;
 r.Require(Project::CreateNew((out/"source").string(),"Navigation input revision audit",project,error),"ordinary owned project",error);db.Scan(project.RootDir(),project.AssetsDir());
 Scene prefab;auto& floor=prefab.CreateObject("shared physical floor");floor.body=SceneBodyComponent{};floor.body->halfExtents={8,.25f,8};const auto sourceBody=floor.id;const fs::path source=fs::path(project.AssetsDir())/"shared.judasprefab";
 r.Require(SaveSceneToFile(prefab,source.string(),error),"physical prefab source",error);AssetRecord prefabAsset;r.Require(db.Track(source.string(),prefabAsset,error),"normal prefab identity",error);
 for(unsigned i=0;i<2;++i){Scene scene;SceneObjectId root=0;r.Require(InstantiatePrefab(scene,prefab,prefabAsset.id,{},root,error),"linked physical prefab instance",error);auto& surface=scene.CreateObject("owned navigation surface");surface.navigationSurface=NavigationSurfaceSettings{};surface.navigationSurface->halfExtents={10,3,10};const auto id=surface.id;Scene resolved,flat;r.Require(ResolvePrefabs(scene,&db,resolved,error)&&FlattenHierarchy(resolved,flat,error),"normal prefab and hierarchy resolution",error);NavigationData baked;r.Require(BakeNavigation(flat,*flat.Find(id),project.Settings().navigation,baked,error,&db),"ordinary compact initial bake",error);
  auto path=fs::path(project.AssetsDir())/("initial-"+std::to_string(i)+".judasnav");r.Require(SaveNavigation(path.string(),baked,error),"save accepted initial bake",error);AssetRecord nav;r.Require(db.Track(path.string(),nav,error),"normal navigation identity",error);scene.Find(id)->navigationSurface->asset=nav.id;auto scenePath=fs::path(project.ScenesDir())/("shared-"+std::to_string(i)+".judas");r.Require(SaveSceneToFile(scene,scenePath.string(),error),"owned scene",error);if(!i)project.Settings().startupScene=project.MakeRelative(scenePath.string());
 }r.Require(project.Save(error),"registered startup",error);
 std::vector<NavigationDependency> rows;r.Require(InspectNavigationDependencies(project,db,prefabAsset.id,rows,error),"inspect transitive physical prefab dependency",error);r.Check(rows.size()==2&&std::all_of(rows.begin(),rows.end(),[](const auto& v){return !v.stale&&!v.readOnly;}),"both independently authored scenes are fresh dependents of the prefab");
 prefab.Find(sourceBody)->body->halfExtents.y=.35f;r.Require(SaveSceneToFile(prefab,source.string(),error),"real shared collider edit",error);r.Require(InspectNavigationDependencies(project,db,prefabAsset.id,rows,error),"reinspect changed source",error);r.Check(rows.size()==2&&std::all_of(rows.begin(),rows.end(),[](const auto& v){return v.stale;}),"both scenes detect changed resolved prefab geometry");
 const auto before=PublishedBytes(project,project.MakeRelative(source.string()));unsigned calls=0;bool edited=false;
 // The operation's existing cancel poll is used only as a deterministic owner-
 // thread observer boundary: return false so this is NOT cancellation evidence.
 auto observe=[&]{++calls;if(calls==3){prefab.Find(sourceBody)->body->halfExtents.x=7.25f;edited=SaveSceneToFile(prefab,source.string(),error);}return false;};
 const bool rebuilt=RebuildNavigationDependencies(project,db,{rows.front()},error,observe);const auto failure=error;const auto after=PublishedBytes(project,project.MakeRelative(source.string()));r.Check(edited,"source changes after the actual bounded bake");r.Check(!rebuilt&&failure.find("inputs changed")!=std::string::npos,"revision race rejected before batch publication",{{"error",failure},{"polls",calls}});r.Check(before==after,"failed rebuild retains exact scene/assets/metadata/project bytes and creates no published orphan");
 bool scratch=false;for(const auto& e:fs::directory_iterator(project.RootDir()))scratch|=e.path().filename().string().rfind(".navigation-stage-",0)==0;r.Check(!scratch,"failed revision race releases private staging");
 r.Require(InspectNavigationDependencies(project,db,prefabAsset.id,rows,error),"changed source remains inspectable",error);r.Check(std::all_of(rows.begin(),rows.end(),[](const auto& v){return v.stale;}),"neither dependent is falsely reported fresh after race rejection");r.Require(RebuildNavigationDependencies(project,db,rows,error),"new explicit rebuild from the current source",error);r.Require(InspectNavigationDependencies(project,db,prefabAsset.id,rows,error),"recheck newly accepted batch",error);r.Check(rows.size()==2&&std::all_of(rows.begin(),rows.end(),[](const auto& v){return !v.stale;}),"both owned dependents are now fresh");
 for(const auto& row:rows){Scene scene;r.Require(LoadSceneFromFile(project.Resolve(row.scene),scene,error),"read accepted scene",error);r.Check(ValidateSceneContent(scene,db,project.Settings().classification,project.Settings().navigation,error),"normal export validation accepts actual refreshed dependency",{{"error",error}});}
 return r.Finish(out,"Navigation rebuild must revalidate current transitive physical inputs and preserve the previous published batch if inputs changed during bake");
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';r.Check(false,"unexpected fixture exception",{{"error",e.what()}});if(!out.empty())return r.Finish(out,"navigation input revision boundary");return 2;}}
