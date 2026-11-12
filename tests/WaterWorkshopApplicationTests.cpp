#include "EngineHost.h"
#include "InteractivePlay.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "ScriptSystem.h"
#include "fluid/GameWaterRuntime.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>

// Exercise the actual shipped game script with real input and MK2 bodies,
// rather than a substitute controller that merely calls the same APIs.
int main() {

    int checks = 0, failures = 0;
    auto check = [&](bool passed, const std::string& message) {
        ++checks;
        failures += !passed;
        std::cout << (passed ? "PASS " : "FAIL ") << message << '\n';
    };
    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path() /
                      "projects/water_v2_workshop";
    std::string error;
    Project project;
    if (!project.Load((root / "water_v2_workshop.judasproj").string(), error)) {
        std::cerr << error << '\n';
        return 1;
    }
    EngineHost host;
    if (!host.Init("Water Workshop controls", 1280, 720, false, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    host.OpenProjectAssets(project.RootDir(), project.AssetsDir());
    host.Resources().SetBlockingMode(true);
    auto& window = host.GetWindow();
    window.SetTestInputMode(true);
    check(window.Input().SetMap(project.Settings().input, error), "use project input bindings");
    auto owned = std::make_unique<RuntimeWorld>();
    auto& world = *owned;
    world.ConfigureProject(project.Settings());
    Scene scene;
    if (!LoadSceneFromFile(project.StartupScenePath(), scene, error) ||
        !world.Build(scene, &host.Resources(), error)) {
        std::cerr << error << '\n';
        return 1;
    }
    host.Resources().WaitForAll();
    EntityId player = 0, crate = 0;
    for (const auto& object : scene.Objects()) {
        if (object.characterMotor) player = object.id;
        if (object.name == "Carryable light crate") crate = object.id;
    }
    check(player && crate, "actual authored motor and carryable crate exist");
    EntityPhysicalState playerState;
    world.GetEntityState(player, playerState);
    playerState.position = {-2, 3.35f, 9.8f};
    playerState.linearVelocity = {};
    check(world.SetEntityState(player, playerState), "position on real deck facing the real crate");
    InteractivePlay play;
    if (!play.Begin(world, WorldCoordinates(world.Settings().worldOrigin), error)) {
        std::cerr << error << '\n';
        return 1;
    }
    auto frame = [&](float dt = 1.f / 60, float lookY = 0.f,
                     const char* key = nullptr, float value = 0.f) {
        window.Input().BeginFrame();
        if (key) window.Input().SetPhysical(key, value);
        if (lookY) window.Input().AddDelta("mouse:dy", lookY);
        host.PumpResources();
        play.Frame(window, host.GetRenderer(), dt, false);
    };
    auto tap = [&](const char* key) {
        frame(1.f / 60, 0, key, 1);
        frame(1.f / 60, 0, key, 0);
    };
    auto hud = [&]() {
        const auto document = world.UI().Find("game_ui");
        const auto* element = world.UI().Element(document, "counter");
        return element ? element->text : std::string{};
    };
    for (int i = 0; i < 8; ++i) frame();
    EntityPhysicalState box;
    world.GetEntityState(crate, box);
    world.GetEntityState(player, playerState);
    const auto eye = playerState.position + glm::vec3(0, .65f, 0);
    const auto offset = box.position - eye;
    const float pitch = std::atan2(offset.y, -offset.z);
    frame(1.f / 60, -(pitch + .24f) / .0021f);
    const auto before = box.position;
    tap("key:G");
    for (int i = 0; i < 50; ++i) frame();
    world.GetEntityState(crate, box);
    std::cout << "GRAB displacement=" << glm::length(box.position - before)
              << " deltaY=" << box.position.y - before.y << '\n';
    check(box.position.y > before.y + .12f,
          "G lifts the aimed crate using the shipped controller and real forces");
    check(hud().find("Holding a prop") != std::string::npos,
          "HUD confirms a prop is actually held");
    const auto carried = box.position;
    tap("key:H");
    world.GetEntityState(crate, box);
    std::cout << "THROW velocity=" << box.linearVelocity.x << ','
              << box.linearVelocity.y << ',' << box.linearVelocity.z << '\n';
    check(box.linearVelocity.z < -3,
          "H actually launches the held body in the view direction");
    for (int i = 0; i < 20; ++i) frame();
    world.GetEntityState(crate, box);
    check(box.position.z < carried.z - .6f, "thrown body leaves the grip instead of snapping back");
    tap("key:H");
    check(hud().find("Pick up a prop with G before throwing") != std::string::npos,
          "empty-handed throw explains its prerequisite");
    // Reset only the fixture poses, then prove G drops rather than regrabbing.
    box.position = {-2, 2.77f, 7};
    box.linearVelocity = {};
    box.angularVelocity = {};
    world.SetEntityState(crate, box);
    tap("key:G");
    for (int i = 0; i < 30; ++i) frame();
    world.GetEntityState(crate, box);
    const auto lifted = box.position.y;
    tap("key:G");
    check(hud().find("Released the prop") != std::string::npos,
          "G explicitly releases a carried object");
    for (int i = 0; i < 22; ++i) frame();
    world.GetEntityState(crate, box);
    check(box.position.y < lifted - .06f, "released crate falls under real gravity");
    auto* water = world.GridWater()->Find(2);
    const auto added = water->AddedQuantity();
    tap("key:F");
    for (int i = 0; i < 12; ++i) frame();
    check(water->AddedQuantity() > added, "F supplies actual water through the pump");
    tap("key:F");
    const auto stopped = water->AddedQuantity();
    for (int i = 0; i < 6; ++i) frame();
    check(water->AddedQuantity() == stopped, "F stops the pump again");
    frame(0, 0, "key:F", 1);
    frame(0, 0, "key:F", 0);
    frame(1.f / 30);
    check(water->AddedQuantity() > stopped,
          "short tap survives zero-step frames and is not repeated during catch-up");
    tap("key:F");
    tap("key:Escape");
    check(play.IsPaused(), "Escape opens the pause menu");
    const auto steps = play.FixedStepsSinceReset();
    for (int i = 0; i < 4; ++i) frame();
    check(play.FixedStepsSinceReset() == steps, "pause stops physics");
    tap("key:Escape");
    check(!play.IsPaused(), "Escape resumes play");
    // Independent movement endpoints, while actually standing on the deck.
    world.GetEntityState(player, playerState);
    const auto walking = playerState.position;
    frame(1.f / 60, 0, "key:D", 1);
    for (int i = 0; i < 20; ++i) frame();
    frame(1.f / 60, 0, "key:D", 0);
    world.GetEntityState(player, playerState);
    check(playerState.position.x > walking.x + .6f, "D moves the motor across the real deck");
    tap("key:Space");
    world.GetEntityState(player, playerState);
    check(playerState.linearVelocity.y > 3, "Space jumps from supported ground");
    playerState.position = {-3, 2.8f, 3};
    playerState.linearVelocity = {};
    world.SetEntityState(player, playerState);
    // Aim down at an unobstructed part of the pool, away from the divider.
    frame(1.f / 60, -(-.55f - pitch) / .0021f);
    tap("key:B");
    check(hud().find("Splashed the water at the crosshair") != std::string::npos,
          "B reaches actual water along the view ray");
    check(water->Stats().maximumSpeed > .08f && !world.GridWater()->Paused(2),
          "splash produces a real water disturbance without pausing the solver");
    const auto waterQuantity = water->Quantity();
    playerState.position = {-3, 1.4f, 1};
    playerState.linearVelocity = {};
    world.SetEntityState(player, playerState);
    frame(1.f / 60, 0, "key:Q", 1);
    for (int i = 0; i < 12; ++i) frame();
    frame(1.f / 60, 0, "key:Q", 0);
    world.GetEntityState(player, playerState);
    const auto diving = playerState.linearVelocity.y;
    std::cout << "DIVE y=" << playerState.position.y << " vy=" << diving << '\n';
    check(diving < -.3f, "Q dives while actually immersed");
    frame(1.f / 60, 0, "key:Space", 1);
    for (int i = 0; i < 20; ++i) frame();
    frame(1.f / 60, 0, "key:Space", 0);
    world.GetEntityState(player, playerState);
    check(playerState.linearVelocity.y > .1f && playerState.linearVelocity.y > diving,
          "Space swims upward while immersed");
    check(std::abs(water->Quantity() - waterQuantity) < 1e-6,
          "splashing and swimming preserve water quantity");
    const auto crosshair = world.UI().LayoutOf(world.UI().Find("game_ui"), "crosshair");
    check(crosshair && glm::length(crosshair->rect.position + crosshair->rect.size * .5f -
                                  glm::vec2(640, 360)) < .1f,
          "reticle is centred on the actual camera ray");
    const auto* scripts = world.Scripts();
    if (scripts) for (const auto& diagnostic : scripts->Diagnostics())
        std::cerr << diagnostic.callback << ": " << diagnostic.message << '\n';
    check(scripts && scripts->Diagnostics().empty(), "gameplay has no script errors");
    play.End();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
