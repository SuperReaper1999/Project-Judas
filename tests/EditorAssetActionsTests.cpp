#include "editor/EditorAssetActions.h"
#include "editor/EditorPanels.h"
#include "imgui.h"
#include <iostream>
#include <stdexcept>

int main() {
    unsigned checks = 0;
    const auto check = [&](bool okay, const char* label) {
        ++checks;
        if (!okay) throw std::runtime_error(label);
        std::cout << "PASS " << label << '\n';
    };
    try {
        EditorPanelState state;
        AssetRecord asset;
        asset.id = "12345678901234567890123456789012";
        asset.type = AssetType::Script;
        check(RequestEditorScriptEdit(asset, state) && state.editScriptAsset == asset.id,
              "script activation requests the existing source editor");
        state.editScriptAsset.clear();
        asset.missing = true;
        check(!RequestEditorScriptEdit(asset, state) && state.editScriptAsset.empty() &&
                  state.status.find("missing") != std::string::npos, "missing script gives actionable failure");
        asset.missing = false; asset.readOnly = true;
        check(!RequestEditorScriptEdit(asset, state) && state.editScriptAsset.empty() &&
                  state.status.find("read-only") != std::string::npos, "mod/read-only source is protected");
        asset.readOnly = false; state.mode = EditorMode::Play;
        check(RequestEditorScriptEdit(asset, state) && state.editScriptAsset == asset.id,
              "Play retains the existing deliberate source-edit/reload workflow");
        state.editScriptAsset.clear();
        state.mode = EditorMode::Edit; asset.type = AssetType::Mesh;
        check(!RequestEditorScriptEdit(asset, state) && state.editScriptAsset.empty(),
              "non-script activation retains the normal asset inspector route");

        EditorDocument doc;
        const auto first = doc.GetScene().CreateObject("First object").id;
        const auto second = doc.GetScene().CreateObject("Second object").id;
        check(!BeginEditorRename(doc, state), "no selection cannot start rename");
        doc.Select(first);
        check(BeginEditorRename(doc, state) && state.renamingId == first && state.renameBuffer == "First object",
              "rename begins on the exact selected entity with its existing name");
        check(doc.GetScene().Find(first)->name == "First object" && !doc.EditInProgress(),
              "starting rename does not change the authored scene or open a transaction");
        doc.Select(second, true); state.renamingId = 0;
        check(!BeginEditorRename(doc, state) && state.renamingId == 0, "ambiguous multi-selection does not rename");
        doc.Select(first); state.mode = EditorMode::Play;
        check(!BeginEditorRename(doc, state) && state.renamingId == 0, "rename is read-only during Play");
        state.mode = EditorMode::Edit;

        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr; io.LogFilename = nullptr;
        io.DisplaySize = {800, 600}; io.DeltaTime = 1.f / 60;
        unsigned char* pixels; int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        bool editingText = false;
        char text[32] = "An active field";
        const auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowFocus();
            ImGui::Begin("Hierarchy shortcut test");
            if (editingText) {
                ImGui::SetKeyboardFocusHere();
                ImGui::InputText("Name", text, sizeof(text));
            }
            HandleHierarchyRenameShortcut(doc, state);
            ImGui::End(); ImGui::Render();
        };
        frame(); frame(); io.AddKeyEvent(ImGuiKey_F2, true); frame();
        check(state.renamingId == first, "actual F2 input works in a focused hierarchy window");
        io.AddKeyEvent(ImGuiKey_F2, false); frame(); state.renamingId = 0;
        editingText = true; frame(); frame();
        io.AddKeyEvent(ImGuiKey_F2, true); frame();
        check(state.renamingId == 0, "text input owns F2 while a field is being edited");
        ImGui::DestroyContext();
        std::cout << "Editor asset actions: " << checks << " checks, 0 failures\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
