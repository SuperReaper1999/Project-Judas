#include "ComponentEditors.h"
#include "EditorPanelWidgets.h"
#include "EditorPanels.h"
#include "EditorWidgets.h"
#include "EditorWorkspace.h"
#include "EditorVisuals.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <set>
#include <sstream>

using namespace EditorPanelWidgets;
#include "EditorAssetActions.h"
#include "EditorCreationMenu.h"
#include "PerformanceProfiler.h"

namespace {
EditorVisuals::Icon HierarchyIcon(const SceneObject &object) {
    using Icon = EditorVisuals::Icon;
    if (object.characterMotor || object.playerStart)
        return Icon::Person;
    if (object.renderCamera)
        return Icon::Camera;
    if (object.light)
        return Icon::Light;
    if (object.planet || object.gravity || object.celestial)
        return Icon::Globe;
    if (object.ui)
        return Icon::UI;
    if (object.audioEmitter || object.audioListener)
        return Icon::Audio;
    if (object.render && object.render->shape == SceneShape::Sprite)
        return Icon::Image;
    if (!object.scripts.empty() && !object.render)
        return Icon::Script;
    if (!object.render && !object.body)
        return Icon::Folder;
    return Icon::Cube;
}
} // namespace

void DrawHierarchyPanel(EditorDocument &doc, EditorPanelState &state, EditorRequests &requests) {
    if (state.dockingEnabled ? !state.showHierarchy : !state.workspace.showRails)
        return;
    JUDAS_PROFILE_SCOPE("Editor hierarchy");
    if (!BeginEditorPanel("Hierarchy", state.dockingEnabled ? &state.showHierarchy : nullptr, state,
                          &state.workspace.hierarchy)) {
        ImGui::End();
        return;
    }
    const bool editing = state.mode == EditorMode::Edit;
    Scene &scene = doc.GetScene();
    HandleHierarchyRenameShortcut(doc, state);
    const float scale = ImGui::GetIO().FontGlobalScale;
    ImGui::BeginDisabled(!editing);
    if (EditorVisuals::ActionButton("Add object", EditorVisuals::Icon::Plus, true, {0, 30.f * scale}))
        ImGui::OpenPopup("hierarchyCreate");
    ToolbarHint("Create a root object. Right-click an existing row to create a child.");
    if (ImGui::BeginPopup("hierarchyCreate")) {
        DrawEditorCreationItems(state, requests);
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    ImGui::Spacing();
    char search[256];
    CopyToBuffer(state.hierarchySearch, search, sizeof(search));
    ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - 30.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {25.f * scale, 5.f * scale});
    if (ImGui::InputTextWithHint("##hierarchySearch", "Search names, tags, folders...", search,
                                 sizeof(search)))
        state.hierarchySearch = search;
    ImGui::PopStyleVar();
    const auto searchPosition = ImGui::GetItemRectMin();
    EditorVisuals::DrawIcon(ImGui::GetWindowDrawList(), EditorVisuals::Icon::Search,
                            {searchPosition.x + 6.f * scale, searchPosition.y + 6.f * scale}, 13.f * scale,
                            ImGui::GetColorU32(ImGuiCol_TextDisabled));
    ImGui::SameLine();
    if (ImGui::SmallButton("X##clearSearch"))
        state.hierarchySearch.clear();
    ToolbarHint("Clear hierarchy search");
    ImGui::Spacing();
    ImGui::PushFont(EditorVisuals::HeadingFont());
    EditorVisuals::IconText(EditorVisuals::Icon::Scene,
                            scene.Settings().name.empty() ? "Scene" : scene.Settings().name.c_str(), 14);
    ImGui::PopFont();
    ImGui::TextDisabled("%zu objects  /  %zu selected", scene.Objects().size(), doc.Selection().size());
    ImGui::Separator();
    SceneObjectId toDelete = kInvalidSceneObjectId;
    SceneObjectId reparentTo = kInvalidSceneObjectId;
    bool reparent = false;
    int move = 0;
    SceneObjectId moveId = kInvalidSceneObjectId;
    ImGui::BeginChild("list", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()));
    CategoryMask matchingTags = 0;
    if (state.project && !state.hierarchySearch.empty())
        for (const auto &[id, name] : state.project->Settings().classification.tags.names)
            if (name.find(state.hierarchySearch) != std::string::npos)
                matchingTags |= CategoryBit(id);
    const auto &rows = doc.HierarchyRows(state.hierarchySearch, matchingTags);
    // ImGui snaps item positions to pixels. A fractional row pitch would
    // round differently above the viewport and change the scroll extent.
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float rowPitch =
        std::floor(std::max(ImGui::GetTextLineHeightWithSpacing(), 23.f * scale) + spacing);
    const float rowHeight = rowPitch - spacing;
    std::size_t submitted = 0;
    const auto drawRow = [&](const EditorHierarchyRow &row) {
        ++submitted;
        const SceneObject &o = scene.Objects()[row.sceneIndex];
        ImGui::PushID(static_cast<int>(o.id));
        const bool selected = doc.IsSelected(o.id);
        if (editing && state.renamingId == o.id) {
            char buffer[256];
            CopyToBuffer(state.renameBuffer, buffer, sizeof(buffer));
            ImGui::SetKeyboardFocusHere();
            if (ImGui::InputText("##rename", buffer, sizeof(buffer),
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                if (SceneObject *target = scene.Find(o.id)) {
                    doc.BeginEdit();
                    target->name = buffer;
                    doc.CommitEdit();
                }
                state.renamingId = kInvalidSceneObjectId;
            } else {
                state.renameBuffer = buffer;
                if (ImGui::IsItemDeactivated() || ImGui::IsKeyPressed(ImGuiKey_Escape))
                    state.renamingId = kInvalidSceneObjectId;
            }
            ImGui::PopID();
            return;
        }
        std::string label = o.name.empty() ? "(unnamed)" : o.name;
        if (state.runtime) {
            if (const EntityRecord *e = state.runtime->FindEntity(o.id)) {
                label += e->lifecycle == EntityLifecycle::Destroyed
                             ? "  [destroyed]"
                             : std::string("  [") + FidelityName(e->fidelity) + "]";
            }
        }
        // Problem indicator: a mesh render whose asset does not resolve.
        bool broken = false;
        if (o.render && o.render->shape == SceneShape::Mesh) {
            const AssetRecord *record = state.assets && !o.render->meshAsset.empty()
                                            ? state.assets->Find(o.render->meshAsset)
                                            : nullptr;
            broken = !record || record->missing;
        }
        if (broken)
            label += "  (!)";
        label += "##" + std::to_string(o.id);
        const float indentation = float(row.depth) * 12.0f * ImGui::GetIO().FontGlobalScale;
        if (indentation > 0)
            ImGui::Indent(indentation);
        const auto textColor = ImGui::GetColorU32(broken ? ImVec4(1.0f, 0.55f, 0.3f, 1.0f)
                                                         : ImGui::GetStyleColorVec4(ImGuiCol_Text));
        auto accent = EditorVisuals::AccentColor();
        accent.w = .18f;
        ImGui::PushStyleColor(ImGuiCol_Header, accent);
        accent.w = .26f;
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, accent);
        accent.w = .36f;
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, accent);
        // Keep the original selectable identity/interaction; draw its visible
        // label beside a code-native type icon rather than a letter prefix.
        ImGui::PushStyleColor(ImGuiCol_Text, {0, 0, 0, 0});
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick,
                              {0, rowHeight})) {
            if (ImGui::GetIO().KeyShift)
                doc.SelectRange(o.id, rows, ImGui::GetIO().KeyCtrl);
            else
                doc.Select(o.id, ImGui::GetIO().KeyCtrl);
            if (editing && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                state.renamingId = o.id;
                state.renameBuffer = o.name;
            }
        }
        ImGui::PopStyleColor(4);
        const auto rowMinimum = ImGui::GetItemRectMin(), rowMaximum = ImGui::GetItemRectMax();
        auto *draw = ImGui::GetWindowDrawList();
        const float glyph = 14.f * scale;
        const float rowTop = rowMinimum.y + (rowMaximum.y - rowMinimum.y - glyph) * .5f;
        EditorVisuals::DrawIcon(draw, HierarchyIcon(o), {rowMinimum.x + 5.f * scale, rowTop}, glyph,
                                selected ? ImGui::GetColorU32(EditorVisuals::AccentColor()) : textColor);
        const ImVec4 clip{rowMinimum.x + glyph + 12.f * scale, rowMinimum.y, rowMaximum.x - 4.f * scale,
                          rowMaximum.y};
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                      {clip.x, rowMinimum.y + (rowHeight - ImGui::GetFontSize()) * .5f}, textColor,
                      label.c_str(), ImGui::FindRenderedTextEnd(label.c_str()), 0, &clip);
        if (selected) {
            accent.w = .55f;
            draw->AddRect(rowMinimum, rowMaximum, ImGui::GetColorU32(accent), 4.f);
        }
        if (row.depth) {
            const float branchX = rowMinimum.x - 5.f * scale;
            const float middle = (rowMinimum.y + rowMaximum.y) * .5f;
            const auto line = ImGui::GetColorU32(ImGuiCol_Border);
            draw->AddLine({branchX, rowMinimum.y - spacing}, {branchX, middle}, line);
            draw->AddLine({branchX, middle}, {rowMinimum.x + 2.f * scale, middle}, line);
        }
        if (ImGui::IsItemHovered()) {
            const std::string indicators = ComponentIndicators(o);
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(o.name.empty() ? "(unnamed)" : o.name.c_str());
            ImGui::TextDisabled("ID %llu  /  %s", static_cast<unsigned long long>(o.id),
                                indicators.empty() ? "empty object" : indicators.c_str());
            if (!o.authoringFolder.empty())
                ImGui::Text("Folder: %s", o.authoringFolder.c_str());
            if (const auto *parent = scene.Find(o.parent))
                ImGui::Text("Parent: %s", parent->name.c_str());
            if (broken)
                ImGui::TextColored({1, .55f, .3f, 1}, "Mesh asset is missing or unknown");
            ImGui::TextDisabled("Ctrl: add selection   Shift: select range");
            ImGui::EndTooltip();
        }
        if (editing && ImGui::BeginPopupContextItem()) {
            if (!selected)
                doc.Select(o.id);
            if (ImGui::BeginMenu("Create child")) {
                DrawEditorCreationItems(state, requests, o.id);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Focus", "F"))
                requests.focusSelection = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Rename", "F2", false, doc.Selection().size() == 1))
                BeginEditorRename(doc, state);
            if (ImGui::MenuItem("Duplicate"))
                requests.duplicateId = o.id;
            if (ImGui::BeginMenu("Set parent")) {
                if (ImGui::MenuItem("Scene root")) {
                    reparentTo = 0;
                    reparent = true;
                }
                ImGui::Separator();
                static ImGuiTextFilter parentFilter;
                if (ImGui::IsWindowAppearing())
                    parentFilter.Clear();
                parentFilter.Draw("Find parent");
                for (const auto &candidate : scene.Objects()) {
                    if (doc.IsSelected(candidate.id) || !parentFilter.PassFilter(candidate.name.c_str()))
                        continue;
                    ImGui::PushID(static_cast<int>(candidate.id));
                    if (ImGui::MenuItem(candidate.name.c_str())) {
                        reparentTo = candidate.id;
                        reparent = true;
                    }
                    ImGui::PopID();
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Move up")) {
                move = -1;
                moveId = o.id;
            }
            if (ImGui::MenuItem("Move down")) {
                move = 1;
                moveId = o.id;
            }
            if (ImGui::MenuItem("Delete"))
                toDelete = o.id;
            ImGui::EndPopup();
        }
        if (indentation > 0)
            ImGui::Unindent(indentation);
        ImGui::PopID();
    };
    // Inline rename has a different row height. Keep popup owners submitted
    // even if the list scrolls while their context menu is open.
    if (state.renamingId != kInvalidSceneObjectId ||
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        for (const auto &row : rows)
            drawRow(row);
    } else {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(rows.size()), rowPitch);
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                drawRow(rows[i]);
    }
    JUDAS_PROFILE_COUNTER("Hierarchy matched rows", double(rows.size()), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Hierarchy submitted rows", double(submitted), ProfileCounterMode::Latest);
    DrawHierarchyCreationPopup(state, requests);
    ImGui::EndChild();
    if (editing) {
        ImGui::BeginDisabled(doc.Selected() == kInvalidSceneObjectId);
        if (ImGui::Button("Actions", {-1, 0}))
            ImGui::OpenPopup("##selectionActions");
        ToolbarHint("Duplicate, group or delete the selection");
        if (ImGui::BeginPopup("##selectionActions")) {
            if (ImGui::MenuItem("Focus selection", "F"))
                requests.focusSelection = true;
            if (ImGui::MenuItem("Duplicate selection", "Ctrl+D")) {
                std::string error;
                if (!doc.DuplicateSelection(error))
                    state.status = error;
            }
            if (ImGui::MenuItem("Group in folder")) {
                std::string error;
                if (!doc.GroupSelection(error))
                    state.status = error;
            }
            if (ImGui::MenuItem("Delete selection", "Delete"))
                toDelete = doc.Selected();
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
    }
    DrawHierarchyCreationPopup(state, requests);
    if (reparent) {
        std::string error;
        state.status = doc.ReparentSelection(reparentTo, error) ? "Parent changed; world placement preserved"
                                                                : "Parent change rejected: " + error;
    }
    if (toDelete != kInvalidSceneObjectId) {
        std::string error;
        if (!doc.DeleteSelection(error))
            state.status = "Deletion rejected: " + error;
        else
            state.status = "Deleted selected hierarchies";
    }
    if (move != 0) {
        doc.BeginEdit();
        scene.MoveObject(moveId, move);
        doc.CommitEdit();
    }
    ImGui::End();
}
