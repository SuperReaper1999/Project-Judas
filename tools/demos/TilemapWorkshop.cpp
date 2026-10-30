#include "ProjectStarter.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "SpriteAnimation.h"
#include "authoring/Tilemap.h"
#include "RuntimeUI.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
int main(int argc,char** argv){try{
 if(argc!=3)return 2;
 const auto root=fs::absolute(argv[1]);if(fs::exists(root))throw std::runtime_error("Output must be fresh; preserve edited projects");
 Project project;Scene base;std::string error;
 if(!Project::CreateNew(root.string(),"Tilemap Workshop",project,error)||!PopulatePlanarProjectStarter(project,base,true,error))throw std::runtime_error(error);
 std::vector<SceneObjectId> remove;
 for(const auto& o:base.Objects())if(o.name=="Backdrop - editable sprite tiles"||(o.body&&o.body->motion==SceneBodyMotion::Static))remove.push_back(o.id);
 for(auto id:remove)base.DestroyObject(id);
 for(auto& o:base.Objects())if(o.body&&o.body->motion==SceneBodyMotion::Dynamic)o.transform.position={-8,2,0};
 const auto folder=fs::path(project.AssetsDir());fs::copy_file(argv[2],folder/"tiles.png");
 std::ofstream(folder/"workshop.js")<<R"JS(import {input,saves,scenes,session,ui} from 'judas';
export default class {
 constructor(){this.state={request:null};}
 uiUpdate(){
  if(!saves.busy){if(input.pressed('side_view'))scenes.load('Scenes/main.judas');
   else if(input.pressed('top_view'))scenes.load('Scenes/village.judas');
   else if(input.pressed('restart'))scenes.reload();
   else if(input.pressed('save_game'))this.state.request=saves.save('tilemap');
   else if(input.pressed('load_game'))this.state.request=saves.load('tilemap');}
  const doc=ui.get('sprite_help'),status=this.state.request?saves.status(this.state.request):null;
  if(doc)doc.get('help').text='TILEMAP TRAIL | WASD / arrows | Space: jump | 1: Trail | 2: Village | R: restart\nFind the stars! '+(session.get('stars')??0)+' collected | F6: save | F7: load'+(status?' | '+status.state+' '+status.error:'');
 }
})JS";
 std::ofstream(folder/"star.js")<<R"JS(import {session} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={collected:false};}
 onTriggerEnter(event){if(!this.state.collected&&event.other?.body2D?.kind==='capsule'){this.state.collected=true;this.entity.renderVisible=false;session.set('stars',(session.get('stars')??0)+1);}}
})JS";
 UIDocument hud;if(!LoadUIDocument((folder/"hud.judasui").string(),hud,error))throw std::runtime_error(error);
 UIElement scrim;scrim.id="help-background";scrim.parent="canvas";scrim.kind=UIKind::Panel;scrim.anchorMax={1,0};scrim.size={0,100};scrim.background={.02f,.035f,.05f,.91f};hud.elements.insert(hud.elements.begin()+1,scrim);
 if(!SaveUIDocument((folder/"hud.judasui").string(),hud,error))throw std::runtime_error(error);
 AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());AssetRecord image,palette,controller,star;
 if(!assets.Track((folder/"tiles.png").string(),image,error)||!assets.Track((folder/"workshop.js").string(),controller,error)||!assets.Track((folder/"star.js").string(),star,error))throw std::runtime_error(error);
 SpriteSheet sheet;sheet.texture=image.id;sheet.width=sheet.height=128;sheet.pixelsPerUnit=32;
 const char* names[]={"Grass edge","Earth","Planks","Stone","Water","Roof","Shrub","Path","Crate","Tree","Star","Meadow","Brick","Door","Steel","Night stone"};
 for(int i=0;i<16;++i)sheet.frames.push_back({"tile-"+std::to_string(i),names[i],{(i%4)*32,(i/4)*32,32,32},{.5,.5}});
 if(!SaveSpriteSheet((folder/"tiles.judassprite").string(),sheet,error)||!assets.Track((folder/"tiles.judassprite").string(),palette,error))throw std::runtime_error(error);
 auto generate=[&](bool top){Scene scene=base;scene.Settings().name=top?"Tilemap Village":"Tilemap Trail";
  if(top){std::vector<SceneObjectId> fields;for(auto& o:scene.Objects()){if(o.gravity)fields.push_back(o.id);if(o.body&&o.body->motion==SceneBodyMotion::Dynamic){o.transform.position={0,-4,0};o.scripts[0].properties="{\"sideView\":false}";}}for(auto id:fields)scene.DestroyObject(id);}
  auto owner=scene.CreateObject(top?"Village grid - select and open Tilemap painter":"Trail grid - select and open Tilemap painter").id;
  TilemapRecipe r;r.root=owner;r.sheet=palette.id;r.layers={{"ground","Ground",0,-5,!top},{"scenery","Scenery",-1,-10,false},{"props","Props",0,1,false}};
  auto paint=[&](const char* layer,int x,int y,int bx,int by,int frame){if(!PaintTilemap(r,layer,{x,y},{bx,by},"tile-"+std::to_string(frame),false,false,false,error))throw std::runtime_error(error);};
  if(!top){
   paint("ground",-14,-2,36,-1,1);paint("ground",-14,-1,36,-1,0);
   for(int x=1;x<=4;++x)paint("ground",x,0,x,x/2,3);
   paint("ground",11,1,15,1,2);paint("ground",22,3,27,3,14);
   paint("ground",-14,0,-14,4,3);paint("ground",36,0,36,6,3);
   for(int x:{-11,-5,17,31}){paint("scenery",x,0,x,1,9);paint("scenery",x-1,2,x+1,2,6);}
   paint("scenery",20,0,25,2,12);paint("scenery",19,3,26,3,5);paint("props",20,0,20,1,13);
  }else{
   paint("scenery",-12,-7,12,7,11);paint("props",-11,-1,11,0,7);paint("props",-1,-6,0,6,7);
   paint("ground",-12,-7,12,-7,3);paint("ground",-12,7,12,7,3);paint("ground",-12,-6,-12,6,3);paint("ground",12,-6,12,6,3);
   for(int start:{-9,4}){paint("ground",start,2,start+4,2,12);paint("ground",start,3,start,5,12);paint("ground",start+4,3,start+4,5,12);paint("ground",start,6,start+4,6,5);
    if(!PaintTilemap(r,"ground",{start+2,2},{start+2,2},"",true,false,false,error))throw std::runtime_error(error);}
   for(int x:{-8,-5,5,9})paint("props",x,-5,x,-4,9);
  }
  Scene compiled;TilemapRecipe baked;if(!BakeTilemap(scene,r,assets,compiled,baked,error))throw std::runtime_error(error);scene=std::move(compiled);
  for(int i=0;i<3;++i){auto& o=scene.CreateObject("Collectible star");o.transform.position=top?glm::vec3(-7+i*7,-3,0):glm::vec3(-3+i*12,2+i%2,0);o.render=SceneRenderComponent{};o.render->shape=SceneShape::Sprite;o.render->color={1,1,1};o.render->textureAsset=image.id;o.render->sprite.size={.6,.6};o.render->sprite.rect={.5,.5,.25,.25};o.render->sprite.sortingLayer=2;
   o.body=SceneBodyComponent{};o.body->shape=SceneShape::Planar;o.body->planar=PlanarCircle(.3);o.body->sensor=true;o.scripts.push_back({1,star.id,true,"{}"});}
  for(int i=0;i<2;++i){auto& o=scene.CreateObject("Pushable crate");o.transform.position={float(-4+i*2),top?-3.f:1.f,0};o.render=SceneRenderComponent{};o.render->shape=SceneShape::Sprite;o.render->color={1,1,1};o.render->textureAsset=image.id;o.render->sprite.rect={0,.5,.25,.25};o.render->sprite.size={.8,.8};o.render->sprite.sortingLayer=1;
   o.body=SceneBodyComponent{};o.body->shape=SceneShape::Planar;o.body->planar=PlanarBox({.4,.4});o.body->motion=SceneBodyMotion::Dynamic;o.body->mass=2;}
  auto& control=scene.CreateObject("Workshop controls - ordinary project JavaScript");control.scripts.push_back({1,controller.id,true,"{}"});
  if(!SaveSceneToFile(scene,(root/(top?"Scenes/village.judas":"Scenes/main.judas")).string(),error))throw std::runtime_error(error);
 };
 generate(false);generate(true);
 for(auto [name,key]:{std::pair<const char*,const char*>{"side_view","key:1"},{"top_view","key:2"},{"restart","key:R"},{"save_game","key:F6"},{"load_game","key:F7"}}){project.Settings().input.Add(name,false);project.Settings().input.AddBinding(name,{key});}
 project.Settings().startupScene="Scenes/main.judas";if(!project.Save(error))throw std::runtime_error(error);
 std::ofstream(root/"README.md")<<"# Tilemap Workshop\n\nEditable native 2D tile authoring, compiled to ordinary sprites and MK2 colliders.\n\nWASD/arrows, Space to jump; 1 Trail, 2 Village, R restart; F6/F7 modern save/load. Collect stars and push crates.\n\nIn the editor: View > 2D authoring > Tilemap painter. Select a grid, choose a palette tile and enable Paint in Scene view. Left drag draws, right drag erases; rectangle tools fill/erase. Release commits one undoable stroke; Escape cancels.\n\nPaint source scenes; prefabs retain compiled objects. 2,048 cells, 8 layers, full-cell static collision. No animated tiles, slopes or one-way platforms yet. Palette edits require explicit Rebuild from palette. Original art generation: tools/demos/generate_tile_palette.py.\n";
 std::cout<<project.ProjectFile()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
