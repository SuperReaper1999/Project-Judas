// Exercise native binding contracts through ordinary registered scripts.
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
void CheckUI(const std::filesystem::path& root, AssetDatabase& assets, ResourceManager& resources) {
    const auto source = root / "Assets" / "map-layout.js";
    std::ofstream(source) << R"JS(import {ui, WorldMap} from 'judas';
export default class {
    constructor() { this.state = {phase: 0}; }
    fixedUpdate() {
        const phase = ++this.state.phase;
        const doc = ui.get("map");
        const image = doc.get("image");
        const marker = doc.get("marker");
        const input = doc.get("entry");
        if (phase === 1) {
            this.map = new WorldMap(image, {min:{x:-5,z:-5},max:{x:5,z:5}});
            this.map.placeMarker(marker,{x:0,z:0});
            this.saved = doc.captureState();
            input.selection = {anchor:0,caret:0};
        } else if (phase === 2) {
            for (let i=0; i<8; ++i) {
                this.map.setView({center:{x:0,z:0},width:10,height:10});
                this.map.placeMarker(marker,{x:0,z:0});
                image.setLayout(image.layout);
                marker.visible = marker.visible;
                doc.visible = doc.visible;
                doc.referenceSize = doc.referenceSize;
                doc.scale = doc.scale;
            }
            input.text = input.text;
            if (input.selection.caret !== 3) throw Error("same editable text must still reset caret");
            const invalid = [
                () => {marker.visible = 1;},
                () => marker.setLayout({unknown:{x:0,y:0}}),
                () => marker.setLayout({size:{x:-1,y:0}}),
                () => marker.setLayout({anchorMin:{x:.9,y:.5},anchorMax:{x:.1,y:.5}}),
                () => marker.setLayout({offset:{x:Infinity,y:0}}),
                () => this.map.setView({width:0})];
            for (const assign of invalid) {
                let rejected = false;
                try {assign();} catch (e) {rejected = e instanceof TypeError;}
                if (!rejected) throw Error("invalid UI mutation was accepted");
            }
        } else if (phase === 3) {
            this.map.setView({heading:Math.PI/2,circular:true});
            const p = this.map.worldToMap({x:0,z:2.5});
            const back = this.map.mapToWorld(p);
            if (!p.visible || Math.abs(p.x-.75)>1e-6 || Math.abs(back.z-2.5)>1e-6)
                throw Error("map coordinate round trip");
        } else if (phase === 4) {
            this.map.placeMarker(marker,{x:0,z:2.5});
            image.setLayout({offset:{x:100,y:70}});
        } else if (phase === 5) {
            this.map.placeMarker(marker,{x:5,z:5});
            marker.enabled = false;
        } else if (phase === 6) {
            doc.restoreState(this.saved);
            this.map.placeMarker(marker,{x:0,z:2.5});
        } else if (phase === 7) {
            doc.unload();
            let stale = false;
            try { this.map.placeMarker(marker,{x:0,z:2.5}); }
            catch(e) { stale = e instanceof ReferenceError; }
            if (!stale) throw Error("stale marker was accepted");
        }
    }
})JS";
    AssetRecord script;
    std::string error;
    if (!assets.Track(source.string(), script, error)) {
        Check(false, "register map layout script");
        return;
    }
    Scene scene;
    scene.CreateObject("Map UI").scripts.push_back({1,script.id,true,"{}"});
    RuntimeWorld world;
    if (!world.Build(scene,&resources,error)) {
        Check(false,"build map layout world");
        return;
    }
    auto& ui=world.UI();
    UIDocument doc;
    doc.elements.resize(4);
    auto& canvas=doc.elements[0];
    canvas.id="canvas";
    canvas.kind=UIKind::Canvas;
    auto& image=doc.elements[1];
    image.id="image";
    image.parent=canvas.id;
    image.kind=UIKind::Image;
    image.size={200,160};
    image.clip=true;
    auto& marker=doc.elements[2];
    marker.id="marker";
    marker.parent=image.id;
    marker.size={10,10};
    auto& entry=doc.elements[3];
    entry.id="entry";
    entry.parent=canvas.id;
    entry.kind=UIKind::TextInput;
    entry.text="abc";
    const auto handle=ui.Add(doc,"map",0,error);
    if (!handle) {
        Check(false,"create map layout document");
        return;
    }
    auto step=[&] {
        world.FixedScripts(nullptr,1.f/60);
        ui.Layout(1280,720);
    };
    step();
    const auto initial=*ui.LayoutOf(handle,"marker");
    const auto initialPasses=ui.Stats().layoutPasses;
    Check(initial.visible && initial.rect.position==glm::vec2(95,75),"initial marker uses centred anchors");
    step();
    Check(ui.Stats().layoutPasses==initialPasses &&
          ui.LayoutOf(handle,"marker")->rect.position==initial.rect.position,
          "identical writes, reads and rejected patches leave layout cached");
    step();
    const auto* mapImage=static_cast<const RuntimeUI&>(ui).Element(handle,"image");
    Check(ui.Stats().layoutPasses==initialPasses && mapImage && mapImage->mapSampling &&
          mapImage->mapSampling->circular,
          "changed UV sampling leaves geometry cached");
    step();
    const auto moved=*ui.LayoutOf(handle,"marker");
    Check(ui.Stats().layoutPasses==initialPasses+1 && moved.rect.position==glm::vec2(245,145) &&
          moved.clip.position==glm::vec2(100,70) && moved.clip.size==glm::vec2(200,160),
          "moving the marker and its parent updates child geometry and clip");
    ui.Layout(640,360);
    Check(ui.LayoutOf(handle,"marker")->rect.position==moved.rect.position*.5f,
          "resize recomputes the marker position");
    step();
    Check(!ui.LayoutOf(handle,"marker")->visible && !ui.LayoutOf(handle,"marker")->enabled,
          "circular clipping hides an outlying marker and enabled prevents interaction");
    step();
    Check(ui.LayoutOf(handle,"marker")->visible && ui.LayoutOf(handle,"marker")->enabled &&
          ui.LayoutOf(handle,"marker")->rect.position==moved.rect.position,
          "restored visibility and marker placement reach the live document");
    step();
    Check(!static_cast<const RuntimeUI&>(ui).Document(handle) && world.Scripts()->Diagnostics().empty(),
          "unload keeps stale-handle errors and script validation intact");
    for(const auto& diagnostic:world.Scripts()->Diagnostics())
        std::fprintf(stderr,"%s: %s\n",diagnostic.asset.c_str(),diagnostic.message.c_str());
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
    CheckUI(root,assets,resources);
    std::printf("SUMMARY %d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
