#include "EditorVehicleCreation.h"
#include "EditorDocument.h"
#include "EditorPrimitiveAssets.h"
#include "EditorScriptWorkflow.h"
#include "Project.h"
#include "EnginePaths.h"
#include "authoring/VehicleAssembly.h"
#include <filesystem>
#include <fstream>

SceneObjectId CreateEditorVehicle(EditorDocument& document,Project& project,AssetDatabase& assets,
                                 const std::string& kind,glm::vec3 position,SceneObjectId parent,
                                 std::string& error) {
    namespace fs=std::filesystem;
    if(document.EditInProgress()||!project.IsLoaded()){
        error="Open a project and finish the current edit before creating a vehicle.";return 0;
    }
    if(kind!="vehicle-car"&&kind!="vehicle-truck"&&kind!="vehicle-bus"&&kind!="vehicle-trailer"){
        error="Unknown vehicle assembly preset.";return 0;
    }
    error.clear();
    if(kind!="vehicle-trailer")for(const auto* name:{"vehicle_throttle","vehicle_steer","vehicle_brake"}) {
        const auto* action=project.Settings().input.Find(name);
        const bool axis=std::string(name)!="vehicle_brake";
        if(action&&action->axis!=axis){error=std::string("Correct the input type for '")+name+"' in Project Settings before creating a powered vehicle.";return 0;}
    }
    for(auto id=parent;id;) {
        const auto* object=document.GetScene().Find(id);
        if(!object||object->transform.scale!=glm::vec3(1)){error="Create vehicles under unit-scale parents, or at the scene root.";return 0;}
        id=object->parent;
    }
    std::ifstream source(ResolveEngineDataPath("docs/judasjs/examples/vehicle-driver.js"));
    const std::string driver{std::istreambuf_iterator<char>(source),{}};
    if(driver.empty()){error="This SDK is missing docs/judasjs/examples/vehicle-driver.js.";return 0;}
    EditorDocument cylinder;
    const auto visual=CreateEditorPrimitive(cylinder,project,assets,"cylinder",{},0,error);
    if(!visual)return 0;
    JudasVehicles::AssemblyRecipe recipe;
    recipe.name=kind=="vehicle-car"?"Car":kind=="vehicle-truck"?"Truck":kind=="vehicle-bus"?"Bus":"Trailer";
    recipe.axles=kind=="vehicle-truck"?3:kind=="vehicle-bus"?4:kind=="vehicle-trailer"?1:2;
    recipe.mass=kind=="vehicle-car"?500:kind=="vehicle-trailer"?800:2000;
    recipe.powered=kind!="vehicle-trailer";recipe.parent=parent;
    recipe.pose.position=parent?glm::vec3(0,1.1f,0):position+glm::vec3(0,1.1f,0);
    recipe.wheelMesh=cylinder.GetScene().Find(visual)->render->meshAsset;
    recipe.drivenAxles={recipe.axles-1};
    auto candidate=document.GetScene();JudasVehicles::AssemblyResult created;
    if(!JudasVehicles::CreateAssembly(candidate,recipe,created,error))return 0;
    // Stage the entire document/history: a failed asset or project write must
    // not leave a half-created assembly in the live scene.
    auto staged=document;
    staged.BeginEdit();staged.GetScene()=std::move(candidate);
    const auto originalInput=project.Settings().input;
    if(recipe.powered) {
        auto& map=project.Settings().input;
        if(!map.Find("vehicle_throttle")){map.Add("vehicle_throttle",true);map.AddBinding("vehicle_throttle",{"key:W",1,0});map.AddBinding("vehicle_throttle",{"key:S",-1,0});}
        if(!map.Find("vehicle_steer")){map.Add("vehicle_steer",true);map.AddBinding("vehicle_steer",{"key:A",-1,0});map.AddBinding("vehicle_steer",{"key:D",1,0});}
        if(!map.Find("vehicle_brake")){map.Add("vehicle_brake",false);map.AddBinding("vehicle_brake",{"key:Space",1,0});}
        if(!project.Save(error)){project.Settings().input=originalInput;return 0;}
        std::string name="VehicleDriver.js";
        for(unsigned index=2;index<10000&&fs::exists(fs::u8path(project.AssetsDir())/"Scripts"/name);++index)
            name="VehicleDriver"+std::to_string(index)+".js";
        AssetRecord script;
        const auto read=[](const std::string& path){std::ifstream file(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(file),{});};
        const auto starter=read(ResolveEngineDataPath("docs/judasjs/examples/first-project.js"));
        if(!starter.empty())for(auto& object:staged.GetScene().Objects())for(auto& slot:object.scripts){
            const auto* record=assets.Find(slot.asset);
            if(record&&record->type==AssetType::Script&&!record->missing&&read(record->path)==starter)slot.enabled=false;
        }
        if(!CreateAndAttachEditorScript(staged,created.root,project,assets,name,script,error,driver)){
            project.Settings().input=originalInput;
            std::string rollback;if(!project.Save(rollback))error+="; input rollback could not be saved: "+rollback;
            return 0;
        }
    }else staged.CommitEdit();
    if(!staged.ValidationError().empty()){error=staged.ValidationError();return 0;}
    document=std::move(staged);
    document.Select(created.root);
    return created.root;
}
