#include "Application.h"
#include "AssetDatabase.h"
#include "EngineHost.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "ScriptSystem.h"
#include "ToolProcess.h"
#include "../third_party/nlohmann/json.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#if defined(__linux__) && !defined(__ANDROID__)
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

void Require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

void WriteFixture(const fs::path& path) {
    Project project;
    std::string error;
    const bool created = Project::CreateNew(path.string(), "Game Settings Fixture", project, error);
    Require(created, error);
    const auto script = fs::path(project.AssetsDir()) / "settings-fixture.js";
    std::ofstream(script) << R"JS(
import {settings, audio, input} from "judas";
const require = (ok, message) => { if (!ok) throw new Error(message); };
export default class {
    constructor() { this.state = {phase: "start"}; }
    update() {
        const state = settings.read();
        require(state && state.desktop && state.persistent, "Missing standalone preferences");
        const game = settings.readGame();
        if (game.liveFixture) {
            if (this.state.phase === "start") {
                require(state.activeBackend === "opengl" && state.liveBackendSwitch, "Expected native live backend candidate");
                audio.setGroup("master",game.master);
                input.setBindings(game.bindings);
                this.continuity = {value: 17};
                settings.previewDisplay({fullscreen: true});
                this.state.phase = "live-preview";
            } else if (this.state.phase === "live-preview") {
                require(state.fullscreen && state.displayPreview, state.displayError || "Fullscreen preview did not open");
                settings.confirmDisplay();
                this.state.phase = "live-apply";
            } else if (this.state.phase === "live-apply") {
                require(state.fullscreen && !state.displayPreview,"Fullscreen was not confirmed");
                settings.switchBackend("vulkan");
                require(settings.read().backendTransition === "pending" &&
                        settings.read().activeBackend === "opengl", "Queued candidate reported active early");
                this.state.phase = "live-vulkan";
            } else if (this.state.phase === "live-vulkan") {
                require(state.activeBackend === "vulkan" && state.backendTransition === "completed" &&
                        state.fullscreen && this.continuity.value === 17, state.backendError || "Vulkan lost the script instance");
                this.continuity.value++;
                settings.switchBackend("opengl");
                this.state.phase = "live-opengl";
            } else if (this.state.phase === "live-opengl") {
                require(state.activeBackend === "opengl" && state.backendTransition === "completed" &&
                        state.fullscreen && this.continuity.value === 18, state.backendError || "OpenGL lost the script instance");
                settings.switchBackend("vulkan");
                this.state.phase = "live-fail";
            } else if (this.state.phase === "live-fail") {
                require(state.activeBackend === "opengl" && state.backendTransition === "recovered" &&
                        state.backendError && state.fullscreen && this.continuity.value === 18, "Previous backend failed to recover");
                settings.previewDisplay({fullscreen: false, width: 800, height: 600});
                this.state.phase = "live-windowed";
            } else if (this.state.phase === "live-windowed") {
                require(!state.fullscreen && state.displayPreview, state.displayError || "Windowed preview did not open");
                settings.confirmDisplay();
                this.state.phase = "live-save";
            } else if (this.state.phase === "live-save") {
                settings.save();
                this.state.phase = "live";
            }
            return;
        }
        if (this.state.phase === "start") {
            if (game.committed) {
                require(state.fpsCap === 30 && !state.shadows && state.localShadowBudget === 2, "Video restoration");
                audio.setGroup("master", game.master);
                input.setBindings(game.bindings);
                this.state.phase = "restored";
                return;
            }
            if (state.loadError) {
                require(state.fpsCap === 0 && state.shadows, "Invalid file changed runtime defaults");
                let rejected = false;
                try { settings.save(); } catch (_) { rejected = true; }
                require(rejected, "Corrupt file replaced without explicit reset");
                this.state.phase = "corrupt";
                return;
            }
            for (const fpsCap of [1, 2001]) {
                let rejected = false;
                try { settings.applyVideo({fpsCap, shadows: false}); }
                catch (_) { rejected = true; }
                require(rejected && settings.read().shadows === state.shadows, "Invalid video patch partially applied");
            }
            const map = input.getBindings();
            map.find(x => x.name === "jump").bindings[0].control = "key:K";
            audio.setGroup("master", {gain: 0.25, mute: true});
            input.setBindings(map);
            const other = state.activeBackend === "opengl" ? "vulkan" : "opengl";
            if (state.backendLocked) {
                require(!state.liveBackendSwitch, "Launch override allowed live backend switching");
                let blocked = false;
                try { settings.applyVideo({backend: other}); } catch (_) { blocked = true; }
                require(blocked, "Launch backend override was bypassed");
            } else if (state.backends.some(backend => backend.name === other)) {
                settings.applyVideo({backend: other});
                const staged = settings.read();
                require(staged.restartRequired && staged.requestedBackend === other &&
                        staged.activeBackend === state.activeBackend, "Backend choice did not remain restart-only");
            }
            const patch = {fpsCap: 30, shadows: false, localShadowBudget: 2};
            if (!state.backendLocked) patch.backend = state.activeBackend;
            settings.applyVideo(patch);
            this.state.vsyncChanged = false;
            if (state.vsync !== null && !state.displayLocked && state.canDisableVSync !== false) {
                try {
                    settings.applyVideo({vsync: !state.vsync});
                    this.state.vsyncChanged = true;
                    this.state.vsyncTarget = !state.vsync;
                } catch (error) {
                    require(state.activeBackend === "opengl", "Vulkan surface supported VSync request failed");
                    console.log("OpenGL VSync unavailable: " + String(error));
                }
            }
            settings.setGame({committed: true, master: {gain: 0.25, mute: true}, bindings: map});
            this.state.phase = state.resolutions.length ? "display" : "save";
            return;
        }
        if (this.state.phase === "display") {
            if (this.state.vsyncChanged) require(state.vsync === this.state.vsyncTarget, "Presentation did not adopt the accepted swap interval");
            if (state.displayLocked) { this.state.phase = "save"; return; }
            require(state.resolutions.length > 0, "No reported display modes");
            this.state.original = {width: state.width, height: state.height};
            settings.previewDisplay({fullscreen: false, width: 800, height: 600});
            this.state.phase = "resized";
        } else if (this.state.phase === "resized") {
            require(state.displayPreview && state.width === 800 && state.height === 600, state.displayError || "Resize did not apply");
            let rejected = false;
            try { settings.save(); } catch (_) { rejected = true; }
            require(rejected, "Unconfirmed display was saved");
            settings.revertDisplay();
            this.state.phase = "windowed";
        } else if (this.state.phase === "windowed") {
            require(!state.displayPreview && state.width === this.state.original.width, "Resize revert failed");
            settings.previewDisplay({fullscreen: true});
            this.state.phase = "fullscreen";
        } else if (this.state.phase === "fullscreen") {
            require(state.displayPreview && state.fullscreen, state.displayError || "Fullscreen did not apply");
            settings.confirmDisplay();
            this.state.phase = "confirmed";
        } else if (this.state.phase === "confirmed") {
            require(state.fullscreen && !state.displayPreview, "Confirmation failed");
            settings.previewDisplay({fullscreen: false, width: 800, height: 600});
            this.state.phase = "return";
        } else if (this.state.phase === "return") {
            require(state.displayPreview && !state.fullscreen, state.displayError || "Windowed mode did not return");
            settings.confirmDisplay();
            this.state.phase = "save";
        } else if (this.state.phase === "save") {
            settings.save();
            this.state.phase = "saved";
        }
    }
}
)JS";
    AssetDatabase assets;
    assets.Scan(project.RootDir(), project.AssetsDir());
    AssetRecord record;
    const bool tracked = assets.Track(script.string(), record, error);
    Require(tracked, error);
    project.Settings().startupScene = "Scenes/main.judas";
    const bool projectSaved = project.Save(error);
    Require(projectSaved, error);
    const auto mesh = fs::path(project.AssetsDir()) / "continuity.obj";
    std::ofstream(mesh) << "v -1 -1 0\nv 1 -1 0\nv 0 1 0\nf 1 2 3\n";
    AssetRecord meshRecord;
    Require(assets.Track(mesh.string(),meshRecord,error),error);
    Scene scene;
    auto& cube = scene.CreateObject("Shadow caster");
    cube.render = SceneRenderComponent{};
    cube.render->shape = SceneShape::Mesh;
    cube.render->meshAsset = meshRecord.id;
    cube.transform.position = {0, 0, -3};
    auto& camera = scene.CreateObject("Continuity render target");
    camera.renderCamera = SceneRenderCameraComponent{};
    camera.renderCamera->width = 64;
    camera.renderCamera->height = 64;
    camera.renderCamera->updateEveryFrames = 8;
    scene.CreateObject("Settings Script").scripts.push_back({1, record.id, true, "{}"});
    const bool saved = SaveSceneToFile(scene, project.StartupScenePath(), error);
    Require(saved, error);
}

int RunChild(const std::string& project, const std::string& terminalPhase) {
    unsigned frames = 0;
    ApplicationControl control;
    control.hidden = std::getenv("JUDAS_WINDOW_SIZE") != nullptr;
    bool finished = false;
    std::uint32_t hud = 0;
    if (terminalPhase == "live") {
        control.worldReady = [&](EngineHost&, RuntimeWorld& world, InteractivePlay&) {
            UIDocument document;
            UIElement canvas, text;
            canvas.id = "canvas";
            canvas.kind = UIKind::Canvas;
            document.elements.push_back(canvas);
            text.id = "status";
            text.parent = "canvas";
            text.kind = UIKind::Text;
            text.text = "HUD stays live";
            document.elements.push_back(text);
            std::string error;
            hud = world.UI().Add(document,"live-hud",0,error);
            Require(hud != 0,error);
        };
    }
    unsigned lastShadowPasses = 0;
    std::optional<double> transitionSimulationTime;

    using Clock = std::chrono::steady_clock;
    Clock::time_point lastFrame;
    auto minimumPacedFrame = Clock::duration::max();
    control.beforeFrame = [&](EngineHost&, RuntimeWorld& world, InteractivePlay&) {
        if (transitionSimulationTime) {
            Require(world.UI().Find("live-hud") == hud && world.CameraTexture(2).IsValid(),
                    "HUD ownership or camera target was lost on a rebuilt backend");
            Require(world.SimulationTimeSeconds() == *transitionSimulationTime,
                    "Simulation advanced during graphics transition");
            transitionSimulationTime.reset();
        }
        const auto now = Clock::now();
        if (frames >= 2) minimumPacedFrame = std::min(minimumPacedFrame, now - lastFrame);
        lastFrame = now;
    };
    control.afterFrame = [&](EngineHost& host, RuntimeWorld& world, InteractivePlay&) {
        ++frames;
        auto scripts = world.Scripts();
        Require(scripts && scripts->Diagnostics().empty(),
                scripts && !scripts->Diagnostics().empty() ? scripts->Diagnostics().front().message : "Script missing");
        if (const auto* expected = std::getenv("JUDAS_SETTINGS_EXPECT_BACKEND")) {
            Require(host.GetWindow().UsesVulkan() == (std::string(expected) == "vulkan"), "Unexpected active backend");
        }
        for (const auto& entry : scripts->Capture()) {
            const auto state = Json::parse(entry.json);
            const auto phase = state.at("phase").get<std::string>();
            if (phase == "live-vulkan" || phase == "live-opengl" || phase == "live-fail") {
                transitionSimulationTime = world.SimulationTimeSeconds();
                if (phase == "live-fail") SDL_setenv("JUDAS_VULKAN_DEVICE","impossible-fixture-device-name",1);
            }
            if (phase != terminalPhase || frames < 3) continue;
            if (terminalPhase != "corrupt") {
                Require(!host.GetRenderer().ShadowsEnabled(), "Native global shadow setting was not applied");
                Require(host.GetRenderer().Stats().shadowPasses == lastShadowPasses, "A disabled shadow pass still ran");
                Require(world.Settings().sunEnabled, "Global shadow option disabled sunlight");
                AudioGroupSettings master;
                Require(host.Audio().GetGroup("master", master) && master.mute && master.gain == 0.25f,
                        "Native mixer gain/mute mismatch");
                const auto* jump = host.GetWindow().Input().Map().Find("jump");
                Require(jump && jump->bindings.front().control == "key:K", "Native input map was not rebound");
            }
            finished = true;
        }
        lastShadowPasses = host.GetRenderer().Stats().shadowPasses;
        if (finished || frames > 90) {
            SDL_Event quit{};
            quit.type = SDL_QUIT;
            SDL_PushEvent(&quit);
        }
    };
    std::string executable = "settings-fixture", projectPath = project;
    char* args[] = {executable.data(), projectPath.data()};
    const int result = Application{}.Run(2, args, &control);
    Require(result == 0 && finished, "Standalone settings session did not reach " + terminalPhase);
    std::cout << "PASS standalone " << terminalPhase << " with " << frames << " gameplay frames";
    if (terminalPhase != "corrupt") {
        Require(minimumPacedFrame >= std::chrono::milliseconds(32), "30 FPS cap did not pace the frame loop");
        std::cout << ", minimum paced frame "
                  << std::chrono::duration<double, std::milli>(minimumPacedFrame).count() << " ms";
    }
    std::cout << '\n';
    return 0;
}
}

int main(int argc, char** argv) try {
#if !defined(__linux__) || defined(__ANDROID__)
    return 0;
#else
    if (argc == 4 && std::string(argv[1]) == "child") {
        // CaptureToolOutput captures standard output only. Include driver and
        // script diagnostics from both streams in the parent validation gate.
        Require(dup2(STDOUT_FILENO, STDERR_FILENO) >= 0, "Cannot combine test output streams");
        return RunChild(argv[2], argv[3]);
    }
    if (argc != 2) return 2;
    const auto path = fs::absolute(argv[1]);
    Require(fs::create_directory(path), "Use a fresh fixture output directory");
    Require(SDL_setenv("XDG_DATA_HOME", (path / "user").c_str(), 1) == 0, "Could not isolate preferences");
    WriteFixture(path / "project");
    const auto project = Project::FindProjectFileFor((path / "project").string());
    auto run = [&](const char* phase) {
        std::string output, error;
        const bool passed = CaptureToolOutput(fs::absolute(argv[0]), {"child", project, phase},
                                              output, error, std::chrono::seconds(60), 65536);
        std::cout << output;
        Require(passed, error);
        Require(output.find("[Vulkan validation]") == std::string::npos &&
                output.find("Validation Error:") == std::string::npos,
                "The settings session emitted a Vulkan validation diagnostic");
    };
    run("saved");
    run("restored");
    const auto settingsRoot = path / "user/Judas/settings";
    const auto preferences = fs::directory_iterator(settingsRoot)->path() / "preferences.json";
    const auto* launchBackend = std::getenv("JUDAS_GRAPHICS");
    if (launchBackend && std::string(launchBackend) == "vulkan") {
        Json saved;
        { std::ifstream in(preferences); in >> saved; }
        saved["video"]["backend"] = "vulkan";
        std::ofstream(preferences) << saved.dump();
        unsetenv("JUDAS_GRAPHICS");
        SDL_setenv("JUDAS_SETTINGS_EXPECT_BACKEND", "vulkan", 1);
        run("restored");
        const auto* previous = std::getenv("VK_DRIVER_FILES");
        const auto oldDrivers = previous ? std::optional<std::string>(previous) : std::nullopt;
        SDL_setenv("VK_DRIVER_FILES", (path / "missing-icd.json").c_str(), 1);
        SDL_setenv("JUDAS_SETTINGS_EXPECT_BACKEND", "opengl", 1);
        run("restored");
        if (oldDrivers) SDL_setenv("VK_DRIVER_FILES", oldDrivers->c_str(), 1);
        else unsetenv("VK_DRIVER_FILES");
        unsetenv("JUDAS_SETTINGS_EXPECT_BACKEND");
        saved["video"]["backend"] = "opengl";
        saved["game"]["liveFixture"] = true;
        std::ofstream(preferences) << saved.dump();
        run("live");
        { std::ifstream in(preferences); in >> saved; }
        Require(saved["video"]["backend"] == "opengl", "Failed live choice overwrote the effective backend");
        SDL_setenv("JUDAS_GRAPHICS", "vulkan", 1);
    }
    std::ofstream(preferences) << "broken JSON";
    run("corrupt");
    std::ifstream remaining(preferences);
    std::string line;
    std::getline(remaining, line);
    Require(line == "broken JSON", "Startup rewrote corrupt preferences");
    return 0;
#endif
} catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
}
