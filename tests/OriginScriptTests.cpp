#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "ScriptSystem.h"
#include "LocalMultiplayer.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <cstdio>

namespace {
unsigned checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
}
int main(){
 namespace fs=std::filesystem;const auto root=fs::absolute(".cache/m90/script-fixture");fs::remove_all(root);fs::create_directories(root/"Assets");
 std::ofstream(root/"Assets/origin.js")<<R"JS(import {world} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={calls:0,cache:{x:2,y:3,z:4},unrelated:512};}
 onOriginShift(e){
  this.state.calls++;this.state.published=e.revision===world.originState.revision&&e.origin.x===world.origin.x;
  for(const k of ['x','y','z'])this.state.cache[k]-=e.delta[k];
  this.state.pose=this.entity.transform.position;
  try{this.entity.character.velocity={x:1,y:0,z:0};this.state.guarded=false;}catch{this.state.guarded=true;}
 }
})JS";
 std::ofstream(root/"Assets/fault.js")<<"export default class {onOriginShift(){throw Error('isolated origin failure');}}";
 AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());AssetRecord good,bad;std::string error;
 Check(assets.Track((root/"Assets/origin.js").string(),good,error)&&assets.Track((root/"Assets/fault.js").string(),bad,error),"ordinary origin callback assets register");
 Scene scene;auto& a=scene.CreateObject("motor script");a.characterMotor=CharacterMotorSettings{};a.transform.position={2,3,4};a.scripts.push_back({1,good.id,true,"{}"});
 auto& b=scene.CreateObject("fault isolation");b.scripts.push_back({1,bad.id,true,"{}"});
 ResourceManager resources(nullptr,&assets);RuntimeWorld world;world.legacyGameplay=false;
 Check(world.Build(scene,&resources,error),"ordinary VM world builds");if(failures)return 1;
 auto* player=world.Players().Create("independent view",{},error);SceneTransform view;view.position={5,6,7};world.Players().SetView(player->id,view,70,.1f,500,error);
 // Unstarted instances cannot receive callbacks before their lifecycle begins.
 Check(world.RequestOrigin({512,0,0},error)&&world.CommitOrigin(),"unstarted world translates safely");world.UpdateScripts(nullptr,1.f/60);
 auto state=[&](){for(auto& record:world.Scripts()->Capture())if(record.entity==1)return nlohmann::json::parse(record.json);return nlohmann::json();};
 Check(state()["calls"]==0,"origin hook never precedes start/restore");
 Check(world.RequestOrigin({1024,-512,512},error)&&world.CommitOrigin(),"started scripts receive committed chart notification");
 auto s=state();Check(s["calls"]==1&&s["published"]==true,"notification observes fully published origin and revision");
 Check(s["cache"]["x"]==-510&&s["cache"]["y"]==515&&s["cache"]["z"]==-508&&s["unrelated"]==512,"explicit script cache translation leaves arbitrary JSON untouched");
 Check(s["pose"]["x"]==-1022&&s["guarded"]==true,"entity pose already translated and fixed-only writes remain guarded");
 Check(world.Scripts()->Diagnostics().size()==1&&world.OriginRevision()==2,"one faulty notification cannot undo native chart or healthy scripts");
 Check(glm::length(player->view->position-glm::vec3(-1019,518,-505))<.0001f,"independent local-player view translates without changing identity/viewport");
 const auto revision=world.OriginRevision();Check(!world.RequestOrigin({50000,0,0},error)&&world.OriginRevision()==revision&&state()["calls"]==1,"rejected transaction emits no callback");
 Check(world.RequestOrigin({1024,-512,512},error)&&world.CommitOrigin()&&state()["calls"]==1,"zero translation emits no duplicate notification");
 Check(world.DestroyEntity(1)&&world.RequestOrigin({512,0,0},error)&&world.CommitOrigin(),"destroyed/faulted slots do not block subsequent native chart publication");
 std::printf("SUMMARY %u checks %u failures\n",checks,failures);return failures?1:0;
}
