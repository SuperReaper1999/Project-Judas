// A real consumer game through Application, native utility decisions and MK2.
// UI clicks use published hit rectangles; world clicks use the actual camera.
#include "Application.h"
#include "CameraLens.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "SaveService.h"
#include "SceneSession.h"
#include "ScreenshotWriter.h"
#include "WorldPersistence.h"
#include "../third_party/nlohmann/json.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <thread>

namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
unsigned checks = 0, failures = 0;
void Check(bool value, const std::string& message) {
    ++checks;
    if (!value) ++failures;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", message.c_str());
    std::fflush(stdout);
}
void Quit() { SDL_Event event{}; event.type = SDL_QUIT; SDL_PushEvent(&event); }
Json State(RuntimeWorld& world) {
    if (world.Scripts()) for (const auto& row : world.Scripts()->Capture())
        if (row.entity == 1 && row.slot == 1) return Json::parse(row.json);
    throw std::runtime_error("Willow Town coordinator state is missing");
}
Json Agents(RuntimeWorld& world, const Json& state) {
    Json result = Json::object();
    for (const auto& native : world.Scripts()->AIInspections()) {
        if (native.entity != 1 || native.slot != 1) continue;
        bool found = false;
        for (const auto& resident : state.at("residents")) {
            const std::string name = resident.at("name"), id = resident.at("id");
            if (native.name.rfind(name, 0) == 0) {
                if (result.contains(id)) throw std::runtime_error("Duplicate native resident agent");
                result[id] = Json::parse(native.json);
                found = true;
                break;
            }
        }
        if (!found) throw std::runtime_error("Unrecognised native resident: " + native.name);
    }
    return result;
}
unsigned CompletedServices(const Json& resident) {
    return resident.at("workDone").get<unsigned>() + resident.at("meals").get<unsigned>() +
        resident.at("rests").get<unsigned>() + resident.at("chats").get<unsigned>();
}
unsigned Sum(const Json& state, const char* field) {
    unsigned count = 0;
    for (const auto& row : state.at("residents")) count += row.at(field).get<unsigned>();
    return count;
}
Json StoredState(const GameSnapshot& snapshot) {
    const auto found = snapshot.participants.find("scripts");
    if (found == snapshot.participants.end() || found->second.version != 1)
        throw std::runtime_error("Modern script save participant is missing");
    SaveArchive archive(found->second.data);
    std::uint32_t count = 0;
    archive(count);
    if (count > 4096) throw std::runtime_error("Unbounded script save participant");
    Json state;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint64_t entity = 0, slot = 0;
        std::string text;
        archive(entity, slot, text);
        if (entity == 1 && slot == 1) state = Json::parse(text);
    }
    archive.Finish();
    if (!state.is_object()) throw std::runtime_error("Saved town state is missing");
    return state;
}
void Endpoint(RuntimeWorld& world, const Json& expected) {
    for (std::uint64_t id = 301; id <= 306; ++id) {
        const auto p = world.Physics().GetTransform(world.RuntimeBody(id)).position;
        const auto& saved = expected.at("positions").at(std::to_string(id));
        const glm::vec3 reference(saved[0].get<float>(), saved[1].get<float>(), saved[2].get<float>());
        Check(p == reference, "cold-restored actual MK2 endpoint before stepping: " + std::to_string(id));
    }
}
}

int main(int argc, char** argv) try {
    if (argc != 3 && argc != 4) {
        std::fprintf(stderr, "usage: %s <Willow_Town.judasproj> <fresh-output-directory> [cold]\n", argv[0]);
        return 2;
    }
    const bool cold = argc == 4 && std::string(argv[3]) == "cold";
    if (argc == 4 && !cold) throw std::runtime_error("Unknown test mode");
    const fs::path source = fs::absolute(argv[1]), output = fs::absolute(argv[2]);
    const auto project = output / "project";
    if (!cold) {
        if (fs::exists(output)) throw std::runtime_error("Write test requires a fresh output directory");
        fs::create_directories(output);
        fs::copy(source.parent_path(), project, fs::copy_options::recursive);
    }
    SDL_setenv("XDG_DATA_HOME", (output / "user").string().c_str(), 1);
    // Keep the real responsive UI and all 600 game seconds, but use the game's
    // supported minimum viewport so software-rendered CI does not spend its
    // generic wall-clock allowance shading an arbitrary desktop-sized window.
    SDL_setenv("JUDAS_WINDOW_SIZE", "640x480", 1);
    std::string projectFile = (project / source.filename()).string(), error;
    Project definition;
    Check(definition.Load(projectFile, error), "isolated editable town project loads: " + error);
    Json expected;
    if (cold) { std::ifstream stream(output / "expected.json"); stream >> expected; }

    enum class Phase { Playing, ClosedShop, ReopenedShop, Saving, Loading, Restored, Advanced, Complete };
    Phase phase = Phase::Playing;
    unsigned frame = 0, phaseFrame = 0;
    bool complete = false, freeze = false, releasePointer = false;
    std::uint64_t request = 0;
    RuntimeWorld* outgoing = nullptr;
    glm::vec3 controlStart{}, pausedPosition{};
    double pausedTime = 0, closedAt = 0;
    unsigned closedMeals = 0, closedSales = 0, reopeningMeals = 0;
    unsigned progressAt400 = 0;
    std::map<std::string, unsigned> residentProgressAt400;
    bool measured400 = false;
    std::map<std::uint64_t, glm::vec3> initialPositions;
    std::set<std::uint64_t> moved;
    bool worldSelected = false, exercisedControl = false, exercisedPause = false;
    double worstPhysics = 0;
    ApplicationControl control;
    control.hidden = true;
    control.frameSeconds = [&](float) { return freeze ? 0.f : 1.f / 60; };
    control.hostReady = [](EngineHost& host) {
        Check(host.GetWindow().Width() == 640 && host.GetWindow().Height() == 480,
            "software-renderable fixture uses its actual supported 640x480 viewport");
        host.GetWindow().SetTestInputMode(true);
        SDL_GL_SetSwapInterval(0);
        std::string ignored;
        host.Audio().Init(ignored, true);
    };
    control.beforeFrame = [&](EngineHost&, RuntimeWorld& world, InteractivePlay&) {
        if (phase == Phase::Loading && &world != outgoing) {
            Endpoint(world, expected);
            freeze = true;
            phase = Phase::Restored;
        }
    };
    control.afterFrame = [&](EngineHost& host, RuntimeWorld& world, InteractivePlay& play) {
        ++frame;
        if (!world.Scripts() || !world.Scripts()->Diagnostics().empty()) {
            Check(false, world.Scripts() && !world.Scripts()->Diagnostics().empty()
                ? world.Scripts()->Diagnostics().front().message : "Script host missing");
            Quit(); return;
        }
        auto& window = host.GetWindow();
        const auto state = State(world);
        auto* saves = world.SceneControl()->Saves(host.Resources());
        if (releasePointer) { window.QueueTestPhysical("mouse:Left", 0); releasePointer = false; }
        auto click = [&](const char* id) {
            const auto document = world.UI().Find("willow_town");
            world.UI().Layout(window.Width(), window.Height());
            const auto* layout = world.UI().LayoutOf(document, id);
            Check(layout && layout->visible && layout->enabled, std::string("usable authored UI control: ") + id);
            if (layout && layout->visible && layout->enabled) {
                const auto p = layout->rect.position + layout->rect.size * .5f;
                window.SetTestPointer(int(p.x), int(p.y));
                window.QueueTestPhysical("mouse:Left", 1);
                releasePointer = true;
            }
        };
        auto shot = [&](const char* name) {
            std::vector<unsigned char> pixels;
            host.GetRenderer().CaptureFrame(window.Width(), window.Height(), pixels);
            Check(WriteRgbPng((output / name).string(), window.Width(), window.Height(), pixels), "capture actual town frame");
        };
        if (play.LastFixedStepsThisFrame()) worstPhysics = std::max(worstPhysics, double(play.LastFixedStepMilliseconds()));
        for (std::uint64_t id = 301; id <= 306; ++id) {
            const auto p = world.Physics().GetTransform(world.RuntimeBody(id)).position;
            if (!initialPositions.count(id)) initialPositions[id] = p;
            if (glm::distance(initialPositions.at(id), p) > .5f) moved.insert(id);
        }
        if (phase == Phase::Restored) {
            Endpoint(world, expected);
            const auto& saved = expected.at("state");
            Check(state.at("residents") == saved.at("residents"), "cold save preserves all resident needs, money, jobs and decision snapshots exactly");
            Check(state.at("shop") == saved.at("shop") && state.at("time") == saved.at("time"), "cold save preserves shop ledger and town clock without replaying rewards");
            const auto native = Agents(world, state);
            Check(native.size() == 6, "restoration reconstructs six real native utility agents");
            for (const auto& resident : saved.at("residents")) {
                const std::string id = resident.at("id");
                for (const auto field : {"elapsed", "untilEvaluation", "selected", "status", "cooldowns"})
                    Check(native.at(id).at(field) == resident.at("snapshot").at(field), "restored actual native " + id + " " + field);
            }
            freeze = false;
            phase = Phase::Advanced;
        } else if (phase == Phase::Advanced) {
            const auto native = Agents(world, state);
            const double dt = double(1.f / 60); // Utility clocks retain real fixed-step cadence.
            for (const auto& resident : expected.at("state").at("residents")) {
                const std::string id = resident.at("id");
                const double elapsed = native.at(id).at("elapsed"), previous = resident.at("snapshot").at("elapsed");
                Check(std::abs((elapsed - previous) - dt) < 1e-8, "native decision clock continues one real fixed step: " + id);
            }
            bool changed = false;
            for (std::uint64_t id = 301; id <= 306; ++id) {
                const auto actual = world.Physics().GetTransform(world.RuntimeBody(id)).position;
                const auto& p = expected.at("positions").at(std::to_string(id));
                changed |= actual != glm::vec3(p[0].get<float>(), p[1].get<float>(), p[2].get<float>());
            }
            Check(changed, "restored current actions continue moving actual physical residents");
            complete = true; phase = Phase::Complete; Quit();
        } else if (phase == Phase::Playing && cold) {
            Check(state.at("time").get<double>() < expected.at("state").at("time").get<double>(), "fresh process begins without the saved progress already present");
            outgoing = &world;
            request = saves->Request("load", "willow-test", "", "{}", error);
            Check(request != 0, "cold process requests genuine modern save restoration: " + error);
            phase = Phase::Loading;
        } else if (phase == Phase::Playing) {
            if (frame == 20) {
                Check(state.at("residents").size() == 6 && Agents(world, state).size() == 6, "six citizens have six independently instantiated native brains");
                Check(host.Assets().Problems().empty(), "all game assets resolve through normal asset ownership");
                Check(world.view.has_value(), "game publishes its real orthographic view");
                if (world.view) {
                    const auto& view = *world.view;
                    const auto camera = BuildCameraFrame(view.pose.position, view.pose.rotation, view.fov, view.nearPlane, view.farPlane, view.lens, window.Width(), window.Height());
                    const auto projected = camera.Project(world.Physics().GetTransform(world.RuntimeBody(306)).position);
                    Check(projected.inside && !projected.behind, "physical citizen projects into a usable visible pixel");
                    window.SetTestPointer(int(projected.position.x * window.Width()), int(projected.position.y * window.Height()));
                    window.QueueTestPhysical("mouse:Left", 1); releasePointer = true;
                }
            }
            if (frame == 25) { worldSelected = state.at("selected") == "306"; Check(worldSelected, "clicking a real world citizen selects that citizen's stats"); click("control"); }
            if (frame == 30) {
                Check(state.at("controlled") == "306", "Take control assigns the selected resident through game UI");
                const auto document = world.UI().Find("willow_town");
                const auto* name = world.UI().Element(document, "person-name");
                Check(name && name->visible && name->text == "Sam", "clicked person's visible name is Sam rather than a placeholder");
                const Json* selected = nullptr;
                for (const auto& person : state.at("residents")) if (person.at("id") == "306") selected = &person;
                if (!selected) throw std::runtime_error("Selected person is missing");
                for (const auto& field : {std::pair<const char*, const char*>{"hunger", "Hunger"},
                        {"fatigue", "Tiredness"}, {"social", "Needs company"}}) {
                    const auto* text = world.UI().Element(document, "need-" + std::string(field.first));
                    const auto percentage = std::lround(std::clamp(selected->at(field.first).get<double>(), 0., 1.) * 100);
                    const auto expectedText = std::string(field.second) + "  " + std::to_string(percentage) + "%";
                    Check(text && text->visible && text->text == expectedText,
                        "selected person's visible " + std::string(field.first) + " percentage matches their actual state");
                }
                controlStart = world.Physics().GetTransform(world.RuntimeBody(306)).position;
                window.QueueTestPhysical("key:D", 1);
            }
            if (frame == 90) window.QueueTestPhysical("key:D", 0);
            if (frame == 95) {
                const auto p = world.Physics().GetTransform(world.RuntimeBody(306)).position;
                exercisedControl = p.x > controlStart.x + .5f;
                Check(exercisedControl, "ordinary WASD input moves the controlled resident's actual native body");
                click("control");
            }
            if (frame == 100) { Check(state.at("controlled").is_null(), "Return to AI relinquishes direct control"); click("pause"); }
            if (frame == 105) { pausedTime = state.at("time"); pausedPosition = world.Physics().GetTransform(world.RuntimeBody(306)).position; }
            if (frame == 115) {
                exercisedPause = state.at("time").get<double>() == pausedTime && world.Physics().GetTransform(world.RuntimeBody(306)).position == pausedPosition;
                Check(exercisedPause, "town pause freezes needs, decisions and commanded physical movement");
                click("pause");
            }
            if (frame == 120) click("speed");
            if (frame == 130) { Check(state.at("speed") == 3, "visible speed control accelerates game policy without changing fixed physics cadence"); shot("watching.png"); }
            if (!measured400 && state.at("time").get<double>() >= 400) {
                progressAt400 = Sum(state, "workDone") + Sum(state, "meals") + Sum(state, "rests") + Sum(state, "chats");
                for (const auto& resident : state.at("residents"))
                    residentProgressAt400[resident.at("id").get<std::string>()] = CompletedServices(resident);
                measured400 = true;
                std::printf("LONG_WATCH time=%.3f completed_services=%u\n", state.at("time").get<double>(), progressAt400);
                shot("watch-400.png");
            }
            if (state.at("time").get<double>() >= 600 && frame > 140) {
                const auto progress = Sum(state, "workDone") + Sum(state, "meals") + Sum(state, "rests") + Sum(state, "chats");
                Check(measured400 && progress > progressAt400, "long watch continues completing actual work and needs between 400 and 600 game seconds");
                for (const auto& resident : state.at("residents")) {
                    const auto id = resident.at("id").get<std::string>(), name = resident.at("name").get<std::string>();
                    Check(residentProgressAt400.count(id) && CompletedServices(resident) > residentProgressAt400.at(id),
                        "each resident continues real services between 400 and 600 seconds: " + name);
                    Check(resident.at("workDone").get<unsigned>() > 0 && resident.at("meals").get<unsigned>() > 0 &&
                        resident.at("rests").get<unsigned>() > 0,
                        "each resident has genuinely worked, eaten and rested by 600 seconds: " + name);
                }
                std::printf("LONG_WATCH time=%.3f completed_services=%u work=%u meals=%u rest=%u chats=%u\n",
                    state.at("time").get<double>(), progress, Sum(state, "workDone"), Sum(state, "meals"), Sum(state, "rests"), Sum(state, "chats"));
                shot("watch-600.png");
                Check(worldSelected && exercisedControl && exercisedPause, "real UI interaction sequence completed");
                Check(moved.size() == 6, "all six residents genuinely travel through the town");
                Check(Sum(state, "workDone") > 0, "residents physically arrive at jobs and complete paid work");
                Check(Sum(state, "meals") > 0 && state.at("shop").at("sales").get<unsigned>() > 0, "shopping consumes stock, money and actual completed meals");
                Check(Sum(state, "rests") > 0 && Sum(state, "chats") > 0, "fatigue and social needs cause real completed services");
                for (const auto& resident : state.at("residents"))
                    Check(resident.at("money").get<int>() == 8 + 3 * resident.at("workDone").get<int>() - 2 * resident.at("meals").get<int>(), "independent wage/purchase ledger: " + resident.at("name").get<std::string>());
                click("shop-toggle");
                phase = Phase::ClosedShop; phaseFrame = frame;
            }
        } else if (phase == Phase::ClosedShop) {
            if (frame == phaseFrame + 5) { Check(!state.at("shop").at("open").get<bool>(), "shop closure really changes availability"); click("resident-301"); }
            if (frame == phaseFrame + 10) { click("hungry"); closedAt = state.at("time"); closedMeals = Sum(state, "meals"); closedSales = state.at("shop").at("sales"); }
            if (frame > phaseFrame + 12 && state.at("time").get<double>() > closedAt + 8) {
                Check(Sum(state, "meals") == closedMeals && state.at("shop").at("sales").get<unsigned>() == closedSales, "negative control: a closed shop cannot grant meals or sales despite raised hunger");
                bool rejected = false;
                for (const auto& agent : Agents(world, state)) for (const auto& row : agent.at("candidates"))
                    if (row.at("action") == "eat" && !row.at("eligible").get<bool>()) rejected = true;
                Check(rejected, "actual native candidate diagnostics mark eating ineligible while the shop is closed");
                click("shop-toggle"); phase = Phase::ReopenedShop; phaseFrame = frame; reopeningMeals = Sum(state, "meals");
            }
        } else if (phase == Phase::ReopenedShop) {
            if (frame == phaseFrame + 5) click("restock");
            if (frame == phaseFrame + 10) click("hungry");
            bool moving = false;
            for (std::uint64_t id = 301; id <= 306; ++id) moving |= glm::length(world.Physics().GetLinearVelocity(world.RuntimeBody(id))) > .1f;
            if (frame > phaseFrame + 12 && Sum(state, "meals") > reopeningMeals && moving) {
                Check(state.at("shop").at("open").get<bool>(), "reopened stocked shop resumes real purchases and meals");
                shot("progressed.png");
                request = saves->Request("save", "willow-test", "Willow Town played continuation", "{}", error);
                Check(request != 0, "played town saves through the production modern save service: " + error);
                phase = Phase::Saving;
            }
            if (frame > phaseFrame + 2400) { Check(false, "reopened shop produces a genuine meal within bounded gameplay"); Quit(); }
        } else if (phase == Phase::Saving) {
            const auto* status = saves->Status(request);
            if (status && status->state == "completed") {
                GameSnapshot snapshot; bool recovered = false;
                SaveStorage storage(saves->Root());
                Check(storage.Read("willow-test", snapshot, recovered, error), "independently read the committed town save: " + error);
                Scene scene;
                Check(WorldPersistence::SceneFrom(snapshot.participants, scene, error), "independently decode saved physical town: " + error);
                expected = {{"state", StoredState(snapshot)}, {"positions", Json::object()}};
                for (std::uint64_t id = 301; id <= 306; ++id) {
                    const auto* object = scene.Find(id);
                    if (!object) throw std::runtime_error("Saved resident physical entity missing");
                    const auto p = object->transform.position;
                    expected["positions"][std::to_string(id)] = {p.x, p.y, p.z};
                }
                std::ofstream(output / "expected.json") << expected.dump(2);
                outgoing = &world;
                request = saves->Request("load", "willow-test", "", "{}", error);
                Check(request != 0, "same-process game restores its genuine played save: " + error);
                phase = Phase::Loading;
            }
        }
        if ((phase == Phase::Saving || phase == Phase::Loading) && request) {
            const auto* status = saves->Status(request);
            if (status && status->state == "failed") { Check(false, status->error); Quit(); }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // 600 game seconds at 3x need about 12,000 physical frames. Leave a
        // separate bounded allowance for manual controls, negative controls and
        // the real asynchronous save/load stages; this is not a solver budget.
        if (frame > 22000) { Check(false, "bounded 600-second town workflow completes"); Quit(); }
    };
    Application application;
    char executable[] = "judas";
    char* arguments[] = {executable, projectFile.data()};
    Check(application.Run(2, arguments, &control) == 0 && complete, "ordinary town application completes and shuts down");
    std::printf("WILLOW_TOWN mode=%s checks=%u failures=%u worst_fixed_ms=%.6f\n", cold ? "cold" : "write", checks, failures, worstPhysics);
    return failures ? 1 : 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL %s\n", error.what());
    return 1;
}
