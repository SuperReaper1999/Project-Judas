#include "EngineHost.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "editor/EditorContext.h"
#include "editor/EditorDocument.h"
#include "editor/EditorPanels.h"
#include "editor/EditorSceneViewport.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdio>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char *message) {
    ++checks;
    failures += !value;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", message);
}
ImGuiWindow *List() {
    auto *hierarchy = ImGui::FindWindowByName("Hierarchy");
    for (auto *w : ImGui::GetCurrentContext()->Windows)
        if (w->ParentWindow == hierarchy && std::string(w->Name).find("/list_") != std::string::npos)
            return w;
    return nullptr;
}
} // namespace
int main() {
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1100, 800};
    io.DeltaTime = 1.f / 60;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    EngineHost host;
    std::string startupError;
    if (!host.Init("Hierarchy selection checks", 640, 480, false, startupError)) {
        std::fprintf(stderr, "%s\n", startupError.c_str());
        return 1;
    }
    Project project;
    EditorDocument doc;
    EditorPanelState state;
    EditorRequests requests;
    state.dockingEnabled = true;
    for (int i = 0; i < 1200; ++i) {
        auto &o = doc.GetScene().CreateObject("Unrelated " + std::to_string(i));
        o.transform.position = {10000.f + i, 10000, 10000};
    }
    const auto grand = doc.GetScene().CreateObject("Group").id;
    doc.GetScene().Find(grand)->transform.position = {0, 0, -20};
    const auto parent = doc.GetScene().CreateObject("Parent").id;
    doc.GetScene().Find(parent)->parent = grand;
    doc.GetScene().Find(parent)->transform.position = {0, 0, 10};
    const auto target = doc.GetScene().CreateObject("Target box").id;
    auto *o = doc.GetScene().Find(target);
    o->parent = parent;
    o->transform.position = {0, 0, 20};
    o->render = SceneRenderComponent{};
    o->render->shape = SceneShape::Box;
    std::string before;
    SaveSceneToString(doc.GetScene(), before);
    const auto generation = doc.Generation();
    ImGuiID dock = 0;
    float hierarchyWidth = 360;
    auto frame = [&]() {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({20, 20}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({hierarchyWidth, 720}, ImGuiCond_Always);
        ImGui::Begin("Hierarchy tab host", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        dock = ImGui::GetID("Hierarchy docking fixture");
        ImGui::DockSpace(dock, {0, 0});
        ImGui::End();
        ImGui::SetNextWindowDockID(dock, ImGuiCond_Always);
        DrawHierarchyPanel(doc, state, requests);
        ImGui::SetNextWindowDockID(dock, ImGuiCond_Always);
        ImGui::Begin("Other hierarchy tab");
        ImGui::TextUnformatted("Another tool in the same tab group");
        ImGui::End();
        ImGui::SetNextWindowPos({450, 30}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({550, 620}, ImGuiCond_Always);
        ImGui::Begin("Scene input owner");
        ImGui::TextUnformatted("Scene controls");
        ImGui::End();
        ImGui::Render();
    };
    frame();
    frame();
    if (auto *other = ImGui::FindWindowByName("Other hierarchy tab"))
        other->DockNode->TabBar->NextSelectedTabId = other->TabId;
    frame();
    frame();
    Check(!ImGui::FindWindowByName("Hierarchy")->DockTabIsVisible,
          "another real docked tab initially covers Hierarchy");
    state.hierarchySearch = "Unrelated";
    state.collapsedHierarchyParents = {grand, parent};
    state.showHierarchy = false;
    EditorContext context{doc, project, state, requests, host, host.Assets()};
    EditorSceneViewport viewport;
    state.workspace.viewport = {{450, 30}, {550, 620}};
    viewport.camera.LookAt({0, 0, 10}, 10);
    ImGui::SetWindowFocus("Scene input owner");
    viewport.PickAtPixel(context, 725, 340);
    Check(doc.Selected() == target, "real Scene pixel picking selects the intended geometry");
    Check(state.showHierarchy && state.revealHierarchyObject == target,
          "Scene pick reopens Hierarchy and requests the exact selected row");
    frame();
    frame();
    frame();
    auto *list = List();
    Check(ImGui::FindWindowByName("Hierarchy")->DockTabIsVisible,
          "Scene selection reveals Hierarchy even behind a different docked tab");
    Check(state.hierarchySearch.empty(),
          "an obstructing hierarchy filter clears when Scene selects a hidden object");
    Check(!state.collapsedHierarchyParents.count(grand) && !state.collapsedHierarchyParents.count(parent),
          "selection reveal expands every collapsed parent");
    Check(list && list->Scroll.y > 1000,
          "selection reveal scrolls the actual clipped hierarchy to its distant row");
    const auto &rows = doc.HierarchyRows("");
    size_t index = 0;
    while (index < rows.size() && rows[index].id != target)
        ++index;
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float pitch =
        std::floor(std::max(ImGui::GetTextLineHeightWithSpacing(), 23.f * io.FontGlobalScale) + spacing);
    const float top = list ? list->DC.CursorStartPos.y + float(index) * pitch : 0;
    Check(list && top >= list->InnerClipRect.Min.y - 1 &&
              top + pitch - spacing <= list->InnerClipRect.Max.y + 1,
          "the selected row is actually inside the visible hierarchy clip, not merely included in its model");
    Check(ImGui::GetCurrentContext()->NavWindow == ImGui::FindWindowByName("Scene input owner"),
          "revealing selection does not steal Scene keyboard focus");
    if (list)
        ImGui::SetScrollY(list, 0);
    frame();
    frame();
    frame();
    Check(list && list->Scroll.y == 0 && doc.Selected() == target,
          "manual scrolling remains free while the current selection is unchanged");
    viewport.PickAtPixel(context, 725, 340);
    frame();
    frame();
    frame();
    Check(list && list->Scroll.y > 1000, "picking the same object again explicitly reveals its row again");
    std::string after;
    SaveSceneToString(doc.GetScene(), after);
    Check(after == before && !doc.IsDirty() && !doc.EditInProgress() && !doc.CanUndo() &&
              doc.Generation() == generation,
          "selection, filtering, folding and reveal never edit authored scene or history");
    doc.Select(0);
    frame();
    Check(doc.Selected() == 0, "clearing selection does not invent a target row");
    state.collapsedHierarchyParents.insert(grand);
    state.revealHierarchyObject = target;
    state.hierarchySearch = "stale filter";
    ResetEditorHierarchyView(state);
    Check(
        state.collapsedHierarchyParents.empty() && state.hierarchySearch.empty() &&
            state.revealHierarchyObject == 0 && state.hierarchyObservedSelection == 0,
        "scene replacement retires folding and reveal state instead of applying reused IDs to another scene");
    // A deep tree must keep its selected label visible in a narrow rail;
    // merely including an offscreen row in the clipper is insufficient.
    doc.NewScene();
    hierarchyWidth = 260;
    SceneObjectId branchRoot = 0, previous = 0, deepTarget = 0;
    for (int depth = 0; depth <= 30; ++depth) {
        auto &object = doc.GetScene().CreateObject(depth == 30 ? "Deep selected target"
                                                               : "Deep level " + std::to_string(depth));
        object.parent = previous;
        if (!depth)
            branchRoot = object.id;
        previous = object.id;
        deepTarget = object.id;
    }
    ResetEditorHierarchyView(state);
    doc.Select(deepTarget);
    state.revealHierarchyObject = deepTarget;
    frame();
    frame();
    frame();
    list = List();
    const auto &deepRows = doc.HierarchyRows("");
    const auto selectedRow =
        std::find_if(deepRows.begin(), deepRows.end(), [&](const auto &row) { return row.id == deepTarget; });
    Check(selectedRow != deepRows.end() && selectedRow->depth == 30,
          "narrow hierarchy retains the real thirty-level parent depth for tree operations");
    const float deepTop = list && selectedRow != deepRows.end()
                              ? list->DC.CursorStartPos.y + float(selectedRow - deepRows.begin()) * pitch
                              : 0;
    const auto labelColor = ImGui::GetColorU32(ImGuiCol_Text);
    unsigned visibleLabelVertices = 0;
    float firstGlyph = std::numeric_limits<float>::max();
    float lastGlyph = std::numeric_limits<float>::lowest();
    if (list)
        for (const auto &vertex : list->DrawList->VtxBuffer) {
            if (vertex.col != labelColor || vertex.pos.y < deepTop ||
                vertex.pos.y > deepTop + pitch - spacing)
                continue;
            ++visibleLabelVertices;
            firstGlyph = std::min(firstGlyph, vertex.pos.x);
            lastGlyph = std::max(lastGlyph, vertex.pos.x);
        }
    Check(list && deepTop >= list->InnerClipRect.Min.y - 1 &&
              deepTop + pitch - spacing <= list->InnerClipRect.Max.y + 1 && visibleLabelVertices >= 16 &&
              lastGlyph - firstGlyph >= 25 && firstGlyph >= list->InnerClipRect.Min.x &&
              lastGlyph <= list->InnerClipRect.Max.x,
          "selected deep object's actual name glyphs remain visible inside a narrow hierarchy rail");

    // Use real pointer events, including a genuine second press inside ImGui's
    // double-click interval. Expanding a branch is not a selection or rename.
    if (list)
        ImGui::SetScrollY(list, 0);
    frame();
    frame();
    frame();
    list = List();
    std::string beforeDisclosure;
    SaveSceneToString(doc.GetScene(), beforeDisclosure);
    const auto disclosureGeneration = doc.Generation();
    if (list) {
        const ImVec2 arrow{list->DC.CursorStartPos.x + 4.f,
                           list->DC.CursorStartPos.y + (pitch - spacing) * .5f};
        io.AddMousePosEvent(arrow.x, arrow.y);
        frame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        frame();
        Check(state.collapsedHierarchyParents.count(branchRoot) == 1,
              "actual disclosure press folds the parent, proving the pointer reached its arrow");
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        frame();
        Check(doc.Selected() == deepTarget && state.renamingId == kInvalidSceneObjectId,
              "releasing a disclosure click preserves the selected descendant and does not rename");
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        frame();
        const bool doubleClick = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        frame();
        Check(doubleClick && !state.collapsedHierarchyParents.count(branchRoot),
              "genuine double-click on the disclosure arrow unfolds the same branch");
        Check(doc.Selected() == deepTarget && state.renamingId == kInvalidSceneObjectId,
              "double-clicking the disclosure arrow neither reselects its parent nor starts Rename");
    } else {
        Check(false, "narrow hierarchy list exists for actual disclosure interaction");
    }
    // The same real disclosure press also focuses its native row. Leave the
    // pointer resting on that arrow and activate through ordinary keyboard
    // input: pointer location cannot veto navigation selection.
    if (list) {
        const int rootId = static_cast<int>(branchRoot);
        const auto scope = ImHashData(&rootId, sizeof(rootId), list->ID);
        const auto label = "Deep level 0##" + std::to_string(branchRoot);
        const auto rowId = ImHashStr(label.c_str(), 0, scope);
        Check(ImGui::GetCurrentContext()->NavId == rowId,
              "real disclosure click gives its parent row native keyboard focus");
        // Mouse focus alone leaves ImGui's navigation cursor hidden, which
        // intentionally blocks Enter/Space even for an unchanged native
        // button. Establish actual visible keyboard navigation using Tab;
        // do not force an activation ID or navigation-state flag.
        bool keyboardFocused = false;
        for (unsigned attempt = 0; attempt < 80; ++attempt) {
            const auto *navigation = ImGui::GetCurrentContext();
            if (navigation->NavId == rowId && navigation->NavCursorVisible) {
                keyboardFocused = true;
                break;
            }
            io.AddKeyEvent(ImGuiKey_Tab, true);
            frame();
            io.AddKeyEvent(ImGuiKey_Tab, false);
            frame();
        }
        Check(keyboardFocused,
              "real Tab navigation visibly focuses the parent row before keyboard activation");
        const auto pointer = io.MousePos;
        Check(pointer.x >= list->DC.CursorStartPos.x && pointer.x < list->DC.CursorStartPos.x + 14.f &&
                  pointer.y >= list->DC.CursorStartPos.y && pointer.y < list->DC.CursorStartPos.y + pitch - spacing,
              "pointer still rests on the focused parent's actual disclosure arrow");
        io.AddKeyEvent(ImGuiKey_Enter, true);
        frame();
        io.AddKeyEvent(ImGuiKey_Enter, false);
        frame();
        Check(doc.Selected() == branchRoot && state.renamingId == kInvalidSceneObjectId &&
                  !state.collapsedHierarchyParents.count(branchRoot),
              "keyboard activation selects the focused parent despite a pointer resting on its disclosure arrow");

        // Right-click a different, already visible row near the bottom of a
        // manually scrolled list. Its own context selection must not trigger
        // the external-selection centering path on subsequent frames.
        ImGui::SetScrollY(list, list->ScrollMax.y * .45f);
        frame();
        frame();
        const float scrollBeforeContext = list->Scroll.y;
        const auto &contextRows = doc.HierarchyRows("");
        const int visibleIndex = std::clamp(
            int(std::floor((list->InnerClipRect.Max.y - list->DC.CursorStartPos.y) / pitch)) - 2,
            1, static_cast<int>(contextRows.size()) - 1);
        const auto contextId = contextRows[visibleIndex].id;
        const ImVec2 rowPoint{list->InnerClipRect.Max.x - 24.f,
                             list->DC.CursorStartPos.y + visibleIndex * pitch + (pitch - spacing) * .5f};
        Check(rowPoint.y > list->InnerClipRect.Min.y && rowPoint.y < list->InnerClipRect.Max.y &&
                  contextId != doc.Selected(),
              "context selection fixture targets a visible lower row rather than the current selection");
        io.AddMousePosEvent(rowPoint.x, rowPoint.y);
        frame();
        frame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
        frame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
        frame();
        Check(doc.Selected() == contextId && !ImGui::GetCurrentContext()->OpenPopupStack.empty(),
              "real right-click selects the intended lower row and opens its context menu");
        frame();
        frame();
        Check(state.hierarchyObservedSelection == contextId && state.revealHierarchyObject == 0 &&
                  std::abs(list->Scroll.y - scrollBeforeContext) < .5f,
              "context selection stays internal and does not scroll its row away from the open menu");
        io.AddKeyEvent(ImGuiKey_Escape, true);
        frame();
        io.AddKeyEvent(ImGuiKey_Escape, false);
        frame();
    }
    std::string afterDisclosure;
    SaveSceneToString(doc.GetScene(), afterDisclosure);
    Check(beforeDisclosure == afterDisclosure && doc.Generation() == disclosureGeneration && !doc.IsDirty() &&
              !doc.EditInProgress() && !doc.CanUndo(),
          "disclosure interaction preserves exact authored bytes and document history");
    ImGui::DestroyContext();
    std::printf("Hierarchy selection: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
