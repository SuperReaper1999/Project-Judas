#include "ComponentEditors.h"
#include "EditorPanelWidgets.h"
#include "EditorPanels.h"
#include "EditorWidgets.h"
#include "EditorWorkspace.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "imgui.h"
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
    char search[256];
    CopyToBuffer(state.hierarchySearch, search, sizeof(search));
    ImGui::SetNextItemWidth(std::max(40.0f, ImGui::GetContentRegionAvail().x - 30.0f));
    if (ImGui::InputTextWithHint("##hierarchySearch", "Search names, tags, folders...", search,
                                 sizeof(search)))
        state.hierarchySearch = search;
    ImGui::SameLine();
    if (ImGui::SmallButton("X##clearSearch"))
        state.hierarchySearch.clear();
    ToolbarHint("Clear hierarchy search");
    ImGui::TextDisabled("%zu objects / %zu selected", scene.Objects().size(), doc.Selection().size());
    ImGui::Separator();
    SceneObjectId toDelete = kInvalidSceneObjectId;
    SceneObjectId reparentTo = kInvalidSceneObjectId;
    bool reparent = false;
    int move = 0;
    SceneObjectId moveId = kInvalidSceneObjectId;
    const float footerWidth = ImGui::CalcTextSize("Create...").x + ImGui::CalcTextSize("Actions").x +
                              4 * ImGui::GetStyle().FramePadding.x + ImGui::GetStyle().ItemSpacing.x;
    const float footerRows = footerWidth > ImGui::GetContentRegionAvail().x ? 2.f : 1.f;
    ImGui::BeginChild("list", ImVec2(0.0f, -footerRows * ImGui::GetFrameHeightWithSpacing()));
    CategoryMask matchingTags = 0;
    if (state.project && !state.hierarchySearch.empty())
        for (const auto &[id, name] : state.project->Settings().classification.tags.names)
            if (name.find(state.hierarchySearch) != std::string::npos)
                matchingTags |= CategoryBit(id);
    const auto &rows = doc.HierarchyRows(state.hierarchySearch, matchingTags);
    // ImGui snaps item positions to pixels. A fractional row pitch would
    // round differently above the viewport and change the scroll extent.
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float rowPitch = std::floor(ImGui::GetTextLineHeightWithSpacing() + spacing);
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
                                 ImGuiInputTextFlags_EnterReturnsTrue |
                                     ImGuiInputTextFlags_AutoSelectAll)) {
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
        if (broken)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.3f, 1.0f));
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
        if (broken)
            ImGui::PopStyleColor();
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
        if (ImGui::Button("Create..."))
            ImGui::OpenPopup("hierarchyCreate");
        if (ImGui::BeginPopup("hierarchyCreate")) {
            DrawEditorCreationItems(state, requests);
            ImGui::EndPopup();
        }
        NextActionOnRow("Actions");
        ImGui::BeginDisabled(doc.Selected() == kInvalidSceneObjectId);
        if (ImGui::Button("Actions"))
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
        state.status = doc.ReparentSelection(reparentTo, error)
                           ? "Parent changed; world placement preserved"
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
