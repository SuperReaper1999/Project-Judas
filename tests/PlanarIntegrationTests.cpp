#include "ProjectStarter.h"
#include "editor/EditorPicking.h"
#include "editor/EditorCreationMenu.h"
#include "editor/EditorDocument.h"
#include "WorldStreaming.h"
#include <thread>
#include <chrono>
#include "GameSession.h"
#include "Simulation.h"
#include "Project.h"
#include "AssetDatabase.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "SceneSerialization.h"
#include "WorldPersistence.h"
#include "WorldState.h"
#include "SaveStorage.h"
#include "Prefab.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
unsigned checks=0;
void Check(bool ok,const std::string& label){++checks;if(!ok)throw std::runtime_error(label);std::cout<<"PASS "<<label<<'\n';}
void Tick(RuntimeWorld& world,InputSystem& input,int count) {
    GameSession session;std::string error;
    if(!session.Begin(world,error))throw std::runtime_error(error);
    for(int i=0;i<count;++i){input.BeginFrame();input.BeginFixedStep();
        StepAuthoritativeWorld(session,input,1.f/60);
        world.UpdateScripts(&input,1.f/60);world.PresentationScripts(&input,1.f/60,.5f);
    }
    Check(!world.Scripts()||world.Scripts()->Diagnostics().empty(),"fixed-step execution has no VM diagnostics");
}
EntityId Player(const Scene& scene){for(const auto& o:scene.Objects())if(!o.scripts.empty())return o.id;throw std::runtime_error("missing player");}
void StreamingChecks(const fs::path& root,Project& project){
    std::string error;Scene region;
    auto& floor=region.CreateObject("Native 2D region floor");floor.body=SceneBodyComponent{};floor.body->shape=SceneShape::Planar;floor.body->planar=PlanarBox({5,.25});floor.transform.position={0,-.25f,0};
    auto& rotor=region.CreateObject("Native suspended rotor");rotor.body=SceneBodyComponent{};rotor.body->shape=SceneShape::Planar;rotor.body->planar=PlanarBox({.7,.2});rotor.body->motion=SceneBodyMotion::Dynamic;rotor.transform.position={2,1,0};
    auto& pin=region.CreateObject("Planar region pin");pin.joint=SceneJointComponent{};pin.joint->bodyA=2;pin.joint->settings.type=JointType::PlanarPin;pin.joint->settings.anchorB={2,1,0};
    Check(SaveSceneToFile(region,(root/"Scenes/region.judas").string(),error),"author real planar streamed assembly");
    AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());JobSystem jobs(2);ResourceManager resources(nullptr,&assets,&jobs);
    RuntimeWorld world;world.legacyGameplay=false;Scene empty;Check(world.Build(empty,&resources,error),"empty persistent root for ordinary region coordinator");
    WorldManifest manifest;WorldRegion entry;entry.id="planar";entry.scene="Scenes/region.judas";manifest.regions.emplace(entry.id,entry);
    Check(ValidateWorldManifest(manifest,project,error),"ordinary manifest accepts planar source: "+error);
    WorldStreaming stream(world,resources,project,manifest);
    const auto pump=[&](const std::function<bool()>& done){for(int i=0;i<5000;++i){resources.Pump();stream.Advance(false);if(done())return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return false;};
    auto request=stream.Request("planar",false,error);
    Check(request&&pump([&]{return stream.Status(request)&&stream.Status(request)->state=="active";}),"stream publishes native planar region");
    auto id=stream.Resolve("planar",2);const auto old=world.RuntimeBody(id);
    Check(old.IsValid()&&world.RuntimeJoint(stream.Resolve("planar",3)).IsValid(),"real region body and two-row pin instantiate");
    world.Physics().SetAngularVelocity(old,{0,0,.7f});InputSystem input;Tick(world,input,12);
    const auto before=world.Physics().GetTransform(old);const auto omega=world.Physics().GetAngularVelocity(old);
    stream.Release(request);Check(pump([&]{return stream.Regions()[0].state=="unloaded";})&&world.Physics().AliveBodyCount()==0,"suspension actually removes planar bodies and joint from simulation");
    request=stream.Request("planar",false,error);Check(pump([&]{return stream.Status(request)&&stream.Status(request)->state=="active";}),"region revisit reconstructs planar assembly");
    id=stream.Resolve("planar",2);const auto fresh=world.RuntimeBody(id);const auto after=world.Physics().GetTransform(fresh);
    Check(fresh.id!=old.id&&after.position==before.position&&after.rotation==before.rotation&&world.Physics().GetAngularVelocity(fresh)==omega,"streamed planar endpoint and real spin survive fresh native identity");
    Check(world.Physics().GetPlanarNormal(fresh)==glm::vec3(0,0,1)&&world.RuntimeJoint(stream.Resolve("planar",3)).IsValid(),"streamed native mobility and actual pin restore together");
    Tick(world,input,12);const auto continued=world.Physics().GetTransform(fresh);
    Check(continued.rotation!=after.rotation&&glm::length(continued.position-after.position)<.001f&&std::abs(world.Physics().GetAngularVelocity(fresh).z-.7f)<.001f,"restored region pin really continues rotating around its anchor");

}

int main(int argc,char** argv){try{
    if(argc!=2&&argc!=3)return 2;
    auto root=fs::absolute(argv[1]);std::string error;
    if(argc==3){
        Project project;Check(project.Load((root/"Planar_Playground.judasproj").string(),error),"cold process opens exact saved project");
        AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());ResourceManager resources(nullptr,&assets);
        SaveStorage storage((root/"ColdSaves").string());GameSnapshot snapshot;bool recovered=false;
        Check(storage.Read("planar",snapshot,recovered,error),"cold process reads checksum-protected modern slot: "+error);
        Scene scene;Check(WorldPersistence::SceneFrom(snapshot.participants,scene,error),"cold process reconstructs planar authored geometry: "+error);
        RuntimeWorld world;world.legacyGameplay=false;
        Check(world.Build(scene,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(world,error)&&WorldPersistence::Restore(world,snapshot.participants,error),"cold process restores actual planar physical participants: "+error);
        nlohmann::json expected;{std::ifstream file(root/"physical.json");file>>expected;}
        const auto id=Player(scene);const auto body=world.RuntimeBody(id);const auto pose=world.Physics().GetTransform(body);
        const auto v=world.Physics().GetLinearVelocity(body),n=world.Physics().GetPlanarNormal(body);
        Check(pose.position.x==expected["x"]&&pose.position.y==expected["y"]&&v.x==expected["vx"]&&v.y==expected["vy"]&&n.z==1,"restored endpoint is actual body position/velocity/plane, not cached script status");
        InputSystem input;Tick(world,input,60);Check(world.Physics().GetTransform(body).position.y>-.001f,"cold restored player still has real floor collision");
        std::cout<<"SUMMARY "<<checks<<" checks 0 failures\n";return 0;
    }
    SceneBodyComponent sourceCompatible{false,true,0,kAllCategories};
    Check(sourceCompatible.enabled&&!sourceCompatible.sensor&&!sourceCompatible.planar,"legacy native body-component aggregate retains member order");
    Check(!fs::exists(root),"fresh project output");Project project;
    Check(Project::CreateNew(root.string(),"Planar Playground",project,error),"create normal project: "+error);
    Scene scene;Check(PopulatePlanarProjectStarter(project,scene,true,error),"new 2D platformer has native collision, controller and gravity: "+error);
    project.Settings().startupScene="Scenes/main.judas";
    Check(project.Save(error)&&SaveSceneToFile(scene,project.StartupScenePath(),error),"project/scene serialization");
    Scene loaded;Check(LoadSceneFromFile(project.StartupScenePath(),loaded,error),"reload full planar descriptor");
    const auto id=Player(scene);Check(EncodePlanar(*scene.Find(id)->body->planar)==EncodePlanar(*loaded.Find(id)->body->planar),"lossless native collider serialization");
    AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());ResourceManager resources(nullptr,&assets);
    RuntimeWorld legacy;Check(!legacy.Build(loaded,&resources,error)&&error.find("legacy-gameplay")!=std::string::npos,"legacy spatial player adapter rejects before entering a planar world");
    error.clear();
    RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(loaded,&resources,error),"build native planar runtime: "+error);
    InputSystem input;Check(input.SetMap(project.Settings().input,error),"project owns controls");
    Tick(world,input,120);const auto body=world.RuntimeBody(id);const auto resting=world.Physics().GetTransform(body).position;
    std::cout<<"MEASURE resting="<<resting.x<<','<<resting.y<<'\n';Check(resting.y>.095f&&resting.y<.105f,"capsule lands at actual floor height");
    for(const auto& record:world.Scripts()->Capture())std::cout<<"BEFOREJUMP "<<record.json<<'\n';
    input.SetPhysical("key:Space",1);Tick(world,input,1);input.SetPhysical("key:Space",0);Tick(world,input,12);
    std::cout<<"JUMP y="<<world.Physics().GetTransform(body).position.y<<" vy="<<world.Physics().GetLinearVelocity(body).y<<'\n';
    Check(world.Physics().GetTransform(body).position.y>resting.y+.5f,"ordinary JS jump produces physical upward displacement");
    Tick(world,input,90);input.SetPhysical("key:D",1);Tick(world,input,24);input.SetPhysical("key:D",0);
    Check(world.Physics().GetTransform(body).position.x>1,"ordinary JS tangential acceleration moves actual body");
    const auto original=world.Physics().GetTransform(body);auto sprite=world.RuntimeDefinition(id)->render->sprite;sprite.flipX=!sprite.flipX;sprite.order=17;
    Check(world.SetSprite(id,sprite)&&world.Physics().GetTransform(body).position==original.position,"sprite flips/sorting cannot move physical geometry");
    auto forbidden=world.RuntimeDefinition(id)->transform;const auto definitionBefore=forbidden;
    forbidden.position.z+=1;
    Check(!world.SetRuntimeTransform(id,forbidden)&&world.RuntimeDefinition(id)->transform.position==definitionBefore.position,"off-plane runtime pose rejects before definition publication");
    EntityPhysicalState physicalBefore;world.GetEntityState(id,physicalBefore);auto badPhysical=physicalBefore;badPhysical.linearVelocity.z=1;
    Check(!world.SetEntityState(id,badPhysical)&&world.Physics().GetLinearVelocity(body)==physicalBefore.linearVelocity,"forbidden physical restore rejects before publishing partial endpoint");
    Check(!CanCaptureLegacyWorldState(world,error),"legacy delta saves explicitly reject unsupported planar continuation");
    error.clear();
    GameSnapshot snapshot;snapshot.project="planar-stage2";snapshot.scene="main";snapshot.content=std::string(64,'b');
    Check(WorldPersistence::Capture(world,snapshot.participants,error),"modern capture preserves planar state: "+error);
    SaveStorage storage((root/"ColdSaves").string());Check(storage.Write("planar",snapshot,error),"write modern save");
    const auto p=world.Physics().GetTransform(body).position,v=world.Physics().GetLinearVelocity(body);
    std::ofstream(root/"physical.json")<<nlohmann::json{{"x",p.x},{"y",p.y},{"vx",v.x},{"vy",v.y}};
    // Genuine absent/broken mechanism controls: no floor, mixed physical worlds,
    // forbidden chain authority and invalid geometry must not look successful.
    Scene mixed=scene;auto& spatial=mixed.CreateObject("wrong 3D floor");spatial.body=SceneBodyComponent{};
    RuntimeWorld bad;bad.legacyGameplay=false;Check(!bad.Build(mixed,&resources,error)&&error.find("Mixed")!=std::string::npos,"mixed spatial/planar producer rejects with actionable diagnostic");
    Scene invalid=scene;invalid.Find(id)->body->planar=PlanarChain({{-1,0},{1,0}});
    Check(!bad.Build(invalid,&resources,error),"dynamic chain rejects instead of manufacturing mass");
    Scene noFloor=scene;noFloor.Objects().erase(std::remove_if(noFloor.Objects().begin(),noFloor.Objects().end(),[&](const auto& o){return o.body&&o.body->motion==SceneBodyMotion::Static;}),noFloor.Objects().end());
    RuntimeWorld falling;falling.legacyGameplay=false;Check(falling.Build(noFloor,&resources,error),"no-floor negative control builds");Tick(falling,input,90);
    Check(falling.Physics().GetTransform(falling.RuntimeBody(id)).position.y<-5,"no-floor negative control actually falls; player script does not clamp height");
    Scene topdown;Project topProject;Check(Project::CreateNew((root/"Topdown").string(),"Topdown",topProject,error)&&PopulatePlanarProjectStarter(topProject,topdown,false,error),"create separate top-down project");
    AssetDatabase topAssets;topAssets.Scan(topProject.RootDir(),topProject.AssetsDir());ResourceManager topResources(nullptr,&topAssets);RuntimeWorld top;top.legacyGameplay=false;
    Check(top.Build(topdown,&topResources,error),"top-down native world builds");InputSystem topInput;topInput.SetMap(topProject.Settings().input,error);
    topInput.SetPhysical("key:W",1);Tick(top,topInput,30);Check(top.Physics().GetTransform(top.RuntimeBody(Player(topdown))).position.y>3,"top-down movement follows project up input with no gravity");
    topProject.Settings().startupScene="Scenes/main.judas";Check(topProject.Save(error)&&SaveSceneToFile(topdown,topProject.StartupScenePath(),error),"save editable top-down project");
    const auto probePath=root/"Assets/planar-api.js";
    std::ofstream(probePath)<<R"JS(import {physics} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={};}
 fixedUpdate(){
   if(this.state.done)return;this.state.done=true;
   const info=this.entity.body2D;info.vertices[0].y=99;
   this.state.detached=this.entity.body2D.vertices[0].y===-.3;
   const pose=this.entity.transform,hit=physics.cast2D({kind:'capsule',radius:.3,halfHeight:.3},pose,{x:0,y:-1,z:0},5,{ignored:[this.entity]});
   this.state.query=!!hit&&hit.entity.id!==this.entity.id&&hit.shape==='planar'&&hit.normal.y>.99;
   try{physics.cast2D({kind:'circle',radius:.2},pose,{x:0,y:0,z:1},1)}catch(e){this.state.directionRejected=true;}
   try{physics.cast2D({kind:'chain',vertices:[{x:0,y:0},{x:1,y:0}]},pose,{x:0,y:-1,z:0},1)}catch(e){this.state.chainRejected=true;}
   const oldZ=pose.position.z;pose.position.z=oldZ+1;
   try{this.entity.transform=pose}catch(e){this.state.poseRejected=true;}
   this.state.atomic=this.entity.transform.position.z===oldZ;
   try{this.entity.velocity={x:0,y:0,z:1}}catch(e){this.state.velocityRejected=true;}
   const pin=physics.createJoint(this.entity,{type:'pin2d',bodyA:this.entity});
   this.state.pin=pin.valid&&pin.state.type===4;pin.destroy();
   const weld=physics.createJoint(this.entity,{type:'weld2d',bodyA:this.entity});
   this.state.weld=weld.valid&&weld.state.type===5;weld.destroy();
   try{physics.createJoint(this.entity,{type:'hinge',bodyA:this.entity})}catch(e){this.state.spatialJointRejected=physics.joint(this.entity)===null;}

 }
})JS";
    AssetRecord probe;Check(assets.Track(probePath.string(),probe,error),"track ordinary native 2D API probe");
    Scene api=scene;api.Find(id)->scripts={{1,probe.id,true,"{}"}};
    RuntimeWorld apiWorld;apiWorld.legacyGameplay=false;Check(apiWorld.Build(api,&resources,error),"build public 2D API probe");
    InputSystem apiInput;Tick(apiWorld,apiInput,1);
    const auto observed=nlohmann::json::parse(apiWorld.Scripts()->Capture().front().json);
    for(const auto* key:{"detached","query","directionRejected","chainRejected","poseRejected","atomic","velocityRejected","pin","weld","spatialJointRejected"})
        Check(observed.value(key,false),std::string("actual public JS contract: ")+key);
    Scene prefab;auto& prefabBody=prefab.CreateObject("Native 2D prefab");prefabBody.body=SceneBodyComponent{};prefabBody.body->shape=SceneShape::Planar;
    prefabBody.body->planar=PlanarConvex({{-.4,-.4},{.4,-.4},{0,.5}});prefabBody.body->motion=SceneBodyMotion::Dynamic;
    const auto prefabPath=root/"Assets/planar.judasprefab";Check(SaveSceneToFile(prefab,prefabPath.string(),error),"write ordinary planar prefab");
    AssetRecord prefabAsset;Check(assets.Track(prefabPath.string(),prefabAsset,error),"register ordinary prefab asset");
    SceneTransform spawn;spawn.position={8,4,0};const auto spawned=world.SpawnPrefab(prefabAsset.id,spawn,error);
    Shape spawnedShape;BodyTransform spawnedPose;
    const bool shapeReady=world.Physics().GetBodyShape(world.RuntimeBody(spawned),spawnedShape,spawnedPose);
    std::cout<<"PREFAB id="<<spawned<<" error="<<error<<" shape="<<shapeReady<<" pose="<<spawnedPose.position.x<<','<<spawnedPose.position.y<<'\n';
    Check(spawned&&world.Physics().GetBodyShape(world.RuntimeBody(spawned),spawnedShape,spawnedPose)&&spawnedShape.type==ShapeType::Planar&&
        EncodePlanar(*spawnedShape.planar)==EncodePlanar(*prefabBody.body->planar)&&glm::length(world.Physics().GetTransform(world.RuntimeBody(spawned)).position-spawn.position)<1e-6f&&std::abs(spawnedPose.position.y-3.9f)<1e-6f,"prefab spawns actual convex planar geometry at requested endpoint");

    // Exercise the actual reusable JS preset on prescribed physical supports,
    // not just a native friction surrogate or a cached grounded flag.
    for(bool rotating:{false,true}){
        Scene riding=scene;const float startX=rotating?.8f:0;
        riding.Find(id)->transform.position={startX,1.8f,0};
        auto& support=riding.CreateObject(rotating?"Rotating support":"Translating support");const auto supportId=support.id;
        support.transform.position={0,1,0};support.body=SceneBodyComponent{};
        support.body->shape=SceneShape::Planar;support.body->planar=PlanarBox({3,.2});support.body->motion=SceneBodyMotion::Kinematic;
        support.body->initialLinearVelocity=rotating?glm::vec3(0):glm::vec3(1,0,0);
        support.body->initialAngularVelocity=rotating?glm::vec3(0,0,.1f):glm::vec3(0);
        RuntimeWorld ride;ride.legacyGameplay=false;Check(ride.Build(riding,&resources,error),"build real player/support scene");
        InputSystem rideInput;rideInput.SetMap(project.Settings().input,error);Tick(ride,rideInput,90);
        const auto riderPose=ride.Physics().GetTransform(ride.RuntimeBody(id));const auto supportPose=ride.Physics().GetTransform(ride.RuntimeBody(supportId));
        std::cout<<"RIDE rotating="<<rotating<<" player="<<riderPose.position.x<<','<<riderPose.position.y<<" support="<<supportPose.position.x<<'\n';
        if(rotating)Check(riderPose.position.y>1.85f&&riderPose.position.y<2.1f&&std::abs(riderPose.position.z)<1e-6,"actual JS player is lifted by a rotating physical support");
        else Check(riderPose.position.x>1.2f&&std::abs(riderPose.position.x-supportPose.position.x)<.2&&std::abs(riderPose.position.y-1.8f)<.004,"actual JS player rides translating support without pose parenting");
    }

    EditorDocument authored;authored.NewScene();
    const auto circleId=CreateEditorPlanarPrimitive(authored,project,assets,"planar-circle",{0,2,0},0,error);
    Check(circleId&&authored.Selected()==circleId&&authored.GetScene().Find(circleId)->body->planar->kind==PlanarKind::Circle&&
        assets.Find(authored.GetScene().Find(circleId)->render->textureAsset)->type==AssetType::Texture,"editor creates selected native circle with ordinary matching image asset");
    const auto capsuleId=CreateEditorPlanarPrimitive(authored,project,assets,"planar-capsule",{1,2,0},circleId,error);
    Check(capsuleId&&authored.Selected()==capsuleId&&authored.GetScene().Find(capsuleId)->parent==circleId,"editor rounded 2D creation preserves child/selection workflow");
    const auto imagePath=assets.Find(authored.GetScene().Find(circleId)->render->textureAsset)->path;
    std::ifstream originalImage(imagePath,std::ios::binary);const std::string imageBytes{std::istreambuf_iterator<char>(originalImage),{}};originalImage.close();
    const auto repeatId=CreateEditorPlanarPrimitive(authored,project,assets,"planar-circle",{},0,error);
    std::ifstream repeatedImage(imagePath,std::ios::binary);const std::string reused{std::istreambuf_iterator<char>(repeatedImage),{}};
    Check(repeatId&&reused==imageBytes&&authored.GetScene().Find(repeatId)->render->textureAsset==authored.GetScene().Find(circleId)->render->textureAsset,"repeated creation reuses project image without overwriting artwork");
    Scene picking;auto& chain=picking.CreateObject("Selectable 2D chain");chain.body=SceneBodyComponent{};
    chain.body->shape=SceneShape::Planar;chain.body->planar=PlanarChain({{-2,0},{0,0},{2,1}});
    EditorScenePicker picker;
    Check(picker.Pick(picking,{1.5f,.75f,5},{0,0,-1}).id==chain.id,"editor selects the actual finite chain away from its pivot");
    Check(!picker.Pick(picking,{1.5f,2,5},{0,0,-1}).id,"empty space away from chain is not a hidden filled rectangle");
    StreamingChecks(root,project);
    std::cout<<"SUMMARY "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
