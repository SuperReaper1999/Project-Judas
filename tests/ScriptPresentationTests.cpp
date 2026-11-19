#include "AssetDatabase.h"
#include "InputSystem.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "Simulation.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace {

bool Close(const glm::vec3& a, const glm::vec3& b) {
    return glm::length(a - b) < 2e-5f;
}

bool Close(const glm::quat& a, const glm::quat& b) {
    return std::abs(glm::dot(a, b)) > 1.f - 2e-6f;
}

} // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    if (argc < 2 || argc > 3 ||
        (argc == 3 && std::string_view(argv[2]) != "--authoritative-camera")) {
        std::fputs("Usage: judas_script_presentation_tests <scratch> [--authoritative-camera]\n", stderr);
        return 2;
    }
    const auto root = fs::absolute(argv[1]);
    fs::create_directories(root / "Assets");
    const auto path = root / "Assets" / "follow.js";
    std::string source = R"JS(import {time, world} from 'judas';
export default class {
    constructor({entity}) { this.entity = entity; this.state = {calls: 0}; }
    fixedUpdate() {
        this.entity.character.velocity = {x: 3, y: 0, z: 0};
        this.entity.character.orient({x: 0, y: .70710678, z: 0, w: .70710678});
    }
    update() {
        const a = this.entity.transform.position;
        const b = this.entity.presentedTransform.position;
        if (Math.hypot(a.x-b.x, a.y-b.y, a.z-b.z) > 1e-6)
            throw Error('outside presentation the presented getter must be authoritative');
        this.state.checkedAuthoritativeGetter = true;
    }
    presentationUpdate(dt, alpha) {
        const expectedAlpha = ++this.state.calls === 1 ? .25 : .75;
        if (time.fixed || Math.abs(dt - 1/144) > 1e-6 || alpha !== expectedAlpha)
            throw Error('wrong presentation phase, delta or interpolation fraction');
        const a = this.entity.transform.position;
        const pose = this.entity.presentedTransform;
        if (a.x < .049 || a.x - pose.position.x < .01)
            throw Error('authoritative and presented poses must stay distinct');
        let rejected = false;
        try { this.entity.character.velocity = {x: 0, y: 0, z: 0}; }
        catch { rejected = true; }
        if (!rejected) throw Error('presentation must reject motor velocity intent');
        world.entity('2').transform = pose;
        world.setView(pose, 70);
    }
}
)JS";
    if (argc == 3) {
        const std::string follow = "world.setView(pose, 70)";
        source.replace(source.find(follow), follow.size(),
                       "world.setView(this.entity.transform, 70)");
    }
    if (!(std::ofstream(path) << source)) {
        std::fprintf(stderr, "Could not write fixture %s\n", path.c_str());
        return 1;
    }

    AssetDatabase assets;
    assets.Scan(root.string(), (root / "Assets").string());
    AssetRecord script;
    std::string error;
    if (!assets.Track(path.string(), script, error)) {
        std::fprintf(stderr, "Could not track fixture: %s\n", error.c_str());
        return 1;
    }
    Scene scene;
    auto& character = scene.CreateObject("Following a motor");
    const auto owner = character.id;
    character.characterMotor = CharacterMotorSettings{};
    character.characterMotor->gravityScale = 0;
    character.scripts.push_back({1, script.id, true, "{}"});
    auto& follower = scene.CreateObject("Body-free cosmetic");
    const auto cosmetic = follower.id;
    follower.render = SceneRenderComponent{};

    ResourceManager resources(nullptr, &assets);
    RuntimeWorld world;
    if (!world.Build(scene, &resources, error)) {
        std::fprintf(stderr, "Could not build fixture: %s\n", error.c_str());
        return 1;
    }

    int failures = 0;
    const auto check = [&](bool passed, const char* message) {
        failures += !passed;
        std::printf("%s %s\n", passed ? "PASS" : "FAIL", message);
    };
    InputSystem input;
    const auto authored = scene.Find(owner)->transform;
    input.BeginFixedStep();
    StepAuthoritativeWorld(world, input, 1.f / 60);
    const auto current = world.SimulationTransform(owner, authored);
    const auto velocity = world.RuntimeCharacter(owner)->velocity;
    check(current.position.x > .049f && velocity.x > 2.99f && !Close(authored.rotation, current.rotation),
          "fixed script moves and turns the actual motor");

    for (const float alpha : {.25f, .75f}) {
        world.PresentationScripts(&input, 1.f / 144, alpha);
        const auto expected = world.PresentedTransform(owner, authored, alpha);
        check(Close(expected.position, glm::mix(authored.position, current.position, alpha)) &&
                  Close(expected.rotation, glm::slerp(authored.rotation, current.rotation, alpha)),
              "fixture samples distinct partial steps without advancing simulation");
        check(world.view && Close(world.view->pose.position, expected.position) &&
                  Close(world.view->pose.rotation, expected.rotation),
              "script camera follows the renderer's interpolated pose");
        const auto visual = world.PresentedTransform(cosmetic, {}, alpha);
        check(Close(visual.position, expected.position) && Close(visual.rotation, expected.rotation),
              "body-free cosmetic uses the same pose as the camera");
    }
    world.UpdateScripts(&input, 1.f / 144);
    check(Close(world.SimulationTransform(owner, authored).position, current.position) &&
              Close(world.RuntimeCharacter(owner)->velocity, velocity),
          "presentation preserves authoritative motor motion");

    const auto* scripts = world.Scripts();
    if (scripts) {
        const auto snapshots = scripts->Capture();
        check(snapshots.size() == 1 &&
                  snapshots.front().json.find("\"checkedAuthoritativeGetter\":true") != std::string::npos,
              "ordinary frame update reads the authoritative getter");
        for (const auto& diagnostic : scripts->Diagnostics())
            std::fprintf(stderr, "%s: %s\n", diagnostic.asset.c_str(), diagnostic.message.c_str());
    }
    check(scripts && scripts->Diagnostics().empty(), "script phase and API assertions completed");
    world.Destroy();
    world.PresentationScripts(&input, 1.f / 144, .5f);
    check(!world.Scripts() && !world.view, "destroy removes callbacks and view state");
    return failures ? 1 : 0;
}
