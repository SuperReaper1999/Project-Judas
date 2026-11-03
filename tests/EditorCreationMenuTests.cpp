#include "editor/EditorCreationMenu.h"
#include "editor/EditorPanels.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <iostream>
#include <stdexcept>

int main() {
    unsigned checks = 0;
    const auto check = [&](bool okay, const char* description) {
        ++checks;
        if (!okay) throw std::runtime_error(description);
        std::cout << "PASS " << description << '\n';
    };
    try {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = {1000, 800};
        io.DeltaTime = 1.f / 60;
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        EditorPanelState state;
        state.cameraFocus = {3, 4, 5};
        bool openChildMenu = false;
        const SceneObjectId parentId = 77;
        const auto frame = [&]() {
            EditorRequests requests;
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({20, 20}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({400, 600}, ImGuiCond_Always);
            ImGui::Begin("Hierarchy test");
            DrawHierarchyCreationPopup(state, requests);
            if (openChildMenu) { ImGui::OpenPopup("Create child test"); openChildMenu = false; }
            if (ImGui::BeginPopup("Create child test")) {
                DrawEditorCreationItems(state, requests, parentId);
                ImGui::EndPopup();
            }
            ImGui::End();
            ImGui::Render();
            return requests;
        };
        frame(); frame();
        const auto rightClick = [&]() {
            io.AddMousePosEvent(100, 100); frame();
            io.AddMouseButtonEvent(1, true); frame();
            io.AddMouseButtonEvent(1, false); frame(); frame();
        };
        const struct { const char* label; const char* kind; } types[] = {
            {"Empty object", "empty"}, {"Static box", "box"},
            {"First-person player (JS + motor)", "first-person-player"},
            {"Third-person player (JS + motor)", "third-person-player"},
            {"Pause menu + controls (JS + UI)", "pause-menu-example"},
            {"Capsule", "capsule"}, {"Cylinder", "cylinder"},
            {"Static sphere", "sphere"}, {"Dynamic box", "dynamic-box"},
            {"Dynamic sphere", "dynamic-sphere"}, {"Mesh", "mesh"},
            {"Point light", "point-light"}, {"Spot light", "spot-light"},
            {"Door (legacy gameplay)", "door"}, {"Gravity region", "gravity-region"},
            {"Basic water", "basic-water"}, {"Player start (legacy gameplay)", "player-start"},
            {"Render camera", "render-camera"}, {"Audio emitter", "audio-emitter"},
            {"Audio listener", "audio-listener"}
        };
        for (const auto& type : types) {
            rightClick();
            auto& popups = ImGui::GetCurrentContext()->OpenPopupStack;
            check(!popups.empty() && popups.back().Window, "right-click opens hierarchy creation menu");
            ImGui::ActivateItemByID(popups.back().Window->GetID(type.label));
            auto request = frame();
            if (request.createKind.empty()) request = frame();
            check(request.createKind == type.kind, type.label);
            check(request.createPosition == state.cameraFocus, "same creation placement as main Create menu");
            check(request.createParent == kInvalidSceneObjectId, "empty hierarchy creates at the root");
            check(frame().createKind.empty(), "creation command is emitted once");
        }
        rightClick(); frame();
        io.AddInputCharactersUTF8("cylinder"); frame(); frame();
        auto* filtered = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        ImGui::ActivateItemByID(filtered->GetID("Static box"));
        check(frame().createKind.empty(), "filtered-out objects cannot be activated");
        ImGui::ActivateItemByID(filtered->GetID("Cylinder"));
        auto filteredRequest = frame(); if (filteredRequest.createKind.empty()) filteredRequest = frame();
        check(filteredRequest.createKind == "cylinder", "typing filters the actual popup and creates the matching object");
        rightClick(); frame();
        io.AddInputCharactersUTF8("swimming"); frame(); frame();
        auto* described = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        ImGui::ActivateItemByID(described->GetID("Basic water"));
        auto describedRequest = frame(); if (describedRequest.createKind.empty()) describedRequest = frame();
        check(describedRequest.createKind == "basic-water", "search matches capability descriptions as well as names");
        rightClick();
        io.AddKeyEvent(ImGuiKey_Escape, true); frame();
        io.AddKeyEvent(ImGuiKey_Escape, false);
        check(frame().createKind.empty() && ImGui::GetCurrentContext()->OpenPopupStack.empty(),
              "Escape dismisses menu without creating anything");
        openChildMenu = true; frame(); frame();
        auto* childPopup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        ImGui::ActivateItemByID(childPopup->GetID("Dynamic box"));
        auto childRequest = frame();
        if (childRequest.createKind.empty()) childRequest = frame();
        check(childRequest.createKind == "dynamic-box" && childRequest.createParent == parentId,
              "row creation preserves the exact requested parent");
        state.mode = EditorMode::Play;
        rightClick();
        check(frame().createKind.empty() && ImGui::GetCurrentContext()->OpenPopupStack.empty(),
              "right-click cannot create scene objects during Play");
        EditorDocument doc;
        const auto parent = CreateObjectOfKind(doc, "empty", {10, 20, 30}, "");
        const auto before = doc.GetScene().Objects().size();
        const auto child = CreateObjectOfKind(doc, childRequest.createKind, childRequest.createPosition,
                                              "", nullptr, parent);
        const auto* object = doc.GetScene().Find(child);
        check(object && object->parent == parent && object->transform.position == glm::vec3(0) &&
                  object->transform.scale == glm::vec3(1) && object->transform.rotation == glm::quat(1,0,0,0),
              "child has parent-local origin, identity rotation and unit scale");
        check(doc.Selected() == child && doc.Selection().size() == 1, "new child is immediately the sole selection");
        check(object->render && object->body && object->body->motion == SceneBodyMotion::Dynamic &&
                  object->body->mass == 5.f && object->body->pickable,
              "dynamic box retains ordinary render, collision, mass and interaction defaults");
        doc.Undo();
        check(!doc.GetScene().Find(child) && doc.GetScene().Find(parent) &&
                  doc.GetScene().Objects().size() == before, "one Undo removes child without removing parent");
        doc.Redo();
        check(doc.GetScene().Find(child) && doc.GetScene().Find(child)->parent == parent,
              "Redo restores the same child and parent relationship");
        const auto root = CreateObjectOfKind(doc, "sphere", state.cameraFocus, "");
        object = doc.GetScene().Find(root);
        check(object && object->parent == 0 && object->transform.position == state.cameraFocus &&
                  doc.Selected() == root && object->body->motion == SceneBodyMotion::Static,
              "root object is selected at scene focus with static sphere defaults");
        const auto count = doc.GetScene().Objects().size();
        check(CreateObjectOfKind(doc, "box", {}, "", nullptr, 999999) == 0 &&
                  doc.GetScene().Objects().size() == count && !doc.EditInProgress(),
              "stale parent fails without creating an orphan or leaving an edit open");
        EditorDocument tree;
        auto& scene = tree.GetScene();
        const auto trunk = scene.CreateObject("Trunk").id;
        const auto unused = scene.CreateObject("Unrelated").id;
        const auto branch = scene.CreateObject("Branch").id;
        const auto leaf = scene.CreateObject("Leaf").id;
        scene.Find(branch)->parent = trunk;
        scene.Find(leaf)->parent = branch;
        scene.Find(leaf)->tags = CategoryBit(4);
        check(scene.MoveObject(leaf, -1) && scene.MoveObject(leaf, -1) && scene.MoveObject(leaf, -1), "hierarchy fixture puts a child before its ancestors");
        const auto& filteredRows = tree.HierarchyRows("Rare fixture tag", CategoryBit(4));
        check(filteredRows.size() == 3 && filteredRows[0].id == leaf && filteredRows[0].depth == 2 &&
                  filteredRows[1].id == trunk && filteredRows[1].depth == 0 &&
                  filteredRows[2].id == branch && filteredRows[2].depth == 1,
              "tag search includes all ancestors in drawn scene order with depth");
        tree.Select(leaf);
        tree.SelectRange(trunk, filteredRows);
        check(tree.Selection() == std::vector<SceneObjectId>{leaf, trunk} && tree.Selected() == trunk,
              "Shift selects the visible span rather than numeric IDs");
        tree.Select(unused, true);
        tree.Select(trunk, true);
        tree.Select(trunk, true);
        tree.SelectRange(branch, filteredRows, true);
        check(tree.Selection() == std::vector<SceneObjectId>{leaf, unused, trunk, branch},
              "Ctrl+Shift adds the visible span without losing or duplicating earlier selections");
        check(tree.HierarchyRows("Rare fixture tag").empty(),
              "changed project tag definitions invalidate the same query");
        check(tree.HierarchyRows("Leaf").size() == 3 && tree.HierarchyRows("").size() == 4,
              "name search includes ancestors and clearing the query restores every row");
        tree.BeginEdit();
        scene.Find(leaf)->name = "Renamed leaf";
        tree.CommitEdit();
        check(tree.ValidationError().empty() && tree.HierarchyRows("Renamed leaf").size() == 3 &&
                  tree.HierarchyRows("Renamed leaf").front().id == leaf,
              "committed rename updates filtered rows");
        tree.Undo();
        check(tree.HierarchyRows("Renamed leaf").empty() && tree.HierarchyRows("Leaf").size() == 3,
              "Undo restores filtered rows");
        tree.BeginEdit();
        scene.Find(leaf)->name = "Cancelled change";
        check(tree.HierarchyRows("Cancelled change").size() == 3, "changed query reads the current editing scene");
        tree.CancelEdit();
        check(tree.HierarchyRows("Cancelled change").empty(),
              "cancel discards cached rows from the temporary edit");
        ImGui::DestroyContext();
        std::cout << "Editor creation menu: " << checks << " checks, 0 failures\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
