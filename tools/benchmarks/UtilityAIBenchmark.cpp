#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "Project.h"
#include "ScriptSystem.h"
#include "InputSystem.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

// Measures the actual script bridge/scheduler, without physics, rendering,
// sensing queries, inspector polling or persistence serialization in the loop.
int main() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("judas-ai-bench-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"Assets");
    std::ofstream(root/"Assets/agent.js")<<R"JS(
import {UtilityAgent} from 'judas';
export const properties={ai:{type:'boolean',default:true}};
export default class {
 constructor({entity,properties}) {this.id=entity.id;this.enabled=properties.ai;this.state={};}
 start(){if(this.enabled)this.agent=new UtilityAgent({interval:.2,initialDelay:Number(this.id)%12/60,
 actions:[{id:'work',considerations:[{fact:'need'},{fact:'distance',curve:'inverse'}]},
 {id:'rest',considerations:[{fact:'tired'}]}]});}
 fixedUpdate(dt){if(this.agent)this.agent.tick(dt,()=>[
 {action:'work',target:'near',facts:{need:.8,distance:.2}},
 {action:'work',target:'far',facts:{need:.8,distance:.7}},
 {action:'rest',target:'bed',facts:{tired:.3}}]);}
}
)JS";
    AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());AssetRecord script;std::string error;
    if(!assets.Track((root/"Assets/agent.js").string(),script,error)){std::cerr<<error;return 1;}
    ResourceManager resources(nullptr,&assets);InputSystem input;
    std::cout<<"ai_enabled,agents,ticks,median_ms,p95_ms,worst_ms\n";
    for(bool ai:{false,true})for(int count:{0,100,500}) {
        Scene scene;
        for(int i=0;i<count;++i)scene.CreateObject("worker").scripts.push_back({1,script.id,true,ai?"{\"ai\":true}":"{\"ai\":false}"});
        RuntimeWorld world;ProjectSettings project;world.ConfigureProject(project);
        if(!world.Build(scene,&resources,error)){std::cerr<<error;return 1;}
        for(int i=0;i<12;++i)world.FixedScripts(&input,1.f/60);
        if(world.Scripts()&&!world.Scripts()->Diagnostics().empty())return 2;
        std::vector<double> times;
        for(int i=0;i<120;++i) {
            const auto began=std::chrono::steady_clock::now();world.FixedScripts(&input,1.f/60);
            times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count());
        }
        if(world.Scripts()&&!world.Scripts()->Diagnostics().empty())return 2;
        if(ai&&count&&world.Scripts()->AIInspections().size()!=std::size_t(count))return 3;
        std::sort(times.begin(),times.end());
        std::cout<<ai<<','<<count<<",120,"<<(times[59]+times[60])/2<<','<<times[113]<<','<<times.back()<<'\n';
        world.Destroy();
    }
    fs::remove_all(root);
}
