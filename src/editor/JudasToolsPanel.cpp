#include "JudasToolsPanel.h"
#include "AssetDatabase.h"
#include "EditorContext.h"
#include "EditorDocument.h"
#include "EditorPanels.h"
#include "EditorTheme.h"
#include "EditorWorkspace.h"
#include "EditorVisuals.h"
#include "ScriptTools.h"
#include <algorithm>
#include <filesystem>

namespace {
void OpenScript(EditorContext &context, const std::string &id) {
    context.panels.browserSelection = id;
    context.panels.editScriptAsset = id;
    context.panels.showScriptTools = true;
}

bool FullButton(const char *label, EditorVisuals::Icon icon, bool primary = false) {
    return EditorVisuals::ActionButton(label, icon, primary, {-1, 0});
}
} // namespace

void JudasToolsPanel::Draw(EditorContext &context, std::unique_ptr<ScriptTools> &scripts) {
    const auto accent = EditorSecondaryAccent();
    auto title = ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive);
    title.x = title.x * .8f + accent.x * .2f;
    title.y = title.y * .8f + accent.y * .2f;
    title.z = title.z * .8f + accent.z * .2f;
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, title);
    auto border = accent;
    border.w = .65f;
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + std::max(20.f, viewport->WorkSize.x - 365.f),
                             viewport->WorkPos.y + std::max(100.f, viewport->WorkSize.y - 490.f)},
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({340, 470}, ImGuiCond_FirstUseEver);
    if (BeginEditorPanel("Judas Tools", &context.panels.showJudasTools, context.panels)) {
        const auto header = ImGui::GetCursorScreenPos();
        const auto headerWidth = ImGui::GetContentRegionAvail().x;
        const auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
        const float headerHeight = lineHeight * 2 + 14;
        auto *draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(header, {header.x + headerWidth, header.y + headerHeight},
                            ImGui::GetColorU32(EditorVisuals::SurfaceColor(.035f)), 5);
        draw->AddRectFilled(header, {header.x + 3, header.y + headerHeight}, ImGui::GetColorU32(accent), 2);
        EditorVisuals::BrandMark(draw, {header.x + 12, header.y + 14}, lineHeight * 1.25f, context.panels.brandImage);
        ImGui::SetCursorScreenPos({header.x + lineHeight * 1.25f + 24, header.y + 7});
        ImGui::PushFont(EditorVisuals::HeadingFont());
        ImGui::TextColored(accent, "Judas Tools");
        ImGui::PopFont();
        ImGui::SetCursorScreenPos({header.x + lineHeight * 1.25f + 24, header.y + lineHeight + 10});
        ImGui::TextDisabled("Your project, within reach");
        ImGui::SetCursorScreenPos({header.x, header.y + headerHeight + ImGui::GetStyle().ItemSpacing.y});
        if (ImGui::BeginTabBar("##JudasToolTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
            if (ImGui::BeginTabItem("Scripts")) {
                Scripts(context, scripts);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Build")) {
                Build(context);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Utilities")) {
                Utilities(context);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

void JudasToolsPanel::Scripts(EditorContext &context, std::unique_ptr<ScriptTools> &scripts) {
    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##ScriptSearch", "Search project scripts...", filter.InputBuf,
                                 sizeof(filter.InputBuf)))
        filter.Build();
    const auto *selected = context.assets.Find(selectedScript);
    if (!selected || selected->type != AssetType::Script || selected->missing) {
        selectedScript.clear();
        selected = nullptr;
    }
    const float footer = ImGui::GetFrameHeightWithSpacing() * 4 + ImGui::GetTextLineHeightWithSpacing() * 2;
    if (ImGui::BeginChild(
            "##ProjectScripts",
            {0, std::max(ImGui::GetFrameHeight() * 2, ImGui::GetContentRegionAvail().y - footer)},
            ImGuiChildFlags_Borders)) {
        unsigned count = 0;
        for (const auto &[id, asset] : context.assets.Records()) {
            if (asset.type != AssetType::Script || asset.missing ||
                !filter.PassFilter(asset.relativePath.c_str()))
                continue;
            ++count;
            ImGui::PushID(id.c_str());
            const auto name = std::filesystem::path(asset.relativePath).filename().string();
            const auto label = "     " + name;
            if (ImGui::Selectable(label.c_str(), selectedScript == id, ImGuiSelectableFlags_AllowDoubleClick,
                                  {0, ImGui::GetFrameHeight()})) {
                selectedScript = id;
                context.panels.browserSelection = id;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    OpenScript(context, id);
            }
            const auto row = ImGui::GetItemRectMin();
            const auto rowSize = ImGui::GetItemRectSize();
            const auto iconSize = ImGui::GetFontSize();
            EditorVisuals::DrawIcon(
                ImGui::GetWindowDrawList(), EditorVisuals::Icon::Script,
                {row.x + 4, row.y + (rowSize.y - iconSize) * .5f}, iconSize,
                ImGui::GetColorU32(selectedScript == id ? EditorSecondaryAccent()
                                                        : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s%s\nDouble-click to edit", asset.relativePath.c_str(),
                                  asset.readOnly ? " (read-only)" : "");
            ImGui::PopID();
        }
        if (!count)
            ImGui::TextWrapped(filter.IsActive()
                                   ? "No matching scripts."
                                   : "No project scripts yet. Select an object and use Inspector > Scripts.");
    }
    ImGui::EndChild();
    selected = context.assets.Find(selectedScript);
    ImGui::BeginDisabled(!selected);
    if (FullButton(selected && selected->readOnly ? "View selected script" : "Edit selected script",
                   EditorVisuals::Icon::Script, true) &&
        selected)
        OpenScript(context, selectedScript);
    ImGui::EndDisabled();
    const bool canReload = context.runtime && scripts && !scripts->editing;
    ImGui::BeginDisabled(!canReload);
    if (FullButton("Reload saved scripts", EditorVisuals::Icon::Rotate))
        scripts->reloadRequested = true;
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(
            "%s",
            !context.runtime ? "Press Play before reloading the running world."
            : scripts && scripts->editing
                ? "Save or discard source edits before reloading."
                : "Deliberately reload at a callback boundary, using the current state-retention option.");
    if (FullButton("Open scripting tools", EditorVisuals::Icon::Script))
        context.panels.showScriptTools = true;
    if (FullButton("Check project sources", EditorVisuals::Icon::File)) {
        if (!scripts)
            scripts = std::make_unique<ScriptTools>();
        scripts->projectDiagnostics = ScriptSystem::CheckProjectSources(context.assets);
        scripts->status = scripts->projectDiagnostics.empty()
                              ? "Scripts compile and link. Press Play to test gameplay."
                              : "Source errors found. Open the scripting tools for details.";
        context.panels.showScriptTools = true;
    }
    if (scripts && !scripts->status.empty())
        ImGui::TextWrapped("%s", scripts->status.c_str());
}

void JudasToolsPanel::Build(EditorContext &context) {
    ImGui::Spacing();
    ImGui::TextWrapped("Test this scene or package your project using the existing build workflow.");
    ImGui::Spacing();
    ImGui::BeginDisabled(context.panels.mode != EditorMode::Edit);
    if (FullButton("Build / export project", EditorVisuals::Icon::Cube, true))
        context.panels.showBuildExport = true;
    if (FullButton("Play current scene", EditorVisuals::Icon::Play))
        context.requests.play = true;
    if (FullButton("Run project standalone", EditorVisuals::Icon::Play))
        context.requests.runProject = true;
    if (FullButton("Project settings", EditorVisuals::Icon::Settings))
        context.panels.showProjectSettings = true;
    ImGui::EndDisabled();
    if (context.panels.mode != EditorMode::Edit)
        ImGui::TextWrapped("Stop Play to build or change project settings.");
}

void JudasToolsPanel::Utilities(EditorContext &context) {
    ImGui::Spacing();
    ImGui::BeginDisabled(context.panels.mode != EditorMode::Edit);
    if (FullButton("Asset Browser", EditorVisuals::Icon::Folder))
        context.panels.showAssetBrowser = true;
    ImGui::EndDisabled();
    ImGui::BeginDisabled(context.document.Selected() == kInvalidSceneObjectId);
    if (FullButton("Selected object's scripts", EditorVisuals::Icon::Person)) {
        context.panels.showInspector = true;
        context.panels.revealComponent = "Scripts";
    }
    ImGui::EndDisabled();
    if (FullButton("Console", EditorVisuals::Icon::File))
        context.panels.showConsole = true;
    if (FullButton("Profiler", EditorVisuals::Icon::Settings))
        context.panels.showProfiler = true;
    if (FullButton("Runtime Inspector", EditorVisuals::Icon::Search))
        context.panels.showRuntimeInspector = true;
    if (FullButton("Navigation and shortcuts", EditorVisuals::Icon::Move))
        context.panels.showNavigationHelp = true;
    if (FullButton("Editor workflow guide", EditorVisuals::Icon::File))
        context.requests.openWorkflowGuide = true;
}
