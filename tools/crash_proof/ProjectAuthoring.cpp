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
    scene.Settings().sunDirection={-.4f,1,.3f};scene.Settings().sunColor={2,1.9f,1.8f};scene.Settings().ambientColor={.7f,.75f,.8f};
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
        }layout.push_back(std::move(row));
    }
    auto scriptPath=root/"Assets/crash-project.js";
    std::ofstream script(scriptPath);script<<"import {world,input,saves,scenes,ui} from 'judas';\nconst rigs="<<layout.dump()<<";\n";
    script<<R"JS(
const rotate=(q,v)=>{const x=2*(q.y*v.z-q.z*v.y),y=2*(q.z*v.x-q.x*v.z),z=2*(q.x*v.y-q.y*v.x);return {x:v.x+q.w*x+q.y*z-q.z*y,y:v.y+q.w*y+q.z*x-q.x*z,z:v.z+q.w*z+q.x*y-q.y*x};};
const multiply=(a,b)=>({w:a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,x:a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,y:a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,z:a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w});
export default class {
 constructor(){this.state={selected:0};this.request=null;}
 uiUpdate(){
  for(let n=0;n<rigs.length;n++)if(input.pressed('select_'+(n+1)))this.state.selected=n;
  if(input.pressed('reset'))scenes.reload();
  if(!saves.busy){if(input.pressed('save'))this.request=saves.save('crash-project');if(input.pressed('load'))this.request=saves.load('crash-project');}
  const hud=ui.get('crash-project');if(!hud)return;if(input.pressed('pause'))hud.modal=!hud.modal;
  const damage=world.entity(rigs[this.state.selected].owner).fracture?.state;
  const status=this.request?saves.status(this.request):null;
  hud.get('telemetry').text='DEFORMABLES V2 | Native editable crash project\nW/S drive | A/D steer | Space brake | 1-4 vehicle | K/L save/load | R restart | Esc pause\n'+(damage?'Broken physical connections: '+damage.interfaces.filter(i=>i.broken).length+' / '+damage.interfaces.length:'Preparing physical structure')+(status?' | '+status.state+' '+(status.error||''):'');
 }
 fixedUpdate(){
  for(let n=0;n<rigs.length;n++){const rig=rigs[n],active=n===this.state.selected,owner=world.entity(rig.owner),power=owner.powertrain;
   if(power)power.set({sources:[{name:'motor',input:active?input.axis('drive'):0}],brakes:power.settings.brakes.map(b=>({name:b.name,engagement:active&&input.held('brake')?1:0}))});
   for(let w=0;w<rig.wheels.length;w++){const entity=world.entity(rig.wheels[w]),wheel=entity.wheel;if(!wheel)continue;wheel.set({steeringAngle:active&&w<2?input.axis('steer')*.4:0});
    if(rig.brakes[w]){const state=wheel.state,pose=world.entity(rig.brakes[w]).transform;pose.position=state.center;pose.rotation=multiply(state.rotation,{w:Math.cos(state.angle/2),x:Math.sin(state.angle/2),y:0,z:0});world.entity(rig.brakes[w]).transform=pose;}
   }
  }
 }
 presentationUpdate(){const rig=rigs[this.state.selected],parts=world.entity(rig.owner).fracture?.state.parts,reference=parts?.find(p=>p.index===rig.reference)?.entity;if(!reference)return;
  const pose=reference.presentedTransform,back=rotate(pose.rotation,{x:0,y:0,z:9});
  world.setView({position:{x:pose.position.x+back.x,y:pose.position.y+back.y+3,z:pose.position.z+back.z},rotation:multiply(pose.rotation,{w:Math.cos(-.16),x:Math.sin(-.16),y:0,z:0})},65,{near:.1,far:500});
 }
 destroy(){world.clearView();}
}
)JS";script.close();if(!script)throw std::runtime_error("cannot write project-owned crash controls");
    auto scriptId=track(scriptPath);
    const auto repository=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    auto font=root/"Assets/DejaVuSans.ttf";std::filesystem::copy_file(repository/"assets/fonts/DejaVuSans.ttf",font);std::filesystem::copy_file(repository/"assets/fonts/DejaVuSans-LICENSE.txt",root/"Assets/FONT-LICENSE.txt");
    auto fontId=track(font);
    UIDocument hud;UIElement canvas;canvas.id="canvas";canvas.kind=UIKind::Canvas;canvas.anchorMax={1,1};canvas.size={0,0};hud.elements.push_back(canvas);
    UIElement label;label.id="telemetry";label.parent="canvas";label.kind=UIKind::Text;label.font=fontId;label.fontSize=20;label.offset={20,20};label.size={1240,120};label.background={.02,.03,.04,.85};label.wrap=true;label.text="Preparing physical structure";hud.elements.push_back(label);
    auto hudPath=root/"Assets/crash-project.judasui";if(!SaveUIDocument(hudPath.string(),hud,error))throw std::runtime_error(error);
    auto& controller=scene.CreateObject("Project-owned driving, camera and saves");controller.scripts.push_back({1,scriptId,true,"{}"});controller.ui=SceneUIComponent{track(hudPath),"crash-project",true};
    project.Settings().startupScene="crash-proof.judas";
    project.Settings().input.entries={{"drive",true,{{"key:W",1},{"key:S",-1}}},{"steer",true,{{"key:A",1},{"key:D",-1}}},{"brake",false,{{"key:Space"}}},{"save",false,{{"key:K"}}},{"load",false,{{"key:L"}}},{"reset",false,{{"key:R"}}},{"pause",false,{{"key:Escape"}}}};
    for(unsigned n=0;n<rigs.size();++n)project.Settings().input.entries.push_back({"select_"+std::to_string(n+1),false,{{"key:"+std::to_string(n+1)}}});
    if(!project.Save(error))throw std::runtime_error(error);
    std::ofstream(root/"README-NATIVE-PROJECT.txt")<<"Private local models: do not redistribute. Open the .judasproj in Judas Editor, then Play. W/S drive, A/D steer, Space brake, K/L save/load, R restart, Esc pause. Edit ordinary structural/panel components; physics, rendering and persistence are native. The separate diagnostic tool's controlled-impact and orbit keys are not game bindings. Do not regenerate over an edited project.\n";
}
