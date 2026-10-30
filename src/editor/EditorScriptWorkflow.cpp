#include "EditorScriptWorkflow.h"
#include "EditorPanels.h"
#include "EditorPrimitiveAssets.h"
#include "Project.h"
#include "PlatformServices.h"
#include "EnginePaths.h"
#include "imgui.h"
#include <algorithm>
#include <fstream>
#include <limits>

namespace fs = std::filesystem;
namespace {
constexpr const char* source = R"JS(import {console} from 'judas';

export default class {
  /** @param {import('judas').ScriptContext} context */
  constructor({entity}) {
    this.entity = entity;
    this.state = {elapsed: 0};
  }

  start() { console.log('My script is running.'); }

  /** @param {number} dt */
  update(dt) {
    this.state.elapsed += dt;
    // Add your gameplay here. Physics commands belong in fixedUpdate(dt).
  }
}
)JS";
}

bool AttachEditorScript(EditorDocument& doc, SceneObjectId owner, const AssetDatabase& assets,
                        const AssetId& asset, std::string& error, const std::string& properties) {
    error.clear();
    auto* object = doc.GetScene().Find(owner);
    const auto* record = assets.Find(asset);
    if (!object) { error = "Select an existing object first."; return false; }
    if (!record || record->missing || record->type != AssetType::Script) {
        error = "Choose an available JavaScript asset."; return false;
    }
    uint64_t next = 1;
    for (const auto& slot : object->scripts) {
        if (slot.asset == asset) { error = "This script is already attached to this object."; return false; }
        if (slot.id == std::numeric_limits<uint64_t>::max()) { error = "Script slot IDs exhausted."; return false; }
        next = std::max(next, slot.id + 1);
    }
    doc.BeginEdit();
    auto empty = std::find_if(object->scripts.begin(), object->scripts.end(), [](const auto& s) { return s.asset.empty(); });
    if (empty != object->scripts.end()) { empty->asset = asset; empty->enabled = true; empty->properties = properties; }
    else object->scripts.push_back({next, asset, true, properties});
    doc.CommitEdit();
    error = doc.ValidationError();
    return error.empty();
}

bool CreateAndAttachEditorScript(EditorDocument& doc, SceneObjectId owner, const Project& project,
                                AssetDatabase& assets, const std::string& fileName,
                                AssetRecord& record, std::string& error, const std::string& sourceTemplate,
                                const std::string& properties) {
    error.clear();
    std::string staged;
    try {
        if (!project.IsLoaded() || !doc.GetScene().Find(owner)) throw std::runtime_error("Open a project and select an object first.");
        auto name = fs::u8path(fileName);
        if (fileName.empty() || fileName.find_first_of("/\\:") != std::string::npos || name != name.filename() ||
            name == "." || name == ".." || name.stem().empty())
            throw std::runtime_error("Enter a file name such as MyScript.js, without folders.");
        if (name.extension().empty()) name += ".js";
        if (name.extension() != ".js") throw std::runtime_error("Script names must end in .js.");
        const auto root = fs::canonical(fs::u8path(project.AssetsDir()));
        if (root != fs::canonical(fs::u8path(assets.AssetsDir()))) throw std::runtime_error("Asset database belongs to another project.");
        const auto directory = root / "Scripts";
        if (fs::is_symlink(fs::symlink_status(directory))) throw std::runtime_error("Scripts folder must not be a symbolic link.");
        fs::create_directories(directory);
        const auto destination = directory / name;
        if (fs::symlink_status(destination).type() != fs::file_type::not_found ||
            fs::symlink_status(destination.u8string() + ".judasmeta").type() != fs::file_type::not_found)
            throw std::runtime_error("That script or its metadata already exists. Use Attach existing script instead.");
        staged = CreateImportStagingFile(destination);
        { std::ofstream file(staged, std::ios::binary); file << (sourceTemplate.empty()?source:sourceTemplate); file.close();
          if (!file) throw std::runtime_error("Cannot write the new script."); }
        // No replacement: even a file created concurrently must survive untouched.
        fs::copy_file(staged, destination, fs::copy_options::none);
        fs::remove(staged); staged.clear();
        if (!assets.Track(destination.u8string(), record, error)) {
            error = "Source kept at " + destination.u8string() + "; asset registration failed: " + error;
            return false;
        }
        if (!AttachEditorScript(doc, owner, assets, record.id, error, properties)) {
            error = "Script asset created; attachment failed: " + error; return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (!staged.empty()) { std::error_code ignored; fs::remove(fs::u8path(staged), ignored); }
        error = e.what(); return false;
    }
}

void DrawEditorScriptWorkflow(EditorDocument& doc, SceneObject& object, EditorPanelState& state) {
    const bool editable=state.mode == EditorMode::Edit && state.project && state.project->IsLoaded() && state.assets && doc.Selection().size()==1;
    if(doc.Selection().size()!=1)ImGui::TextWrapped("Select one object to create or attach a script.");
    ImGui::BeginDisabled(!editable);
    if (ImGui::Button("Create script & attach", {-1, 0}) && editable) {
        state.newScriptOwner = object.id;
        state.scriptWorkflowMessage.clear();
        ImGui::OpenPopup("Create script");
    }
    if (ImGui::Button("Attach existing script", {-1, 0}) && editable) ImGui::OpenPopup("Attach script");
    if (ImGui::BeginPopupModal("Create script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Create a JavaScript file in Assets/Scripts and attach it to this object.");
        char name[256]; std::snprintf(name, sizeof(name), "%s", state.newScriptName.c_str());
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("File name", name, sizeof(name))) state.newScriptName = name;
        if (ImGui::Button("Create & edit") && editable) {
            AssetRecord record;
            if (CreateAndAttachEditorScript(doc, state.newScriptOwner, *state.project, *state.assets,
                                           state.newScriptName, record, state.scriptWorkflowMessage)) {
                state.editScriptAsset = record.id; state.revealComponent = "Scripts";
                state.status = "Script attached. Save scene to keep the attachment; edit and save source, then Play.";
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        if (!state.scriptWorkflowMessage.empty()) ImGui::TextWrapped("%s", state.scriptWorkflowMessage.c_str());
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("Attach script")) {
        bool any = false;
        if (state.assets) for (const auto& [id, record] : state.assets->Records()) {
            if (record.type != AssetType::Script || record.missing) continue;
            any = true;
            if (ImGui::Selectable(record.relativePath.c_str()) && editable) {
                std::string error;
                if (AttachEditorScript(doc, object.id, *state.assets, id, error)) {
                    state.revealComponent = "Scripts"; state.status = "Script attached. Save scene, then Play.";
                } else state.status = error;
            }
        }
        if (!any) ImGui::TextDisabled("No scripts yet. Use Create script & attach.");
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
}

SceneObjectId CreateEditorFirstPersonPlayer(EditorDocument& doc, Project& project, AssetDatabase& assets,
                                           glm::vec3 position, SceneObjectId parent, std::string& error, bool thirdPerson) {
    error.clear();
    bool transaction=false;
    try {
    if(doc.EditInProgress()){error="Finish the current inspector edit before creating a player.";return 0;}
    if(!project.IsLoaded()){error="Open or create a project before adding a player.";return 0;}
    if(project.Settings().legacyGameplay){error="Disable legacy gameplay in Project Settings before adding a script-owned player.";return 0;}
    if(parent&&!doc.GetScene().Find(parent)){error="The chosen parent no longer exists.";return 0;}
    for(const auto* name:{"move_x","move_y","look_x","look_y","jump","pause","view_toggle"}){
        const auto* entry=project.Settings().input.Find(name);
        const bool axisInput=std::string(name)!="jump"&&std::string(name)!="pause"&&std::string(name)!="view_toggle";
        if(!entry||entry->axis!=axisInput){error=std::string("Configure logical input '")+name+"' in Project Settings > Input first.";return 0;}
    }
    const auto read=[](const std::string& path){std::ifstream f(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
    const auto playerSource=read(ResolveEngineDataPath("docs/judasjs/examples/first-person-player.js"));
    if(playerSource.empty()){error="First-person player template is missing from this SDK's docs/judasjs/examples folder.";return 0;}
    const auto starterSource=read(ResolveEngineDataPath("docs/judasjs/examples/first-project.js"));
    EditorDocument visual;
    const auto visualId=CreateEditorPrimitive(visual,project,assets,"capsule",{},0,error);
    if(!visualId)return 0;
    const auto render=visual.GetScene().Find(visualId)->render;
    std::string filename="Player.js";
    for(unsigned i=2;fs::exists(fs::u8path(project.AssetsDir())/"Scripts"/filename)&&i<10000;++i)filename="Player"+std::to_string(i)+".js";
    doc.BeginEdit();transaction=true;
    // Only the exact, unedited editor starter is superseded. User cameras are never guessed at.
    if(!starterSource.empty())for(auto& object:doc.GetScene().Objects())for(auto& slot:object.scripts){
        const auto* record=assets.Find(slot.asset);
        if(record&&record->type==AssetType::Script&&!record->missing&&read(record->path)==starterSource)slot.enabled=false;
    }
    auto& object=doc.GetScene().CreateObject(thirdPerson?"Third-person player":"First-person player");const auto id=object.id;
    object.parent=parent;object.transform.position=parent?glm::vec3(0,1.02f,0):position+glm::vec3(0,1.02f,0);
    object.characterMotor=CharacterMotorSettings{};
    object.characterMotor->radius=.5f;object.characterMotor->halfHeight=.5f;
    object.render=render;object.render->color={.2f,.65f,.9f};
    AssetRecord record;
    if(!CreateAndAttachEditorScript(doc,id,project,assets,filename,record,error,playerSource,
                                   thirdPerson?"{\"thirdPerson\":true}":"{}")){
        doc.CancelEdit();return 0;
    }
    doc.Select(id);return id;
    }catch(const std::exception& e){if(transaction)doc.CancelEdit();error=e.what();return 0;}
}

SceneObjectId CreateEditorPlanarPlayer(EditorDocument& doc,Project& project,AssetDatabase& assets,
                                      glm::vec3 position,SceneObjectId parent,std::string& error,bool sideView) {
    if(!project.IsLoaded()){error="Open a project before creating a 2D player";return 0;}
    std::ifstream source(ResolveEngineDataPath("docs/judasjs/examples/planar-player.js"),std::ios::binary);
    if(!source){error="Missing 2D player script in SDK docs";return 0;}
    const std::string text{std::istreambuf_iterator<char>(source),{}};
    doc.BeginEdit();auto& object=doc.GetScene().CreateObject(sideView?"2D platformer player":"2D top-down player");
    const auto id=object.id;object.parent=parent;object.transform.position=parent?glm::vec3(0):position;
    object.render=SceneRenderComponent{};object.render->shape=SceneShape::Sprite;
    object.render->sprite.size={.6f,1.2f};object.render->color={.4f,.8f,1};
    object.body=SceneBodyComponent{};object.body->shape=SceneShape::Planar;object.body->planar=PlanarCapsule(.3,.3,true);
    object.body->motion=SceneBodyMotion::Dynamic;object.body->mass=5;object.body->friction=0;object.body->managed=false;
    doc.CommitEdit();doc.Select(id);
    AssetRecord record;
    if(!CreateAndAttachEditorScript(doc,id,project,assets,"Player2D_"+std::to_string(id)+".js",record,error,text,
          sideView?"{\"sideView\":true}":"{\"sideView\":false}"))return id;
    error.clear();return id;
}
