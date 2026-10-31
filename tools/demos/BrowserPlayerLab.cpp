// Ordinary consumer scene: the same motor, MK2, UI and save APIs as desktop.
#include "Project.h"
#include "SceneSerialization.h"
#include "Scene.h"
#include "AssetDatabase.h"
#include "RuntimeUI.h"
#include <filesystem>
#include <iostream>
namespace fs=std::filesystem;
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("usage: judas_browser_player_lab repository fresh-project-directory");
    const auto source=fs::absolute(argv[1]),root=fs::absolute(argv[2]);
    if(fs::exists(root))throw std::runtime_error("Destination exists; preserve edited projects");
    Project project;Scene scene;std::string error;
    auto require=[&](bool ok){if(!ok)throw std::runtime_error(error);};
    require(Project::CreateNew(root.string(),"Browser Player Lab",project,error));
    const fs::path assetsDir(project.AssetsDir());
    fs::copy_file(source/"docs/judasjs/examples/first-person-player.js",assetsDir/"motor.js");
    fs::copy_file(source/"tools/demos/browser_player/player.js",assetsDir/"player.js");
    AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());
    auto track=[&](const char* name){AssetRecord r;require(assets.Track((assetsDir/name).string(),r,error));return r.id;};
    track("motor.js");const auto script=track("player.js");
    scene.Settings().name="Browser Player Lab";scene.Settings().backgroundColor={.1f,.17f,.25f};
    auto& gravity=scene.CreateObject("Uniform gravity");gravity.gravity=SceneGravityComponent{};
    gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->regionShape=SceneRegionShape::Box;
    gravity.gravity->regionHalfExtents={60,60,60};
    auto box=[&](const char* name,glm::vec3 at,glm::vec3 half,glm::vec3 color,bool dynamic=false){
        auto& o=scene.CreateObject(name);o.transform.position=at;
        o.render=SceneRenderComponent{};o.render->halfExtents=half;o.render->color=color;
        o.body=SceneBodyComponent{};o.body->halfExtents=half;
        o.body->motion=dynamic?SceneBodyMotion::Dynamic:SceneBodyMotion::Static;o.body->mass=4;
    };
    box("Courtyard floor",{0,-.5,0},{12,.5,12},{.32,.4,.4});
    box("Back wall",{0,2,-10},{12,2,.3},{.35,.46,.6});
    box("Left wall",{-10,2,0},{.3,2,10},{.25,.4,.55});
    for(int i=0;i<3;++i)box("Jump steps",{float(3+i),float(i)*.22f+.11f,-3},{.5f,float(i)*.22f+.11f,1},{.75,.48,.2});
    for(int i=0;i<5;++i)box("Pushable MK2 crate",{float(i)-2,1,-2},{.4,.4,.4},{.2f+.1f*i,.65f,.35f},true);
    auto& player=scene.CreateObject("Player - ordinary first person template");
    player.transform.position={0,1.05,6};player.characterMotor=CharacterMotorSettings{};
    player.render=SceneRenderComponent{};player.render->halfExtents={.3,.9,.3};player.render->color={.8,.5,.2};
    player.scripts.push_back({1,script,true,"{}"});
    UIDocument hud;
    UIElement canvas;canvas.id="canvas";canvas.kind=UIKind::Canvas;canvas.anchorMax={1,1};hud.elements.push_back(canvas);
    const glm::vec4 colors[]={{1,0,0,1},{0,1,0,1},{0,0,1,1},{1,1,0,1}};
    for(int i=0;i<4;++i){UIElement e;e.id="corner-"+std::to_string(i);e.parent="canvas";e.size={24,24};
        e.anchorMin=e.anchorMax={float(i%2),float(i/2)};e.align=e.anchorMin;e.background=colors[i];hud.elements.push_back(e);}
    UIElement help;help.id="help";help.parent="canvas";help.kind=UIKind::Text;help.offset={32,26};
    help.size={1050,105};help.fontSize=20;help.wrap=true;hud.elements.push_back(help);
    UIElement button;button.id="test-click";button.parent="canvas";button.kind=UIKind::Button;
    button.anchorMin=button.anchorMax={1,1};button.align={1,1};button.offset={-32,-32};button.size={210,48};
    button.background={.15,.55,.45,1};button.text="Test click";hud.elements.push_back(button);
    require(SaveUIDocument((assetsDir/"hud.judasui").string(),hud,error));const auto ui=track("hud.judasui");
    // The owner script receives its own document's UI events.
    scene.Find(player.id)->ui=SceneUIComponent{ui,"browser_player",true};
    auto& input=project.Settings().input;
    for(const auto& binding:{std::pair<const char*,const char*>{"save_game","key:K"},{"load_game","key:L"}}){
        input.Add(binding.first,false);input.AddBinding(binding.first,{binding.second});
    }
    project.Settings().startupScene="Scenes/main.judas";
    require(SaveSceneToFile(scene,project.StartupScenePath(),error));require(project.Save(error));
    std::cout<<project.ProjectFile()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
