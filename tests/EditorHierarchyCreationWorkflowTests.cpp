#include "Prefab.h"
#include "SceneSerialization.h"
#include "editor/EditorCreationMenu.h"
#include "editor/EditorDocument.h"
#include "editor/EditorPanels.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
bool Check(bool condition, const char *message) {
    ++checks;
    failures += !condition;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
    std::fflush(stdout);
    return condition;
}

class HierarchyFrames {
  public:
    HierarchyFrames() {
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = {1280, 800};
        io.DeltaTime = 1.f / 60;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        unsigned char *pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        panels.dockingEnabled = true;
        panels.showHierarchy = true;
        panels.cameraFocus = {3, 4, 5};
        parent = doc.GetScene().CreateObject("Rotated parent").id;
        auto *object = doc.GetScene().Find(parent);
        object->transform.position = {10, 20, 30};
        object->transform.rotation = glm::angleAxis(.6f, glm::normalize(glm::vec3(1, 2, 3)));
        object->transform.scale = {2, 3, 4};
        unrelated = doc.GetScene().CreateObject("Existing Game menu").id;
        doc.Select(unrelated);
        Frame();
        Frame();
    }
    ~HierarchyFrames() {
        ImGui::DestroyContext();
    }
    EditorRequests Frame() {
        EditorRequests requests;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({20, 20}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({420, 730}, ImGuiCond_Always);
        DrawHierarchyPanel(doc, panels, requests);
        ImGui::Render();
        if (!requests.createKind.empty()) {
            ++emitted;
            lastCreation = requests;
        }
        return requests;
    }
    ImGuiWindow *List() const {
        auto *hierarchy = ImGui::FindWindowByName("Hierarchy");
        for (auto *window : ImGui::GetCurrentContext()->Windows)
            if (window->ParentWindow == hierarchy &&
                std::string(window->Name).find("/list_") != std::string::npos)
                return window;
        return nullptr;
    }
    void Move(ImVec2 point) {
        ImGui::GetIO().AddMousePosEvent(point.x, point.y);
        Frame();
        Frame();
    }
    void Click(int button) {
        ImGui::GetIO().AddMouseButtonEvent(button, true);
        Frame();
        ImGui::GetIO().AddMouseButtonEvent(button, false);
        Frame();
        Frame();
    }
    ImGuiWindow *Popup(unsigned depth = 0) const {
        const auto &stack = ImGui::GetCurrentContext()->OpenPopupStack;
        return stack.Size > int(depth) ? stack[depth].Window : nullptr;
    }
    // Real native hit testing finds the requested visible label. This does not
    // activate an ID, inject a request, or assume the height/order of menus.
    bool HoverItem(ImGuiWindow *window, const char *label) {
        const auto expected = window->GetID(label);
        const float x = window->InnerClipRect.Min.x + std::min(50.f, window->InnerClipRect.GetWidth() * .5f);
        const float stride = std::max(1.f, ImGui::GetFontSize() * .4f);
        const float first = window->InnerClipRect.Min.y + 2;
        const float last = window->InnerClipRect.Max.y - 2;
        for (float y = first; y < last; y += stride) {
            Move({x, y});
            if (ImGui::GetCurrentContext()->HoveredId == expected)
                return true;
        }
        std::printf("Missing hovered label %s in %s (%.1f .. %.1f)\n", label, window->Name, first, last);
        return false;
    }
    bool OpenChildMenu() {
        const auto *list = List();
        if (!Check(list, "actual Hierarchy owns its list window"))
            return false;
        const float rowHeight = std::max(ImGui::GetTextLineHeightWithSpacing(), 23.f);
        Move({list->WorkRect.Min.x + 40, list->DC.CursorStartPos.y + rowHeight * .5f});
        Click(ImGuiMouseButton_Right);
        auto *popup = Popup();
        if (!Check(popup, "right-clicking an actual object row opens its context menu"))
            return false;
        if (!Check(doc.Selected() == parent,
                   "row context action targets the clicked object, not the old selection"))
            return false;
        if (!Check(HoverItem(popup, "Create child"), "native pointer finds the actual Create child action"))
            return false;
        Click(ImGuiMouseButton_Left);
        for (int i = 0; i < 15; ++i)
            Frame();
        return Check(Popup(1), "Create child opens its real nested object-type menu");
    }
    void CloseMenus() {
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
        Frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
        Frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
        Frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
        Frame();
    }
    EditorDocument doc;
    EditorPanelState panels;
    EditorRequests lastCreation;
    SceneObjectId parent = 0, unrelated = 0;
    unsigned emitted = 0;
};

void ActualChildShape(const char *label, const char *kind) {
    HierarchyFrames ui;
    if (!ui.OpenChildMenu())
        return;
    Check(ui.emitted == 0, "opening Create child alone never creates a game menu or other object");
    if (!Check(ui.HoverItem(ui.Popup(1), label), "native pointer reaches requested shape in the nested menu"))
        return;
    ui.Click(ImGuiMouseButton_Left);
    const auto request = ui.lastCreation;
    if (!Check(ui.emitted == 1 && request.createKind == kind && request.createParent == ui.parent,
               "actual row/submenu pointer click emits exactly the requested shape and parent"))
        return;
    std::string before;
    SaveSceneToString(ui.doc.GetScene(), before);
    const auto id = CreateObjectOfKind(ui.doc, request.createKind, request.createPosition, "", nullptr,
                                       request.createParent);
    const auto *created = ui.doc.GetScene().Find(id);
    Check(created && created->parent == ui.parent && created->transform.position == glm::vec3(0) &&
              created->transform.rotation == glm::quat(1, 0, 0, 0) &&
              created->transform.scale == glm::vec3(1),
          "created shape has the exact parent with identity parent-local placement");
    Check(created && created->name != "Existing Game menu" && !created->ui && created->render &&
              created->body,
          "shape creation produces real render/collision components, never a UI game-menu object");
    Scene flattened;
    std::string error;
    Check(FlattenHierarchy(ui.doc.GetScene(), flattened, error) && flattened.Find(id) &&
              glm::length(flattened.Find(id)->transform.position - glm::vec3(10, 20, 30)) < 1.e-6f,
          "child world placement follows the translated rotated scaled parent");
    Check(ui.doc.Selected() == id && ui.doc.Selection().size() == 1,
          "newly created child is immediately the sole selected object");
    const auto &rows = ui.doc.HierarchyRows("");
    const auto parentRow =
        std::find_if(rows.begin(), rows.end(), [&](const auto &row) { return row.id == ui.parent; });
    const bool directlyBelowParent = parentRow != rows.end() && parentRow + 1 != rows.end() &&
                                     (parentRow + 1)->id == id &&
                                     (parentRow + 1)->depth == parentRow->depth + 1;
    Check(
        directlyBelowParent,
        "actual hierarchy rows show the new child directly beneath its real parent, before unrelated Game menu");
    ui.doc.Undo();
    std::string after;
    SaveSceneToString(ui.doc.GetScene(), after);
    Check(before == after && ui.doc.GetScene().Find(ui.parent) && ui.doc.GetScene().Find(ui.unrelated),
          "one Undo restores exact authored bytes and preserves parent and unrelated Game menu");
    ui.doc.Redo();
    Check(ui.doc.GetScene().Find(id) && ui.doc.GetScene().Find(id)->parent == ui.parent,
          "Redo restores the same shape and parent relationship");
    for (int i = 0; i < 4; ++i)
        ui.Frame();
    Check(ui.emitted == 1, "dismissed nested menu cannot repeat a creation request on later frames");
}

void NativeRootAndNegativeControls() {
    HierarchyFrames ui;
    const auto *list = ui.List();
    if (!Check(list, "root-creation fixture uses the actual list"))
        return;
    ui.Move({list->WorkRect.Min.x + 30, list->InnerClipRect.Max.y - 35});
    ui.Click(ImGuiMouseButton_Right);
    auto *popup = ui.Popup();
    if (!Check(popup, "empty list space opens root creation rather than an object context"))
        return;
    if (!Check(ui.HoverItem(popup, "Static sphere"), "native pointer finds root sphere creation"))
        return;
    ui.Click(ImGuiMouseButton_Left);
    Check(ui.emitted == 1 && ui.lastCreation.createKind == "sphere" && ui.lastCreation.createParent == 0 &&
              ui.lastCreation.createPosition == ui.panels.cameraFocus,
          "empty hierarchy creation remains a root at the scene focus");
    ui.CloseMenus();
    ui.panels.mode = EditorMode::Play;
    ui.Move({list->WorkRect.Min.x + 30, list->InnerClipRect.Max.y - 35});
    ui.Click(ImGuiMouseButton_Right);
    Check(!ui.Popup() && ui.emitted == 1, "Play cannot create objects through the empty hierarchy menu");
}
} // namespace

int main() {
    try {
        ActualChildShape("Static box", "box");
        ActualChildShape("Dynamic sphere", "dynamic-sphere");
        NativeRootAndNegativeControls();
    } catch (const std::exception &error) {
        Check(false, error.what());
    }
    std::printf("Hierarchy creation workflow: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
