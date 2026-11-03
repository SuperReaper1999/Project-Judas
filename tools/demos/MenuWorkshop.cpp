#include "Project.h"
#include "AssetDatabase.h"
#include "SceneSerialization.h"
#include "RuntimeUI.h"
#include "ScriptSystem.h"
#include "../../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
int main(int argc,char** argv){try{
    if(argc!=3){std::cerr<<"usage: judas_menu_workshop authored-source-directory fresh-project-directory\n";return 2;}
    const auto source=fs::absolute(argv[1]),root=fs::absolute(argv[2]);
    if(fs::exists(root))throw std::runtime_error("Destination exists; preserve edited projects");
    Project project;Scene scene;std::string error;
    auto require=[&](bool ok){if(!ok)throw std::runtime_error(error);};
    require(Project::CreateNew(root.string(),"Satchel Menu Workshop",project,error));
    const fs::path dir(project.AssetsDir());
    for(const auto& entry:fs::directory_iterator(source))if(entry.is_regular_file()&&entry.path().extension()!=".py")fs::copy_file(entry.path(),dir/entry.path().filename());
    AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());
    auto track=[&](const std::string& file){AssetRecord record;require(assets.Track((dir/file).string(),record,error));return record.id;};
    const auto script=track("menu.js");track("items.js");
    std::map<std::string,std::string> icons;for(const char* name:{"sword","axe","shield","wood","apple","ore","potion","pack"})icons[name]=track(std::string(name)+".png");
    const auto palette=track("palette.png");
    scene.Settings().name="Satchel | generic UI transfers and camera preview";
    scene.Settings().mainCameraRenderMask=1;
    scene.Settings().ambientColor={.3f,.32f,.34f};
    unsigned layer=0;require(project.Settings().classification.render.Add("Menu preview",layer));
    auto model=[&](const char* name,const char* file,glm::vec3 at){auto& o=scene.CreateObject(name);o.renderLayer=layer;o.transform.position=at;o.render=SceneRenderComponent{};o.render->shape=SceneShape::Mesh;o.render->meshAsset=track(file);o.render->textureAsset=palette;o.render->color={1,1,1};return o.id;};
    const auto actor=model("Preview traveller - visual only","traveller.obj",{50,0,0});
    const auto blade=model("Equipped blade - visual only","blade.obj",{50,.83f,0});
    const auto axe=model("Equipped axe - visual only","held_axe.obj",{50,.83f,0});
    const auto shield=model("Equipped shield - visual only","held_shield.obj",{50,1,0});
    const auto pack=model("Equipped pack - visual only","held_pack.obj",{50,1,0});
    auto& camera=scene.CreateObject("Menu preview camera - 512 x 640, preview layer only");const auto cameraId=camera.id;
    camera.transform.position={50,1.02f,3.25f};camera.renderCamera=SceneRenderCameraComponent{};
    camera.renderCamera->renderMask=CategoryMask(1)<<layer;camera.renderCamera->width=512;camera.renderCamera->height=640;camera.renderCamera->verticalFovDegrees=36;camera.renderCamera->farPlane=12;
    auto& gravity=scene.CreateObject("Faithful gravity - normal camp");gravity.gravity=SceneGravityComponent{};gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->regionShape=SceneRegionShape::Box;gravity.gravity->regionHalfExtents={20,20,20};
    auto& floor=scene.CreateObject("Camp floor");floor.transform.position={0,-.2f,0};floor.render=SceneRenderComponent{};floor.render->halfExtents={12,.2f,12};floor.render->color={.13f,.17f,.15f};floor.body=SceneBodyComponent{};floor.body->halfExtents={12,.2f,12};
    auto& chest=scene.CreateObject("Nearby supply chest");chest.transform.position={1,.4f,0};chest.render=SceneRenderComponent{};chest.render->halfExtents={.7f,.4f,.4f};chest.render->color={.35f,.22f,.1f};chest.body=SceneBodyComponent{};chest.body->halfExtents={.7f,.4f,.4f};
    auto& ball=scene.CreateObject("Pause witness - real falling body");ball.transform.position={-1,3,0};ball.render=SceneRenderComponent{};ball.render->shape=SceneShape::Sphere;ball.render->radius=.3f;ball.render->color={.8f,.56f,.2f};ball.body=SceneBodyComponent{};ball.body->motion=SceneBodyMotion::Dynamic;ball.body->shape=SceneShape::Sphere;ball.body->radius=.3f;ball.body->mass=1;
    UIDocument doc;doc.reference={1600,900};doc.modal=true;
    auto add=[&](const std::string& id,UIKind kind,glm::vec2 offset,glm::vec2 size,const std::string& text="",const std::string& parent="menu")->UIElement&{UIElement e;e.id=id;e.parent=parent;e.kind=kind;e.offset=offset;e.size=size;e.text=text;e.fontSize=21;e.color={.89f,.85f,.7f,1};e.textAlign={.5f,.5f};if(kind==UIKind::Button)e.background={.13f,.15f,.14f,1};doc.elements.push_back(e);return doc.elements.back();};
    UIElement canvas;canvas.id="canvas";canvas.kind=UIKind::Canvas;doc.elements.push_back(canvas);UIElement menu;menu.id="menu";menu.parent="canvas";menu.size=doc.reference;menu.background={.015f,.025f,.025f,.86f};doc.elements.push_back(menu);
    auto panel=[&](const std::string& id,glm::vec2 at,glm::vec2 size,const std::string& parent="menu"){add(id+"_rim",UIKind::Panel,at,size,"",parent).background={.52f,.42f,.23f,1};add(id,UIKind::Panel,at+glm::vec2(2),size-glm::vec2(4),"",parent).background={.035f,.045f,.044f,.98f};};
    add("title",UIKind::Text,{30,12},{460,58},"THE WAYFARER'S SATCHEL").fontSize=27;
    add("tab_inventory",UIKind::Button,{520,18},{165,42},"Inventory");add("tab_character",UIKind::Button,{695,18},{165,42},"Character");add("tab_settings",UIKind::Button,{870,18},{165,42},"Settings");
    add("save",UIKind::Button,{1130,18},{120,42},"Save");add("load",UIKind::Button,{1260,18},{120,42},"Load");add("resume",UIKind::Button,{1390,18},{175,42},"Resume / Esc");
    add("inventory_group",UIKind::Panel,{0,0},{1600,900});
    panel("vicinity",{30,100},{330,575},"inventory_group");add("near_title",UIKind::Text,{45,114},{295,46},"NEARBY CHEST","inventory_group").fontSize=25;
    panel("bag_panel",{830,100},{735,645},"inventory_group");add("bag_title",UIKind::Text,{850,112},{340,42},"INVENTORY","inventory_group").fontSize=26;add("weight",UIKind::Text,{1260,112},{280,42},"0 / 35 kg","inventory_group");
    auto slot=[&](std::string key,glm::vec2 pos,glm::vec2 size,const std::string& parent){auto& e=add(key,UIKind::Button,pos,size,"",parent);e.dropType="item";e.textAlign={.5f,.96f};e.fontSize=17;e.focusColor={.96f,.76f,.32f,1};add(key+"_icon",UIKind::Image,{(size.x-72)/2,4},{72,size.y-32},"",key);};
    for(int i=0;i<16;++i)slot("bag"+std::to_string(i),{850.f+(i%4)*173,180.f+(i/4)*132},{164,121},"inventory_group");
    for(int i=0;i<4;++i)slot("near"+std::to_string(i),{55,180.f+i*115},{275,105},"inventory_group");
    add("preview",UIKind::Image,{405,115},{375,500});add("preview_label",UIKind::Text,{390,75},{400,40},"YOUR TRAVELLER");
    add("turn_left",UIKind::Button,{440,620},{130,40},"Rotate left");add("turn_right",UIKind::Button,{595,620},{140,40},"Rotate right");
    int index=0;for(const char* key:{"weapon","offhand","pack"}){slot(key,{410.f+index*125,700},{115,108},"menu");add(std::string(key)+"_caption",UIKind::Text,{410.f+index*125,670},{115,26},key);++index;}
    add("character_group",UIKind::Panel,{0,0},{1600,900}).visible=false;
    panel("character_panel",{830,100},{735,645},"character_group");add("character_info",UIKind::Text,{870,145},{650,500},"THE TRAVELLER\n\nEquipment changes the displayed weapon, shield and pack.\n\nThe preview is ordinary Judas geometry, rendered by an authored camera.\n\nNo second physics world runs behind this screen.\n\nInventory weight, item rules and healing belong to this project's JavaScript.","character_group").wrap=true;
    add("settings_group",UIKind::Panel,{0,0},{1600,900}).visible=false;
    panel("settings_panel",{830,100},{735,645},"settings_group");add("settings_info",UIKind::Text,{870,140},{650,220},"INTERFACE SETTINGS\n\nText size changes without moving inventory hit targets.\n\nMouse: drag, or click an item and choose Use / Equip.\nKeyboard/controller: focus an item, Activate to pick up, navigate, Activate to place; Back cancels.","settings_group").wrap=true;
    add("scale_small",UIKind::Button,{900,430},{250,55},"Smaller text","settings_group");add("scale_large",UIKind::Button,{1200,430},{250,55},"Larger text","settings_group");
    add("health",UIKind::Text,{40,710},{300,45},"Health 72 / 100");add("health_bar",UIKind::Slider,{40,765},{300,12});doc.elements.back().enabled=false;doc.elements.back().color={.49f,.65f,.33f,1};
    add("details",UIKind::Text,{40,810},{1510,32},"Select an item.");
    add("use",UIKind::Button,{850,755},{210,42},"Use / Equip");add("split",UIKind::Button,{1070,755},{210,42},"Split stack");add("nearby",UIKind::Button,{1290,755},{250,42},"Move to chest");
    add("status",UIKind::Text,{40,850},{1520,34},"Ready.").fontSize=17;
    add("hint",UIKind::Text,{30,20},{900,40},"Escape: open satchel | The camp simulates while the menu is closed.","canvas").visible=false;
    require(SaveUIDocument((dir/"satchel.judasui").string(),doc,error));const auto uiAsset=track("satchel.judasui");
    auto& controller=scene.CreateObject("Satchel menu - project-owned rules");controller.ui=SceneUIComponent{uiAsset,"satchel",true};controller.scripts.push_back({1,script,true,"{}"});auto& properties=controller.scripts[0].properties;
    for(const auto& p:{std::pair<const char*,SceneObjectId>{"previewCamera",cameraId},{"previewActor",actor},{"previewBlade",blade},{"previewAxe",axe},{"previewShield",shield},{"previewPack",pack}})require(ScriptSystem::SetPropertyEntity(properties,p.first,p.second));
    auto json=nlohmann::json::parse(properties);for(const auto& p:icons){json[p.first]=p.second;project.Settings().runtimeAssets.push_back(p.second);}properties=json.dump();
    project.Settings().startupScene="Scenes/main.judas";require(SaveSceneToFile(scene,project.StartupScenePath(),error));require(project.Save(error));std::cout<<project.ProjectFile()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
