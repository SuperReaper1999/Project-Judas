#include "ProjectExporter.h"
#include "AssetDatabase.h"
#include "GamePackage.h"
#include "Prefab.h"
#include "SceneFingerprint.h"
#include "SceneSerialization.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <unistd.h>
namespace fs = std::filesystem;
int main(int argc, char** argv) {
    const fs::path temp = fs::temp_directory_path() / ("judas-m38-" + std::to_string(getpid()));
    fs::create_directories(temp); int checks=0, failed=0;
    auto check=[&](bool good,const char* label){ ++checks; failed+=!good; std::printf("%s %s\n",good?"PASS":"FAIL",label); };
    std::string error; Project project;
    check(Project::CreateNew((temp/"source").string(),"Export fixture",project,error), "create ordinary project");
    Scene prefab; prefab.Settings().name="Source";
    auto& child=prefab.CreateObject("Textured mesh"); child.render=SceneRenderComponent{};
    child.render->meshAsset="11111111111111111111111111111111";
    child.render->textureAsset="22222222222222222222222222222222";
    Scene scene; scene.Settings().name="Package fixture"; SceneObjectId instance;
    check(InstantiatePrefab(scene,prefab,"33333333333333333333333333333333",SceneTransform{},instance,error),"linked prefab authored");
    fs::path assets=project.AssetsDir();
    fs::copy_file("tests/fixtures/engine-assets/models/beacon.obj", assets/"mesh.obj");
    fs::copy_file("tests/fixtures/engine-assets/textures/beacon.png", assets/"texture.png");
    check(SaveSceneToFile(prefab,(assets/"prop.judasprefab").string(),error),"write prefab source");
    check(AssetDatabase::WriteMeta((assets/"mesh.obj").string()+kAssetMetaExtension,child.render->meshAsset,AssetType::Mesh,"/private/import/path",error)&&
          AssetDatabase::WriteMeta((assets/"texture.png").string()+kAssetMetaExtension,child.render->textureAsset,AssetType::Texture,"import",error)&&
          AssetDatabase::WriteMeta((assets/"prop.judasprefab").string()+kAssetMetaExtension,"33333333333333333333333333333333",AssetType::Prefab,"import",error),"asset IDs registered");
    project.Settings().input.Add("export_custom_action",false);
    project.Settings().input.AddBinding("export_custom_action",{"key:Q",1,0});
    project.Settings().startupScene="Scenes/start.judas";
    check(SaveSceneToFile(scene,project.StartupScenePath(),error)&&project.Save(error),"startup scene and input configuration saved");
    std::ofstream(assets/"development.log")<<"do not ship";
    ProjectExportOptions options; options.destination=(temp/"game").string();
    if(argc==2) options.runtimeExecutable=argv[1];
    ProjectExportResult result;
    const bool exported=ExportProject(project,options,result,error);
    if(!exported)std::printf("ERROR %s\n",error.c_str());
    check(exported,"Release export succeeds");
    if(exported){
      check(fs::exists(temp/"game/judas")&&fs::exists(temp/"game/engine/assets/fonts/DejaVuSans.ttf")&&result.assetCount==3,"package runtime/font and transitive registered assets");
      check(!fs::exists(temp/"game/Assets/development.log")&&!fs::exists(temp/"game/src"),"unregistered development junk excluded");
      GamePackage launch; check(ReadGamePackage((temp/"game").string(),launch,error)&&launch.projectFile=="game.judasproj","explicit package launch record");
      Project shipped; check(shipped.Load((temp/"game/game.judasproj").string(),error)&&shipped.Settings().startupScene==project.Settings().startupScene&&shipped.Settings().input.Serialize()==project.Settings().input.Serialize(),"startup and project input map retained");
      AssetDatabase before,after;before.Scan(project.RootDir(),project.AssetsDir());after.Scan(shipped.RootDir(),shipped.AssetsDir());
      Scene a,b;std::string ha,hb;
      check(ResolvePrefabs(scene,&before,a,error)&&ResolvePrefabs(scene,&after,b,error)&&ComputeSceneFingerprint(a,ha,error)&&ComputeSceneFingerprint(b,hb,error)&&ha==hb,"effective authored fingerprint independent of package path");
      AssetId id;AssetType type;std::string provenance;
      check(AssetDatabase::ReadMeta((temp/"game/Assets/mesh.obj.judasmeta").string(),id,type,provenance,error)&&id==child.render->meshAsset&&provenance=="Assets/mesh.obj","normal stable ID and relative provenance");
      std::ofstream(temp/"game/stale.txt")<<"old export";
      check(ExportProject(project,options,result,error)&&!fs::exists(temp/"game/stale.txt"),"re-export replaces stale package files");
      fs::remove(assets/"texture.png");
      check(!ExportProject(project,options,result,error)&&!error.empty()&&fs::exists(temp/"game/Assets/texture.png"),"missing asset fails without damaging last complete export");
      fs::copy_file("tests/fixtures/engine-assets/textures/beacon.png", assets/"texture.png");
      fs::rename(temp/"game",temp/"moved game");
      Project moved;check(ReadGamePackage((temp/"moved game").string(),launch,error)&&moved.Load((temp/"moved game"/launch.projectFile).string(),error)&&fs::exists(moved.StartupScenePath()),"moved package resolves unchanged relative scene");
      std::string saves; check(PackageSaveDirectory(launch,saves,error)&&fs::path(saves).is_absolute()&&saves.find((temp/"moved game").string())==std::string::npos,"runtime saves outside installation");
    }
    options.destination=(temp/"source/inside").string();check(!ExportProject(project,options,result,error),"source-tree export refused");
    options.destination=(temp/"unrelated").string();fs::create_directory(options.destination);check(!ExportProject(project,options,result,error),"arbitrary destination not replaced");
    fs::remove_all(temp);std::printf("SUMMARY %d checks %d failures\n",checks,failed);return failed?1:0;
}
