#include "RuntimeWorld.h"
#include "EngineHost.h"
#include "GameSession.h"
#include "Simulation.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "NamedAuthoring.h"
#include "WorldPersistence.h"
#include "Prefab.h"
#include "WorldStreaming.h"
#include "editor/EditorDocument.h"
#include <thread>
#include <chrono>
#include <glm/gtc/quaternion.hpp>
#include <filesystem>
#include <fstream>
#include <limits>
#include <cstdio>
namespace fs=std::filesystem;
unsigned checks=0,failures=0;
void Check(bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
bool Near(glm::vec3 a,glm::vec3 b,float tolerance=1e-4f){return glm::length(a-b)<tolerance;}
Scene Fixture(){
 Scene scene;
 auto& field=scene.CreateObject("Ordinary spatial gravity");field.id=1;
 field.gravity=SceneGravityComponent{};field.gravity->kind=SceneGravityKind::Uniform;field.gravity->regionRadius=1000;
 auto& wall=scene.CreateObject("Wall field outside its bounds");wall.id=2;wall.transform.position={100,0,0};
 wall.transform.rotation=glm::angleAxis(glm::radians(90.f),glm::vec3(0,0,1));
 wall.gravity=SceneGravityComponent{};wall.gravity->kind=SceneGravityKind::Uniform;wall.gravity->magnitude=5;wall.gravity->regionRadius=2;
 auto& radial=scene.CreateObject("RadicalGravity source");radial.id=3;radial.transform.position={100,20,0};
 radial.gravity=SceneGravityComponent{};radial.gravity->magnitude=10;radial.gravity->regionRadius=1;
 auto& floor=scene.CreateObject("Physical floor");floor.id=4;floor.body=SceneBodyComponent{};floor.body->halfExtents={20,.1f,20};
 auto& surface=scene.CreateObject("Physical wall");surface.id=5;surface.transform.position={3.1f,5,0};surface.body=SceneBodyComponent{};surface.body->halfExtents={.1f,5,20};
 auto& character=scene.CreateObject("Independent motor");character.id=10;character.transform.position={0,1,0};character.characterMotor=CharacterMotorSettings{};
 auto& other=scene.CreateObject("Independent ordinary motor");other.id=11;other.transform.position={-3,1,0};other.characterMotor=CharacterMotorSettings{};
 for(int i=0;i<2;++i){auto& body=scene.CreateObject("Physical prop");body.id=20+i;body.transform.position={-5.f,5.f,float(i*2)};body.body=SceneBodyComponent{};body.body->motion=SceneBodyMotion::Dynamic;body.body->halfExtents={.2f,.2f,.2f};}
 scene.SetNextId(30);return scene;
}
int main(int argc,char** argv){
 fs::path output=argc>1?argv[1]:".cache/gravity-selection/checks";fs::create_directories(output);
 std::string error;
 {Scene scene;auto& field=scene.CreateObject("Oblique uniform field");field.gravity=SceneGravityComponent{};field.gravity->kind=SceneGravityKind::Uniform;
 field.gravity->regionShape=SceneRegionShape::Box;field.gravity->regionHalfExtents={2,.2f,2};field.transform.rotation=glm::angleAxis(glm::radians(90.f),glm::vec3(0,0,1));
 RuntimeWorld world;Check(world.Build(scene,nullptr,error),"oriented root zone builds");
 Check(Near(world.Gravity().Sample({0,1,0}),{9.81f,0,0}),"root zone uses authored rotated bounds");
 Check(Near(world.Gravity().Sample({1,0,0}),{0,0,0}),"outside rotated zone stays unclaimed");}
 EngineHost host;Check(host.Init("Gravity selection",320,240,false,error),"ordinary real host");if(failures)return 1;
 auto fixture=fs::absolute(output/"fixture");fs::create_directories(fixture/"Assets");
 const AssetId scriptId="73737373737373737373737373737301";
 std::ofstream(fixture/"Assets/proof.js")<<R"JS(import {world,physics} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;}
 start(){this.state={};this.source=world.entity('2');this.other=world.entity('20');this.otherGravity=this.other.gravity;const g=this.entity.gravity;
  this.state.spatial=g.state.mode==='spatial';
  g.select(world.entity('2'));this.state.field=g.state.available&&Math.abs(g.acceleration.x-5)<.0001;
  this.state.motor=Math.abs(this.entity.character.gravity.x-5)<.0001;
  this.state.world=Math.abs(physics.gravity(this.entity.transform.position).y+9.81)<.0001;
  try{g.setUniform({x:NaN,y:0,z:0});}catch(e){this.state.invalid=e instanceof TypeError&&g.state.mode==='field';}
  try{g.select(world.entity('4'));}catch(e){this.state.nonField=e instanceof TypeError;}
  g.setUniform({x:0,y:0,z:-9.81});this.state.uniform=Math.abs(g.acceleration.z+9.81)<.0001;
  g.setUniform({x:0,y:0,z:0});this.state.zero=g.acceleration.z===0;
  g.clear();this.state.clear=g.state.mode==='spatial';
 }
 update(){
  if(this.source&&!this.source.valid&&!this.state.staleSource){try{this.entity.gravity.select(this.source);}catch(e){this.state.staleSource=e instanceof ReferenceError;}}
  if(this.other&&!this.other.valid&&!this.state.staleOwner){try{this.otherGravity.clear();}catch(e){this.state.staleOwner=e instanceof ReferenceError;}}
 }
 fixedUpdate(){if(this.state.fixed)return;this.state.fixed=true;this.entity.gravity.setUniform({x:9.81,y:0,z:0});}
})JS";
 Check(AssetDatabase::WriteMeta((fixture/"Assets/proof.js").string()+kAssetMetaExtension,scriptId,AssetType::Script,"proof.js",error),"normal script metadata");
 host.OpenProjectAssets(fixture.string(),(fixture/"Assets").string());auto scene=Fixture();scene.Find(10)->scripts.push_back({1,scriptId,true,"{}"});
 RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(scene,&host.Resources(),error),"ordinary mixed world builds");
 host.Resources().WaitForAll();world.UpdateScripts(&host.GetWindow().Input(),1.f/60);
 Check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),"actual public VM calls fault-free");
 auto states=world.Scripts()->Capture();std::string state=states.empty()?"":states.front().json;
 for(const char* key:{"spatial","field","motor","world","invalid","nonField","uniform","zero","clear"})Check(state.find(std::string("\"")+key+"\":true")!=std::string::npos,key);
 Check(Near(world.SampleEntityGravity(11,{-3,1,0}),{0,-9.81f,0}),"unselected neighbour retains FaithfulGravity");
 GravitySelection selection;selection.mode=GravitySelection::Mode::Field;selection.source=3;
 Check(world.SetGravitySelection(20,selection,error),"select RadicalGravity without zone membership");
 Check(world.SetGravitySelection(2,selection,error),"source entities may have their own independent intent");
 auto direct=selection;direct.source=2;world.SetGravitySelection(11,direct,error);
 Check(Near(world.SampleEntityGravity(11,{-3,1,0}),{5,0,0}),"field selection samples actual source, never follows selection chains");
 world.SetGravitySelection(11,std::nullopt,error);

 auto expected=glm::normalize(glm::vec3(100,20,0)-world.RuntimeDefinition(20)->transform.position)*10.f;
 Check(Near(world.SampleEntityGravity(20,world.RuntimeDefinition(20)->transform.position),expected),"selected RadicalGravity remains position-addressed");
 selection.mode=GravitySelection::Mode::Uniform;selection.source=0;selection.acceleration={0,0,-9.81f};
 Check(world.SetGravitySelection(20,selection,error),"rigid body accepts uniform override");
 GameSession game;Check(game.Begin(world,error),"ordinary fixed-step game begins");
 for(int i=0;i<90;++i){host.GetWindow().Input().BeginFixedStep();StepPlayedWorld(game,host.GetWindow(),1.f/60);}
 auto* motor=world.RuntimeCharacter(10);auto* other=world.RuntimeCharacter(11);
 Check(world.Scripts()->Diagnostics().empty(),"fixedUpdate selection is fault-free");
 Check(motor&&motor->result.supported&&glm::dot(motor->result.supportNormal,glm::vec3(-1,0,0))>.99f,"selected motor lands on physical wall");
 Check(motor&&glm::dot(motor->orientation*glm::vec3(0,1,0),glm::vec3(-1,0,0))>.99f,"capsule up reorients to selected field");
 Check(other&&other->result.supported&&Near(other->result.gravity,{0,-9.81f,0}),"other motor remains on ordinary floor");
 auto body=world.RuntimeBody(20),neighbour=world.RuntimeBody(21);
 Check(world.Physics().GetLinearVelocity(body).z<-10&&std::abs(world.Physics().GetLinearVelocity(body).y)<.01f,"normal rigid step applies selected gravity once");
 Check(std::abs(world.Physics().GetLinearVelocity(neighbour).z)<.01f,"unselected body has no new sideways acceleration");
 auto before=motor->velocity;Check(world.SetGravitySelection(10,std::nullopt,error)&&Near(motor->velocity,before),"clear preserves world momentum");
 Check(Near(world.SampleEntityGravity(10,motor->position),{0,-9.81f,0}),"clear restores spatial source");
 auto structuralVersion=world.EntityVersion();selection.acceleration={2,3,4};Check(world.SetGravitySelection(10,selection,error),"independent owner setting");
 Check(world.EntityVersion()==structuralVersion,"intent change does not invalidate structural metadata");
 auto version=world.EntityVersion();Check(world.SetGravitySelection(10,selection,error)&&version==world.EntityVersion(),"unchanged intent does not wake/rebuild dependencies");
 selection.acceleration={std::numeric_limits<float>::infinity(),0,0};
 Check(!world.SetGravitySelection(10,selection,error)&&Near(world.SampleEntityGravity(10,motor->position),{2,3,4}),"invalid native write is atomic");
 std::map<std::string,SaveChunk> chunks;Check(WorldPersistence::Capture(world,chunks,error),"modern slot captures gravity intent");if(!error.empty())std::printf("CAPTURE %s\n",error.c_str());
 Scene saved;Check(WorldPersistence::SceneFrom(chunks,saved,error),"snapshot uses ordinary scene fields");
 RuntimeWorld restored;restored.legacyGameplay=false;Check(restored.Build(saved,&host.Resources(),error,nullptr,nullptr,true)&&WorldPersistence::Prepare(restored,error)&&WorldPersistence::Restore(restored,chunks,error),"fresh world restores selection and motion");if(!error.empty())std::printf("RESTORE %s\n",error.c_str());
 Check(Near(restored.SampleEntityGravity(10,restored.RuntimeCharacter(10)->position),{2,3,4}),"restored motor selected gravity");
 restored.Destroy();
 selection.mode=GravitySelection::Mode::Field;selection.source=2;selection.acceleration={0,0,0};world.SetGravitySelection(10,selection,error);
 world.RemoveRegionObject(2);world.RebuildRegionGravity({});
 Check(!world.RuntimeDefinition(2),"selected source retirement removes native field safely");
 Check(!world.GravitySelectionAvailable(10)&&Near(world.SampleEntityGravity(10,motor->position),{0,-9.81f,0}),"missing source exposes unavailable spatial fallback");
 auto fresh=world.AllocateRuntimeEntityId();Check(fresh!=2,"runtime identity cannot alias removed field");
 Check(world.DestroyHierarchy(20,error),"selected rigid owner destroys normally");
 world.UpdateScripts(&host.GetWindow().Input(),1.f/60);
 auto late=world.Scripts()->Capture();const auto lateState=late.empty()?std::string{}:late.front().json;
 Check(lateState.find("\"staleSource\":true")!=std::string::npos,"retained JS source handle rejects safely after retirement");
 Check(lateState.find("\"staleOwner\":true")!=std::string::npos,"retained JS gravity facade rejects safely after owner destruction");

 Scene authored=Fixture();authored.Find(10)->gravitySelection=GravitySelection{};authored.Find(10)->gravitySelection->acceleration={1,2,3};
 std::string text;SaveSceneToString(authored,text);Scene round;
 Check(LoadSceneFromString(text,round,error)&&ScenesEqual(authored,round),"ordinary scene optional-selection round trip");
 std::string named;Check(LegacyToNamed(text,"scene",named,error),"named scene conversion");
 Scene namedScene;Check(LoadSceneFromString(named,namedScene,error)&&ScenesEqual(authored,namedScene),"named selection round trip");
 std::string a,b;ComputeSceneFingerprint(Fixture(),a,error);ComputeSceneFingerprint(authored,b,error);Check(a!=b,"non-default intent contributes conditional schema-5 identity");
 Scene prefab;auto& p=prefab.CreateObject("Motor prefab");p.characterMotor=CharacterMotorSettings{};p.gravitySelection=GravitySelection{};p.gravitySelection->acceleration={0,0,-2};
 Scene instance;SceneObjectId id=0;Check(InstantiatePrefab(instance,prefab,"73737373737373737373737373737302",{},id,error)&&instance.Find(id)->gravitySelection->acceleration.z==-2,"prefab instance keeps independent uniform intent");
 Scene fieldPrefab;auto& f=fieldPrefab.CreateObject("Field");f.gravity=SceneGravityComponent{};auto fieldId=f.id;
 auto& c=fieldPrefab.CreateObject("Consumer");c.parent=fieldId;c.gravitySelection=GravitySelection{GravitySelection::Mode::Field,fieldId,{0,0,0}};
 Check(InstantiatePrefab(instance,fieldPrefab,"73737373737373737373737373737303",{},id,error),"authored prefab field assembly instantiates");
 bool remapped=false;for(const auto& o:instance.Objects())if(o.gravitySelection&&o.gravitySelection->mode==GravitySelection::Mode::Field)remapped=o.gravitySelection->source==id;
 Check(remapped,"prefab selected-source identity remaps");
 EditorDocument editor;editor.GetScene()=fieldPrefab;editor.Select(fieldId);
 Check(!editor.SelectionReferences().empty(),"editor reports selected-source references");
 auto original=editor.GetScene();Check(editor.DuplicateSelection(error),"ordinary editor hierarchy duplication");
 auto duplicated=editor.GetScene();bool mappedCopy=false;
 for(const auto& o:duplicated.Objects())if(o.gravitySelection&&o.id!=c.id)mappedCopy=o.gravitySelection->source==editor.Selected();
 Check(mappedCopy,"duplicate source/consumer references remap together");
 editor.Undo();Check(ScenesEqual(original,editor.GetScene()),"undo restores selected-source hierarchy");
 editor.Redo();Check(ScenesEqual(duplicated,editor.GetScene()),"redo restores selected-source hierarchy");

 {
  Project project;auto directory=fs::absolute(output/"stream-project");fs::remove_all(directory);fs::create_directories(directory);
  Check(Project::CreateNew(directory.string(),"Gravity retention",project,error),"ordinary streamed project");
  Scene regional;auto& source=regional.CreateObject("Regional uniform source");source.gravity=SceneGravityComponent{};source.gravity->kind=SceneGravityKind::Uniform;source.gravity->magnitude=3;
  source.gravity->regionRadius=1;const auto sourceId=source.id;
  auto& consumer=regional.CreateObject("Regional selected consumer");consumer.gravitySelection=GravitySelection{GravitySelection::Mode::Field,sourceId,{0,0,0}};const auto localId=consumer.id;
  Check(SaveSceneToFile(regional,project.ScenesDir()+"/region.judas",error),"normal registered regional scene");
  Scene root;auto& owner=root.CreateObject("Persistent outside-zone owner");owner.transform.position={1000,0,0};const auto ownerId=owner.id;
  RuntimeWorld streamed;Check(streamed.Build(root,&host.Resources(),error),"streamed root world");WorldManifest manifest;
  WorldRegion region;region.id="field";region.scene="Scenes/region.judas";manifest.regions[region.id]=region;
  WorldStreaming stream(streamed,host.Resources(),project,manifest);
  auto pump=[&](const auto& done){for(int i=0;i<2000;++i){stream.Advance(false);if(done())return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;};
  auto token=stream.Request("field",false,error);Check(pump([&]{auto state=stream.Status(token);return state&&state->state=="active";}),"field publishes through real asynchronous residency");
  auto firstSource=stream.Resolve("field",sourceId),firstConsumer=stream.Resolve("field",localId);
  Check(streamed.RuntimeDefinition(firstConsumer)&&streamed.RuntimeDefinition(firstConsumer)->gravitySelection->source==firstSource,"regional source identity remaps at publication");
  Check(Near(streamed.SampleEntityGravity(firstConsumer,{1000,0,0}),{0,-3,0}),"regional field remains selected outside volume");
  GravitySelection chosen{GravitySelection::Mode::Field,firstSource,{0,0,0}};Check(streamed.SetGravitySelection(ownerId,chosen,error),"persistent entity selects streamed field");
  stream.Release(token);stream.Advance(false);auto rows=stream.Regions();
  Check(!rows.empty()&&rows.front().state=="active"&&!rows.front().pins.empty(),"external selection pins field until intent clears");
  streamed.SetGravitySelection(ownerId,std::nullopt,error);
  Check(pump([&]{return stream.Regions().front().state=="unloaded";}),"clearing selection releases real region retirement");
  token=stream.Request("field",false,error);Check(pump([&]{auto state=stream.Status(token);return state&&state->state=="active";}),"retained region returns normally");
  auto returnedSource=stream.Resolve("field",sourceId),returnedConsumer=stream.Resolve("field",localId);
  Check(returnedSource!=firstSource&&streamed.RuntimeDefinition(returnedConsumer)&&streamed.RuntimeDefinition(returnedConsumer)->gravitySelection->source==returnedSource,"retained selected-source remaps to fresh identity");
  Check(Near(streamed.SampleEntityGravity(returnedConsumer,{1000,0,0}),{0,-3,0}),"retained selection resolves fresh field without cached pointer");
 }
 for(const auto& mode:{std::string("spatial"),std::string("uniform"),std::string("field")}){
  GravitySelection choice;choice.acceleration={1,2,3};if(mode=="field"){choice.mode=GravitySelection::Mode::Field;choice.source=3;choice.acceleration={0,0,0};}
  world.SetGravitySelection(11,mode=="spatial"?std::optional<GravitySelection>{}:choice,error);
  const auto start=std::chrono::steady_clock::now();volatile float sum=0;
  for(int i=0;i<20000;++i)sum=sum+world.SampleEntityGravity(11,{float(i%20),1,0}).x;
  auto nanoseconds=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/20000;
  std::printf("PERF %s ns_per_sample %.3f checksum %.3f\n",mode.c_str(),nanoseconds,float(sum));
 }
 game.End();world.Destroy();Check(!world.RuntimeCharacter(10),"Stop clears motor and selection world");host.Shutdown();
 std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}
