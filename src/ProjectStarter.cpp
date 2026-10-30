#include "ProjectStarter.h"
#include "Project.h"
#include "Scene.h"
#include "AssetDatabase.h"
#include "EnginePaths.h"
#include "RuntimeUI.h"
#include <filesystem>
#include <fstream>
#include <algorithm>

bool PopulateVisibleProjectStarter(Project& project,Scene& scene,std::string& error) {
    const auto source=ResolveEngineDataPath("docs/judasjs/examples/first-project.js");
    std::ifstream input(source,std::ios::binary);
    if(!input){error="Visible starter script is missing: "+source+". Keep the SDK docs with the editor, or choose Empty scene.";return false;}
    const std::string bytes{std::istreambuf_iterator<char>(input),{}};
    const auto destination=std::filesystem::u8path(project.AssetsDir())/"game.js";
    if(std::filesystem::exists(destination)){error="Starter would overwrite "+destination.u8string();return false;}
    std::ofstream output(destination,std::ios::binary);output.write(bytes.data(),bytes.size());output.close();
    if(!output){error="Cannot write starter script: "+destination.u8string();return false;}
    AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());AssetRecord record;
    if(!assets.Track(destination.u8string(),record,error))return false;
    auto& object=scene.CreateObject("My rotating box - edit Assets/game.js");
    object.transform.position={0,2,0};object.render=SceneRenderComponent{};
    object.render->color={.2f,.7f,.85f};object.scripts.push_back({1,record.id,true,"{}"});
    error.clear();return true;
}

bool Populate2DProjectStarter(Project& project,Scene& scene,bool sideView,std::string& error) {
    namespace fs=std::filesystem;
    const auto assetsRoot=fs::u8path(project.AssetsDir());
    for(const auto& file:std::vector<std::pair<std::string,std::string>>{
            {"sprite-starter.js","game.js"},{"2d/explorer.png","explorer.png"}}) {
        const auto source=fs::u8path(ResolveEngineDataPath("docs/judasjs/examples/"+file.first));
        const auto destination=assetsRoot/file.second;
        std::error_code ec;
        if(!fs::copy_file(source,destination,fs::copy_options::none,ec)) {
            error="Cannot create 2D starter asset: "+destination.u8string()+": "+ec.message();return false;
        }
    }
    UIDocument hud;hud.reference={1280,720};
    UIElement canvas;canvas.id="canvas";canvas.kind=UIKind::Canvas;canvas.anchorMax={1,1};hud.elements.push_back(canvas);
    UIElement help;help.id="help";help.parent="canvas";help.kind=UIKind::Text;
    help.offset={24,20};help.size={1100,58};help.fontSize=20;help.wrap=true;
    help.text=sideView?"2D SIDE VIEW | WASD / arrows: move | Stage 1: no collision or jumping yet":"2D TOP-DOWN | WASD / arrows: explore | Edit Assets/game.js";
    hud.elements.push_back(help);
    if(!SaveUIDocument((assetsRoot/"hud.judasui").u8string(),hud,error))return false;
    AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());
    AssetRecord image,script,ui;
    if(!assets.Track((assetsRoot/"explorer.png").u8string(),image,error)||
       !assets.Track((assetsRoot/"game.js").u8string(),script,error)||
       !assets.Track((assetsRoot/"hud.judasui").u8string(),ui,error))return false;
    scene=Scene{};scene.Settings().backgroundColor={.055f,.085f,.11f};scene.Settings().sunEnabled=false;
    auto& player=scene.CreateObject("Explorer - edit Assets/game.js");
    player.render=SceneRenderComponent{};player.render->shape=SceneShape::Sprite;player.render->textureAsset=image.id;
    player.render->color={1,1,1};player.render->sprite.size={1,1.25f};player.render->sprite.pivot={.5f,.9f};
    player.render->sprite.sortingLayer=1;player.render->sprite.ySort=true;
    player.scripts.push_back({1,script.id,true,sideView?"{\"sideView\":true}":"{}"});
    const auto backdrop=scene.CreateObject("Backdrop - editable sprite tiles").id;
    // Background geometry is ordinary, editable sprites; it owns no physics.
    for(int y=-5;y<=5;++y)for(int x=-8;x<=8;++x) {
        auto& tile=scene.CreateObject("Ground tile");tile.parent=backdrop;tile.transform.position={float(x),float(y),-.2f};
        tile.render=SceneRenderComponent{};tile.render->shape=SceneShape::Sprite;
        tile.render->sprite.size={1,1};tile.render->sprite.sortingLayer=-10;
        tile.render->color=(x+y)%2?glm::vec3(.13f,.25f,.23f):glm::vec3(.15f,.28f,.25f);
    }
    auto& overlay=scene.CreateObject("Controls HUD");overlay.ui=SceneUIComponent{ui.id,"sprite_help",true};
    // The template owns its controls; don't expose legacy flight/debug bindings
    // in a brand-new 2D project's settings or optional pause-menu example.
    auto& bindings=project.Settings().input.entries;
    bindings.erase(std::remove_if(bindings.begin(),bindings.end(),[](const auto& entry){
        return entry.name!="move_x"&&entry.name!="move_y"&&entry.name!="pause"&&entry.name.rfind("ui_",0)!=0;
    }),bindings.end());
    project.Settings().editor2D=true;
    error.clear();return true;
}

// Stage 2 templates keep gameplay policy in a normal, editable project script.
// The Stage 1 sprite-only helper remains available for visual-only workflows.
bool PopulatePlanarProjectStarter(Project& project,Scene& scene,bool sideView,std::string& error) {
    if(!Populate2DProjectStarter(project,scene,sideView,error))return false;
    std::ifstream source(ResolveEngineDataPath("docs/judasjs/examples/planar-player.js"),std::ios::binary);
    if(!source){error="Missing native 2D player template in SDK docs";return false;}
    std::ofstream script(std::filesystem::u8path(project.AssetsDir())/"game.js",std::ios::binary|std::ios::trunc);
    script<<source.rdbuf();script.close();if(!script){error="Cannot write 2D player script";return false;}
    for(auto& object:scene.Objects())if(!object.scripts.empty()) {
        object.name=sideView?"Platformer player - editable game.js":"Top-down player - editable game.js";
        object.transform.position={0,2,0};
        object.render->sprite.pivot={.5f,.5f};object.render->sprite.size={.7f,1.3f};
        object.body=SceneBodyComponent{};object.body->shape=SceneShape::Planar;
        object.body->motion=SceneBodyMotion::Dynamic;object.body->mass=5;
        object.body->planar=PlanarCapsule(.3,.3,true);object.body->managed=false;
        object.body->friction=0;object.scripts.front().properties=sideView?"{\"sideView\":true}":"{\"sideView\":false}";
    }
    // Matching real collision, not a screen boundary or a scripted clamp.
    auto solid=[&](const char* name,glm::vec2 center,glm::vec2 half){
        auto& o=scene.CreateObject(name);o.transform.position={center,0};
        o.render=SceneRenderComponent{};o.render->shape=SceneShape::Sprite;
        o.render->sprite.size=half*2.f;o.render->color={.3f,.5f,.52f};
        o.body=SceneBodyComponent{};o.body->shape=SceneShape::Planar;o.body->planar=PlanarBox(half);
    };
    solid("Floor",{0,-1},{12,.5});solid("Left wall",{-12,3},{.5,4});solid("Right wall",{12,3},{.5,4});
    if(sideView){solid("Jump platform",{3,.7f},{1.4f,.25f});solid("Upper platform",{7,2.5f},{1.4f,.25f});
        auto& gravity=scene.CreateObject("2D world gravity");gravity.gravity=SceneGravityComponent{};
        gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->regionShape=SceneRegionShape::Box;
        gravity.gravity->regionHalfExtents={100,100,10};}
    else solid("Interior obstacle",{3,2},{.7f,1.5f});
    auto& bindings=project.Settings().input;
    if(sideView&&!bindings.Find("jump")){bindings.Add("jump",false);bindings.AddBinding("jump",{"key:Space"});bindings.AddBinding("jump",{"pad:South"});}
    // Update only the just-created help asset, before it is ever user-authored.
    UIDocument hud;const auto path=(std::filesystem::u8path(project.AssetsDir())/"hud.judasui").u8string();
    if(!LoadUIDocument(path,hud,error))return false;
    for(auto& element:hud.elements)if(element.id=="help")element.text=sideView?"PLATFORMER | A/D or arrows: move | Space: jump | Edit Assets/game.js":"TOP-DOWN | WASD / arrows: move | Native 2D collision | Edit Assets/game.js";
    return SaveUIDocument(path,hud,error);
}
