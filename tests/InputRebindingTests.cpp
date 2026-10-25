#include "InputSystem.h"
#include "Project.h"
#include "AssetDatabase.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(){
    namespace fs=std::filesystem;unsigned checks=0;
    auto check=[&](bool okay,const char* label){++checks;if(!okay)throw std::runtime_error(label);std::cout<<"PASS "<<label<<'\n';};
    try{
        InputSystem input;std::string error;const auto configured=input.Map().Serialize();
        auto changed=input.Map();changed.Find("jump")->bindings[0].control="key:K";
        check(input.RequestBindings(&changed,error)&&input.Map().Serialize()==configured,"rebind queues without changing the current snapshot");
        input.BeginFrame();check(input.Map().Find("jump")->bindings[0].control=="key:K","next input frame publishes the real map");
        input.SetPhysical("key:K",1);check(input.Action("jump").held,"rebound physical key drives the existing logical action");
        input.SetPhysical("key:K",0);input.SetPhysical("key:Space",1);check(!input.Action("jump").held,"previous binding no longer drives the action");
        auto invalid=changed;invalid.entries.pop_back();check(!input.RequestBindings(&invalid,error),"rebind cannot remove project actions");
        invalid=changed;invalid.Find("jump")->axis=true;check(!input.RequestBindings(&invalid,error),"rebind cannot change logical action type");
        invalid=changed;invalid.Find("jump")->bindings[0].control="bad";check(!input.RequestBindings(&invalid,error),"invalid binding is rejected transactionally");
        check(input.RequestBindings(nullptr,error),"restore project-default bindings queues");input.BeginFrame();
        check(input.Map().Serialize()==configured&&!input.Action("jump").held,"reset restores exact original map without replaying held inputs");
        input.SetLogicalMode(true);check(!input.RequestBindings(&changed,error),"external logical-source owner cannot be bypassed");input.SetLogicalMode(false);
        const auto root=fs::temp_directory_path()/("judas-rebind-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Project project;check(Project::CreateNew(root.string(),"Rebind",project,error),"ordinary project fixture");
        const auto script=fs::path(project.AssetsDir())/"rebind.js";
        std::ofstream(script)<<R"JS(import {input} from 'judas';
export default class {
 constructor(){this.state={detached:false,queued:false,negative:0};}
 start(){
  const map=input.getBindings();map.find(x=>x.name==='jump').bindings[0].control='key:K';
  this.state.detached=input.getBindings().find(x=>x.name==='jump').bindings[0].control==='key:Space';
  this.state.queued=input.setBindings(map);
  for(const bad of [[],undefined,[...map,{name:'extra',axis:false,bindings:[]}],map.map(x=>x.name==='jump'?{...x,axis:true}:x)]){
   try{input.setBindings(bad);}catch(e){if(e instanceof TypeError)this.state.negative++;}
  }
 }
 update(){if(input.pressed('reset'))input.setBindings(null);}
})JS";
        AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());AssetRecord record;check(assets.Track(script.string(),record,error),"register actual public-API script");
        ResourceManager resources(nullptr,&assets);RuntimeWorld world;world.legacyGameplay=false;Scene scene;
        auto& object=scene.CreateObject("Rebind observer");object.scripts.push_back({1,record.id,true,"{}"});
        check(world.Build(scene,&resources,error),"script builds through ordinary runtime");world.UpdateScripts(&input,.016f);
        auto state=nlohmann::json::parse(world.Scripts()->Capture().front().json);
        check(world.Scripts()->Diagnostics().empty()&&state["detached"]==true&&state["queued"]==true,"real JudasJS binding returns detached map and queues replacement");
        check(state["negative"]==4,"real native API rejects removed controls, undefined, added actions and changed types");
        check(input.Map().Serialize()==configured,"invalid JS requests do not publish or cancel the valid pending map");
        input.BeginFrame();check(input.Map().Find("jump")->bindings[0].control=="key:K","native JS request publishes the intended physical binding");
        input.SetPhysical("key:R",1);world.UpdateScripts(&input,.016f);input.BeginFrame();
        check(input.Map().Serialize()==configured,"native JS null reset restores authored defaults");
        check(project.Settings().input.Serialize()==configured,"runtime rebinding never edits authored project settings");
        world.Destroy();fs::remove_all(root);
        std::cout<<"Input rebinding: "<<checks<<" checks, 0 failures\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
