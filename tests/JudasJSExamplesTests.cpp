// Current cookbook examples, independent of historical game applications.
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "Simulation.h"
#include "Project.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs = std::filesystem;
int main(int argc, char** argv) {
    unsigned checks=0;
    auto check=[&](bool okay,const std::string& message){++checks;if(!okay)throw std::runtime_error(message);};
    try {
        const fs::path output=fs::absolute(argc>1?argv[1]:".cache/cookbook-current");
        fs::create_directories(output);
        std::string error;
        EngineHost host;
        check(host.Init("Current JudasJS cookbook",640,360,false,error),error);
        for(const char* name:{"minimal","profiling","queries","impulse-point","presentation","surface"}) {
            const auto root=output/(std::string(name)+"-"+MintAssetId());
            Project project;
            check(Project::CreateNew(root.string(),name,project,error),error);
            fs::copy_file((fs::path(__FILE__).parent_path().parent_path()/"docs/judasjs/examples")/(std::string(name)+".js"),root/"Assets/example.js");
            host.OpenProjectAssets(root.string(),(root/"Assets").string());
            host.Resources().SetBlockingMode(true);
            AssetRecord script;
            check(host.Assets().Track((root/"Assets/example.js").string(),script,error),error);
            Scene scene;
            auto& floor=scene.CreateObject("Query floor");
            floor.transform.position={0,-.5f,0};floor.body=SceneBodyComponent{};
            floor.body->halfExtents={8,.5f,8};
            auto& actor=scene.CreateObject("Script owner");
            const auto actorId=actor.id;
            actor.transform.position={0,1,0};actor.body=SceneBodyComponent{};
            actor.body->motion=SceneBodyMotion::Dynamic;actor.body->mass=2;
            actor.scripts.push_back({1,script.id,true,"{}"});
            RuntimeWorld world;
            world.ConfigureProject(project.Settings());
            check(world.Build(scene,&host.Resources(),error),error);
            for(unsigned frame=0;frame<100;++frame) {
                auto& input=host.GetWindow().Input();input.BeginFixedStep();
                world.UpdateScripts(&input,1.f/60);
                StepPlayedWorld(world,host.GetWindow(),1.f/60);
                world.PresentationScripts(&input,1.f/60,.5f);
            }
            check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),std::string(name)+": callback faults");
            std::string state;
            for(const auto& row:world.Scripts()->Capture())if(row.entity==actorId)state=row.json;
            check(!state.empty(),std::string(name)+": serializable state");
            std::ofstream(output/(std::string(name)+".json"))<<state;
            auto yes=[&](const char* key){return state.find(std::string("\"")+key+"\":true")!=std::string::npos;};
            const std::string n=name;
            if(n=="minimal")check(state.find("\"started\":1")!=std::string::npos&&state.find("\"steps\":100")!=std::string::npos&&state.find("\"frames\":100")!=std::string::npos,"actual render/fixed lifecycle counts");
            if(n=="profiling")check(yes("error")&&yes("invalid")&&state.find("\"calls\":102")!=std::string::npos,"exception-safe profiling callbacks and counters");
            if(n=="queries")check(yes("ray")&&yes("shape")&&yes("closest")&&yes("collider"),"real MK2 query results in copyable example");
            if(n=="impulse-point")check(yes("angular")&&yes("linear")&&yes("rejectsInvalid")&&yes("unchanged"),"off-centre impulse and atomic invalid-input rejection");
            if(n=="presentation")check(yes("synchronized")&&yes("frameMode"),"presentation timing and interpolation context");
            if(n=="surface")check(state.find("Entity.destroy")!=std::string::npos,"actual VM surface enumeration");
            world.Destroy();
        }
        std::cout<<checks<<" current cookbook checks passed\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
