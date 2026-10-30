// Reproducible editable content authoring, never a runtime engine mode.
#include "ProjectStarter.h"
#include "Project.h"
#include "Scene.h"
#include "SceneSerialization.h"
#include "AssetDatabase.h"
#include "RuntimeUI.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <glm/gtc/quaternion.hpp>
#include "Prefab.h"
namespace fs=std::filesystem;
SceneObjectId Solid(Scene& scene,const std::string& name,glm::vec2 p,std::shared_ptr<const PlanarCollider> geometry,
                    glm::vec2 size,glm::vec3 color,SceneBodyMotion motion=SceneBodyMotion::Static) {
    auto& o=scene.CreateObject(name);o.transform.position={p,0};o.body=SceneBodyComponent{};
    o.body->shape=SceneShape::Planar;o.body->planar=std::move(geometry);o.body->motion=motion;o.body->mass=2;o.body->managed=false;
    o.render=SceneRenderComponent{};o.render->shape=SceneShape::Sprite;o.render->sprite.size=size;o.render->color=color;
    return o.id;
}
int main(int argc,char** argv){try{
    if(argc!=2)return 2;
    const auto root=fs::absolute(argv[1]);
    if(fs::exists(root))throw std::runtime_error("Output must be a fresh directory");
    std::string error;Project project;Scene scene;
    if(!Project::CreateNew(root.string(),"Planar Playground",project,error)||!PopulatePlanarProjectStarter(project,scene,true,error))throw std::runtime_error(error);
    const auto assetsPath=fs::path(project.AssetsDir());
    std::ofstream(assetsPath/"platform.js")<<R"JS(export const properties={spin:{type:'boolean',default:false}};
export default class {
 constructor({entity,properties}){this.entity=entity;this.properties=properties;this.state={time:0};}
 fixedUpdate(dt){
   this.state.time+=dt;const q=this.entity.transform.rotation,n=this.entity.body2D.normal;
   const right={x:1-2*(q.y*q.y+q.z*q.z),y:2*(q.x*q.y+q.w*q.z),z:2*(q.x*q.z-q.w*q.y)};
   const speed=this.properties.spin?0:1.2*Math.cos(this.state.time*.7),spin=this.properties.spin ? .14*Math.cos(this.state.time*.6):0;
   this.entity.setKinematicVelocity({x:right.x*speed,y:right.y*speed,z:right.z*speed},{x:n.x*spin,y:n.y*spin,z:n.z*spin});
 }
})JS";
    std::ofstream(assetsPath/"playground.js")<<R"JS(import {input,scenes,saves,ui} from 'judas';
export default class {
 constructor(){this.state={visits:0,request:null};}
 update(){
   if(input.pressed('platformer'))scenes.load('Scenes/main.judas');
   if(input.pressed('rotated'))scenes.load('Scenes/rotated.judas');
   if(input.pressed('topdown'))scenes.load('Scenes/topdown.judas');
   if(input.pressed('restart'))scenes.reload();
   if(input.pressed('save_game'))this.state.request=saves.save('playground');
   if(input.pressed('load_game'))this.state.request=saves.load('playground');
   const doc=ui.get('sprite_help');
   if(doc){const status=this.state.request?saves.status(this.state.request):null;
     doc.get('help').text='PLANAR PLAYGROUND | WASD/arrows | Space: jump | 1: platformer | 2: top-down | 3: tilted plane | R: restart\nF6/F7: save/load | Green beacon visits: '+this.state.visits+(status?' | '+status.state+' '+status.error:'');}
 }
 onTriggerEnter(event){if(event.other?.body2D?.kind==='capsule'&&event.other.body2D.lockRotation)this.state.visits++;}
})JS";
    AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());AssetRecord platform,game;
    if(!assets.Track((assetsPath/"platform.js").string(),platform,error)||!assets.Track((assetsPath/"playground.js").string(),game,error))throw std::runtime_error(error);
    for(auto& o:scene.Objects()){
        if(o.name=="Floor"){o.transform.position={9,-1,0};o.body->planar=PlanarBox({24,.5});o.render->sprite.size={48,1};}
        if(o.name=="Right wall")o.transform.position.x=33;
    }
    const glm::vec3 blue{.25,.65,.85},gold{.9,.65,.25};
    for(int i=0;i<3;++i)Solid(scene,"Pushable crate",{float(-3+i),.2f},PlanarBox({.35,.35}),{.7,.7},gold,SceneBodyMotion::Dynamic);
    const auto lift=Solid(scene,"Moving platform - physical support",{11,1},PlanarBox({1.4,.2}),{2.8,.4},blue,SceneBodyMotion::Kinematic);
    scene.Find(lift)->scripts.push_back({1,platform.id,true,"{}"});
    const auto rocker=Solid(scene,"Rotating support",{17,1.6},PlanarBox({2,.2}),{4,.4},blue,SceneBodyMotion::Kinematic);
    scene.Find(rocker)->scripts.push_back({1,platform.id,true,"{\"spin\":true}"});
    auto ramp=Solid(scene,"Sloped physical ramp",{22,0},PlanarConvex({{-2,-.5},{2,-.5},{2,1.5}}),{4,2},blue);
    // Visual shape is intentionally independent; debug collider shows its slope.
    scene.Find(ramp)->render.reset();
    const glm::vec2 corners[]={{-2,-.5},{2,-.5},{2,1.5}};
    for(int i=0;i<3;++i){const auto a=corners[i],b=corners[(i+1)%3];auto& edge=scene.CreateObject("Ramp outline (visual)");edge.parent=ramp;
        edge.transform.position={.5f*(a+b),0};edge.transform.rotation=glm::angleAxis(std::atan2(b.y-a.y,b.x-a.x),glm::vec3(0,0,1));
        edge.render=SceneRenderComponent{};edge.render->shape=SceneShape::Sprite;edge.render->sprite.size={glm::length(b-a),.06f};edge.render->color=blue;}
    const auto rotor=Solid(scene,"Pinned physical rotor",{27,1.8},PlanarBox({1.5,.12}),{3,.24},gold,SceneBodyMotion::Dynamic);
    auto& pin=scene.CreateObject("2D pin joint (world anchor)");pin.transform.position={27,1.8,0};pin.joint=SceneJointComponent{};
    pin.joint->bodyA=rotor;pin.joint->settings.type=JointType::PlanarPin;
    const auto beacon=Solid(scene,"Goal sensor",{30,1},PlanarBox({.6,1.5}),{1.2,3},{.2,.95,.5});
    scene.Find(beacon)->body->sensor=true;scene.Find(beacon)->render->alpha=.35f;scene.Find(beacon)->scripts.push_back({1,game.id,true,"{}"});
    auto& policy=project.Settings().input;
    for(const auto& binding:std::vector<std::pair<std::string,std::string>>{{"platformer","key:1"},{"topdown","key:2"},{"rotated","key:3"},{"restart","key:R"},{"save_game","key:F6"},{"load_game","key:F7"}}){policy.Add(binding.first,false);policy.AddBinding(binding.first,{binding.second});}
    project.Settings().startupScene="Scenes/main.judas";
    if(!project.Save(error)||!SaveSceneToFile(scene,project.StartupScenePath(),error))throw std::runtime_error(error);
    Scene top=scene;top.Settings().name="Planar Playground - Top-down";
    for(auto& o:top.Objects()){
        o.gravity.reset();if(o.body&&o.body->planar->lockRotation&&!o.scripts.empty())o.scripts.front().properties="{\"sideView\":false}";
        // No machinery controller outside the platformer test scene.
        if(o.body&&o.body->motion==SceneBodyMotion::Kinematic){o.body->motion=SceneBodyMotion::Static;o.scripts.clear();}
    }
    Solid(top,"Top boundary",{9,6},PlanarBox({24,.3}),{48,.6},blue);
    if(!SaveSceneToFile(top,(root/"Scenes/topdown.judas").string(),error))throw std::runtime_error(error);
    Scene tilted;if(!FlattenHierarchy(scene,tilted,error))throw std::runtime_error(error);
    const auto rotation=glm::angleAxis(.63f,glm::normalize(glm::vec3(1,2,3)));
    for(auto& o:tilted.Objects()){o.parent=0;o.transform.position=rotation*o.transform.position;o.transform.rotation=rotation*o.transform.rotation;
        if(o.body){o.body->initialLinearVelocity=rotation*o.body->initialLinearVelocity;o.body->initialAngularVelocity=rotation*o.body->initialAngularVelocity;}}
    if(!SaveSceneToFile(tilted,(root/"Scenes/rotated.judas").string(),error))throw std::runtime_error(error);
    std::cout<<project.ProjectFile()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
