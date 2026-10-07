#include "ScriptSystem.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "WorldState.h"
#include "InputSystem.h"
#include "Prefab.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdio>
namespace fs=std::filesystem;
int checks=0,failures=0;
void Check(bool ok,const char* label){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
int main(){
    fs::path root="build/m40-script-tests";fs::remove_all(root);fs::create_directories(root/"Assets");
    std::ofstream(root/"Assets/helper.js")<<"export const increment=2;";
    std::ofstream(root/"Assets/counter.js")<<R"JS(import {increment} from './helper.js';import {input,world,console} from 'judas';
export const properties={speed:{type:'number',default:3},label:{type:'string',default:'counter'},active:{type:'boolean',default:true}};
export default class {
 constructor({entity,properties}){this.entity=entity;this.properties=properties;this.state={frames:0,steps:0,start:0};}
 start(){this.state.start++;this.state.speed=this.properties.speed;}
 update(){this.state.frames+=increment;if(input.pressed('interact'))this.state.input=true;}
 fixedUpdate(){this.state.steps++;}
})JS";
    AssetDatabase db;db.Scan(fs::absolute(root).string(),fs::absolute(root/"Assets").string());AssetRecord helper,script;std::string error;
    Check(db.Track(fs::absolute(root/"Assets/helper.js").string(),helper,error)&&db.Track(fs::absolute(root/"Assets/counter.js").string(),script,error),"registered JavaScript assets");
    std::string schema;Check(ScriptSystem::Inspect(db,script.id,schema,error),"restricted module metadata and relative import");
    std::vector<ScriptProperty> properties;Check(ScriptSystem::ReadProperties(schema,"{\"speed\":7}",properties,error)&&properties.size()==3,"typed authored properties with source defaults");
    Scene scene;auto& a=scene.CreateObject("A");a.render=SceneRenderComponent{};a.scripts.push_back({4,script.id,true,"{\"speed\":7}"});auto id=a.id;
    auto& b=scene.CreateObject("B");b.scripts.push_back({2,script.id,true,"{}"});auto id2=b.id;
    std::string text;Scene copy;SaveSceneToString(scene,text);Check(LoadSceneFromString(text,copy,error)&&ScenesEqual(scene,copy),"script slots scene round-trip");
    ResourceManager resources(nullptr,&db);RuntimeWorld world;Check(world.Build(scene,&resources,error),"ordinary runtime constructs scripted world");
    InputSystem input;input.SetPhysical("key:E",1);world.UpdateScripts(&input,.02f);world.FixedScripts(&input,1.f/60);world.FixedScripts(&input,1.f/60);
    Check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),"real VM runs callbacks without faults");
    auto states=world.Scripts()->Capture();for(const auto& state:states)std::printf("STATE %llu/%llu %s\n",(unsigned long long)state.entity,(unsigned long long)state.slot,state.json.c_str());
    Check(states.size()==2&&states[0].json.find("\"frames\":2")!=std::string::npos&&states[0].json.find("\"steps\":2")!=std::string::npos,"frame/fixed callbacks have separate cadence");
    Check(states.size()==2&&states[0].json.find("\"speed\":7")!=std::string::npos&&states[1].json.find("\"speed\":3")!=std::string::npos,"independent instances and authored overrides");
    auto saved=CaptureWorldState(world);Check(SaveWorldStateToString(saved,text),"controlled script state saved");WorldState parsed;Check(LoadWorldStateFromString(text,parsed,error),"controlled script state parsed");
    RuntimeWorld restored;Check(restored.Build(scene,&resources,error)&&ApplyWorldState(restored,parsed,error),"script state restore before VM callbacks");restored.UpdateScripts(&input,.02f);
    auto after=restored.Scripts()->Capture();Check(after[0].json.find("\"frames\":4")!=std::string::npos,"restored state visible to resumed gameplay");
    auto bad=parsed;bad.scripts[0].slot=99;auto before=CaptureWorldState(restored);Check(!ApplyWorldState(restored,bad,error)&&CaptureWorldState(restored).scripts[0].json==before.scripts[0].json,"invalid slot leaves live state unchanged");
    Check(ScriptSystem::ValidateJson("{\"x\":[true,null,2]}",error)&&!ScriptSystem::ValidateJson("{\"x\":1e999}",error),"JSON-like state accepts finite bounded data only");
    Scene prefab=scene;prefab.DestroyObject(id2);prefab.Find(id)->body=SceneBodyComponent{};prefab.Find(id)->body->motion=SceneBodyMotion::Dynamic;
    auto child=prefab.CreateObject("Child").id;prefab.Find(child)->parent=id;prefab.Find(child)->scripts.push_back({1,script.id,true,"{}"});
    Check(SaveSceneToFile(prefab,(root/"Assets/prop.judasprefab").string(),error),"scripted prefab uses ordinary component serialization");AssetRecord prefabAsset;
    Check(db.Track(fs::absolute(root/"Assets/prop.judasprefab").string(),prefabAsset,error),"scripted prefab registered normally");Scene instances;SceneTransform placement;SceneObjectId rootId;
    InstantiatePrefab(instances,prefab,prefabAsset.id,placement,rootId,error);auto edited=instances;edited.Find(rootId)->scripts[0].properties="{\"speed\":9}";CapturePrefabEdits(instances,edited);Scene resolved;
    Check(ResolvePrefabs(edited,&db,resolved,error)&&resolved.Find(rootId)->scripts[0].properties=="{\"speed\":9}","generic prefab property override preserves script data");
    placement.position={2,3,4};auto spawned=world.SpawnPrefab(prefabAsset.id,placement,error);
    Check(spawned&&world.RuntimeDefinition(spawned)->scripts.size()==1&&world.FindEntity(spawned),"runtime SpawnPrefab creates ordinary scripted hierarchy");
    world.UpdateScripts(&input,.01f);auto spawnSave=CaptureWorldState(world);RuntimeWorld spawnRestored;
    Check(spawnRestored.Build(scene,&resources,error)&&ApplyWorldState(spawnRestored,spawnSave,error),"runtime-created script instance and state restore together");spawnRestored.UpdateScripts(&input,.01f);
    Check(spawnRestored.Scripts()->Capture().size()==4&&world.DestroyHierarchy(spawned,error)&&!world.RuntimeDefinition(spawned),"spawned behaviours independent and normal hierarchy destruction");spawnRestored.Destroy();
    std::ofstream(root/"Assets/native.js")<<R"JS(import {input,world} from 'judas';
export default class {constructor({entity}){this.entity=entity;this.state={};}
start(){this.state.parent=this.entity.parent;this.state.classification=this.entity.classification;}
fixedUpdate(){this.entity.applyImpulse({x:1,y:0,z:0});this.entity.applyForce({x:0,y:1,z:0});this.entity.applyTorque({x:0,y:0,z:1});
 this.state.velocity=this.entity.velocity;this.entity.angularVelocity={x:0,y:0,z:2};this.state.angular=this.entity.angularVelocity;
 this.entity.addTag('target');this.state.tag=this.entity.hasTag('target');this.state.query=world.queryTags(['target']).length;
 this.state.overlap=world.overlap({x:-2,y:-2,z:-2},{x:2,y:2,z:2},{includeLayers:['Default'],requiredTags:['target']}).length;
 this.state.sweep=world.sweepCapsule({x:-3,y:0,z:0},{x:5,y:0,z:0}).hit;
 this.entity.transform={position:{x:0,y:2,z:0}};this.state.position=this.entity.transform.position;this.state.input=input.pressed('interact');}
})JS";
    AssetRecord native;db.Track(fs::absolute(root/"Assets/native.js").string(),native,error);Scene nativeScene;
    auto& nativeObject=nativeScene.CreateObject("Native calls");nativeObject.body=SceneBodyComponent{};nativeObject.body->motion=SceneBodyMotion::Dynamic;nativeObject.scripts.push_back({1,native.id,true,"{}"});
    ProjectClassification categories;unsigned tag;categories.tags.Add("target",tag);RuntimeWorld nativeWorld;
    Check(nativeWorld.Build(nativeScene,&resources,error,&categories),"native binding world uses project classifications");InputSystem nativeInput;nativeInput.SetPhysical("key:G",1);nativeInput.SetPhysical("key:G",0);nativeInput.BeginFixedStep();nativeWorld.FixedScripts(&nativeInput,.016f);
    auto nativeStates=nativeWorld.Scripts()->Capture();auto nativeText=nativeStates.empty()?"":nativeStates[0].json;std::printf("NATIVE %s\n",nativeText.c_str());
    Check(nativeWorld.Scripts()->Diagnostics().empty()&&nativeText.find("\"input\":true")!=std::string::npos,"fixed script receives latched short logical-input press");
    Check(nativeText.find("\"tag\":true")!=std::string::npos&&nativeText.find("\"query\":1")!=std::string::npos&&nativeText.find("\"overlap\":1")!=std::string::npos,"tags and filtered authoritative physics queries");
    Check(nativeText.find("\"sweep\":true")!=std::string::npos,"world.sweepCapsule hits a body in a script-owned (non-legacy) world");
    Check(nativeText.find("\"velocity\":{\"x\":1")!=std::string::npos&&nativeText.find("\"position\":{\"x\":0,\"y\":2")!=std::string::npos,"forces/impulse/velocity and immediate authoritative transform readback");nativeWorld.Destroy();
    for(int count:{0,10,100}){Scene load;for(int i=0;i<count;++i){auto& o=load.CreateObject("light script");o.scripts.push_back({1,script.id,true,"{}"});}RuntimeWorld perf;perf.Build(load,&resources,error);perf.UpdateScripts(&input,.016f);
        auto start=std::chrono::steady_clock::now();for(int i=0;i<200;++i){perf.UpdateScripts(&input,.016f);perf.FixedScripts(&input,.016f);}
        std::printf("PERFORMANCE scripts=%d frame_plus_fixed_us=%.3f\n",count,std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/200);perf.Destroy();}
    auto fingerprint=world.BaselineFingerprint();std::ofstream(root/"Assets/helper.js")<<"export const increment=3;";RuntimeWorld changed;Check(changed.Build(scene,&resources,error)&&changed.BaselineFingerprint()!=fingerprint&&!ApplyWorldState(changed,parsed,error),"transitive script source change rejects old save");
    std::ofstream(root/"Assets/fault.js")<<"export default class {update(){throw new Error('intentional fault')}}";
    std::ofstream(root/"Assets/loop.js")<<"export default class {update(){while(true){}}}";
    AssetRecord fault,loop;db.Track(fs::absolute(root/"Assets/fault.js").string(),fault,error);db.Track(fs::absolute(root/"Assets/loop.js").string(),loop,error);
    scene.Find(id)->scripts.push_back({5,fault.id,true,"{}"});scene.Find(id2)->scripts.push_back({3,loop.id,true,"{}"});
    RuntimeWorld hostile;Check(hostile.Build(scene,&resources,error),"hostile fixture builds on normal path");hostile.UpdateScripts(&input,.01f);hostile.UpdateScripts(&input,.01f);
    Check(hostile.Scripts()->Diagnostics().size()==2&&hostile.Scripts()->Capture().size()==2,"throw/infinite loop fault only offending instances once");
    Check(hostile.DestroyEntity(id)&&!hostile.RuntimeDefinition(id),"ordinary entity destruction invalidates native script handle");hostile.UpdateScripts(&input,.01f);
    std::ofstream(root/"Assets/syntax.js")<<"export default class { invalid ! }";
    AssetRecord syntax;db.Track(fs::absolute(root/"Assets/syntax.js").string(),syntax,error);scene.Find(id)->scripts.push_back({6,syntax.id,true,"{}"});
    scene.Find(id2)->scripts.push_back({4,std::string(32,'0'),true,"{}"});RuntimeWorld broken;
    Check(broken.Build(scene,&resources,error),"syntax/missing script assets do not abort unrelated world construction");broken.UpdateScripts(&input,.01f);
    Check(broken.Scripts()->Diagnostics().size()==4&&broken.Scripts()->Capture().size()==2,"module and callback faults remain isolated with useful diagnostics");broken.Destroy();
    auto transform=world.RuntimeDefinition(id)->transform;transform.position={0,-5,0};world.SetRuntimeTransform(id,transform);world.RestoreAuthoredState();
    Check(world.RuntimeDefinition(id)->transform.position==scene.Find(id)->transform.position&&!world.Scripts(),"reset restores authored scripted static transforms and fresh session");
    std::ofstream(root/"Assets/stale.js")<<"import {world} from 'judas';export default class {constructor(){this.target=world.entity('2');this.state={};} update(){this.state.position=this.target.transform.position;}}";
    AssetRecord stale;db.Track(fs::absolute(root/"Assets/stale.js").string(),stale,error);Scene staleScene;
    auto& owner=staleScene.CreateObject("handle owner");owner.scripts.push_back({1,stale.id,true,"{}"});auto& target=staleScene.CreateObject("target");target.body=SceneBodyComponent{};target.body->motion=SceneBodyMotion::Dynamic;
    RuntimeWorld staleWorld;staleWorld.Build(staleScene,&resources,error);staleWorld.UpdateScripts(&input,.01f);staleWorld.DestroyEntity(2);staleWorld.UpdateScripts(&input,.01f);
    Check(staleWorld.Scripts()->Diagnostics().size()==1&&staleWorld.Scripts()->Diagnostics()[0].message.find("stale or invalid entity")!=std::string::npos,"retained JS wrapper rejects destroyed native entity");staleWorld.Destroy();
    std::ofstream(root/"Assets/order.js")<<"let count=0;export default class {constructor(){this.state={};}start(){this.state.order=++count;}update(){}}";
    AssetRecord ordered;db.Track(fs::absolute(root/"Assets/order.js").string(),ordered,error);Scene orderScene;auto& orderObject=orderScene.CreateObject("ordered slots");orderObject.scripts={{2,ordered.id,true,"{}"},{1,ordered.id,true,"{}"}};
    RuntimeWorld orderWorld;orderWorld.Build(orderScene,&resources,error);orderWorld.UpdateScripts(&input,.01f);auto orderedStates=orderWorld.Scripts()->Capture();
    Check(orderedStates[0].slot==1&&orderedStates[0].json.find("\"order\":2")!=std::string::npos&&orderedStates[1].json.find("\"order\":1")!=std::string::npos,"authored slot order and shared session module caching");orderWorld.Destroy();
    Check(ScriptSystem::Inspect(db,ordered.id,schema,error),"no-world inspector reads code without constructing gameplay instances");
    hostile.Destroy();world.Destroy();restored.Destroy();changed.Destroy();resources.Shutdown();
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
