// M69: real QuickJS/public Judas API on the unchanged authoritative query path.
// One fixture covers correctness, prevalidation and a matched warmed 100-ray cost.
#include "ScriptSystem.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "PerformanceProfiler.h"
#include "InputSystem.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs=std::filesystem;
using Json=nlohmann::json;
namespace {
unsigned checks=0;
void Check(bool value,const char* message){++checks;std::printf("%s %s\n",value?"PASS":"FAIL",message);if(!value)throw std::runtime_error(message);}
Json State(RuntimeWorld& world){auto states=world.Scripts()->Capture();if(states.size()!=1)throw std::runtime_error("one probe state required");return Json::parse(states[0].json);}
void Phase(RuntimeWorld& world,const char* phase){auto states=world.Scripts()->Capture();states[0].json=Json{{"phase",phase},{"checks",0}}.dump();std::string error;Check(world.Scripts()->Restore(states,error),"normal script state selects focused fixture phase");}
struct Counts {double bridges=0,filters=0,queries=0,geometryMs=0;};
Counts RecordedStep(RuntimeWorld& world,InputSystem& input){
 auto& profiler=PerformanceProfiler::Get();profiler.Clear();profiler.Enable(true);profiler.RegisterThread("M69 query fixture");
 {ProfileFrame frame("M69 query fixture");world.FixedScripts(&input,1.f/60);}
 Counts count;auto history=profiler.History();if(history.empty())throw std::runtime_error("profiler frame unavailable");
 for(const auto& counter:history.back().counters){if(counter.name=="Physics query bridge calls")count.bridges=counter.value;if(counter.name=="Physics query filter preparations")count.filters=counter.value;if(counter.name=="Physics geometric queries")count.queries=counter.value;}
 for(const auto& scope:history.back().scopes)if(scope.name=="Physics query geometry")count.geometryMs+=double(scope.end-scope.start)/1e6;
 profiler.Enable(false);return count;
}
struct Cost {double medianUs=0,maximumUs=0;};
Cost Warmed(RuntimeWorld& world,InputSystem& input){
 // The same cached rays, filter, VM, geometry and 100 real queries in both modes.
 for(unsigned i=0;i<5;++i)world.FixedScripts(&input,1.f/60);
 std::vector<double> elapsed;for(unsigned i=0;i<21;++i){auto before=std::chrono::steady_clock::now();world.FixedScripts(&input,1.f/60);elapsed.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-before).count());}
 std::sort(elapsed.begin(),elapsed.end());return {elapsed[elapsed.size()/2],elapsed.back()};
}
}
int main(int argc,char** argv){try{
 const auto root=fs::absolute(argc>1?fs::path(argv[1]):fs::path("build/m69-query-fixture"));fs::create_directories(root/"Assets");
 std::ofstream(root/"Assets/batch.js")<<R"JS(import {physics,world} from 'judas';
const origin={x:0,y:0,z:0},direction={x:1,y:0,z:0},pose={position:origin};
// A ray at y=4 clears the dynamic box, but a sphere/capsule there reaches it.
// Keep this common miss clear of the entire finite swept volume for all shapes.
const ray={origin,direction,maximum:20},miss={origin:{x:0,y:8,z:0},direction,maximum:20};
const sphere={...ray,radius:.5},capsule={pose,radius:.25,halfHeight:.75,direction,maximum:20};
export default class {
 constructor(){this.state={checks:0,phase:'correctness'};this.rays=Array.from({length:100},(_,i)=>({...ray,direction:{x:1,y:(i-50)/1000,z:0}}));this.filter={includeLayers:['Default']};}
 check(value,label){if(!value)throw Error(label);this.state.checks++;}
 near(value,expected){return Math.abs(value-expected)<4e-5;}
 same(a,b){this.check(JSON.stringify(a)===JSON.stringify(b),'scalar/batch snapshot mismatch');}
 rejects(callback,index=null){let caught=false;try{callback();}catch(e){caught=e instanceof TypeError||e instanceof RangeError;this.check(index===null||e.message.includes('['+index+']'),'indexed request diagnostic');}this.check(caught,'invalid request must reject');}
 start(){
  const nearest=physics.raycast(origin,direction,20),hits=physics.raycastMany([ray,miss,ray]);
  this.same(nearest,hits[0]);this.same(nearest,hits[2]);this.check(hits.length===3&&hits[1]===null,'order and miss preserved');
  this.check(nearest.entity.id==='2'&&nearest.entity.valid&&this.near(nearest.distance,3)&&this.near(nearest.fraction,.15),'independent nearest distance/entity');
  this.check(this.near(nearest.point.x,3)&&this.near(nearest.normal.x,-1)&&nearest.shape==='box','independent point/normal/shape');this.saved=nearest;
  const filters=[{includeLayers:['Other']},{excludeLayers:['Default']},{requiredTags:['wanted']},{excludedTags:['wanted']},{ignored:[world.entity('2')]},{includeSensors:true}];
  for(const filter of filters)this.same(physics.raycast(origin,direction,20,filter),physics.raycastMany([ray],filter)[0]);
  this.check(physics.raycastMany([ray],filters[0])[0]===null,'include layer miss');
  this.check(physics.raycastMany([ray],filters[3])[0].entity.id==='3','excluded tag reaches far body');
  this.check(physics.raycastMany([ray],filters[4])[0].entity.id==='3','ignored entity reaches far body');
  this.check(physics.raycastMany([ray],filters[5])[0].entity.id==='4','sensor opt-in nearest');
  this.check(physics.raycastMany([ray])[0].entity.id==='2','disabled and default sensor omitted');
  const rotated={origin,direction:{x:0,y:0,z:-1},maximum:10},rh=physics.raycastMany([rotated])[0];
  this.same(rh,physics.raycast(origin,rotated.direction,10));this.check(rh.entity.id==='5'&&this.near(rh.distance,5-Math.SQRT2),'rotated box analytic ray');
  const inside={origin:{x:4,y:0,z:0},direction,maximum:0},ih=physics.raycastMany([inside])[0];
  this.check(ih.initialOverlap&&ih.distance===0&&ih.fraction===0,'zero distance overlap');
  const shapes=physics.sphereCastMany([sphere,{...sphere,origin:miss.origin},sphere]);
  this.same(shapes[0],physics.sphereCast(origin,.5,direction,20));this.same(shapes[0],shapes[2]);
  this.check(shapes[1]===null&&this.near(shapes[0].distance,2.5)&&this.near(shapes[0].point.x,3),'sphere independent witness/order/miss');
  const capsules=physics.capsuleCastMany([capsule,{...capsule,pose:{position:miss.origin}},capsule]);
  this.same(capsules[0],physics.capsuleCast(pose,.25,.75,direction,20));this.same(capsules[0],capsules[2]);
  this.check(capsules[1]===null&&this.near(capsules[0].distance,2.75),'capsule independent distance/order/miss');
  const quaternion={w:Math.cos(.35),x:0,y:0,z:Math.sin(.35)},rotatedCapsule={...capsule,pose:{position:origin,rotation:quaternion}};
  this.same(physics.capsuleCastMany([rotatedCapsule])[0],physics.capsuleCast(rotatedCapsule.pose,.25,.75,direction,20));
  this.same(physics.sphereCastMany([{...sphere,radius:0}])[0],physics.raycast(origin,direction,20));
  this.same(physics.capsuleCastMany([{...capsule,radius:0,halfHeight:0}])[0],physics.capsuleCast(pose,0,0,direction,20));
  for(const filter of filters){this.same(physics.sphereCastMany([sphere],filter)[0],physics.sphereCast(origin,.5,direction,20,filter));this.same(physics.capsuleCastMany([capsule],filter)[0],physics.capsuleCast(pose,.25,.75,direction,20,filter));}
  for(const [many,entry] of [[physics.raycastMany,ray],[physics.sphereCastMany,sphere],[physics.capsuleCastMany,capsule]]){
   this.check(many([]).length===0,'empty batch');this.check(many(Array(256).fill(entry)).length===256,'256 accepted');
   this.rejects(()=>many(Array(257).fill(entry)));this.rejects(()=>many([entry,{...entry,direction:{x:0,y:0,z:0}},entry]),1);this.rejects(()=>many([entry,{...entry,maximum:NaN},entry]),1);
  }
  this.rejects(()=>physics.raycastMany([ray,null,ray]),1);this.rejects(()=>physics.raycastMany({}));
  this.rejects(()=>physics.raycastMany([ray,{...ray,maximum:-1}]),1);
  this.rejects(()=>physics.sphereCastMany([sphere,{...sphere,radius:-1}]),1);
  this.rejects(()=>physics.capsuleCastMany([capsule,{...capsule,halfHeight:-1}]),1);
  this.rejects(()=>physics.capsuleCastMany([capsule,{...capsule,pose:{position:origin,rotation:{w:0,x:0,y:0,z:0}}}]),1);
  this.rejects(()=>physics.raycastMany([ray],{includeLayers:['does-not-exist']}));
  const runtime=physics.raycastMany([ray],{requiredTags:['runtime']})[0];this.check(runtime&&runtime.entity.valid&&this.near(runtime.distance,11.5),'runtime prefab query participates');
  const dynamic=physics.raycastMany([{origin:{x:0,y:3,z:0},direction,maximum:20}])[0];this.check(dynamic?.entity.id==='7'&&this.near(dynamic.distance,3.5),'dynamic body query');
  this.state.ready=true;
 }
 fixedUpdate(){
  if(this.state.phase==='invalid'){for(const [many,entry] of [[physics.raycastMany,ray],[physics.sphereCastMany,sphere],[physics.capsuleCastMany,capsule]])this.rejects(()=>many([entry,{...entry,direction:{x:0,y:0,z:0}},entry]),1);return;}
  if(this.state.phase==='lifetime'){
   this.check(!this.saved.entity.valid,'snapshot entity handle invalid after destruction');let stale=false;try{this.saved.entity.transform;}catch(e){stale=e instanceof ReferenceError;}this.check(stale,'stale hit access raises ReferenceError');
   const hit=physics.raycastMany([ray],{ignored:[this.saved.entity]})[0];this.same(hit,physics.raycast(origin,direction,20,{ignored:[this.saved.entity]}));
   this.check(hit&&hit.bodyId!==this.saved.bodyId&&hit.entity?.valid&&this.near(hit.distance,3.5),'stale ignore cannot alias reused runtime body');return;
  }
  const hits=this.state.phase==='scalar'?this.rays.map(r=>physics.raycast(r.origin,r.direction,r.maximum,this.filter)):physics.raycastMany(this.rays,this.filter);
  this.state.checksum=hits.reduce((sum,hit)=>sum+(hit?.distance??0),0);this.state.results=hits.length;
 }
})JS";
 AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());AssetRecord script,prefab;std::string error;
 Check(assets.Track((root/"Assets/batch.js").string(),script,error),"normal registered script asset");
 ProjectClassification categories;unsigned wanted=0,runtimeTag=0,otherLayer=0;
 Check(categories.tags.Add("wanted",wanted)&&categories.tags.Add("runtime",runtimeTag)&&categories.collision.Add("Other",otherLayer),"ordinary project category registries");
 Scene definition;auto owner=definition.CreateObject("Query script").id;definition.Find(owner)->scripts.push_back({1,script.id,true,"{}"});
 auto box=[&](const char* name,glm::vec3 position,glm::vec3 extents){auto id=definition.CreateObject(name).id;auto* object=definition.Find(id);object->transform.position=position;object->body=SceneBodyComponent{};object->body->halfExtents=extents;return id;};
 auto near=box("Nearest",{4,0,0},{1,1,1});definition.Find(near)->tags=CategoryBit(wanted);
 box("Farther",{8,0,0},{1,1,1});auto sensor=box("Sensor",{2,0,0},{.1f,.1f,.1f});definition.Find(sensor)->body->sensor=true;
 auto rotated=box("Rotated",{0,0,-5},{1,1,1});definition.Find(rotated)->transform.rotation=glm::angleAxis(glm::radians(45.f),glm::vec3(0,1,0));
 auto disabled=box("Disabled",{1,0,0},{.1f,.1f,.1f});definition.Find(disabled)->body->enabled=false;
 auto dynamic=box("Dynamic",{4,3,0},{.5f,.5f,.5f});definition.Find(dynamic)->body->motion=SceneBodyMotion::Dynamic;
 Scene prefabScene;auto spawned=prefabScene.CreateObject("Runtime box").id;auto* spawn=prefabScene.Find(spawned);spawn->body=SceneBodyComponent{};spawn->tags=CategoryBit(runtimeTag);
 Check(SaveSceneToFile(prefabScene,(root/"Assets/query.judasprefab").string(),error)&&assets.Track((root/"Assets/query.judasprefab").string(),prefab,error),"ordinary prefab asset");
 ResourceManager resources(nullptr,&assets);RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(definition,&resources,error,&categories),"normal authoritative runtime world");
 SceneTransform placement;placement.position={12,0,0};Check(world.SpawnPrefab(prefab.id,placement,error)!=0,"normal runtime prefab spawn");
 auto nearBody=world.RuntimeBody(near);auto dynamicBody=world.RuntimeBody(dynamic);auto nearBefore=world.Physics().GetTransform(nearBody);auto dynamicBefore=world.Physics().GetTransform(dynamicBody);
 InputSystem input;world.UpdateScripts(&input,1.f/60);
 for(const auto& diagnostic:world.Scripts()->Diagnostics())std::printf("SCRIPT FAILURE %s\n",diagnostic.message.c_str());
 Check(world.Scripts()->Diagnostics().empty(),"real VM correctness phase without faults");auto state=State(world);Check(state.value("ready",false)&&state.value("checks",0)>=65,"public batch/scalar, independent geometric and limit assertions");
 std::printf("VM_CORRECTNESS_CHECKS %u\n",state["checks"].get<unsigned>());
 auto nearAfter=world.Physics().GetTransform(nearBody);auto dynamicAfter=world.Physics().GetTransform(dynamicBody);
 Check(nearBefore.position==nearAfter.position&&nearBefore.rotation==nearAfter.rotation&&dynamicBefore.position==dynamicAfter.position&&dynamicBefore.rotation==dynamicAfter.rotation&&world.Physics().GetLinearVelocity(dynamicBody)==glm::vec3(0),"queries never mutate authoritative poses or velocity");
 Phase(world,"invalid");auto invalid=RecordedStep(world,input);Check(invalid.bridges==3&&invalid.filters==3&&invalid.queries==0,"invalid middle entry executes zero geometry queries in all three batches");
 Phase(world,"scalar");auto scalarCount=RecordedStep(world,input);auto scalarState=State(world);auto scalarCost=Warmed(world,input);
 Phase(world,"batch");auto batchCount=RecordedStep(world,input);auto batchState=State(world);auto batchCost=Warmed(world,input);
 Check(scalarCount.bridges==100&&batchCount.bridges==1,"100 scalar bridges versus exactly one accepted batch bridge");
 Check(scalarCount.filters==100&&batchCount.filters==1&&scalarCount.queries==100&&batchCount.queries==100,"filter preparation once, same 100 actual geometric queries");
 Check(scalarState["results"]==batchState["results"]&&std::abs(scalarState["checksum"].get<double>()-batchState["checksum"].get<double>())<1e-8,"matched modes preserve result/checksum");
 Json measurement={{"rays",100},{"warmupCallbacks",5},{"samples",21},{"scalar",{{"bridgeCalls",scalarCount.bridges},{"filterPreparations",scalarCount.filters},{"geometricQueries",scalarCount.queries},{"medianCallbackUs",scalarCost.medianUs},{"maximumCallbackUs",scalarCost.maximumUs},{"profiledGeometryMs",scalarCount.geometryMs}}},{"batch",{{"bridgeCalls",batchCount.bridges},{"filterPreparations",batchCount.filters},{"geometricQueries",batchCount.queries},{"medianCallbackUs",batchCost.medianUs},{"maximumCallbackUs",batchCost.maximumUs},{"profiledGeometryMs",batchCount.geometryMs}}}};
 std::ofstream(root/"measurement.json")<<measurement.dump(2)<<'\n';std::printf("MATCHED_MEASUREMENT %s\n",measurement.dump().c_str());
 Check(world.DestroyEntity(near),"normal entity destruction");placement.position={4,0,0};auto replacement=world.SpawnPrefab(prefab.id,placement,error);Check(replacement&&world.RuntimeBody(replacement).id!=nearBody.id,"body reuse changes generation");
 Phase(world,"lifetime");world.FixedScripts(&input,1.f/60);
 for(const auto& diagnostic:world.Scripts()->Diagnostics())std::printf("SCRIPT FAILURE %s\n",diagnostic.message.c_str());
 Check(world.Scripts()->Diagnostics().empty()&&State(world).value("checks",0)>=4,"destroyed hit and stale ignore remain safe through native VM");
 world.Destroy();resources.Shutdown();std::printf("M69 batch queries: %u C++ checks PASS\n",checks);return 0;
}catch(const std::exception& error){std::fprintf(stderr,"M69 batch queries FAIL after %u checks: %s\n",checks,error.what());return 1;}}
