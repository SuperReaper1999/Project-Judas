#include "ProjectFeaturePolicy.h"
#include "LiquidSystem.h"
#include "LiquidGeometry.h"
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
std::string State(LiquidSystem& system){SaveArchive a;system.Persist(a);return a.bytes;}
}
int main(){try{
 std::string error;GravityEquilibrium eq;eq.magnitude=9.81;LiquidBasinData baked;
 Check(BakeLiquidBasin(LiquidBox({-1,0,-1},{1,1,1}),eq,1e-6,.001,"policy",baked,error),"valid conserved geometry fixture");
 LiquidSystem enabled(true);LiquidBasinSettings author;author.initialVolume=1;
 auto handle=enabled.AddBasin(1,author,{},std::make_shared<LiquidBasinData>(baked),error);Check(handle.id!=0,"enabled system admits owner");
 const auto ownerBytes=State(enabled);
 Check(enabled.Remove(handle)&&enabled.Parcels().size()==1,"removing last owner retains its actual volume as a parcel");
 const auto parcelBytes=State(enabled);
 LiquidSystem control(true);SaveArchive valid(parcelBytes);control.Persist(valid);valid.Finish();
 Check(control.Parcels().size()==1&&control.Accounting("water").total==1,"enabled parcel-only restore conserves exact quantity");
 for(const auto& bytes:{ownerBytes,parcelBytes}){
  LiquidSystem disabled(false);const auto before=State(disabled);bool rejected=false;
  try{SaveArchive a(bytes);disabled.Persist(a);}catch(const std::exception& e){rejected=std::string(e.what()).find("Advanced Fluid Dynamics")!=std::string::npos;}
  Check(rejected,"disabled restore rejects advanced owner/parcel state with actionable diagnostic");
  Check(State(disabled)==before,"failed disabled restore publishes no ledger or parcels");
 }
 LiquidSystem empty(false),restored(false);SaveArchive emptyReader(State(empty));restored.Persist(emptyReader);emptyReader.Finish();Check(State(empty)==State(restored),"disabled empty participant remains compatible");
 Check(State(enabled)==parcelBytes,"rejected reads never modify source advanced state");
 // A serialized zero-volume ledger is still advanced state, not an empty participant.
 SaveArchive history;uint32_t zero=0;uint64_t next=1;std::map<std::string,double> expected{{"water",0}},density{{"water",1000}};std::map<SceneObjectId,bool> connections;
 history(zero,expected,density,next,connections,zero);bool denied=false;try{SaveArchive a(history.bytes);restored.Persist(a);}catch(const std::exception&){denied=true;}
 Check(denied&&State(restored)==State(empty),"even zero-quantity advanced history cannot mutate a disabled system");
 const auto root=std::filesystem::temp_directory_path()/("judas-fluid-policy-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Project project;Check(Project::CreateNew(root.string(),"Dormant fluids",project,error),"create public project fixture");
 Scene scene;auto& object=scene.CreateObject("Dormant basin");object.render=SceneRenderComponent{};object.liquidBasin=LiquidBasinSettings{};object.liquidBasin->geometry=std::string(32,'a');object.liquidBasin->asset=std::string(32,'b');
 project.Settings().startupScene="Scenes/region.judas";Check(SaveSceneToFile(scene,project.StartupScenePath(),error)&&project.Save(error),"save unchanged dormant authoring");
 AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());JobSystem jobs(2);ResourceManager resources(nullptr,&assets,&jobs);
 RuntimeWorld world;world.ConfigureProject(project.Settings());Check(world.Build(Scene{},&resources,error),"disabled world starts without advanced dependencies");
 WorldRegion region;region.id="pool";region.scene=project.Settings().startupScene;PreparedWorldRegion prepared;
 Check(PrepareWorldRegion(region,project,assets,prepared,error)&&prepared.requiredGeometry.empty(),"dormant region requires no advanced geometry or bake");
 Check(prepared.source.Find(object.id)->liquidBasin.has_value(),"prepared region preserves authored fluid component");
 WorldManifest manifest;manifest.regions.emplace(region.id,region);WorldStreaming stream(world,resources,project,manifest);
 auto request=stream.Request("pool",false,error);Check(request!=0,"ordinary region request accepted");
 for(int i=0;i<500&&stream.Status(request)->state!="active";++i){resources.Pump();stream.Advance(false);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 Check(stream.Status(request)->state=="active","dormant region actually publishes through streaming");
 Check(stream.Status(request)->pins.empty(),"disabled conserved authoring creates no physical suspension pin");
 Check(stream.Release(request),"release region demand");
 for(int i=0;i<500&&stream.Stats().active;++i){stream.Advance(false);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 Check(stream.Stats().active==0,"disabled fluid region actually unloads");
 project.Settings().advancedFluidDynamics=true;
 Check(!PrepareWorldRegion(region,project,assets,prepared,error)&&!error.empty(),"opt-in restores required bake/geometry validation");
 project.Settings().advancedFluidDynamics=false;
 ProjectExportOptions options;options.destination=(root.parent_path()/(root.filename().string()+"-package")).string();options.runtimeExecutable=(std::filesystem::path(EngineExecutableDir())/RuntimeExecutableName()).string();ProjectExportResult result;
 error.clear();
 Check(ExportProject(project,options,result,error),"disabled authored basin exports without unregistered advanced assets: "+error);
 Project packaged;Check(packaged.Load((std::filesystem::path(result.packageDirectory)/"game.judasproj").string(),error)&&!packaged.Settings().advancedFluidDynamics,"export preserves disabled project policy");
 Scene exported;Check(LoadSceneFromFile(packaged.StartupScenePath(),exported,error)&&exported.Find(object.id)->liquidBasin.has_value(),"export retains inert authored component");
 project.Settings().advancedFluidDynamics=true;options.destination+="-enabled";
 Check(!ExportProject(project,options,result,error),"enabled export refuses unavailable feature or missing advanced assets");
 if(AdvancedFluidDynamicsRestriction()) {
  Check(error.find("contact stability")!=std::string::npos,"Alpha exporter reports the actual feature restriction before dependency errors");
  Check(!std::filesystem::exists(options.destination),"rejected Alpha export leaves no misleading package");
  Check(project.Settings().advancedFluidDynamics,"export preflight never silently rewrites project policy");
  RuntimeWorld blocked;blocked.ConfigureProject(project.Settings());
  Check(!blocked.Build(Scene{},nullptr,error)&&error.find("contact stability")!=std::string::npos,
        "public runtime construction rejects a hand-authored Alpha opt-in");
  project.Settings().advancedFluidDynamics=false;blocked.ConfigureProject(project.Settings());
  Check(blocked.Build(Scene{},nullptr,error),"correcting policy allows ordinary world construction after rejection");
 }

 std::cout<<"Project fluid policy: "<<checks<<" checks, 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
