#include "WorldPersistence.h"
#include "ScriptSystem.h"
#include "BuildIdentity.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
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
    Check(!world.Scripts()||world.Scripts()->InspectPhase(id,4)!=ScriptInstancePhase::Running,"inspection does not start callbacks");
    InputSystem input;input.SetPhysical("key:E",1);world.UpdateScripts(&input,.02f);world.FixedScripts(&input,1.f/60);world.FixedScripts(&input,1.f/60);
    Check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),"real VM runs callbacks without faults");
    Check(world.Scripts()->InspectPhase(id,999)==ScriptInstancePhase::Absent,"missing slot is not ready");
    Check(world.Scripts()->InspectPhase(id,4)==ScriptInstancePhase::Running,"completed lifecycle callback is observable without JSON capture");
    auto states=world.Scripts()->Capture();for(const auto& state:states)std::printf("STATE %llu/%llu %s\n",(unsigned long long)state.entity,(unsigned long long)state.slot,state.json.c_str());
    Check(states.size()==2&&states[0].json.find("\"frames\":2")!=std::string::npos&&states[0].json.find("\"steps\":2")!=std::string::npos,"frame/fixed callbacks have separate cadence");
    Check(states.size()==2&&states[0].json.find("\"speed\":7")!=std::string::npos&&states[1].json.find("\"speed\":3")!=std::string::npos,"independent instances and authored overrides");
    auto saved=world.Scripts()->Capture(true);
    RuntimeWorld restored;Check(restored.Build(scene,&resources,error)&&restored.RestoreScriptState(saved,error),"script state restores before VM callbacks");restored.UpdateScripts(&input,.02f);
    auto after=restored.Scripts()->Capture();Check(after[0].json.find("\"frames\":4")!=std::string::npos,"restored state visible to resumed gameplay");
    auto bad=after;bad[0].json="not JSON";auto before=restored.Scripts()->Capture();Check(!restored.RestoreScriptState(bad,error)&&restored.Scripts()->Capture()[0].json==before[0].json,"invalid state leaves live script state unchanged");
    Check(ScriptSystem::ValidateJson("{\"x\":[true,null,2]}",error)&&!ScriptSystem::ValidateJson("{\"x\":1e999}",error),"JSON-like state accepts finite bounded data only");
    Scene prefab=scene;prefab.DestroyObject(id2);prefab.Find(id)->body=SceneBodyComponent{};prefab.Find(id)->body->motion=SceneBodyMotion::Dynamic;
    auto child=prefab.CreateObject("Child").id;prefab.Find(child)->parent=id;prefab.Find(child)->scripts.push_back({1,script.id,true,"{}"});
    Check(SaveSceneToFile(prefab,(root/"Assets/prop.judasprefab").string(),error),"scripted prefab uses ordinary component serialization");AssetRecord prefabAsset;
    Check(db.Track(fs::absolute(root/"Assets/prop.judasprefab").string(),prefabAsset,error),"scripted prefab registered normally");Scene instances;SceneTransform placement;SceneObjectId rootId;
    InstantiatePrefab(instances,prefab,prefabAsset.id,placement,rootId,error);auto edited=instances;edited.Find(rootId)->scripts[0].properties="{\"speed\":9}";CapturePrefabEdits(instances,edited);Scene resolved;
    Check(ResolvePrefabs(edited,&db,resolved,error)&&resolved.Find(rootId)->scripts[0].properties=="{\"speed\":9}","generic prefab property override preserves script data");
    placement.position={2,3,4};auto spawned=world.SpawnPrefab(prefabAsset.id,placement,error);
    Check(spawned&&world.RuntimeDefinition(spawned)->scripts.size()==1&&world.FindEntity(spawned),"runtime SpawnPrefab creates ordinary scripted hierarchy");
    world.UpdateScripts(&input,.01f);std::map<std::string,SaveChunk> spawnSave;Scene spawnScene;RuntimeWorld spawnRestored;
    Check(WorldPersistence::Capture(world,spawnSave,error)&&WorldPersistence::SceneFrom(spawnSave,spawnScene,error)&&spawnRestored.Build(spawnScene,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(spawnRestored,error)&&WorldPersistence::Restore(spawnRestored,spawnSave,error),"runtime-created script instance and state restore together");spawnRestored.UpdateScripts(&input,.01f);
    Check(spawnRestored.Scripts()->Capture().size()==4&&world.DestroyHierarchy(spawned,error)&&!world.RuntimeDefinition(spawned),"spawned behaviours independent and normal hierarchy destruction");spawnRestored.Destroy();
    std::ofstream(root/"Assets/native.js")<<R"JS(import {input,world,physics} from 'judas';
export default class {constructor({entity}){this.entity=entity;this.state={};}
start(){this.state.parent=this.entity.parent;this.state.classification=this.entity.classification;}
fixedUpdate(){this.entity.applyImpulse({x:1,y:0,z:0});this.entity.applyForce({x:0,y:1,z:0});this.entity.applyTorque({x:0,y:0,z:1});
 this.state.velocity=this.entity.velocity;this.entity.angularVelocity={x:0,y:0,z:2};this.state.angular=this.entity.angularVelocity;
 this.entity.addTag('target');this.state.tag=this.entity.hasTag('target');this.state.query=world.queryTags(['target']).length;
 this.state.overlap=world.overlap({x:-2,y:-2,z:-2},{x:2,y:2,z:2},{includeLayers:['Default'],requiredTags:['target']}).length;
 this.state.sweep=!!physics.capsuleCast({position:{x:-3,y:0,z:0},rotation:{x:0,y:0,z:0,w:1}},0.3,0.5,{x:1,y:0,z:0},5);
 this.entity.transform={position:{x:0,y:2,z:0}};this.state.position=this.entity.transform.position;this.state.input=input.pressed('interact');}
})JS";
    AssetRecord native;db.Track(fs::absolute(root/"Assets/native.js").string(),native,error);Scene nativeScene;
    auto& nativeObject=nativeScene.CreateObject("Native calls");nativeObject.body=SceneBodyComponent{};nativeObject.body->motion=SceneBodyMotion::Dynamic;nativeObject.scripts.push_back({1,native.id,true,"{}"});
    ProjectClassification categories;unsigned tag;categories.tags.Add("target",tag);RuntimeWorld nativeWorld;
    Check(nativeWorld.Build(nativeScene,&resources,error,&categories),"native binding world uses project classifications");InputSystem nativeInput;nativeInput.SetPhysical("key:G",1);nativeInput.SetPhysical("key:G",0);nativeInput.BeginFixedStep();nativeWorld.FixedScripts(&nativeInput,.016f);
    auto nativeStates=nativeWorld.Scripts()->Capture();auto nativeText=nativeStates.empty()?"":nativeStates[0].json;std::printf("NATIVE %s\n",nativeText.c_str());
    Check(nativeWorld.Scripts()->Diagnostics().empty()&&nativeText.find("\"input\":true")!=std::string::npos,"fixed script receives latched short logical-input press");
    Check(nativeText.find("\"tag\":true")!=std::string::npos&&nativeText.find("\"query\":1")!=std::string::npos&&nativeText.find("\"overlap\":1")!=std::string::npos,"tags and filtered authoritative physics queries");
    Check(nativeText.find("\"sweep\":true")!=std::string::npos,"explicit physics.capsuleCast hits a body in a scripted world");
    Check(nativeText.find("\"velocity\":{\"x\":1")!=std::string::npos&&nativeText.find("\"position\":{\"x\":0,\"y\":2")!=std::string::npos,"forces/impulse/velocity and immediate authoritative transform readback");nativeWorld.Destroy();
    std::ofstream(root/"Assets/runtime-controls.js")<<R"JS(
export default class {
  constructor({entity}) { this.entity=entity; this.state={}; }
  fixedUpdate() {
    const e=this.entity;
    this.state.com=e.centerOfMass;
    this.state.water=e.setBasicWater({flow:{x:2,y:0,z:0},opacity:0.5});
    try { e.setBasicWater({opacity:0.2,wavelength:0}); }
    catch { this.state.invalidWater=true; }
    this.state.filter=e.setCollisionFilter({layer:'Default',mask:[]});
    try { e.setCollisionFilter({mask:['Default','missing-layer']}); }
    catch { this.state.invalidFilter=true; }
    e.applyForceAtPoint({x:8,y:0,z:0},{x:2,y:1,z:0});
    this.state.camera=e.setCameraPresentation({fov:42,updateEveryFrames:3,renderLayers:[]});
    this.state.particles=e.setParticles({rate:0,size:0.4,velocity:{x:2,y:0,z:0},
      loop:false,localSpace:true,useGravity:true,lifetime:0.5,burst:2,maxParticles:2,seed:99,
      color:{r:1,g:0,b:0,a:1},endColor:{r:0,g:1,b:0,a:0}});
    try {e.setParticles({rate:33,textureAsset:'missing-texture'});}
    catch {this.state.invalidTexture=true;}
    try { e.setCameraPresentation({fov:70,renderLayers:['missing-layer']}); }
    catch { this.state.invalidCamera=true; }
    try { e.setParticles({rate:33,spread:{x:-1,y:0,z:0}}); }
    catch { this.state.invalidParticles=true; }
    const snapshot=e.particles;
    snapshot.velocity.x=100;
    snapshot.color.r=0;
    this.state.particleReadback=e.particles.velocity.x===2&&e.particles.color.r===1&&
      e.particles.textureAsset===null&&e.particles.seed===99&&e.particles.maxParticles===2;
    this.state.cameraReadback=e.camera.fov===42&&e.camera.updateEveryFrames===3&&
      Array.isArray(e.camera.renderLayers)&&e.camera.renderLayers.length===0;
    e.setCameraPresentation({renderLayers:null});
    this.state.allLayers=e.camera.renderLayers===null;
    e.setCameraPresentation({renderLayers:['Default']});
    this.state.namedLayers=e.camera.renderLayers.join(',')==='Default';
    e.setCameraPresentation({renderLayers:[]});
    this.state.nav=e.navigation.configure({cornerDistance:0.75});
    try {e.navigation.configure({cornerDistance:0,speed:99});}
    catch {this.state.invalidCorner=true;}

  }
})JS";
    AssetRecord controls;
    Check(db.Track(fs::absolute(root/"Assets/runtime-controls.js").string(),controls,error),"register runtime controls script");
    Scene controlsScene;
    auto& controlled=controlsScene.CreateObject("Runtime controls");
    controlled.transform.position={2,0,0};
    controlled.body=SceneBodyComponent{};
    controlled.body->motion=SceneBodyMotion::Dynamic;
    controlled.renderCamera=SceneRenderCameraComponent{};
    controlled.basicWater=BasicWaterSettings{};
    controlled.particleEmitter=ParticleEmitterSettings{};
    controlled.navigationAgent=NavigationAgentSettings{};
    controlled.scripts.push_back({1,controls.id,true,"{}"});
    RuntimeWorld controlsWorld;
    Check(controlsWorld.Build(controlsScene,&resources,error),"runtime controls fixture builds");
    Check(controlsWorld.EmitParticleBurst(controlled.id,4),"seed particles in the old coordinate space");
    controlsWorld.FixedScripts(nullptr,1.f/60);
    const auto observations=controlsWorld.Scripts()->Capture();
    const auto observed=observations.empty()?std::string{}:observations.front().json;
    Check(observed.find("\"com\":{\"x\":2")!=std::string::npos,
          "script reads the physical centre of mass");
    Check(observed.find("\"invalidWater\":true")!=std::string::npos&&
          observed.find("\"invalidFilter\":true")!=std::string::npos&&
          observed.find("\"invalidCamera\":true")!=std::string::npos&&
          observed.find("\"invalidParticles\":true")!=std::string::npos&&
          observed.find("\"invalidTexture\":true")!=std::string::npos,
          "invalid control patches throw through the public API");
    Check(observed.find("\"particleReadback\":true")!=std::string::npos&&
          observed.find("\"cameraReadback\":true")!=std::string::npos&&
          observed.find("\"allLayers\":true")!=std::string::npos&&
          observed.find("\"namedLayers\":true")!=std::string::npos,
          "camera layer masks and detached particle settings round trip through JudasJS");
    const auto& nav=*controlsWorld.RuntimeDefinition(controlled.id)->navigationAgent;
    Check(observed.find("\"nav\":true")!=std::string::npos&&
          observed.find("\"invalidCorner\":true")!=std::string::npos&&
          nav.cornerDistance==.75f&&nav.speed==3.f,
          "navigation corner tolerance reaches the agent and rejects invalid patches atomically");
    const auto waters=controlsWorld.BasicWaters();
    const auto swim=controlsWorld.SampleWater({2,-1,0},{0,1,0},0.5f,0.2f);
    Check(waters.size()==1&&waters.front().settings.opacity==0.5f&&swim.immersion>0.9f&&swim.velocity.x==2,
          "basic water patch reaches presentation and swimming and rejects invalid settings atomically");
    Check(controlsWorld.Scripts()->Diagnostics().empty(),"runtime controls run without unexpected script faults");
    const auto& cameras=controlsWorld.PresentationCameras();
    Check(cameras.size()==1&&cameras.front().settings.verticalFovDegrees==42&&
          cameras.front().settings.updateEveryFrames==3&&cameras.front().settings.renderMask==0,
          "camera patch reaches presentation and rejects an invalid layer atomically");
    const auto& emitters=controlsWorld.VisualEmitters();
    Check(emitters.size()==1&&emitters.front().pool.settings.size==0.4f&&
          emitters.front().pool.settings.rate==0&&emitters.front().pool.settings.velocity.x==2,
          "particle patch reaches the live emitter and rejects negative spread atomically");
    const auto& pool=emitters.front().pool;
    Check(pool.Particles().empty()&&pool.settings.localSpace&&pool.settings.useGravity&&
          !pool.settings.loop&&pool.settings.maxParticles==2&&pool.settings.seed==99,
          "coordinate-space change clears old particles and applies simulation options");
    controlsWorld.UpdateVisualParticles(.25f);
    auto& livePool=controlsWorld.VisualEmitters().front().pool;
    const auto& billboards=livePool.Presentation({0,0,0},{1,0,0,0},{1,1,1});
    Check(billboards.size()==2&&billboards[0].color==glm::vec4(1,0,0,1),
          "edited startup burst, capacity and colour reach live billboards");
    controlsWorld.UpdateVisualParticles(.6f);
    Check(pool.Particles().empty(),"edited lifetime expires particles without restarting a non-looping emitter");
    auto controlledBody=controlsWorld.RuntimeBody(controlled.id);
    unsigned collisionLayer=64;
    CategoryMask collisionMask=kAllCategories;
    Check(controlsWorld.Physics().GetCollisionFilter(controlledBody,collisionLayer,collisionMask)&&
          collisionLayer==0&&collisionMask==0&&controlsWorld.RuntimeDefinition(controlled.id)->body->collisionMask==0,
          "script collision filter updates native and retained settings atomically");
    controlsWorld.Physics().Step(1.f/60);
    Check(controlsWorld.Physics().GetLinearVelocity(controlledBody).x>0&&
          controlsWorld.Physics().GetAngularVelocity(controlledBody).z<0,
          "off-centre scripted force drives both translation and rotation");
    std::map<std::string,SaveChunk> controlsSave;
    Scene savedControlsScene;
    RuntimeWorld reloadedControls;
    const bool controlsRestored=WorldPersistence::Capture(controlsWorld,controlsSave,error)&&
        WorldPersistence::SceneFrom(controlsSave,savedControlsScene,error)&&
        reloadedControls.Build(savedControlsScene,&resources,error,nullptr,nullptr,true)&&
        WorldPersistence::Prepare(reloadedControls,error)&&
        WorldPersistence::Restore(reloadedControls,controlsSave,error);
    if(!controlsRestored)std::fprintf(stderr,"controls save: %s\n",error.c_str());
    Check(controlsRestored&&reloadedControls.VisualEmitters().size()==1&&
          ParticleSettingsEqual(reloadedControls.VisualEmitters().front().pool.settings,pool.settings)&&
          reloadedControls.VisualEmitters().front().pool.Particles().empty(),
          "modern save participants restore edited emitter settings with a transient particle pool");
    controlsWorld.Destroy();
    for(int count:{0,10,100}){Scene load;for(int i=0;i<count;++i){auto& o=load.CreateObject("light script");o.scripts.push_back({1,script.id,true,"{}"});}RuntimeWorld perf;perf.Build(load,&resources,error);perf.UpdateScripts(&input,.016f);
        auto start=std::chrono::steady_clock::now();for(int i=0;i<200;++i){perf.UpdateScripts(&input,.016f);perf.FixedScripts(&input,.016f);}
        std::printf("PERFORMANCE scripts=%d frame_plus_fixed_us=%.3f\n",count,std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/200);perf.Destroy();}
    auto fingerprint=world.BaselineFingerprint();std::ofstream(root/"Assets/helper.js")<<"export const increment=3;";RuntimeWorld changed;Check(changed.Build(scene,&resources,error)&&changed.BaselineFingerprint()!=fingerprint,"transitive script source change changes compatibility identity");
    std::ofstream(root/"Assets/fault.js")<<"export default class {update(){throw new Error('intentional fault')}}";
    std::ofstream(root/"Assets/loop.js")<<"export default class {update(){while(true){}}}";
    AssetRecord fault,loop;db.Track(fs::absolute(root/"Assets/fault.js").string(),fault,error);db.Track(fs::absolute(root/"Assets/loop.js").string(),loop,error);
    scene.Find(id)->scripts.push_back({5,fault.id,true,"{}"});scene.Find(id2)->scripts.push_back({3,loop.id,true,"{}"});
    RuntimeWorld hostile;Check(hostile.Build(scene,&resources,error),"hostile fixture builds on normal path");hostile.UpdateScripts(&input,.01f);hostile.UpdateScripts(&input,.01f);
    Check(hostile.Scripts()->InspectPhase(id,5)==ScriptInstancePhase::Faulted,"faulted callback cannot be mistaken for readiness");
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
    std::ofstream(root/"Assets/version.js") << "import {engine} from 'judas';export default class {constructor(){const info=engine.buildInfo;let frozen=false;try{info.version='wrong'}catch{frozen=true}this.state={version:info.version,revision:info.source_revision,frozen,unchanged:engine.buildInfo.version===info.version};}}";
    AssetRecord versionRecord;db.Track(fs::absolute(root/"Assets/version.js").string(),versionRecord,error);
    Scene versionScene;versionScene.CreateObject("version reader").scripts={{1,versionRecord.id,true,"{}"}};
    RuntimeWorld versionWorld;Check(versionWorld.Build(versionScene,&resources,error),"build version API consumer");
    versionWorld.UpdateScripts(&input,.01f);auto versionStates=versionWorld.Scripts()->Capture();
    Check(versionStates.size()==1&&versionStates[0].json.find(GetBuildIdentity().version)!=std::string::npos&&versionStates[0].json.find(GetBuildIdentity().sourceRevision)!=std::string::npos,"JudasJS reads actual embedded version and exact source revision");
    Check(versionStates.size()==1&&versionStates[0].json.find("\"frozen\":true")!=std::string::npos&&versionStates[0].json.find("\"unchanged\":true")!=std::string::npos,"script cannot mutate engine build provenance");
    versionWorld.Destroy();
    hostile.Destroy();world.Destroy();restored.Destroy();changed.Destroy();resources.Shutdown();
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
