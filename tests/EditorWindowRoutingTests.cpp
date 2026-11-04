#include "Window.h"
#include <cstdio>
namespace {
unsigned checks=0,failures=0;
void Check(bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
void WindowEvent(Uint32 id,Uint8 kind,int a=0,int b=0){SDL_Event e{};e.type=SDL_WINDOWEVENT;e.window.windowID=id;e.window.event=kind;e.window.data1=a;e.window.data2=b;SDL_PushEvent(&e);}
void Key(Uint32 id,bool down){SDL_Event e{};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.windowID=id;e.key.keysym.scancode=SDL_SCANCODE_A;e.key.keysym.sym=SDLK_a;e.key.state=down?SDL_PRESSED:SDL_RELEASED;SDL_PushEvent(&e);}
}
int main(){
 Window main;Check(main.Init("Editor window routing",320,240,false),"primary native window initializes");if(failures)return 1;
 main.SetMouseCaptured(false);auto* tool=SDL_CreateWindow("Detached tool",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,160,120,SDL_WINDOW_HIDDEN);Check(tool!=nullptr,"real secondary native window initializes");if(!tool)return 1;
 const auto primary=SDL_GetWindowID(main.NativeWindow()),secondary=SDL_GetWindowID(tool);unsigned observed=0;
 main.SetEventHook([&](const SDL_Event& e){if(e.type==SDL_WINDOWEVENT&&e.window.windowID==secondary)++observed;});
 InputMap map;map.Add("test",false);map.AddBinding("test",{"key:A"});std::string error;Check(main.Input().SetMap(map,error),"ordinary game input map configures");
 main.PollEvents();SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
 WindowEvent(secondary,SDL_WINDOWEVENT_SIZE_CHANGED,900,700);WindowEvent(secondary,SDL_WINDOWEVENT_CLOSE);Key(secondary,true);main.PollEvents();
 Check(observed==2,"editor hook receives secondary native window events");
 Check(main.Width()==320&&main.Height()==240,"secondary resize never changes primary dimensions");
 Check(!main.ShouldClose(),"closing detached tool never exits the application");
 Check(!main.Input().Action("test").held,"secondary key events never become game controls");
 main.CancelCloseRequest();WindowEvent(primary,SDL_WINDOWEVENT_FOCUS_GAINED);Key(primary,true);main.PollEvents();Check(main.Input().Action("test").held,"primary key still reaches game input");
 WindowEvent(secondary,SDL_WINDOWEVENT_FOCUS_LOST);main.PollEvents();Check(main.Input().Action("test").held,"secondary focus loss never clears held primary input");
 Key(primary,false);WindowEvent(primary,SDL_WINDOWEVENT_SIZE_CHANGED,400,300);main.PollEvents();Check(main.Width()==400&&main.Height()==300&&!main.Input().Action("test").held,"primary resize and key release still work");
 WindowEvent(primary,SDL_WINDOWEVENT_CLOSE);main.PollEvents();Check(main.ShouldClose(),"primary close retains application close request");
 main.CancelCloseRequest();WindowEvent(0,SDL_WINDOWEVENT_SIZE_CHANGED,320,240);main.PollEvents();Check(main.Width()==320&&main.Height()==240,"legacy synthetic window-id zero events preserve their contract");
 WindowInputRegion region{secondary,{20,10,100,80},200,160};
 Check(main.SetInputRegion(region)&&main.InputWidth()==200&&main.InputHeight()==160&&main.Width()==320,"embedded view has separate render and native dimensions");
 WindowEvent(secondary,SDL_WINDOWEVENT_FOCUS_GAINED);Key(secondary,true);main.PollEvents();Check(main.Input().Action("test").held,"explicitly selected detached game window receives controls");
 Key(primary,false);main.PollEvents();Check(main.Input().Action("test").held,"other editor window cannot release game-window controls");
 Key(secondary,false);main.PollEvents();Check(!main.Input().Action("test").held,"detached game-window key releases normally");
 int x=70,y=50;main.MapInputPosition(x,y);Check(x==100&&y==80,"content-local pointer maps to render resolution at non-unit scale");
 const auto normalized=main.NormalizeInputPosition(70,50);Check(normalized.x==.5f&&normalized.y==.5f,"same content bounds normalize game and local-player pointers");
 Check(!main.InputContains(19,20)&&main.InputContains(20,10)&&!main.InputContains(120,20),"content boundary rejects outside clicks with half-open edges");
 const auto ime=region.ToNative({100,80,20,24});Check(ime.x==70&&ime.y==50&&ime.w==10&&ime.h==12,"game IME rectangle maps back to native content coordinates");
 auto invalid=region;invalid.content.w=0;Check(!main.SetInputRegion(invalid)&&main.InputWidth()==200,"invalid input region rejects without replacing valid state");
 WindowEvent(secondary,SDL_WINDOWEVENT_CLOSE);main.PollEvents();Check(!main.ShouldClose(),"closing active detached game view still never closes editor");
 main.ClearInputRegion();Check(main.InputWidth()==main.Width()&&main.InputWindow()==main.NativeWindow(),"clearing embed restores ordinary desktop runtime defaults");
 SDL_DestroyWindow(tool);std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}
