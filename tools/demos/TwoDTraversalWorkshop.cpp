#include "Project.h"
#include "ProjectStarter.h"
#include "SceneSerialization.h"
#include "Scene.h"
#include "AssetDatabase.h"
#include "RuntimeUI.h"
#include "EnginePaths.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
int main(int argc,char** argv){try{
 if(argc!=2)return 2;
 const auto root=fs::absolute(argv[1]);if(fs::exists(root))throw std::runtime_error("destination must be fresh");Project project;Scene side,grid;std::string error;
 auto require=[&](bool ok){if(!ok)throw std::runtime_error(error);};require(Project::CreateNew(root.string(),"2D Traversal Workshop",project,error));require(PopulateGridProjectStarter(project,grid,error));const fs::path dir(project.AssetsDir());
 fs::copy_file(ResolveEngineDataPath("docs/judasjs/examples/planar-player.js"),dir/"platform-player.js");AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());AssetRecord platform;
 require(assets.Track((dir/"platform-player.js").string(),platform,error));side=grid;
 // Each experience owns its layout. Do not stack the free-movement starter's
 // obstacle beneath the authored blocked grid cells or cap the jump scene.
 for(auto* scene:{&side,&grid}) {
  std::vector<SceneObjectId> remove;
  for(const auto& o:scene->Objects())if(o.name=="Blocked grid cell"||o.name=="Interior obstacle"||(scene==&side&&o.name=="North grid boundary"))remove.push_back(o.id);
  for(auto id:remove)scene->DestroyObject(id);
 }
auto* actor=side.Find(1);actor->scripts[0].asset=platform.id;actor->scripts[0].properties="{\"sideView\":true}";actor->render->sprite.pivot={.5,.5};actor->body->motion=SceneBodyMotion::Dynamic;actor->body->planar=PlanarCapsule(.3,.3,true);actor->transform.position={0,.4,0};side.Settings().name="One-way platform workshop";
 for(auto at:{glm::vec2(3,.7f),glm::vec2(7,2.1f)}){auto& o=side.CreateObject("One-way platform - preview arrow in Body");o.transform.position={at,0};o.body=SceneBodyComponent{};o.body->shape=SceneShape::Planar;o.body->planar=PlanarChain({{-1.4,0},{1.4,0}},true);o.render=SceneRenderComponent{};o.render->shape=SceneShape::Sprite;o.render->sprite.size={2.8,.12};o.render->sprite.pivot={.5,0};o.render->color={.9,.65,.25};}
 auto& gravity=side.CreateObject("2D gravity");gravity.gravity=SceneGravityComponent{};gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->regionShape=SceneRegionShape::Box;gravity.gravity->regionHalfExtents={100,100,10};
 grid.Settings().name="Grid RPG workshop";
 for(auto at:{glm::vec2(2,2),glm::vec2(2,3),glm::vec2(-2,4)}){auto& o=grid.CreateObject("Blocked grid cell");o.transform.position={at,0};o.body=SceneBodyComponent{};o.body->shape=SceneShape::Planar;o.body->planar=PlanarBox({.45,.45});o.render=SceneRenderComponent{};o.render->shape=SceneShape::Sprite;o.render->sprite.size={.9,.9};o.render->color={.3,.55,.35};}
 std::ofstream(dir/"controls.js")<<R"JS(import {input,scenes,saves,ui} from 'judas';
export default class {constructor(){this.state={request:null};}
 uiUpdate(){if(!saves.busy){if(input.pressed('side_view'))scenes.load('Scenes/main.judas');else if(input.pressed('grid_view'))scenes.load('Scenes/grid.judas');else if(input.pressed('restart'))scenes.reload();else if(input.pressed('save_game'))this.state.request=saves.save('traversal');else if(input.pressed('load_game'))this.state.request=saves.load('traversal');}
 const doc=ui.get('sprite_help'),s=this.state.request?saves.status(this.state.request):null;if(doc)doc.get('help').text='2D TRAVERSAL | 1: one-way platforms | 2: grid RPG | R: reset\nWASD / arrows: move | Space: jump in platform scene | K: save | L: load\nOne-way: walk underneath, jump through and land above. Grid: tap a direction; walls block full cells.'+(s?'\nSave: '+s.state+' '+s.error:'');}
})JS";
 AssetRecord controls;assets.Scan(project.RootDir(),project.AssetsDir());require(assets.Track((dir/"controls.js").string(),controls,error));
 for(auto* scene:{&side,&grid}){auto& o=scene->CreateObject("Scene and save controls - project JavaScript");o.scripts.push_back({1,controls.id,true,"{}"});}
 UIDocument hud;require(LoadUIDocument((dir/"hud.judasui").string(),hud,error));for(auto& e:hud.elements)if(e.id=="help"){e.size.y=82;e.fontSize=16;}UIElement panel;panel.id="help-background";panel.parent="canvas";panel.kind=UIKind::Panel;panel.anchorMax={1,0};panel.size={0,100};panel.background={.015,.025,.035,.95};hud.elements.insert(hud.elements.begin()+1,panel);require(SaveUIDocument((dir/"hud.judasui").string(),hud,error));
 project.Settings().startupScene="Scenes/main.judas";require(SaveSceneToFile(side,project.StartupScenePath(),error));require(SaveSceneToFile(grid,(root/"Scenes/grid.judas").string(),error));
 for(auto [name,key]:{std::pair<const char*,const char*>{"jump","key:Space"},{"side_view","key:1"},{"grid_view","key:2"},{"restart","key:R"},{"save_game","key:F6"},{"load_game","key:F7"}}){project.Settings().input.Add(name,false);project.Settings().input.AddBinding(name,{key});}
 require(project.Settings().input.AddBinding("save_game",{"key:K"}));require(project.Settings().input.AddBinding("load_game",{"key:L"}));
 require(project.Save(error));std::cout<<project.ProjectFile()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
