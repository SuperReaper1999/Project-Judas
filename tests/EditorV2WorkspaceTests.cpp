#include "../third_party/nlohmann/json.hpp"
#include "EngineHost.h"
#include "Project.h"
#include "editor/EditorContext.h"
#include "editor/EditorDocument.h"
#include "editor/EditorPanelRegistry.h"
#include "editor/EditorPanels.h"
#include "editor/EditorPreferences.h"
#include "editor/EditorWorkspace.h"
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
    state.dockingEnabled = true;
    state.showInspector = true;
    EditorWorkspace workspace;
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
    loadedState.dockingEnabled = true;
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
    ImGui::DestroyContext();
    std::printf("SUMMARY %u checks %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
