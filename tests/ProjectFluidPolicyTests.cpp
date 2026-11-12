#include "SaveArchive.h"
#include "RuntimeWorld.h"
#include "Project.h"
#include "WorldStreaming.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "ProjectExporter.h"
#include "EnginePaths.h"
#include "PlatformServices.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

namespace {
int checks=0;
void Check(bool okay,const std::string& message){++checks;if(!okay)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}

}
int main(){try{
 std::string error;
 const auto root=std::filesystem::temp_directory_path()/("judas-fluid-policy-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Project project;Check(Project::CreateNew(root.string(),"Dormant fluids",project,error),"create public project fixture");
 Scene scene;auto& object=scene.CreateObject("Dormant basin");object.render=SceneRenderComponent{};object.fluidVolume=SceneFluidVolumeComponent{};object.fluidVolume->spacing=.4f;object.fluidVolume->countY=8;
 project.Settings().startupScene="Scenes/region.judas";Check(SaveSceneToFile(scene,project.StartupScenePath(),error)&&project.Save(error),"save unchanged dormant authoring");
 AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());JobSystem jobs(2);ResourceManager resources(nullptr,&assets,&jobs);
 RuntimeWorld world;world.ConfigureProject(project.Settings());Check(world.Build(Scene{},&resources,error),"disabled world starts without advanced dependencies");
 WorldRegion region;region.id="pool";region.scene=project.Settings().startupScene;PreparedWorldRegion prepared;
 Check(PrepareWorldRegion(region,project,assets,prepared,error)&&prepared.requiredGeometry.empty(),"dormant region requires no advanced geometry or bake");
 Check(prepared.source.Find(object.id)->fluidVolume.has_value(),"prepared region preserves authored fluid component");
 WorldManifest manifest;manifest.regions.emplace(region.id,region);WorldStreaming stream(world,resources,project,manifest);
 auto request=stream.Request("pool",false,error);Check(request!=0,"ordinary region request accepted");
 for(int i=0;i<500&&stream.Status(request)->state!="active";++i){resources.Pump();stream.Advance(false);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 Check(stream.Status(request)->state=="active","dormant region actually publishes through streaming: "+stream.Status(request)->state+" "+stream.Status(request)->error);
 Check(stream.Status(request)->pins.empty(),"disabled conserved authoring creates no physical suspension pin");
 Check(stream.Release(request),"release region demand");
 for(int i=0;i<500&&stream.Stats().active;++i){stream.Advance(false);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 Check(stream.Stats().active==0,"disabled fluid region actually unloads");
 project.Settings().advancedFluidDynamics=true;
 Check(!PrepareWorldRegion(region,project,assets,prepared,error)&&!error.empty(),"opt-in rejects unsupported suspended water owners");
 project.Settings().advancedFluidDynamics=false;
 ProjectExportOptions options;options.destination=(root.parent_path()/(root.filename().string()+"-package")).string();options.runtimeExecutable=(std::filesystem::path(EngineExecutableDir())/RuntimeExecutableName()).string();ProjectExportResult result;
 error.clear();
 Check(ExportProject(project,options,result,error),"disabled authored grid exports without solver allocation: "+error);
 Project packaged;Check(packaged.Load((std::filesystem::path(result.packageDirectory)/"game.judasproj").string(),error)&&!packaged.Settings().advancedFluidDynamics,"export preserves disabled project policy");
 Scene exported;Check(LoadSceneFromFile(packaged.StartupScenePath(),exported,error)&&exported.Find(object.id)->fluidVolume.has_value(),"export retains inert authored component");
 project.Settings().advancedFluidDynamics=true;options.destination+="-enabled";
 const bool enabledExport=ExportProject(project,options,result,error);
 Check(enabledExport,"V2 opt-in exports in every build channel: "+error);
 if(enabledExport){Project enabledPackage;Check(enabledPackage.Load((std::filesystem::path(result.packageDirectory)/"game.judasproj").string(),error)&&enabledPackage.Settings().advancedFluidDynamics,"grid export preserves explicit opt-in");}


 std::cout<<"Project fluid policy: "<<checks<<" checks, 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
