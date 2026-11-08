#include "RuntimeUI.h"
#include "InputSystem.h"
#include "EngineHost.h"
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
 fs::path out="build/runtime-ui-tests";
 for (int i=1;i<argc;++i) {
     const std::string option=argv[i];
     if (i+1==argc||option!="--output") {
         std::fprintf(stderr,"usage: judas_runtime_ui_tests [--output DIRECTORY]\n");
         return 2;
     }
     out=argv[++i];
 }
 fs::create_directories(out);
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
 // Core document rendering is independent of a historical game's controller.
 // Native script ownership, menu clicks and cold publication are exercised by
 // UIMenuIntegrationTests against the current Satchel menu project.
 EngineHost host;Check(host.Init("Runtime UI document",640,360,false,error),"real engine host and GL context");if(failures)return 1;
 auto& r=host.GetRenderer();r.BeginFrame(640,360);r.EndFrame();std::vector<unsigned char> beforePixels;r.CaptureFrame(640,360,beforePixels);
 ui.Layout(640,360);r.BeginUIFrame(640,360);ui.Draw(r,640,360);r.EndUIFrame();std::vector<unsigned char> pixels;r.CaptureFrame(640,360,pixels);
 Check(pixels!=beforePixels,"normal clipped UI document changes actual framebuffer pixels");
 Check(WriteRgbPng((out/"document.png").string(),640,360,pixels),"UI framebuffer capture succeeds");
 Check(ui.Unload(next)&&ui.Empty(),"document retirement releases the final UI owner");
 RuntimeUI perf;auto hud=d;hud.modal=false;while(hud.elements.size()<100){UIElement e;e.id="text"+std::to_string(hud.elements.size());e.parent="canvas";e.kind=UIKind::Text;e.text="HUD";hud.elements.push_back(e);}perf.Add(hud,"perf",0,error);
 auto start=std::chrono::steady_clock::now();for(int i=0;i<100;++i)perf.Layout(1280,720);auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/100;
 r.BeginUIFrame(640,360);perf.Draw(r,640,360);r.EndUIFrame();std::printf("PERFORMANCE elements=100 layout_us=%.3f render_us=%.3f submissions=%u\n",us,perf.Stats().renderUs,perf.Stats().draws);
 host.Shutdown();std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
