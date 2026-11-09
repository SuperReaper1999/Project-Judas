#include "editor/EditorVehicleCreation.h"
#include "editor/EditorDocument.h"
#include "Project.h"
#include "ProjectStarter.h"
#include "ScriptSystem.h"
#include "SceneFingerprint.h"
#include "Prefab.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
int main(){unsigned checks=0;auto check=[&](bool value,const std::string& message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';};try{
 const auto root=std::filesystem::temp_directory_path()/("judas-vehicle-authoring-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Project project;std::string error;check(Project::CreateNew(root.string(),"Driving",project,error),"create fresh developer project: "+error);
 EditorDocument document;check(PopulateVisibleProjectStarter(project,document.GetScene(),error),"populate ordinary project starter: "+error);
 AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());
 const auto baseline=document.GetScene();
 const auto car=CreateEditorVehicle(document,project,assets,"vehicle-car",{4,0,2},0,error);
 check(car&&document.Selected()==car,"Create car selects a complete assembly: "+error);
 check(document.GetScene().Find(car)->powertrain&&document.GetScene().Find(car)->scripts.size()==1,"project-owned driver and native powertrain are attached");
 unsigned disabled=0;for(const auto& object:document.GetScene().Objects())for(const auto& slot:object.scripts)disabled+=!slot.enabled;
 check(disabled==1,"exact starter camera is superseded without guessing at user scripts");
 check(ScriptSystem::CheckProjectSources(assets).empty(),"packaged driver imports and compiles against actual JudasJS");
 document.Undo();check(ScenesEqual(document.GetScene(),baseline),"one Undo removes complete assembly and restores starter camera");
 document.Redo();check(document.GetScene().Find(car)&&document.GetScene().Find(car)->scripts.size()==1,"Redo restores vehicle and driver");
 Project loaded;check(loaded.Load(project.ProjectFile(),error)&&loaded.Settings().input.Find("vehicle_throttle")&&loaded.Settings().input.Find("vehicle_brake"),"driving input settings survive project reopen: "+error);
 Scene prefab;check(CreatePrefab(document.GetScene(),car,prefab,error),"complete vehicle and driver are prefab-authorable: "+error);
 const auto parent=document.GetScene().CreateObject("Parent").id;
 for(const auto* kind:{"vehicle-truck","vehicle-bus","vehicle-trailer"}){
   const auto id=CreateEditorVehicle(document,project,assets,kind,{0,0,0},parent,error);
   check(id&&document.GetScene().Find(id)->parent==parent,"preset creates actual child assembly: "+error);
   if(std::string(kind)=="vehicle-trailer"){
     unsigned wheels=0;for(const auto& object:document.GetScene().Objects())if(object.wheel&&object.wheel->chassis==id)++wheels;
     check(wheels==2&&!document.GetScene().Find(id)->powertrain,"unpowered trailer preset uses a freely following single axle, not a fixed car wheelbase");
   }
 }
 document.GetScene().Find(parent)->transform.scale={2,1,1};const auto before=document.GetScene();
 check(!CreateEditorVehicle(document,project,assets,"vehicle-car",{},parent,error)&&ScenesEqual(before,document.GetScene()),"scaled parent rejects without changing scene");
 project.Settings().input.Find("vehicle_throttle")->axis=false;
 check(!CreateEditorVehicle(document,project,assets,"vehicle-car",{},0,error)&&ScenesEqual(before,document.GetScene())&&error.find("vehicle_throttle")!=std::string::npos,"conflicting logical input types reject atomically with an actionable message");
 std::filesystem::remove_all(root);std::cout<<"SUMMARY "<<checks<<" checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
