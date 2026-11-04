#include "EditorCorePanels.h"
#include "EditorConsolePanel.h"
#include "EditorPanelRegistry.h"
#include "EditorPanels.h"
#include "Project.h"
#include "RuntimeInspector.h"
#include "BrowserExportWorkflow.h"
#include "RetargetPanel.h"

void RegisterEditorCorePanels(EditorPanelRegistry& registry){
    RegisterConsolePanel(registry);
    constexpr unsigned author=EditorPhaseMask(EditorPanelPhase::Edit);
    constexpr unsigned observe=author|EditorPhaseMask(EditorPanelPhase::Play)|EditorPhaseMask(EditorPanelPhase::Paused);
    registry.Add({"hierarchy","Hierarchy","Scene",observe,&EditorPanelState::showHierarchy},[](auto& c){DrawHierarchyPanel(c.document,c.panels,c.requests);});
    registry.Add({"inspector","Inspector","Scene",observe,&EditorPanelState::showInspector},[](auto& c){DrawInspectorPanel(c.document,c.panels);});
    registry.Add({"assets","Asset Browser","Content",author,&EditorPanelState::showAssetBrowser},[](auto& c){DrawAssetBrowserPanel(c.document,c.panels,c.requests);});
    registry.Add({"scene-settings","Scene settings","Scene",author,&EditorPanelState::showSceneSettings},[](auto& c){DrawSceneSettingsPanel(c.document,c.panels);});
    registry.Add({"world","World building","World",author,&EditorPanelState::showWorldBuilding},[](auto& c){DrawWorldBuildingPanel(c.document,c.panels);});
    registry.Add({"project","Project Settings","Project",author,&EditorPanelState::showProjectSettings},[](auto& c){DrawProjectSettingsPanel(c.document,c.panels,c.requests);});
    registry.Add({"build","Build and export","Project",author,&EditorPanelState::showBuildExport},[](auto& c){DrawEditorBuildExportPanel(c.panels,c.requests);});
    registry.Add({"retarget","Animation retargeting","Animation",author,&EditorPanelState::showRetarget},[](auto& c){DrawRetargetPanel(c.panels,c.requests);});
    registry.Add({"regions","World regions","World",observe,&EditorPanelState::showWorldRegions},[](auto& c){DrawStreamingPanel(c.document,c.panels,c.requests);});
    registry.Add({"runtime","Runtime Inspector","Debug",observe,&EditorPanelState::showRuntimeInspector},[](auto& c){DrawRuntimeInspector(c.panels,c.requests);});
    registry.Add({"profiler","Profiler","Debug",observe,&EditorPanelState::showProfiler},[](auto& c){DrawProfilerPanel(c.panels);});
    registry.Add({"help","Navigation and shortcuts","Help",observe,&EditorPanelState::showNavigationHelp},[](auto& c){DrawEditorNavigationHelp(c.panels,c.requests);});
    registry.Add({"browser-export","Browser export","Project",author,&EditorPanelState::showBrowserExport},[](auto& c){DrawBrowserExportWindow(c.panels,c.requests);});
}
