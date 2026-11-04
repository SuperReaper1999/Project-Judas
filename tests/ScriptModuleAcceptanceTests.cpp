// Public-path proof for a module-local Audio binding correction. The mechanical
// extraction's central dispatcher is kept byte-identical by the qualification.
#include "AssetDatabase.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
int checks = 0;
int failures = 0;

void Check(bool passed, const char* label) {
    ++checks;
    failures += !passed;
    std::printf("%s %s\n", passed ? "PASS" : "FAIL", label);
}

bool HasTrue(const std::string& json, const char* name) {
    return json.find(std::string("\"") + name + "\":true") != std::string::npos;
}
}

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::absolute(fs::path("build/scripting-v2-module-acceptance") /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "Assets");
    const auto source = root / "Assets/local-audio.js";
    std::ofstream(source) << R"JS(export default class {
  constructor({entity}) { this.entity=entity; this.state={calls:0}; }
  fixedUpdate() {
    this.state.calls++;
    if(this.state.calls===1) {
      this.state.known=this.entity.setAudio({volume:0.75,loop:true,pitch:1.25});
    } else if(this.state.calls===2) {
      try { this.entity.setAudio({volume:0.25,loop:false,volumeDb:-8}); }
      catch(error) { this.state.unknown=error instanceof TypeError; }
    } else if(this.state.calls===3) {
      this.entity.setAudio({volume:0.75,loop:true,pitch:1.25});
      try { this.entity.setAudio({volume:-1,pitch:2}); }
      catch(error) { this.state.invalid=error instanceof TypeError; }
    } else if(this.state.calls===4) {
      const entity=this.entity;
      this.state.nested=entity.setAudio({
        get volume() { entity.setCameraEnabled(false); return 0.5; }
      });
    }
  }
})JS";
    AssetDatabase assets;
    assets.Scan(root.string(), (root / "Assets").string());
    AssetRecord script;
    std::string error;
    Check(assets.Track(source.string(), script, error), "register ordinary project script");
    Scene scene;
    auto& owner = scene.CreateObject("Module-local controls");
    owner.audioEmitter = SceneAudioEmitterComponent{};
    owner.audioEmitter->playOnStart = false;
    owner.renderCamera = SceneRenderCameraComponent{};
    owner.scripts.push_back({1, script.id, true, "{}"});
    ResourceManager resources(nullptr, &assets);
    RuntimeWorld world;
    if (!world.Build(scene, &resources, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        Check(false, "build ordinary audio/camera scripted world");
        return 1;
    }
    Check(true, "build ordinary audio/camera scripted world");
    auto capture = [&] {
        const auto state = world.Scripts()->Capture();
        return state.size() == 1 ? state.front().json : std::string{};
    };
    auto unchanged = [&] {
        const auto& audio = world.AudioEmitters().front().settings;
        return audio.volume == .75f && audio.loop && audio.pitch == 1.25f;
    };
    world.FixedScripts(nullptr, 1.f / 60);
    if (!world.Scripts() || world.AudioEmitters().size() != 1 ||
        world.PresentationCameras().size() != 1) {
        Check(false, "script and native component owners remain available");
        return 1;
    }
    Check(HasTrue(capture(), "known") && unchanged(), "valid Audio patch changes the actual emitter");
    world.FixedScripts(nullptr, 1.f / 60);
    Check(HasTrue(capture(), "unknown") && unchanged(),
          "unknown Audio option rejects the whole patch before native mutation");
    world.FixedScripts(nullptr, 1.f / 60);
    Check(HasTrue(capture(), "invalid") && unchanged(),
          "invalid recognized Audio value preserves previously published settings");
    world.FixedScripts(nullptr, 1.f / 60);
    Check(HasTrue(capture(), "nested") && world.AudioEmitters().front().settings.volume == .5f &&
          !world.PresentationCameras().front().settings.enabled,
          "a property accessor can make a nested camera call during Audio validation");
    Check(world.Scripts()->Diagnostics().empty(), "handled validation errors do not fault the owner slot");
    world.Destroy();
    Check(!world.Scripts(), "normal world teardown retires the scripting session");
    std::printf("SUMMARY %d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
