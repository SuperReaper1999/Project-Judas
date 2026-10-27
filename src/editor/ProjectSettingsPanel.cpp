#include "ProjectFeaturePolicy.h"
#include "EditorPanels.h"
#include "ProjectSettingsHistory.h"
#include "EditorDocument.h"
#include "Project.h"
#include "ResourceManager.h"
#include "SaveEvolution.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <vector>

namespace {
void CopyToBuffer(const std::string& value,char* data,size_t size){
    if(size){const auto count=std::min(value.size(),size-1);std::memcpy(data,value.data(),count);data[count]=0;}
}
}

void DrawProjectSettingsPanel(EditorDocument& doc,EditorPanelState& state,EditorRequests& requests){
    (void)doc;
    if(!state.showProjectSettings||!state.project)return;
    ImGui::SetNextWindowSize({920,680},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Project Settings",&state.showProjectSettings)){ImGui::End();return;}
    auto& project=*state.project;
    if(!project.IsLoaded()){ImGui::TextDisabled("Open a project to configure it.");ImGui::End();return;}
    if(!state.projectHistory)state.projectHistory=std::make_shared<ProjectSettingsHistory>();
    auto& history=*state.projectHistory;history.Observe(project);
    auto& s=project.Settings();
    static int page=0;
    const char* categories[]={"General","Rendering","Physics and gravity","Input","Audio","World and persistence",
                              "Navigation","Localization","Water","Networking","Experimental","Export"};
    ImGui::TextDisabled("%s",project.ProjectFile().c_str());
    if(project.IsDirty())ImGui::TextColored({1,.75f,.3f,1},"Unsaved project settings");
    ImGui::BeginChild("Categories",{182,-64},true);
    for(int i=0;i<12;++i)if(ImGui::Selectable(categories[i],page==i))page=i;
    ImGui::EndChild();ImGui::SameLine();
    ImGui::BeginChild("Settings",{0,-64},true);
    ImGui::TextUnformatted(categories[page]);ImGui::Separator();
    const bool editing=state.mode==EditorMode::Edit;
    if(!editing)ImGui::TextWrapped("Stop Play before changing project configuration. The running world owns a snapshot.");
    ImGui::BeginDisabled(!editing);
    if(page==0){
    char name[256];
    CopyToBuffer(s.name, name, sizeof(name));
    if (ImGui::InputText("Name", name, sizeof(name))) s.name = name;
    if (ImGui::BeginCombo("Game icon", s.iconAsset.empty() ? "(Judas fallback)" : s.iconAsset.c_str())) {
        if (ImGui::Selectable("(Judas fallback)", s.iconAsset.empty())) s.iconAsset.clear();
        if (state.assets) for (const auto& [id, asset] : state.assets->Records())
            if (asset.type == AssetType::Texture && !asset.missing &&
                std::filesystem::path(asset.path).extension() == ".png" &&
                ImGui::Selectable(asset.relativePath.c_str(), s.iconAsset == id)) s.iconAsset = id;
        ImGui::EndCombo();
    }
    if (ImGui::BeginCombo("Startup scene", s.startupScene.empty() ? "(none)" : s.startupScene.c_str())) {
        for (const std::string& scene : state.sceneFiles) {
            if (ImGui::Selectable(scene.c_str(), scene == s.startupScene)) s.startupScene = scene;
        }
        ImGui::EndCombo();
    }
    ImGui::TextWrapped("Startup scene, identity and fallback icon are included in standalone packages.");
    }
    if(page==1){
        int budget=int(s.visualBudgetMiB);
        if(ImGui::InputInt("Cooked visual budget (MiB)",&budget))s.visualBudgetMiB=unsigned(std::clamp(budget,0,65536));
        ImGui::TextWrapped("Zero preserves pinned visual resources. A nonzero budget enables cooked visual residency on project reopen; demand from all active cameras is retained. Required physics/animation resources remain pinned.");
        ImGui::TextWrapped("Lighting, exposure, sky and background are authored per scene. Material and light properties belong to their components.");
        if(ImGui::Button("Open scene rendering settings"))state.showSceneSettings=true;
    }
    if(page==2){
        ImGui::TextWrapped("Collision filters use stable project category IDs. Gravity fields, regions, body properties and solver settings belong to scenes/components; no global world-Y gravity override is imposed here.");
    {
        auto registry=[&](const char* label,CategoryRegistry& r,bool preserveDefault){
            ImGui::PushID(label);
            if(ImGui::TreeNode(label)){
                static char addName[128]="";ImGui::InputText("New name",addName,sizeof(addName));
                if(ImGui::Button("Create")){unsigned id;if(!r.Add(addName,id))state.status="Name exists/empty or all 64 lifetime IDs used";else addName[0]=0;}
                for(auto it=r.names.begin();it!=r.names.end();++it){ImGui::PushID(int(it->first));char renamed[256];CopyToBuffer(it->second,renamed,sizeof(renamed));
                    ImGui::Text("Stable ID %u",it->first);if(ImGui::InputText("Name",renamed,sizeof(renamed))&&!r.Rename(it->first,renamed))state.status="Invalid/duplicate category name";
                    if(!(preserveDefault&&it->first==0)&&ImGui::Button("Delete")){r.Remove(it->first,preserveDefault);ImGui::PopID();break;}
                    ImGui::PopID();
                }
                ImGui::TextWrapped("Deleted IDs are never reused. Repair assignments before Play/export. Save project to persist changes.");ImGui::TreePop();
            }
            ImGui::PopID();
        };
        registry("Tags",s.classification.tags,false);registry("Collision layers",s.classification.collision,true);registry("Render layers",s.classification.render,true);
    }
        if(ImGui::Button("Open scene physics settings"))state.showSceneSettings=true;
    }
    if(page==3){
    {
        static char newName[128]="";static int newType=0;
        ImGui::InputText("New input name",newName,sizeof(newName));ImGui::Combo("Input type",&newType,"Action\0Scalar axis\0Paired vector\0");
        if(ImGui::Button("Create input")){if(!(newType==2?s.input.AddVector(newName):s.input.Add(newName,newType==1)))state.status="Input name is empty or already exists";else newName[0]=0;}
        ImGui::TextWrapped("Scalar controls: key:Space, mouse:Left, mouse:dx/dy/wheelX/wheelY, pad:South, stick:LeftX. Paired vectors use stick:Left or stick:Right. Circular processing preserves direction, then applies signed X/Y scales. Save project persists bindings.");
        for(std::size_t i=0;i<s.input.entries.size();++i){
            auto& entry=s.input.entries[i];ImGui::PushID(static_cast<int>(i));
            if(ImGui::TreeNode(entry.name.c_str(),"%s (%s)",entry.name.c_str(),entry.vector?"vector":entry.axis?"axis":"action")){
                char inputName[129];CopyToBuffer(entry.name,inputName,sizeof(inputName));
                if(ImGui::InputText("Rename",inputName,sizeof(inputName)))s.input.Rename(entry.name,inputName);
                if(ImGui::Button("Delete input")){const auto name=entry.name;s.input.Remove(name);ImGui::TreePop();ImGui::PopID();break;}
                for(std::size_t b=0;b<entry.bindings.size();++b){
                    ImGui::PushID(static_cast<int>(b));auto binding=entry.bindings[b];char control[129];CopyToBuffer(binding.control,control,sizeof(control));
                    bool changed=ImGui::InputText("Control",control,sizeof(control));binding.control=control;
                    changed|=ImGui::DragFloat(entry.vector?"X scale / inversion":"Scale",&binding.scale,.05f,-10000,10000);
                    if(entry.vector){changed|=ImGui::DragFloat("Y scale / inversion",&binding.scaleY,.05f,-10000,10000);changed|=ImGui::Checkbox("Circular deadzone",&binding.circular);}
                    if(binding.control.rfind("stick:",0)==0)changed|=ImGui::SliderFloat("Deadzone",&binding.deadzone,0,.99f);
                    if(changed&&!s.input.ReplaceBinding(entry.name,b,binding))state.status="Invalid input binding";
                    if(ImGui::Button("Remove binding")){s.input.RemoveBinding(entry.name,b);ImGui::PopID();break;}
                    ImGui::Separator();ImGui::PopID();
                }
                if(ImGui::Button("Add binding"))s.input.AddBinding(entry.name,entry.vector?InputBinding{"stick:Left",1,.15f,1,true}:InputBinding{"key:Space"});
                ImGui::TreePop();
            }ImGui::PopID();
        }
    }
    }
    if(page==4){
    {
        for(auto it=s.audio.groups.begin();it!=s.audio.groups.end();){ImGui::PushID(it->first.c_str());ImGui::TextUnformatted(it->first.c_str());ImGui::SliderFloat("Gain",&it->second.gain,0,1);ImGui::Checkbox("Mute",&it->second.mute);ImGui::Checkbox("Paused",&it->second.paused);bool remove=ImGui::SmallButton("Remove group");ImGui::PopID();if(remove)it=s.audio.groups.erase(it);else ++it;}
        static char group[65]{};ImGui::InputText("New group",group,sizeof(group));if(ImGui::Button("Add sound group")&&group[0]&&std::string(group)!="master"&&s.audio.groups.size()<16)s.audio.groups.emplace(group,AudioGroupSettings{});
        ImGui::TextWrapped("Empty emitter group uses master. Games choose group names and pause policy in JavaScript. Save project to persist routing.");
    }
    }
    if(page==5){
    if(state.assets&&ImGui::BeginCombo("Optional world manifest",s.worldManifest.empty()?"(single scene)":s.worldManifest.c_str())){
        if(ImGui::Selectable("(single scene)",s.worldManifest.empty()))s.worldManifest.clear();
        for(auto& [id,asset]:state.assets->Records())if(asset.type==AssetType::World&&ImGui::Selectable(asset.relativePath.c_str(),s.worldManifest==id))s.worldManifest=id;
        ImGui::EndCombo();
    }
    ImGui::Text("Assets: %s",s.assetsDir.c_str());
    ImGui::Text("Scenes: %s",s.scenesDir.c_str());
    ImGui::Text("Saves: %s",s.savesDir.c_str());
    ImGui::TextWrapped("Directories are project-relative. The optional world manifest owns streaming regions, suspension and budgets. Runtime saves use the existing platform save directory; no working-directory dependency.");
    {
        static std::string evolutionProject;static std::vector<char> draft(1024*1024+1);
        if(evolutionProject!=project.ProjectFile()){evolutionProject=project.ProjectFile();CopyToBuffer(s.saveEvolution,draft.data(),draft.size());}
        ImGui::TextWrapped("Optional project-owned JSON policy. Pin exact old content hashes; explicit ordered migrations never relax native physics validation. See docs/M98_SAVE_EVOLUTION.md. Restart Play after saving configuration.");
        ImGui::InputTextMultiline("Evolution policy",draft.data(),draft.size(),{0,220});
        if(ImGui::Button("Validate / apply save policy")){std::string e;if(ValidateSaveEvolution(draft.data(),e)){s.saveEvolution=draft.data();state.status="Applied save policy; Save project, then restart Play";}else state.status=e;}
        ImGui::SameLine();if(ImGui::Button("Reset policy draft"))CopyToBuffer(s.saveEvolution,draft.data(),draft.size());
        SaveEvolutionIdentity identity;std::string e;if(SaveEvolutionCurrent(s.saveEvolution,identity,e))ImGui::Text("Compatibility: %s | Content version: %u",identity.compatibility.empty()?"exact-content only":identity.compatibility.c_str(),identity.version);
    }
    }
    if(page==6){
    {
        auto& nav=s.navigation;static char areaName[128]="",profileName[128]="";
        ImGui::InputText("New navigation area",areaName,sizeof(areaName));if(ImGui::Button("Add area")&&nav.areas.nextId<62){unsigned id;if(nav.areas.Add(areaName,id))areaName[0]=0;}
        for(auto it=nav.areas.names.begin();it!=nav.areas.names.end();++it){ImGui::PushID(int(it->first));char name[256];CopyToBuffer(it->second,name,sizeof(name));if(ImGui::InputText("Area name",name,sizeof(name)))nav.areas.Rename(it->first,name);if(it->first&&ImGui::Button("Delete area")){nav.areas.Remove(it->first,true);ImGui::PopID();break;}ImGui::PopID();}
        ImGui::InputText("New agent profile",profileName,sizeof(profileName));if(ImGui::Button("Add profile")&&nav.nextProfile<64&&profileName[0]){NavigationProfile p;p.name=profileName;nav.profiles[nav.nextProfile++]=p;profileName[0]=0;}
        for(auto it=nav.profiles.begin();it!=nav.profiles.end();++it){auto& p=it->second;ImGui::PushID(int(it->first)+1000);char name[256];CopyToBuffer(p.name,name,sizeof(name));if(ImGui::InputText("Profile name",name,sizeof(name)))p.name=name;ImGui::DragFloat("Radius",&p.radius,.01f,.02f,10);ImGui::DragFloat("Height",&p.height,.05f,.1f,20);ImGui::DragFloat("Slope degrees",&p.slope,1,0,89);ImGui::DragFloat("Climb",&p.climb,.01f,0,p.height*.9f);if(it->first&&ImGui::Button("Delete profile")){nav.profiles.erase(it);ImGui::PopID();break;}ImGui::PopID();}
        ImGui::TextWrapped("Stable IDs are never reused. Up to 62 navigation areas and 64 lifetime profile IDs. Changing profiles/source geometry requires rebaking; Save project persists settings.");
    }
    }
    if(page==7){
    {
        auto asset=[&](const char* label,std::string& id,AssetType type){if(ImGui::BeginCombo(label,id.empty()?"(none)":id.c_str())){if(ImGui::Selectable("(none)",id.empty()))id.clear();if(state.assets)for(auto& [key,record]:state.assets->Records())if(record.type==type&&!record.missing){if(ImGui::Selectable(record.relativePath.c_str(),id==key))id=key;}ImGui::EndCombo();}};
        auto fonts=[&](const char* label,std::vector<std::string>& list){ImGui::PushID(label);if(ImGui::TreeNode(label)){for(size_t i=0;i<list.size();++i){ImGui::PushID(int(i));asset("Font",list[i],AssetType::Font);if(i&&ImGui::SmallButton("Move up"))std::swap(list[i],list[i-1]);ImGui::SameLine();if(ImGui::SmallButton("Remove")){list.erase(list.begin()+i);ImGui::PopID();break;}ImGui::PopID();}if(list.size()<16&&ImGui::Button("Add fallback font"))list.push_back("");ImGui::TreePop();}ImGui::PopID();};
        if(ImGui::BeginCombo("Default locale",s.localization.defaultLocale.c_str())){for(auto& [tag,entry]:s.localization.locales){(void)entry;if(ImGui::Selectable(tag.c_str(),tag==s.localization.defaultLocale))s.localization.defaultLocale=tag;}ImGui::EndCombo();}
        fonts("Global ordered fallback fonts",s.localization.fonts);
        for(auto it=s.localization.locales.begin();it!=s.localization.locales.end();++it){ImGui::PushID(it->first.c_str());if(ImGui::TreeNode(it->first.c_str())){asset("Catalog",it->second.catalog,AssetType::Catalog);if(ImGui::BeginCombo("Fallback locale",it->second.fallback.empty()?"(default/parent)":it->second.fallback.c_str())){if(ImGui::Selectable("(default/parent)",it->second.fallback.empty()))it->second.fallback.clear();for(auto& [tag,entry]:s.localization.locales)if(tag!=it->first){(void)entry;if(ImGui::Selectable(tag.c_str(),tag==it->second.fallback))it->second.fallback=tag;}ImGui::EndCombo();}fonts("Locale ordered fallback fonts",it->second.fonts);if(ImGui::Button("Remove locale")){auto tag=it->first;s.localization.locales.erase(it);for(auto& [name,entry]:s.localization.locales){(void)name;if(entry.fallback==tag)entry.fallback.clear();}if(s.localization.defaultLocale==tag)s.localization.defaultLocale=s.localization.locales.empty()?"en":s.localization.locales.begin()->first;ImGui::TreePop();ImGui::PopID();break;}ImGui::TreePop();}ImGui::PopID();}
        static char newTag[65]="";static std::string newCatalog;ImGui::InputText("New canonical BCP 47 locale",newTag,sizeof(newTag));asset("New locale catalog",newCatalog,AssetType::Catalog);if(ImGui::Button("Add locale")){auto candidate=s.localization;candidate.locales[newTag]={newCatalog,"",{}};if(s.localization.locales.empty())candidate.defaultLocale=newTag;std::string error;if(!s.localization.locales.count(newTag)&&state.assets&&ValidateLocalizationAssets(candidate,*state.assets,error)){s.localization=std::move(candidate);newTag[0]=0;newCatalog.clear();}else state.status=error.empty()?"Duplicate locale":error;}
        std::string localizationError;if(!s.localization.Validate(localizationError))ImGui::TextWrapped("Cannot save: %s",localizationError.c_str());
        ImGui::TextDisabled("Save project to persist choices; start fresh Play for configuration edits.");
        static std::string projectPath;static char config[16384];if(projectPath!=project.ProjectFile()){projectPath=project.ProjectFile();CopyToBuffer(s.localization.Encode(),config,sizeof(config));}
        ImGui::TextWrapped("Version, default locale, ordered fallback font count/IDs, locale count, then locale/catalog/fallback/font count/IDs. Catalogs and fonts use registered asset IDs.");
        ImGui::InputTextMultiline("Localization configuration",config,sizeof(config),{0,95});
        if(ImGui::Button("Apply localization settings")){ProjectLocalization c;std::string error;if(ProjectLocalization::Parse(config,c,error)&&state.assets&&ValidateLocalizationAssets(c,*state.assets,error)){s.localization=std::move(c);state.previewLocale=s.localization.defaultLocale;state.status="Applied localization; Save project";state.textReload=true;}else state.status=error;}
        for(auto& [locale,entry]:s.localization.locales)ImGui::BulletText("%s : %s",locale.c_str(),entry.catalog.c_str());
        ImGui::TextDisabled("Import .judasloc / .ttf / .otf in Asset Browser. UI source editor offers keys and real text preview.");
    }
    }
    if(page==8){
        ImGui::Checkbox("Basic water (surfaces and volumes)",&s.basicWater);
        ImGui::Checkbox("Water swimming queries",&s.waterSwimming);
        ImGui::TextWrapped("Basic water is enabled by default. Add a Basic water component to an entity for lakes, pools or river segments. Configure its size, colour, visual waves and prescribed current in the Inspector.");
        ImGui::TextWrapped("No particles, conserved-volume solver, pressure solve, flow simulation or rigid-body buoyancy is required. Waves are visual; swimming samples the mean water volume. CharacterMotor games use the existing world.fluidSample query and choose their own swimming controls in JavaScript.");
        ImGui::TextWrapped("Each volume has its own local up, independent of world Y. Continuous spherical oceans are not represented by one box; use appropriately oriented local regions.");
        if(ImGui::Button("Create basic water"))requests.createKind="basic-water";
        ImGui::TextColored({.3f,.8f,.75f,1},"Advanced Fluid Dynamics: %s",AdvancedFluidDynamicsRestriction()?"unavailable in Alpha":(s.advancedFluidDynamics?"enabled (experimental)":"disabled"));
    }
    if(page==9){
        ImGui::TextWrapped("Online and local multiplayer are optional runtime services, configured by project JavaScript through the existing players/network APIs. Hosting, joining, ports, interest and authority remain explicit game policy; opening a project never starts a network listener.");
        ImGui::TextWrapped("See docs/judasjs/network.md and docs/M99_NETWORK.md for supported replication and security limits.");
    }
    if(page==10){
        if (const char* restriction = AdvancedFluidDynamicsRestriction()) {
            ImGui::TextWrapped("%s", restriction);
            bool unavailable = false;
            ImGui::BeginDisabled();
            ImGui::Checkbox("Advanced Fluid Dynamics", &unavailable);
            ImGui::EndDisabled();
            if (s.advancedFluidDynamics) {
                ImGui::TextWrapped("This project still requests advanced fluids. Its authored components are preserved; Play and export are blocked until you disable the request and save.");
                if (ImGui::Button("Disable advanced fluids for this project")) s.advancedFluidDynamics = false;
            }
        } else {
        bool requested=s.advancedFluidDynamics;
        if(ImGui::Checkbox("Advanced Fluid Dynamics",&requested)&&editing){
            if(requested)ImGui::OpenPopup("Enable experimental fluid physics?");
            else s.advancedFluidDynamics=false;
        }
        if(s.advancedFluidDynamics)ImGui::TextColored({1,.65f,.2f,1},"EXPERIMENTAL - performance-intensive; supported combinations only");
        ImGui::TextWrapped("Enables particle/PBF simulation, conserved reservoirs/containers, transfers, dynamic surfaces, fluid buoyancy and drag. Disabled by default. Basic water and its swimming queries work independently.");
        ImGui::TextWrapped("Existing advanced components remain authored when disabled, but do not simulate or allocate particle fields. Save, then restart Play. Old advanced-fluid projects must opt in explicitly. See docs/PROJECT_SETTINGS_AND_WATER.md for numerical, persistence and integration limits.");
        if(ImGui::BeginPopupModal("Enable experimental fluid physics?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextWrapped("Advanced Fluid Dynamics is experimental and potentially performance-intensive.\n"
                "Known contact stability issues can apply excessive forces; unavailable in Alpha.\n"
                "Particle/PBF, conserved-volume dynamics and fluid-body interactions have documented\n"
                "numerical, geometry, streaming and origin-shift limitations.\n"
                "Enable only for projects that need those systems. Basic water does not need it.");
            if(ImGui::Button("Enable experimental fluids")&&editing){s.advancedFluidDynamics=true;ImGui::CloseCurrentPopup();}
            ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        }
        ImGui::Separator();
        ImGui::Checkbox("Legacy gameplay compatibility (historical projects)",&s.legacyGameplay);
        ImGui::TextWrapped("New projects use script-owned gameplay. Compatibility restores historical built-in controls only when explicitly selected.");
    }
    if(page==11){
    {
        bool closure=s.exportAssetPolicy=="closure";if(ImGui::Checkbox("Asset dependency closure (explicit dynamic roots)",&closure))s.exportAssetPolicy=closure?"closure":"all";
        ImGui::TextWrapped("All registered assets is the safe default. Closure keeps selected scene dependencies, script/UI/localization compatibility roots and the runtime IDs below. Declare dynamically loaded prefabs/audio/materials and save-only assets.");
        for(size_t i=0;i<s.runtimeAssets.size();){ImGui::PushID(int(i));ImGui::TextUnformatted(s.runtimeAssets[i].c_str());ImGui::SameLine();if(ImGui::SmallButton("Remove root"))s.runtimeAssets.erase(s.runtimeAssets.begin()+i);else ++i;ImGui::PopID();}
        if(state.assets)for(const auto& [id,asset]:state.assets->Records())if(std::find(s.runtimeAssets.begin(),s.runtimeAssets.end(),id)==s.runtimeAssets.end()){ImGui::PushID(id.c_str());if(ImGui::Selectable(("Include runtime: "+asset.relativePath).c_str()))s.runtimeAssets.push_back(id);ImGui::PopID();}
        bool explicitSet=!s.exportScenes.empty();if(ImGui::Checkbox("Explicit inclusion list",&explicitSet)){if(explicitSet)s.exportScenes=state.sceneFiles;else s.exportScenes.clear();}
        for(const auto& scene:state.sceneFiles){ImGui::PushID(scene.c_str());bool included=s.exportScenes.empty()||std::find(s.exportScenes.begin(),s.exportScenes.end(),scene)!=s.exportScenes.end();bool excluded=std::find(s.excludeScenes.begin(),s.excludeScenes.end(),scene)!=s.excludeScenes.end();ImGui::TextUnformatted(scene.c_str());if(!s.exportScenes.empty()&&ImGui::Checkbox("Include",&included)){s.exportScenes.erase(std::remove(s.exportScenes.begin(),s.exportScenes.end(),scene),s.exportScenes.end());if(included)s.exportScenes.push_back(scene);}ImGui::SameLine();if(ImGui::Checkbox("Exclude",&excluded)){s.excludeScenes.erase(std::remove(s.excludeScenes.begin(),s.excludeScenes.end(),scene),s.excludeScenes.end());if(excluded)s.excludeScenes.push_back(scene);}ImGui::PopID();}
        ImGui::TextWrapped("Startup and world-manifest regions must be included. Registered assets stay available for dynamic ID loading. Save project before export.");
    }
        char destination[4096];CopyToBuffer(state.exportDestination,destination,sizeof(destination));
        if(ImGui::InputText("Package directory",destination,sizeof(destination)))state.exportDestination=destination;
        ImGui::SameLine();if(ImGui::Button("Browse package folder..."))requests.browseExport=true;
        ImGui::TextWrapped("Save project and scene edits first. Export copies the project policy, selected content and runtime dependencies. Choose a destination outside the project.");
        if(ImGui::Button("Export project (Release)"))requests.exportProject=true;
    }
    ImGui::EndDisabled();
    ImGui::EndChild();ImGui::Separator();
    ImGui::BeginDisabled(!editing);
    history.Observe(project);
    ImGui::BeginDisabled(!history.CanUndo());if(ImGui::Button("Undo settings"))history.Undo(project);ImGui::EndDisabled();
    ImGui::SameLine();ImGui::BeginDisabled(!history.CanRedo());if(ImGui::Button("Redo settings"))history.Redo(project);ImGui::EndDisabled();
    if(ImGui::Button("Save project"))requests.saveProject=true;
    ImGui::SameLine();if(ImGui::Button("Run project"))requests.runProject=true;
    ImGui::EndDisabled();
    ImGui::SameLine();ImGui::TextDisabled("Save configuration; restart Play to apply runtime changes.");
    if(!state.runProjectInfo.empty())ImGui::TextWrapped("%s",state.runProjectInfo.c_str());
    ImGui::End();
}
