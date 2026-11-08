// Fresh-project generator for the Sprout Patch farming slice. Uses the same
// starter/import/recipe/UI serializers as the editor; never clones an existing game.
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
 if(argc!=3){std::cerr<<"usage: judas_sprout_patch repository-root fresh-project-directory\n";return 2;}
 const auto source=fs::absolute(argv[1]),root=fs::absolute(argv[2]);
 if(fs::exists(root))throw std::runtime_error("Destination exists; preserve edited projects");
 Project project;Scene scene;std::string error;
 auto require=[&](bool ok){if(!ok)throw std::runtime_error(error);};
 require(Project::CreateNew(root.string(),"Sprout Patch",project,error));
 require(PopulatePlanarProjectStarter(project,scene,false,error));
 project.Settings().startupScene="Scenes/main.judas";scene.Settings().name="Sprout Patch";
 const fs::path assetsDir(project.AssetsDir()),art=source/"tools/demos/sprout_patch";
 fs::copy_file(art/"game.js",assetsDir/"game.js",fs::copy_options::overwrite_existing);
 fs::copy_file(art/"farm_tiles.png",assetsDir/"farm_tiles.png");
 fs::copy_file(source/"projects/sprite_animation_workshop/Assets/traveller.png",assetsDir/"traveller.png");
 AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());
 auto track=[&](const char* file){AssetRecord r;require(assets.Track((assetsDir/file).string(),r,error));return r.id;};
 const auto image=track("farm_tiles.png"),actor=track("traveller.png");
 SpriteSheet tiles;tiles.texture=image;tiles.width=tiles.height=128;tiles.pixelsPerUnit=32;
 require(SliceSpriteSheet(tiles,{32,32},{0,0},{0,0},error));
 require(SaveSpriteSheet((assetsDir/"farm.judassprite").string(),tiles,error));const auto palette=track("farm.judassprite");
 SpriteSheet traveller;require(LoadSpriteSheet((source/"projects/sprite_animation_workshop/Assets/traveller.judassprite").string(),traveller,error));traveller.texture=actor;
 require(SaveSpriteSheet((assetsDir/"traveller.judassprite").string(),traveller,error));const auto animation=track("traveller.judassprite");
 // Keep only the ordinary starter player; author the farm from scratch.
 std::vector<SceneObjectId> remove;for(const auto& o:scene.Objects())if(o.id!=1)remove.push_back(o.id);
 for(auto id:remove)scene.DestroyObject(id);
 // Farm grid: 20 x 14 one-metre cells, x -10..9, y -7..6. Soil/crop layers start
 // empty and are edited at runtime by game.js through Entity.setTile.
 const auto owner=scene.CreateObject("Farm grid - edit in Tilemap painter").id;
 TilemapRecipe recipe;recipe.root=owner;recipe.sheet=palette;
 recipe.layers={{"ground","Grass",-4,0,false},{"path","Paths",-3,0,false},{"soil","Tilled soil",-2,0,false},
  {"crops","Crops",-1,0,false},{"solid","Fence and pond",-1,1,true}};
 auto paint=[&](const char* layer,int x,int y,int bx,int by,const char* frame){require(PaintTilemap(recipe,layer,{x,y},{bx,by},frame,false,false,false,error));};
 paint("ground",-10,-7,9,6,"cell-0-0");
 for(auto [x,y]:{std::pair{-6,-5},{7,4},{-2,4},{6,-6},{-8,-3},{2,5},{8,-2},{-4,2}})paint("ground",x,y,x,y,"cell-0-1");
 // Path ring framing the field (cells x -3..4, y -4..0) and a lane to the pond.
 paint("path",-4,-5,5,-5,"cell-0-2");paint("path",-4,1,5,1,"cell-0-2");
 paint("path",-4,-4,-4,0,"cell-0-2");paint("path",5,-4,5,0,"cell-0-2");paint("path",-6,1,-5,1,"cell-0-2");
 // Solid fence border and pond (x -9..-7, y 1..4): real planar collision.
 paint("solid",-10,-7,9,-7,"cell-0-3");paint("solid",-10,6,9,6,"cell-0-3");
 paint("solid",-10,-6,-10,5,"cell-0-3");paint("solid",9,-6,9,5,"cell-0-3");
 paint("solid",-9,1,-7,4,"cell-1-0");
 Scene compiled;TilemapRecipe baked;require(BakeTilemap(scene,recipe,assets,compiled,baked,error));scene=std::move(compiled);
 // Target cursor: a body-free sprite moved by game.js in presentationUpdate.
 auto& cursor=scene.CreateObject("Target cursor - follows the player's facing");cursor.render=SceneRenderComponent{};
 cursor.render->shape=SceneShape::Sprite;cursor.render->color={1,1,1};cursor.render->textureAsset=image;
 cursor.render->sprite.size={1,1};cursor.render->sprite.rect={.75f,.5f,.25f,.25f};cursor.render->sprite.sortingLayer=0;
 const auto cursorId=cursor.id;
 auto* player=scene.Find(1);player->name="Farmer - editable game.js";player->transform.position={.5f,3.f,0};
 player->render->spritePlayback=SpritePlayback{animation,"idle","",0,1,true,true};player->render->sprite.sortingLayer=1;
 player->scripts[0].properties="{}";
 require(ScriptSystem::SetPropertyEntity(player->scripts[0].properties,"grid",owner));
 require(ScriptSystem::SetPropertyEntity(player->scripts[0].properties,"cursor",cursorId));
 // Mobile-first HUD. Buttons are non-interactive panels: game.js hit-tests every
 // finger itself, so a second finger can press USE while the first steers.
 UIDocument hud;
 auto element=[&](const char* key,const char* parent,UIKind kind,glm::vec2 anchor,glm::vec2 align,glm::vec2 at,glm::vec2 size,glm::vec4 background,float font,const char* text){
  UIElement e;e.id=key;e.parent=parent;e.kind=kind;e.anchorMin=e.anchorMax=anchor;e.align=align;e.offset=at;e.size=size;
  e.background=background;e.fontSize=font;e.text=text;e.wrap=true;e.textAlign={.5f,.5f};hud.elements.push_back(e);return &hud.elements.back();};
 const glm::vec4 tool{.12f,.24f,.16f,.85f},clear{0,0,0,0};
 auto* canvas=element("canvas","",UIKind::Canvas,{0,0},{0,0},{0,0},{1280,720},clear,22,"");canvas->anchorMax={1,1};
 auto* bar=element("status-bg","canvas",UIKind::Panel,{0,0},{0,0},{0,0},{0,104},{.02f,.05f,.03f,.8f},22,"");bar->anchorMax={1,0};
 // Full-width text with side margins, so narrow portrait screens wrap instead of clipping.
 auto* status=element("status","canvas",UIKind::Text,{0,0},{0,0},{0,6},{0,40},clear,28,"");status->anchorMax={1,0};status->margin={24,0,24,0};status->textAlign={0,.5f};
 auto* message=element("message","canvas",UIKind::Text,{0,0},{0,0},{0,46},{0,56},clear,19,"");message->anchorMax={1,0};message->margin={24,0,24,0};message->textAlign={0,0};
 element("btn-save","canvas",UIKind::Text,{1,0},{1,0},{-24,118},{130,56},tool,22,"SAVE");
 element("btn-load","canvas",UIKind::Text,{1,0},{1,0},{-24,184},{130,56},tool,22,"LOAD");
 element("btn-use","canvas",UIKind::Text,{1,1},{1,1},{-40,-40},{200,200},{.86f,.45f,.18f,.92f},40,"USE");
 element("btn-water","canvas",UIKind::Text,{1,1},{1,1},{-264,-40},{180,64},tool,24,"WATER");
 element("btn-seeds","canvas",UIKind::Text,{1,1},{1,1},{-264,-114},{180,64},tool,24,"SEEDS");
 element("btn-hoe","canvas",UIKind::Text,{1,1},{1,1},{-264,-188},{180,64},tool,24,"HOE");
 element("joy-base","canvas",UIKind::Panel,{0,0},{.5f,.5f},{150,570},{220,220},{1,1,1,.12f},22,"");
 element("joy-knob","canvas",UIKind::Panel,{0,0},{.5f,.5f},{150,570},{96,96},{1,1,1,.35f},22,"");
 // Replace the starter's already-registered HUD content, keeping its asset identity.
 require(SaveUIDocument((assetsDir/"hud.judasui").string(),hud,error));
 const auto* hudRecord=assets.FindByRelativePath("Assets/hud.judasui");if(!hudRecord)throw std::runtime_error("starter HUD asset missing");const auto hudAsset=hudRecord->id;
 auto& hudObject=scene.CreateObject("Farm HUD and touch controls");hudObject.ui=SceneUIComponent{hudAsset,"farm_hud",true};
 fs::copy_file(art/"README.md",root/"README.md");
 auto& input=project.Settings().input;
 for(auto [action,keys]:{std::pair<const char*,std::vector<const char*>>{"use",{"key:Space","key:E","pad:South"}},
   {"tool_hoe",{"key:1","pad:West"}},{"tool_seeds",{"key:2","pad:North"}},{"tool_water",{"key:3","pad:East"}},
   {"tap",{"mouse:Left"}},{"restart",{"key:R"}},{"save_game",{"key:F6","key:K"}},{"load_game",{"key:F7","key:L"}}}){
  if(!input.Find(action))input.Add(action,false);
  for(auto key:keys)input.AddBinding(action,{key});
 }
 require(SaveSceneToFile(scene,project.StartupScenePath(),error));require(project.Save(error));
 std::cout<<project.ProjectFile()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
