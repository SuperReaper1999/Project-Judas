#include "editor/EditorDesign.h"
#include "editor/EditorTheme.h"
#include "editor/EditorVisuals.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char *message) {
    ++checks;
    failures += !value;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", message);
    std::fflush(stdout);
}
struct Item {
    ImGuiID id = 0;
    ImRect rect;
};
struct Frame {
    ImGuiContext *context = nullptr;
    bool disabled = false, toggled = false, selected = false, focusAction = false, popupRequest = false;
    bool nativeAction = false;
    bool action = false, toggleChange = false, row = false, open = true, searchChange = false, choice = false;
    bool fieldPreserved = false, scopesBalanced = false, paddingFits = false;
    char search[128] = "", value[128] = "Original";
    Item actionItem, toggleItem, rowItem, headerItem, searchItem, valueItem, choiceItem, treeItem, spanItem;
    bool treeOpen = false, treeScopes = false, spanPressed = false;
    explicit Frame(bool withHeading = false) {
        context = ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = {900, 850};
        io.DeltaTime = 1.f / 60;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        if (withHeading) {
            const auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
            const auto regular = (root / "assets/fonts/DejaVuSans.ttf").string();
            const auto heading = (root / "assets/editor/DejaVuSans-Bold.ttf").string();
            Check(io.Fonts->AddFontFromFileTTF(regular.c_str(), 15) &&
                      io.Fonts->AddFontFromFileTTF(heading.c_str(), 15),
                  "measurement fixture loads the editor's actual distinct body and heading fonts");
        }
        unsigned char *pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        ApplyEditorTheme();
        Draw();
        Draw();
    }
    ~Frame() {
        ImGui::DestroyContext(context);
    }
    static Item Last() {
        return {ImGui::GetItemID(), {ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}};
    }
    void Draw() {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({20, 20}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({600, 760}, ImGuiCond_Always);
        ImGui::Begin("Judas design interaction controls");
        ImGui::BeginDisabled(disabled);
        if (focusAction) {
            ImGui::SetKeyboardFocusHere();
            focusAction = false;
        }
        action = nativeAction ? ImGui::Button("Run##primary")
                              : EditorDesign::ActionButton("Run##primary", EditorDesign::Tone::Primary);
        actionItem = Last();
        toggleChange = EditorDesign::Toggle("Enable shadows", &toggled);
        toggleItem = Last();
        row = EditorDesign::Row("Object##row", selected);
        rowItem = Last();
        if (row)
            selected = !selected;
        const auto canonical = ImGui::GetID("Original component title");
        open = EditorDesign::Disclosure("Friendly settings", true, canonical);
        headerItem = Last();
        const int treeDepth = ImGui::GetCurrentWindow()->IDStack.Size;
        treeOpen = EditorDesign::TreeRow("Folder", false, true);
        treeItem = Last();
        const bool leaf = EditorDesign::TreeRow("Leaf", false, false);
        treeScopes = leaf && ImGui::GetCurrentWindow()->IDStack.Size == treeDepth;
        searchChange = EditorDesign::Search("##search", "Search objects...", search, sizeof(search));
        searchItem = Last();
        auto *window = ImGui::GetCurrentWindow();
        const int ids = window->IDStack.Size, colors = ImGui::GetCurrentContext()->ColorStack.Size;
        const int styles = ImGui::GetCurrentContext()->StyleVarStack.Size;
        const float right = window->ContentRegionRect.Max.x, work = window->WorkRect.Max.x;
        const auto region = ImGui::GetCursorScreenPos();
        {
            EditorDesign::Surface card(EditorDesign::Tone::Secondary, {12, 10});
            ImGui::TextUnformatted("Object properties");
            const auto expected = ImGui::GetID("##value");
            {
                EditorDesign::FieldFrame frame;
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##value", value, sizeof(value));
                valueItem = Last();
            }
            fieldPreserved = ImGui::GetItemID() == expected && ImGui::GetItemID() == valueItem.id &&
                             ImGui::GetItemRectMin().x == valueItem.rect.Min.x &&
                             ImGui::GetItemRectMax().x == valueItem.rect.Max.x;
            paddingFits = valueItem.rect.Min.x >= region.x + 12 && valueItem.rect.Max.x <= right - 11;
        }
        scopesBalanced = window->IDStack.Size == ids &&
                         ImGui::GetCurrentContext()->ColorStack.Size == colors &&
                         ImGui::GetCurrentContext()->StyleVarStack.Size == styles &&
                         window->ContentRegionRect.Max.x == right && window->WorkRect.Max.x == work;
        spanPressed = false;
        if (ImGui::BeginTable("Detailed assets", 3, ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("State");
            ImGui::TableNextColumn();
            spanPressed = EditorDesign::Row("Asset spanning row", false, {}, false, 0, true);
            spanItem = Last();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Image");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Ready");
            ImGui::EndTable();
        }
        ImGui::EndDisabled();
        if (popupRequest) {
            ImGui::OpenPopup("Picker");
            popupRequest = false;
        }
        choice = false;
        if (ImGui::BeginPopup("Picker")) {
            choice = EditorDesign::Row("Choose this", false);
            choiceItem = Last();
            ImGui::EndPopup();
        }
        ImGui::End();
        ImGui::Render();
    }
    void Move(ImVec2 point) {
        ImGui::GetIO().AddMousePosEvent(point.x, point.y);
        Draw();
        Draw();
    }
    void Click(const Item &item) {
        Move(item.rect.GetCenter());
        ImGui::GetIO().AddMouseButtonEvent(0, true);
        Draw();
        ImGui::GetIO().AddMouseButtonEvent(0, false);
        Draw();
    }
    void Key(ImGuiKey key, bool down) {
        ImGui::GetIO().AddKeyEvent(key, down);
        Draw();
    }
    bool TabToAction() {
        for (unsigned attempt = 0; attempt < 20; ++attempt) {
            const auto *state = ImGui::GetCurrentContext();
            if (state->NavId == actionItem.id && state->NavCursorVisible)
                return true;
            Key(ImGuiKey_Tab, true);
            Key(ImGuiKey_Tab, false);
        }
        return false;
    }
};
} // namespace

int main() {
    {
        Frame ui;
        Check(ui.actionItem.id == ImHashStr("Run##primary", 0,
                                            ImGui::FindWindowByName("Judas design interaction controls")->ID),
              "custom primary action retains its native labelled widget identity");
        Check(ui.headerItem.id == ImGui::FindWindowByName("Judas design interaction controls")
                                      ->GetID("Original component title"),
              "friendly disclosure title retains its explicit original component identity");
        Check(
            ui.open && ui.fieldPreserved && ui.scopesBalanced && ui.paddingFits,
            "custom card and inset field retain native item metadata, IDs and balanced layout/style scopes");
        ui.Click(ui.actionItem);
        Check(ui.action, "native pointer press/release activates the custom primary action");
        ui.Draw();
        Check(!ui.action, "ordinary following frames do not repeat a released action");
        ui.Click(ui.toggleItem);
        Check(ui.toggleChange && ui.toggled,
              "custom toggle changes actual state through ordinary pointer input");
        ui.Click(ui.rowItem);
        Check(ui.row && ui.selected, "custom selectable row returns the native selection action");
        ui.Click(ui.headerItem);
        Check(!ui.open, "custom disclosure collapses through its actual native header hit target");
        ui.Click(ui.headerItem);
        Check(ui.open, "the same custom disclosure reopens without an extra ID or tree scope");
        Check(!ui.treeOpen && ui.treeScopes,
              "custom folder and leaf rows retain native open state without ID/tree pushes");
        const auto arrow = ImVec2(ui.treeItem.rect.Min.x + 10, ui.treeItem.rect.GetCenter().y);
        ui.Move(arrow);
        ImGui::GetIO().AddMouseButtonEvent(0, true);
        ui.Draw();
        ImGui::GetIO().AddMouseButtonEvent(0, false);
        ui.Draw();
        Check(ui.treeOpen && ui.treeScopes,
              "folder arrow opens the custom row using native tree interaction");
        const auto rowBody = ImVec2(ui.treeItem.rect.Max.x - 30, ui.treeItem.rect.GetCenter().y);
        ui.Move(rowBody);
        ImGui::GetIO().AddMouseButtonEvent(0, true);
        ui.Draw();
        ImGui::GetIO().AddMouseButtonEvent(0, false);
        ui.Draw();
        Check(ui.treeOpen, "ordinary folder body selection does not accidentally close its branch");
        const auto thirdColumn = ImVec2(ui.spanItem.rect.Max.x - 30, ui.spanItem.rect.GetCenter().y);
        ui.Move(thirdColumn);
        ImGui::GetIO().AddMouseButtonEvent(0, true);
        ui.Draw();
        ImGui::GetIO().AddMouseButtonEvent(0, false);
        ui.Draw();
        Check(ui.spanPressed,
              "detailed-list row remains clickable in its third column, beyond the Name cell");
        ui.Click(ui.searchItem);
        ImGui::GetIO().AddInputCharactersUTF8("crate");
        ui.Draw();
        Check(ui.searchChange && std::strcmp(ui.search, "crate") == 0 &&
                  ImGui::GetActiveID() == ui.searchItem.id,
              "custom search uses real native text editing and its original field identity");
        const auto field = ui.searchItem;
        const float glyph = ImGui::GetFontSize();
        const ImVec2 clear{field.rect.Max.x - glyph * .5f - 9, (field.rect.Min.y + field.rect.Max.y) * .5f};
        ui.Move(clear);
        ImGui::GetIO().AddMouseButtonEvent(0, true);
        ui.Draw();
        ImGui::GetIO().AddMouseButtonEvent(0, false);
        ui.Draw();
        const bool cleared = ui.searchChange && ui.search[0] == 0;
        ui.Draw();
        Check(cleared && ImGui::GetActiveID() == ui.searchItem.id,
              "search clear action empties the buffer and returns keyboard focus to the real text field");
        ui.Click(ui.valueItem);
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
        ImGui::GetIO().AddKeyEvent(ImGuiKey_A, true);
        ui.Draw();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_A, false);
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
        ui.Draw();
        ImGui::GetIO().AddInputCharactersUTF8("Edited");
        ui.Draw();
        Check(std::strcmp(ui.value, "Edited") == 0 && ui.fieldPreserved && ui.scopesBalanced,
              "a real input inside the custom card remains editable with native selection/text handling");
    }
    {
        Frame baseline;
        baseline.nativeAction = true;
        baseline.focusAction = true;
        baseline.Draw();
        baseline.Draw();
        baseline.Key(ImGuiKey_Space, true);
        const bool hiddenFocusPressed = baseline.action;
        baseline.Key(ImGuiKey_Space, false);
        Check(
            !hiddenFocusPressed && !baseline.action,
            "original hidden programmatic-focus stimulus also cannot Space-activate an unchanged native button");
        Check(baseline.TabToAction(), "real Tab navigation visibly focuses the native comparison button");
        baseline.Key(ImGuiKey_Space, true);
        const bool nativePressed = baseline.action;
        baseline.Key(ImGuiKey_Space, false);
        Check(nativePressed || baseline.action,
              "the same native button activates when keyboard navigation is established");
    }
    {
        Frame ui;
        Check(ui.TabToAction(), "real Tab navigation visibly focuses the painted action before activation");
        ui.Key(ImGuiKey_Space, true);
        const bool pressedDown = ui.action;
        ui.Key(ImGuiKey_Space, false);
        Check(pressedDown || ui.action, "native keyboard focus and Space activate the painted action");
        ui.disabled = true;
        ui.Draw();
        ui.Click(ui.actionItem);
        Check(!ui.action, "disabled action rejects ordinary pointer activation");
        ui.Click(ui.toggleItem);
        Check(!ui.toggleChange && !ui.toggled, "disabled toggle preserves state under pointer input");
        ui.Click(ui.searchItem);
        ImGui::GetIO().AddInputCharactersUTF8("blocked");
        ui.Draw();
        Check(!ui.searchChange && ui.search[0] == 0, "disabled search rejects native text input");
        ui.Key(ImGuiKey_Tab, true);
        ui.Key(ImGuiKey_Tab, false);
        ui.Key(ImGuiKey_Space, true);
        ui.Key(ImGuiKey_Space, false);
        Check(!ui.action && !ui.toggleChange && !ui.toggled,
              "native navigation cannot activate disabled custom controls");
    }
    {
        Frame ui;
        ui.popupRequest = true;
        ui.Draw();
        ui.Draw();
        ui.Click(ui.choiceItem);
        Check(ui.choice, "painted popup row responds to actual pointer selection");
        ui.Draw();
        Check(ImGui::GetCurrentContext()->OpenPopupStack.empty(),
              "custom picker rows close their native popup like an ordinary selectable");
    }
    {
        Frame ui;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({200, 180});
        ImGui::SetNextWindowCollapsed(true, ImGuiCond_Always);
        ImGui::Begin("Collapsed custom controls");
        auto *draw = ImGui::GetWindowDrawList();
        const int before = draw->VtxBuffer.Size;
        bool value = false;
        char text[16] = "";
        EditorDesign::ActionButton("Hidden action");
        EditorDesign::Toggle("Hidden toggle", &value);
        EditorDesign::Row("Hidden row", false);
        EditorDesign::Disclosure("Hidden heading");
        EditorDesign::TreeRow("Hidden folder", false, true);
        EditorDesign::Search("Hidden search", "", text, sizeof(text));
        {
            EditorDesign::Surface card;
            EditorDesign::Field field;
            ImGui::InputText("Hidden value", text, sizeof(text));
        }
        Check(draw->VtxBuffer.Size == before && !value && text[0] == 0,
              "SkipItems controls emit no custom geometry or state changes in a collapsed panel");
        ImGui::End();
        ImGui::SetNextWindowPos({0, 200});
        ImGui::SetNextWindowSize({240, 160});
        ImGui::Begin("Clipped custom controls");
        ImGui::SetCursorPosY(2000);
        draw = ImGui::GetWindowDrawList();
        const int clippedBefore = draw->VtxBuffer.Size;
        EditorDesign::ActionButton("Clipped action");
        EditorDesign::Toggle("Clipped toggle", &value);
        EditorDesign::Row("Clipped row", false);
        EditorDesign::Disclosure("Clipped heading");
        EditorDesign::TreeRow("Clipped folder", false, true);
        EditorDesign::Search("Clipped search", "", text, sizeof(text));
        {
            EditorDesign::Surface card;
            EditorDesign::Field field;
            ImGui::InputText("Clipped value", text, sizeof(text));
        }
        Check(draw->VtxBuffer.Size == clippedBefore && !value,
              "offscreen custom controls and surfaces skip painting while retaining normal layout");
        ImGui::End();
        ImGui::Render();
    }
    {
        Frame ui;
        bool partialBodyRow = false, preservedVerticalClip = true;
        for (unsigned frame = 0; frame < 4; ++frame) {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({600, 300});
            ImGui::Begin("Frozen list controls");
            if (ImGui::BeginTable("Scrolled frozen table", 3,
                                  ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV, {550, 155})) {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("State");
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();
                ImGui::SetScrollY(95);
                for (unsigned row = 0; row < 30; ++row) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    auto *window = ImGui::GetCurrentWindow();
                    auto *table = ImGui::GetCurrentTable();
                    const auto verticalClip = window->ClipRect;
                    const auto firstIndex = window->DrawList->IdxBuffer.Size;
                    const auto label = std::string("Scrolled asset ") + std::to_string(row);
                    EditorDesign::Row(label.c_str(), true, {0, 25}, false, 0, true);
                    const auto rectangle = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                    if (frame >= 2 && verticalClip.Min.y > table->InnerClipRect.Min.y + 1 &&
                        rectangle.Min.y < verticalClip.Min.y && rectangle.Max.y > verticalClip.Min.y) {
                        partialBodyRow = true;
                        for (const auto &command : window->DrawList->CmdBuffer)
                            if (command.ElemCount &&
                                command.IdxOffset + command.ElemCount > unsigned(firstIndex))
                                preservedVerticalClip &= command.ClipRect.y >= verticalClip.Min.y &&
                                                         command.ClipRect.w <= verticalClip.Max.y;
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Texture");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Ready");
                }
                ImGui::EndTable();
            }
            ImGui::End();
            ImGui::Render();
        }
        Check(
            partialBodyRow && preservedVerticalClip,
            "scrolled spanning rows keep the frozen-header vertical clip while expanding their horizontal hit area");
    }
    {
        Frame ui(true);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({600, 300});
        ImGui::Begin("Control caption geometry");
        const char *caption = "WWWWMMMM";
        const float leading = 20;
        EditorDesign::ActionButton(caption, EditorDesign::Tone::Primary, {}, leading);
        const auto actionRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        const float headingWidth =
            EditorVisuals::HeadingFont()->CalcTextSizeA(ImGui::GetFontSize(), INFINITY, 0, caption).x;
        Check(actionRect.GetWidth() >=
                  std::ceil(headingWidth) + leading + 2 * ImGui::GetStyle().FramePadding.x,
              "primary action auto-width measures the same heading font that paints its complete caption");
        auto *draw = ImGui::GetWindowDrawList();
        const auto firstVertex = draw->VtxBuffer.Size;
        bool enabled = true;
        EditorDesign::Toggle("Enabled", &enabled);
        const auto toggle = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        const auto color = ImGui::GetColorU32(ImGuiCol_Text);
        const float trackStart = toggle.Max.x - ImGui::GetStyle().FramePadding.x - 34;
        float actualRight = -INFINITY;
        for (int vertex = firstVertex; vertex < draw->VtxBuffer.Size; ++vertex)
            if (draw->VtxBuffer[vertex].col == color && draw->VtxBuffer[vertex].pos.x < trackStart)
                actualRight = std::max(actualRight, draw->VtxBuffer[vertex].pos.x);
        auto *font = ImGui::GetFont();
        const float scale = ImGui::GetFontSize() / font->FontSize;
        float pen = std::floor(toggle.Min.x + ImGui::GetStyle().FramePadding.x), expectedRight = pen;
        for (const unsigned char letter : std::string("Enabled"))
            if (const auto *glyph = font->FindGlyph(letter)) {
                expectedRight = std::max(expectedRight, pen + glyph->X1 * scale);
                pen += glyph->AdvanceX * scale;
            }
        Check(actualRight >= expectedRight - .01f && actualRight < trackStart,
              "toggle caption retains its complete final glyph with a clear gap before the switch");
        ImGui::End();
        ImGui::Render();
    }
    std::printf("Editor design: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
