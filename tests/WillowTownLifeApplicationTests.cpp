// Consumer-life integration through the real application, UI and modern saves.
// Inputs establish game conditions; native utility choices and physical arrivals
// must produce employment and service effects. No test writes published state.
#include "GraphicsApi.h"
#include "Application.h"
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
Json Person(const Json& state, std::string id) {
    for (const auto& person : state.at("residents")) if (person.at("id") == id) return person;
    throw std::runtime_error("Required resident is missing: " + id);
}
Json Job(const Json& state, std::string id) {
    for (const auto& job : state.at("jobs")) if (job.at("id") == id) return job;
    throw std::runtime_error("Required job is missing: " + id);
}
Json Agents(RuntimeWorld& world, const Json& state) {
    Json result = Json::object();
    for (const auto& native : world.Scripts()->AIInspections()) {
        if (native.entity != 1 || native.slot != 1) continue;
        bool found = false;
        for (const auto& person : state.at("residents")) {
            const std::string name = person.at("name"), id = person.at("id");
            if (native.name.rfind(name, 0) == 0) {
                if (result.contains(id)) throw std::runtime_error("Duplicate resident native brain");
                result[id] = Json::parse(native.json); found = true; break;
            }
        }
        if (!found) throw std::runtime_error("Unrecognised native resident brain: " + native.name);
    }
    return result;
}
bool Selected(const Json& agents, const std::string& id, const char* action) {
    return agents.contains(id) && !agents.at(id).at("selected").is_null() &&
        agents.at(id).at("selected").at("action") == action;
}
Json Effects(const Json& person) {
    return {{"workDone", person.at("workDone")}, {"meals", person.at("meals")},
        {"rests", person.at("rests")}, {"chats", person.at("chats")},
        {"money", person.at("money")}, {"jobsFound", person.at("jobsFound")}};
}
Json StoredState(const GameSnapshot& snapshot) {
    const auto found = snapshot.participants.find("scripts");
    if (found == snapshot.participants.end() || found->second.version != 1)
        throw std::runtime_error("Modern script participant missing");
    SaveArchive archive(found->second.data);
    std::uint32_t count = 0; archive(count);
    if (count > 4096) throw std::runtime_error("Unbounded script save participant");
    Json state;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint64_t entity = 0, slot = 0; std::string text;
        archive(entity, slot, text);
        if (entity == 1 && slot == 1) state = Json::parse(text);
    }
    archive.Finish();
    if (!state.is_object()) throw std::runtime_error("Saved town state missing");
    return state;
}
void CheckEndpoints(RuntimeWorld& world, const Json& expected) {
    for (std::uint64_t id = 301; id <= 306; ++id) {
        const auto p = world.Physics().GetTransform(world.RuntimeBody(id)).position;
        const auto& saved = expected.at("positions").at(std::to_string(id));
        Check(p == glm::vec3(saved[0].get<float>(), saved[1].get<float>(), saved[2].get<float>()),
            "restored actual physical endpoint before stepping: " + std::to_string(id));
    }
}
void EmploymentInvariant(const Json& state) {
    std::set<std::string> employees;
    for (const auto& job : state.at("jobs")) if (!job.at("employeeId").is_null()) {
        const std::string id = job.at("employeeId");
        Check(employees.insert(id).second, "one resident does not occupy multiple jobs: " + id);
        const auto person = Person(state, id);
        Check(person.at("alive").get<bool>() && person.at("jobId") == job.at("id"),
            "job board ownership agrees with a living resident: " + id);
    }
}
} // namespace

int main(int argc, char** argv) try {
    if (argc != 3 && argc != 4) {
        std::fprintf(stderr, "usage: %s <Willow_Town.judasproj> <fresh-output-directory> [cold|absence]\n", argv[0]);
        return 2;
    }
    const bool cold = argc == 4 && std::string(argv[3]) == "cold";
    const bool absence = argc == 4 && std::string(argv[3]) == "absence";
    if (argc == 4 && !cold && !absence) throw std::runtime_error("Unknown mode");
    const auto source = fs::absolute(argv[1]), output = fs::absolute(argv[2]);
    const auto project = output / "project";
    if (!cold) {
        if (fs::exists(output)) throw std::runtime_error("Write test requires a fresh output directory");
        fs::create_directories(output);
        fs::copy(source.parent_path(), project, fs::copy_options::recursive);
    }
    SDL_setenv("XDG_DATA_HOME", (output / "user").string().c_str(), 1);
    // Match the original town application's supported responsive viewport.
    // This bounds software pixel work without shortening any gameplay checks.
    SDL_setenv("JUDAS_WINDOW_SIZE", "640x480", 1);
    std::string projectFile = (project / source.filename()).string(), error;
    Project definition;
    Check(definition.Load(projectFile, error), "isolated life-game project loads: " + error);
    Json expected;
    if (cold) { std::ifstream stream(output / "expected.json"); stream >> expected; }

    enum class Phase { HiringPaused, Hiring, HiredUI, Fired, Rehiring, SelectSam, ControlSam,
        Killing, SelectAda, CloseCourier, NewRole, RoleWork, DeadHold, Saving,
        Loading, Restored, Advanced, Complete };
    Phase phase = Phase::HiringPaused;
    unsigned frame = 0, phaseFrame = 0, hurtClicks = 0;
    double phaseTime = 0, deathAt = 0, livingClock = 0;
    bool releasePointer = false, freeze = false, complete = false;
    bool sawJobSearch = false, sawRoleSearch = false;
    glm::vec3 adaStart{}, deadPosition{};
    unsigned priorParcels = 0, roleWork = 0;
    Json deadEffects;
    std::uint64_t request = 0;
    RuntimeWorld* outgoing = nullptr;
    double worstPhysics = 0;
    glm::vec3 heldPosition{};
    double holdTime = 0, initialAbsence = 0;
    unsigned heldWork = 0, heldEarnings = 0, initialLost = 0, initialFirings = 0;
    ApplicationControl control;
    control.hidden = true;
    control.frameSeconds = [&](float) { return freeze ? 0.f : 1.f / 60; };
    control.hostReady = [](EngineHost& host) {
        Check(host.GetWindow().Width() == 640 && host.GetWindow().Height() == 480,
            "software-renderable fixture uses its actual supported 640x480 viewport");
        host.GetWindow().SetTestInputMode(true); GraphicsSetSwapInterval(0);
        std::string ignored; host.Audio().Init(ignored, true);
    };
    control.beforeFrame = [&](EngineHost&, RuntimeWorld& world, InteractivePlay&) {
        if (phase == Phase::Loading && &world != outgoing) {
            CheckEndpoints(world, expected); freeze = true; phase = Phase::Restored;
        }
    };
    control.afterFrame = [&](EngineHost& host, RuntimeWorld& world, InteractivePlay& play) {
        ++frame;
        if (!world.Scripts() || !world.Scripts()->Diagnostics().empty()) {
            Check(false, world.Scripts() && !world.Scripts()->Diagnostics().empty()
                ? world.Scripts()->Diagnostics().front().message : "Script host missing");
            Quit(); return;
        }
        const auto state = State(world), agents = Agents(world, state);
        const auto ada = Person(state, "305");
        const auto sam = Person(state, "306");
        const auto time = state.at("time").get<double>();
        auto& window = host.GetWindow();
        auto* saves = world.SceneControl()->Saves(host.Resources());
        if (releasePointer) { window.QueueTestPhysical("mouse:Left", 0); releasePointer = false; }
        auto click = [&](const char* id) {
            const auto document = world.UI().Find("willow_town");
            world.UI().Layout(window.Width(), window.Height());
            const auto* layout = world.UI().LayoutOf(document, id);
            Check(layout && layout->visible && layout->enabled, std::string("usable life-game UI control: ") + id);
            if (layout && layout->visible && layout->enabled) {
                const auto p = layout->rect.position + layout->rect.size * .5f;
                window.SetTestPointer(int(p.x), int(p.y));
                window.QueueTestPhysical("mouse:Left", 1); releasePointer = true;
            }
        };
        auto enter = [&](Phase next) { phase = next; phaseFrame = frame; phaseTime = time; };
        auto boardArrival = [&] {
            const auto p = world.Physics().GetTransform(world.RuntimeBody(305)).position;
            const auto* board = world.RuntimeDefinition(1105);
            Check(board && glm::distance(glm::vec2(p), glm::vec2(board->transform.position)) <= .35f,
                "hiring follows actual arrival at the public job board, not a remote assignment");
        };
        if (play.LastFixedStepsThisFrame()) worstPhysics = std::max(worstPhysics, double(play.LastFixedStepMilliseconds()));
        if (absence) {
            const auto mara = Person(state, "301");
            if (frame == 1) Check(mara.at("alive").get<bool>() && mara.at("jobId") == "baker",
                "absence case starts with a living employed baker during the ordinary morning shift");
            if (frame == 5) click("resident-301");
            if (frame == 10) click("control");
            if (frame == 15) {
                Check(state.at("controlled") == "301", "normal UI takes control before the absence observation");
                heldPosition = world.Physics().GetTransform(world.RuntimeBody(301)).position;
                holdTime = time; initialAbsence = mara.at("absenceWorkingSeconds");
                heldWork = mara.at("workDone"); heldEarnings = mara.at("workEarnings");
                initialLost = mara.at("jobsLost"); initialFirings = state.at("census").at("firings");
            }
            if (frame == 20) click("speed");
            if (frame > 25 && mara.at("jobId").is_null()) {
                Check(mara.at("jobLostReason").get<std::string>().find("Missed") != std::string::npos,
                    "actual job loss records automatic missed-work dismissal, not a manager command");
                Check(mara.at("lastEmploymentChange").get<double>() - holdTime + initialAbsence >= 65 - 1e-6,
                    "automatic dismissal requires the original 65 working-game-second absence window");
                Check(Job(state, "baker").at("employeeId").is_null(), "automatic dismissal vacates actual job-board ownership");
                Check(mara.at("jobsLost").get<unsigned>() == initialLost + 1 &&
                    state.at("census").at("firings").get<unsigned>() >= initialFirings + 1,
                    "automatic dismissal records resident and town counters exactly once for this employee");
                Check(mara.at("workDone").get<unsigned>() == heldWork && mara.at("workEarnings").get<unsigned>() == heldEarnings,
                    "holding a resident away from work grants no paid task or earnings");
                Check(mara.at("alive").get<bool>() && state.at("controlled") == "301",
                    "missed work causes unemployment while the living resident remains under player control");
                Check(world.Physics().GetTransform(world.RuntimeBody(301)).position == heldPosition &&
                    glm::length(world.Physics().GetLinearVelocity(world.RuntimeBody(301))) == 0,
                    "ordinary held control keeps the actual body stationary throughout the absence");
                Check(!Selected(agents, "301", "work"), "automatically dismissed employee has no stale native work selection");
                EmploymentInvariant(state); complete = true; enter(Phase::Complete); Quit();
            }
            if (time > 100 || frame > 2500) { Check(false, "bounded morning absence case completes"); Quit(); }
            return;
        }
        if (phase == Phase::Restored) {
            const auto& saved = expected.at("state");
            CheckEndpoints(world, expected);
            for (const auto field : {"residents", "jobs", "census", "shop", "time", "hiringOpen"})
                Check(state.at(field) == saved.at(field), std::string("cold restoration preserves actual life state: ") + field);
            Check(!sam.at("alive").get<bool>() && !agents.contains("306"),
                "dead resident stays dead and has no resurrected native brain");
            Check(world.RuntimeBody(306).IsValid() && !world.Physics().IsBodyEnabled(world.RuntimeBody(306)),
                "restored deceased body remains natively disabled before any fixed step");
            for (const auto& person : saved.at("residents")) {
                const std::string id = person.at("id");
                if (!person.at("alive").get<bool>()) continue;
                Check(agents.contains(id), "living resident reconstructs its native brain: " + id);
                for (const auto field : {"elapsed", "untilEvaluation", "selected", "status", "cooldowns"})
                    Check(agents.at(id).at(field) == person.at("snapshot").at(field),
                        "restored native continuation " + id + " " + field);
            }
            EmploymentInvariant(state);
            freeze = false; enter(Phase::Advanced);
        } else if (phase == Phase::Advanced) {
            Check(!sam.at("alive").get<bool>() && !agents.contains("306"), "a real post-restore step does not revive the deceased");
            Check(world.RuntimeBody(306).IsValid() && !world.Physics().IsBodyEnabled(world.RuntimeBody(306)),
                "a real post-restore fixed step does not reenable the deceased collider");
            Check(Effects(sam) == Effects(Person(expected.at("state"), "306")), "deceased cannot earn, eat, rest or find a job after restoration");
            for (const auto& person : expected.at("state").at("residents")) if (person.at("alive").get<bool>()) {
                const std::string id = person.at("id");
                const double prior = person.at("snapshot").at("elapsed"), now = agents.at(id).at("elapsed");
                Check(std::abs(now - prior - double(1.f / 60)) < 1e-8, "living native clock continues one actual fixed step: " + id);
            }
            complete = true; enter(Phase::Complete); Quit();
        } else if (cold && phase == Phase::HiringPaused) {
            Check(time < expected.at("state").at("time").get<double>(), "fresh process starts before the saved life progression");
            outgoing = &world; request = saves->Request("load", "willow-life-test", "", "{}", error);
            Check(request != 0, "request genuine fresh-process life-save restoration: " + error); enter(Phase::Loading);
        } else if (phase == Phase::HiringPaused) {
            if (frame == 1) {
                Check(state.at("version") == 2 && state.at("residents").size() == 6 && agents.size() == 6,
                    "six living residents start with the versioned life model and six real brains");
                Check(ada.at("jobId").is_null() && Job(state, "courier").at("employeeId").is_null(),
                    "unemployed applicant and actual vacant courier job exist initially");
                adaStart = world.Physics().GetTransform(world.RuntimeBody(305)).position;
            }
            if (frame == 5) click("resident-305");
            if (frame == 10) click("hiring-toggle");
            if (frame == 15) {
                Check(!state.at("hiringOpen").get<bool>(), "hiring pause changes the actual project employment policy");
                phaseTime = time;
            }
            if (frame == 20) click("speed");
            if (frame > 25 && time >= phaseTime + 8) {
                Check(!state.at("hiringOpen").get<bool>() && Job(state, "courier").at("open").get<bool>(),
                    "negative control retains a genuine open vacancy while applications are paused");
                Check(ada.at("jobId").is_null() && ada.at("jobsFound") == 0,
                    "paused hiring cannot manufacture employment despite a real vacancy");
                bool rejected = false;
                for (const auto& row : agents.at("305").at("candidates"))
                    if (row.at("action") == "jobsearch" && !row.at("eligible").get<bool>()) rejected = true;
                Check(rejected, "actual native job-search diagnostics reject paused applications");
                click("hiring-toggle"); enter(Phase::Hiring);
            }
        } else if (phase == Phase::Hiring) {
            sawJobSearch |= Selected(agents, "305", "jobsearch");
            if (!ada.at("jobId").is_null()) {
                Check(sawJobSearch && ada.at("jobId") == "courier" && ada.at("jobsFound").get<unsigned>() == 1,
                    "native job-search action obtains an actual job after hiring resumes");
                Check(glm::distance(adaStart, world.Physics().GetTransform(world.RuntimeBody(305)).position) > .5f,
                    "applicant genuinely travels to seek employment");
                boardArrival(); EmploymentInvariant(state);
                // The hire happened in fixedUpdate, after this frame's UI
                // publication. Wait for the ordinary next UI boundary before
                // attempting the newly enabled manager button.
                enter(Phase::HiredUI);
            }
            if (time > phaseTime + 180) { Check(false, "bounded applicant finds the available job"); Quit(); }
        } else if (phase == Phase::HiredUI && frame >= phaseFrame + 2) {
            const auto document = world.UI().Find("willow_town");
            const auto* button = world.UI().Element(document, "dismiss");
            if (button && button->visible && button->enabled) {
                Check(true, "ordinary UI publication enables dismissal after the actual hire");
                click("dismiss"); enter(Phase::Fired);
            } else if (frame > phaseFrame + 15) {
                Check(false, "newly hired resident exposes its dismissal control within bounded UI frames");
                Quit();
            }
        } else if (phase == Phase::Fired) {
            if (frame == phaseFrame + 3) {
                Check(ada.at("jobId").is_null() && Job(state, "courier").at("employeeId").is_null(),
                    "dismissal clears both resident employment and actual job-board ownership");
                Check(ada.at("jobsLost").get<unsigned>() >= 1 && state.at("census").at("firings").get<unsigned>() >= 1,
                    "dismissal persists its reason and counters");
                Check(!ada.at("jobLostReason").get<std::string>().empty() && !Selected(agents, "305", "work"),
                    "dismissed resident cannot continue a stale paid work action");
                sawJobSearch = false; enter(Phase::Rehiring);
            }
        } else if (phase == Phase::Rehiring) {
            sawJobSearch |= Selected(agents, "305", "jobsearch");
            if (ada.at("jobsFound").get<unsigned>() >= 2) {
                Check(sawJobSearch && ada.at("jobId") == "courier", "dismissed resident independently finds work again");
                boardArrival(); click("resident-306"); enter(Phase::SelectSam);
            }
            if (time > phaseTime + 180) { Check(false, "bounded dismissed resident returns to employment"); Quit(); }
        } else if (phase == Phase::SelectSam && frame == phaseFrame + 4) {
            Check(state.at("selected") == "306" && sam.at("alive").get<bool>(), "living Sam can be selected for health and control inspection");
            click("control"); enter(Phase::ControlSam);
        } else if (phase == Phase::ControlSam && frame == phaseFrame + 4) {
            Check(state.at("controlled") == "306", "resident is genuinely controlled before the death boundary");
            click("hurt"); hurtClicks = 1; enter(Phase::Killing);
        } else if (phase == Phase::Killing) {
            if (!sam.at("alive").get<bool>()) {
                Check(sam.at("health").get<double>() == 0 && !sam.at("deathCause").get<std::string>().empty(),
                    "health loss produces a recorded death with a cause");
                Check(state.at("controlled").is_null() && !agents.contains("306") && agents.size() == 5,
                    "death relinquishes control and retires only the dead resident's actual native brain");
                Check(Job(state, "keeper").at("employeeId").is_null(), "death vacates the actual park-keeper job");
                Check(glm::length(world.Physics().GetLinearVelocity(world.RuntimeBody(306))) == 0,
                    "dead resident's actual physical body is stopped");
                Check(world.RuntimeBody(306).IsValid() && !world.Physics().IsBodyEnabled(world.RuntimeBody(306)),
                    "death disables the actual native body rather than only changing a life flag");
                const auto document = world.UI().Find("willow_town");
                const auto* button = world.UI().Element(document, "control");
                Check(button && !button->enabled, "dead resident cannot be taken over through the normal game interface");
                deadPosition = world.Physics().GetTransform(world.RuntimeBody(306)).position;
                deadEffects = Effects(sam); deathAt = time;
                Check(agents.contains("301"), "unrelated living resident retains its native decision agent");
                livingClock = agents.at("301").at("elapsed");
                click("resident-305"); enter(Phase::SelectAda);
            } else if (frame > phaseFrame + 3 && (frame - phaseFrame) % 4 == 0 && hurtClicks < 6) {
                click("hurt"); ++hurtClicks;
            }
            if (frame > phaseFrame + 40) { Check(false, "bounded explicit health control reaches the death boundary"); Quit(); }
        } else if (phase == Phase::SelectAda && frame == phaseFrame + 4) {
            Check(ada.at("jobId") == "courier", "living applicant still holds the original job before workplace closure");
            priorParcels = state.at("shop").at("parcels"); click("job-toggle"); enter(Phase::CloseCourier);
        } else if (phase == Phase::CloseCourier && frame == phaseFrame + 4) {
            Check(!Job(state, "courier").at("open").get<bool>() && Job(state, "courier").at("employeeId").is_null(),
                "closing the courier workplace removes its actual occupied position");
            Check(ada.at("jobId").is_null() && ada.at("jobsLost").get<unsigned>() >= 2,
                "workplace closure is a real second job loss rather than a label change");
            sawRoleSearch = false; enter(Phase::NewRole);
        } else if (phase == Phase::NewRole) {
            sawRoleSearch |= Selected(agents, "305", "jobsearch");
            if (!ada.at("jobId").is_null()) {
                Check(sawRoleSearch && ada.at("jobId") == "keeper" && ada.at("jobsFound").get<unsigned>() >= 3,
                    "native job search fills the deceased resident's vacancy with a different worker");
                Check(Job(state, "keeper").at("employeeId") == "305" && Job(state, "courier").at("employeeId").is_null(),
                    "closed job remains unfilled while the open replacement role has correct ownership");
                boardArrival(); EmploymentInvariant(state); roleWork = ada.at("workDone"); enter(Phase::RoleWork);
            }
            if (time > phaseTime + 240) { Check(false, "bounded applicant obtains the different available role"); Quit(); }
        } else if (phase == Phase::RoleWork) {
            if (ada.at("workDone").get<unsigned>() > roleWork) {
                Check(ada.at("jobId") == "keeper", "new employee actually performs the replacement job");
                const auto p = world.Physics().GetTransform(world.RuntimeBody(305)).position;
                const auto* workplace = world.RuntimeDefinition(1106);
                Check(workplace && glm::distance(glm::vec2(p), glm::vec2(workplace->transform.position)) <= .35f,
                    "replacement wages require arrival at the new physical workplace");
                Check(state.at("shop").at("parcels").get<unsigned>() == priorParcels,
                    "work follows current role: former courier no longer creates courier parcels");
                enter(Phase::DeadHold);
            }
            // A full day plus travel allowance permits a genuine off-shift wait.
            if (time > phaseTime + 540) { Check(false, "replacement job completes within a day and travel allowance"); Quit(); }
        } else if (phase == Phase::DeadHold && time > phaseTime + 10) {
            Check(!sam.at("alive").get<bool>() && Effects(sam) == deadEffects && !agents.contains("306"),
                "deceased resident receives no later wages, services or employment");
            Check(world.Physics().GetTransform(world.RuntimeBody(306)).position == deadPosition,
                "deceased physical endpoint stays fixed while the rest of town continues");
            Check(agents.at("301").at("elapsed").get<double>() > livingClock && time > deathAt,
                "unrelated living native agent and world continue after death");
            for (const auto& person : state.at("residents"))
                Check(person.at("money").get<int>() == 8 + 3 * person.at("workDone").get<int>() - 2 * person.at("meals").get<int>(),
                    "independent life-game wage/purchase ledger: " + person.at("name").get<std::string>());
            request = saves->Request("save", "willow-life-test", "Played employment and death", "{}", error);
            Check(request != 0, "played life state requests a genuine modern save: " + error); enter(Phase::Saving);
        } else if (phase == Phase::Saving) {
            const auto* status = saves->Status(request);
            if (status && status->state == "completed") {
                GameSnapshot snapshot; bool recovered = false; SaveStorage storage(saves->Root());
                Check(storage.Read("willow-life-test", snapshot, recovered, error), "independently read committed life save: " + error);
                Scene scene;
                Check(WorldPersistence::SceneFrom(snapshot.participants, scene, error), "independently decode saved life-world physics: " + error);
                expected = {{"state", StoredState(snapshot)}, {"positions", Json::object()}};
                for (std::uint64_t id = 301; id <= 306; ++id) {
                    const auto* object = scene.Find(id); if (!object) throw std::runtime_error("Saved resident missing");
                    const auto p = object->transform.position; expected["positions"][std::to_string(id)] = {p.x, p.y, p.z};
                }
                std::ofstream(output / "expected.json") << expected.dump(2);
                outgoing = &world; request = saves->Request("load", "willow-life-test", "", "{}", error);
                Check(request != 0, "same-process restore uses the genuine played life save: " + error); enter(Phase::Loading);
            }
        }
        if ((phase == Phase::Saving || phase == Phase::Loading) && request) {
            const auto* status = saves->Status(request);
            if (status && status->state == "failed") { Check(false, status->error); Quit(); }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (frame > 30000) { Check(false, "bounded life-game workflow completes"); Quit(); }
    };
    Application application; char program[] = "judas"; char* arguments[] = {program, projectFile.data()};
    Check(application.Run(2, arguments, &control) == 0 && complete, "ordinary life-game application completes and shuts down");
    std::printf("WILLOW_TOWN_LIFE mode=%s checks=%u failures=%u worst_fixed_ms=%.6f\n", cold ? "cold" : absence ? "absence" : "write", checks, failures, worstPhysics);
    return failures ? 1 : 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL %s\n", error.what()); return 1;
}
