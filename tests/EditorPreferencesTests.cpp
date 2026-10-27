#include "editor/EditorPreferences.h"
#include "editor/EditorPanels.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include "EngineHost.h"
#include "ScreenshotWriter.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    unsigned checks = 0;
    auto check = [&](bool okay, const char* label) {
        ++checks;
        if (!okay) throw std::runtime_error(label);
        std::cout << "PASS " << label << '\n';
    };
    try {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.LogFilename = nullptr;
        io.DisplaySize = {1200, 800}; io.DeltaTime = 1.f / 60;
        unsigned char* pixels; int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        EditorPanelState state;
        EditorPreferences prefs;
        std::string error;
        check(prefs.Load(R"({"version":1,"hierarchyWidth":310,"inspectorWidth":420,"assetBrowserHeight":100,"showAssets":false})", state, error), "legacy workspace preferences load");
        prefs.ApplyPending(state);
        check(state.hierarchyWidth == 310 && state.inspectorWidth == 420 && state.assetBrowserHeight == 100 && !state.showAssetBrowser,
              "legacy custom dimensions including small asset rail survive migration");
        check(!prefs.ConsumeSaveRequest(), "loading preferences does not request a disk rewrite");
        const auto windowFrame = [] {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({45, 75}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({300, 200}, ImGuiCond_Always);
            ImGui::Begin("Saved tool"); ImGui::TextUnformatted("A movable tool window"); ImGui::End();
            ImGui::Render();
        };
        windowFrame(); windowFrame();
        state.showScriptTools = true; state.showAssetBrowser = true; state.showRuntimeInspector = true;
        check(prefs.SaveLayout("Scripting", state, false, error), "save named layout from real ImGui window settings");
        check(prefs.layouts[0].windows.find("Saved tool") != std::string::npos, "layout stores tool-window positions and sizes");
        check(!prefs.SaveLayout("Scripting", state, false, error) && prefs.layouts.size() == 1, "new save cannot silently overwrite a name");
        state.inspectorWidth = 500;
        check(prefs.SaveLayout("Scripting", state, true, error) && prefs.layouts[0].inspector == 500, "explicit update modifies selected layout");
        check(!prefs.SaveLayout("  ", state, false, error), "empty-looking layout names reject");
        state.inspectorWidth = 260; state.showScriptTools = false; state.showRuntimeInspector = false;
        check(prefs.QueueLayout("Scripting") && state.inspectorWidth == 260, "apply is queued rather than changing an active frame");
        ImGui::SetWindowPos("Saved tool", {600, 500});
        prefs.ApplyPending(state);
        check(state.inspectorWidth == 500 && state.showScriptTools && state.showRuntimeInspector, "named layout restores panel dimensions and visibility");
        ImGui::NewFrame(); ImGui::Begin("Saved tool");
        check(ImGui::GetWindowPos().x == 45 && ImGui::GetWindowPos().y == 75, "named layout actually restores an existing tool's position");
        ImGui::End(); ImGui::Render();
        check(!prefs.QueueLayout("Missing layout"), "absent layout does not enqueue a replacement");
        prefs.appearance.light = true; prefs.appearance.background = {.94f,.95f,.97f};
        prefs.appearance.text = {.1f,.13f,.17f}; prefs.appearance.accent = {.5f,.2f,.7f}; prefs.appearance.scale = 1.2f;
        prefs.RefreshAppearance(); prefs.ApplyPending(state);
        check(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x > .9f && ImGui::GetStyle().Colors[ImGuiCol_Text].x < .2f,
              "light appearance changes actual editor style colours");
        check(ImGui::GetStyle().Colors[ImGuiCol_CheckMark].x == .5f && io.FontGlobalScale == 1.2f,
              "custom accent and scaling reach ImGui");
        const auto padding = ImGui::GetStyle().WindowPadding.x;
        prefs.RefreshAppearance(); prefs.ApplyPending(state);
        check(ImGui::GetStyle().WindowPadding.x == padding, "repeated theme application does not accumulate scale");
        prefs.automaticUpdates = false; prefs.updateChannel = "Alpha";
        const auto serialized = prefs.Serialize(state);
        EditorPreferences fresh; EditorPanelState restored;
        check(fresh.Load(serialized, restored, error), "v2 theme and named layouts deserialize in a fresh preferences owner");
        fresh.ApplyPending(restored);
        check(fresh.appearance.light && fresh.appearance.accent == prefs.appearance.accent && fresh.layouts.size() == 1 && restored.showScriptTools,
              "custom theme, saved layout and current panel visibility survive roundtrip");
        check(!fresh.automaticUpdates && fresh.updateChannel == "Alpha", "update startup policy and explicit channel persist independently of project settings");
        auto invalidUpdate = nlohmann::json::parse(serialized);
        invalidUpdate["updates"]["channel"] = "main";
        check(!fresh.Load(invalidUpdate.dump(), restored, error) && fresh.updateChannel == "Alpha" && !fresh.automaticUpdates,
              "unknown update channel rejects transactionally without opting into Experimental");
        const auto current = nlohmann::json::parse(serialized);
        for (const char* field : {"scale", "accent", "mode"}) {
            auto broken = current;
            if (std::string(field) == "scale") broken["appearance"][field] = 1000;
            else if (std::string(field) == "accent") broken["appearance"][field] = {1, 2, 3};
            else broken["appearance"][field] = "neon-unknown";
            const auto oldWidth = restored.inspectorWidth;
            check(!fresh.Load(broken.dump(), restored, error) && restored.inspectorWidth == oldWidth && fresh.appearance.light,
                  "invalid theme rejects transactionally");
        }
        for (auto broken : {nlohmann::json{{"version", 99}}, nlohmann::json::array(), current}) {
            if (broken == current) broken["layouts"].push_back(broken["layouts"][0]);
            check(!fresh.Load(broken.dump(), restored, error), "unknown schema or duplicate layouts reject");
        }
        check(!fresh.Load("{broken", restored, error), "malformed preference JSON is reported without throwing");
        auto badWidth = current; badWidth["current"]["inspectorWidth"] = -1;
        check(!fresh.Load(badWidth.dump(), restored, error), "invalid layout geometry rejects without publication");
        auto partialLight = current; partialLight["appearance"] = {{"mode", "light"}};
        EditorPreferences partial; EditorPanelState partialState;
        check(partial.Load(partialLight.dump(), partialState, error) && partial.appearance.background[0] > .9f,
              "partial light preferences use readable light defaults");
        const auto root = std::filesystem::temp_directory_path() / ("judas-preferences-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(root);
        const auto file = (root / "workspace.json").string();
        check(prefs.SaveFile(file, state, error), "preferences write atomically to a user-selected test file");
        std::ifstream original(file); const std::string before{std::istreambuf_iterator<char>(original), {}};
        original.close();
        check(prefs.SaveFile(file, state, error) && std::filesystem::exists(file + ".bak"), "subsequent save preserves a previous-file backup");
        std::ifstream backup(file + ".bak"); const std::string old{std::istreambuf_iterator<char>(backup), {}};
        backup.close();
        check(before == old, "backup preserves exact prior bytes");
        prefs.appearance.scale = 1000;
        check(!prefs.SaveFile(file, state, error), "invalid in-memory settings cannot replace a valid preferences file");
        prefs.appearance.scale = 1.2f;
        check(!prefs.SaveFile((root / "workspace.json" / "prefs.json").string(), state, error), "invalid file-as-directory destination reports failure");
        check(prefs.DeleteLayout("Scripting") && !prefs.DeleteLayout("Scripting"), "explicit deletion removes only the requested layout");
        prefs.ResetLayout(); prefs.ApplyPending(state);
        check(state.hierarchyWidth == 250 && state.inspectorWidth == 340 && state.showAssetBrowser && !state.showScriptTools,
              "reset restores default workspace dimensions and panels");
        state.showEditorPreferences = true;
        ImGui::NewFrame(); prefs.Draw(state); ImGui::Render();
        check(state.showEditorPreferences, "actual preference UI draws without changing scene/editor mode");
        check(!state.project && state.mode == EditorMode::Edit, "preferences need no project and do not enter Play");
        if (argc > 1) {
            const std::filesystem::path output = argv[1]; std::filesystem::create_directories(output);
            EngineHost host;
            check(host.Init("Editor appearance validation", 1000, 800, false, error), "native preference graphics host starts");
            auto& window = host.GetWindow();
            ImGui_ImplSDL2_InitForOpenGL(window.NativeWindow(), window.NativeGLContext());
            ImGui_ImplOpenGL3_Init("#version 330 core");
            for (bool light : {false, true}) {
                prefs.appearance = EditorAppearance{};
                if (light) {
                    prefs.appearance.light = true; prefs.appearance.background = {.94f,.95f,.97f};
                    prefs.appearance.text = {.1f,.13f,.17f}; prefs.appearance.accent = {.16f,.38f,.64f};
                }
                prefs.RefreshAppearance(); prefs.ApplyPending(state);
                for (int frame = 0; frame < 3; ++frame) {
                    host.GetRenderer().BeginFrame(window.Width(), window.Height()); host.GetRenderer().EndFrame();
                    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
                    prefs.Draw(state); ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                }
                std::vector<unsigned char> captured;
                host.GetRenderer().CaptureFrame(window.Width(), window.Height(), captured);
                check(WriteRgbPng((output / (light ? "light.png" : "dark.png")).string(), window.Width(), window.Height(), captured),
                      "actual theme controls render to native framebuffer");
            }
            // Distinct frames let ImGui's normal input trickling publish movement,
            // press and release before inspecting the resulting visible UI.
            const auto click = [&](ImVec2 position) {
                for (int frame = 0; frame < 6; ++frame) {
                    host.GetRenderer().BeginFrame(window.Width(), window.Height()); host.GetRenderer().EndFrame();
                    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame();
                    io.AddMousePosEvent(position.x, position.y);
                    if (frame == 1) io.AddMouseButtonEvent(0, true);
                    if (frame == 3) io.AddMouseButtonEvent(0, false);
                    ImGui::NewFrame(); prefs.Draw(state); ImGui::Render();
                    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                }
            };
            click({190, 138});
            auto* tabbar = ImGui::GetCurrentContext()->TabBars.GetByIndex(0);
            auto* selectedTab = ImGui::TabBarFindTabByID(tabbar, tabbar->VisibleTabId);
            check(selectedTab && std::string(ImGui::TabBarGetTabName(tabbar, selectedTab)) == "Layouts",
                  "actual mouse input selects the visible Layouts tab");
            std::vector<unsigned char> layoutPixels;
            host.GetRenderer().CaptureFrame(window.Width(), window.Height(), layoutPixels);
            check(WriteRgbPng((output / "layouts.png").string(), window.Width(), window.Height(), layoutPixels),
                  "layout controls render after actual mouse tab selection");
            prefs.ConsumeSaveRequest();
            // The additional Runtime Inspector panel adds one visible table row.
            click({140, 609});
            check(prefs.layouts.size() == 1 && prefs.layouts[0].name == "My layout",
                  "actual Save as new layout button captures the current workspace");
            check(prefs.ConsumeSaveRequest(), "UI layout save requests persistent storage");
            ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown();
        }
        std::filesystem::remove_all(root);
        ImGui::DestroyContext();
        std::cout << "Editor preferences: " << checks << " checks, 0 failures\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
