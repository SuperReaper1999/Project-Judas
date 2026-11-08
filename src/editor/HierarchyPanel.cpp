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
CategoryMask MatchingHierarchyTags(const EditorPanelState &state) {
    CategoryMask result = 0;
    if (state.project && !state.hierarchySearch.empty())
        for (const auto &[id, name] : state.project->Settings().classification.tags.names)
            if (name.find(state.hierarchySearch) != std::string::npos)
                result |= CategoryBit(id);
    return result;
}
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

void ResetEditorHierarchyView(EditorPanelState &state) {
    state.hierarchyObservedSelection = kInvalidSceneObjectId;
    state.revealHierarchyObject = kInvalidSceneObjectId;
    state.collapsedHierarchyParents.clear();
    state.hierarchySearch.clear();
}

void DrawHierarchyPanel(EditorDocument &doc, EditorPanelState &state, EditorRequests &requests) {
    if (!state.showHierarchy)
        return;
    JUDAS_PROFILE_SCOPE("Editor hierarchy");
    if (state.hierarchyObservedSelection != doc.Selected()) {
        state.hierarchyObservedSelection = doc.Selected();
        state.revealHierarchyObject = doc.Selected();
    }
    const auto reveal = state.revealHierarchyObject;
    if (reveal && doc.GetScene().Find(reveal)) {
        // Reveal the tab without taking keyboard focus away from Scene or a
        // property field. A selection made in Scene can also reopen the panel.
        if (auto *window = ImGui::FindWindowByName("Hierarchy"))
            if (window->DockNode && window->DockNode->TabBar) {
                window->DockNode->SelectedTabId = window->TabId;
                window->DockNode->TabBar->SelectedTabId = window->TabId;
                window->DockNode->TabBar->VisibleTabId = window->TabId;
                // NextSelectedTabId is deliberately not used: docking treats
                // it as a navigation-focus request, which would steal Scene's keys.
            }
        std::set<SceneObjectId> visited;
        for (auto *object = doc.GetScene().Find(reveal); object && visited.insert(object->id).second;
             object = doc.GetScene().Find(object->parent))
            state.collapsedHierarchyParents.erase(object->parent);
        const auto &filtered = doc.HierarchyRows(state.hierarchySearch, MatchingHierarchyTags(state));
        if (std::none_of(filtered.begin(), filtered.end(),
                         [reveal](const auto &row) { return row.id == reveal; }))
            state.hierarchySearch.clear();
    }
    if (!BeginEditorPanel("Hierarchy", &state.showHierarchy, state,
                          reveal ? ImGuiWindowFlags_NoFocusOnAppearing : 0)) {
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
    if (EditorDesign::Search("##hierarchySearch", "Search scene objects", search, sizeof(search)))
        state.hierarchySearch = search;
    ImGui::Spacing();
    ImGui::PushFont(EditorVisuals::HeadingFont());
    EditorVisuals::IconText(EditorVisuals::Icon::Scene,
                            scene.Settings().name.empty() ? "Scene" : scene.Settings().name.c_str(), 14);
    ImGui::PopFont();
    ImGui::Separator();
    SceneObjectId toDelete = kInvalidSceneObjectId;
    SceneObjectId reparentTo = kInvalidSceneObjectId;
    bool reparent = false;
    int move = 0;
    SceneObjectId moveId = kInvalidSceneObjectId;
    const float footerHeight = std::max(ImGui::GetFrameHeight(), 32.f * scale) +
                               ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("list", ImVec2(0.0f, -footerHeight));
    const auto matchingTags = MatchingHierarchyTags(state);
    const auto &matchedRows = doc.HierarchyRows(state.hierarchySearch, matchingTags);
    std::vector<EditorHierarchyRow> foldedRows;
    const auto &rows = [&]() -> const std::vector<EditorHierarchyRow> & {
        if (state.collapsedHierarchyParents.empty() || !state.hierarchySearch.empty())
            return matchedRows;
        foldedRows.reserve(matchedRows.size());
        unsigned foldedDepth = ~0u;
        for (const auto &row : matchedRows) {
            if (row.depth > foldedDepth)
                continue;
            foldedDepth = ~0u;
            foldedRows.push_back(row);
            if (state.collapsedHierarchyParents.count(row.id))
                foldedDepth = row.depth;
        }
        return foldedRows;
    }();
    std::set<SceneObjectId> parents;
    for (const auto &row : matchedRows)
        if (scene.Objects()[row.sceneIndex].parent)
            parents.insert(scene.Objects()[row.sceneIndex].parent);
    const auto revealRow =
        std::find_if(rows.begin(), rows.end(), [reveal](const auto &row) { return row.id == reveal; });
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
        const float indentation = std::min(float(row.depth) * 16.f * scale,
            std::max(0.f, ImGui::GetContentRegionAvail().x - 125.f * scale));
        if (indentation > 0)
            ImGui::Indent(indentation);
        const auto textColor = ImGui::GetColorU32(broken ? ImVec4(1.0f, 0.55f, 0.3f, 1.0f)
                                                         : ImGui::GetStyleColorVec4(ImGuiCol_Text));
        const bool hasChildren = parents.count(o.id) != 0;
        const auto start = ImGui::GetCursorScreenPos();
        const auto pointer = ImGui::GetIO().MousePos;
        const bool overDisclosure = hasChildren && pointer.x >= start.x && pointer.x < start.x + 14.f * scale &&
                                    pointer.y >= start.y && pointer.y < start.y + rowHeight;
        const bool activated = EditorDesign::Row(label.c_str(), selected, {0, rowHeight}, true,
                                                  14.f * scale + 14.f * scale + 12.f * scale);
        const auto rowId = ImGui::GetItemID();
        const auto &navigation = *ImGui::GetCurrentContext();
        const bool navigationActivated = navigation.NavActivateId == rowId ||
                                         navigation.NavActivatePressedId == rowId;
        // Pointer presses on the disclosure arrow fold without selecting.
        // A resting pointer must not suppress keyboard/gamepad activation.
        if (activated && (!overDisclosure || navigationActivated)) {
            if (ImGui::GetIO().KeyShift)
                doc.SelectRange(o.id, rows, ImGui::GetIO().KeyCtrl);
            else
                doc.Select(o.id, ImGui::GetIO().KeyCtrl);
            state.hierarchyObservedSelection = doc.Selected();
            if (editing && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                state.renamingId = o.id;
                state.renameBuffer = o.name;
            }
        }
        const auto rowMinimum = ImGui::GetItemRectMin(), rowMaximum = ImGui::GetItemRectMax();
        auto *draw = ImGui::GetWindowDrawList();
        const float glyph = 14.f * scale;
        const float rowTop = rowMinimum.y + (rowMaximum.y - rowMinimum.y - glyph) * .5f;
        const float arrowSize = 14.f * scale;
        const bool folded = state.collapsedHierarchyParents.count(o.id) != 0;
        if (hasChildren) {
            const ImVec2 center{rowMinimum.x + arrowSize * .5f, rowMinimum.y + rowHeight * .5f};
            const float r = 3.5f * scale;
            if (folded)
                draw->AddTriangleFilled({center.x - r * .5f, center.y - r},
                                        {center.x - r * .5f, center.y + r}, {center.x + r, center.y},
                                        textColor);
            else
                draw->AddTriangleFilled({center.x - r, center.y - r * .5f},
                                        {center.x + r, center.y - r * .5f}, {center.x, center.y + r},
                                        textColor);
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                ImGui::GetIO().MousePos.x < rowMinimum.x + arrowSize) {
                if (folded)
                    state.collapsedHierarchyParents.erase(o.id);
                else
                    state.collapsedHierarchyParents.insert(o.id);
            }
        }
        EditorVisuals::DrawIcon(draw, HierarchyIcon(o), {rowMinimum.x + arrowSize + 5.f * scale, rowTop},
                                glyph,
                                selected ? ImGui::GetColorU32(EditorVisuals::AccentColor()) : textColor);
        if (o.id == reveal) {
            ImGui::SetScrollFromPosY(rowMinimum.y - ImGui::GetWindowPos().y + rowHeight * .5f, .5f);
            state.revealHierarchyObject = kInvalidSceneObjectId;
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
            if (!selected) {
                doc.Select(o.id);
                // This is an internal hierarchy selection, just like a left
                // click. Keep the row beside its open context menu rather
                // than treating it as an external Scene reveal next frame.
                state.hierarchyObservedSelection = doc.Selected();
            }
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
        if (revealRow != rows.end())
            clipper.IncludeItemByIndex(static_cast<int>(revealRow - rows.begin()));
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                drawRow(rows[i]);
    }
    JUDAS_PROFILE_COUNTER("Hierarchy matched rows", double(rows.size()), ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Hierarchy submitted rows", double(submitted), ProfileCounterMode::Latest);
    DrawHierarchyCreationPopup(state, requests);
    ImGui::EndChild();
    ImGui::TextDisabled("%zu objects", scene.Objects().size());
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - 78.f * scale));
    if (editing) {
        ImGui::BeginDisabled(doc.Selected() == kInvalidSceneObjectId);
        if (EditorDesign::ActionButton("Actions", EditorDesign::Tone::Quiet, {78.f * scale, 0}))
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
