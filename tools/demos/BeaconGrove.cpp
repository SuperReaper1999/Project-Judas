// Fresh-project workflow fixture: uses the same starter/import/recipe serializers
// as the editor, never clones a pre-existing game or introduces engine gameplay.
#include "ProjectStarter.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "SpriteAnimation.h"
#include "authoring/Tilemap.h"
#include "RuntimeUI.h"
#include "ScriptSystem.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
int main(int argc,char** argv){try{
 if(argc!=3){std::cerr<<"usage: judas_beacon_grove repository-root fresh-project-directory\n";return 2;}
 const auto source=fs::absolute(argv[1]),root=fs::absolute(argv[2]);
 if(fs::exists(root))throw std::runtime_error("Destination exists; preserve edited projects");
 Project project;Scene scene;std::string error;
 auto require=[&](bool ok){if(!ok)throw std::runtime_error(error);};
 require(Project::CreateNew(root.string(),"Beacon Grove",project,error));
 require(PopulatePlanarProjectStarter(project,scene,false,error));
 project.Settings().startupScene="Scenes/main.judas";scene.Settings().name="Beacon Grove";
 const fs::path assetsDir(project.AssetsDir());
 fs::copy_file(assetsDir/"game.js",assetsDir/"motion.js");
 fs::copy_file(source/"tools/demos/beacon_grove/player.js",assetsDir/"game.js",fs::copy_options::overwrite_existing);
 fs::copy_file(source/"tools/demos/beacon_grove/patrol.js",assetsDir/"patrol.js");
 fs::copy_file(source/"tools/demos/beacon_grove/pause.js",assetsDir/"pause.js");
 fs::copy_file(source/"projects/tilemap_workshop/Assets/tiles.png",assetsDir/"tiles.png");
 fs::copy_file(source/"projects/sprite_animation_workshop/Assets/traveller.png",assetsDir/"traveller.png");
 AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());
 auto track=[&](const char* file){AssetRecord r;require(assets.Track((assetsDir/file).string(),r,error));return r.id;};
 const auto image=track("tiles.png"),actor=track("traveller.png"),patrol=track("patrol.js");track("motion.js");
 SpriteSheet tiles;tiles.texture=image;tiles.width=tiles.height=128;tiles.pixelsPerUnit=32;
 require(SliceSpriteSheet(tiles,{32,32},{0,0},{0,0},error));
 require(SaveSpriteSheet((assetsDir/"grove.judassprite").string(),tiles,error));const auto palette=track("grove.judassprite");
 SpriteSheet traveller;require(LoadSpriteSheet((source/"projects/sprite_animation_workshop/Assets/traveller.judassprite").string(),traveller,error));traveller.texture=actor;
 require(SaveSpriteSheet((assetsDir/"traveller.judassprite").string(),traveller,error));const auto animation=track("traveller.judassprite");
 // Keep the ordinary starter player and its HUD; author a new level from scratch.
 std::vector<SceneObjectId> remove;for(const auto& o:scene.Objects())if(o.id!=1&&!o.ui)remove.push_back(o.id);
 for(auto id:remove)scene.DestroyObject(id);
 auto tag=[&](const char* name){unsigned index=0;project.Settings().classification.tags.Add(name,index);return CategoryMask(1)<<index;};
 const auto cellTag=tag("lantern-cell"),beaconTag=tag("beacon"),exitTag=tag("exit"),dangerTag=tag("danger");
 const auto owner=scene.CreateObject("Grove grid - edit in Tilemap painter").id;
 TilemapRecipe recipe;recipe.root=owner;recipe.sheet=palette;
 recipe.layers={{"ground","Solid walls",0,0,true},{"meadow","Meadow",-2,-10,false},{"path","Paths",-1,-5,false},{"trees","Trees",0,2,false}};
 auto paint=[&](const char* layer,int x,int y,int bx,int by,const char* frame){require(PaintTilemap(recipe,layer,{x,y},{bx,by},frame,false,false,false,error));};
 paint("meadow",-8,-6,15,6,"cell-2-3");paint("path",-1,-1,13,1,"cell-1-3");
 paint("ground",-8,-6,15,-6,"cell-0-3");paint("ground",-8,6,15,6,"cell-0-3");
 paint("ground",-8,-5,-8,5,"cell-0-3");paint("ground",15,-5,15,5,"cell-0-3");
 paint("ground",10,-5,10,-2,"cell-3-0");paint("ground",10,2,10,5,"cell-3-0");
 for(int x:{-6,-3,1,5,8,13}){paint("trees",x,4,x,5,"cell-2-1");paint("trees",x,-5,x,-4,"cell-2-1");}
 Scene compiled;TilemapRecipe baked;require(BakeTilemap(scene,recipe,assets,compiled,baked,error));scene=std::move(compiled);
 auto sprite=[&](const char* name,glm::vec2 at,int frame,glm::vec2 size){auto& o=scene.CreateObject(name);o.transform.position={at,0};o.render=SceneRenderComponent{};o.render->shape=SceneShape::Sprite;o.render->color={1,1,1};o.render->textureAsset=image;o.render->sprite.size=size;o.render->sprite.rect={float(frame%4)/4,float(frame/4)/4,.25,.25};o.render->sprite.sortingLayer=1;return o.id;};
 auto body=[&](SceneObjectId id,bool sensor,glm::dvec2 half){auto* o=scene.Find(id);o->body=SceneBodyComponent{};o->body->shape=SceneShape::Planar;o->body->planar=PlanarBox(half);o->body->sensor=sensor;};
 const auto gate=sprite("Beacon gate - real static collider",{10,0},14,{1,3});body(gate,false,{.5,1.5});
 for(int i=0;i<3;++i){auto id=sprite("Lantern cell",{2.f+2*i,0},10,{.6,.6});body(id,true,{.3,.3});scene.Find(id)->tags=cellTag;}
 auto id=sprite("Restore beacon here with E",{8,0},15,{1,1});body(id,true,{1,1});scene.Find(id)->tags=beaconTag;
 id=sprite("East exit",{13,0},13,{1,2});body(id,true,{.5,1});scene.Find(id)->tags=exitTag;
 id=sprite("Danger brambles - avoid the west patch",{-3,0},6,{2,2});body(id,true,{1,1});scene.Find(id)->tags=dangerTag;
 id=sprite("Patrolling grove wisp",{0,3},10,{.8,.8});body(id,true,{.4,.4});scene.Find(id)->body->motion=SceneBodyMotion::Kinematic;scene.Find(id)->tags=dangerTag;scene.Find(id)->scripts.push_back({1,patrol,true,"{}"});
 auto* player=scene.Find(1);player->transform.position={0,0,0};player->render->spritePlayback=SpritePlayback{animation,"idle","",0,1,true,true};
 player->scripts[0].properties="{\"sideView\":false}";
 require(ScriptSystem::SetPropertyEntity(player->scripts[0].properties,"gate",gate));
 // A second ordinary body makes manipulation/collision observable during play.
 id=sprite("Pushable supply crate",{4,-2},8,{.8,.8});body(id,false,{.4,.4});scene.Find(id)->body->motion=SceneBodyMotion::Dynamic;scene.Find(id)->body->mass=2;
 UIDocument hud;require(LoadUIDocument((assetsDir/"hud.judasui").string(),hud,error));UIElement scrim;scrim.id="help-background";scrim.parent="canvas";scrim.kind=UIKind::Panel;scrim.anchorMax={1,0};scrim.size={0,135};scrim.background={.015f,.03f,.04f,.94f};hud.elements.insert(hud.elements.begin()+1,scrim);require(SaveUIDocument((assetsDir/"hud.judasui").string(),hud,error));
 UIDocument menu;menu.visible=false;
 auto element=[&](const char* key,const char* parent,UIKind kind,glm::vec2 at,glm::vec2 size,const char* text){UIElement e;e.id=key;e.parent=parent;e.kind=kind;e.offset=at;e.size=size;e.text=text;e.fontSize=22;e.wrap=true;e.background=kind==UIKind::Button?glm::vec4(.12,.3,.28,1):glm::vec4(0);e.textAlign={.5,.5};menu.elements.push_back(e);};
 element("canvas","",UIKind::Canvas,{0,0},{1280,720},"");menu.elements.back().background={.01,.02,.025,.85};
 element("menu","canvas",UIKind::Panel,{-220,-210},{440,420},"");menu.elements.back().anchorMin=menu.elements.back().anchorMax={.5,.5};menu.elements.back().background={.025,.08,.09,1};
 element("title","menu",UIKind::Text,{20,20},{400,70},"BEACON GROVE\nPaused");
 element("resume","menu",UIKind::Button,{40,115},{360,55},"Resume");
 element("retry","menu",UIKind::Button,{40,190},{360,55},"Retry from the beginning");
 element("quit","menu",UIKind::Button,{40,265},{360,55},"Quit");
 element("help","menu",UIKind::Text,{20,340},{400,60},"Escape to resume | arrows + Enter or click");
 require(SaveUIDocument((assetsDir/"pause.judasui").string(),menu,error));const auto pauseUI=track("pause.judasui"),pauseScript=track("pause.js");
 auto& pause=scene.CreateObject("Pause menu - ordinary editable UI");pause.ui=SceneUIComponent{pauseUI,"grove_pause",true};pause.scripts.push_back({1,pauseScript,true,"{}"});
 fs::copy_file(source/"tools/demos/beacon_grove/README.md",root/"README.md");
 for(auto [action,key]:{std::pair<const char*,const char*>{"interact","key:E"},{"restart","key:R"},{"save_game","key:F6"},{"load_game","key:F7"}}){project.Settings().input.Add(action,false);project.Settings().input.AddBinding(action,{key});}
 project.Settings().input.AddBinding("save_game",{"key:K"});
 project.Settings().input.AddBinding("load_game",{"key:L"});
 require(SaveSceneToFile(scene,project.StartupScenePath(),error));require(project.Save(error));
 std::cout<<project.ProjectFile()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
