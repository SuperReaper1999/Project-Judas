#include "Application.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {

void PushKey(bool pressed) {
    SDL_Event event{};
    event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.keysym.scancode = SDL_SCANCODE_E;
    event.key.keysym.sym = SDLK_e;
    SDL_PushEvent(&event);
}

} // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    if (argc != 3) {
        std::fputs("Usage: judas_script_developer_consumer_tests <developer-lab> <scratch>\n", stderr);
        return 2;
    }
    const auto root = fs::absolute(argv[2]);
    fs::create_directories(root);
    fs::copy(argv[1], root, fs::copy_options::recursive);
    int failures = 0;
    const auto check = [&](bool passed, const char* message) {
        failures += !passed;
        std::printf("%s %s\n", passed ? "PASS" : "FAIL", message);
    };
    int frame = 0;
    std::vector<std::string> observedGains;
    ApplicationControl control;
    control.hidden = true;
    control.frameSeconds = [](float) { return 1.f / 60; };
    control.hostReady = [](EngineHost& host) {
        std::string error;
        host.Audio().Init(error, true);
    };
    control.worldReady = [&](EngineHost&, RuntimeWorld& world, InteractivePlay&) {
        auto& vm = world.PrepareScriptTools();
        vm.SetDebugger([&](const ScriptDebugStop& stop) {
            if (!stop.frames.empty()) {
                for (const auto& variable : stop.frames.front().variables)
                    if (variable.name == "local gain") observedGains.push_back(variable.value);
            }
            return ScriptDebugAction::Continue;
        });
        vm.SetBreakpoints({{"Assets/scripts/calibration.js", 5, true}});
    };
    control.beforeFrame = [&](EngineHost&, RuntimeWorld&, InteractivePlay&) {
        if (frame == 1 || frame == 5) PushKey(true);
        if (frame == 2 || frame == 6) PushKey(false);
    };
    control.afterFrame = [&](EngineHost&, RuntimeWorld& world, InteractivePlay&) {
        ++frame;
        auto* vm = world.Scripts();
        if (frame == 3) {
            check(vm && vm->Diagnostics().size() == 1 &&
                      vm->Diagnostics().front().message.find("Calibration gain") != std::string::npos,
                  "the real lab reports its deliberate helper fault");
            const auto path = root / "Assets" / "scripts" / "calibration.js";
            std::ifstream input(path);
            std::string source{std::istreambuf_iterator<char>(input), {}};
            input.close();
            const auto gain = source.find("gain: 0");
            bool edited = false;
            if (gain != std::string::npos) {
                source.replace(gain, 7, "gain: 2");
                edited = bool(std::ofstream(path) << source);
            }
            std::string error;
            check(edited && vm && vm->Reload(false, error),
                  "repair the copied lab source and reload deliberately without restarting");
            if (!error.empty()) std::fprintf(stderr, "%s\n", error.c_str());
        }
        if (frame == 10) {
            bool repaired = false;
            if (vm) {
                for (const auto& state : vm->Capture()) {
                    if (state.entity == 30)
                        repaired = state.json.find("\"calibrated\":true") != std::string::npos;
                }
            }
            check(vm && repaired && vm->Diagnostics().empty(), "same input runs the repaired helper");
            check(std::find(observedGains.begin(), observedGains.end(), "0") != observedGains.end() &&
                      std::find(observedGains.begin(), observedGains.end(), "2") != observedGains.end(),
                  "real project breakpoints see both failing and corrected locals");
            SDL_Event event{};
            event.type = SDL_QUIT;
            SDL_PushEvent(&event);
        }
    };
    std::string executable = "judas";
    auto project = (root / "developer_lab.judasproj").string();
    char* args[] = {executable.data(), project.data()};
    Application app;
    check(app.Run(2, args, &control) == 0 && frame >= 10, "complete the lab workflow and shut down");
    return failures ? 1 : 0;
}
