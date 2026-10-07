#include "InputSystem.h"
#include "NamedAuthoring.h"
#include "Project.h"
#include "Window.h"
#include "StructuredAuthor.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <filesystem>
#include <fstream>

namespace {
int checks=0,failures=0;
void Check(bool ok,const char* text){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
bool Near(float a,float b){return std::abs(a-b)<2e-5f;}
bool Neutral(InputVector v){return v.x==0&&v.y==0;}
}
int main(int argc,char** argv){
    std::string error;
    const auto legacy=InputMap::Defaults();
    InputMap map=legacy;
    Check(map.AddVector("paired")&&map.AddBinding("paired",{"stick:Right",1,.2f,-1,true}),"paired binding authors independently of existing scalar bindings");
    InputSystem input;
    Check(input.SetMap(map,error),"paired map installs");
    input.BeginFrame();input.SetPhysical("key:Space",1);input.SetPhysical("key:Space",0);input.BeginFrame();input.BeginFixedStep();
    Check(input.FixedAction("jump").pressed&&input.FixedAction("jump").released&&!input.FixedAction("jump").held,"default scalar action short-press semantics survive zero-step frames");
    input.BeginFixedStep();Check(!input.FixedAction("jump").pressed,"old button catch-up steps do not duplicate edges");
    input.SetPhysical("key:A",1);input.SetPhysical("key:D",1);input.AddDelta("mouse:dx",13);
    Check(input.Axis("move_x")==0&&input.Axis("look_x")==13,"opposed keyboard and unclamped mouse scalar bindings remain compatible");
    input.BeginFrame();Check(input.Axis("look_x")==0,"old relative mouse binding still resets each frame");
    input.SetStick("right",0,0); // first controller observation establishes baseline
    input.BeginFrame();input.SetStick("right",.6f,.8f);
    auto raw=input.Stick("right"),mapped=input.Vector("paired"),delta=input.StickDelta("right");
    Check(Near(raw.x,.6f)&&Near(raw.y,.8f)&&Near(delta.x,.6f)&&Near(delta.y,.8f),"raw paired values/frame delta are independent from fixed steps");
    Check(Near(mapped.x,.6f)&&Near(mapped.y,-.8f),"circular deadzone preserves diagonal direction then applies Y inversion");
    Check(Near(input.Axis("look_stick_x"),(.6f-.15f)/.85f)&&Near(input.Axis("look_stick_y"),(.8f-.15f)/.85f),"old scalar deadzones retain independent response");
    input.SetStick("right",1,1);raw=input.Stick("right");mapped=input.Vector("paired");
    Check(raw.x==1&&raw.y==1&&Near(mapped.x,std::sqrt(.5f))&&Near(mapped.y,-std::sqrt(.5f)),"raw diagonal is not clamped to unit circle; new processed radial response is");
    input.SetStick("right",.2f,0);Check(Neutral(input.Vector("paired")),"circular boundary is neutral");
    input.SetStick("right",.6f,0);Check(Near(input.Vector("paired").x,.5f),"circular travel outside deadzone is rescaled once");
    Check(!map.ReplaceBinding("paired",0,{"stick:Right",1,1,1,true})&&!map.ReplaceBinding("paired",0,{"stick:Right",1,-.1f,1,true})&&!map.ReplaceBinding("paired",0,{"stick:Right",std::numeric_limits<float>::infinity(),.2f,1,true}),"invalid/nonfinite paired settings reject atomically");
    Check(!map.AddBinding("jump",{"stick:Right",1,0,1,true})&&!map.AddBinding("paired",{"key:Space"}),"paired and scalar binding types remain distinct");
    InputMap parsed;Check(InputMap::Parse(map.Serialize(),parsed,error)&&parsed.Serialize()==map.Serialize()&&map.Serialize().rfind("2 ",0)==0,"extended legacy input map round-trips paired fields");
    Check(InputMap::Parse(legacy.Serialize(),parsed,error)&&parsed.Serialize()==legacy.Serialize()&&legacy.Serialize().rfind("1 ",0)==0,"old default input maps preserve their canonical v1 representation");
    std::string named,back;
    Check(LegacyToNamed(map.Serialize(),"input",named,error)&&InputMap::Parse(named,parsed,error)&&parsed.Serialize()==map.Serialize()&&NamedToLegacy(named,"input",back,error)&&back==map.Serialize(),"M67 named input documents round-trip vector/circular/inversion settings");
    ProjectSettings settings;settings.name="Paired input";settings.input=map;ProjectSettings project;
    Check(Project::ParseFromString(Project::SerializeToString(settings),project,error)&&project.input.Serialize()==map.Serialize(),"normal project input-map integration round-trips");
    Check(LegacyToNamed(Project::SerializeToString(settings),"project",named,error)&&Project::ParseFromString(named,project,error)&&project.input.Serialize()==map.Serialize(),"named project authoring retains paired map");
    const auto authoring=std::filesystem::absolute(argc>1?argv[1]:".cache/m69/paired-native");std::filesystem::create_directories(authoring);
    auto structured=[&](const std::string& text){std::ofstream(authoring/"input.json")<<text;if(!WriteStructuredContent((authoring/"input.json").string(),(authoring/"input.map").string(),error))return false;std::ifstream in(authoring/"input.map");const std::string bytes((std::istreambuf_iterator<char>(in)),{});return InputMap::Parse(bytes,parsed,error);};
    Check(structured(R"({"kind":"input","entries":[{"name":"old","axis":true,"bindings":[{"control":"stick:LeftX","deadzone":0.2}]}]})")&&parsed.Find("old")&&parsed.Find("old")->axis&&parsed.Find("old")->bindings[0].scaleY==1&&!parsed.Find("old")->bindings[0].circular,"compact structured authoring keeps old scalar defaults");
    Check(structured(R"({"kind":"input","entries":[{"name":"pair","vector":true,"bindings":[{"control":"stick:Right","scale":0.5,"scaleY":-1,"deadzone":0.2,"circular":true}]}]})")&&parsed.Find("pair")&&parsed.Find("pair")->vector&&parsed.Find("pair")->bindings[0].circular&&parsed.Find("pair")->bindings[0].scaleY==-1,"compact structured authoring exposes paired settings through normal serializer");

    input.Reset();input.SetStick("right",0,0);const auto cursor=input.StickSamples("right").sequence;
    input.BeginFrame();input.SetStick("right",.75f,.75f);input.SetStick("right",0,0);
    auto samples=input.StickSamples("right",cursor),again=input.StickSamples("right",cursor);
    Check(Neutral(input.Stick("right"))&&Neutral(input.StickDelta("right"))&&samples.samples.size()==2&&samples.samples.front().x==.75f&&samples.samples.back().x==0,"out-and-back observations survive neutral endpoint/net delta before any fixed step");
    Check(samples.samples.size()==again.samples.size()&&samples.sequence==again.sequence&&!samples.reset&&!samples.overflow,"repeated readers receive identical non-destructive snapshots");
    Check(samples.samples.size()==2&&samples.samples[0].sequence<samples.samples[1].sequence&&samples.samples[0].time<=samples.samples[1].time&&samples.samples[0].time>=0,"observation IDs order monotonically with receipt-time seconds");
    input.BeginFrame();input.BeginFixedStep();input.BeginFixedStep();
    Check(input.StickSamples("right",cursor).samples.size()==2&&input.StickSamples("right",samples.sequence).samples.empty(),"zero-step frames/catch-up steps retain history while reader cursor prevents repeats");
    const auto prior=input.StickSamples("right").sequence;
    for(int i=0;i<140;++i)input.SetStick("right",i%2?1.f:0.f,0);
    const auto overflow=input.StickSamples("right",prior);
    Check(overflow.capacity==128&&overflow.samples.size()==128&&overflow.overflow&&!overflow.reset,"bounded ring reports overflow instead of pretending truncated flick is complete");
    const auto recent=overflow.samples.back().sequence;
    input.SetStick("left",0,0);for(int i=0;i<150;++i)input.SetStick("left",i%2?1.f:0.f,0);
    Check(!input.StickSamples("right",recent).overflow,"global sequence gaps from other stick do not fabricate per-stick overflow");
    input.DiscardPending();auto reset=input.StickSamples("right",recent);
    Check(reset.reset&&reset.samples.empty()&&Neutral(input.StickDelta("right")),"pause/session edge discard clears stick history/delta with detectable reset");
    input.SetStick("right",.9f,.4f);Check(input.StickSamples("right",reset.sequence).samples.empty()&&Neutral(input.StickDelta("right")),"first observation after discontinuity establishes baseline without fabricated flick");
    input.BeginFrame();input.SetStick("right",0,0);Check(input.StickSamples("right",reset.sequence).samples.size()==1,"subsequent actual movement remains observable after baseline");
    const auto clearCursor=input.StickSamples("right").sequence;input.ClearDevice("stick:");
    Check(Neutral(input.Stick("right"))&&Neutral(input.StickDelta("right"))&&input.StickSamples("right",clearCursor).reset&&input.StickSamples("right",clearCursor).samples.empty(),"disconnect clears raw movement and pending observations");
    input.SetStick("right",0,0);input.SetStick("right",.6f,.8f);input.ConsumeBindings({"look_stick_x"});
    Check(Neutral(input.Stick("right"))&&Neutral(input.Vector("paired"))&&input.StickSamples("right").samples.empty(),"UI consumption suppresses paired bindings sharing a constituent scalar control");
    input.BeginFrame();Check(Near(input.Stick("right").x,.6f)&&input.StickSamples("right").samples.empty(),"input resumes held values without stale consumed observations");

    Window window;const bool initialized=window.Init("M69 paired input",320,240,false);
    Check(initialized,"normal SDL/GL Window input backend starts");
    if(initialized){
#if SDL_VERSION_ATLEAST(2,0,14)
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
        const int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);
        const int secondDevice=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);
        Check(device>=0&&secondDevice>=0&&SDL_IsGameController(device)&&SDL_IsGameController(secondDevice),"two synthetic SDL controllers exercise actual device selection; no hardware claim");
        SDL_Joystick* pad=device>=0?SDL_JoystickOpen(device):nullptr;
        SDL_Joystick* other=secondDevice>=0?SDL_JoystickOpen(secondDevice):nullptr;
        if(pad&&other){
            const auto id=SDL_JoystickInstanceID(pad),otherId=SDL_JoystickInstanceID(other);
            window.PollEvents();const auto backendCursor=window.Input().StickSamples("right").sequence;
            auto axisEvent=[](SDL_JoystickID which,int axis,Sint16 value){SDL_Event e{};e.type=SDL_CONTROLLERAXISMOTION;e.caxis.which=which;e.caxis.axis=Uint8(axis);e.caxis.value=value;SDL_PushEvent(&e);};
            axisEvent(id,SDL_CONTROLLER_AXIS_RIGHTX,24575);axisEvent(id,SDL_CONTROLLER_AXIS_RIGHTY,24575);axisEvent(id,SDL_CONTROLLER_AXIS_RIGHTX,0);axisEvent(id,SDL_CONTROLLER_AXIS_RIGHTY,0);
            axisEvent(otherId,SDL_CONTROLLER_AXIS_RIGHTX,32767);window.PollEvents();
            const auto observed=window.Input().StickSamples("right",backendCursor);
            Check(observed.samples.size()==4&&Near(observed.samples[1].x,24575.f/32767.f)&&Near(observed.samples[1].y,24575.f/32767.f)&&Neutral(window.Input().Stick("right"))&&Neutral(window.Input().StickDelta("right")),"delivered SDL neutral/diagonal/neutral axis events survive one render pump; other controller ignored");
            SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_RIGHTX,-32768);SDL_JoystickSetVirtualAxis(pad,SDL_CONTROLLER_AXIS_RIGHTY,16384);window.PollEvents();
            Check(window.Input().Stick("right").x==-1&&Near(window.Input().Stick("right").y,16384.f/32767.f),"backend negative/positive endpoints reuse current normalization and SDL signs");
            auto focus=[&](Uint8 state){SDL_Event e{};e.type=SDL_WINDOWEVENT;e.window.windowID=SDL_GetWindowID(window.NativeWindow());e.window.event=state;SDL_PushEvent(&e);};
            const auto focusCursor=window.Input().StickSamples("right").sequence;focus(SDL_WINDOWEVENT_FOCUS_LOST);window.PollEvents();
            Check(Neutral(window.Input().Stick("right"))&&window.Input().StickSamples("right",focusCursor).reset&&window.Input().StickSamples("right",focusCursor).samples.empty(),"normal focus loss neutralizes raw pair and clears history");
            focus(SDL_WINDOWEVENT_FOCUS_GAINED);window.PollEvents();
            Check(window.Input().Stick("right").x==-1&&Neutral(window.Input().StickDelta("right"))&&window.Input().StickSamples("right").samples.empty(),"focus regain restores held controller as baseline without false motion");
            window.SetInputClaimed(true,false);window.PollEvents();
            Check(Neutral(window.Input().Stick("right"))&&window.Input().StickSamples("right").samples.empty(),"editor-owned controller input is neutral with no pending gameplay flick");
            window.SetInputClaimed(false,false);window.PollEvents();
            Check(window.Input().Stick("right").x==-1&&Neutral(window.Input().StickDelta("right"))&&window.Input().StickSamples("right").samples.empty(),"editor ownership release rebaselines held controller");
            SDL_JoystickSetVirtualAxis(other,SDL_CONTROLLER_AXIS_RIGHTX,16384);window.PollEvents();
            const auto disconnectCursor=window.Input().StickSamples("right").sequence;
            SDL_JoystickClose(pad);pad=nullptr;SDL_JoystickDetachVirtual(device);window.PollEvents();
            Check(Near(window.Input().Stick("right").x,16384.f/32767.f)&&Neutral(window.Input().StickDelta("right"))&&window.Input().StickSamples("right",disconnectCursor).reset&&window.Input().StickSamples("right",disconnectCursor).samples.empty(),"controller reassignment uses same new device pair with neutral delta/history");
        }
        if(pad)SDL_JoystickClose(pad);
        if(other)SDL_JoystickClose(other);
        // Only this process's two fixture devices were attached above.
        if(secondDevice>=0){const int remaining=SDL_NumJoysticks()-1;if(remaining>=0&&SDL_JoystickIsVirtual(remaining))SDL_JoystickDetachVirtual(remaining);}
        if(pad&&device>=0&&device<SDL_NumJoysticks()&&SDL_JoystickIsVirtual(device))SDL_JoystickDetachVirtual(device);
        window.PollEvents();Check(Neutral(window.Input().Stick("right")),"no controller yields defined neutral raw pair");
#else
        Check(false,"SDL virtual-controller fixture requires SDL >=2.0.14");
#endif
        window.Shutdown();
    }
    std::printf("SUMMARY checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
