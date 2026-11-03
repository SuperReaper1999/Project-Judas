#include "EditorPanels.h"
#include "EditorPreferences.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
ImGuiWindow* RowsWindow() {
    const auto* panel = ImGui::FindWindowByName("Hierarchy");
    return panel && panel->DC.ChildWindows.Size ? panel->DC.ChildWindows.front() : nullptr;
}

void Check(bool okay, const char* text) {
    if (!okay) throw std::runtime_error(text);
    std::printf("PASS %s\n", text);
}

EditorRequests Frame(EditorDocument& document, EditorPanelState& state) {
    EditorRequests requests;
    ImGui::NewFrame();
    DrawHierarchyPanel(document, state, requests);
    ImGui::Render();
    return requests;
}

void RightClick(ImGuiIO& io, EditorDocument& doc, EditorPanelState& state, float x, float y) {
    io.AddMousePosEvent(x, y); Frame(doc, state);
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, true); Frame(doc, state);
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, false); Frame(doc, state);
    Frame(doc, state);
}

void Exercise(float scale, const char* theme) {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.DisplaySize = {1280, 800};
    io.DeltaTime = 1.f / 60;
    io.FontGlobalScale = scale;
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    EditorDocument document;
    auto& scene = document.GetScene();
    for (int i = 0; i < 512; ++i) scene.CreateObject("Entity " + std::to_string(i));
    const auto first = scene.Objects().front().id;
    const auto last = scene.Objects().back().id;
    EditorPanelState state;
    state.workspace.showRails = true;
    state.workspace.hierarchy = {{20, 20}, {380, 620}};
    if (std::string(theme) != "default") {
        EditorPreferences preferences;
        preferences.appearance.scale = scale;
        preferences.appearance.light = std::string(theme) == "light";
        preferences.ApplyPending(state);
    }
    Frame(document, state); Frame(document, state);
    auto* list = RowsWindow();
    Check(list && list->ScrollMax.y > 0, "large hierarchy has real scrolling extent");
    ImGui::SetScrollY(list, list->ScrollMax.y);
    Frame(document, state); Frame(document, state);
    std::printf("DIAGNOSTIC bottom first scroll=%g max=%g\n", list->Scroll.y, list->ScrollMax.y);
    ImGui::SetScrollY(list, list->ScrollMax.y);
    Frame(document, state); Frame(document, state);
    std::printf("DIAGNOSTIC bottom stable scroll=%g max=%g\n", list->Scroll.y, list->ScrollMax.y);
    Check(list->Scroll.y == list->ScrollMax.y, "last rows reached at bottom");

    const auto label = scene.Objects().back().name + "##" + std::to_string(last);
    const auto row = ImHashStr(label.c_str(), 0, list->GetID(static_cast<int>(last)));
    const float rowX = .5f * (list->InnerClipRect.Min.x + list->InnerClipRect.Max.x);
    const float rowY = list->DC.CursorMaxPos.y - .5f * io.Fonts->Fonts[0]->FontSize * scale;
    Check(list->InnerClipRect.Contains({rowX, rowY}), "last row click lies inside the clipped list");
    document.Select(first);
    io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddMousePosEvent(rowX, rowY); Frame(document, state);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true); Frame(document, state);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false); Frame(document, state);
    io.AddKeyEvent(ImGuiMod_Shift, false); Frame(document, state);
    Check(document.Selected() == last && document.Selection().size() == 512,
          "mouse Shift selection spans the full authored list across hidden rows");

    RightClick(io, document, state, rowX, rowY);
    auto& popups = ImGui::GetCurrentContext()->OpenPopupStack;
    Check(!popups.empty() && popups.back().Window && popups.back().PopupId == row,
          "right-click reaches the correct clipped bottom row");
    ImGui::ActivateItemByID(popups.back().Window->GetID("Create child"));
    Frame(document, state); Frame(document, state);
    Check(popups.size() >= 2 && popups.back().Window, "row opens the real Create child submenu");
    ImGui::ActivateItemByID(popups.back().Window->GetID("Empty object"));
    auto child = Frame(document, state);
    if (child.createKind.empty()) child = Frame(document, state);
    Check(child.createKind == "empty" && child.createParent == last, "submenu targets the exact bottom-row parent");

    document.Select(last);
    ImGui::FocusWindow(list);
    const auto printFocus = [&] (const char* step) {
        const auto& gui = *ImGui::GetCurrentContext();
        std::printf("DIAGNOSTIC %s focus=%s text=%d popups=%d active=%u rename=%llu\n", step,
                    gui.NavWindow ? gui.NavWindow->Name : "(none)", io.WantTextInput, popups.Size,
                    gui.ActiveId, static_cast<unsigned long long>(state.renamingId));
    };
    printFocus("menu-closed");
    Frame(document, state);
    Frame(document, state);
    printFocus("idle");
    io.AddKeyEvent(ImGuiKey_F2, true); Frame(document, state);
    printFocus("f2-down");
    io.AddKeyEvent(ImGuiKey_F2, false); Frame(document, state);
    printFocus("f2-up");
    Check(state.renamingId == last, "F2 begins inline rename on the scrolled row");
    io.AddInputCharactersUTF8("Renamed bottom");
    Frame(document, state);
    io.AddKeyEvent(ImGuiKey_Enter, true); Frame(document, state);
    io.AddKeyEvent(ImGuiKey_Enter, false); Frame(document, state);
    Check(scene.Find(last)->name == "Renamed bottom" && state.renamingId == kInvalidSceneObjectId,
          "inline rename commits typed text");
    document.Undo();
    Check(scene.Find(last)->name == "Entity 511", "one undo restores the old name");
    ImGui::DestroyContext();
}
} // namespace

int main() {
    try {
        for (const char* theme : {"default", "dark", "light"}) {
            for (float scale : {1.f, 1.5f}) {
                std::printf("THEME %s SCALE %.2f\n", theme, scale);
                Exercise(scale, theme);
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL %s\n", error.what());
        return 1;
    }
}
