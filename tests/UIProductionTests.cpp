#include "RuntimeUI.h"
#include "TextLayout.h"
#include "InputSystem.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "Window.h"
#include "LocalMultiplayer.h"
#include "Renderer.h"
#include "ScreenshotWriter.h"
#include "NamedAuthoring.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "AuthoringDocument.h"
#include <fstream>
#include <SDL2/SDL.h>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <algorithm>
int checks=0,failures=0;
void Check(bool ok,const char* name){++checks;if(!ok)++failures;printf("%s %s\n",ok?"PASS":"FAIL",name);}
UIDocument Fixture(){UIDocument d;d.modal=true;UIElement root;root.id="canvas";root.kind=UIKind::Canvas;d.elements.push_back(root);
 UIElement input;input.id="name";input.parent="canvas";input.kind=UIKind::TextInput;input.offset={30,30};input.size={360,50};input.placeholder="Your name / имя";input.background={.08f,.12f,.2f,1};input.accessibleLabel="Character name";d.elements.push_back(input);
 UIElement list;list.id="items";list.parent="canvas";list.kind=UIKind::List;list.offset={30,110};list.size={400,240};list.rowHeight=40;list.background={.1f,.15f,.22f,1};d.elements.push_back(list);
 UIElement scroll;scroll.id="scroll";scroll.parent="canvas";scroll.kind=UIKind::Scroll;scroll.offset={500,30};scroll.size={400,240};scroll.background={.15f,.12f,.2f,1};scroll.flow=UIFlow::Vertical;d.elements.push_back(scroll);
 for(int n=0;n<40;++n){UIElement e;e.id="button"+std::to_string(n);e.parent="scroll";e.kind=UIKind::Button;e.text="Action "+std::to_string(n);e.size={350,42};e.background={.2f,.24f,.3f,1};d.elements.push_back(e);}return d;}
int main(int argc,char** argv){std::filesystem::path out=argc>1?argv[1]:".cache/m96/ui-tests";std::filesystem::create_directories(out);std::string error;
 auto doc=Fixture();UIDocument decoded;Check(ParseUIDocument(SerializeUIDocument(doc),decoded,error)&&SerializeUIDocument(doc)==SerializeUIDocument(decoded),"v3 controls lossless archive roundtrip");std::string named;Check(LegacyToNamed(SerializeUIDocument(doc),"ui",named,error)&&ParseUIDocument(named,decoded,error)&&SerializeUIDocument(doc)==SerializeUIDocument(decoded),"named authoring preserves new controls and accessibility");
 auto authoredPath=out/"edited.judasui";std::ofstream(authoredPath)<<named;AuthoringDocument author;Check(author.Load("ui",authoredPath.string(),error),"shared editor document opens new controls");auto modified=doc;modified.elements[1].placeholder="Edited in source tool";std::string modifiedNamed;LegacyToNamed(SerializeUIDocument(modified),"ui",modifiedNamed,error);Check(author.Replace(modifiedNamed,error)&&author.Dirty(),"editor validated control edit becomes dirty");Check(author.Undo()&&author.Source()==named&&author.Redo()&&author.Source()==modifiedNamed,"editor undo/redo preserves extended fields");Check(author.Save(error)&&LoadUIDocument(authoredPath.string(),decoded,error)&&decoded.elements[1].placeholder=="Edited in source tool","explicit editor save reloads exact authored controls");std::ofstream(authoredPath,std::ios::app)<<" ";Check(!author.Save(error),"external source conflict refuses overwrite");
 auto bad=doc;bad.elements[1].text="two\nlines";Check(!bad.Validate(error),"single-line input rejects newlines");bad=doc;bad.elements[1].maxBytes=0;Check(!bad.Validate(error),"zero input capacity rejects");
 std::string text="a\xCC\x81";auto b=TextGraphemes(text);Check(b.size()==2&&b.back()==3,"combining sequence is one grapheme");text="👩‍🚀🇬🇧";Check(TextGraphemes(text).size()==3,"ZWJ emoji and flag are indivisible graphemes");TextSelection selection{0,1};Check(EditText(text,selection,"Ж",32)&&text=="Ж🇬🇧"&&selection.caret==1,"replace selection never splits emoji/UTF-8");auto before=text;Check(!EditText(text,selection,std::string("\xFF"),100)&&text==before,"invalid UTF-8 transaction rejects");Check(!EditText(text,selection,"over capacity",2)&&text==before,"capacity transaction rejects without truncation");
 RuntimeUI ui;auto h=ui.Add(doc,"production",0,error);ui.Layout(1280,720);Check(h&&ui.Focus(h,"name")&&ui.Focused(h)=="name"&&ui.TextTarget().has_value(),"explicit text focus requests native session");Check(!ui.Focus(h,"missing"),"unknown focus target rejects");Check(!ui.Focus(h,"canvas"),"nonfocusable canvas rejects");
 InputSystem input;input.textPlatform=std::make_shared<TextInputPlatform>();std::string clipboard;input.textPlatform->clipboardRead=[&]{return clipboard;};input.textPlatform->clipboardWrite=[&](const std::string& value){clipboard=value;};
 auto edit=[&](const std::string& cmd,const std::string& value="",bool shift=false){input.BeginFrame();input.QueueText({cmd,value,shift});ui.Input(input,{-1,-1},false,1280,720);};
 input.BeginFrame();input.SetPhysical("key:Tab",1);input.QueueText({"tab","",true});ui.Input(input,{-1,-1},false,1280,720);Check(ui.Focused(h)=="button39","Shift+Tab reverses modal focus across controls");ui.Focus(h,"name");input.Reset();
 edit("insert","Conner 👩‍🚀");Check(ui.Element(h,"name")->text=="Conner 👩‍🚀","committed Unicode input edits field");edit("left","",true);edit("copy");Check(clipboard=="👩‍🚀","clipboard copy honors grapheme selection");edit("cut");Check(ui.Element(h,"name")->text=="Conner ","cut removes whole selected grapheme");edit("paste");Check(ui.Element(h,"name")->text=="Conner 👩‍🚀","paste uses same bounded insertion path");edit("undo");Check(ui.Element(h,"name")->text=="Conner ","undo restores text and selection");edit("redo");Check(ui.Element(h,"name")->text=="Conner 👩‍🚀","redo restores edit");
 auto oldSession=ui.TextTarget()->token;ui.Focus(h,"items");ui.Focus(h,"name");auto unchangedText=ui.Element(h,"name")->text;input.BeginFrame();TextInputEvent stale{"insert","STALE"};stale.token=oldSession;input.QueueText(stale);ui.Input(input,{-1,-1},false,1280,720);Check(ui.Element(h,"name")->text==unchangedText,"retired field-session commit cannot alias a newly focused field");
 auto stable=ui.Element(h,"name")->text;edit("composition","候補");Check(ui.Element(h,"name")->text==stable,"IME preedit is not committed text");edit("insert","語");Check(ui.Element(h,"name")->text==stable+"語","IME commit inserted once");edit("submit");auto events=ui.TakeEvents();Check(std::count_if(events.begin(),events.end(),[](auto& e){return e.type=="submit";})==1&&events.back().text==stable+"語","submit delivers detached final text exactly once");
 ui.Element(h,"name")->readOnly=true;edit("insert","BAD");Check(ui.Element(h,"name")->text==stable+"語","read-only blocks user edits");ui.Element(h,"name")->readOnly=false;
 input.BeginFrame();input.SetPhysical("key:W",1);input.QueueText({"insert","w"});ui.Input(input,{-1,-1},false,1280,720);input.BeginFixedStep();Check(!input.Action("move_forward").held&&!input.FixedAction("move_forward").pressed,"typing suppresses shared keyboard gameplay bindings");
 std::vector<UIListItem> items;for(int n=0;n<10000;++n)items.push_back({"item-"+std::to_string(n),"Salvage inventory item "+std::to_string(n),n!=2});Check(ui.SetItems(h,"items",items,error),"10000 dynamic rows accepted without 10000 widgets");auto invalid=items;invalid[1].id=invalid[0].id;Check(!ui.SetItems(h,"items",invalid,error),"duplicate row keys reject transactionally");Check(ui.Select(h,"items",9999)&&ui.Selected(h,"items")==9999&&ui.Scroll(h,"items").y>390000,"keyboard selection reveals distant virtual row");Check(!ui.Select(h,"items",2)&&ui.Selected(h,"items")==9999,"disabled rows cannot select");
 ui.Layout(1280,720);Check(ui.SetScroll(h,"scroll",{0,100000}),"scroll accepts bounded offset");ui.Layout(1280,720);auto offset=ui.Scroll(h,"scroll");Check(offset.y>1500&&offset.y<2100,"scroll clamps to actual child extent");Check(ui.Focus(h,"button0"),"focus may reveal clipped child");ui.Layout(1280,720);Check(ui.Scroll(h,"scroll").y<1,"focus scrolls clipped control into view");Check(ui.Focus(h,"button39"),"focus last control");ui.Layout(1280,720);auto rect=ui.LayoutOf(h,"button39")->rect,clip=ui.LayoutOf(h,"scroll")->rect;Check(rect.position.y+rect.size.y<=clip.position.y+clip.size.y+.01f,"last focused control lies inside scroll viewport");
 ui.SetScroll(h,"scroll",{0,0});ui.Layout(1280,720);input.Reset();input.AddDelta("mouse:wheelY",-1);ui.Input(input,{850,120},true,1280,720);Check(ui.Scroll(h,"scroll").y>0,"logical wheel axis scrolls hovered container");ui.TakeEvents();ui.Layout(1280,720);auto point=ui.LayoutOf(h,"items")->rect.position+glm::vec2(40,160);input.Reset();input.SetPhysical("mouse:Left",1);ui.Input(input,point,true,1280,720);input.BeginFrame();ui.Input(input,point-glm::vec2(0,50),true,1280,720);input.BeginFrame();input.SetPhysical("mouse:Left",0);ui.Input(input,point-glm::vec2(0,50),true,1280,720);auto dragEvents=ui.TakeEvents();Check(std::none_of(dragEvents.begin(),dragEvents.end(),[](const auto& e){return e.type=="change"&&e.element=="items";}),"scroll drag never activates release row");
 Check(ui.SetScale(h,1.25f)&&!ui.SetScale(h,3)&&ui.Scale(h)==1.25f,"readable scale validated");ui.Layout(1280,720);Check(ui.LayoutOf(h,"name")->rect.size.x==450,"scaling changes real layout geometry");auto saved=ui.CaptureState(h);auto savedText=ui.Element(h,"name")->text;ui.Element(h,"name")->text="changed";ui.SetScale(h,1);Check(ui.RestoreState(h,saved,error)&&ui.Element(h,"name")->text==savedText&&ui.Scale(h)==1.25f&&ui.Selected(h,"items")==9999,"UI state restores text/selection/list/scroll/scale/focus");auto state=ui.CaptureState(h);Check(!ui.RestoreState(h,"{\"version\":999}",error)&&ui.CaptureState(h)==state,"malformed state preserves all live values atomically");
 ui.Document(h)->visible=false;auto hidden=ui.CaptureState(h);Check(ui.RestoreState(h,hidden,error)&&!ui.Document(h)->visible,"hidden document snapshot restores without native focus");ui.Document(h)->visible=true;
 Check(ui.Accessibility(h).find("Character name")!=std::string::npos&&ui.Accessibility(h).find("textbox")!=std::string::npos,"semantic snapshot exposes real control roles and labels");
 UIDocument nested;UIElement canvas;canvas.id="canvas";canvas.kind=UIKind::Canvas;UIElement scroller;scroller.id="scroll";scroller.parent="canvas";scroller.kind=UIKind::Scroll;scroller.offset={500,30};scroller.size={400,200};UIElement field;field.id="field";field.parent="scroll";field.kind=UIKind::TextInput;field.offset={0,600};field.size={300,50};field.text="Select this text";nested.elements={canvas,scroller,field};auto nestedHandle=ui.Add(nested,"nested",0,error);ui.Focus(nestedHandle,"field");ui.Layout(1280,720);auto originalScroll=ui.Scroll(nestedHandle,"scroll");auto fieldPoint=ui.LayoutOf(nestedHandle,"field")->rect.position+glm::vec2(10);input.Reset();input.SetPhysical("mouse:Left",1);ui.Input(input,fieldPoint,true,1280,720);input.BeginFrame();ui.Input(input,fieldPoint+glm::vec2(0,30),true,1280,720);Check(ui.Scroll(nestedHandle,"scroll")==originalScroll,"text-selection drag inside scroll container never pans its parent");ui.Unload(nestedHandle);input.Reset();
 auto other=ui.Add(doc,"player-two",0,error);ui.SetPlayer(other,42);ui.Focus(other,"name");InputSystem second;second.QueueText({"insert","P2",false,false,0,0,42});ui.Input(second,{-1,-1},false,1280,720,42);Check(ui.Element(other,"name")->text=="P2"&&ui.Element(h,"name")->text==savedText,"player-addressed text never changes another document");second.BeginFrame();second.QueueText({"insert","WRONG",false,false,0,0,0});ui.Input(second,{-1,-1},false,1280,720,42);Check(ui.Element(other,"name")->text=="P2","text with other owner is ignored");ui.RemovePlayer(42);Check(!ui.Document(other)&&!ui.TextTarget(),"leaving player releases native text target and handles");
 EngineHost host;Check(host.Init("M96 production UI",1280,720,false,error),"native GL host");if(failures){printf("ERROR %s\n",error.c_str());return 1;}
 auto& window=host.GetWindow();window.SetTextInput(true);SDL_Event event{};event.type=SDL_TEXTINPUT;SDL_strlcpy(event.text.text,"native",sizeof(event.text.text));SDL_PushEvent(&event);window.PollEvents();Check(std::any_of(window.Input().TextEvents().begin(),window.Input().TextEvents().end(),[](auto& e){return e.command=="insert"&&e.text=="native";}),"SDL native text event reaches engine input adapter");window.SetTextInput(false);
 Check(!SDL_IsTextInputActive(),"native text input is off without a focused field");
 window.SetInputClaimed(true,true);window.SetHostTextInput(true);
 Check(SDL_IsTextInputActive(),"host text field activates native input despite claimed gameplay keyboard");
 window.SetTextInput(false);
 Check(SDL_IsTextInputActive(),"runtime without a text target cannot disable host field input");
 bool hostReceived=false;window.SetEventHook([&](const SDL_Event& e){if(e.type==SDL_TEXTINPUT&&std::string(e.text.text)=="editor")hostReceived=true;});
 event={};event.type=SDL_TEXTINPUT;SDL_strlcpy(event.text.text,"editor",sizeof(event.text.text));SDL_PushEvent(&event);window.PollEvents();
 Check(hostReceived&&window.Input().TextEvents().empty()&&SDL_IsTextInputActive(),"claimed text reaches host event hook without leaking to game input");
 event={};event.type=SDL_WINDOWEVENT;event.window.windowID=SDL_GetWindowID(window.NativeWindow());event.window.event=SDL_WINDOWEVENT_FOCUS_LOST;SDL_PushEvent(&event);window.PollEvents();
 Check(!SDL_IsTextInputActive(),"host native text stops on window focus loss");
 event.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;SDL_PushEvent(&event);window.PollEvents();window.SetHostTextInput(true);
 Check(SDL_IsTextInputActive(),"host field resumes native text after focus returns");
 window.SetHostTextInput(false);window.SetEventHook({});window.SetInputClaimed(false,false);
 Check(!SDL_IsTextInputActive(),"leaving host field releases native text session");
 // Exercise the same native-event -> SDL backend -> ImGui path as editor fields.
 ImGui::CreateContext();auto& editorIO=ImGui::GetIO();editorIO.IniFilename=nullptr;editorIO.LogFilename=nullptr;
 unsigned char* fontPixels;int fontWidth,fontHeight;editorIO.Fonts->GetTexDataAsRGBA32(&fontPixels,&fontWidth,&fontHeight);
 ImGui_ImplSDL2_InitForOpenGL(window.NativeWindow(),window.NativeGLContext());
 window.SetEventHook([](const SDL_Event& e){ImGui_ImplSDL2_ProcessEvent(&e);});
 char editorFields[3][128]{};const char* labels[]={"Optional new folder name","Project name","Save scene"};
 int focusField=-1;
 auto editorFrame=[&](){
  window.SetInputClaimed(editorIO.WantCaptureKeyboard,false);window.SetHostTextInput(editorIO.WantTextInput);window.PollEvents();
  ImGui_ImplSDL2_NewFrame();ImGui::NewFrame();ImGui::Begin("Editor text regression");
  for(int i=0;i<3;++i){if(focusField==i)ImGui::SetKeyboardFocusHere();ImGui::InputText(labels[i],editorFields[i],sizeof(editorFields[i]));}
  ImGui::End();ImGui::Render();focusField=-1;
 };
 editorFrame();editorFrame();editorFrame();
 for(int field=0;field<3;++field){
  // ImGui publishes the focus request before the following event-poll boundary.
  focusField=field;editorFrame();editorFrame();editorFrame();editorFrame();
  Check(SDL_IsTextInputActive(),"focused editor field requests a real native text session");
  event={};event.type=SDL_TEXTINPUT;event.text.windowID=SDL_GetWindowID(window.NativeWindow());SDL_strlcpy(event.text.text,"My game é",sizeof(event.text.text));SDL_PushEvent(&event);
  editorFrame();editorFrame();
  Check(std::string(editorFields[field])=="My game é",labels[field]);
  Check(window.Input().TextEvents().empty(),"editor field typing never becomes gameplay text");
 }
 window.SetHostTextInput(false);window.SetInputClaimed(false,false);window.SetEventHook({});ImGui_ImplSDL2_Shutdown();ImGui::DestroyContext();
 auto originalClipboard=window.Input().textPlatform->clipboardRead();window.Input().textPlatform->clipboardWrite("Judas café");Check(window.Input().textPlatform->clipboardRead()=="Judas café","native SDL clipboard roundtrips Unicode");window.Input().textPlatform->clipboardWrite(originalClipboard);
 LocalPlayers routed;auto hub=std::make_shared<InputDeviceHub>();routed.devices=hub;window.Input().localDevices=hub;auto p1=routed.Create("first",InputMap::Defaults(),error)->id,p2=routed.Create("second",InputMap::Defaults(),error)->id;routed.Assign(p2,*hub,{"keyboard",{}, {0,0,1,1}},error);window.PollEvents();routed.Route(*hub);auto firstUI=ui.Add(doc,"native-one",0,error),secondUI=ui.Add(doc,"native-two",0,error);ui.SetPlayer(firstUI,p1);ui.SetPlayer(secondUI,p2);ui.Focus(firstUI,"name");ui.Focus(secondUI,"name");window.SetTextInput(true,p2);event={};event.type=SDL_TEXTINPUT;SDL_strlcpy(event.text.text,"routed",sizeof(event.text.text));SDL_PushEvent(&event);window.PollEvents();routed.Route(*hub);ui.Input(routed.Find(p1)->input,{-1,-1},false,1280,720,p1);ui.Input(routed.Find(p2)->input,{-1,-1},false,1280,720,p2);Check(ui.Element(firstUI,"name")->text.empty()&&ui.Element(secondUI,"name")->text=="routed","native SDL to hub to player to field respects composition owner");ui.RemovePlayer(p1);ui.RemovePlayer(p2);window.SetTextInput(false);window.Input().localDevices.reset();
 ui.SetScale(h,1);ui.Focus(h,"name");ui.Select(h,"items",9999);auto& renderer=host.GetRenderer();renderer.BeginFrame(1280,720);renderer.EndFrame();renderer.BeginUIFrame(1280,720);ui.Draw(renderer,1280,720);renderer.EndUIFrame();Check(ui.Stats().listRows<=8&&ui.Stats().listRows>0,"actual GL draw shapes only visible virtual rows");std::vector<unsigned char> pixels;renderer.CaptureFrame(1280,720,pixels);Check(WriteRgbPng((out/"ui.png").string(),1280,720,pixels),"Unicode edit/list/focus pixels captured");
 UIDocument clipped;UIElement rootClip;rootClip.id="canvas";rootClip.kind=UIKind::Canvas;UIElement parentClip;parentClip.id="parent";parentClip.parent="canvas";parentClip.kind=UIKind::Panel;parentClip.size={400,240};parentClip.clip=true;UIElement virtualList;virtualList.id="list";virtualList.parent="parent";virtualList.kind=UIKind::List;virtualList.size={400,400000};virtualList.rowHeight=40;clipped.elements={rootClip,parentClip,virtualList};auto clippedHandle=ui.Add(clipped,"ancestor-clipped",0,error);ui.SetItems(clippedHandle,"list",items,error);ui.Document(h)->visible=false;renderer.BeginUIFrame(1280,720);ui.Draw(renderer,1280,720);renderer.EndUIFrame();Check(ui.Stats().listRows==6,"virtual list uses actual ancestor clip, not its huge authored rectangle");ui.Unload(clippedHandle);ui.Document(h)->visible=true;ui.Layout(1280,720);
 std::vector<double> samples;for(int n=0;n<320;++n){auto t=std::chrono::steady_clock::now();input.BeginFrame();ui.Input(input,{-1,-1},false,1280,720);if(n>=20)samples.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count());}std::sort(samples.begin(),samples.end());printf("PERFORMANCE rows=10000 widgets=%zu median_us=%.3f p95_us=%.3f worst_us=%.3f visible_rows=%u draws=%u\n",doc.elements.size(),samples[150],samples[285],samples.back(),ui.Stats().listRows,ui.Stats().draws);
 for(size_t count:{size_t(100),size_t(1000),size_t(10000)}){auto subset=std::vector<UIListItem>(items.begin(),items.begin()+count);ui.SetItems(h,"items",subset,error);ui.Select(h,"items",int(count)-1);ui.Layout(1280,720);std::vector<double> work;size_t bytes=0;for(auto& row:subset)bytes+=sizeof(UIListItem)+row.id.size()+row.text.size();for(int n=0;n<110;++n){auto t=std::chrono::steady_clock::now();input.BeginFrame();ui.Input(input,{-1,-1},false,1280,720);if(n>=10)work.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count());}std::sort(work.begin(),work.end());renderer.BeginUIFrame(1280,720);ui.Draw(renderer,1280,720);renderer.EndUIFrame();printf("SCALING rows=%zu median_us=%.3f p95_us=%.3f worst_us=%.3f visible_rows=%u payload_bytes=%zu draw_cpu_us=%.3f\n",count,work[50],work[95],work.back(),ui.Stats().listRows,bytes,ui.Stats().renderUs);}
 Check(ui.Unload(h)&&!ui.SelectText(h,"name",{0,0})&&!ui.Focus(h,"name"),"unloaded handles fail safely");
 for(bool legacy:{false,true}){
  RuntimeWorld world;world.legacyGameplay=legacy;Scene scene;
  Check(world.Build(scene,&host.Resources(),error),"HUD isolation world builds through ordinary runtime");
  InteractivePlay play;Check(play.Begin(world,WorldCoordinates{},error),"HUD isolation enters ordinary Play");
  play.Frame(window,renderer,0,false,true);const auto withoutHud=renderer.UIDrawCalls();
  play.Frame(window,renderer,0,true,true);const auto withHud=renderer.UIDrawCalls();
  Check(legacy?withHud>withoutHud:withHud==withoutHud&&withHud==0,
        legacy?"explicit legacy gameplay retains its diagnostic HUD":"modern starter renders no legacy diagnostic HUD");
  auto authored=world.UI().Add(Fixture(),"authored-hud",0,error);Check(authored!=0,"authored UI added through ordinary runtime");
  world.UI().debugOverlayVisible=false;
  play.Frame(window,renderer,0,false,true);const auto authoredOnly=renderer.UIDrawCalls();
  play.Frame(window,renderer,0,true,true);
  Check(authoredOnly>0&&renderer.UIDrawCalls()==authoredOnly,"project-owned UI renders independently of legacy HUD");
  world.UI().debugOverlayVisible=true;play.Frame(window,renderer,0,true,true);
  Check(legacy?renderer.UIDrawCalls()>authoredOnly:renderer.UIDrawCalls()==authoredOnly,
        "debug-overlay preference cannot enable inactive legacy gameplay HUD");
  play.End();world.Destroy();
 }
 host.Shutdown();printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
