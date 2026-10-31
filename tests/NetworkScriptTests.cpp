#include "TestProjectPath.h"
#include "AssetDatabase.h"
#include "GameSession.h"
#include "InputSystem.h"
#include "JobSystem.h"
#include "NetworkSession.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "SceneSession.h"
#include "ScriptSystem.h"
#include "Simulation.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
namespace fs=std::filesystem;using Json=nlohmann::json;
unsigned checks=0;
void Check(bool value,const std::string& message){++checks;std::printf("%s %s\n",value?"PASS":"FAIL",message.c_str());std::fflush(stdout);if(!value)throw std::runtime_error(message);}
Json State(RuntimeWorld& world){if(world.Scripts())for(const auto& value:world.Scripts()->Capture())if(value.entity==1)return Json::parse(value.json);return Json::object();}
void Healthy(RuntimeWorld& world){if(world.Scripts())for(const auto& diagnostic:world.Scripts()->Diagnostics())throw std::runtime_error(diagnostic.asset+" "+diagnostic.callback+" "+diagnostic.message);}
struct Content {
 Project project;Scene scene;AssetDatabase assets;JobSystem jobs{2};ResourceManager resources{nullptr,&assets,&jobs};
 explicit Content(const fs::path& file){std::string error;Check(project.Load(file.string(),error),"project loads: "+error);assets.Scan(project.RootDir(),project.AssetsDir());Check(assets.Problems().empty(),"ordinary asset registration has no problems");const bool loaded=LoadSceneFromFile(project.StartupScenePath(),scene,error);Check(loaded,"scene loads: "+error);}
};
struct Instance {
 RuntimeWorld world;GameSession game;InputSystem input;Content& content;
 explicit Instance(Content& c,const Scene& source):content(c){std::string error;world.legacyGameplay=false;world.viewportWidth=1280;world.viewportHeight=720;world.SetSceneControl(std::make_shared<SceneSession>(c.project,c.project.StartupScenePath()));Check(world.Build(source,&c.resources,error,&c.project.Settings().classification,&c.project.Settings().navigation),"headless world builds through ordinary resource path: "+error);Check(input.SetMap(c.project.Settings().input,error)&&game.Begin(world,error),"authoritative session starts: "+error);}
 ~Instance(){world.EndScripts();game.End();world.Destroy();}
 void Frame(double now){input.BeginFrame();world.PumpNetwork(now);world.UpdateUIScripts(&input,1.f/60);world.UpdateScripts(&input,1.f/60);input.BeginFixedStep();if(auto* players=world.PlayersIfLoaded())players->BeginFixedStep();StepAuthoritativeWorld(game,input,1.f/60);Healthy(world);}
};
const char* Probe=R"JS(import {network,world,input} from 'judas';
export const properties={role:{type:'string',default:'host'},port:{type:'number',default:0}};
export default class {
 constructor({properties}){this.props=properties;this.state={};}
 start(){
  this.state.offline=network.mode==='offline';
  if(this.props.role==='host'){
   network.host({address:'127.0.0.1',port:this.props.port,controls:['thrust','activate']});
   this.entry=network.replicate(world.entity('2'),{owner:'host',state:{value:7}});
   const neutral=network.replicate(world.entity('3'));this.state.defaultState=neutral.state===null;neutral.state=[1,true];this.state.arrayState=Array.isArray(neutral.state);neutral.forget();
   try{network.replicate(world.entity('4'),{owner:'host',state:{}})}catch(e){this.state.nestedRejected=e instanceof TypeError}
  }else network.join({address:'127.0.0.1',port:this.props.port,name:'Actual JS client'});
 }
 update(){
  if(this.props.role!=='client')return;
  if(network.status==='connected'){
   network.sendInput({thrust:input.axis('thrust'),activate:input.held('activate')?1:0});
   this.entry=network.entities.find(e=>e.entity?.id==='2')||this.entry;
   if(this.entry){this.state.received=this.entry.state.value;this.state.owner=this.entry.owner;
    if(!this.state.tested){this.state.tested=true;
     try{this.entry.state={value:99}}catch(e){this.state.stateDenied=e instanceof TypeError}
     try{this.entry.owner='host'}catch(e){this.state.ownerDenied=e instanceof TypeError}
     try{network.sendInput({undeclared:1})}catch(e){this.state.channelDenied=e instanceof TypeError}
    }
   }
  }
  if(input.held('leave')&&!this.state.closed){const old=this.entry;network.close();this.state.closed=network.mode==='offline';try{old.state}catch(e){this.state.stale=e instanceof ReferenceError}}
 }
 fixedUpdate(){
  if(this.props.role!=='host')return;
  const peer=network.peers.find(p=>p.id!=='host'&&p.connected);if(!peer)return;
  this.state.peer=peer.id;this.state.raw=network.input(peer.id).thrust||0;
  if(peer.player){peer.player.entity=world.entity('3');this.state.player=peer.player.valid;this.state.logical=peer.player.input.axis('thrust');if(peer.player.input.pressed('activate'))this.state.pulses=(this.state.pulses||0)+1;}
  if(!this.state.transferred){this.entry.owner=peer.id;this.entry.state={value:9};this.state.transferred=true;}
  if(this.state.raw>0)world.entity('2').applyForce({x:10*this.state.raw,y:0,z:0});
 }
})JS";

int Bindings(const fs::path& root){
 Check(!fs::exists(root),"fresh fixture directory preserves original evidence");fs::create_directories(root/"Assets");fs::create_directories(root/"Scenes");
 std::ofstream(root/"Assets/probe.js")<<Probe;
 AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());AssetRecord script;std::string error;Check(assets.Track((root/"Assets/probe.js").string(),script,error),"ordinary JS fixture registered: "+error);
 ProjectSettings settings;settings.name="M99 public script fixture";settings.legacyGameplay=false;settings.startupScene="Scenes/fixture.judas";settings.saveIdentity=std::string(64,'8');settings.input.entries.clear();settings.input.Add("thrust",true);settings.input.AddBinding("thrust",{"key:D"});settings.input.Add("activate",false);settings.input.AddBinding("activate",{"key:Space"});settings.input.Add("leave",false);settings.input.AddBinding("leave",{"key:L"});
 std::ofstream(root/"fixture.judasproj")<<Project::SerializeToString(settings);
 Scene scene;auto& owner=scene.CreateObject("Binding probe");owner.scripts={{1,script.id,true,"{}"}};auto& box=scene.CreateObject("Physical target");box.body=SceneBodyComponent{};box.body->motion=SceneBodyMotion::Dynamic;box.body->mass=2;box.transform.position={0,2,0};auto& motor=scene.CreateObject("Associated motor");motor.characterMotor=CharacterMotorSettings{};motor.characterMotor->gravityScale=0;auto& nested=scene.CreateObject("Unsupported child");nested.parent=2;
 Check(SaveSceneToFile(scene,(root/"Scenes/fixture.judas").string(),error),"fixture scene writes: "+error);
 Content content(root/"fixture.judasproj");Instance host(content,content.scene);host.Frame(100);Check(State(host.world).value("offline",false)&&State(host.world).value("nestedRejected",false),"actual public JS opts in and rejects unsupported replication");
 Check(State(host.world).value("defaultState",false)&&State(host.world).value("arrayState",false),"default null and nonobject plain JSON replication state match the public contract");
 double now=100;for(unsigned i=0;i<600&&host.world.Network().Status()!="listening";++i){content.resources.Pump();host.Frame(now+=1./60);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 Check(host.world.Network().Status()=="listening","public host binding reaches real listener");
 Check(!host.world.Scripts()->Reload(true,error)&&error.find("network")!=std::string::npos,"active network rejects script-VM replacement before lifecycle mutation");
 Scene remote=content.scene;remote.Find(1)->scripts[0].properties=Json({{"role","client"},{"port",host.world.Network().Port()}}).dump();Instance client(content,remote);client.Frame(now);
 auto pump=[&](unsigned count){for(unsigned i=0;i<count;++i){now+=1./60;content.resources.Pump();client.Frame(now);host.Frame(now);std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
 for(unsigned i=0;i<600&&client.world.Network().Status()!="connected";++i)pump(1);
 Check(client.world.Network().Status()=="connected","public join binding completes content-checked handshake");pump(20);
 auto state=State(client.world);Check(state.value("received",0)==9&&state.value("owner",std::string())==client.world.Network().PeerId(),"actual JS observes replicated JSON and controlled ownership");
 Check(state.value("stateDenied",false)&&state.value("ownerDenied",false)&&state.value("channelDenied",false),"client cannot publish host state, ownership or undeclared channels");
 client.input.SetPhysical("key:D",.75f);client.input.SetPhysical("key:Space",1);pump(24);
 auto observed=State(host.world);Check(observed.value("player",false)&&std::abs(observed.value("raw",0.)-.75)<1e-6&&std::abs(observed.value("logical",0.)-.75)<1e-6,"custom project channel reaches raw network intent and existing PlayerInput equally");
 Check(observed.value("pulses",0)==1,"remote custom action is seen once by authoritative fixed input");
 Check(host.world.Physics().GetLinearVelocity(host.world.RuntimeBody(2)).x>1,"host JS applies real force to physical body from remote intent");
 client.input.SetPhysical("key:D",0);client.input.SetPhysical("key:Space",0);pump(12);Check(State(host.world).value("raw",1.)==0,"neutral logical snapshot stops remote intent");
 client.input.SetPhysical("key:L",1);pump(1);state=State(client.world);Check(state.value("closed",false)&&state.value("stale",false),"actual JS close invalidates previous replica handles");pump(8);
 bool disconnected=false;for(auto peer:host.world.Network().Peers())if(peer.id!="host")disconnected|=!peer.connected;
 Check(disconnected,"host observes ordinary client close");
 return 0;
}

void DiagonalIntent(){
 Content content(TestProjectPath("connected_workshop")/"connected_workshop.judasproj");
 Instance host(content,content.scene);host.world.Network().SetLaunch("host","127.0.0.1",0,true);
 Scene peerScene=content.scene;peerScene.Find(1)->scripts.clear();Instance client(content,peerScene);
 double now=200;auto pump=[&](unsigned count){for(unsigned i=0;i<count;++i){now+=1./60;content.resources.Pump();client.Frame(now);host.Frame(now);std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
 for(unsigned i=0;i<600&&host.world.Network().Status()!="listening";++i)pump(1);
 Check(host.world.Network().Status()=="listening","actual game host starts for remote intent normalization check");
 NetworkSession::Options options;options.address="127.0.0.1";options.port=host.world.Network().Port();std::string error;
 Check(client.world.Network().Join(options,error),"independent native client uses ordinary declared control protocol");
 for(unsigned i=0;i<600&&client.world.Network().Status()!="connected";++i)pump(1);
 Check(client.world.Network().Status()=="connected","non-demo client connects to actual project host");
 // Valid scalar axes can still describe an overlength vector. The host game,
 // rather than trusting a particular client's readIntent helper, owns speed.
 Check(client.world.Network().SendInput({{"move_x",1},{"move_y",1},{"interact",0}},error),"raw diagonal snapshot is valid at the generic network boundary");
 pump(30);EntityPhysicalState before,after;Check(host.world.GetEntityState(101,before),"read actual authoritative motor before measured interval");pump(30);Check(host.world.GetEntityState(101,after),"read actual authoritative motor after measured interval");
 const double distance=std::hypot(after.position.x-before.position.x,after.position.z-before.position.z),speed=distance/.5;
 std::printf("GAME_DIAGONAL speed_mps=%.9f physical_distance_m=%.9f fixed_seconds=0.5\n",speed,distance);
 Check(speed>5.45&&speed<5.55,"host JS enforces its 5.5 m/s rule for unnormalized but bounded remote axes");
}

int Demo(const std::string& role,const fs::path& project,unsigned port,const fs::path& output,bool verifyDelivery=true){
 Check(role=="host"||role=="client","valid process role");Content content(project);Scene actual=content.scene;
 // Distinct fallback properties make loss of launch intent observable. These
 // are ordinary runtime scene properties; on-disk content stays byte-identical.
 if(role=="client")actual.Find(1)->scripts[0].properties="{\"serverAddress\":\"192.0.2.99\",\"port\":1}";
 Instance instance(content,actual);
 const float authoredGuestX=content.scene.Find(101)->transform.position.x;
 auto& net=instance.world.Network();net.SetLaunch(role=="host"?"host":"join","127.0.0.1",port,role=="host");
 if(role=="host"){
  auto actor=instance.world.RuntimeDefinition(101)->transform;actor.position={6.3f,.95f,0};Check(instance.world.SetRuntimeTransform(101,actor),"arrange actual motor facing test cargo");
  auto crate=instance.world.RuntimeDefinition(200)->transform;crate.position={8.2f,.56f,0};Check(instance.world.SetRuntimeTransform(200,crate),"arrange actual rigid crate before shared game starts");
 }
 const auto started=std::chrono::steady_clock::now();double connectedAt=-1;Json trace=Json::array();bool connected=false,ready=false;
 for(unsigned frame=0;frame<360;++frame){
  const auto deadline=started+std::chrono::microseconds((frame+1)*1000000/60);
  double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();content.resources.Pump();
  if(role=="client"&&net.Status()=="connected"&&connectedAt<0)connectedAt=seconds;
  const double age=connectedAt<0?-1:seconds-connectedAt;
  if(role=="client"){instance.input.SetPhysical("key:D",age>=0&&age<.10?1:0);instance.input.SetPhysical("key:E",age>=.25&&age<2.8?1:0);}
  instance.Frame(1000+seconds);
  if(role=="host"&&!ready&&net.Status()=="listening"){ready=true;std::printf("READY host port=%u\n",net.Port());std::fflush(stdout);}
  for(const auto& peer:net.Peers())if(peer.connected&&peer.id!="host")connected=true;
  if(role=="client")connected|=net.Status()=="connected";
  if(frame%6==0){auto pose=instance.world.Physics().GetTransform(instance.world.RuntimeBody(200));trace.push_back({{"time",seconds},{"x",pose.position.x},{"y",pose.position.y},{"z",pose.position.z},{"state",State(instance.world)}});}
  std::this_thread::sleep_until(deadline);
 }
 auto physical=instance.world.Physics().GetTransform(instance.world.RuntimeBody(200));EntityPhysicalState guest;const bool guestAvailable=instance.world.GetEntityState(101,guest);auto state=State(instance.world);auto stats=net.Stats();Json replicated=Json::array();for(auto entry:net.Entries())replicated.push_back({{"entity",entry.entity},{"owner",entry.owner},{"state",Json::parse(entry.state)}});
 Json result={{"role",role},{"verify_delivery",verifyDelivery},{"connected",connected},{"mode",net.Mode()},{"status",net.Status()},{"error",net.Error()},{"state",state},{"replicated",replicated},{"crate",{physical.position.x,physical.position.y,physical.position.z}},{"authored_guest_x",authoredGuestX},{"guest",{guest.position.x,guest.position.y,guest.position.z}},{"sent",stats.transport.bytesSent},{"received",stats.transport.bytesReceived},{"snapshots",stats.snapshots},{"trace",trace}};
 if(output.has_parent_path())fs::create_directories(output.parent_path());
 std::ofstream(output)<<result.dump(2)<<'\n';
 Check(connected,"actual game process exchanges real session traffic");Check(stats.transport.bytesSent>0&&stats.transport.bytesReceived>0,"both wire directions are exercised");
 if(role=="client"){const auto document=instance.world.UI().Find("connected");Check(document&&instance.world.UI().Element(document,"address")->text=="127.0.0.1"&&instance.world.UI().Element(document,"address_label")->text.find(std::to_string(port))!=std::string::npos,"game UI retains the actual launch address and custom port instead of authored fallbacks");}
 if(!verifyDelivery){
  Check(replicated.size()==14,"real dedicated server publishes both authored motors and all twelve physical crates");
  Check(guestAvailable&&guest.position.x>authoredGuestX+.15f,"ordinary client input moves the actual server-owned motor away from its authored position");
  Check(std::isfinite(physical.position.x)&&std::isfinite(physical.position.y)&&std::isfinite(physical.position.z),"actual replicated cargo endpoint remains finite without fixture rearrangement");
 }else if(role=="host"){Check(state.value("delivered",0)>0&&physical.position.x>9,"actual JS raycast/impulse moves real cargo into scoring bay");}
 else{bool score=false;for(const auto& entry:replicated)if(entry.at("entity")==100)score=entry.at("state").value("delivered",0)>0;Check(score&&physical.position.x>9,"client receives host score and matching actual cargo endpoint");}
 return 0;
}
}

int main(int argc,char** argv){try{
 if(argc==6&&std::string(argv[1])=="--demo")Demo(argv[2],fs::absolute(argv[3]),unsigned(std::stoul(argv[4])),fs::absolute(argv[5]));
 else if(argc==5&&std::string(argv[1])=="--probe")Demo("client",fs::absolute(argv[2]),unsigned(std::stoul(argv[3])),fs::absolute(argv[4]),false);
 else{Bindings(fs::absolute(argc>1?argv[1]:".cache/m99/script-bindings"));DiagonalIntent();}
 std::printf("SUMMARY checks=%u failures=0\n",checks);return 0;
}catch(const std::exception& error){std::fprintf(stderr,"FAIL %s\nSUMMARY checks=%u failures=1\n",error.what(),checks);return 1;}}
