#include "../third_party/nlohmann/json.hpp"
#include "EngineHost.h"
#include "Project.h"
#include "editor/EditorContext.h"
#include "editor/EditorDocument.h"
#include "editor/EditorPanelRegistry.h"
#include "editor/EditorPanels.h"
#include "editor/EditorPreferences.h"
#include "editor/EditorWorkspace.h"
#include "editor/EditorPlatformPlacement.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool condition, const char *label) {
    ++checks;
    failures += !condition;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", label);
}
template <class Function> bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception &) {
        return true;
    }
    return false;
}
// This independent panel has no dependency on an application/frame-loop type.
void DrawExtensionPanel(EditorContext &context) {
    ImGui::Begin("Extension proof");
    ImGui::TextUnformatted(context.project.IsLoaded() ? "Project" : "No project");
    ImGui::End();
}
ImVec2 nativePosition{1505, 380};
unsigned nativeQueries = 0;
ImVec2 NativePosition(ImGuiViewport*) { ++nativeQueries; return nativePosition; }

void CheckNativePlacement(ImGuiWindow* window) {
    auto& context = *ImGui::GetCurrentContext();
    auto& platform = ImGui::GetPlatformIO();
    const auto previousMonitors = platform.Monitors;
    const auto previousGetter = platform.Platform_GetWindowPos;
    const auto previousPosition = window->Pos, previousSize = window->Size;
    auto* previousViewport = window->Viewport;
    const auto previousViewportId = window->ViewportId;
    const auto previousOwned = window->ViewportOwned;
    const auto previousNext = context.NextWindowData;
    const auto previousMouse = context.IO.MousePos;
    ImGuiPlatformMonitor monitor;
    monitor.MainPos={0,0}; monitor.MainSize={1920,1080};
    monitor.WorkPos={0,28}; monitor.WorkSize={1920,998};
    platform.Monitors.clear(); platform.Monitors.push_back(monitor);
    platform.Platform_GetWindowPos = NativePosition;
    ImGuiViewportP native;
    native.ID=window->ID; native.Window=window; native.Pos={1590,688}; native.Size={337,646};
    native.PlatformWindowCreated=true; native.PlatformMonitor=0;
    window->Viewport=&native; window->ViewportId=native.ID; window->ViewportOwned=true;
    window->Size=native.Size;
    ImGui::SetWindowPos(window,native.Pos,ImGuiCond_Always);
    context.NextWindowData.ClearFlags();
    const auto active=context.ActiveId;
    ReconcileEditorPlatformPlacement(&native);
    Check(window->Pos.x==1505&&window->Pos.y==380&&native.Pos.x==1505&&native.Pos.y==380,
          "idle native placement adopts the compositor's real position before building hit areas");
    Check(context.ActiveId==active&&window->Size.x==337&&window->Size.y==646,
          "placement reconciliation preserves focus ownership and user sizing");
    context.MovingWindow=window; context.IO.MouseDown[0]=true; context.IO.MousePos={1650,700};
    native.Pos={1590,688}; ImGui::SetWindowPos(window,native.Pos,ImGuiCond_Always);
    ReconcileEditorPlatformPlacement(&native);
    Check(window->Pos.x==1583&&window->Pos.y==380&&native.Pos.x==1583&&native.Pos.y==380,
          "drag projection uses the compositor work area instead of requesting a rejected native position");
    Check(context.IO.MousePos.x==1650&&context.IO.MousePos.y==700&&context.IO.MouseDown[0]&&
              context.MovingWindow==window&&context.ActiveId==active,
          "constrained drag retains the real pointer, held button and docking payload owner");
    native.Pos={800,200}; ImGui::SetWindowPos(window,native.Pos,ImGuiCond_Always);
    ReconcileEditorPlatformPlacement(&native);
    Check(window->Pos.x==800&&window->Pos.y==200,"valid native positions are not snapped or relocated");
    ImGuiPlatformMonitor left=monitor;left.MainPos={-1280,0};left.MainSize={1280,1024};
    left.WorkPos={-1280,25};left.WorkSize={1280,999};platform.Monitors.push_back(left);
    context.IO.MousePos={-100,200};native.Pos={-150,700};ImGui::SetWindowPos(window,native.Pos,ImGuiCond_Always);
    ReconcileEditorPlatformPlacement(&native);
    Check(window->Pos.x==-337&&window->Pos.y==378,
          "native drags choose the pointer's monitor including negative desktop coordinates");
    context.MovingWindow=nullptr;context.IO.MouseDown[0]=false;
    native.Pos={800,200};ImGui::SetWindowPos(window,{50,60},ImGuiCond_Always);
    ReconcileEditorPlatformPlacement(&native);
    Check(window->Pos.x==50&&window->Pos.y==60,
          "explicit application/off-screen recovery placement is not overwritten before admission");
    native.Pos={800,200};ImGui::SetWindowPos(window,native.Pos,ImGuiCond_Always);
    context.NextWindowData.HasFlags|=ImGuiNextWindowDataFlags_HasPos;
    context.NextWindowData.PosVal={70,90};
    ReconcileEditorPlatformPlacement(&native);
    Check(window->Pos.x==800&&window->Pos.y==200&&context.NextWindowData.PosVal.x==70,
          "ordinary explicit next-window placement retains priority");
    ImGuiDockNode host(window->ID);window->DockNodeAsHost=&host;
    ReconcileEditorPlatformPlacement(&native);
    Check(window->Pos.x==1505&&window->Pos.y==380&&context.NextWindowData.PosVal.x==1505&&
              context.NextWindowData.PosVal.y==380,
          "floating dock-host pending placement agrees with its admitted native position");
    window->DockNodeAsHost=nullptr;context.NextWindowData.ClearFlags();
    native.PlatformWindowCreated=false;const auto queries=nativeQueries;
    ReconcileEditorPlatformPlacement(&native);
    Check(nativeQueries==queries,"new viewports are never queried before native window creation");
    const auto primary=ImGui::GetMainViewport()->Pos;
    ReconcileEditorPlatformPlacement(ImGui::GetMainViewport());
    Check(ImGui::GetMainViewport()->Pos.x==primary.x&&ImGui::GetMainViewport()->Pos.y==primary.y,
          "main editor viewport is never constrained or repositioned by panel admission");
    native.PlatformWindowCreated=false;
    platform.Platform_GetWindowPos=previousGetter;platform.Monitors=previousMonitors;
    window->Viewport=previousViewport;window->ViewportId=previousViewportId;window->ViewportOwned=previousOwned;
    window->Size=previousSize;ImGui::SetWindowPos(window,previousPosition,ImGuiCond_Always);
    context.NextWindowData=previousNext;context.IO.MousePos=previousMouse;
}
} // namespace

int main() {
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.DisplaySize = {1200, 800};
    io.DeltaTime = 1.f / 60;
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    EditorDocument document;
    Project project;
    EditorPanelState state;
    EditorRequests requests;
    EngineHost host;
    EditorContext context{document, project, state, requests, host, host.Assets()};
    EditorPanelRegistry registry;
    std::vector<int> drawn;
    registry.Add({"extension", "Extension proof", "Tools", kEditorAllPanelPhases}, [&](EditorContext &value) {
        drawn.push_back(1);
        DrawExtensionPanel(value);
    });
    registry.Add({"authoring", "Authoring only", "Scene", EditorPhaseMask(EditorPanelPhase::Edit),
                  &EditorPanelState::showInspector},
                 [&](EditorContext &) { drawn.push_back(2); });
    Check(registry.Contains("extension") && registry.Descriptors().size() == 2,
          "independent module is admitted through registration without application code");
    Check(Throws([&] { registry.Add({"extension", "Duplicate", "Tools"}, [](auto &) {}); }) &&
              registry.Descriptors().size() == 2,
          "duplicate panel rejects without replacing the installed implementation");
    Check(Throws([&] { registry.Add({"", "Unnamed", "Tools"}, [](auto &) {}); }) &&
              Throws([&] { registry.Add({"bad", "Bad phase", "Tools", 32}, [](auto &) {}); }),
          "invalid panel declarations fail before they can enter drawing");
    ImGui::NewFrame();
    registry.DrawAll(context);
    ImGui::Render();
    Check(drawn == std::vector<int>{1, 2}, "registered visible panels draw in deterministic order");
    drawn.clear();
    state.showInspector = false;
    ImGui::NewFrame();
    registry.DrawAll(context);
    ImGui::Render();
    Check(drawn == std::vector<int>{1}, "hidden panel performs no callback work");
    drawn.clear();
    state.showInspector = true;
    context.phase = EditorPanelPhase::DebugStopped;
    ImGui::NewFrame();
    registry.DrawAll(context);
    ImGui::Render();
    Check(drawn == std::vector<int>{1}, "debugger-stopped phase excludes authoring callback");
    bool mutationRejected = false;
    registry.Add({"mutation", "Mutation probe", "Tools"}, [&](auto &) {
        mutationRejected = Throws([&] { registry.Add({"during-draw", "Unsafe", "Tools"}, [](auto &) {}); });
    });
    ImGui::NewFrame();
    registry.DrawAll(context);
    ImGui::Render();
    Check(mutationRejected && !registry.Contains("during-draw"),
          "registration during drawing cannot invalidate the active registry pass");

    EditorPreferences preferences;

    state.showInspector = true;
    EditorWorkspace workspace;
    bool showWorld = false;
    bool placeWorld = false;
    auto frame = [&] {
        preferences.ApplyPending(state);
        ImGui::NewFrame();
        workspace.BeginFrame(state, preferences);
        if (BeginEditorPanel("Hierarchy", &state.showHierarchy, state))
            ImGui::TextUnformatted("Objects");
        ImGui::End();
        if (BeginEditorPanel("Inspector", &state.showInspector, state))
            ImGui::TextUnformatted("Selected object");
        ImGui::End();
        if (BeginEditorPanel("Scene###Judas.Scene", &state.showSceneView, state))
            ImGui::TextUnformatted("Scene contents");
        ImGui::End();
        if (BeginEditorPanel("Game###Judas.Game", &state.showGameView, state))
            ImGui::TextUnformatted("Play preview");
        ImGui::End();
        if (BeginEditorPanel("Asset Browser", &state.showAssetBrowser, state))
            ImGui::TextUnformatted("Project content");
        ImGui::End();
        if (BeginEditorPanel("Console", &state.showConsole, state))
            ImGui::TextUnformatted("Script output");
        ImGui::End();
        if (showWorld) {
            if (placeWorld) {
                ImGui::SetNextWindowPos({420, 220});
                ImGui::SetNextWindowSize({360, 300});
                placeWorld = false;
            }
            if (ImGui::Begin("World building", &showWorld)) {
                for (int i = 0; i < 60; ++i) ImGui::Text("Recipe property %d", i);
            }
            ImGui::End();
        }
        ImGui::Render();
    };
    frame();
    frame();
    auto *hierarchy = ImGui::FindWindowByName("Hierarchy");
    auto *inspector = ImGui::FindWindowByName("Inspector");
    auto *scene = ImGui::FindWindowByName("Scene###Judas.Scene");
    Check(hierarchy && inspector && scene && hierarchy->DockId && inspector->DockId && scene->DockId &&
              hierarchy->DockId != inspector->DockId,
          "real hierarchy, inspector and scene windows dock into independent regions");
    std::string error;
    const auto *game = ImGui::FindWindowByName("Game###Judas.Game");
    Check(game && scene && game->DockId == scene->DockId && scene->DockTabIsVisible,
          "Scene and Game share the central tabs with Scene visible initially");
    const auto *assets = ImGui::FindWindowByName("Asset Browser");
    const auto *console = ImGui::FindWindowByName("Console");
    Check(assets && console && assets->DockId == console->DockId && assets->DockTabIsVisible,
          "fresh Scene workspace opens content instead of the last-created Console tab");
    Check(preferences.SaveLayout("Docked scene", state, false, error),
          "named layout captures actual dock tree");
    const auto serialized = preferences.Serialize(state);
    const auto data = nlohmann::json::parse(serialized);
    Check(data.at("version") == 3 && data.at("current").at("dockVersion") == 1 &&
              data.at("current").at("windows").get<std::string>().find("[Docking][Data]") !=
                  std::string::npos,
          "versioned workspace contains real ImGui docking state");
    preferences.appearance.secondaryAccent = {.7f, .2f, .6f};
    EditorPreferences loaded;
    EditorPanelState loadedState;

    Check(loaded.Load(preferences.Serialize(state), loadedState, error) && loaded.layouts.size() == 1 &&
              loaded.appearance.secondaryAccent == preferences.appearance.secondaryAccent,
          "fresh owner restores named layout and editable secondary accent");
    Check(preferences.QueueWorkspace("Scripting"), "built-in workspace switch is available");
    frame();
    frame();
    Check(preferences.WorkspaceName() == "Scripting" && state.showScriptTools && state.showHierarchy,
          "workspace switch reveals relevant tools without modifying the document");
    Check(!preferences.QueueWorkspace("Invented workflow") && preferences.WorkspaceName() == "Scripting",
          "unknown workspace is rejected without resetting the current arrangement");
    auto broken = nlohmann::json::parse(preferences.Serialize(state));
    broken["current"]["workspace"] = "Unknown";
    Check(!loaded.Load(broken.dump(), loadedState, error) && loaded.layouts.size() == 1,
          "invalid workspace preference rejects transactionally");
    Check(preferences.QueueWorkspace("Audio"), "Audio workspace uses the existing settings workflow");
    frame();
    Check(state.showProjectSettings && state.projectSettingsPage == 4,
          "Audio workspace exposes genuine project Audio controls");
    preferences.ResetLayout();
    frame();
    frame();
    Check(preferences.WorkspaceName() == "Scene" && state.showHierarchy && state.showInspector &&
              !state.showScriptTools,
          "reset restores discoverable core panels and default task workspace");

    const float regularInspectorWidth = inspector->Size.x;
    preferences.appearance.scale = 1.5f;
    preferences.RefreshAppearance();
    preferences.ResetLayout();
    frame();
    frame();
    Check(inspector->Size.x > regularInspectorWidth * 1.3f && scene->Size.x > 200.f,
          "fresh large-text layout widens property rails while retaining a usable central view");
    const float scaledInspectorWidth = inspector->Size.x;
    frame();
    Check(inspector->Size.x == scaledInspectorWidth,
          "ordinary frames preserve the chosen dock tree instead of continuously resizing panels");

    ImGui::NewFrame();
    ImGui::SetNextWindowPos({50, 60}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({300, 200}, ImGuiCond_Always);
    ImGui::Begin("Recoverable tool");
    ImGui::TextUnformatted("Tool");
    ImGui::End();
    auto *floating = ImGui::FindWindowByName("Recoverable tool");
    ImGui::SetWindowPos(floating, {50000, 50000}, ImGuiCond_Always);
    workspace.RecoverFloatingWindows();
    Check(floating->Pos.x < io.DisplaySize.x && floating->Pos.y < io.DisplaySize.y,
          "tool saved on a disconnected monitor is recovered onto the visible workspace");
    ImGui::Render();
    Check(!document.IsDirty() && !state.runtime && !project.IsLoaded(),
          "docking, workspace changes and recovery never create a game world or edit scene content");
    showWorld = placeWorld = true;
    frame();frame();
    auto* world = ImGui::FindWindowByName("World building");
    ImGui::DockBuilderDockWindow("World building", scene->DockId);
    frame();frame();
    Check(world->DockId == scene->DockId && world->DockTabIsVisible,
          "World building can join the scene's real dock tabs");
    const auto tab = world->DC.DockTabItemRect;
    io.AddMousePosEvent(tab.Min.x + 30, tab.GetCenter().y);
    frame();
    io.AddMouseButtonEvent(0, true);frame();
    io.AddMousePosEvent(tab.Min.x + 60, tab.Min.y + 95);frame();frame();
    const auto dragged = world->Pos;
    for (int i=0;i<12;++i)frame();
    Check(world->Pos.x==dragged.x&&world->Pos.y==dragged.y,
          "held docking drag has stable geometry while the pointer is stationary");
    io.AddMouseButtonEvent(0, false);frame();frame();
    Check(!io.MouseDown[0]&&!ImGui::GetCurrentContext()->MovingWindow,
          "release after docking/undocking leaves no stuck mouse move");
    std::printf("WORLD_DOCK_TRACE dock=%u pos=%.1f,%.1f size=%.1f,%.1f title=%.1f active=%u\n",
                world->DockId,world->Pos.x,world->Pos.y,world->Size.x,world->Size.y,world->TitleBarHeight,
                ImGui::GetCurrentContext()->ActiveId);
    CheckNativePlacement(world);
    io.AddMousePosEvent(world->Pos.x+world->Size.x-ImGui::GetStyle().FramePadding.x-ImGui::GetFontSize()*.5f,
                        world->Pos.y+world->TitleBarHeight*.5f);
    frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();frame();
    Check(!showWorld,"World building closes immediately after drag without a scrollbar/focus recovery click");
    ImGui::DestroyContext();
    std::printf("SUMMARY %u checks %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
