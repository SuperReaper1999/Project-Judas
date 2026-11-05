#include "../third_party/nlohmann/json.hpp"
#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SaveService.h"
#include "SceneSession.h"
#include "ScreenshotWriter.h"

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {
using Json = nlohmann::json;
unsigned checks = 0, failures = 0;
void Check(bool condition, const char* text) {
    ++checks;
    failures += !condition;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", text);
}
std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing qualification input: " + path.string());
    return {std::istreambuf_iterator<char>(input), {}};
}
Json State(const RuntimeWorld& world) {
    if (world.Scripts())
        for (const auto& state : world.Scripts()->Capture())
            if (state.entity == 1) return Json::parse(state.json);
    return Json::object();
}
void Environment(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    if (*value) setenv(name, value, 1);
    else unsetenv(name);
#endif
}
} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    try {
        if (argc != 3 || (std::string(argv[1]) != "--write" && std::string(argv[1]) != "--read"))
            throw std::invalid_argument("usage: ParticleWorkshopApplicationTests --write|--read OWNED_OUTPUT");
        const bool read = std::string(argv[1]) == "--read";
        const auto output = std::filesystem::absolute(argv[2]);
        const auto projectRoot = output / "project";
        if (!read) {
            if (std::filesystem::exists(output))
                throw std::invalid_argument("write output exists; use a fresh path to preserve evidence");
            std::filesystem::create_directories(output);
            std::filesystem::copy("projects/particle_v2_lab", projectRoot,
                                   std::filesystem::copy_options::recursive);
        }
        Json expected = read ? Json::parse(Read(output / "EXPECTED.json")) : Json::object();
        const auto authored = Read(projectRoot / "Scenes/workshop.judas");
        auto project = (projectRoot / "particle_v2_lab.judasproj").string();
        Environment("JUDAS_WORLD_STATE", "none");
        Environment("JUDAS_TEST_SCRIPT", "");
        unsigned frames = 0, phase = 0;
        bool finished = false;
        std::string release, pending;
        auto queue = [&](const char* key) { pending = key; };
        ApplicationControl control;
        control.hidden = true;
        control.frameSeconds = [](float) { return 1.f / 60; };
        control.hostReady = [](EngineHost& host) {
            host.GetWindow().SetTestInputMode(true);
            SDL_GL_SetSwapInterval(0);
        };
        control.beforeFrame = [&](EngineHost& host, RuntimeWorld&, InteractivePlay&) {
            if (!release.empty()) {
                host.GetWindow().QueueTestPhysical(release, 0);
                release.clear();
            }
            if (!pending.empty()) {
                host.GetWindow().QueueTestPhysical(pending, 1);
                release = pending;
                pending.clear();
            }
        };
        control.afterFrame = [&](EngineHost& host, RuntimeWorld& world, InteractivePlay&) {
            ++frames;
            const auto state = State(world);
            if (frames == 10) {
                Check(world.Scripts() && world.Scripts()->Diagnostics().empty(),
                      "real editable workshop registers its gameplay script without faults");
                Check(world.VisualEmitters().size() == 4,
                      "ordinary project constructs all modest CPU workshop emitters");
                if (read) { queue("key:F7"); phase = 3; }
            }
            if (!read && phase == 0 && frames >= 150) {
                Check(state.value("count", 0.) > 0 && !state.value("cells", Json::object()).empty(),
                      "actual collision impacts create nonzero game-owned receiver-local coating");
                queue("key:F6");
                phase = 1;
            } else if (!read && phase == 1 && frames > 155) {
                auto* saves = world.SceneControl() ? world.SceneControl()->SavesIfLoaded() : nullptr;
                if (saves && !saves->Busy() && !saves->List().empty()) {
                    expected = state;
                    Check(state.value("paused", false) && state.value("count", 0.) > 0,
                          "save waits for a paused, drained reliable-particle boundary");
                    std::ofstream(output / "EXPECTED.json") << expected.dump(2) << '\n';
                    queue("key:C");
                    phase = 2;
                }
            } else if (!read && phase == 2 && state.value("count", -1.) == 0) {
                Check(state.at("cells").empty() && state.at("receivers").empty(),
                      "negative control clears the actual gameplay coating before restore");
                queue("key:F7");
                phase = 3;
            } else if (phase == 3 && frames > 15 && state.value("count", -1.) == expected.value("count", -2.) &&
                       state.value("paused", false)) {
                Check(state.at("cells") == expected.at("cells") &&
                          state.at("receivers") == expected.at("receivers") &&
                          state.at("weight") == expected.at("weight"),
                      read ? "fresh process restores exact deposited cells, counts and weight" :
                             "real save/load restores exact deposited cells, counts and weight");
                const auto receivers = state.at("receivers");
                Check(!receivers.empty(), "restored state contains actual impacted receivers");
                if (!receivers.empty()) {
                    const auto id = std::stoull(receivers.begin().key());
                    std::string material;
                    float friction = 0, restitution = 0;
                    const auto weight = receivers.begin().value().at("weight").get<double>();
                    const float wanted = float(.75 - .6 * (1 - std::exp(-weight / 8)));
                    Check(world.Physics().GetPhysicalMaterial(world.RuntimeBody(id), material, friction, restitution) &&
                              std::abs(friction - wanted) < 1e-6 && friction < .75f,
                          "coating reconstruction restores the real receiving body's changed friction");
                }
                bool empty = true;
                for (const auto& emitter : world.VisualEmitters()) empty &= emitter.pool.Particles().empty();
                Check(empty, "visual particle population is transient rather than used as persistent deposited material");
                Check(world.Scripts()->Diagnostics().empty(), "edit/save/clean/restore callbacks remain fault-free");
                std::vector<unsigned char> image;
                host.GetRenderer().CaptureFrame(host.GetWindow().Width(), host.GetWindow().Height(), image);
                WriteRgbPng((output / (read ? "cold-restored.png" : "restored.png")).string(),
                            host.GetWindow().Width(), host.GetWindow().Height(), image);
                finished = true;
                world.UI().RequestQuit();
            }
            if (frames >= 1200) {
                Check(false, "workshop integration watchdog expired before genuine save restoration");
                world.UI().RequestQuit();
            }
        };
        std::string program = "judas";
        char* arguments[] = {program.data(), project.data()};
        Application app;
        Check(app.Run(2, arguments, &control) == 0 && finished,
              "ordinary project starts, renders, advances and shuts down after verified persistence");
        Check(Read(projectRoot / "Scenes/workshop.judas") == authored,
              "Play/save/load does not rewrite the authored workshop scene");
    } catch (const std::exception& error) {
        Check(false, error.what());
    }
    std::printf("Particle workshop application: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
