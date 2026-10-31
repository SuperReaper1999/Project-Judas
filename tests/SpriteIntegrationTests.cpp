#include "AssetDatabase.h"
#include "InputSystem.h"
#include "Project.h"
#include "ProjectStarter.h"
#include "Prefab.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "ScriptSystem.h"
#include "WorldPersistence.h"
#include "SaveStorage.h"
#include "../third_party/nlohmann/json.hpp"
#include "editor/EditorPicking.h"
#include "editor/EditorCreationMenu.h"
#include "editor/EditorDocument.h"
#include "editor/EditorCamera.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
unsigned checks=0;
void Check(bool pass,const std::string& name){++checks;if(!pass)throw std::runtime_error(name);std::cout<<"PASS "<<name<<'\n';}
int main(int argc,char** argv){try{
    if(argc==3&&std::string(argv[2])=="--restore"){
        const auto root=fs::absolute(argv[1]);std::string error,path,expected;
        {std::ifstream file(root/"project-path.txt");std::getline(file,path);}
        {std::ifstream file(root/"expected-sprite.json");std::getline(file,expected);}
        Project project;Check(project.Load(path,error),"cold process opens saved project");
        AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());ResourceManager resources(nullptr,&assets);
        SaveStorage storage((root/"ColdSaves").string());GameSnapshot snapshot;bool recovered=false;
        Check(storage.Read("sprite",snapshot,recovered,error),"cold process reads original slot with checksum");
        Scene scene;Check(WorldPersistence::SceneFrom(snapshot.participants,scene,error),"cold process reconstructs exact sprite scene");
        RuntimeWorld world;world.legacyGameplay=false;
        Check(world.Build(scene,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(world,error)&&WorldPersistence::Restore(world,snapshot.participants,error),"cold process restores full participants: "+error);
        const SceneObject* player=nullptr;for(const auto& o:scene.Objects())if(!o.scripts.empty())player=&o;
        Check(player&&EncodeSprite(world.RuntimeDefinition(player->id)->render->sprite)==expected&&world.view&&world.view->lens.pixelPerfect&&world.view->lens.projection==CameraProjectionKind::Orthographic,"cold process actual sprite and lens endpoint matches saved state");
    std::cout<<"SUMMARY checks="<<checks<<" failures=0\n";return 0;
    }
    if(argc!=2)return 2;
    const auto root=fs::absolute(argv[1]);Check(!fs::exists(root),"fresh test output");
    std::string error;Project project;
    Check(Project::CreateNew(root.string(),"Sprite Workshop",project,error),"create normal project: "+error);
    Scene scene;Check(Populate2DProjectStarter(project,scene,false,error),"create native 2D starter: "+error);
    project.Settings().startupScene="Scenes/main.judas";
    Check(project.Save(error)&&SaveSceneToFile(scene,project.StartupScenePath(),error),"save 2D project and scene");
    Check(!Populate2DProjectStarter(project,scene,false,error),"starter refuses overwriting authored assets");
    Project reopened;Scene loaded;
    Check(reopened.Load(project.ProjectFile(),error)&&reopened.Settings().editor2D&&LoadSceneFromFile(project.StartupScenePath(),loaded,error),"reopen project default 2D view and native sprite scene");
    AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());
    ResourceManager resources(nullptr,&assets);RuntimeWorld world;world.legacyGameplay=false;
    Check(world.Build(loaded,&resources,error,&project.Settings().classification,&project.Settings().navigation),"runtime builds ordinary sprites: "+error);
    world.viewportWidth=320;world.viewportHeight=240;
    InputSystem input;world.UpdateScripts(&input,1.f/60);world.PresentationScripts(&input,1.f/60,.5f);
    Check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),"public starter script runs without faults");
    Check(world.view&&world.view->lens.projection==CameraProjectionKind::Orthographic&&world.view->lens.pixelPerfect,"project JS owns actual orthographic pixel-perfect view");
    const SceneObject* player=nullptr;for(const auto& o:loaded.Objects())if(!o.scripts.empty())player=&o;
    Check(player&&player->render&&player->render->shape==SceneShape::Sprite&&!player->body,"sprite player is explicitly visual, no hidden physics");
    auto sprite=player->render->sprite;sprite.flipX=true;sprite.order=7;
    Check(world.SetSprite(player->id,sprite)&&world.RuntimeDefinition(player->id)->render->sprite==sprite,"runtime sprite mutation updates actual visual definition");
    auto invalid=sprite;invalid.size.x=-1;
    Check(!world.SetSprite(player->id,invalid)&&world.RuntimeDefinition(player->id)->render->sprite==sprite,"invalid mutation atomically rejected");
    GameSnapshot snapshot;snapshot.project="sprite-stage1";snapshot.scene="main";snapshot.content=std::string(64,'a');
    Check(WorldPersistence::Capture(world,snapshot.participants,error),"capture modern complete sprite/lens participants: "+error);
    SaveStorage storage((root/"ColdSaves").string());
    Check(storage.Write("sprite",snapshot,error),"write checked production slot to disk: "+error);
    std::ofstream(root/"project-path.txt")<<project.ProjectFile();
    std::ofstream(root/"expected-sprite.json")<<EncodeSprite(sprite);
    Scene savedScene;Check(WorldPersistence::SceneFrom(snapshot.participants,savedScene,error),"restore modern scene definitions");
    RuntimeWorld restored;restored.legacyGameplay=false;
    Check(restored.Build(savedScene,&resources,error,nullptr,nullptr,true)&&WorldPersistence::Prepare(restored,error)&&WorldPersistence::Restore(restored,snapshot.participants,error),"fresh runtime restores complete participants: "+error);
    Check(restored.RuntimeDefinition(player->id)->render->sprite==sprite&&restored.view&&restored.view->lens==world.view->lens,"actual modern restored sprite settings and camera lens match saved state");
    Scene bad=loaded;bad.Objects()[0].body=SceneBodyComponent{};bad.Objects()[0].body->shape=SceneShape::Sprite;
    RuntimeWorld rejected;Check(!rejected.Build(bad,&resources,error),"sprite visual cannot silently become an unsupported collision shape");
    const auto probePath=fs::path(project.AssetsDir())/"api-probe.js";
    std::ofstream(probePath)<<R"JS(import {world,players} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={};}
 update(){
   world.setView({position:{x:0,y:0,z:5}},60,{projection:'orthographic',height:4,near:.01,far:100});
   const point=world.project({x:1,y:0,z:0});const ray=world.unproject({x:point.x,y:point.y});
   this.state.ray=!!ray&&Math.abs(ray.origin.x-1)<.0001&&Math.abs(ray.direction.z+1)<.0001;
   const detached=this.entity.sprite;detached.order=99;
   this.state.detached=this.entity.sprite.order!==99;
   this.state.patched=this.entity.setSprite({flipX:true,rect:{x:0,y:0,width:.5,height:1}})&&this.entity.sprite.flipX;
   try{this.entity.setSprite({size:{x:-1,y:1}})}catch(e){this.state.invalid=true}
   this.state.atomic=this.entity.sprite.size.x===1;
   const left=players.create('left'),right=players.create('right');
   left.viewport={x:0,y:0,width:.5,height:1};right.viewport={x:.5,y:0,width:.5,height:1};
   left.setView({position:{x:0,y:0,z:5}},60,{projection:'orthographic',height:4});
   right.setView({position:{x:4,y:0,z:5}},60,{projection:'orthographic',height:4});
   this.state.independent=left.project({x:0,y:0,z:0}).inside&&!right.project({x:0,y:0,z:0}).inside;
   const lr=left.ray({x:.25,y:.25}),rr=left.ray({x:.75,y:.75});
   this.state.parallel=!!lr&&!!rr&&Math.abs(lr.direction.x-rr.direction.x)<.00001;
   left.leave();right.leave();
 }
})JS";
    AssetRecord probe;Check(assets.Track(probePath.string(),probe,error),"track ordinary API probe");
    Scene api;auto& probeObject=api.CreateObject("Sprite API probe");probeObject.render=SceneRenderComponent{};
    probeObject.render->shape=SceneShape::Sprite;probeObject.scripts.push_back({1,probe.id,true,"{}"});
    RuntimeWorld apiWorld;apiWorld.legacyGameplay=false;Check(apiWorld.Build(api,&resources,error),"API probe world builds");
    apiWorld.viewportWidth=320;apiWorld.viewportHeight=240;apiWorld.UpdateScripts(&input,1.f/60);
    Check(apiWorld.Scripts()->Diagnostics().empty(),"sprite/projection/multiplayer API probe has no VM faults");
    const auto observed=nlohmann::json::parse(apiWorld.Scripts()->Capture().front().json);
    for(const auto* key:{"ray","detached","patched","invalid","atomic","independent","parallel"})
        Check(observed.value(key,false),std::string("public JS contract: ")+key);
    Scene picking;auto& back=picking.CreateObject("back");const auto backId=back.id;
    back.render=SceneRenderComponent{};back.render->shape=SceneShape::Sprite;back.render->sprite.size={2,2};
    auto& front=picking.CreateObject("front");const auto frontId=front.id;
    front.render=player->render;front.render->sprite.size={2,2};front.render->sprite.pivot={.5f,.5f};front.render->sprite.sortingLayer=3;
    EditorScenePicker picker;
    Check(picker.Pick(picking,{0,0,5},{0,0,-1},&assets,&resources,&error).id==frontId,"visible image pixels select front sorted sprite");
    Check(picker.Pick(picking,{-.95f,.95f,5},{0,0,-1},&assets,&resources,&error).id==backId,"transparent PNG border selects sprite underneath");
    picking.Find(frontId)->render->sprite.alphaPick=false;
    Check(picker.Pick(picking,{-.95f,.95f,5},{0,0,-1},&assets,&resources,&error).id==frontId,"explicit bounds picking includes transparent border");
    const auto hits=picker.PickAll(picking,{0,0,5},{0,0,-1},&assets,&resources,&error);
    Check(hits.size()==2&&hits[0].id==frontId&&hits[1].id==backId,"overlap cycling exposes actual sorted hits");
    picking.Find(frontId)->transform.position={3,0,0};picking.Find(backId)->parent=frontId;
    picking.Find(backId)->transform.position={2,0,-.1f};
    Check(picker.Pick(picking,{5,0,5},{0,0,-1},&assets,&resources,&error).id==backId,"child sprite selection follows its actual parented offset outside the parent bounds");
    auto& frontSprite=picking.Find(frontId)->render->sprite;frontSprite.pivot={0,.5f};frontSprite.flipX=true;
    Check(picker.Pick(picking,{1.5f,0,5},{0,0,-1},&assets,&resources,&error).id==frontId,"off-centre flipped pivot moves pick bounds to the same side as rendered geometry");
    picking.Find(frontId)->render->textureAsset="missing-image";frontSprite.alphaPick=true;
    Check(picker.Pick(picking,{1.5f,0,5},{0,0,-1},&assets,&resources,&error).id==frontId,"missing image placeholder remains selectable by bounds");
    EditorDocument document;document.NewScene();
    const auto created=CreateEditorSpriteFromImage(document,assets,player->render->textureAsset,{2,3,0},0,error);
    Check(created&&document.Selected()==created,"image placement immediately selects new sprite");
    const auto* object=document.GetScene().Find(created);
    Check(object&&object->render->sprite.size==glm::vec2(1,1.25f)&&!object->body,"PNG dimensions determine aspect/32px per unit without collision");
    document.Undo();Check(!document.GetScene().Find(created),"image placement is one undo transaction");
    document.Redo();Check(document.GetScene().Find(created),"redo restores sprite asset, transform and selection state");
    Scene prefab;
    Check(CreatePrefab(document.GetScene(),created,prefab,error),"create sprite prefab through existing scene boundary");
    auto properties=ObjectProperties(prefab.Objects()[0]);
    Check(properties.count("render.sprite-v1")!=0,"prefab properties retain native sprite data");
    SpriteSettings changed=prefab.Objects()[0].render->sprite;changed.flipY=true;changed.order=9;
    auto edited=prefab.Objects()[0];edited.render->sprite=changed;
    Check(ApplyObjectProperties(prefab.Objects()[0],{{"render.sprite-v1",ObjectProperties(edited).at("render.sprite-v1")}},error)&&prefab.Objects()[0].render->sprite==changed,"generic prefab property override updates actual sprite settings");
    auto invalidProperties=properties;invalidProperties["render.sprite-v1"]="{}";
    Check(!ApplyObjectProperties(prefab.Objects()[0],invalidProperties,error),"malformed sprite prefab overrides reject");
    EditorCamera editorCamera;const auto previous=editorCamera.Position();editorCamera.Set2D(true);
    editorCamera.LookAt({2,3,0},5);glm::vec3 rayA,rayB,dirA,dirB;
    editorCamera.PixelRay(20,30,320,240,rayA,dirA);editorCamera.PixelRay(200,160,320,240,rayB,dirB);
    Check(glm::length(dirA-dirB)<1e-6f&&glm::length(rayA-rayB)>1,"2D editor picks with parallel rays and distinct world origins");
    const auto height=editorCamera.OrthographicHeight();editorCamera.Dolly(1);
    Check(editorCamera.OrthographicHeight()<height&&editorCamera.Position()==glm::vec3(2,3,10),"wheel zoom changes orthographic scale, not camera position");
    editorCamera.Set2D(false);Check(editorCamera.Position()==previous,"switching back restores the previous 3D editor camera");
    PlanetSettings planetSettings;planetSettings.radius=100;planetSettings.maxLevel=8;
    PlanetSurface surface(planetSettings);
    const glm::dvec3 eye{0,200,0};
    const auto ortho=SelectPlanetTiles(surface,std::vector<PlanetView>{{eye,.5,240,20}});
    const auto otherFov=SelectPlanetTiles(surface,std::vector<PlanetView>{{eye,2.0,240,20}});
    const auto zoomedOut=SelectPlanetTiles(surface,std::vector<PlanetView>{{eye,.5,240,2000}});
    Check(ortho.leaves==otherFov.leaves,"orthographic planetary LOD is independent of unused perspective FOV");
    Check(zoomedOut.leaves.size()<ortho.leaves.size(),"orthographic height changes actual terrain refinement rather than using eye distance");
    bool rejectedLens=false;try{SelectPlanetTiles(surface,std::vector<PlanetView>{{eye,.5,240,-1}});}catch(const std::exception&){rejectedLens=true;}
    Check(rejectedLens,"invalid orthographic terrain projection rejects before publication");
    Scene camera;auto& c=camera.CreateObject("ortho render camera");c.renderCamera=SceneRenderCameraComponent{};
    c.renderCamera->lens=world.view->lens;c.renderCamera->lens.height=7;
    Check(SaveSceneToFile(camera,(root/"camera.judas").string(),error),"save authored orthographic render camera");
    Scene roundtrip;Check(LoadSceneFromFile((root/"camera.judas").string(),roundtrip,error)&&roundtrip.Objects()[0].renderCamera->lens==c.renderCamera->lens,"render camera lens roundtrips exactly");
    Scene defaults;auto& perspective=defaults.CreateObject("default perspective camera");
    perspective.renderCamera=SceneRenderCameraComponent{};
    std::string defaultText;Check(SaveSceneToString(defaults,defaultText),"serialize default perspective camera");
    Check(defaultText.find("camera.projection")==std::string::npos,"default writer intentionally omits lens extension");
    Scene defaultRead;Check(LoadSceneFromString(defaultText,defaultRead,error)&&defaultRead.Objects()[0].renderCamera->lens==CameraLens{},"legacy/default perspective camera loads without new 2D fields");
    auto partial=defaultText;const auto at=partial.find("  render-camera");
    Check(at!=std::string::npos,"default camera record exists for malformed-extension control");
    partial.insert(at,"  camera.projection \"orthographic\"\n");
    Check(!LoadSceneFromString(partial,defaultRead,error),"partial explicit lens extension still rejects");
    std::string preserved;Check(SaveSceneToString(defaultRead,preserved)&&preserved==defaultText,"failed lens extension preserves the previously loaded scene");
    std::cout<<"SUMMARY checks="<<checks<<" failures=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
