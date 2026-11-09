#include "EngineHost.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "editor/TerrainV2Editor.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
}
float Highest(const std::shared_ptr<const MeshData> &mesh) {
    float h = -1e9f;
    for (auto &v : mesh->vertices)
        h = std::max(h, v.position.y);
    return h;
}
} // namespace
int main(int argc, char **argv) {
    try {
        const auto input =
            argc > 2 ? fs::path(argv[2]) :
            fs::path(__FILE__).parent_path().parent_path() / "projects/terrain_v2_workshop";
        const fs::path root = argc > 1 ? argv[1] : "/tmp/judas-terrain-v2-editor-check";
        Check(!fs::exists(root), "UI qualification uses a fresh disposable project copy");
        fs::create_directories(root);
        fs::copy(input, root, fs::copy_options::recursive);
        std::string error;
        Project project;
        Check(project.Load((root / "terrain_v2_workshop.judasproj").string(), error),
              "load editable workshop: " + error);
        EngineHost host;
        Check(host.Init("Terrain V2 authoring qualification", 1000, 800, false, error),
              "initialize existing editor services: " + error);
        host.OpenProjectAssets(project.RootDir(), project.AssetsDir());
        auto *context = ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = {1000, 800};
        io.DeltaTime = 1.f / 60;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        unsigned char *pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        EditorDocument document;
        Check(document.Load(project.StartupScenePath(), error),
              "open ordinary scene document: " + error);
        EditorPanelState state;
        state.project = &project;
        state.assets = &host.Assets();
        state.importJobs = &host.Jobs();
        state.showTerrain = true;
        EditorRequests requests;
        TerrainV2Editor editor;
        auto frame = [&] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({650, 760}, ImGuiCond_Always);
            if (!editor.Panel(document, state, requests))
                throw std::runtime_error("V2 source selects V2 authoring module");
            ImGui::Render();
            editor.Prepare(host.GetRenderer(), state);
        };
        auto activate = [&](const char *label) {
            auto *window = ImGui::FindWindowByName("Terrain - sculpt / paint");
            ImGui::ActivateItemByID(window->GetID(label));
            frame();
        };
        auto wait = [&] {
            for (unsigned i = 0; i < 5000; ++i) {
                frame();
                if (!editor.Busy())
                    return;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            throw std::runtime_error("editor preparation timeout");
        };
        frame();
        frame();
        activate("Edit landscape");
        wait();
        Check(editor.Loaded() && editor.Preview(),
              "real open control builds a native landscape preview");
        activate("Select landscape");
        Check(document.Selected() == 1, "selection control selects the actual terrain owner");
        const float before = Highest(editor.Preview());
        Check(editor.Stroke(document.GetScene(), 1, {0, 12, 0}, {0, -1, 0}, true, true, false, true,
                            state.status),
              "viewport starts an actual sculpt gesture");
        wait();
        editor.Stroke(document.GetScene(), 1, {0, 12, 0}, {0, -1, 0}, false, false, false, true,
                      state.status);
        Check(Highest(editor.Preview()) > before + .1f,
              "viewport stroke changes actual terrain geometry");
        Check(!editor.Guard(state), "unsaved source prevents stale Play");
        activate("Undo");
        wait();
        Check(std::abs(Highest(editor.Preview()) - before) < 1e-5f,
              "one undo restores the whole source gesture");
        activate("Redo");
        wait();
        Check(Highest(editor.Preview()) > before + .1f, "redo restores geometry");
        activate("Save and update playable terrain");
        wait();
        Check(editor.Guard(state), "single save/update completes source and playable products");
        Check(requests.rescanAssets, "published products request normal asset refresh");
        TerrainV2::Source saved;
        std::ifstream file(root / "Authoring/highlands.judasterrain2");
        std::string text((std::istreambuf_iterator<char>(file)), {});
        Check(TerrainV2::Parse(text, saved, error), "saved source is readable: " + error);
        Check(TerrainV2::Height(saved, 0, 0) > before + .1f,
              "sculpt survives actual disk persistence");
        editor.Shutdown(host.GetRenderer(), &host.Jobs());
        ImGui::DestroyContext(context);
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
