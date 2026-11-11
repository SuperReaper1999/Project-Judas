#include "ProjectAuthoring.h"
#include "AssetDatabase.h"
#include "ModelArchive.h"
#include "Project.h"
#include "RuntimeUI.h"
#include "../../third_party/nlohmann/json.hpp"
#include <fstream>
#include <stdexcept>
using Json=nlohmann::json;
void AuthorCrashProject(const std::filesystem::path& root,Scene& scene,const std::vector<CrashRig>& rigs,
                        const std::array<MeshData,4>& wheels,const std::array<MeshData,4>& brakes,AssetDatabase& assets){
    std::string error;Project project;
    if(!Project::CreateNew(root.string(),"Deformables V2 Native Crash Project",project,error))throw std::runtime_error(error);
    scene.Settings().name="Deformables V2 Native Crash Project";
    scene.Settings().linearRendering=true;scene.Settings().backgroundColor={.15f,.19f,.24f};
    // Preserve the caller's authored lighting/environment. The normal runtime
    // owns shadows and transparent passes; source fidelity needs the same scene.
    for(auto& object:scene.Objects())if(object.body&&!object.render){
        object.render=SceneRenderComponent{};object.render->halfExtents=object.body->halfExtents;
        object.render->color=object.name=="Ground"?glm::vec3(.25f,.28f,.29f):glm::vec3(.65f,.68f,.68f);
    }
    auto track=[&](const std::filesystem::path& path){AssetRecord record;if(!assets.Track(path.string(),record,error))throw std::runtime_error(error);return record.id;};
    auto model=[&](const MeshData& mesh,const std::string& name){
        if(mesh.indices.empty())return AssetId{};
        std::vector<uint8_t> bytes;if(!EncodeModelArchive(mesh,bytes,error))throw std::runtime_error(error);
        auto path=root/"Assets"/(name+".judasmodel");std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));out.close();if(!out)throw std::runtime_error("cannot write private wheel derivative");return track(path);
    };
    Json layout=Json::array();
    for(const auto& rig:rigs){Json row={{"owner",std::to_string(rig.owner)},{"reference",rig.referencePart},{"wheels",Json::array()},{"brakes",Json::array()}};
        for(unsigned i=0;i<4;++i){auto* wheel=scene.Find(rig.wheels[i]);wheel->render=SceneRenderComponent{};wheel->render->shape=SceneShape::Mesh;wheel->render->meshAsset=model(wheels[i],"wheel-"+std::to_string(rig.owner)+"-"+std::to_string(i));wheel->render->color={1,1,1};
            row["wheels"].push_back(std::to_string(wheel->id));
            if(auto id=model(brakes[i],"caliper-"+std::to_string(rig.owner)+"-"+std::to_string(i));!id.empty()){
                auto& caliper=scene.CreateObject("Physical wheel caliper");caliper.render=SceneRenderComponent{};caliper.render->shape=SceneShape::Mesh;caliper.render->meshAsset=id;caliper.render->color={1,1,1};row["brakes"].push_back(std::to_string(caliper.id));
            }else row["brakes"].push_back(nullptr);
        }
        row["panels"]=Json::array();
        for(const auto& object:scene.Objects())if(object.deformable && !object.deformable->asset.empty())
            for(const auto& mount:object.deformable->attachments)if(mount.target==rig.owner){row["panels"].push_back(std::to_string(object.id));break;}
        layout.push_back(std::move(row));
    }
    auto scriptPath=root/"Assets/crash-project.js";
    std::ofstream script(scriptPath);script<<"import {world,input,saves,scenes,ui} from 'judas';\nconst rigs="<<layout.dump()<<";\n";
    const auto templatePath=std::filesystem::path(__FILE__).parent_path()/"DrivingProject.js";
    std::ifstream driving(templatePath);if(!driving)throw std::runtime_error("cannot read crash project controls");
    script<<driving.rdbuf();script.close();if(!script)throw std::runtime_error("cannot write project-owned crash controls");
    auto scriptId=track(scriptPath);
    const auto repository=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    auto font=root/"Assets/DejaVuSans.ttf";std::filesystem::copy_file(repository/"assets/fonts/DejaVuSans.ttf",font);std::filesystem::copy_file(repository/"assets/fonts/DejaVuSans-LICENSE.txt",root/"Assets/FONT-LICENSE.txt");
    auto fontId=track(font);
    UIDocument hud;UIElement canvas;canvas.id="canvas";canvas.kind=UIKind::Canvas;canvas.anchorMax={1,1};canvas.size={0,0};hud.elements.push_back(canvas);
    UIElement label;label.id="telemetry";label.parent="canvas";label.kind=UIKind::Text;label.font=fontId;label.fontSize=18;label.offset={20,20};label.size={1240,170};label.background={.02,.03,.04,.85};label.wrap=true;label.text="Preparing physical structure";hud.elements.push_back(label);
    auto hudPath=root/"Assets/crash-project.judasui";if(!SaveUIDocument(hudPath.string(),hud,error))throw std::runtime_error(error);
    auto& controller=scene.CreateObject("Project-owned driving, camera and saves");controller.scripts.push_back({1,scriptId,true,"{}"});controller.ui=SceneUIComponent{track(hudPath),"crash-project",true};
    project.Settings().startupScene="crash-proof.judas";
    project.Settings().input.entries={{"drive",true,{{"key:W",1},{"key:S",-1}}},{"steer",true,{{"key:A",1},{"key:D",-1}}},{"brake",false,{{"key:Space"}}},{"save",false,{{"key:K"}}},{"load",false,{{"key:L"}}},{"reset",false,{{"key:R"}}},{"pause",false,{{"key:Escape"}}}};
    project.Settings().input.entries.push_back({"orbit",false,{{"mouse:Right"}}});
    project.Settings().input.entries.push_back({"look_x",true,{{"mouse:dx"}}});
    project.Settings().input.entries.push_back({"look_y",true,{{"mouse:dy"}}});
    project.Settings().input.entries.push_back({"zoom",true,{{"mouse:wheelY"}}});
    project.Settings().input.entries.push_back({"behind",false,{{"key:C"}}});
    project.Settings().input.entries.push_back({"launch",false,{{"key:Return"}}});
    project.Settings().input.entries.push_back({"speed",false,{{"key:X"}}});
    for(unsigned n=0;n<rigs.size();++n)project.Settings().input.entries.push_back({"select_"+std::to_string(n+1),false,{{"key:"+std::to_string(n+1)}}});
    if(!project.Save(error))throw std::runtime_error(error);
    std::ofstream(root/"README-NATIVE-PROJECT.txt")<<"Private local models: do not redistribute. Open the .judasproj in Judas Editor, then Play. W/S drive, A/D steer, Space brake, K/L save/load, R restart, Esc pause. Edit ordinary structural/panel components; physics, rendering and persistence are native. Hold right mouse to orbit, mouse wheel to zoom, C behind car. Enter launches using one physical impulse; X cycles launch speed. Number keys select the actual authored vehicles shown by the HUD. Do not regenerate over an edited project.\n";
}
