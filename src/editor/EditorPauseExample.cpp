#include "EditorPauseExample.h"
#include "EditorScriptWorkflow.h"
#include "EditorDocument.h"
#include "RuntimeUI.h"
#include "Project.h"
#include "AssetDatabase.h"
#include "EnginePaths.h"
#include "PlatformServices.h"
#include <filesystem>
#include <fstream>

SceneObjectId CreateEditorPauseExample(EditorDocument& doc,const Project& project,AssetDatabase& assets,std::string& error,SceneObjectId parent){
    namespace fs=std::filesystem;
    bool transaction=false;std::string staged;
    try{
        if(!project.IsLoaded()||project.Settings().legacyGameplay)throw std::runtime_error("Open a project with legacy gameplay disabled before adding the pause example.");
        if(doc.EditInProgress())throw std::runtime_error("Finish the current inspector edit first.");
        if(parent&&!doc.GetScene().Find(parent))throw std::runtime_error("The chosen parent no longer exists.");
        for(const auto& object:doc.GetScene().Objects())if(object.ui&&object.ui->name=="pause_menu")throw std::runtime_error("This scene already has a pause_menu document.");
        for(const auto* name:{"pause","ui_click","ui_activate","ui_up","ui_down","ui_left","ui_right"}){
            const auto* entry=project.Settings().input.Find(name);
            if(!entry||entry->axis||entry->vector)throw std::runtime_error(std::string("Configure the logical action '")+name+"' in Project Settings > Input first.");
        }
        std::ifstream script(ResolveEngineDataPath("docs/judasjs/examples/pause-menu.js"),std::ios::binary);
        const std::string source{std::istreambuf_iterator<char>(script),{}};
        if(source.empty())throw std::runtime_error("Pause example source is missing from the SDK docs/judasjs/examples folder.");
        const auto root=fs::canonical(fs::u8path(project.AssetsDir()));
        if(root!=fs::canonical(fs::u8path(assets.AssetsDir())))throw std::runtime_error("Asset database belongs to another project.");
        const auto directory=root/"UI";
        if(fs::is_symlink(fs::symlink_status(directory)))throw std::runtime_error("UI folder must not be a symbolic link.");
        fs::create_directories(directory);const auto path=directory/"pause-menu.judasui";
        if(fs::symlink_status(path).type()!=fs::file_type::not_found||fs::symlink_status(path.u8string()+".judasmeta").type()!=fs::file_type::not_found)throw std::runtime_error("Pause menu asset already exists. Reuse or rename it; existing files are not overwritten.");
        UIDocument ui;ui.visible=false;
        const auto element=[&](const char* id,const char* parent,UIKind kind,glm::vec2 offset,glm::vec2 size,const char* text=""){
            UIElement e;e.id=id;e.parent=parent;e.kind=kind;e.offset=offset;e.size=size;e.text=text;e.fontSize=20;e.wrap=true;
            if(kind==UIKind::Button){e.background={.11f,.2f,.27f,1};e.focusColor={.3f,.8f,1,1};e.textAlign={.5f,.5f};}
            ui.elements.push_back(e);
        };
        element("canvas","",UIKind::Canvas,{0,0},{1280,720});ui.elements.back().background={0,0,0,.65f};
        element("menu","canvas",UIKind::Panel,{-260,-300},{520,600});ui.elements.back().anchorMin=ui.elements.back().anchorMax={.5f,.5f};ui.elements.back().background={.04f,.065f,.085f,.98f};
        element("title","menu",UIKind::Text,{20,18},{480,44},"PAUSE MENU");ui.elements.back().fontSize=30;
        element("tab_pause","menu",UIKind::Button,{20,76},{230,44},"Pause");
        element("tab_settings","menu",UIKind::Button,{270,76},{230,44},"Settings");
        element("pause","menu",UIKind::Panel,{20,140},{480,440});
        element("resume","pause",UIKind::Button,{0,16},{480,52},"Resume");
        element("restart","pause",UIKind::Button,{0,86},{480,52},"Restart scene");
        element("quit","pause",UIKind::Button,{0,156},{480,52},"Quit game");
        element("help","pause",UIKind::Text,{0,240},{480,150},"Escape pauses/resumes. Settings lists your project's controls. This is editable project UI and JavaScript.");
        element("settings","menu",UIKind::Panel,{20,140},{480,440});ui.elements.back().visible=false;
        element("status","settings",UIKind::Text,{0,0},{480,65},"Select a binding to change.");ui.elements.back().fontSize=17;
        element("filter","settings",UIKind::TextInput,{0,70},{480,38});ui.elements.back().placeholder="Find a key, mouse button or gamepad control";ui.elements.back().visible=false;
        element("bindings","settings",UIKind::List,{0,116},{480,210});ui.elements.back().rowHeight=35;ui.elements.back().background={.07f,.11f,.14f,1};ui.elements.back().fontSize=17;
        element("choices","settings",UIKind::List,{0,116},{480,210});ui.elements.back().rowHeight=35;ui.elements.back().background={.07f,.11f,.14f,1};ui.elements.back().fontSize=17;ui.elements.back().visible=false;
        element("change","settings",UIKind::Button,{0,344},{480,44},"Change selected binding");
        element("apply","settings",UIKind::Button,{0,344},{480,44},"Apply selected control");ui.elements.back().visible=false;
        element("reset","settings",UIKind::Button,{0,400},{480,40},"Restore project defaults");
        std::string detail;if(!ui.Validate(detail))throw std::runtime_error(detail);
        staged=CreateImportStagingFile(path);
        if(!SaveUIDocument(staged,ui,detail))throw std::runtime_error(detail);
        fs::copy_file(staged,path,fs::copy_options::none);fs::remove(staged);staged.clear();
        AssetRecord uiAsset;if(!assets.Track(path.u8string(),uiAsset,error))return 0;
        doc.BeginEdit();transaction=true;auto& object=doc.GetScene().CreateObject("Pause menu + controls");const auto id=object.id;
        object.parent=parent;
        object.ui=SceneUIComponent{uiAsset.id,"pause_menu",true};
        AssetRecord scriptAsset;
        if(!CreateAndAttachEditorScript(doc,id,project,assets,"PauseMenu.js",scriptAsset,error,source)){doc.CancelEdit();return 0;}
        doc.Select(id);return id;
    }catch(const std::exception& e){if(transaction)doc.CancelEdit();if(!staged.empty()){std::error_code ignored;fs::remove(staged,ignored);}error=e.what();return 0;}
}
