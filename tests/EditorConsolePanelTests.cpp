#include "AssetDatabase.h"
#include "EngineHost.h"
#include "InputSystem.h"
#include "Project.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "editor/EditorConsolePanel.h"
#include "editor/EditorContext.h"
#include "editor/EditorDocument.h"
#include "editor/EditorPanelRegistry.h"
#include "editor/EditorPanels.h"
#include "editor/EditorToolPanels.h"
#include "editor/ScriptTools.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;
unsigned checks = 0;
void Check(bool condition, const char *label) {
    ++checks;
    if (!condition)
        throw std::runtime_error(label);
    std::printf("PASS %s\n", label);
}
struct Fixture {
    fs::path root = fs::temp_directory_path() /
                    ("judas-console-" + std::to_string(
                        std::chrono::steady_clock::now().time_since_epoch().count()));
    ~Fixture() {
        std::error_code error;
        fs::remove_all(root, error);
    }
};
struct Gui {
    Gui() {
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = {1000, 800};
        io.DeltaTime = 1.f / 60;
        unsigned char *pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }
    ~Gui() { ImGui::DestroyContext(); }
};
std::string Read(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Could not read Console fixture");
    return {std::istreambuf_iterator<char>(input), {}};
}
std::string Snapshot(ScriptSystem &scripts) {
    std::string result;
    for (const auto &record : scripts.Capture())
        result += std::to_string(record.entity) + "/" + std::to_string(record.slot) + ":" +
                  record.json + "\n";
    return result;
}
ImGuiWindow *Messages() {
    const auto *console = ImGui::FindWindowByName("Console");
    for (auto *window : ImGui::GetCurrentContext()->Windows)
        if (window->ParentWindow == console &&
            std::string(window->Name).find("Console messages") != std::string::npos)
            return window;
    return nullptr;
}
ImGuiID DiagnosticItem(ImGuiWindow &window, const ScriptDiagnostic &diagnostic,
                       const AssetDatabase &assets) {
    const auto *asset = assets.Find(diagnostic.asset);
    const auto path = asset ? asset->relativePath : diagnostic.asset;
    const auto label = path + " | " + diagnostic.callback + " | " + diagnostic.message;
    // Same ordinary PushID(pointer)/Selectable(label) identity as the real row.
    return ImHashStr(label.c_str(), 0, window.GetID(&diagnostic));
}
} // namespace

int main() try {
    Fixture fixture;
    Project project;
    std::string error;
    Check(Project::CreateNew(fixture.root.string(), "Console consumer", project, error),
          "isolated ordinary project is created");
    const auto faultPath = fixture.root / "Assets/fault.js";
    const auto healthyPath = fixture.root / "Assets/healthy.js";
    const std::string faultSource =
        "export default class { constructor(){this.state={ticks:0};} "
        "update(){this.state.ticks++;throw new Error('Console consumer fault');} }\n";
    std::ofstream(faultPath) << faultSource;
    std::ofstream(healthyPath) << "export default class {\n"
                                   " constructor(){this.state={ticks:0};}\n"
                                   " update(){\n"
                                   "  this.state.ticks++;\n"
                                   " }\n}\n";
    AssetDatabase assets;
    assets.Scan(project.RootDir(), project.AssetsDir());
    AssetRecord faultAsset, healthyAsset;
    Check(assets.Track(faultPath.string(), faultAsset, error) &&
              assets.Track(healthyPath.string(), healthyAsset, error),
          "faulting and live sources are tracked through the normal asset database");
    const auto projectBytes = Read(project.ProjectFile());
    ResourceManager resources(nullptr, &assets);
    Scene scene;
    auto &faulting = scene.CreateObject("faulting consumer");
    const auto faultId = faulting.id;
    faulting.body = SceneBodyComponent{};
    faulting.body->motion = SceneBodyMotion::Dynamic;
    faulting.body->mass = 2;
    faulting.transform.position = {0, 2, 0};
    faulting.scripts.push_back({1, faultAsset.id, true, "{}"});
    auto &healthy = scene.CreateObject("live state control");
    const auto healthyId = healthy.id;
    healthy.scripts.push_back({1, healthyAsset.id, true, "{}"});
    RuntimeWorld world;
    world.legacyGameplay = false;
    Check(world.Build(scene, &resources, error), "consumer world builds without a graphics runtime");
    const auto body = world.RuntimeBody(faultId);
    world.Physics().SetLinearVelocity(body, {1, -2, 3});
    world.Physics().SetAngularVelocity(body, {.2f, .3f, .4f});
    InputSystem input;
    world.UpdateScripts(&input, .016f);
    auto *scripts = world.Scripts();
    Check(scripts && scripts->Diagnostics().size() == 1 &&
              scripts->Diagnostics().front().message.find("Console consumer fault") !=
                  std::string::npos,
          "real gameplay fault produces a source-bearing diagnostic");
    Check(scripts->InspectPhase(faultId, 1) == ScriptInstancePhase::Faulted &&
              scripts->InspectPhase(healthyId, 1) == ScriptInstancePhase::Running &&
              scripts->ConsoleMessages().empty(),
          "fault isolation leaves another script running without output capture");
    Gui gui;
    EngineHost host;
    EditorDocument document;
    document.GetScene() = scene;
    EditorPanelState state;
    state.project = &project;
    state.assets = &assets;
    state.runtime = &world;
    state.mode = EditorMode::Play;
    state.dockingEnabled = true;
    state.showScriptTools = false;
    EditorRequests requests;
    EditorContext context{document, project, state, requests, host, assets, &world,
                          EditorPanelPhase::DebugStopped};
    EditorPanelRegistry registry;
    RegisterConsolePanel(registry);
    Check(registry.Contains("console") && registry.Descriptors().size() == 1,
          "Console registers from its own module without an application frame");
    auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({15, 15}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({930, 700}, ImGuiCond_Always);
        registry.DrawAll(context);
        ImGui::Render();
    };
    unsigned stops = 0;
    scripts->SetDebugger([&](const ScriptDebugStop &stop) {
        ++stops;
        Check(stop.entity == healthyId, "Console observation runs inside a genuine suspended callback");
        const auto before = Snapshot(*scripts);
        const auto pose = world.Physics().GetTransform(body);
        const auto velocity = world.Physics().GetLinearVelocity(body);
        const auto angular = world.Physics().GetAngularVelocity(body);
        const auto diagnostic = scripts->Diagnostics().front().message;
        state.showConsole = false;
        frame();
        Check(!state.console && !ImGui::FindWindowByName("Console"),
              "hidden Console executes no panel body or local-state allocation");
        state.showConsole = true;
        frame();
        frame();
        auto *messages = Messages();
        Check(state.console && messages && messages->Active &&
                  !messages->DrawList->VtxBuffer.empty(),
              "registered Console actually draws diagnostic rows during a debugger stop");
        AssetDatabase unknownSources;
        state.assets = &unknownSources;
        frame();
        ImGui::ActivateItemByID(DiagnosticItem(*Messages(), scripts->Diagnostics().front(),
                                               unknownSources));
        frame();
        frame();
        Check(state.editScriptAsset.empty() && !state.showScriptTools &&
                  scripts->Diagnostics().front().message == diagnostic,
              "unknown diagnostic source stays visible without inventing an edit target");
        state.assets = &assets;
        frame();
        ImGui::ActivateItemByID(DiagnosticItem(*Messages(), scripts->Diagnostics().front(), assets));
        frame();
        frame();
        Check(state.editScriptAsset == faultAsset.id && state.showScriptTools,
              "activating the real source row requests the registered script asset");
        std::unique_ptr<ScriptTools> tools;
        PumpEditorScriptTools(context, tools);
        Check(tools && tools->editing && tools->file == faultAsset.relativePath &&
                  tools->loadedBytes == faultSource && state.editScriptAsset.empty(),
              "ordinary request pump opens the existing source editor with exact source bytes");
        const auto after = world.Physics().GetTransform(body);
        Check(world.Scripts() == scripts && Snapshot(*scripts) == before &&
                  after.position == pose.position && after.rotation == pose.rotation &&
                  world.Physics().GetLinearVelocity(body) == velocity &&
                  world.Physics().GetAngularVelocity(body) == angular,
              "stopped Console draws preserve the same VM, script state and physical endpoint");
        Check(scripts->Diagnostics().size() == 1 &&
                  scripts->Diagnostics().front().message == diagnostic && !document.IsDirty() &&
                  Read(faultPath) == faultSource && Read(project.ProjectFile()) == projectBytes,
              "inspection/source opening preserve diagnostics, authored content and on-disk source");
        return ScriptDebugAction::Continue;
    });
    scripts->SetBreakpoints({{healthyAsset.relativePath, 4, true}});
    world.UpdateScripts(&input, .016f);
    scripts->SetDebugger({});
    Check(stops == 1 && Snapshot(*scripts).find("\"ticks\":2") != std::string::npos,
          "resuming executes the suspended gameplay instruction exactly once");
    const auto beforeStep = world.Physics().GetTransform(body).position;
    world.Physics().Step(1.f / 60);
    Check(world.Physics().GetTransform(body).position != beforeStep,
          "physical control moves when simulation is explicitly advanced");
    world.Destroy();
    resources.Shutdown();
    std::printf("EDITOR_CONSOLE %u checks 0 failures\n", checks);
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "FAIL %s\nEDITOR_CONSOLE %u checks 1 failure\n", error.what(), checks);
    return 1;
}
