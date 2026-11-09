#include "InputSystem.h"
#include "Project.h"
#include "Window.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace {int checks=0,failures=0;void Check(bool ok,const char* text){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}bool Near(float a,float b){return std::abs(a-b)<1e-6f;}}
int main(){
    InputMap map;map.Add("fire",false);map.AddBinding("fire",{"key:Space"});map.AddBinding("fire",{"pad:South"});
    map.Add("move",true);map.AddBinding("move",{"key:A",-1});map.AddBinding("move",{"key:D",1});
    map.Add("look",true);map.AddBinding("look",{"mouse:dx"});map.Add("wheel",true);map.AddBinding("wheel",{"mouse:wheelY"});
    map.Add("stick",true);map.AddBinding("stick",{"stick:LeftX",1,.2f});
    InputSystem input;std::string error;Check(input.SetMap(map,error),"generic map installs");
    input.BeginFrame();input.SetPhysical("key:Space",1);Check(input.Action("fire").held&&input.Action("fire").pressed,"digital pressed and held");
    input.BeginFrame();Check(input.Action("fire").held&&!input.Action("fire").pressed,"press lasts one frame");
    input.SetPhysical("pad:South",1);input.SetPhysical("key:Space",0);Check(input.Action("fire").held&&!input.Action("fire").released,"overlapping bindings do not duplicate or prematurely release");
    input.SetPhysical("pad:South",0);Check(!input.Action("fire").held&&input.Action("fire").released,"last binding releases action");
    input.BeginFrame();Check(!input.Action("fire").released,"release lasts one frame");
    input.SetPhysical("key:A",1);Check(input.Axis("move")==-1,"keyboard negative axis");input.SetPhysical("key:D",1);Check(input.Axis("move")==0,"opposed keyboard axis cancels");
    input.AddDelta("mouse:dx",15);input.AddDelta("mouse:dx",-2);input.AddDelta("mouse:wheelY",2);Check(input.Axis("look")==13&&input.Axis("wheel")==2,"relative delta/wheel accumulate without joystick clamping");
    input.BeginFrame();Check(input.Axis("look")==0&&input.Axis("wheel")==0,"relative input resets each frame");
    input.SetPhysical("stick:LeftX",.1f);Check(input.Axis("stick")==0,"configured deadzone removes small deflection");input.SetPhysical("stick:LeftX",.6f);Check(Near(input.Axis("stick"),.5f),"linear deadzone normalization");
    input.SetPhysical("stick:LeftX",-1);Check(input.Axis("stick")==-1,"negative full stick endpoint");
    input.SetPhysical("pad:South",1);input.ClearDevice("pad:");input.ClearDevice("stick:");Check(!input.Action("fire").held&&input.Action("fire").released&&input.Axis("stick")==0,"disconnect clears button and axis state");
    Check(map.ReplaceBinding("fire",0,{"key:Q"})&&input.SetMap(map,error),"runtime binding replacement");input.SetPhysical("key:Space",1);Check(!input.Action("fire").held,"old replaced binding inactive");input.SetPhysical("key:Q",1);Check(input.Action("fire").pressed,"new binding works");
    Check(map.AddBinding("fire",{"mouse:Left"})&&map.RemoveBinding("fire",1)&&map.Find("fire")->bindings.size()==2,"add/remove/query bindings");
    Check(map.Rename("fire","activate")&&map.Remove("wheel"),"rename/delete logical entries");
    InputMap round;Check(InputMap::Parse(map.Serialize(),round,error)&&round.Serialize()==map.Serialize(),"input map exact serialization round-trip");
    Check(!InputMap::Parse("1 2 \"same\" 0 0 \"same\" 1 0",round,error),"duplicate logical names reject atomically");
    InputSystem shortPress;shortPress.BeginFrame();shortPress.SetPhysical("key:Space",1);shortPress.SetPhysical("key:Space",0);shortPress.BeginFrame();
    shortPress.BeginFixedStep();const auto fixed=shortPress.FixedAction("jump");Check(fixed.pressed&&fixed.released&&!fixed.held,"short press survives zero-step frame into fixed snapshot");
    Check(shortPress.FixedAction("jump").pressed,"multiple fixed consumers see same edge");shortPress.BeginFixedStep();Check(!shortPress.FixedAction("jump").pressed,"catch-up step does not repeat press");
    shortPress.SetPhysical("key:Space",1);shortPress.DiscardPending();shortPress.BeginFixedStep();Check(!shortPress.FixedAction("jump").pressed,"ownership transition discards stale edge");
    ProjectSettings settings;settings.name="Input test";settings.input=map;ProjectSettings loaded;
    Check(Project::ParseFromString(Project::SerializeToString(settings),loaded,error)&&loaded.input.Serialize()==map.Serialize(),"ordinary project round-trips input map");
    Check(Project::ParseFromString("JudasProject 1\nname \"Legacy\"\nstartup-scene \"\"\nassets-dir \"Assets\"\nscenes-dir \"Scenes\"\nsaves-dir \"Saves\"\n",loaded,error)&&loaded.input.Find("jump"),"legacy projects use explicit default bindings");
    const auto path=std::filesystem::path(".cache/m35-project");std::filesystem::remove_all(path);Project project;
    Check(Project::CreateNew(path.string(),"Input",project,error),"create real project");project.Settings().input=map;Check(project.Save(error),"save project input");Project reopened;
    Check(reopened.Load(project.ProjectFile(),error)&&reopened.Settings().input.Serialize()==map.Serialize(),"project reload restores rebinding");
    Window window;Check(window.Init("M35 input",320,240,false),"real SDL window backend starts");window.SetMouseCaptured(true);
    auto key=[&](Uint32 type,SDL_Scancode code){SDL_Event e{};e.type=type;e.key.type=type;e.key.keysym.scancode=code;SDL_PushEvent(&e);};
    window.PollEvents();key(SDL_KEYDOWN,SDL_SCANCODE_SPACE);key(SDL_KEYUP,SDL_SCANCODE_SPACE);window.PollEvents();
    Check(window.Input().Action("jump").pressed&&window.Input().Action("jump").released,"actual SDL short press feeds logical input and existing gameplay handoff");
    auto runtimeMap=InputMap::Defaults();runtimeMap.ReplaceBinding("move_y",1,{"key:Q"});window.Input().SetMap(runtimeMap,error);
    key(SDL_KEYDOWN,SDL_SCANCODE_Q);window.PollEvents();Check(window.InputAxis("move_y")==1,"window consumers honor project rebind");key(SDL_KEYUP,SDL_SCANCODE_Q);window.PollEvents();Check(window.InputAxis("move_y")==0,"raw release clears rebound axis");
    SDL_Event motion{};motion.type=SDL_MOUSEMOTION;motion.motion.xrel=30;SDL_PushEvent(&motion);window.PollEvents();
    Check(window.InputAxis("look_x")!=0,"actual mouse movement reaches the project-owned look axis");
    window.SetInputClaimed(true,true);key(SDL_KEYDOWN,SDL_SCANCODE_SPACE);window.PollEvents();Check(!window.Input().Action("jump").pressed,"editor-owned input does not create gameplay press");
    window.SetInputClaimed(false,false);
#if SDL_VERSION_ATLEAST(2,0,14)
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1"); // hidden-window test, not runtime policy
    const int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);
    Check(device>=0&&SDL_IsGameController(device),"tiny SDL virtual pad exercises normal controller adapter");
    SDL_Joystick* pad=SDL_JoystickOpen(device);
    SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTX,16384);SDL_JoystickSetVirtualButton(pad,SDL_CONTROLLER_BUTTON_A,1);window.PollEvents();
    std::printf("PAD raw=%d button=%d logical=%.9f held=%d focusFlags=%u\n",SDL_JoystickGetAxis(pad,0),SDL_JoystickGetButton(pad,0),window.InputAxis("move_x"),window.Input().Action("jump").held,SDL_GetWindowFlags(window.NativeWindow()));
    Check(window.Input().Action("jump").held&&std::abs(window.InputAxis("move_x")-(16384.f/32767-.15f)/.85f)<1e-5f,"actual SDL button/axis reaches named action and normalized deadzone");
    SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_LEFTX,-32768);window.PollEvents();Check(window.InputAxis("move_x")==-1,"actual SDL negative axis endpoint normalizes exactly");
    SDL_JoystickClose(pad);SDL_JoystickDetachVirtual(device);window.PollEvents();
    Check(!window.Input().Action("jump").held&&window.InputAxis("move_x")==0,"actual SDL disconnect clears active-controller logical state");
#endif
    window.Shutdown();
    for(int n:{46,100}){
        InputMap stress;for(int i=0;i<n;++i){const auto name="action"+std::to_string(i);stress.Add(name,false);stress.AddBinding(name,{"key:Space"});}InputSystem system;system.SetMap(stress,error);
        const auto begin=std::chrono::steady_clock::now();float sink=0;
        for(int i=0;i<10000;++i){system.BeginFrame();system.SetPhysical("key:Space",float(i&1));sink+=system.Action("action0").held;}
        std::printf("PERFORMANCE bindings=%d update_query_mean_us=%.6f checksum=%.0f\n",n,std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/10000,sink);
    }
    std::printf("SUMMARY checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
