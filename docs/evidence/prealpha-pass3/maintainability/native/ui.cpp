#include "RuntimeUI.h"
#include "RuntimeWorld.h"
#include "InputSystem.h"
#include "AssetDatabase.h"
#include "ResourceManager.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "Project.h"
#include "ProjectExporter.h"
#include "ScreenshotWriter.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cstdio>
#include <algorithm>
namespace fs=std::filesystem;
int checks=0,failures=0;
void Check(bool ok,const char* text){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
int main(int argc,char** argv){
 fs::path out="build/m41-ui-tests";if(argc==3&&std::string(argv[1])=="--output")out=argv[2];fs::create_directories(out);
 std::string error;UIDocument d;UIElement root;root.id="canvas";root.kind=UIKind::Canvas;d.elements.push_back(root);d.modal=true;
 UIElement panel;panel.id="panel";panel.parent="canvas";panel.anchorMin=panel.anchorMax={.5,.5};panel.size={300,220};panel.align={.5,.5};panel.padding={10,10,10,10};panel.flow=UIFlow::Vertical;panel.clip=true;panel.background={.1,.2,.3,1};d.elements.push_back(panel);
 for(auto name:{"button","toggle","slider"}){UIElement e;e.id=name;e.parent="panel";e.kind=std::string(name)=="button"?UIKind::Button:std::string(name)=="toggle"?UIKind::Toggle:UIKind::Slider;e.size={260,44};e.text=name;e.background={.2,.3,.4,1};d.elements.push_back(e);}
 UIDocument decoded;Check(ParseUIDocument(SerializeUIDocument(d),decoded,error)&&SerializeUIDocument(decoded)==SerializeUIDocument(d),"UI document round-trip");
 auto bad=d;bad.elements.back().id="button";Check(!bad.Validate(error),"duplicate stable IDs rejected");
 RuntimeUI ui;auto handle=ui.Add(d,"menu",0,error);Check(handle&&ui.Find("menu")==handle&&ui.Element(handle,"toggle"),"stable lookup");ui.Layout(1280,720);auto a=ui.LayoutOf(handle,"panel")->rect;ui.Layout(640,360);auto b=ui.LayoutOf(handle,"panel")->rect;Check(glm::length(b.position-a.position*.5f)<.01f&&glm::length(b.size-a.size*.5f)<.01f,"reference-resolution anchors at two sizes");
 InputSystem input;ui.Layout(1280,720);auto rect=ui.LayoutOf(handle,"button")->rect;auto point=rect.position+rect.size*.5f;input.SetPhysical("mouse:Left",1);ui.Input(input,point,true,1280,720);input.BeginFrame();input.SetPhysical("mouse:Left",0);ui.Input(input,point,true,1280,720);auto events=ui.TakeEvents();Check(std::any_of(events.begin(),events.end(),[](const auto& e){return e.type=="click"&&e.element=="button";}),"real pointer press/release click");
 input.Reset();input.SetPhysical("pad:DpadDown",1);ui.Input(input,{-1,-1},false,1280,720);input.BeginFrame();input.SetPhysical("pad:South",1);ui.Input(input,{-1,-1},false,1280,720);events=ui.TakeEvents();Check(ui.Element(handle,"toggle")->value==1&&std::any_of(events.begin(),events.end(),[](const auto& e){return e.type=="change";}),"controller focus and toggle activation");input.BeginFixedStep();Check(!input.Action("jump").pressed&&!input.FixedAction("jump").pressed&&!input.Action("jump").held,"shared-binding consumption blocks gameplay and fixed input");
 ui.Element(handle,"button")->enabled=false;ui.Element(handle,"toggle")->visible=false;input.Reset();input.SetPhysical("key:Return",1);ui.Input(input,point,true,1280,720);events=ui.TakeEvents();Check(std::none_of(events.begin(),events.end(),[](const auto& e){return e.type=="click";}),"hidden/disabled controls do not activate");
 Check(ui.Unload(handle)&&!ui.Document(handle),"unload invalidates handles");auto next=ui.Add(d,"menu",0,error);Check(next!=handle,"unloaded handle never revives");
 // Real GL rendering and ordinary world/JS/editor-compatible session.
 EngineHost host;Check(host.Init("M41 tests",640,360,false,error),"real engine host and GL context");if(failures){std::printf("INIT %s\n",error.c_str());return 1;}
 Project project;Check(project.Load("projects/ui_demo/ui_demo.judasproj",error),"UI demo project loads");AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());Check(assets.Problems().empty(),"normal UI asset registry");
 ResourceManager resources(&host.GetRenderer(),&assets);resources.SetBlockingMode(true);Scene scene;Check(LoadSceneFromFile(project.StartupScenePath(),scene,error),"scene UI/script component load");
 RuntimeWorld world;Check(world.Build(scene,&resources,error,&project.Settings().classification),"normal world constructs UI document");if(!world.IsBuilt()){std::printf("ERROR %s\n",error.c_str());return 1;}
 input.SetMap(project.Settings().input,error);world.UpdateUIScripts(&input,1.f/60);auto h=world.UI().Find("game_ui");Check(h&&world.UI().Paused(),"authored main menu modal");
 auto& r=host.GetRenderer();r.BeginFrame(640,360);r.EndFrame();std::vector<unsigned char> beforePixels;r.CaptureFrame(640,360,beforePixels);r.BeginUIFrame(640,360);world.UI().Draw(r,640,360);r.EndUIFrame();std::vector<unsigned char> pixels;r.CaptureFrame(640,360,pixels); // screenshot readback uses normal renderer API below
 Check(pixels!=beforePixels,"UI draws modify actual GL framebuffer pixels");
 Check(WriteRgbPng((out/"menu.png").string(),640,360,pixels),"actual GL UI pixels captured");
 world.UI().Layout(640,360);point=world.UI().LayoutOf(h,"start")->rect.position+world.UI().LayoutOf(h,"start")->rect.size*.5f;
 input.Reset();input.SetPhysical("mouse:Left",1);world.UI().Input(input,point,true,640,360);input.BeginFrame();input.SetPhysical("mouse:Left",0);world.UI().Input(input,point,true,640,360);world.DispatchUIEvents(&input,.016f);std::printf("TRACE_UI %s\n",world.UI().CaptureState(h).c_str());Check(!world.UI().Paused()&&world.UI().Element(h,"hud")->visible,"JS receives click and starts HUD");
 auto* spawner=world.RuntimeDefinition(8);(void)spawner;world.UpdateScripts(&input,.016f);Check(world.UI().Element(h,"counter")->text.find("Spawned props:")==0,"JS modifies HUD text");
 input.BeginFrame();input.SetPhysical("key:Escape",1);world.UpdateUIScripts(&input,.016f);Check(world.UI().Paused()&&world.UI().Element(h,"pause")->visible,"JS migrates pause menu to authored panel");
 world.UI().Input(input,{-1,-1},false,640,360);world.DispatchUIEvents(&input,.016f);std::printf("TRACE_UI %s\n",world.UI().CaptureState(h).c_str()); // same opening edge should not close again (script checks below)
 // Preserve the authored scene through runtime mutation and Stop.
 std::string source;SaveSceneToString(scene,source);world.EndScripts();Check(!world.UIIfLoaded()&&([&](){std::string after;SaveSceneToString(scene,after);return after==source;})(),"Stop releases UI/VM and leaves authored scene unchanged");
 RuntimeUI perf;auto hud=d;hud.modal=false;while(hud.elements.size()<100){UIElement e;e.id="text"+std::to_string(hud.elements.size());e.parent="canvas";e.kind=UIKind::Text;e.text="HUD";hud.elements.push_back(e);}perf.Add(hud,"perf",0,error);
 auto start=std::chrono::steady_clock::now();for(int i=0;i<100;++i)perf.Layout(1280,720);auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/100;
 r.BeginUIFrame(640,360);perf.Draw(r,640,360);r.EndUIFrame();std::printf("PERFORMANCE elements=100 layout_us=%.3f render_us=%.3f submissions=%u\n",us,perf.Stats().renderUs,perf.Stats().draws);
 world.Destroy();resources.Shutdown();host.Shutdown();std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
