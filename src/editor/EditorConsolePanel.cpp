#include "EditorConsolePanel.h"
#include "EditorPanelRegistry.h"
#include "EditorPanelWidgets.h"
#include "EditorPanels.h"
#include "EditorWorkspace.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "imgui.h"
#include <algorithm>

// Console state belongs to this panel; selecting a message only requests the
// existing script editor. It never evaluates gameplay or invokes a second VM.
struct EditorConsoleState {
    std::string search;
    bool errorsOnly = false;
};

void DrawConsolePanel(EditorPanelState &state) {
    if (!state.showConsole)
        return;
    if (!BeginEditorPanel("Console", &state.showConsole, state)) {
        ImGui::End();
        return;
    }
    if (!state.console)
        state.console = std::make_shared<EditorConsoleState>();
    auto &console = *state.console;
    char search[256];
    EditorPanelWidgets::CopyToBuffer(console.search, search, sizeof(search));
    ImGui::SetNextItemWidth(std::max(100.f, ImGui::GetContentRegionAvail().x - 140.f));
    if (ImGui::InputTextWithHint("##consoleSearch", "Search output and script errors...", search,
                                 sizeof(search)))
        console.search = search;
    ImGui::SameLine();
    ImGui::Checkbox("Errors only", &console.errorsOnly);
    ImGuiTextFilter filter(console.search.c_str());
    auto *scripts = state.runtime ? state.runtime->Scripts() : nullptr;
    if (!scripts) {
        ImGui::TextWrapped("Play the scene to inspect gameplay output. Select a script asset to edit or "
                           "check it in Scripting.");
        if (ImGui::Button("Open Scripting tools"))
            state.showScriptTools = true;
        ImGui::End();
        return;
    }
    ImGui::TextDisabled("%zu diagnostics / %zu captured output lines", scripts->Diagnostics().size(),
                        scripts->ConsoleMessages().size());
    if (scripts->ConsoleMessages().empty()) {
        ImGui::TextWrapped("Script output capture uses the existing Scripting debugger. Errors remain "
                           "available here without capture.");
        if (ImGui::SmallButton("Open Scripting capture controls"))
            state.showScriptTools = true;
    }
    ImGui::BeginChild("Console messages", {0, 0});
    for (const auto &diagnostic : scripts->Diagnostics()) {
        const auto *asset = state.assets ? state.assets->Find(diagnostic.asset) : nullptr;
        const auto path = asset ? asset->relativePath : diagnostic.asset;
        const auto label = path + " | " + diagnostic.callback + " | " + diagnostic.message;
        if (!filter.PassFilter(label.c_str()))
            continue;
        ImGui::PushID(&diagnostic);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, .48f, .40f, 1));
        if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) && asset) {
            state.editScriptAsset = asset->id;
            state.showScriptTools = true;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Click to open the registered source.\n%s", diagnostic.message.c_str());
        ImGui::PopID();
    }
    if (!console.errorsOnly) {
        for (const auto &message : scripts->ConsoleMessages())
            if (filter.PassFilter(message.c_str()))
                ImGui::TextUnformatted(message.c_str());
    }
    ImGui::EndChild();
    ImGui::End();
}

void RegisterConsolePanel(EditorPanelRegistry& registry) {
    registry.Add({"console", "Console", "Debug", kEditorAllPanelPhases, &EditorPanelState::showConsole},
                 [](EditorContext& context) { DrawConsolePanel(context.panels); });
}
