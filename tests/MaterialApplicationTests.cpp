// Real application/presentation integration; assets remain ordinary project content.
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "WorldPresentation.h"
#include "SceneSerialization.h"
#include "Project.h"
#include "GameSession.h"
#include "Simulation.h"
#include "SceneFingerprint.h"
#include "ScreenshotWriter.h"
#include "PerformanceProfiler.h"
#include "Prefab.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
namespace fs=std::filesystem;
unsigned checks=0,failures=0;void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
struct Options {
 fs::path output="build/m57-application", sourceRoot=fs::current_path();
 std::string mode;
};
Options Parse(int argc,char** argv){
 Options result;std::vector<std::string> positional;bool sourceSpecified=false;
 for(int i=1;i<argc;++i){
  const std::string word=argv[i];
  if(word=="--source-root"){
   if(sourceSpecified||i+1>=argc)throw std::runtime_error("Specify --source-root CHECKOUT once");
   result.sourceRoot=argv[++i];sourceSpecified=true;
  }else if(word.rfind("--",0)==0)throw std::runtime_error("Unknown option: "+word);
  else positional.push_back(word);
 }
 if(positional.size()>3)throw std::runtime_error("Usage: judas_material_application_tests [OUTPUT] [quick|desktop] [SOURCE_ROOT] [--source-root CHECKOUT]");
 if(!positional.empty())result.output=positional[0];
 if(positional.size()>1)result.mode=positional[1];
 if(positional.size()>2){
  if(sourceSpecified)throw std::runtime_error("Source root was supplied twice");
  result.sourceRoot=positional[2];
 }
 if(!result.mode.empty()&&result.mode!="quick"&&result.mode!="desktop")throw std::runtime_error("Test mode must be quick or desktop");
 result.output=fs::absolute(result.output);result.sourceRoot=fs::absolute(result.sourceRoot);
 for(const auto* name:{"material_lab","liquid_surface_demo","shooter_game"}){
  if(result.mode=="quick"&&std::string(name)=="shooter_game")continue;
  const auto project=result.sourceRoot/"projects"/name/(std::string(name)+".judasproj");
  if(!fs::is_regular_file(project))throw std::runtime_error("Required project fixture is missing: "+project.string()+". Run from the checkout or pass --source-root CHECKOUT; fixtures are copied into OUTPUT before use.");
 }
 return result;
}
fs::path CopyFixture(const Options& options,const char* name,const char* destination){
 const auto source=options.sourceRoot/"projects"/name;
 const auto target=options.output/destination;
 // Never replace authored fixtures, including when OUTPUT was chosen inside
 // a project. Only the dedicated generated copy beneath OUTPUT is disposable.
 const auto canonicalSource=fs::weakly_canonical(source),canonicalTarget=fs::weakly_canonical(target);
 const auto inside=[](const fs::path& child,const fs::path& parent){auto c=child.begin();for(auto p=parent.begin();p!=parent.end();++p,++c)if(c==child.end()||*c!=*p)return false;return true;};
 if(inside(canonicalTarget,canonicalSource)||inside(canonicalSource,canonicalTarget))throw std::runtime_error("Fixture output overlaps authored source: "+target.string());
 fs::remove_all(target);fs::create_directories(target);
 fs::copy(source,target,fs::copy_options::recursive|fs::copy_options::overwrite_existing);
 return target;
}
int Run(int argc,char** argv){const auto options=Parse(argc,argv);const auto out=options.output;fs::create_directories(out);const bool desktop=options.mode=="desktop";const auto fixture=CopyFixture(options,"material_lab","lab-fixture");const auto liquidFixture=CopyFixture(options,"liquid_surface_demo","liquid-fixture");const auto shooterFixture=options.mode=="quick"?fs::path{}:CopyFixture(options,"shooter_game","shooter-fixture");std::string error;EngineHost host;Check(host.Init("M57 integration",640,360,false,error),"ordinary host");if(failures)return 1;host.Audio().Init(error,true);auto& r=host.GetRenderer();
 Project lab;Check(lab.Load((fixture/"material_lab.judasproj").string(),error),"material lab project registered");if(failures){std::puts(error.c_str());return 1;}host.OpenProjectAssets(lab.RootDir(),lab.Resolve("Assets"));Scene scene,round;Check(LoadSceneFromFile(lab.StartupScenePath(),scene,error),"material lab scene loads");if(failures){std::puts(error.c_str());return 1;}std::string authored;SaveSceneToString(scene,authored);Check(LoadSceneFromString(authored,round,error)&&ScenesEqual(scene,round),"scene/material/environment round trip");
 RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(scene,&host.Resources(),error,&lab.Settings().classification,&lab.Settings().navigation),"lab world uses normal resources");host.Resources().WaitForAll();GameSession game;Check(game.Begin(world,error),"lab game begins");auto& window=host.GetWindow();window.Input().SetMap(lab.Settings().input,error);window.SetTestInputMode(true);
 for(int i=0;i<6;++i){world.UpdateScripts(&window.Input(),1.f/60);window.Input().BeginFixedStep();StepPlayedWorld(game,window,1.f/60);world.PresentationScripts(&window.Input(),1.f/60,.5f);host.Resources().WaitForAll();}
 Check(world.Scripts()->Diagnostics().empty(),"lab script uses public API without faults");auto* overridden=world.RuntimeDefinition(40);auto* neighbour=world.RuntimeDefinition(41);Check(overridden->render->runtimeMaterials.at("#0").overrides.baseColor.has_value()&&neighbour->render->runtimeMaterials.empty(),"JS edits one shared-material instance only");
 auto spawned=world.SpawnPrefab("44914741099d012af2c3eca432018951",SceneTransform{{0,0,7},{1,0,0,0},{1,1,1}},error);Check(spawned&&world.RuntimeDefinition(spawned)->render->materials.size()==1,"prefab-spawned material/skinned instance");
 Check(world.ClearRuntimeMaterial(40,"#0")&&world.RuntimeDefinition(40)->render->runtimeMaterials.empty(),"runtime revert restores shared parameters");auto matid=scene.Find(41)->render->materials[0].asset;Check(host.Resources().TryGetMaterial(matid).IsValid(),"async material uploaded");Check(host.Resources().DecodeThreadOf(matid)!=std::this_thread::get_id(),"material decode uses worker");
 auto envId=scene.Settings().environmentAsset;Check(host.Resources().TryGetEnvironment(envId).IsValid(),"derived environment uploaded");auto meshId=scene.Find(34)->render->meshAsset;auto mesh=host.Resources().TryGetMeshMaterial(meshId,0),mesh2=host.Resources().TryGetMeshMaterial(meshId,1);Check(mesh&&mesh2&&mesh->metallic!=mesh2->metallic,"embedded static mesh retains two imported slots");
 auto camera=world.view->pose;auto view=glm::mat4_cast(glm::inverse(camera.rotation))*glm::translate(glm::mat4(1),-camera.position);auto proj=glm::perspective(glm::radians(60.f),640.f/360,.1f,100.f);
 RenderWorldFrame(r,640,360,world,&game,view,proj,camera.position,.5f);r.BeginUIFrame(640,360);world.UI().Draw(r,640,360);r.EndUIFrame();std::vector<unsigned char> image;r.CaptureFrame(640,360,image);WriteRgbPng((out/"lab.png").string(),640,360,image);Check(r.Stats().feedbackFallbacks>0,"lab screen retains camera feedback protection");auto baseline=world.BaselineFingerprint();auto bytes=r.AppearanceBytes();for(int i=0;i<3;++i)RenderWorldFrame(r,640,360,world,&game,view,proj,camera.position,.5f);Check(r.AppearanceBytes()==bytes,"main/secondary dimensions reuse HDR targets");Check(world.CameraTexture(50).IsValid()&&r.Stats().offscreenPasses>=4,"actual M33 material screen rendered");Check(glGetError()==GL_NO_ERROR,"lab skinned/static/secondary pass has no GL errors");
 host.Resources().Release(matid);host.Resources().RequestMaterial(matid);host.Resources().Release(matid);host.Resources().WaitForAll();Check(host.Resources().StateOf(matid)==ResourceState::Unloaded,"in-flight material cancellation installs no stale resource");host.Resources().Invalidate(matid);host.Resources().RequestMaterial(matid);host.Resources().WaitForAll();Check(host.Resources().TryGetMaterial(matid).IsValid(),"material source reload through normal resource workflow");game.End();world.Destroy();Check(!world.Scripts()&&world.PresentationCameras().empty(),"Stop clears scripts and camera ownership");std::string after;SaveSceneToString(scene,after);Check(after==authored,"authored scene untouched by runtime edits");

 auto relocated=fs::absolute(out/"relocated-lab");fs::remove_all(relocated);fs::copy(fixture,relocated,fs::copy_options::recursive);host.OpenProjectAssets(relocated.string(),(relocated/"Assets").string());RuntimeWorld moved;moved.legacyGameplay=false;Check(moved.Build(scene,&host.Resources(),error,&lab.Settings().classification,&lab.Settings().navigation)&&moved.BaselineFingerprint()==baseline,"material fingerprint remains path independent");moved.Destroy();auto* record=host.Assets().Find(matid);MaterialDefinition changed;Check(record&&LoadMaterial(record->path,changed,error),"shared source readable");changed.roughness=.85f;Check(SaveMaterial(record->path,changed,error),"isolated shared-source edit");Check(moved.Build(scene,&host.Resources(),error,&lab.Settings().classification,&lab.Settings().navigation)&&moved.BaselineFingerprint()!=baseline,"material source bytes change authored fingerprint");moved.Destroy();
 // Registered material/environment declarations cover assignments made later by JS.
 Scene legacy=scene;legacy.Settings().linearRendering=false;legacy.Settings().environmentAsset.clear();for(auto& o:legacy.Objects())if(o.render)o.render->materials.clear();
 Check(moved.Build(legacy,&host.Resources(),error,&lab.Settings().classification,&lab.Settings().navigation),"legacy-mode declared-material scene builds");auto declared=moved.BaselineFingerprint();moved.Destroy();changed.roughness=.87f;Check(SaveMaterial(record->path,changed,error)&&moved.Build(legacy,&host.Resources(),error,&lab.Settings().classification,&lab.Settings().navigation)&&moved.BaselineFingerprint()!=declared,"runtime-assignable declared source is fingerprinted before assignment");moved.Destroy();
 // Actual accepted M55 geometry supplies optical paths on both sides. No simulation changes.
 Project liquid;Check(liquid.Load((liquidFixture/"liquid_surface_demo.judasproj").string(),error),"M55 project loads unchanged");host.OpenProjectAssets(liquid.RootDir(),liquid.Resolve("Assets"));Scene water;Check(LoadSceneFromFile(liquid.StartupScenePath(),water,error),"M55 scene loads");RuntimeWorld wet;wet.legacyGameplay=false;wet.ConfigureProject(liquid.Settings());Check(wet.Build(water,&host.Resources(),error,&liquid.Settings().classification,&liquid.Settings().navigation),"M55 world builds");host.Resources().WaitForAll();wet.UpdateLiquids(1./60);auto settings=wet.Settings();settings.linearRendering=true;wet.SetAppearance(settings);
 for(int i=0;i<2;++i){glm::vec3 eye=i?glm::vec3(0,1,-10):glm::vec3(0,6,2),target(0,1,-18);auto v=glm::lookAt(eye,target,glm::vec3(0,1,0)),p=glm::perspective(glm::radians(70.f),640.f/360,.1f,100.f);auto paths=wet.Liquids().OpticalPaths(v,p,32,18,.5f);unsigned crossed=0;for(auto pair:paths)crossed+=pair.y>pair.x;Check(crossed>0,"actual above/below camera intersects liquid boundary paths");RenderWorldFrame(r,640,360,wet,nullptr,v,p,eye,.5f);r.CaptureFrame(640,360,image);WriteRgbPng((out/(i?"liquid-below.png":"liquid-above.png")).string(),640,360,image);Check(glGetError()==GL_NO_ERROR,"M55 optical composition and modern resolve agree");}
 Check(wet.Liquids().Errors().empty(),"M55 accounting/surface has no errors");wet.Destroy();host.Shutdown();
 // M56 collects the same real workload, fixed cadence/input/resolution in each mode.
 for(int mode=0;mode<(options.mode=="quick"?0:3);++mode){auto& profiler=PerformanceProfiler::Get();profiler.Freeze(false);profiler.Clear();setenv("JUDAS_PROFILE","1",1);setenv("JUDAS_WORLD_STATE","none",1);setenv("JUDAS_PROFILE_SCRIPT_ATTRIBUTION","1",1);unsetenv("JUDAS_PROFILE_OUTPUT");unsigned frame=0;ApplicationControl control;control.hidden=!desktop;control.frameSeconds=[](float){return 1.f/60;};control.hostReady=[&](EngineHost& h){SDL_GL_SetSwapInterval(0);std::printf("PROFILE hardware %s renderer %s dimensions %dx%d\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER),h.GetWindow().Width(),h.GetWindow().Height());};control.beforeFrame=[](EngineHost& h,RuntimeWorld&,InteractivePlay&){h.GetRenderer().ResetStats();};control.worldReady=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay&){h.Resources().WaitForAll();if(mode<2){auto s=w.Settings();s.environmentAsset.clear();s.linearRendering=mode==1;s.environmentBackground=false;w.SetAppearance(s);}if(mode==0){for(auto& o:w.ScriptObjects())if(o.render){for(unsigned slot=0;slot<o.render->materials.size();++slot)w.SetMaterialSlot(o.id,slot,{});}}std::printf("PROFILE bodies %zu cameras %zu\n",w.CountLifecycle().physicsBodies,w.PresentationCameras().size());};
 control.afterFrame=[&](EngineHost& h,RuntimeWorld& w,InteractivePlay& play){++frame;if(frame==90){std::vector<unsigned char> capture;h.GetRenderer().CaptureFrame(h.GetWindow().Width(),h.GetWindow().Height(),capture);WriteRgbPng((out/("range-"+std::to_string(mode)+".png")).string(),h.GetWindow().Width(),h.GetWindow().Height(),capture);std::printf("PROFILE mode %d fixed %.6f appearance bytes %zu draws %u\n",mode,play.LastFixedStepMilliseconds(),h.GetRenderer().AppearanceBytes(),h.GetRenderer().Stats().drawCalls);}if(frame==150){Check(w.Scripts()->Diagnostics().empty(),"Spring Range scripts fault-free in comparable profile");w.UI().RequestQuit();}};
 control.beforeShutdown=[&](EngineHost&,RuntimeWorld&,InteractivePlay&){Check(profiler.Export((out/("range-"+std::to_string(mode)+"-profile.json")).string(),error),"M56 structured render profile exported");};std::string project=(shooterFixture/"shooter_game.judasproj").string();char* args[]={const_cast<char*>("judas"),project.data()};Application app;Check(app.Run(2,args,&control)==0&&frame==150,"Spring Range real application startup/render/Stop");}
 std::printf("M57 integration %u checks %u failures\n",checks,failures);return failures?1:0;}
int main(int argc,char** argv){
 try{return Run(argc,argv);}
 catch(const std::exception& error){std::fprintf(stderr,"Material application validation could not run: %s\n",error.what());return 2;}
}
