#include "ProjectFeaturePolicy.h"
#include "editor/EditorPanels.h"
#include "editor/BrowserExportWorkflow.h"
#include "editor/EditorDocument.h"
#include "Project.h"
#include "editor/ProjectSettingsHistory.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <fstream>
namespace {
int checks=0,failures=0;
void Check(bool v,const char* what){++checks;failures+=!v;std::cout<<(v?"PASS ":"FAIL ")<<what<<'\n';}
}
int main(){
    const auto root=std::filesystem::temp_directory_path()/("judas-water-settings-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Project project;std::string error;
    Check(Project::CreateNew(root.string(),"Settings workflow",project,error),"create ordinary project");
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1280,800};io.DeltaTime=1.f/60;
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    EditorPanelState state;state.project=&project;state.showProjectSettings=true;state.mode=EditorMode::Edit;EditorRequests requests;EditorDocument doc;
    auto frame=[&](){requests={};ImGui::NewFrame();ImGui::SetNextWindowPos({20,20},ImGuiCond_Always);DrawProjectSettingsPanel(doc,state,requests);ImGui::Render();};
    auto child=[&](const char* component)->ImGuiWindow*{for(auto* w:ImGui::GetCurrentContext()->Windows)if(w->ParentWindow&&std::string(w->ParentWindow->Name)=="Project Settings"&&std::string(w->Name).find(std::string("/")+component+"_")!=std::string::npos)return w;return nullptr;};
    auto activate=[&](ImGuiWindow* w,const char* label){Check(w!=nullptr,"actual settings window available");if(w){ImGui::ActivateItemByID(w->GetID(label));frame();frame();}};
    frame();frame();
    activate(child("Categories"),"Experimental");
    Check(!project.Settings().advancedFluidDynamics,"fresh project leaves advanced fluids disabled");
    if (AdvancedFluidDynamicsRestriction()) {
        activate(child("Settings"),"Advanced Fluid Dynamics");
        Check(!project.Settings().advancedFluidDynamics&&!ImGui::IsPopupOpen("Enable experimental fluid physics?",ImGuiPopupFlags_AnyPopupId),
              "Alpha cannot enable fluids or open the old opt-in warning");
        // Simulate a real pre-existing project request; editing must preserve it
        // until the operator explicitly disables it, without offering activation.
        project.Settings().advancedFluidDynamics=true;frame();
        Check(project.Settings().advancedFluidDynamics,"Alpha does not silently rewrite existing authored requests");
    } else {
    activate(child("Settings"),"Advanced Fluid Dynamics");
    Check(!project.Settings().advancedFluidDynamics&&ImGui::IsPopupOpen("Enable experimental fluid physics?",ImGuiPopupFlags_AnyPopupId),"checkbox opens explicit warning without enabling the solver");
    activate(ImGui::FindWindowByName("Enable experimental fluid physics?"),"Cancel");
    Check(!project.Settings().advancedFluidDynamics,"warning cancel does not opt in");
    activate(child("Settings"),"Advanced Fluid Dynamics");
    activate(ImGui::FindWindowByName("Enable experimental fluid physics?"),"Enable experimental fluids");
    Check(project.Settings().advancedFluidDynamics,"warning confirmation explicitly opts in");
    }
    Check(project.IsDirty()&&!doc.IsDirty(),"project dirty state is independent of clean scene ownership");
    activate(ImGui::FindWindowByName("Project Settings"),"Save project");
    // Queue once more and inspect that exact frame's ordinary editor request.
    ImGui::ActivateItemByID(ImGui::FindWindowByName("Project Settings")->GetID("Save project"));frame();
    Check(requests.saveProject,"Save project button emits the real editor command");
    Check(project.Save(error)&&!project.IsDirty(),"ordinary project save resets its own dirty baseline");Project reopened;
    Check(reopened.Load(project.ProjectFile(),error)&&reopened.Settings().advancedFluidDynamics,"fresh editor session retains explicit opt-in in named project data");
    state.mode=EditorMode::Play;activate(child("Settings"),"Advanced Fluid Dynamics");Check(project.Settings().advancedFluidDynamics,"running world settings cannot be changed through disabled controls");
    state.mode=EditorMode::Edit;activate(child("Settings"),AdvancedFluidDynamicsRestriction()?"Disable advanced fluids for this project":"Advanced Fluid Dynamics");Check(!project.Settings().advancedFluidDynamics,"explicit opt-out applies to next world");
    activate(child("Categories"),"Water");Check(project.Settings().basicWater&&project.Settings().waterSwimming,"basic water and swimming remain independent of opt-out");
    activate(child("Settings"),"Create basic water");
    ImGui::ActivateItemByID(child("Settings")->GetID("Create basic water"));frame();
    Check(requests.createKind=="basic-water","Water page uses ordinary scene create command");
    Check(project.IsDirty(),"unsaved opt-out is detected");
    Check(project.DiscardSettings(error)&&!project.IsDirty()&&project.Settings().advancedFluidDynamics,"discard returns exactly to saved project settings");
    ProjectSettingsHistory history;history.Observe(project);project.Settings().basicWater=false;history.Observe(project);
    Check(history.CanUndo()&&project.IsDirty(),"editor settings edit creates independent undo history");history.Undo(project);
    Check(project.Settings().basicWater&&!project.IsDirty()&&history.CanRedo(),"settings undo returns to saved baseline without dirtying scene");history.Redo(project);
    Check(!project.Settings().basicWater&&project.IsDirty(),"settings redo reapplies project edit");
    activate(child("Categories"),"Export");
    Check(!state.browserExport,"browser controls are absent without per-launch opt-in");
    state.experimentalBrowserSupport=true;frame();frame();
    Check(state.browserExport!=nullptr,"explicit opt-in exposes browser workflow");
    ImGui::ActivateItemByID(child("Settings")->GetID("Build browser game"));frame();
    Check(requests.exportBrowser&&!requests.exportProject,"browser build emits its own saved-content editor action");
    state.mode=EditorMode::Play;
    ImGui::ActivateItemByID(child("Settings")->GetID("Build browser game"));frame();
    Check(!requests.exportBrowser,"browser export cannot launch from Play");
    state.mode=EditorMode::Edit;
    {std::ofstream external(project.ProjectFile(),std::ios::app);external<<"# external edit\n";}
    Check(!project.Save(error)&&project.IsDirty()&&error.find("externally")!=std::string::npos,"failed external-conflict save retains unsaved settings");
    ImGui::DestroyContext();std::filesystem::remove_all(root);
    std::cout<<"ProjectSettingsEditorTests: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
