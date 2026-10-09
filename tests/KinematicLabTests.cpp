#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "ScriptSystem.h"
#include "WorldPersistence.h"
#include "WorldState.h"
#include "Prefab.h"
#include "SaveStorage.h"
#include "WorldStreaming.h"
#include "SaveArchive.h"
#include "../third_party/nlohmann/json.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

namespace {
namespace fs=std::filesystem;
using Json=nlohmann::json;
constexpr float dt=1.f/60;
int checks=0,failures=0;
void Check(bool pass,const std::string& label){++checks;failures+=!pass;std::printf("%s %s\n",pass?"PASS":"FAIL",label.c_str());}
bool Near(glm::vec3 a,glm::vec3 b,float tolerance=1e-4f){return glm::length(a-b)<=tolerance;}
const char* script=R"JS(import {world} from 'judas';
export const properties={mode:{type:'string',default:'target'}};
export default class {
 constructor({entity,properties}){this.entity=entity;this.props=properties;this.state={steps:0,pumps:0,enter:0,stay:0,exit:0,rejected:0,presentationRejected:0};}
 fixedUpdate(dt){
  this.state.steps++;
  if(this.state.steps===1){
   this.state.motionType=this.entity.motionType;
   this.entity.setMotionType('kinematic');
   if(this.props.mode==='target'){
    const q={w:1,x:0,y:0,z:0};
    this.entity.moveKinematic({position:{x:1,y:0,z:0},rotation:q},4*dt);
    this.entity.moveKinematic({position:{x:2,y:0,z:0},rotation:q},4*dt);
    for(const bad of [{position:{x:NaN,y:0,z:0},rotation:q},{position:{x:8,y:0,z:0},rotation:{w:0,x:0,y:0,z:0}}]){
     try{this.entity.moveKinematic(bad);}catch(error){this.state.rejected++;}
    }
   }else this.entity.setKinematicVelocity({x:.5,y:0,z:0},{x:0,y:.2,z:0});
   this.state.before=this.entity.transform.position;
   const detached=this.entity.kinematicMotion;detached.linearVelocity.x=999;detached.target.position.x=999;
   this.state.detached=this.entity.kinematicMotion.linearVelocity.x!==999&&this.entity.kinematicMotion.target.position.x!==999;
  }
  this.state.actual=this.entity.transform;this.state.velocity=this.entity.velocity;this.state.point=this.entity.pointVelocity(this.entity.transform.position);
 }
 presentationUpdate(){this.state.pumps++;this.state.presented=this.entity.presentedTransform;
  // Rendering cannot supply new authoritative history through the public API.
  try{this.entity.stopKinematic();}catch(error){this.state.presentationRejected++;}
 }
 onCollisionEnter(e){this.state.enter++;this.state.other=e.other?.id;}
 onCollisionStay(){this.state.stay++;}
 onCollisionExit(){this.state.exit++;}
})JS";
Scene Fixture(const std::string& asset){
    Scene scene;
    SceneObject pusher;pusher.id=10;pusher.name="Public VM target";pusher.transform.position={-2,0,0};pusher.body=SceneBodyComponent{};
    pusher.body->motion=SceneBodyMotion::Kinematic;pusher.body->halfExtents={.25f,.25f,.25f};pusher.body->restitution=0;pusher.scripts.push_back({1,asset,true,"{\"mode\":\"target\"}"});scene.InsertObject(pusher);
    SceneObject crate;crate.id=11;crate.name="Ordinary dynamic neighbour";crate.body=SceneBodyComponent{};crate.body->motion=SceneBodyMotion::Dynamic;crate.body->shape=SceneShape::Sphere;crate.body->radius=.15f;crate.body->restitution=0;scene.InsertObject(crate);
    SceneObject support;support.id=20;support.name="Independent durable velocity";support.transform.position={5,0,0};support.body=SceneBodyComponent{};support.body->motion=SceneBodyMotion::Kinematic;
    support.body->halfExtents={.25f,.25f,.25f};support.scripts.push_back({1,asset,true,"{\"mode\":\"velocity\"}"});scene.InsertObject(support);return scene;
}
struct Assets {
    AssetDatabase database;std::string scriptId,error;ResourceManager resources;
    bool Track(const fs::path& path,AssetRecord& record,std::string& outError){
        // Reruns and the separate save/load processes retain asset identities.
        const auto relative=path.lexically_relative(fs::path(database.AssetsDir()).parent_path()).generic_string();
        if(const auto* existing=database.FindByRelativePath(relative)){record=*existing;return true;}
        return database.Track(path.string(),record,outError);
    }
    explicit Assets(const fs::path& root):resources(nullptr,&database){
        fs::create_directories(root/"Assets");std::ofstream(root/"Assets/control.js")<<script;
        database.Scan(root.string(),(root/"Assets").string());AssetRecord record;
        if(!Track(root/"Assets/control.js",record,error))throw std::runtime_error(error);
        scriptId=record.id;
    }
};
void Tick(RuntimeWorld& world,int pumps=0){
    world.FixedScripts(nullptr,dt);world.Physics().Step(dt);world.UpdateCharacters(dt);
    SyncDynamicBodiesFromPhysics(world.DynamicBodies(),world.Physics());world.DispatchPhysicsEvents(nullptr,dt);
    for(int i=0;i<pumps;++i)world.PresentationScripts(nullptr,dt/pumps,float(i+1)/pumps);
}
Json State(RuntimeWorld& world,EntityId id){for(const auto& record:world.Scripts()->Capture())if(record.entity==id)return Json::parse(record.json);return Json();}
bool Restore(RuntimeWorld& world,ResourceManager& resources,const GameSnapshot& snapshot,std::string& error){
    Scene scene;return WorldPersistence::SceneFrom(snapshot.participants,scene,error)&&world.Build(scene,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(world,error)&&WorldPersistence::Restore(world,snapshot.participants,error);
}
bool PumpStreaming(WorldStreaming& stream,const std::function<bool()>& done){
    for(int i=0;i<4000;++i){stream.Advance(false);if(done())return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;
}
// Rewrite just one optional command field in a complete v4 archive. Its typed
// region/schema fields and trailing ownership tables remain intact, so refusal
// must identify incomplete intent rather than truncation or checksum damage.
std::string MissingRetainedKinematic(const std::string& bytes){
    SaveArchive input(bytes),output;std::string baseline;uint32_t regions=0;input(baseline,regions);output(baseline,regions);bool removed=false;
    for(uint32_t r=0;r<regions;++r){
        std::string id;bool active=false;std::map<SceneObjectId,EntityId> mapping;std::vector<SceneObjectId> members;std::set<std::string> pins;uint64_t regionBytes=0;uint32_t records=0;
        input(id,active,mapping,members,pins,regionBytes,records);output(id,active,mapping,members,pins,regionBytes,records);
        for(uint32_t i=0;i<records;++i){
            SceneObjectId local=0;bool destroyed=false;std::string text,deformable,animation,kinematic;EntityPhysicalState state;uint32_t scripts=0;
            input(local,destroyed,text,state.position,state.rotation,state.linearVelocity,state.angularVelocity,deformable,animation,kinematic,scripts);
            if(!removed&&!destroyed&&!kinematic.empty()){kinematic.clear();removed=true;}
            output(local,destroyed,text,state.position,state.rotation,state.linearVelocity,state.angularVelocity,deformable,animation,kinematic,scripts);
            for(uint32_t j=0;j<scripts;++j){ScriptStateRecord script;input(script.entity,script.slot,script.json);output(script.entity,script.slot,script.json);}
        }
    }
    if(!removed)throw std::runtime_error("fixture has no retained kinematic command");
    output.bytes+=bytes.substr(input.position);return output.bytes;
}
int StreamingRoundTrip(){
    const auto root=fs::absolute("build/m71-kinematic-stream-tests");fs::remove_all(root);std::string error;Project project;
    Check(Project::CreateNew(root.string(),"kinematic_stream",project,error),"ordinary isolated streaming project: "+error);
    project.Settings().legacyGameplay=false;project.Settings().startupScene="Scenes/bootstrap.judas";Check(project.Save(error),"stream project settings persist");
    Scene bootstrap,region;
    for(EntityId id:{10,20,30,40}){SceneObject object;object.id=id;object.name="Ordinary prescribed region actor";object.transform.position={float(id-10),0,0};object.body=SceneBodyComponent{};object.body->motion=SceneBodyMotion::Kinematic;object.body->halfExtents={.25f,.25f,.25f};if(id==40)object.body->initialLinearVelocity={2,0,0};region.InsertObject(object);}
    Check(SaveSceneToFile(bootstrap,(root/"Scenes/bootstrap.judas").string(),error)&&SaveSceneToFile(region,(root/"Scenes/region.judas").string(),error),"ordinary root and kinematic region sources persist");
    AssetDatabase database;database.Scan(project.RootDir(),project.AssetsDir());JobSystem jobs(2);ResourceManager resources(nullptr,&database,&jobs);
    WorldManifest manifest;manifest.unitsPerFrame=1;manifest.installMilliseconds=10;WorldRegion declaration;declaration.id="motion";declaration.scene="Scenes/region.judas";manifest.regions.emplace(declaration.id,declaration);
    Check(ValidateWorldManifest(manifest,project,error),"normal region manifest validates");
    RuntimeWorld world;Check(world.Build(bootstrap,&resources,error),"stream bootstrap builds");WorldStreaming stream(world,resources,project,manifest);
    auto demand=stream.Request("motion",false,error);const bool loaded=PumpStreaming(stream,[&]{auto status=stream.Status(demand);return status&&status->state=="active";});Check(loaded,"actual staged coordinator publishes prescribed region");if(!loaded)return 1;
    const auto target=stream.Resolve("motion",10),velocity=stream.Resolve("motion",20),traveller=stream.Resolve("motion",30),stopped=stream.Resolve("motion",40);
    const auto targetBody=world.RuntimeBody(target),velocityBody=world.RuntimeBody(velocity),travellerBody=world.RuntimeBody(traveller),stoppedBody=world.RuntimeBody(stopped);
    Check(world.Physics().MoveKinematic(targetBody,{{4,0,0},{1,0,0,0}},4*dt)&&world.Physics().SetKinematicVelocity(velocityBody,{1,0,0},{0,.25f,0})&&world.Physics().SetKinematicVelocity(travellerBody,{0,0,2},{0,0,0})&&world.Physics().StopKinematic(stoppedBody),"independent target, velocity, traveller and stopped commands accepted");
    world.Physics().Step(dt);world.Physics().Step(dt);SyncDynamicBodiesFromPhysics(world.DynamicBodies(),world.Physics());
    const auto identity=stream.PersistentKey(traveller);Check(stream.Adopt(traveller,"root",error)&&stream.PersistentKey(traveller)==identity&&world.RuntimeBody(traveller).id==travellerBody.id,"adoption preserves qualified identity and native trajectory owner");
    std::map<SceneObjectId,std::string> commands;for(auto pair:{std::pair<SceneObjectId,EntityId>{10,target},{20,velocity},{40,stopped}})Check(WorldPersistence::CaptureKinematic(world,pair.second,commands[pair.first],error),"capture expected retained command "+std::to_string(pair.first));
    std::string travellerCommand;Check(WorldPersistence::CaptureKinematic(world,traveller,travellerCommand,error),"capture expected adopted root command");
    stream.Release(demand);Check(PumpStreaming(stream,[&]{return stream.Regions().front().state=="unloaded";}),"actual coordinator snapshots and retires region");
    Check(world.RuntimeDefinition(traveller)&&stream.Owner(traveller)=="root"&&!world.RuntimeDefinition(target)&&stream.ArchiveVersion()==4,"suspended kinematic intent selects v4 while adopted traveller stays live");
    SaveArchive archive;stream.Persist(archive,stream.ArchiveVersion());GameSnapshot snapshot;snapshot.project="M71-stream";snapshot.scene="Scenes/bootstrap.judas";snapshot.content=world.BaselineFingerprint();
    Check(WorldPersistence::Capture(world,snapshot.participants,error),"root participant captures adopted prescribed body");snapshot.participants["streaming"]={4,archive.bytes};
    const auto file=root/"stream.save";std::ofstream(file,std::ios::binary)<<EncodeGameSnapshot(snapshot);std::ifstream input(file,std::ios::binary);std::ostringstream encoded;encoded<<input.rdbuf();GameSnapshot decoded;
    Check(DecodeGameSnapshot(encoded.str(),decoded,error),"complete stream/root snapshot survives durable encoding");
    RuntimeWorld restored;const bool rebuilt=Restore(restored,resources,decoded,error);Check(rebuilt,"cold native root reconstruction restores adopted intent: "+error);if(!rebuilt)return 1;
    WorldStreaming resumed(restored,resources,project,manifest);SaveArchive reading(decoded.participants.at("streaming").data);resumed.Persist(reading,4);reading.Finish();
    Check(resumed.Owner(traveller)=="root"&&resumed.ResolvePersistentKey(identity)==traveller&&resumed.Regions().front().state=="unloaded","cold coordinator restores root adoption and suspended residency");
    std::string restoredTraveller;Check(WorldPersistence::CaptureKinematic(restored,traveller,restoredTraveller,error)&&restoredTraveller==travellerCommand,"adopted root command restores exactly");
    demand=resumed.Request("motion",false,error);const bool revisited=PumpStreaming(resumed,[&]{auto status=resumed.Status(demand);return status&&status->state=="active";});Check(revisited,"v4 retained region reconstructs through actual coordinator revisit");if(!revisited)return 1;
    Check(!resumed.Resolve("motion",30)&&resumed.ResolvePersistentKey(identity)==traveller,"adoption tombstone prevents duplicate traveller source on revisit");
    for(auto local:{10,20,40}){std::string command;const auto id=resumed.Resolve("motion",local);Check(id&&WorldPersistence::CaptureKinematic(restored,id,command,error)&&command==commands.at(local),"v4 publication restores exact command and resolved velocities "+std::to_string(local));Check(restored.Physics().GetBodyMotionSegments(restored.RuntimeBody(id)).empty(),"v4 revisit reconstructs no preceding trajectory history "+std::to_string(local));}
    const auto resumedTarget=restored.RuntimeBody(resumed.Resolve("motion",10)),resumedVelocity=restored.RuntimeBody(resumed.Resolve("motion",20)),resumedStopped=restored.RuntimeBody(resumed.Resolve("motion",40));
    const auto velocityBefore=restored.Physics().GetTransform(resumedVelocity).position,travellerBefore=restored.Physics().GetTransform(restored.RuntimeBody(traveller)).position,stoppedBefore=restored.Physics().GetTransform(resumedStopped).position;
    restored.Physics().Step(dt);Check(Near(restored.Physics().GetTransform(resumedTarget).position,{3,0,0}),"first resumed target advances only its remaining fixed interval");
    Check(Near(restored.Physics().GetTransform(resumedVelocity).position-velocityBefore,{dt,0,0}),"first resumed regional velocity continues once");
    Check(Near(restored.Physics().GetTransform(restored.RuntimeBody(traveller)).position-travellerBefore,{0,0,2*dt}),"first resumed adopted root velocity continues once");
    Check(Near(restored.Physics().GetTransform(resumedStopped).position,stoppedBefore),"retained stopped intent overrides authored initial velocity");
    restored.Physics().Step(dt);restored.Physics().Step(dt);Check(Near(restored.Physics().GetTransform(resumedTarget).position,{4,0,0}),"restored target finishes then holds");
    RuntimeWorld invalid;invalid.Build(bootstrap,&resources,error);WorldStreaming rejected(invalid,resources,project,manifest);bool refused=false;std::string rejection;
    try{SaveArchive incomplete(MissingRetainedKinematic(archive.bytes));rejected.Persist(incomplete,4);incomplete.Finish();}catch(const std::exception& e){rejection=e.what();refused=rejection.find("kinematic")!=std::string::npos;}
    std::printf("INCOMPLETE_REJECTION %s\n",rejection.c_str());Check(refused,"incomplete v4 kinematic intent refuses before publication");
    bool downgradeRefused=false;try{SaveArchive legacy;stream.Persist(legacy,3);}catch(const std::exception& e){downgradeRefused=std::string(e.what()).find("kinematic")!=std::string::npos;}
    Check(downgradeRefused,"retained kinematic intent refuses lossy older archive writes");
    std::printf("SUMMARY streaming %d checks %d failures\n",checks,failures);return failures?1:0;
}
int Process(const std::string& operation,const fs::path& path){
    auto assetRoot=fs::absolute(path.parent_path()/(path.filename().string()+".fixtures"));Assets assets(assetRoot);std::string error;
    if(operation=="--write"){
        const auto fixture=Fixture(assets.scriptId);RuntimeWorld world;Check(world.Build(fixture,&assets.resources,error),"fresh process save fixture constructs");Tick(world);Tick(world);
        GameSnapshot snapshot;snapshot.project="M71";snapshot.scene="main";snapshot.displayName="M71 prescribed state";
        Check(ComputeSceneFingerprint(fixture,snapshot.content,error),"fresh process metadata uses the canonical authored scene fingerprint");
        Check(WorldPersistence::Capture(world,snapshot.participants,error),"capture active target and durable velocity: "+error);
        fs::create_directories(path.parent_path());std::ofstream file(path,std::ios::binary);file<<EncodeGameSnapshot(snapshot);file.close();Check(bool(file),"write modern snapshot for separate process");
    }else{
        std::ifstream file(path,std::ios::binary);std::ostringstream bytes;bytes<<file.rdbuf();GameSnapshot snapshot;
        Check(DecodeGameSnapshot(bytes.str(),snapshot,error),"fresh process decodes complete modern snapshot: "+error);RuntimeWorld world;
        Check(Restore(world,assets.resources,snapshot,error),"fresh process restores command state and current physical state: "+error);
        if(!world.RuntimeBody(10).IsValid())return 1;
        auto pusher=world.RuntimeBody(10),support=world.RuntimeBody(20);
        Check(Near(world.Physics().GetTransform(pusher).position,{0,0,0}),"restoration does not replay preceding two metres of target displacement");
        Check(world.Physics().GetBodyMotionSegments(pusher).empty(),"disposable previous motion history reconstructed empty");
        const auto before=world.Physics().GetTransform(support).position;Tick(world);
        Check(Near(world.Physics().GetTransform(pusher).position,{1,0,0}),"first resumed target step advances only remaining interval");
        Check(Near(world.Physics().GetTransform(support).position-before,{.5f*dt,0,0}),"first resumed velocity step continues once");
        Check(world.Scripts()->Diagnostics().empty()&&State(world,10)["steps"]==3,"resumed real VM sees durable state without start replay");
        Tick(world);Tick(world);Check(Near(world.Physics().GetTransform(pusher).position,{2,0,0}),"restored completed target holds after its interval");
    }
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
}
int main(int argc,char** argv){
    if(argc==2&&std::string(argv[1])=="--stream")return StreamingRoundTrip();
    if(argc==3&&(std::string(argv[1])=="--write"||std::string(argv[1])=="--read"))return Process(argv[1],fs::absolute(argv[2]));
    auto root=fs::absolute("build/m71-lab-tests");Assets assets(root);std::string error;
    {
        const auto example=root/"Assets/kinematic-example.js";fs::copy_file("docs/judasjs/examples/kinematic.js",example,fs::copy_options::overwrite_existing);AssetRecord record;
        Check(assets.Track(example,record,error),"copyable kinematic cookbook example is a normal tracked script");Scene cookbook;auto& actor=cookbook.CreateObject("Cookbook prescribed body");actor.body=SceneBodyComponent{};actor.scripts.push_back({1,record.id,true,"{}"});const auto owner=actor.id;
        RuntimeWorld execution;Check(execution.Build(cookbook,&assets.resources,error),"copyable cookbook uses ordinary scene/runtime construction");for(int i=0;i<50;++i)Tick(execution);
        const auto state=State(execution,owner);std::printf("COOKBOOK kinematic %s\n",state.dump().c_str());
        Check(execution.Scripts()->Diagnostics().empty()&&state["velocityQueued"]==true&&state["targetQueued"]==true&&state["pointIncludesRotation"]==true&&state["invalidRejected"]==true&&state["completed"]==true,"executed public cookbook covers velocity, target, stop, point readback and validation");
    }
    auto scene=Fixture(assets.scriptId);std::string serialized;SaveSceneToString(scene,serialized);Scene parsed;
    Check(LoadSceneFromString(serialized,parsed,error)&&ScenesEqual(scene,parsed),"kinematic authored mode round-trips through normal scene serializer");
    std::string fingerprint,changed;ComputeSceneFingerprint(scene,fingerprint,error);auto altered=scene;altered.Find(10)->body->motion=SceneBodyMotion::Static;ComputeSceneFingerprint(altered,changed,error);
    Check(changed!=fingerprint,"kinematic authoring changes baseline fingerprint deliberately");
    RuntimeWorld world;Check(world.Build(scene,&assets.resources,error),"ordinary scene creates independent prescribed bodies");auto id=world.RuntimeBody(10).id;
    Tick(world,2);auto state=State(world,10);
    Check(world.Scripts()->Diagnostics().empty()&&state["motionType"]=="kinematic"&&state["detached"]==true&&state["rejected"]==2,"actual VM validates complete requests and detached readback");
    Check(state["before"]["x"]==-2&&Near(world.Physics().GetTransform(world.RuntimeBody(10)).position,{-1,0,0}),"public JS queues target before normal fixed-step advancement");
    Check(state["presentationRejected"]==2,"presentation attempts cannot publish kinematic commands");
    for(int i=0;i<4;++i)Tick(world,2);
    Check(Near(world.Physics().GetTransform(world.RuntimeBody(10)).position,{2,0,0})&&world.Physics().GetLinearVelocity(world.RuntimeBody(11)).x>1,"public VM target drives ordinary contact impulse into dynamic neighbour");
    Check(State(world,10)["enter"]==1,"real callback delivery emits one enter for the body pair");
    Check(world.RuntimeBody(10).id==id,"script authority declaration preserves existing body handle");
    RuntimeWorld sparse,dense;sparse.Build(scene,&assets.resources,error);dense.Build(scene,&assets.resources,error);
    for(int i=0;i<12;++i){Tick(sparse,1);Tick(dense,7);}
    Check(Near(sparse.Physics().GetTransform(sparse.RuntimeBody(10)).position,dense.Physics().GetTransform(dense.RuntimeBody(10)).position,0)&&Near(sparse.Physics().GetTransform(sparse.RuntimeBody(11)).position,dense.Physics().GetTransform(dense.RuntimeBody(11)).position,0),"different presentation rates retain identical fixed-step physics");
    Check(State(dense,20)["pumps"]==84&&State(sparse,20)["pumps"]==12,"rate comparison actually exercised distinct presentation cadence");
    Scene prefabScene;auto& prefabObject=prefabScene.CreateObject("Independent ordinary prescribed prefab");prefabObject.body=SceneBodyComponent{};prefabObject.body->motion=SceneBodyMotion::Kinematic;prefabObject.scripts.push_back({1,assets.scriptId,true,"{\"mode\":\"velocity\"}"});
    auto prefabPath=root/"Assets/platform.judasprefab";SaveSceneToFile(prefabScene,prefabPath.string(),error);AssetRecord prefab;assets.Track(prefabPath,prefab,error);
    SceneTransform a,b;a.position={10,0,0};b.position={20,0,0};auto first=world.SpawnPrefab(prefab.id,a,error),second=world.SpawnPrefab(prefab.id,b,error);Tick(world);
    Check(first&&second&&first!=second&&world.Physics().GetMotionType(world.RuntimeBody(first))==BodyMotionType::Kinematic,"ordinary prefab spawn contains independent prescribed state");
    world.Physics().StopKinematic(world.RuntimeBody(first));auto firstBefore=world.Physics().GetTransform(world.RuntimeBody(first)).position,secondBefore=world.Physics().GetTransform(world.RuntimeBody(second)).position;Tick(world);
    Check(Near(world.Physics().GetTransform(world.RuntimeBody(first)).position,firstBefore)&&Near(world.Physics().GetTransform(world.RuntimeBody(second)).position-secondBefore,{.5f*dt,0,0}),"stopping one prefab instance leaves another moving independently");
    auto retired=world.RuntimeBody(first);world.Physics().MoveKinematic(retired,{{100,0,0},{1,0,0,0}});
    Check(world.DestroyHierarchy(first,error)&&!world.RuntimeBody(first).IsValid(),"ordinary removal retires native body and pending target");Tick(world);KinematicMotionState motion;
    Check(!world.Physics().GetKinematicMotion(retired,motion),"removed prefab native handle cannot read stale command");
    auto targetScene=scene;RuntimeWorld saved,restored;saved.Build(targetScene,&assets.resources,error);Tick(saved);Tick(saved);GameSnapshot snapshot;
    Check(WorldPersistence::Capture(saved,snapshot.participants,error)&&Restore(restored,assets.resources,snapshot,error),"modern participants preserve target/velocity control: "+error);
    Check(Near(restored.Physics().GetTransform(restored.RuntimeBody(10)).position,saved.Physics().GetTransform(saved.RuntimeBody(10)).position),"save restores current pose without command replay");
    for(int i=0;i<4;++i){Tick(saved);Tick(restored);}
    Check(Near(saved.Physics().GetTransform(saved.RuntimeBody(10)).position,restored.Physics().GetTransform(restored.RuntimeBody(10)).position)&&Near(saved.Physics().GetTransform(saved.RuntimeBody(20)).position,restored.Physics().GetTransform(restored.RuntimeBody(20)).position),"resumed target and durable velocity continue equivalently");
    auto missing=snapshot;missing.participants.erase("kinematic-motion");RuntimeWorld rejected;
    Check(!Restore(rejected,assets.resources,missing,error),"missing prescribed participant refuses incomplete restore");
    Check(!CanCaptureLegacyWorldState(world,error)&&error.find("save")!=std::string::npos,"legacy delta explicitly rejects prescribed state with modern save guidance");
    Scene ordinary;auto& object=ordinary.CreateObject("Static legacy baseline");object.body=SceneBodyComponent{};auto staticId=object.id;RuntimeWorld compatibility;compatibility.Build(ordinary,nullptr,error);
    Check(CanCaptureLegacyWorldState(compatibility,error),"unchanged ordinary static baseline retains legacy delta support");
    auto handle=compatibility.RuntimeBody(staticId);Check(compatibility.SetRuntimeMotionType(staticId,SceneBodyMotion::Dynamic,false,error)&&compatibility.RuntimeBody(staticId).id==handle.id,"static runtime promotion reuses native handle");
    Check(!CanCaptureLegacyWorldState(compatibility,error),"legacy delta rejects a changed authority declaration");
    Check(compatibility.SetRuntimeMotionType(staticId,SceneBodyMotion::Static,false,error)&&CanCaptureLegacyWorldState(compatibility,error),"restoring baseline authority restores legacy eligibility");
    {
        Scene empty;RuntimeWorld regional;regional.Build(empty,nullptr,error);SceneObject member;member.id=800;member.name="Ordinary staged prescribed body";member.body=SceneBodyComponent{};
        member.body->motion=SceneBodyMotion::Kinematic;member.body->initialLinearVelocity={1,0,0};
        Check(regional.StageRegionObject(member,member,error)&&!regional.RuntimeDefinition(member.id),"regional initial prescribed body remains private before publication");
        regional.Physics().Step(dt);regional.PublishRegion({member.id});auto h=regional.RuntimeBody(member.id);regional.Physics().Step(dt);
        Check(Near(regional.Physics().GetTransform(h).position,{dt,0,0}),"fresh regional prescribed initial velocity begins at publication once");
        regional.RemoveRegionObject(member.id);Check(!regional.RuntimeBody(member.id).IsValid(),"regional removal retires motion ownership");
        SceneObject retained=member;retained.id=801;Scene baseline;baseline.InsertObject(retained);RuntimeWorld donor;donor.Build(baseline,nullptr,error);
        donor.Physics().StopKinematic(donor.RuntimeBody(retained.id));std::string stopped;WorldPersistence::CaptureKinematic(donor,retained.id,stopped,error);
        regional.StageRegionObject(retained,retained,error);Check(WorldPersistence::RestoreKinematic(regional,retained.id,stopped,error),"retained stopped intent replaces staged authored velocity");regional.PublishRegion({retained.id});regional.Physics().Step(dt);
        Check(Near(regional.Physics().GetTransform(regional.RuntimeBody(retained.id)).position,{0,0,0}),"retained stopped regional body remains stopped after publication");regional.RemoveRegionObject(retained.id);
        donor.Physics().MoveKinematic(donor.RuntimeBody(retained.id),{{1,0,0},{1,0,0,0}},4*dt);std::string target;WorldPersistence::CaptureKinematic(donor,retained.id,target,error);
        retained.id=802;regional.StageRegionObject(retained,retained,error);Check(WorldPersistence::RestoreKinematic(regional,retained.id,target,error),"retained target replaces staged authored velocity");regional.PublishRegion({retained.id});regional.Physics().Step(dt);
        Check(Near(regional.Physics().GetTransform(regional.RuntimeBody(retained.id)).position,{.25f,0,0}),"retained target regional publication advances its own remaining interval");
        member.id=803;member.body->enabled=false;regional.StageRegionObject(member,member,error);regional.PublishRegion({member.id});regional.Physics().Step(dt);
        Check(!regional.Physics().IsBodyEnabled(regional.RuntimeBody(member.id))&&Near(regional.Physics().GetTransform(regional.RuntimeBody(member.id)).position,{0,0,0}),"disabled regional authoring does not start prescribed initial velocity");
    }
    world.RestoreAuthoredState();Check(world.Physics().GetMotionType(world.RuntimeBody(10))==BodyMotionType::Kinematic&&Near(world.Physics().GetLinearVelocity(world.RuntimeBody(10)),{0,0,0}),"Play/Stop authored reset retires previous durable commands");
    std::printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
