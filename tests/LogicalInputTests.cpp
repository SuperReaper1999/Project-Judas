#include "InputSystem.h"
#include "LocalMultiplayer.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace {
int checks=0,failures=0;
void Check(bool condition,const char* description){++checks;if(!condition)++failures;std::printf("%s %s\n",condition?"PASS":"FAIL",description);}
InputMap Map(){
    InputMap map;
    map.Add("activate",false);map.AddBinding("activate",{"key:E"});
    map.Add("secondary",false);map.AddBinding("secondary",{"key:Q"});
    map.Add("move",true);map.AddBinding("move",{"key:D"});
    map.AddVector("aim");map.AddBinding("aim",{"stick:Right"});
    return map;
}
}

int main(){
    std::string error;InputSystem input;
    Check(input.SetMap(Map(),error),"existing authored map installs");
    Check(!input.SubmitLogical({{"activate",1}},error)&&!error.empty(),"injection requires explicit opt-in");
    input.SetPhysical("key:E",1);
    Check(input.Action("activate").held,"ordinary input works before opt-in");
    input.SetLogicalMode(true);
    Check(input.LogicalMode()&&!input.Action("activate").held&&!input.Action("activate").released,"changing source is neutral without invented release");
    Check(input.SubmitLogical({{"activate",1},{"move",-.75f}},error)&&error.empty(),"logical snapshot uses names without synthetic physical keys");
    Check(input.Action("activate").pressed&&input.Action("activate").held&&input.Axis("move")==-.75f,"logical action and axis exposed through ordinary reads");
    input.SetPhysical("key:E",0);input.SetPhysical("key:Q",1);input.SetStick("right",1,1);input.SetPointer(.5f,.5f,true);input.SetTouch(1,1,.5f,.5f,1,true);input.QueueText({"text","ignored"});
    Check(input.Action("activate").held&&!input.Action("secondary").held,"physical input cannot override logical source");
    Check(input.Stick("right").x==0&&input.Vector("aim").x==0&&!input.Pointer().down&&input.Touches().empty()&&input.TextEvents().empty(),"logical mode does not synthesize unsupported stick/vector/touch/text data");

    for(const auto& invalid:std::vector<std::pair<std::string,float>>{{"unknown",1},{"aim",1},{"activate",.5f},{"secondary",-1},{"move",1.001f},{"activate",std::numeric_limits<float>::quiet_NaN()}}){
        Check(!input.SubmitLogical({invalid},error)&&input.Action("activate").held&&input.Axis("move")==-.75f,"invalid snapshot preserves complete previous levels");
    }
    Check(!input.SubmitLogical({{"activate",0},{"move",.1f},{"unknown",0}},error)&&input.Action("activate").held&&input.Axis("move")==-.75f,"a late invalid member prevents earlier member mutation");
    Check(!input.SubmitLogical({{"move",std::numeric_limits<float>::infinity()}},error)&&input.Axis("move")==-.75f,"nonfinite axis rejects transactionally");
    input.BeginFrame();
    Check(input.Action("activate").held&&!input.Action("activate").pressed&&input.Axis("move")==-.75f,"frame start clears frame edges while retaining logical levels");
    input.BeginFixedStep();
    Check(input.FixedAction("activate").held&&input.FixedAction("activate").pressed,"fixed step still sees edge received before zero-step frame");
    Check(input.FixedAction("activate").pressed,"multiple fixed readers observe same edge");
    input.BeginFixedStep();
    Check(input.FixedAction("activate").held&&!input.FixedAction("activate").pressed,"catch-up fixed step does not repeat edge");
    Check(input.SubmitLogical({},error)&&!input.Action("activate").held&&input.Action("activate").released&&input.Axis("move")==0,"omitted names neutralize held input and axes");
    input.BeginFixedStep();
    Check(input.FixedAction("activate").released&&!input.FixedAction("activate").held,"neutralization release reaches authoritative step");
    input.BeginFrame();
    Check(input.SubmitLogical({{"activate",1}},error)&&input.SubmitLogical({},error),"several snapshots may arrive before one fixed step");
    input.BeginFrame();input.BeginFixedStep();
    Check(input.FixedAction("activate").pressed&&input.FixedAction("activate").released&&!input.FixedAction("activate").held,"short logical press/release survives packets and zero-step frames");
    input.BeginFixedStep();
    Check(!input.FixedAction("activate").pressed&&!input.FixedAction("activate").released,"short edge pair drains once");
    input.SubmitLogical({{"activate",1}},error);input.DiscardPending();input.BeginFixedStep();
    Check(input.FixedAction("activate").held&&!input.FixedAction("activate").pressed,"discarding menu-owned edges preserves held logical state");
    input.Reset();
    Check(input.LogicalMode()&&!input.Action("activate").held&&input.Axis("move")==0,"reset neutralizes source but retains explicit mode");
    input.SubmitLogical({{"activate",1}},error);input.SetLogicalMode(true);
    Check(input.Action("activate").held,"reselecting current mode preserves its snapshot");
    Check(input.SetMap(Map(),error)&&input.LogicalMode()&&!input.Action("activate").held,"map replacement clears old logical state");
    input.SetLogicalMode(false);input.SetPhysical("key:E",1);
    Check(!input.LogicalMode()&&input.Action("activate").held&&input.Action("activate").pressed,"source can return to normal physical input");

    LocalPlayers players;InputDeviceHub hub;
    auto* remote=players.Create("External",Map(),error);auto* local=players.Create("Local",Map(),error);
    Check(remote&&local,"independent world-local identities are ordinary players");
    if(!remote||!local)return 1;
    LocalPlayers::Assignment keyboard;keyboard.device="keyboard";
    Check(players.Assign(local->id,hub,keyboard,error),"local keyboard route installs");
    remote->input.SetLogicalMode(true);remote->input.SubmitLogical({{"activate",1},{"move",-.5f}},error);
    hub.BeginFrame();players.Route(hub);hub.BeginFrame();hub.Set("keyboard","key:D",1);players.Route(hub);
    Check(remote->input.Action("activate").pressed&&remote->input.Axis("move")==-.5f,"local routing does not clear external frame edges or overwrite levels");
    Check(local->input.Axis("move")==1&&!local->input.Action("activate").held,"local and external sources remain independent");
    hub.Reset();hub.BeginFrame();players.Route(hub);
    Check(remote->input.Action("activate").held&&remote->input.Axis("move")==-.5f&&local->input.Axis("move")==0,"local focus loss neutralizes devices without clearing remote peer input");
    players.BeginFixedStep();
    Check(remote->input.FixedAction("activate").pressed,"ordinary player fixed boundary snapshots external edge");
    remote->input.SubmitLogical({},error);players.BeginFixedStep();
    Check(remote->input.FixedAction("activate").released,"disconnect neutral snapshot publishes ordinary release");
    players.Unassign(local->id,"keyboard");
    Check(players.Assign(remote->id,hub,keyboard,error),"device assignment may be retained for subsequent source switch");
    hub.BeginFrame();hub.Set("keyboard","key:E",1);players.Route(hub);
    Check(!remote->input.Action("activate").held,"assigned local device remains ignored in logical mode");
    remote->input.SetLogicalMode(false);hub.BeginFrame();players.Route(hub);
    Check(remote->input.Action("activate").held&&!remote->input.Action("activate").pressed,"return to device source restores held baseline without artificial press");
    hub.BeginFrame();hub.Set("keyboard","key:E",0);players.Route(hub);
    Check(remote->input.Action("activate").released,"subsequent ordinary device edges remain functional");
    std::printf("SUMMARY checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
