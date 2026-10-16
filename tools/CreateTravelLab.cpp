#include "Project.h"
#include "AssetDatabase.h"
#include "SceneSerialization.h"
#include "NamedAuthoring.h"
#include "NavigationAsset.h"
#include "PlanetTerrain.h"
#include "RuntimeUI.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <glm/gtc/quaternion.hpp>

// Ordinary reproducible project content. No native demo flag or travel policy.
namespace fs=std::filesystem;
const std::string wheelId="90909090909090909090909000000001",carId="90909090909090909090909000000002",actorId="90909090909090909090909000000003",scriptId="90909090909090909090909000000004",uiId="90909090909090909090909000000005",worldId="90909090909090909090909000000006",navId="90909090909090909090909000000007";
int main(){
 const auto root=fs::absolute("projects/travelling_lab");std::string error;Project project;
 if(fs::exists(root/"travelling_lab.judasproj")){if(!project.Load((root/"travelling_lab.judasproj").string(),error)){std::cerr<<error;return 1;}}
 else {if(!Project::CreateNew(root.string(),"travelling_lab",project,error)){std::cerr<<error;return 1;}project.SaveAs((root/"travelling_lab.judasproj").string(),error);}
 for(auto path:{"Assets/models","Assets/prefabs","Assets/scripts","Assets/ui","Assets/navigation","Assets/worlds","Scenes"})fs::create_directories(root/path);
 auto meta=[&](std::string path,std::string id,std::string kind){std::ofstream(root/(path+".judasmeta"))<<"JudasAssetMeta 1\nid \""<<id<<"\"\ntype "<<kind<<"\nsource \"\"\n";};
 auto save=[&](const Scene& scene,std::string path){std::string bytes;return SaveSceneToString(scene,bytes)&&WriteAuthoredDocument((root/path).string(),bytes,"scene",error,true);};
 fs::copy_file("projects/wheel_lab/Assets/models/wheel.obj",root/"Assets/models/wheel.obj",fs::copy_options::overwrite_existing);meta("Assets/models/wheel.obj",wheelId,"mesh");
 Scene car;if(!LoadSceneFromFile("projects/powertrain_lab/Assets/prefabs/buggy.judasprefab",car,error)){std::cerr<<error;return 1;}
 for(auto& o:car.Objects()){if(o.wheel){o.render->meshAsset=wheelId;o.wheel->radius=.38f;}if(o.powertrain){auto& source=o.powertrain->sources[0];source.kind=PowertrainSourceKind::Electric;source.maxTorque=140;source.maxPower=30000;source.starterTorque=0;source.torqueCurve.clear();o.powertrain->shafts[0].initialAngularVelocity=0;}}
 car.Find(1)->name="Electric expedition rover / physical gearbox and differential";
 if(!save(car,"Assets/prefabs/rover.judasprefab")){std::cerr<<error;return 1;}meta("Assets/prefabs/rover.judasprefab",carId,"prefab");
 Scene actor;auto& character=actor.CreateObject("Independent script-driven explorer");character.characterMotor=CharacterMotorSettings{};character.render=SceneRenderComponent{};character.render->halfExtents={.25f,.7f,.25f};character.render->color={.12f,.8f,.8f};
 if(!save(actor,"Assets/prefabs/explorer.judasprefab")){std::cerr<<error;return 1;}meta("Assets/prefabs/explorer.judasprefab",actorId,"prefab");
 meta("Assets/scripts/travel.js",scriptId,"script");meta("Assets/scripts/math.js","90909090909090909090909000000008","script");
 auto& settings=project.Settings();settings.name="Bring Everything Along";settings.legacyGameplay=false;settings.startupScene="Scenes/earth.judas";settings.worldManifest=worldId;settings.input.entries.clear();settings.saveIdentity="9090909090909090909090909090909090909090909090909090909090909090";
 auto action=[&](const char* name,const char* key){settings.input.entries.push_back({name,false,{{std::string("key:")+key}}});};
 for(auto p:{std::pair{"forward","W"},{"backward","S"},{"left","A"},{"right","D"},{"brake","Space"},{"use","E"},{"watch","T"},{"navigate","N"},{"interact","F"},{"pause","Escape"},{"save","F6"},{"load","F7"},{"reload","R"}})action(p.first,p.second);
 settings.input.entries.push_back({"look_x",true,{{"mouse:dx"}}});settings.input.entries.push_back({"look_y",true,{{"mouse:dy"}}});
 settings.classification.tags.names[0]="Courier";settings.classification.tags.names[1]="Cargo";settings.classification.tags.nextId=2;
 Scene station;auto box=[&](glm::vec3 position,glm::vec3 half,glm::vec3 color,bool dynamic=false){auto& o=station.CreateObject(dynamic?"Movable service cargo":"Waystation structure");o.transform.position=position;o.render=SceneRenderComponent{};o.render->halfExtents=half;o.render->color=color;o.body=SceneBodyComponent{};o.body->halfExtents=half;o.body->motion=dynamic?SceneBodyMotion::Dynamic:SceneBodyMotion::Static;o.body->mass=dynamic?8:1;if(dynamic)o.tags=CategoryBit(1);return o.id;};
 box({0,-.4f,0},{14,.4f,12},{.32f,.36f,.39f});
 for(float x:{-9.f,9.f}){box({x,2,5},{.15f,2,.15f},{.9f,.62f,.16f});box({x,2,-5},{.15f,2,.15f},{.9f,.62f,.16f});}
 box({0,4,0},{9.4f,.16f,5.4f},{.15f,.23f,.3f});box({-6,.55f,2},{.6f,.55f,.6f},{.8f,.45f,.2f},true);box({6,.55f,-2},{.6f,.55f,.6f},{.8f,.45f,.2f},true);
 auto& bot=station.CreateObject("Navigation courier / N changes destination");bot.transform.position={-6,.95f,-3};bot.characterMotor=CharacterMotorSettings{};bot.navigationAgent=NavigationAgentSettings{};bot.tags=1;bot.render=SceneRenderComponent{};bot.render->halfExtents={.3f,.7f,.3f};bot.render->color={.1f,.8f,.8f};
 auto& surface=station.CreateObject("Local-frame navigation surface");surface.navigationSurface=NavigationSurfaceSettings{};surface.navigationSurface->halfExtents={16,5,14};surface.navigationSurface->minRegion=1;const auto surfaceId=surface.id;
 NavigationData data;if(!BakeNavigation(station,surface,settings.navigation,data,error)){std::cerr<<error;return 1;}
 if(!SaveNavigation((root/"Assets/navigation/station.judasnav").string(),data,error)){std::cerr<<error;return 1;}meta("Assets/navigation/station.judasnav",navId,"navigation");station.Find(surfaceId)->navigationSurface->asset=navId;
 if(!save(station,"Scenes/station.judas")){std::cerr<<error;return 1;}
 PlanetSettings planet;planet.bands={{{.08},{.018,0,.024}},{{1.5},{.0007,0,.0011}}};planet.collisionRadius=18;planet.maxCollisionTiles=128;planet.collisionSpacing=.5;planet.maxTiles=256;planet.maxResident=512;PlanetSurface earth(planet);
 Scene scene;scene.Settings().name="M90 — Bring Everything Along";scene.Settings().worldOrigin={0,6371000,0};scene.Settings().sunDirection={.4f,.8f,.25f};scene.Settings().ambientColor={.27f,.3f,.33f};
 auto& globe=scene.CreateObject("Earth-sized precision-safe terrain");globe.planet=planet;
 auto& field=scene.CreateObject("Ordinary RadicalGravity");field.transform.position={0,-6371000,0};field.gravity=SceneGravityComponent{};field.gravity->regionRadius=2e7f;
 auto& driver=scene.CreateObject("Project travel, input and camera policy");driver.scripts.push_back({1,scriptId,true,"{}"});driver.ui=SceneUIComponent{uiId,"travel",true};driver.audioListener=SceneAudioListenerComponent{true,true};
 if(!save(scene,"Scenes/earth.judas")){std::cerr<<error;return 1;}
 std::ofstream manifest(root/"Assets/worlds/route.judasworld");manifest<<std::setprecision(17)<<"JudasWorld 1\n";
 for(unsigned i=0;i<12;++i){double distance=double(i)*1024.;auto direction=glm::normalize(glm::dvec3(0,1,-std::tan(distance/planet.radius)));auto p=earth.Point(direction)+direction*.08;auto q=glm::normalize(glm::quat(float(1+direction.y),float(direction.z),0,float(-direction.x)));manifest<<"region \"station"<<i<<"\" \"Scenes/station.judas\" "<<p.x<<' '<<p.y<<' '<<p.z<<' '<<q.x<<' '<<q.y<<' '<<q.z<<' '<<q.w<<" 30 10 30 0 \"snapshot\" 262144 \"\"\n";}
 manifest.close();meta("Assets/worlds/route.judasworld",worldId,"world");
 UIDocument hud;UIElement panel;panel.id="canvas";panel.kind=UIKind::Canvas;hud.elements.push_back(panel);UIElement title;title.id="title";title.parent="canvas";title.kind=UIKind::Text;title.text="BRING EVERYTHING ALONG";title.offset={24,18};title.size={850,40};title.fontSize=28;hud.elements.push_back(title);title.id="controls";title.offset={24,58};title.fontSize=17;title.text="WASD drive / walk  |  E enter / leave  |  Space brake / jump  |  T watch drive  |  N courier  |  F push cargo\nMouse look  |  F6 / F7 save / load  |  Escape pause  |  R reload";title.size={1040,62};hud.elements.push_back(title);title.id="status";title.offset={24,130};title.fontSize=18;title.size={1020,160};title.text="Preparing real terrain collision...";hud.elements.push_back(title);title.id="pause";title.offset={400,320};title.fontSize=28;title.text="PAUSED — Escape resumes";title.visible=false;hud.elements.push_back(title);
 if(!SaveUIDocument((root/"Assets/ui/travel.judasui").string(),hud,error)){std::cerr<<error;return 1;}meta("Assets/ui/travel.judasui",uiId,"ui");
 if(!project.Save(error)){std::cerr<<error;return 1;}std::cout<<project.ProjectFile()<<'\n';
}
