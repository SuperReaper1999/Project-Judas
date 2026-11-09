#include "EngineHost.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "editor/ComponentEditors.h"
#include "editor/TerrainV2Editor.h"
#include "editor/TerrainV2Editor.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "terrain/TerrainAsset.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
}
} // namespace
int main(int argc, char **argv) {
    try {
        namespace fs = std::filesystem;
        const fs::path root = argc > 1 ? argv[1] : "/tmp/judas-terrain-v2-fresh-project";
        Check(!fs::exists(root), "use a fresh disposable project");
        Project project;
        std::string error;
        Check(Project::CreateNew(root.string(), "Fresh terrain authoring", project, error),
              "create ordinary project: " + error);
        EngineHost host;
        Check(host.Init("Fresh terrain authoring qualification", 1000, 900, false, error), error);
        host.OpenProjectAssets(project.RootDir(), project.AssetsDir());
        auto *context = ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = {1000, 900};
        io.DeltaTime = 1.f / 60;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        unsigned char *pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        EditorDocument document;
        // Project::CreateNew creates directories/settings. The normal document
        // workflow supplies and saves a scene; it is not a hidden demo loader.
        document.NewScene();
        project.Settings().startupScene = "Scenes/main.judas";
        Check(document.SaveAs(project.StartupScenePath(), error) && project.Save(error),
              "save an empty new scene through the ordinary project/document workflow");
        Check(document.Load(project.StartupScenePath(), error),
              "load fresh startup scene: " + error);
        EditorPanelState state;
        state.project = &project;
        state.assets = &host.Assets();
        state.resources = &host.Resources();
        state.importJobs = &host.Jobs();
        state.showTerrain = true;
        EditorRequests requests;
        TerrainV2Editor tools;
        auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({700, 850}, ImGuiCond_Always);
            tools.Panel(document, state, requests);
            ImGui::Render();
            tools.Prepare(host.GetRenderer(), state);
        };
        auto click = [&](const char *label) {
            auto *window = ImGui::FindWindowByName("Terrain - sculpt / paint");
            for (const auto &popup : context->OpenPopupStack)
                if (popup.Window && popup.Window->Active)
                    window = popup.Window;
            ImGui::ActivateItemByID(window->GetID(label));
            frame();
        };
        auto wait = [&] {
            for (unsigned i = 0; i < 10000; ++i) {
                frame();
                if (!tools.Busy())
                    return;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            throw std::runtime_error("fresh workflow preparation timeout");
        };
        frame();
        frame();
        click("New landscape");
        frame();
        Check(tools.Active(), "normal terrain panel opens V2 creation without authored files");
        click("Create landscape");
        Check(!tools.Guard(state), "pending creation blocks project/Play lifetime changes");
        wait();
        const auto first = document.Selected();
        const auto *owner = document.GetScene().Find(first);
        Check(owner && owner->landscape && owner->render && owner->body,
              "creation inserts and selects a complete native terrain owner");
        Check(document.IsDirty() && document.CanUndo(),
              "scene insertion is an ordinary undoable edit");
        const auto firstMesh = owner->render->meshAsset;
        const auto sourceId = owner->landscape->asset;
        const auto *native = host.Assets().Find(sourceId);
        TerrainV2::Source source;
        Check(native && native->type == AssetType::Terrain &&
                  TerrainV2::LoadAsset(native->path, source, error),
              "created asset is registered and readable: " + error);
        Check(source.samplesX == 129 && source.heights.front() == 0,
              "default terrain is a sensible editable flat surface at zero");
        Check(tools.Loaded() && tools.Preview() && tools.Sources().size() == 1,
              "creation enters live viewport editing automatically");
        const auto firstPath = tools.Sources().front();
        document.Undo();
        Check(!document.GetScene().Find(first),
              "scene Undo removes new owner without deleting assets");
        document.Redo();
        document.Select(first);
        Check(document.GetScene().Find(first) && host.Assets().Find(sourceId),
              "Redo restores placement and asset identity");
        Check(tools.Stroke(document.GetScene(), first, {0, 10, 0}, {0, -1, 0}, true, true,
                               false, true, state.status),
              "sculpt fresh terrain through actual viewport hook");
        wait();
        tools.Stroke(document.GetScene(), first, {0, 10, 0}, {0, -1, 0}, false, false, false,
                         true, state.status);
        Check(!tools.Guard(state), "unsaved source edit cannot silently enter Play");
        click("Save and update playable terrain");
        wait();
        Check(tools.Guard(state), "one publish action makes source and gameplay agree");
        // Import an original texture through the ordinary asset registry, then
        // select it through the palette's real layer dropdown.
        std::string tga(22, '\0');
        tga[2] = 2;
        tga[12] = tga[14] = 1;
        tga[16] = 32;
        tga[17] = 8;
        tga[18] = 70;
        tga[19] = 120;
        tga[20] = 180;
        tga[21] = char(255);
        const auto texturePath = root / "Assets/test-material.tga";
        {
            std::ofstream file(texturePath, std::ios::binary);
            file.write(tga.data(), tga.size());
        }
        AssetRecord texture;
        Check(host.Assets().Track(texturePath.string(), texture, error),
              "import source material normally");
        click("Surface materials");
        auto *panel = ImGui::FindWindowByName("Terrain - sculpt / paint");
        const auto layer = panel->GetID(0);
        ImGui::ActivateItemByID(ImHashStr("Texture", 0, layer));
        frame();
        click(texture.relativePath.c_str());
        Check(!tools.Guard(state), "unapplied palette choice cannot silently start Play");
        auto scalar = [&](const char *label, const char *value) {
            ImGui::ActivateItemByID(ImHashStr(label, 0, layer));
            context->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
            frame();
            io.AddKeyEvent(ImGuiMod_Ctrl, true);
            io.AddKeyEvent(ImGuiKey_A, true);
            frame();
            io.AddKeyEvent(ImGuiKey_A, false);
            io.AddKeyEvent(ImGuiMod_Ctrl, false);
            frame();
            io.AddInputCharactersUTF8(value);
            frame();
            io.AddKeyEvent(ImGuiKey_Enter, true);
            frame();
            io.AddKeyEvent(ImGuiKey_Enter, false);
            frame();
        };
        scalar("Tile size (metres)", "2");
        scalar("PBR roughness multiplier", "0.4");
        click("Shared normal detail");
        click("Normal texture");
        click(texture.relativePath.c_str());
        click("Apply surface materials");
        click("Surface materials");
        click("Undo");
        wait();
        click("Redo");
        wait();
        click("Save and update playable terrain");
        wait();
        const auto nativePath = host.Assets().Find(sourceId)->path;
        Check(TerrainV2::LoadAsset(nativePath, source, error) &&
                  source.layerTextures[0] == texture.id &&
                  source.materialDetail.normalTexture == texture.id &&
                  source.materialDetail.tileMetres.x == 2.f &&
                  source.materialDetail.roughness.x == .4f,
              "palette texture survives source undo/redo and native asset publication");
        Check(tools.Guard(state), "published material changes permit Play again");
        Check(document.Save(error), "ordinary scene Save persists terrain ownership: " + error);
        RuntimeWorld world;

        Check(world.Build(document.GetScene(), &host.Resources(), error),
              "new project's normal scene starts with real native collision: " + error);
        Check(world.HasRuntimeTerrain(first) && world.LiveTerrain().Status(first)->ready,
              "new terrain needs no special gameplay bootstrap");
        world.Destroy();
        click("New landscape");
        click("Create landscape");
        wait();
        const auto second = document.Selected();
        Check(second != first && tools.Sources().size() == 2,
              "second source gets independent assets and appears in the source chooser");
        click("Source");
        click(fs::path(firstPath).filename().string().c_str());
        wait();
        // Both generated names share a prefix; stable filenames, not ID order,
        // determine the chooser's deterministic ordering.
        Check(tools.MeshId() == firstMesh,
              "choosing another source reopens its actual native preview");
        Check(document.Selected() &&
                  document.GetScene().Find(document.Selected())->render->meshAsset ==
                      tools.MeshId(),
              "source choice immediately selects its actual scene owner");
        Check(document.GetScene().Find(first)->render->meshAsset == firstMesh,
              "creating and switching sources preserves existing terrain references");
        // A real native file chooser feeds the same high-bit-depth importer.
        const unsigned char png[] = {
            137, 80,  78,  71,  13,  10, 26, 10, 0,  0,  0,  13,  73,  72,  68, 82, 0,  0,  0,
            2,   0,   0,   0,   2,   16, 0,  0,  0,  0,  7,  77,  142, 187, 0,  0,  0,  16, 73,
            68,  65,  84,  120, 156, 99, 96, 96, 96, 96, 4,  194, 255, 255, 1,  3,  17, 2,  1,
            218, 231, 163, 127, 0,   0,  0,  0,  73, 69, 78, 68,  174, 66,  96, 130};
        const auto imagePath = root / "heightmap16.png";
        {
            std::ofstream file(imagePath, std::ios::binary);
            file.write(reinterpret_cast<const char *>(png), sizeof(png));
        }
        click("New landscape");
        click("Start from");
        // ImGui::Combo scopes its item IDs by index; this differs from the
        // source chooser's custom BeginCombo/Selectable controls above.
        auto *presets = context->OpenPopupStack.back().Window;
        ImGui::ActivateItemByID(ImHashStr("Import heightmap", 0, presets->GetID(3)));
        frame();
        click("Browse heightmap");
        auto *dialog = ImGui::FindWindowByName("Choose a path");
        ImGuiWindow *files = nullptr;
        for (auto *candidate : context->Windows)
            if (candidate->Active && candidate->ParentWindow == dialog &&
                (candidate->Flags & ImGuiWindowFlags_ChildWindow))
                files = candidate;
        if (!files)
            for (auto *candidate : context->Windows)
                if (candidate->Active)
                    std::cerr << "UI_WINDOW name=" << candidate->Name << " parent="
                              << (candidate->ParentWindow ? candidate->ParentWindow->Name : "none")
                              << " flags=" << candidate->Flags << '\n';
        Check(files != nullptr, "heightmap workflow opens the existing native file browser");
        ImGui::ActivateItemByID(files->GetID("         heightmap16.png"));
        frame();
        click("Choose file");
        click("Create landscape");
        wait();
        const auto third = document.Selected();
        const auto *imported = document.GetScene().Find(third);
        const auto *importedAsset = imported && imported->landscape
                                        ? host.Assets().Find(imported->landscape->asset)
                                        : nullptr;
        Check(importedAsset && TerrainV2::LoadAsset(importedAsset->path, source, error),
              "image workflow publishes a normal native terrain asset");
        Check(source.heights.front() == 0 && source.heights[128] > 0 &&
                  source.heights[128] < .001f && source.heights.back() == 32,
              "UI import preserves one-unit16-bit differences, selected scale and orientation");
        const auto stem = fs::path(importedAsset->path).stem().string();
        const auto original = root / "Authoring" / (stem + ".heightmap.png");
        const auto recipe = root / "Authoring" / (stem + ".recipe.json");
        Check(fs::exists(original) && fs::exists(recipe) && fs::file_size(original) == sizeof(png),
              "source heightmap and generation recipe are preserved for later handover");
        click("Tool");
        auto *toolsMenu = context->OpenPopupStack.back().Window;
        ImGui::ActivateItemByID(ImHashStr("Paint", 0, toolsMenu->GetID(4)));
        frame();
        const auto previousMesh = tools.Preview();
        const auto oldWeight = source.weights[64 * 129 + 64].x;
        Check(tools.Stroke(document.GetScene(), third, {0, 100, 0}, {0, -1, 0}, true, true,
                               false, true, state.status),
              "fresh image terrain accepts an actual paint gesture");
        frame();
        tools.Stroke(document.GetScene(), third, {0, 100, 0}, {0, -1, 0}, false, false, false,
                         true, state.status);
        Check(!tools.Busy() && tools.Preview() == previousMesh,
              "material-only painting does not rebuild or replace terrain geometry");
        click("Save and update playable terrain");
        wait();
        Check(TerrainV2::LoadAsset(importedAsset->path, source, error) &&
                  source.weights[64 * 129 + 64].x > oldWeight,
              "painted material samples survive native source publication");
        Check(document.Save(error),
              "multiple native terrain sources save through the ordinary scene document");
        const ComponentEditor *descriptor = nullptr;
        for (const auto &entry : ComponentEditorRegistry())
            if (std::string(entry.name) == "Landscape")
                descriptor = &entry;
        Check(descriptor && descriptor->has(*document.GetScene().Find(first)) &&
                  descriptor->requestAdd && !descriptor->add,
              "native component owns a real inspector and asset-backed creation workflow");
        descriptor->requestAdd(state);
        frame();
        Check(tools.Active() && !state.terrainCreateRequested,
              "registry creation request is consumed locally by the terrain workflow");
        click("Cancel new landscape");
        document.BeginEdit();
        descriptor->remove(*document.GetScene().Find(first));
        document.CommitEdit();
        Check(!document.GetScene().Find(first)->landscape &&
                  !document.GetScene().Find(first)->body && document.GetScene().Find(first)->render,
              "removing source physics never exposes the identity Box as an intact collider");
        document.Undo();
        state.terrainEditAsset = sourceId;
        frame();
        wait();
        Check(document.Selected() == first && tools.MeshId() == firstMesh,
              "inspector/asset request reopens the correct source and selects its owner");
        document.BeginEdit();
        auto &ground = document.GetScene().CreateObject("Ground");
        ground.transform.position = {12, 4.5f, -7};
        ground.body = SceneBodyComponent{};
        ground.body->halfExtents = {20, .5f, 20};
        ground.render = SceneRenderComponent{};
        ground.render->halfExtents = ground.body->halfExtents;
        ground.gravity = SceneGravityComponent{};
        ground.gravity->kind = SceneGravityKind::Uniform;
        const auto groundId = ground.id;
        document.CommitEdit();
        document.Select(groundId);
        click("New landscape");
        click("Replace selected ground");
        click("Create landscape");
        wait();
        const auto replacement = document.Selected();
        Check(!document.GetScene().Find(groundId)->body->enabled &&
                  !document.GetScene().Find(groundId)->render->visible &&
                  document.GetScene().Find(groundId)->gravity->magnitude == 9.81f,
              "explicit ground replacement prevents duplicate hidden collision while retaining "
              "gravity");
        Check(document.GetScene().Find(replacement)->transform.position == glm::vec3(12, 5, -7),
              "replacement terrain is placed on the original ground surface");
        document.Undo();
        Check(!document.GetScene().Find(replacement) &&
                  document.GetScene().Find(groundId)->body->enabled &&
                  document.GetScene().Find(groundId)->render->visible,
              "one ordinary scene undo restores the original ground completely");
        document.Redo();
        Check(document.GetScene().Find(replacement) &&
                  !document.GetScene().Find(groundId)->body->enabled,
              "redo reinstates replacement without duplicating support");
        tools.Shutdown(host.GetRenderer(), &host.Jobs());
        ImGui::DestroyContext(context);
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
