#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "WorldPersistence.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "SaveArchive.h"
#include "../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
unsigned checks = 0, failures = 0;
void Check(bool condition, const std::string& message) {
    ++checks;
    if (!condition) ++failures;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message.c_str());
}
Json State(RuntimeWorld& world) {
    for (const auto& record : world.Scripts()->Capture())
        if (record.entity == 1 && record.slot == 1) return Json::parse(record.json);
    throw std::runtime_error("Workshop coordinator state missing");
}
Json StoredState(const GameSnapshot& snapshot) {
    const auto found = snapshot.participants.find("scripts");
    if (found == snapshot.participants.end() || found->second.version != 1)
        throw std::runtime_error("required modern script participant missing");
    SaveArchive archive(found->second.data);
    std::uint32_t count = 0;
    archive(count);
    if (count > 4096) throw std::runtime_error("invalid script count");
    Json state;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint64_t entity = 0, slot = 0;
        std::string text;
        archive(entity, slot, text);
        if (entity == 1 && slot == 1) state = Json::parse(text);
    }
    archive.Finish();
    if (!state.is_object()) throw std::runtime_error("committed script state missing");
    return state;
}
Json NativeAgents(RuntimeWorld& world) {
    Json agents = Json::object();
    for (const auto& row : world.Scripts()->AIInspections()) {
        if (row.entity != 1 || row.slot != 1) continue;
        const std::string key = row.name.find("Mara") != std::string::npos ? "citizen" : "guard";
        if (agents.contains(key)) throw std::runtime_error("duplicate Workshop native agent");
        agents[key] = Json::parse(row.json);
    }
    return agents;
}
void EndApplication() {
    SDL_Event event{};
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}
bool Moving(RuntimeWorld& world) {
    for (const auto id : {301ull, 302ull})
        if (glm::length(world.Physics().GetLinearVelocity(world.RuntimeBody(id))) > .5f) return true;
    return false;
}
void CheckEndpoint(RuntimeWorld& world, const Json& expected) {
    for (const auto id : {301ull, 302ull}) {
        const auto position = world.Physics().GetTransform(world.RuntimeBody(id)).position;
        const auto& saved = expected.at("positions").at(std::to_string(id));
        const glm::vec3 reference(saved[0].get<float>(), saved[1].get<float>(), saved[2].get<float>());
        Check(position == reference, "restored actual MK2 endpoint is exact before stepping: " + std::to_string(id));
        for (int k = 0; k < 3; ++k) Check(std::isfinite(position[k]), "restored position is finite");
    }
}
} // namespace

int main(int argc, char** argv) try {
    if (argc != 3 && argc != 4) {
        std::fprintf(stderr, "usage: %s <AI_Workshop.judasproj> <fresh-output-dir> [cold]\n", argv[0]);
        return 2;
    }
    const bool cold = argc == 4 && std::string(argv[3]) == "cold";
    if (argc == 4 && !cold) throw std::runtime_error("unknown mode");
    const fs::path output = fs::absolute(argv[2]), project = output / "project";
    if (!cold) {
        if (fs::exists(output)) throw std::runtime_error("write test requires a fresh output directory");
        fs::create_directories(output);
        fs::copy(fs::absolute(argv[1]).parent_path(), project, fs::copy_options::recursive);
    }
    SDL_setenv("XDG_DATA_HOME", (output / "user").string().c_str(), 1);
    std::string projectFile = (project / "AI_Workshop.judasproj").string(), error;
    Project definition;
    Check(definition.Load(projectFile, error), "load isolated editable Workshop: " + error);
    Json expected;
    if (cold) { std::ifstream file(output / "expected.json"); file >> expected; }

    enum class Phase { Playing, Saving, Loading, RestoredZeroStep, AdvanceOneStep, Complete };
    Phase phase = Phase::Playing;
    unsigned frame = 0;
    std::uint64_t request = 0;
    RuntimeWorld* outgoing = nullptr;
    bool holdPhysics = false, complete = false;
    ApplicationControl control;
    control.hidden = true;
    control.frameSeconds = [&](float) { return holdPhysics ? 0.f : 1.f / 60; };
    control.hostReady = [](EngineHost& host) {
        host.GetWindow().SetTestInputMode(true);
        SDL_GL_SetSwapInterval(0);
        std::string ignored;
        host.Audio().Init(ignored, true);
    };
    control.beforeFrame = [&](EngineHost&, RuntimeWorld& world, InteractivePlay&) {
        if (phase == Phase::Loading && &world != outgoing) {
            CheckEndpoint(world, expected);
            // Zero simulation time exercises normal restore/UI lifecycle before
            // advancing any physical endpoint or decision clock. This is a test
            // observation boundary, not an engine settling delay.
            holdPhysics = true;
            phase = Phase::RestoredZeroStep;
        }
    };
    control.afterFrame = [&](EngineHost& host, RuntimeWorld& world, InteractivePlay&) {
        ++frame;
        if (!world.Scripts() || !world.Scripts()->Diagnostics().empty()) {
            Check(false, world.Scripts() && !world.Scripts()->Diagnostics().empty() ?
                world.Scripts()->Diagnostics().front().message : "script host missing");
            EndApplication();
            return;
        }
        const auto state = State(world);
        auto* service = world.SceneControl()->Saves(host.Resources());
        if (phase == Phase::RestoredZeroStep) {
            CheckEndpoint(world, expected);
            Check(state == expected.at("state"), "restored script data exactly matches independently read save before stepping");
            const auto agents = NativeAgents(world);
            Check(agents.size() == 2, "restore callback reconstructs two real native agents on zero-physics frame");
            for (const auto key : {"citizen", "guard"}) {
                const auto& saved = expected.at("state").at(key);
                for (const auto field : {"elapsed", "untilEvaluation", "selected", "status", "cooldowns"})
                    Check(agents.at(key).at(field) == saved.at(field), std::string("actual native ") + key + " restored " + field);
            }
            Check(state.at("time") == expected.at("state").at("time"), "restore callback does not replay gameplay time");
            Check(state.at("meals") == expected.at("state").at("meals"), "restore callback does not duplicate completed rewards");
            holdPhysics = false;
            phase = Phase::AdvanceOneStep;
        } else if (phase == Phase::AdvanceOneStep) {
            const auto agents = NativeAgents(world);
            for (const auto key : {"citizen", "guard"}) {
                const double saved = expected.at("state").at(key).at("elapsed");
                const double actual = agents.at(key).at("elapsed");
                Check(std::abs((actual - saved) - double(1.f / 60)) < 1e-9,
                    std::string("native ") + key + " clock continues one fixed step rather than resetting");
            }
            bool changed = false;
            for (const auto id : {301ull, 302ull}) {
                const auto actual = world.Physics().GetTransform(world.RuntimeBody(id)).position;
                const auto& p = expected.at("positions").at(std::to_string(id));
                changed |= actual != glm::vec3(p[0].get<float>(), p[1].get<float>(), p[2].get<float>());
            }
            Check(changed, "restored running action advances an actual physical actor");
            complete = true;
            phase = Phase::Complete;
            EndApplication();
        } else if (phase == Phase::Playing && cold) {
            Check(NativeAgents(world).at("citizen").at("elapsed").get<double>() < 1,
                "fresh process native clock initially differs from saved continuation");
            outgoing = &world;
            request = service->Request("load", "ai-continuation", "", "{}", error);
            Check(request != 0, "cold process requests real modern save load: " + error);
            phase = Phase::Loading;
        } else if (phase == Phase::Playing && frame >= 300 && Moving(world)) {
            bool cooling = false;
            for (const auto key : {"citizen", "guard"})
                for (const auto& value : state.at(key).at("cooldowns")) cooling |= value.get<double>() > 0;
            if (cooling) {
                Check(state.at("time").get<double>() >= 5 && state.at("meals").get<unsigned>() > 0,
                    "save fixture contains genuine played progress and nontrivial native timers");
                request = service->Request("save", "ai-continuation", "Played AI continuation", "{}", error);
                Check(request != 0, "request save through production service: " + error);
                phase = Phase::Saving;
            }
        } else if (phase == Phase::Saving) {
            const auto* status = service->Status(request);
            if (status && status->state == "completed") {
                GameSnapshot snapshot;
                bool recovered = false;
                SaveStorage storage(service->Root());
                Check(storage.Read("ai-continuation", snapshot, recovered, error), "independently read durable modern save: " + error);
                Scene scene;
                Check(WorldPersistence::SceneFrom(snapshot.participants, scene, error), "independently decode saved physical scene: " + error);
                expected = Json{{"state", StoredState(snapshot)}, {"positions", Json::object()}};
                for (const auto id : {301ull, 302ull}) {
                    const auto* object = scene.Find(id);
                    if (!object) throw std::runtime_error("saved physical actor missing");
                    const auto p = object->transform.position;
                    expected["positions"][std::to_string(id)] = {p.x, p.y, p.z};
                }
                Check(expected.at("state").at("citizen").at("elapsed").get<double>() > 5,
                    "committed snapshot really contains progressed native decision clock");
                std::ofstream(output / "expected.json") << expected.dump(2);
                outgoing = &world;
                request = service->Request("load", "ai-continuation", "", "{}", error);
                Check(request != 0, "request same-process modern restoration: " + error);
                phase = Phase::Loading;
            }
        }
        if ((phase == Phase::Saving || phase == Phase::Loading) && request) {
            const auto* status = service->Status(request);
            if (status && status->state == "failed") {
                Check(false, status->error);
                EndApplication();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (frame > 1500) { Check(false, "bounded AI save/load workflow completes"); EndApplication(); }
    };
    Application application;
    char program[] = "judas";
    char* arguments[] = {program, projectFile.data()};
    Check(application.Run(2, arguments, &control) == 0 && complete, "native application completes and shuts down");
    std::printf("UTILITY_AI_APPLICATION mode=%s checks=%u failures=%u\n", cold ? "cold" : "write", checks, failures);
    return failures ? 1 : 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL %s\n", error.what());
    return 1;
}
