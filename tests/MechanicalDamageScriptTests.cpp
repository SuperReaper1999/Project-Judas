// Public JudasJS only, executed in the real QuickJS VM. Physical attachment
// release is caused by a real body/material load rather than a script flag.
#include "ScriptSystem.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "AssetDatabase.h"
#include "InputSystem.h"
#include "Deformable.h"
#include "Powertrain.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs=std::filesystem;
using Json=nlohmann::json;
namespace {
unsigned checks=0;
void Check(bool value,const char* label){++checks;std::printf("%s %s\n",value?"PASS":"FAIL",label);if(!value)throw std::runtime_error(label);}
Json State(RuntimeWorld& world){auto states=world.Scripts()->Capture();if(states.size()!=1)throw std::runtime_error("one probe script required");return Json::parse(states.front().json);}
void Step(RuntimeWorld& world,InputSystem& input){input.BeginFixedStep();world.FixedScripts(&input,1.f/60);world.UpdatePowertrains(1.f/60);world.ResolvePowertrainLoads(1.f/60);world.Physics().Step(1.f/60);world.PublishPowertrains(1.f/60);world.UpdateDeformables(1./60);world.PublishPowertrainConnections();}
Scene Fixture(const AssetId& material,const AssetId& script){
 Scene scene;
 auto& fixed=scene.CreateObject("Prescribed negative control");fixed.id=1;fixed.body=SceneBodyComponent{};fixed.body->collisionMask=0;fixed.transform.position={5,0,0};
 auto& mount=scene.CreateObject("Ordinary physical mount");mount.id=2;mount.body=SceneBodyComponent{};mount.body->motion=SceneBodyMotion::Dynamic;mount.body->mass=2;mount.body->halfExtents={.2f,.2f,.05f};mount.body->collisionMask=0;
 auto& owner=scene.CreateObject("Physical material");owner.id=3;owner.deformable=DeformableSettings{};auto& d=*owner.deformable;d.asset=material;d.selfContact=false;d.collisionMask=0;d.material.density=10;d.material.damping=0;d.substeps=4;d.iterations=6;DeformableAttachment attachment;attachment.group="patch";attachment.kind=DeformableAttachment::Kind::Body;attachment.target=2;d.attachments={attachment};owner.scripts={{1,script,true,"{}"}};
 auto& machine=scene.CreateObject("Generic transmission");machine.id=4;machine.powertrain=PowertrainSettings{};auto& p=*machine.powertrain;
 PowertrainShaft rotor;rotor.name="rotor";PowertrainShaft output;output.name="output";p.shafts={rotor,output};
 PowertrainSource source;source.name="source";source.maxTorque=100;p.sources={source};
 PowertrainLink link;link.name="coupling";link.connection.owner=3;link.connection.attachment="patch";p.links={link};
 return scene;
}
}

int main(int argc,char** argv){try{
 const auto destination=fs::absolute(argc>1?fs::path(argv[1]):fs::temp_directory_path()/"judas-m83-script");
 // Each execution owns its registration metadata. Reusing an old temporary
 // database must not accidentally make a successful VM test source-dependent.
 const auto root=destination/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());fs::create_directories(root/"Assets");
 auto asset=MakeDeformableSheet(3,3,.4,.4);auto& patch=asset.groups["patch"];for(unsigned n=0;n<asset.nodes.size();++n)patch.push_back(n);std::string error;
 Check(PrepareDeformableAsset(asset,error),"registered fixture has an actual non-collinear physical material patch");
 std::ofstream(root/"Assets/patch.judasdeform")<<EncodeDeformableAsset(asset);
 std::ofstream(root/"Assets/probe.js")<<R"JS(import {world,Deformable} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={steps:0};}
 rejects(fn,kind){let caught=false;try{fn();}catch(e){caught=e instanceof kind;}if(!caught)throw Error('expected '+kind.name);}
 start(){
  this.material=this.entity.deformable;this.power=world.entity('4').powertrain;
  if(!(this.material instanceof Deformable)||!this.material.valid)throw Error('material handle');
  if(world.entity('2').deformable!==null)throw Error('phantom component');
  this.state.started=true;
 }
 update(){
  this.rejects(()=>this.material.attach('patch',{kind:'body',target:world.entity('2'),forceLimit:1}),TypeError);
  this.state.phase=true;
 }
 fixedUpdate(){
  this.state.steps++;
  if(this.state.steps===1){
   this.material.attach('patch',{kind:'body',target:world.entity('2'),forceLimit:1,torqueLimit:0});
   const before=JSON.stringify(this.material.state.attachments);
   for(const forceLimit of [-1,NaN,Infinity,'1'])this.rejects(()=>this.material.attach('patch',{kind:'body',target:world.entity('2'),forceLimit}),TypeError);
   this.rejects(()=>this.material.attach('patch',{kind:'body',target:world.entity('2'),torqueLimit:-1}),TypeError);
   this.rejects(()=>this.material.attach('patch',{kind:'world',forceLimit:1}),TypeError);
   this.rejects(()=>this.material.attach('patch',{kind:'body',target:world.entity('1'),forceLimit:1}),TypeError);
   this.rejects(()=>this.material.attach('patch',{kind:'body',target:world.entity('99999'),forceLimit:1}),TypeError);
   this.rejects(()=>this.material.attach('patch',{kind:'invalid',offset:{x:1,y:2,z:3}}),TypeError);
   this.rejects(()=>this.material.attach('patch',{kind:'body',target:world.entity('99999'),offset:{x:1,y:2,z:3}}),TypeError);
   this.rejects(()=>this.material.attach('missing',{kind:'body',target:world.entity('2'),forceLimit:1}),TypeError);
   if(JSON.stringify(this.material.state.attachments)!==before)throw Error('invalid mutation changed connection');
   this.power.set({sources:[{name:'source',customTorque:10}]});
   this.state.negatives=true;
  }
  const patch=this.material.state.attachments[0];
  if(patch){
   if(patch.group!=='patch'||patch.forceLimit!==1||patch.torqueLimit!==0||typeof patch.force.x!=='number'||typeof patch.moment.z!=='number')throw Error('snapshot shape');
   patch.forceLimit=999;patch.force.x=999;patch.released=true;
   const actual=this.material.state.attachments[0];if(actual.forceLimit!==1||actual.force.x===999)throw Error('live native memory');
   this.state.snapshot=true;
   if(actual.released){
    if(actual.active||this.power.state.links[0].connected||this.power.state.links[0].active)throw Error('broken physical path');
    this.power.set({links:[{name:'coupling',enabled:true}]});
    if(this.power.state.links[0].connected)throw Error('driver command repaired material');
    this.state.released=true;this.state.force=actual.peakForce;
   }
  }
 }
})JS";
 AssetDatabase db;db.Scan(root.string(),(root/"Assets").string());AssetRecord material,script;
 Check(db.Track((root/"Assets/patch.judasdeform").string(),material,error)&&db.Track((root/"Assets/probe.js").string(),script,error),"normal AssetDatabase registers material and public probe");
 ResourceManager resources(nullptr,&db);resources.AddRef(material.id);resources.RequestDeformable(material.id);resources.WaitForAll();resources.Pump(100);
 Check(bool(resources.GetDeformable(material.id,error)),"ordinary resource path publishes baked material without special native fixture injection");
 auto scene=Fixture(material.id,script.id);RuntimeWorld world;world.legacyGameplay=false;
 const bool built=world.Build(scene,&resources,error);if(!built)std::printf("BUILD_ERROR %s\n",error.c_str());
 Check(built,"ordinary runtime builds material, dynamic mount and conditional transmission");
 InputSystem input;world.UpdateScripts(&input,1.f/60);Step(world,input);
 auto* d=world.RuntimeDeformable(3,error);auto* power=world.RuntimePowertrain(4);
 Check(d&&d->settings.attachments.size()==1&&d->settings.attachments[0].forceLimit==1&&!d->released[0],"real VM authors finite dynamic material connection while unloaded negative stays intact");
 Check(power&&power->state.links[0].connected&&power->state.links[0].active&&power->state.shafts[1].angularVelocity>0,"connected material path transmits actual generic rotational source motion");
 world.Physics().SetLinearVelocity(world.RuntimeBody(2),{1,0,0});Step(world,input);
 Check(d->error.empty()&&d->released[0]&&d->attachmentState[0].peakForce>1,"physical body load causes actual material connection failure");
 Check(!power->state.links[0].connected&&!power->state.links[0].active,"published transmission disconnect derives from actual released material");
 Step(world,input);world.UpdateScripts(&input,1.f/60);
 const auto observed=State(world);
 Check(world.Scripts()->Diagnostics().empty()&&observed.value("started",false)&&observed.value("phase",false)&&observed.value("negatives",false)&&observed.value("snapshot",false)&&observed.value("released",false),"public VM validates finite options, update phase, value snapshots and physical disconnect");
 Check(!power->state.links[0].connected&&power->command.links[0].enabled,"ordinary enable command cannot bypass a broken physical load path");

 std::ofstream(root/"Assets/lifetime.js")<<R"JS(import {world} from 'judas';export default class {
 constructor(){this.material=world.entity('3').deformable;this.state={};}
 fixedUpdate(){if(!this.material.valid){let read=false,write=false;try{this.material.state;}catch(e){read=e instanceof ReferenceError;}try{this.material.attach('patch',{kind:'world'});}catch(e){write=e instanceof ReferenceError;}if(!read||!write)throw Error('stale material');this.state.stale=true;}}
})JS";
 AssetRecord lifetime;Check(db.Track((root/"Assets/lifetime.js").string(),lifetime,error),"ordinary retained-handle lifetime probe registered");
 auto lifeScene=scene;lifeScene.Find(3)->scripts.clear();lifeScene.Find(2)->scripts={{1,lifetime.id,true,"{}"}};RuntimeWorld life;life.legacyGameplay=false;
 Check(life.Build(lifeScene,&resources,error),"independent material lifetime world builds");Step(life,input);
 Check(life.DestroyEntity(3),"normal owning material destruction succeeds");Step(life,input);
 Check(life.Scripts()->Diagnostics().empty()&&State(life).value("stale",false),"destroyed generation-safe material handle invalidates and throws on state and mutations");life.Destroy();

 fs::copy_file("docs/judasjs/examples/mechanical-damage.js",root/"Assets/example.js",fs::copy_options::overwrite_existing);AssetRecord example;
 Check(db.Track((root/"Assets/example.js").string(),example,error),"exact cookbook file registered as project script");
 auto cookbookScene=Fixture(material.id,example.id);DeformableAttachment attachment;attachment.group="patch";attachment.kind=DeformableAttachment::Kind::Body;attachment.target=2;attachment.forceLimit=1;cookbookScene.Find(3)->deformable->attachments={attachment};
 RuntimeWorld cookbook;cookbook.legacyGameplay=false;Check(cookbook.Build(cookbookScene,&resources,error),"exact cookbook scene uses normal authored finite connection");
 InputMap map;map.Add("test_load",false);map.AddBinding("test_load",{"key:I"});Check(input.SetMap(map,error),"project logical input map binds the cookbook load action");input.SetPhysical("key:I",1);
 Step(cookbook,input);Step(cookbook,input);const auto sample=State(cookbook);auto* physical=cookbook.RuntimeDeformable(3,error);
 Check(cookbook.Scripts()->Diagnostics().empty()&&sample.value("loaded",false)&&sample.value("released",false)&&physical&&physical->released[0],"exact cookbook applies physical impulse and observes real released attachment without script break");
 cookbook.Destroy();world.Destroy();resources.ReleaseRef(material.id);resources.Shutdown();std::printf("M83 mechanical damage JS %u PASS\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"M83 mechanical damage JS FAIL after %u: %s\n",checks,e.what());return 1;}}
