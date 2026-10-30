#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "ScriptSystem.h"
#include "InputSystem.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
namespace fs=std::filesystem;
int main(int argc,char** argv){unsigned checks=0;try{
 if(argc!=3)return 2;
 std::string error;auto check=[&](bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(std::string(text)+": "+error);std::cout<<"PASS "<<text<<'\n';};
 const auto root=fs::absolute(argv[2]);check(!fs::exists(root),"fresh isolated helper fixture");fs::create_directories(root/"Assets");
 for(const auto* name:{"melee-2d.js","parallax-2d.js"})fs::copy_file(fs::path(argv[1])/name,root/"Assets"/name);
 std::ofstream(root/"Assets/probe.js")<<R"JS(import {swordHits} from './melee-2d.js';
export default class {constructor({entity}){this.entity=entity;this.state={};}
 fixedUpdate(){this.state.stab=swordHits(this.entity,{x:1,y:0,z:0}).map(h=>h.entity.id);
 this.state.swipe=swordHits(this.entity,{x:1,y:0,z:0},{arc:Math.PI/2,samples:17}).map(h=>h.entity.id);
 this.state.invalid=false;try{swordHits(this.entity,{x:0,y:0,z:1});}catch(e){this.state.invalid=true;}}
})JS";
 AssetDatabase db;db.Scan(root.string(),(root/"Assets").string());AssetRecord probe,parallax,melee;
 check(db.Track((root/"Assets/probe.js").string(),probe,error)&&db.Track((root/"Assets/parallax-2d.js").string(),parallax,error)&&db.Track((root/"Assets/melee-2d.js").string(),melee,error),"register real relative-import scripts");
 ResourceManager resources(nullptr,&db);Scene scene;
 auto add=[&](const char* name,glm::vec3 position,float radius){auto& o=scene.CreateObject(name);o.transform.position=position;o.body=SceneBodyComponent{};o.body->shape=SceneShape::Planar;o.body->planar=PlanarCircle(radius);return o.id;};
 auto owner=add("sword owner",{0,0,0},.2f),ahead=add("front target",{1,0,0},.2f),side=add("side target",{.8f,.7f,0},.2f);
 scene.Find(owner)->scripts.push_back({1,probe.id,true,"{}"});
 auto background=scene.CreateObject("parallax backdrop").id;scene.Find(background)->render=SceneRenderComponent{};scene.Find(background)->render->shape=SceneShape::Sprite;
 std::string props="{\"factorX\":0.25,\"factorY\":0.5}";check(ScriptSystem::SetPropertyEntity(props,"target",owner),"assign ordinary inspector-style target reference");scene.Find(background)->scripts.push_back({1,parallax.id,true,props});
 auto observe=[&](RuntimeWorld& w){InputSystem input;w.UpdateScripts(&input,.016f);w.FixedScripts(&input,1.f/60);for(const auto& d:w.Scripts()->Diagnostics())throw std::runtime_error(d.message);
  for(const auto& s:w.Scripts()->Capture()){if(s.entity==owner)return nlohmann::json::parse(s.json);}
  throw std::runtime_error("probe absent");};
 RuntimeWorld world;world.legacyGameplay=false;check(world.Build(scene,&resources,error),"build actual planar targets and visual parallax");auto state=observe(world);
 check(state["stab"].size()==1&&state["stab"][0]==std::to_string(ahead),"thrust really hits front body without hitting side body");
 const auto hits=state["swipe"].get<std::vector<std::string>>();
 check(std::set<std::string>(hits.begin(),hits.end())==std::set<std::string>{std::to_string(ahead),std::to_string(side)}&&state["invalid"]==true,"bounded swipe reaches both physical targets once; invalid plane rejects");
 auto pose=*world.RuntimeDefinition(owner);pose.transform.position={4,2,0};check(world.SetRuntimeTransform(owner,pose.transform),"move authoritative follow target");InputSystem input;world.PresentationScripts(&input,.016f,1);
 const auto p=world.RuntimeDefinition(background)->transform.position;std::printf("PARALLAX %.6f %.6f\n",p.x,p.y);check(glm::length(p-glm::vec3(3,1,0))<1e-6,"parallax uses interpolated target and declared independent axis factors");world.PresentationScripts(&input,.016f,1);
 check(world.RuntimeDefinition(background)->transform.position==p,"repeated presentation does not accumulate drift");world.Destroy();
 auto wall=scene.CreateObject("solid occluder").id;scene.Find(wall)->transform.position={.5,0,0};scene.Find(wall)->body=SceneBodyComponent{};scene.Find(wall)->body->shape=SceneShape::Planar;scene.Find(wall)->body->planar=PlanarBox({.1,2});
 check(world.Build(scene,&resources,error),"build independent wall-occlusion control");state=observe(world);
 check(state["stab"].size()==1&&state["stab"][0]==std::to_string(wall),"thrust hits intervening wall rather than target behind it");
 check(state["swipe"].size()==1&&state["swipe"][0]==std::to_string(wall),"swipe rays respect occlusion and deduplicate the wall");
 std::cout<<"TWO_D_HELPERS checks="<<checks<<" failures=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
