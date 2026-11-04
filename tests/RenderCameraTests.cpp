#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include "Renderer.h"
#include "Window.h"
#include "EngineHost.h"
#include <set>
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "SceneFingerprint.h"
#include "SceneSerialization.h"
#include "WorldPresentation.h"
#include "WorldState.h"
#include "InteractivePlay.h"
#include "editor/EditorDocument.h"
#include "ScreenshotWriter.h"
namespace {
int checks=0, failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
Scene Fixture(){
 Scene s;s.Settings().name="Camera surfaces";s.Settings().ambientColor=glm::vec3(1);s.Settings().sunColor=glm::vec3(0);
 auto& red=s.CreateObject("Moving subject");red.transform.position={-1,1,0};red.render=SceneRenderComponent{};
 red.render->color={1,.05f,.02f};red.body=SceneBodyComponent{};red.body->motion=SceneBodyMotion::Dynamic;
 red.body->initialLinearVelocity={.15f,0,0};red.body->pickable=true;
 auto& green=s.CreateObject("Other subject");green.transform.position={1,1,0};green.render=SceneRenderComponent{};green.render->color={.02f,1,.05f};
 auto& c=s.CreateObject("West view");c.transform.position={-1,1,4};c.renderCamera=SceneRenderCameraComponent{};
 c.renderCamera->width=128;c.renderCamera->height=96;c.renderCamera->farPlane=50;
 auto& d=s.CreateObject("East view");d.transform.position={1,1,4};d.renderCamera=SceneRenderCameraComponent{};
 d.renderCamera->width=128;d.renderCamera->height=96;d.renderCamera->farPlane=50;
 auto& screen=s.CreateObject("Display surface");screen.transform.position={0,1,2};screen.render=SceneRenderComponent{};
 screen.render->halfExtents={.7f,.45f,.025f};screen.render->color=glm::vec3(1);screen.render->textureCamera=3;
 auto& player=s.CreateObject("Player start");player.transform.position={0,1,6};player.playerStart=ScenePlayerStartComponent{};
 player.playerStart->view=ScenePlayerView::FirstPerson;
 return s;
}
glm::mat4 View(){return glm::lookAt(glm::vec3(0,2,8),glm::vec3(0,1,0),glm::vec3(0,1,0));}
glm::mat4 Projection(){return glm::perspective(glm::radians(60.f),320.f/240,.1f,50.f);}
void Frame(Renderer& r,RuntimeWorld& w){RenderWorldFrame(r,320,240,w,nullptr,View(),Projection(),{0,1,0},1);}
double Median(std::vector<double> v){std::sort(v.begin(),v.end());return .5*(v[(v.size()-1)/2]+v[v.size()/2]);}
}
int main(int argc,char**argv){
 std::filesystem::path out="build/m33-focused";
 if(argc==3&&std::string(argv[1])=="--demo"){std::string error;return SaveSceneToFile(Fixture(),argv[2],error)?0:2;}
 if(argc==3&&std::string(argv[1])=="--output")out=argv[2];
 std::filesystem::create_directories(out);
 Scene authored=Fixture(),round;std::string text,error;
 SaveSceneToString(authored,text);Check(LoadSceneFromString(text,round,error)&&ScenesEqual(authored,round),"camera configuration and stable texture reference round-trip");
 std::string a,b;ComputeSceneFingerprint(authored,a,error);round.Objects()[2].renderCamera->updateEveryFrames=4;
 ComputeSceneFingerprint(round,b,error);Check(a!=b,"authored cadence enters save compatibility fingerprint");
 Scene bad=authored;bad.Objects()[4].render->textureCamera=999;
 SaveSceneToString(bad,text);Scene unchanged=authored;
 Check(!LoadSceneFromString(text,unchanged,error)&&ScenesEqual(unchanged,authored),"missing camera reference rejected without replacing scene");
 bad=authored;bad.Objects()[2].renderCamera->nearPlane=100;
 RuntimeWorld invalid;Check(!invalid.Build(bad,nullptr,error),"invalid programmatic projection rejected");
 std::set<unsigned int> liveTextures; EngineHost host;
 Check(host.Init("M33 cameras",320,240,false,error,[&](const ResourceTraceEvent& event){
    if(event.point==ResourceTracePoint::TextureCreated)liveTextures.insert(event.handle);
    if(event.point==ResourceTracePoint::TextureDestroyed)liveTextures.erase(event.handle);
 }),"normal EngineHost creates context, loads GL and initializes Renderer");if(failures)return 1;
 Window& window=host.GetWindow();Renderer& renderer=host.GetRenderer();
 {
 TextureData pattern;pattern.width=64;pattern.height=64;pattern.pixels.resize(64*64*4);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x){
 const int k=(y*64+x)*4;pattern.pixels[k]=y<32?(x<32?255:0):(x<32?0:255);
 pattern.pixels[k+1]=x<32?0:255;pattern.pixels[k+2]=y<32?0:(x<32?255:0);pattern.pixels[k+3]=255;
 }
 const auto texture=renderer.CreateTexture(pattern);
 renderer.BeginFrame(320,240);renderer.SetLighting({0,1,0},{0,0,0},{1,1,1});renderer.SetDynamicLights({});
 renderer.SetCamera(glm::lookAt(glm::vec3(0,0,3),glm::vec3(0),glm::vec3(0,1,0)),glm::ortho(-1.f,1.f,-1.f,1.f,.1f,10.f));
 renderer.DrawBox({0,0,0},glm::quat(1,0,0,0),{1,1,.02f},{1,1,1},1,texture);renderer.EndFrame();
 std::vector<unsigned char> pixels;renderer.CaptureFrame(320,240,pixels);
 bool correct=true;
 for(int y:{24,90,150,216})for(int x:{32,120,200,288}){
 const int k=(y*320+x)*3;const int r=y>=120?(x<160?255:0):(x<160?0:255);
 const int g=x<160?0:255,b=y>=120?0:(x<160?255:0);
 correct=correct&&std::abs(int(pixels[k])-r)<3&&std::abs(int(pixels[k+1])-g)<3&&std::abs(int(pixels[k+2])-b)<3;
 }
 Check(correct,"ordinary textured box face preserves all four quadrants across both triangles");
 WriteRgbPng((out/"box-uv.png").string(),320,240,pixels);renderer.DestroyTexture(texture);
 }
 renderer.BeginFrame(320,240);auto state=renderer.RenderTargetDiagnostics();
 auto target=renderer.CreateRenderTarget(73,59,error);Check(target.IsValid()&&error.empty(),"complete target at requested dimensions");
 TextureData pixels;Check(renderer.ReadTextureForDiagnostics(renderer.RenderTargetTexture(target),pixels)&&pixels.width==73&&pixels.height==59,"generated colour texture uses ordinary texture readback");
 Check(renderer.BeginRenderTarget(target),"target pass binds successfully");
 Check(renderer.RenderTargetDiagnostics().viewport==glm::ivec4(0,0,73,59),"target viewport uses target dimensions");
 renderer.EndRenderTarget();auto restored=renderer.RenderTargetDiagnostics();
 Check(restored.viewport==state.viewport&&restored.defaultFramebuffer==state.defaultFramebuffer&&!restored.passActive,"target pass restores framebuffer and viewport");
 auto old=renderer.RenderTargetTexture(target);Check(renderer.ResizeRenderTarget(target,91,47,error),"target resize succeeds");
 Check(!renderer.ReadTextureForDiagnostics(old,pixels)&&renderer.RenderTargetSize(target)==glm::ivec2(91,47),"resize releases old colour and updates dimensions");
 renderer.DestroyRenderTarget(target);
 Check(renderer.RenderTargetDiagnostics().liveTargets==0&&renderer.Stats().targetDeletionFailures==0,"GPU framebuffer, depth and colour are deleted");
 Check(!renderer.CreateRenderTarget(0,20,error).IsValid()&&!error.empty(),"invalid target reports error");
 {
 RuntimeWorld world;Check(world.Build(authored,nullptr,error),"ordinary runtime constructs camera scene without disk asset manager");
 renderer.ResetStats();EntityPhysicalState before,after;world.GetEntityState(1,before);Frame(renderer,world);world.GetEntityState(1,after);
 Check(before.position==after.position&&before.linearVelocity==after.linearVelocity,"secondary/main views do not advance authoritative body state");
 TextureData west,east;const auto westHandle=world.CameraTexture(3);
 Check(renderer.ReadTextureForDiagnostics(westHandle,west)&&west.width==128&&west.height==96,"actual world render writes target pixels at authored size");
 Check(renderer.ReadTextureForDiagnostics(world.CameraTexture(4),east)&&west.pixels!=east.pixels,"two authored camera views produce different pixels");
 std::size_t coloured=0;
 for(std::size_t i=0;i+3<west.pixels.size();i+=4) if(west.pixels[i]>120&&west.pixels[i+1]<80&&west.pixels[i+2]<80)++coloured;
 Check(coloured>10,"target receives non-clear rendered pixel content");
 Check(renderer.Stats().offscreenPasses==2&&renderer.Stats().feedbackFallbacks>0,"real draws detect and safely replace active-target feedback");
 std::vector<unsigned char> mainPixels;renderer.CaptureFrame(320,240,mainPixels);
 Check(WriteRgbPng((out/"main.png").string(),320,240,mainPixels),"actual main framebuffer screenshot recorded");
 Check(std::count_if(mainPixels.begin(),mainPixels.end(),[](unsigned char v){return v>100;})>100,"main framebuffer renders after secondary passes");
 Check(renderer.RenderTargetDiagnostics().defaultFramebuffer&&renderer.RenderTargetDiagnostics().viewport==glm::ivec4(0,0,320,240),"main pass restores its own viewport/default framebuffer");
 EntityPhysicalState moved=before;moved.position.z+=1;
 Check(world.SetEntityState(1,moved),"ordinary runtime body motion supplied");Frame(renderer,world);TextureData movedPixels;renderer.ReadTextureForDiagnostics(westHandle,movedPixels);
 Check(movedPixels.pixels!=west.pixels,"secondary texture observes the same authoritative body motion");
 world.SetEntityState(1,before);
 world.PresentationCameras()[0].transform.position.x=2;
 Frame(renderer,world);TextureData changed;renderer.ReadTextureForDiagnostics(westHandle,changed);
 Check(changed.pixels!=west.pixels,"moving runtime camera changes actual generated pixels");
 // Target source is sampled by the ordinary main mesh: disabling just that
 // material reference changes pixels without changing the camera or geometry.
 world.PresentationCameras()[0].transform.position.x=-1;
 Scene plain=authored;plain.Objects()[4].render->textureCamera=0;
 RuntimeWorld other;Check(other.Build(plain,nullptr,error),"untextured comparison world builds");
 Frame(renderer,other);std::vector<unsigned char> plainPixels;renderer.CaptureFrame(320,240,plainPixels);
 Frame(renderer,world);std::vector<unsigned char> textured;renderer.CaptureFrame(320,240,textured);
 Check(plainPixels!=textured,"ordinary screen mesh consumes generated target texture");
 other.Destroy();
 world.PresentationCameras()[0].settings.width=64;Frame(renderer,world);
 Check(renderer.RenderTargetSize(world.PresentationCameras()[0].target)==glm::ivec2(64,96),"runtime target resolution recreation is integrated");
 world.Destroy();Check(!renderer.ReadTextureForDiagnostics(westHandle,pixels)&&renderer.RenderTargetDiagnostics().liveTargets==0,"world teardown releases borrowed textures and all targets");
 }
 {
 const auto assetsPath=std::filesystem::absolute(out/"resize-assets");
 std::filesystem::create_directories(assetsPath);
 const auto scriptPath=assetsPath/"resize.js";
 std::ofstream(scriptPath)<<R"JS(export default class {
   constructor({entity}) {this.entity=entity;this.state={};}
   fixedUpdate() {
      this.state.applied=this.entity.setCameraTargetSize(80,48);
      try {this.entity.setCameraTargetSize(80,2.5);}
      catch {this.state.rejected=true;}
      this.state.camera=this.entity.camera;
   }
 })JS";
 AssetDatabase assets;
 assets.Scan(assetsPath.parent_path().string(),assetsPath.string());
 AssetRecord script;
 Check(assets.Track(scriptPath.string(),script,error),"register scripted resize fixture");
 ResourceManager resources(nullptr,&assets);
 auto scene=authored;
 scene.Find(3)->scripts.push_back({1,script.id,true,"{}"});
 RuntimeWorld world;
 Check(world.Build(scene,&resources,error),"scripted secondary camera builds");
 Frame(renderer,world);
 const auto beforeResize=world.CameraTexture(3);
 world.FixedScripts(nullptr,1.f/60);
 const auto observations=world.Scripts()->Capture();
 const auto pending=observations.empty()?std::string{}:observations.front().json;
 Check(pending.find("\"applied\":true")!=std::string::npos&&
       pending.find("\"rejected\":true")!=std::string::npos&&
       pending.find("\"targetWidth\":128")!=std::string::npos,
       "script resize validates atomically and keeps the prior allocation until rendering");
 Frame(renderer,world);
 const auto afterResize=world.CameraTexture(3);
 TextureData resized;
 Check(!renderer.ReadTextureForDiagnostics(beforeResize,resized)&&
       renderer.ReadTextureForDiagnostics(afterResize,resized)&&resized.width==80&&resized.height==48&&
       renderer.RenderTargetDiagnostics().liveTargets==2,
       "new target has requested pixels and replaces the old GPU allocation");
 world.FixedScripts(nullptr,1.f/60);
 Frame(renderer,world);
 Check(world.CameraTexture(3).id==afterResize.id,"same scripted size keeps the live target");
 auto& camera=world.PresentationCameras().front();
 camera.settings.width=1000000; // Inject a guaranteed driver-size rejection.
 const auto updates=camera.updates;
 Frame(renderer,world);
 Check(camera.updates==updates+1&&!camera.error.empty()&&camera.targetSize==glm::ivec2(80,48)&&
       world.CameraTexture(3).id==afterResize.id,"failed allocation preserves and renders the last good target");
 world.Destroy();
 Check(renderer.RenderTargetDiagnostics().liveTargets==0,"scripted targets are cleaned up on world destruction");
 }
 {
 Scene scene=authored;scene.Objects()[0].renderCamera=SceneRenderCameraComponent{};
 RuntimeWorld source,destination;Check(source.Build(scene,nullptr,error)&&destination.Build(scene,nullptr,error),"camera lifecycle worlds share authored baseline");
 Frame(renderer,source);Check(source.CameraTexture(1).IsValid(),"body-attached camera allocates its runtime target");
 Check(source.DestroyEntity(1,&error)&&!source.CameraTexture(1).IsValid(),"destroyed camera releases target and resolves fallback");
 SceneObject consumer=scene.Objects()[4];consumer.id=kInvalidSceneObjectId;consumer.body=SceneBodyComponent{};
 consumer.body->motion=SceneBodyMotion::Dynamic;consumer.render->textureCamera=1;
 Check(source.CreateEntity(consumer,nullptr,&error)!=kInvalidSceneObjectId,"runtime consumer may retain stable reference to destroyed authored camera");
 const WorldState delta=CaptureWorldState(source);
 Check(ApplyWorldState(destination,delta,error)&&destination.FindEntity(1)->lifecycle==EntityLifecycle::Destroyed,"mixed destroy/create camera delta applies through shared persistence preflight");
 consumer.render->textureCamera=999;
 Check(!destination.ValidateEntityCreation(consumer,error),"runtime consumer rejects nonexistent camera definition");
 }
 {
 EditorDocument doc;doc.GetScene()=authored;const Scene baseline=doc.GetScene();
 doc.BeginEdit();doc.GetScene().Objects()[2].renderCamera->updateEveryFrames=4;doc.CommitEdit();doc.Undo();
 Check(ScenesEqual(doc.GetScene(),baseline),"camera editor undo restores authored configuration");doc.Redo();doc.Undo();
 RuntimeWorld world;Check(world.Build(doc.GetScene(),nullptr,error),"Play world builds from editor document");
 InteractivePlay play;window.SetTestInputMode(true);Check(play.Begin(world,WorldCoordinates{},error),"same InteractivePlay runtime path starts");
 play.Frame(window,renderer,1.f/60,false);play.Frame(window,renderer,1.f/60,false);
 Check(world.PresentationCameras()[0].updates>0,"actual InteractivePlay frame renders secondary camera");
 play.End();world.Destroy();Check(ScenesEqual(doc.GetScene(),baseline),"Play/Stop leaves authored camera scene unchanged");
 }
 for(int cadence:{0,1,2,4}){
 Scene scene=authored;scene.Objects()[3].renderCamera->enabled=false;
 scene.Objects()[2].renderCamera->enabled=cadence!=0;scene.Objects()[2].renderCamera->updateEveryFrames=std::max(1,cadence);
 RuntimeWorld world;Check(world.Build(scene,nullptr,error),"cadence fixture builds through ordinary runtime");
 renderer.ResetStats();std::vector<double> times;
 for(int i=0;i<40;++i){renderer.FinishForDiagnostics();const auto begin=std::chrono::steady_clock::now();Frame(renderer,world);renderer.FinishForDiagnostics();times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());}
 Check(renderer.Stats().offscreenPasses==static_cast<unsigned int>(cadence?40/cadence:0),"authored cadence/disabled state schedules exact render count");
 double mean=0;for(double t:times)mean+=t/40;
 std::printf("PERFORMANCE cadence=%d frames=40 cpu_render_median_ms=%.6f cpu_render_mean_ms=%.6f first_frame_ms=%.6f draw_calls=%u offscreen_passes=%u\n",cadence,Median(times),mean,times.front(),renderer.Stats().drawCalls,renderer.Stats().offscreenPasses);
 }
 Check(renderer.RenderTargetDiagnostics().liveTargets==0&&renderer.Stats().targetDeletionFailures==0,"all fixture resources deleted without GL object leaks");
 host.Shutdown();Check(liveTextures.empty(),"actual generated/asset texture creation and destruction operations balance");
 std::printf("SUMMARY checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
