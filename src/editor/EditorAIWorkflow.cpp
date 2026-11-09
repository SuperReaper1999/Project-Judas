#include "EditorAIWorkflow.h"
#include "EditorDocument.h"
#include "EditorPanels.h"
#include "EditorPrimitiveAssets.h"
#include "AIBehaviourTemplate.h"
#include "UtilityAIEditor.h"
#include "PlatformServices.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "ScriptSystem.h"
#include "PlanarGeometry.h"
#include "Prefab.h"
#include "imgui.h"
#include "../../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace {
namespace fs=std::filesystem;
using Json=nlohmann::json;
std::string Source(const fs::path& path) {
    if(fs::file_size(path)>1024*1024)throw std::runtime_error("AI behaviour source exceeds 1 MiB");
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("Cannot read AI behaviour source");
    return {std::istreambuf_iterator<char>(file),{}};
}
void Replace(std::string& value,const std::string& token,const std::string& replacement) {
    const auto at=value.find(token);
    if(at==std::string::npos)throw std::runtime_error("AI template linkage is missing");
    value.replace(at,token.size(),replacement);
}
void WriteNew(const fs::path& destination,const std::string& source) {
    auto staged=CreateImportStagingFile(destination);
    try {
        {std::ofstream file(fs::u8path(staged),std::ios::binary);file<<source;file.close();
         if(!file)throw std::runtime_error("Cannot write AI behaviour asset");}
        fs::copy_file(fs::u8path(staged),destination,fs::copy_options::none);
        std::error_code ignored;fs::remove(fs::u8path(staged),ignored);
    }catch(...){std::error_code ignored;fs::remove(fs::u8path(staged),ignored);throw;}
}
std::string GuardDefinition() {
    return R"JSON({"version":1,"name":"Patrol and hide","interval":0.2,"switchAdvantage":0.05,
      "actions":[
        {"id":"patrol","weight":0.2,"considerations":[]},
        {"id":"hide","weight":2,"minCommitment":0.4,"cooldown":0.2,
         "considerations":[{"fact":"health","curve":"inverse"}]}
      ]})JSON";
}
}

bool CreateAndAttachEditorAI(EditorDocument& doc,SceneObjectId owner,const Project& project,
                            AssetDatabase& assets,const std::string& requested,
                            EditorAIBehaviourAssets& output,std::string& error) {
    error.clear();EditorAIBehaviourAssets result;
    std::vector<fs::path> created;
    std::vector<AssetId> registered;
    bool edit=false;
    try {
        if(!project.IsLoaded()||!doc.GetScene().Find(owner))throw std::runtime_error("Open a project and select an existing object first");
        if(doc.EditInProgress())throw std::runtime_error("Finish the current inspector edit before creating AI behaviour");
        // A human may have added Scripts before choosing this creation command.
        // Validate a copy with only the first placeholder we will fill omitted;
        // the real document and its complete Undo snapshot remain untouched.
        Scene baselineCandidate=doc.GetScene();
        auto& existingSlots=baselineCandidate.Find(owner)->scripts;
        const auto placeholder=std::find_if(existingSlots.begin(),existingSlots.end(),[](const auto& slot){return slot.asset.empty();});
        if(placeholder!=existingSlots.end())existingSlots.erase(placeholder);
        std::string baseline;SaveSceneToString(baselineCandidate,baseline);Scene checkedBaseline;
        if(!LoadSceneFromString(baseline,checkedBaseline,error))throw std::runtime_error("Fix the existing scene before adding AI: "+error);
        const auto original=*doc.GetScene().Find(owner);
        const bool planar=original.body&&original.body->shape==SceneShape::Planar;
        if(original.body&&(!planar||original.body->motion!=SceneBodyMotion::Kinematic||!original.body->planar||original.body->planar->kind==PlanarKind::Chain))
            throw std::runtime_error("This starter supports CharacterMotor actors or kinematic native 2D bodies. Remove the existing rigid body or use your own action code; its physics will not be changed silently.");
        if(original.ragdoll||original.deformable||original.socket)
            throw std::runtime_error("This actor already has another pose owner. Use your own AI action script for this assembly.");
        auto name=fs::u8path(requested);
        if(requested.empty()||requested.size()>96||requested.find_first_of("/\\:\n\r")!=std::string::npos||name!=name.filename())
            throw std::runtime_error("Enter a name such as Guard, without folders");
        if(name.extension()==".js")name=name.stem();
        if(name.empty()||name=="."||name==".."||name.extension()==".ai")
            throw std::runtime_error("Use a behaviour name such as Guard, not an AI definition filename");
        const auto root=fs::absolute(fs::u8path(assets.AssetsDir())).lexically_normal();
        if(fs::canonical(root)!=fs::canonical(fs::u8path(project.AssetsDir())))throw std::runtime_error("Asset database belongs to another project");
        const auto directory=root/"Scripts";
        if(fs::is_symlink(fs::symlink_status(directory)))throw std::runtime_error("Scripts folder must not be a symbolic link");
        fs::create_directories(directory);
        const auto behaviour=directory/(name.u8string()+".js"),definition=directory/(name.u8string()+".ai.js");
        for(const auto& path:{behaviour,definition})for(const auto& candidate:{path,fs::u8path(path.u8string()+".judasmeta")})
            if(fs::symlink_status(candidate).type()!=fs::file_type::not_found)
                throw std::runtime_error("That behaviour or definition already exists. Existing files are never overwritten.");
        // Shared primitive assets are ordinary reusable engine authoring outputs.
        // Generate before the scene transaction; no existing actor is changed.
        std::optional<SceneRenderComponent> visual=original.render;
        if(!visual&&!planar) {
            EditorDocument primitive;
            const auto id=CreateEditorPrimitive(primitive,project,assets,"capsule",{},0,error);
            if(!id)throw std::runtime_error(error);
            visual=primitive.GetScene().Find(id)->render;
        }
        WriteNew(definition,EncodeUtilityAIModule(GuardDefinition()));created.push_back(definition);
        if(!assets.Track(definition.u8string(),result.definition,error))throw std::runtime_error(error);
        registered.push_back(result.definition.id);
        std::string source=kAIPatrolHideTemplate;
        Replace(source,"'__DEFINITION_IMPORT__'",Json("./"+definition.filename().u8string()).dump());
        Replace(source,"__DEFINITION_ID__",result.definition.id);
        WriteNew(behaviour,source);created.push_back(behaviour);
        if(!assets.Track(behaviour.u8string(),result.behaviour,error))throw std::runtime_error(error);
        registered.push_back(result.behaviour.id);
        std::string schema;
        if(!ScriptSystem::Inspect(assets,result.behaviour.id,schema,error))throw std::runtime_error("Generated AI script is invalid: "+error);
        std::vector<ScriptProperty> defaultFields;
        if(!ScriptSystem::ReadProperties(schema,"{}",defaultFields,error))throw std::runtime_error("Generated AI defaults are invalid: "+error);
        // All native scene additions are a single undo step. Targets are root
        // objects, so they do not travel with the actor they're guiding.
        Scene flat;if(!FlattenHierarchy(doc.GetScene(),flat,error))throw std::runtime_error(error);
        const auto* resolved=flat.Find(owner);
        const auto position=resolved->transform.position;const auto rotation=resolved->transform.rotation;
        doc.BeginEdit();edit=true;
        auto* actor=doc.GetScene().Find(owner);
        if(!planar&&!actor->characterMotor)actor->characterMotor=CharacterMotorSettings{};
        if(visual&&!actor->render)actor->render=visual;
        if(planar&&!actor->render){actor->render=SceneRenderComponent{};actor->render->shape=SceneShape::Sprite;actor->render->sprite.size={.6f,1.2f};}
        const auto makeTarget=[&](const char* label,glm::vec3 offset,glm::vec3 color) {
            auto& point=doc.GetScene().CreateObject(original.name+label);
            point.transform.position=position+rotation*offset;
            point.render=SceneRenderComponent{};point.render->shape=SceneShape::Box;
            point.render->halfExtents={.15f,.15f,.15f};point.render->color=color;
            return point.id;
        };
        result.patrolPoint=makeTarget(" - patrol point",planar?glm::vec3(3,0,0):glm::vec3(3,0,0),{.25f,.75f,.45f});
        result.returnPoint=makeTarget(" - return point",{0,0,0},{.55f,.8f,.4f});
        result.coverPoint=makeTarget(" - cover point",planar?glm::vec3(-3,2,0):glm::vec3(-3,0,2),{.25f,.55f,.9f});
        actor=doc.GetScene().Find(owner);uint64_t slot=1;
        for(const auto& existing:actor->scripts){if(existing.id==UINT64_MAX)throw std::runtime_error("Script slot identities exhausted");slot=std::max(slot,existing.id+1);}
        const auto properties=Json{{"patrolPoint",{{"entity",std::to_string(result.patrolPoint)}}},{"returnPoint",{{"entity",std::to_string(result.returnPoint)}}},{"coverPoint",{{"entity",std::to_string(result.coverPoint)}}},{"planar",planar}}.dump();
        std::vector<ScriptProperty> actualFields;
        if(!ScriptSystem::ReadProperties(schema,properties,actualFields,error))throw std::runtime_error("AI target properties are invalid: "+error);
        const auto empty=std::find_if(actor->scripts.begin(),actor->scripts.end(),[](const auto& entry){return entry.asset.empty();});
        if(empty!=actor->scripts.end()) {
            empty->asset=result.behaviour.id;empty->enabled=true;empty->properties=properties;
        } else actor->scripts.push_back({slot,result.behaviour.id,true,properties});
        std::string candidate;SaveSceneToString(doc.GetScene(),candidate);Scene validated;
        if(!LoadSceneFromString(candidate,validated,error))throw std::runtime_error("AI scene setup is invalid: "+error);
        doc.CommitEdit();edit=false;
        if(!doc.ValidationError().empty())throw std::runtime_error(doc.ValidationError());
        output=std::move(result);return true;
    }catch(const std::exception& e) {
        if(edit)doc.CancelEdit();
        // These IDs and paths were all exclusively minted in this invocation.
        for(const auto& id:registered){std::string ignored;assets.Remove(id,ignored);}
        for(const auto& path:created){std::error_code ignored;fs::remove(path,ignored);fs::remove(fs::u8path(path.u8string()+".judasmeta"),ignored);}
        error=e.what();return false;
    }
}

bool FindEditorAIDefinition(const AssetDatabase& assets,const AssetRecord& behaviour,
                            AssetRecord& definition,std::string& error) {
    error.clear();
    try {
        if(behaviour.type!=AssetType::Script||behaviour.missing)return false;
        const auto source=Source(fs::u8path(behaviour.path));
        // Parse only deliberate literal metadata/import declarations, never JS
        // expressions. Line lengths are bounded before parsing; no recursive
        // regular-expression walk over arbitrary project source is involved.
        const auto literal=[](std::string value) {
            const auto end=value.find_last_not_of(" \t\r");
            if(end==std::string::npos||value[end]!=';')throw std::runtime_error("AI linkage requires a literal declaration");
            value.resize(end);
            while(!value.empty()&&(value.back()==' '||value.back()=='\t'))value.pop_back();
            if(value.size()<2)throw std::runtime_error("AI linkage requires a string");
            if(value.front()=='"')return Json::parse(value).get<std::string>();
            if(value.front()=='\''&&value.back()=='\''&&value.find('\\')==std::string::npos)return value.substr(1,value.size()-2);
            throw std::runtime_error("AI linkage uses an unsupported string literal");
        };
        std::string id,imported;unsigned declarations=0,imports=0;
        std::istringstream lines(source);std::string line;
        const std::string declaration="export const aiDefinition = ",importPrefix="import definition from ";
        while(std::getline(lines,line)) {
            const bool metadata=line.rfind(declaration,0)==0,dependency=line.rfind(importPrefix,0)==0;
            if(!metadata&&!dependency)continue;
            if(line.size()>2048)throw std::runtime_error("AI linkage declaration exceeds 2048 bytes");
            if(metadata){id=literal(line.substr(declaration.size()));++declarations;}
            else {imported=literal(line.substr(importPrefix.size()));++imports;}
        }
        if(declarations==0)return false;
        if(declarations!=1||imports!=1||!IsValidAssetId(id))throw std::runtime_error("Ambiguous or invalid AI definition linkage");
        const auto* record=assets.Find(id);
        if(!record||record->missing||record->type!=AssetType::Script)throw std::runtime_error("Linked AI definition is missing");
        std::string parsed;
        if(!DecodeUtilityAIModule(Source(fs::u8path(record->path)),parsed,error))return false;
        const auto importedPath=(fs::u8path(behaviour.relativePath).parent_path()/fs::u8path(imported)).lexically_normal().generic_string();
        const auto* importedRecord=assets.FindByRelativePath(importedPath);
        if(!importedRecord||importedRecord->id!=record->id)throw std::runtime_error("AI metadata and imported definition disagree");
        definition=*record;return true;
    }catch(const std::exception& e){error=e.what();return false;}
}

void DrawEditorAIWorkflow(EditorDocument& doc,SceneObject& object,EditorPanelState& state) {
    const bool editable=state.mode==EditorMode::Edit&&state.project&&state.project->IsLoaded()&&state.assets&&doc.Selection().size()==1;
    ImGui::BeginDisabled(!editable);
    if(ImGui::Button("Create AI behaviour",{-1,0}))ImGui::OpenPopup("Create AI behaviour");
    if(ImGui::BeginPopupModal("Create AI behaviour",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Create a patrol / damage-hide NPC, with matching decision settings and action script. The three target markers can be moved afterward.");
        char name[256];std::snprintf(name,sizeof(name),"%s",state.newAIBehaviourName.c_str());
        if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();
        if(ImGui::InputText("Behaviour name",name,sizeof(name)))state.newAIBehaviourName=name;
        if(ImGui::Button("Create & attach")&&editable) {
            EditorAIBehaviourAssets result;
            if(CreateAndAttachEditorAI(doc,object.id,*state.project,*state.assets,state.newAIBehaviourName,result,state.aiWorkflowMessage)) {
                state.utilityAIEdit=result.definition.id;state.showUtilityAI=true;state.utilityAIAuthorRequested=true;state.utilityAILiveRequested=false;
                state.revealComponent="Scripts";
                state.status="AI attached. Decision settings are open; action code is in Assets/Scripts. Move the green patrol and blue cover markers, then Play.";
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();if(ImGui::Button("Cancel")||ImGui::IsKeyPressed(ImGuiKey_Escape))ImGui::CloseCurrentPopup();
        if(!state.aiWorkflowMessage.empty())ImGui::TextWrapped("%s",state.aiWorkflowMessage.c_str());
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
}
void DrawSelectedAIShortcuts(const SceneObject& object,EditorPanelState& state) {
    if(!state.assets)return;
    bool any=false;
    for(const auto& slot:object.scripts) {
        const auto* behaviour=state.assets->Find(slot.asset);if(!behaviour)continue;
        AssetRecord definition;std::string error;
        if(!FindEditorAIDefinition(*state.assets,*behaviour,definition,error)) {
            if(!error.empty()){ImGui::TextWrapped("AI definition link: %s",error.c_str());if(ImGui::SmallButton("Open behaviour source"))state.editScriptAsset=behaviour->id;}
            continue;
        }
        if(!any){ImGui::SeparatorText("AI behaviour");any=true;}
        ImGui::PushID(static_cast<int>(slot.id));
        ImGui::TextWrapped("%s",behaviour->relativePath.c_str());
        if(ImGui::SmallButton("Decision settings")){state.utilityAIEdit=definition.id;state.showUtilityAI=true;state.utilityAILiveRequested=false;state.utilityAIAuthorRequested=true;}
        ImGui::SameLine();if(ImGui::SmallButton("Action code"))state.editScriptAsset=behaviour->id;
        if(state.mode==EditorMode::Play){
            ImGui::SameLine();if(ImGui::SmallButton("Inspect this NPC")){state.utilityAIEntity=object.id;state.utilityAILiveRequested=true;state.utilityAIAuthorRequested=false;state.showUtilityAI=true;}
        }
        ImGui::PopID();
    }
}
