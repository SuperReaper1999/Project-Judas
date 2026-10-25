#include "editor/EditorAssetActions.h"
#include "editor/EditorPanels.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "Project.h"
#include "Prefab.h"
#include <filesystem>
#include <fstream>
#include <chrono>
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
        editingText = false; io.AddKeyEvent(ImGuiKey_F2, false); frame();
        state.mode = EditorMode::Edit;
        state.importSourceInput = "/source/example.js";
        state.importDestinationInput = "Scripts/example.js";
        const auto review = [&] {
            EditorRequests requests;
            ImGui::NewFrame(); ImGui::Begin("Import review host");
            DrawEditorImportReview(state, requests);
            ImGui::End(); ImGui::Render(); return requests;
        };
        state.reviewAssetImport = true; review(); review();
        auto* popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        ImGui::ActivateItemByID(popup->GetID("Import file"));
        auto imported = review(); if (imported.importSource.empty()) imported = review();
        check(imported.importSource == state.importSourceInput && imported.importDestination == state.importDestinationInput,
              "actual import review emits the reviewed source and destination");
        check(review().importSource.empty(), "import command is emitted only once");
        state.mode = EditorMode::Play; state.reviewAssetImport = true; review(); review();
        popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        ImGui::ActivateItemByID(popup->GetID("Import file"));
        check(review().importSource.empty() && review().importSource.empty(), "import review cannot mutate assets during Play");
        io.AddKeyEvent(ImGuiKey_Escape, true); review(); io.AddKeyEvent(ImGuiKey_Escape, false); review();
        state.mode = EditorMode::Edit;

        const auto root = std::filesystem::temp_directory_path() / ("judas-import-review-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(root);
        const auto source = root / "example.js";
        { std::ofstream file(source); file << "export default class { constructor(){this.state={ready:true};} }"; }
        Project project; std::string error;
        check(Project::CreateNew((root / "project").string(), "Import review", project, error), "isolated import project created");
        AssetDatabase database; database.Scan(project.RootDir(), project.AssetsDir()); AssetRecord script;
        check(database.Import(source.string(), imported.importDestination, script, error) && script.type == AssetType::Script,
              "reviewed destination imports and registers a genuine script asset");
        check(std::filesystem::exists(source) && std::filesystem::exists(script.path + ".judasmeta"),
              "import preserves original source and creates ordinary asset metadata");
        std::filesystem::remove_all(root);

        doc.Select(first); doc.GetScene().Find(first)->transform.position = {3,4,5};
        doc.GetScene().Find(second)->transform.position = {10,0,0};
        check(doc.ReparentSelection(second, error), "hierarchy parent action uses normal validated reparenting");
        Scene flattened;
        check(FlattenHierarchy(doc.GetScene(), flattened, error) && flattened.Find(first)->transform.position == glm::vec3(3,4,5),
              "parenting preserves actual world placement");
        doc.Undo(); check(doc.GetScene().Find(first)->parent == 0, "parent change is one undoable edit");
        doc.Redo(); doc.Select(second);
        check(!doc.ReparentSelection(first, error) && doc.GetScene().Find(second)->parent == 0,
              "cyclic parent choice rejects without changing the hierarchy");
        ImGui::DestroyContext();
        std::cout << "Editor asset actions: " << checks << " checks, 0 failures\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
