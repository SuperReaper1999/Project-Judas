#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "ScriptSystem.h"
#include "WorldPersistence.h"
#include "terrain/TerrainSource.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
void Wait(RuntimeWorld &w, uint64_t id) {
    for (unsigned i = 0; i < 10000; ++i) {
        w.AdvanceRuntimeTerrain();
        auto *state = w.LiveTerrain().Status(id);
        if (state && !state->busy) {
            Check(state->ready && state->error.empty(), "terrain publication: " + state->error);
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("terrain timeout");
}
Scene SceneSource() {
    Scene scene;
    auto &o = scene.CreateObject("Terrain V2");
    o.body = SceneBodyComponent{};
    o.body->halfExtents = {8, .01f, 8};
    o.render = SceneRenderComponent{};
    auto &gravity = scene.CreateObject("Gravity");
    gravity.gravity = SceneGravityComponent{};
    return scene;
}
TerrainV2::Source Hillside() {
    auto s = TerrainV2::Create(33, 33, .5f);
    s.bottom = -4;
    s.top = 8;
    for (unsigned z = 0; z < 33; ++z)
        for (unsigned x = 0; x < 33; ++x) {
            float pz = (float(z) - 16) * .5f;
            s.heights[z * 33 + x] = .8f + 3 * std::exp(-pz * pz / 8);
        }
    return s;
}
void IndependentScriptOwners(JobSystem &jobs) {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() /
                      ("judas-terrain-script-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root / "Assets");
    const auto path = root / "Assets/edit.js";
    {
        std::ofstream file(path);
        file << R"JS(import {world} from 'judas';
export default class {
 constructor(){this.state={};}
 fixedUpdate(){
  const a=world.entity('1').terrain,b=world.entity('2').terrain;
  if(!a.brush({x:0,y:0,z:0},{kind:'remove',radius:1}))throw new Error('first owner rejected');
  if(a.brush({x:0,y:0,z:0},{kind:'remove',radius:1}))throw new Error('busy owner accepted');
  if(!b.brush({x:30,y:0,z:0},{kind:'remove',radius:1}))throw new Error('independent owner blocked');
  this.state.passed=true;
 }
})JS";
    }
    AssetDatabase assets;
    assets.Scan(root.string(), (root / "Assets").string());
    AssetRecord record;
    std::string error;
    Check(assets.Track(path.string(), record, error), "track actual terrain script: " + error);
    ResourceManager resources(nullptr, &assets, &jobs);
    Scene scene;
    for (unsigned i = 0; i < 2; ++i) {
        auto &o = scene.CreateObject("Landscape");
        o.body = SceneBodyComponent{};
        o.transform.position.x = i * 30;
        if (!i)
            o.scripts.push_back({1, record.id, true, "{}"});
    }
    RuntimeWorld world;
    world.legacyGameplay = false;
    Check(world.Build(scene, &resources, error), "build script landscapes: " + error);
    for (uint64_t id : {1, 2}) {
        Check(world.LiveTerrain().CreateLandscape(id, TerrainV2::Create(17, 17, .5f), error),
              "create independent owner: " + error);
        Wait(world, id);
    }
    InputSystem input;
    world.UpdateScripts(&input, .016f);
    world.FixedScripts(&input, .016f);
    if (world.Scripts())
        for (const auto &d : world.Scripts()->Diagnostics())
            std::cerr << d.message << '\n';
    Check(world.Scripts() && world.Scripts()->Diagnostics().empty(),
          "terrain bindings have no script faults");
    const auto state = world.Scripts()->Capture();
    Check(state.size() == 1 && state.front().json.find("\"passed\":true") != std::string::npos,
          "actual JudasJS applies per-owner backpressure without globally serializing edits");
    Wait(world, 1);
    Wait(world, 2);
    world.Destroy();
    resources.Shutdown();
    fs::remove_all(root);
}
} // namespace
int main(int argc, char **argv) {
    try {
        AssetDatabase assets;
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        RuntimeWorld w;
        w.legacyGameplay = false;
        std::string error;
        auto scene = SceneSource();
        Check(w.Build(scene, &resources, error, nullptr, nullptr, true),
              "normal world build: " + error);
        if (argc == 3 && std::string(argv[1]) == "--read") {
            std::ifstream f(argv[2], std::ios::binary);
            std::string bytes((std::istreambuf_iterator<char>(f)), {});
            Check(!bytes.empty() && w.LiveTerrain().Restore(1, bytes, error),
                  "genuine cold process source restore: " + error);
            Check(!w.Physics().Raycast({-.3f, 1.6f, -7}, {0, 0, 1}, 14).hit,
                  "cold restored tunnel is physically open");
        } else {
            IndependentScriptOwners(jobs);
            Check(w.LiveTerrain().CreateLandscape(1, Hillside(), error),
                  "create landscape without navigation dependency: " + error);
            Wait(w, 1);
            auto before = *w.LiveTerrain().Status(1);
            auto hit = w.Physics().Raycast({-.3f, 1.6f, -7}, {0, 0, 1}, 14);
            Check(hit.hit && w.EntityIdOfBody(hit.body) == 1,
                  "generated collision resolves stable terrain owner");
            for (float z = -5; z <= 5; z += .75f) {
                Check(w.LiveTerrain().Brush(1, {0, 1.6f, z}, {VolumeBrushKind::Remove, 1.25f, 1},
                                            error),
                      "queue excavation: " + error);
                Wait(w, 1);
            }
            Check(!w.Physics().Raycast({-.3f, 1.6f, -7}, {0, 0, 1}, 14).hit,
                  "tunnel across four chunks is physically open end to end");
            Check(w.Physics().Raycast({-2, 1.6f, -7}, {0, 0, 1}, 14).hit, "undug hill retained");
            auto state = *w.LiveTerrain().Status(1);
            Check(state.volumeChunks == 4, "only the four edited chunks become volume geometry");
            Check(w.SetColliderEnabled(1, false), "disable terrain collider group");
            Check(!w.Physics().Raycast({-2, 1.6f, -7}, {0, 0, 1}, 14).hit,
                  "disabled owner disables every derived chunk");
            Check(w.SetColliderEnabled(1, true), "enable terrain collider group");
            Check(w.Physics().Raycast({-2, 1.6f, -7}, {0, 0, 1}, 14).hit,
                  "reenabled owner restores chunk collision");
            Check(!w.Physics().Raycast({-.3f, 1.6f, -7}, {0, 0, 1}, 14).hit,
                  "reenabling does not resurrect the uncut seed collider");
            w.HideRegion({1});
            Check(!w.Physics().Raycast({-2, 1.6f, -7}, {0, 0, 1}, 14).hit,
                  "private region hides chunk bodies");
            w.PublishRegion({1});
            Check(w.Physics().Raycast({-2, 1.6f, -7}, {0, 0, 1}, 14).hit,
                  "region publication publishes derived support");
            Check(!w.Physics().Raycast({-.3f, 1.6f, -7}, {0, 0, 1}, 14).hit,
                  "region publication preserves excavation");
            auto placement = w.RuntimeDefinition(1)->transform;
            auto moved = placement;
            moved.position = {35, 4, -20};
            moved.rotation = glm::angleAxis(.6f, glm::normalize(glm::vec3(1, 2, 0)));
            Check(w.SetRuntimeTransform(1, moved), "teleport terrain owner and generated support");
            const auto aim = [&](glm::vec3 point, glm::vec3 direction) {
                return w.Physics()
                    .Raycast(moved.position + moved.rotation * point, moved.rotation * direction,
                             14)
                    .hit;
            };
            Check(!aim({-.3f, 1.6f, -7}, {0, 0, 1}), "rotated translated tunnel remains open");
            Check(aim({-2, 1.6f, -7}, {0, 0, 1}), "rotated translated wall retains collision");
            Check(!w.Physics().Raycast({-2, 1.6f, -7}, {0, 0, 1}, 14).hit,
                  "no stale terrain collision remains at previous placement");
            moved.scale = glm::vec3(2);
            Check(!w.SetRuntimeTransform(1, moved), "unsupported live scaling is rejected");
            Check(!w.SetRuntimeMotionType(1, SceneBodyMotion::Dynamic, false, error),
                  "terrain ownership cannot silently become an ordinary dynamic body");
            Check(w.SetRuntimeTransform(1, placement), "restore original terrain placement");
            std::string bytes;
            Check(w.CaptureRuntimeTerrain(1, bytes, error),
                  "capture terrain participant: " + error);
            if (argc == 3 && std::string(argv[1]) == "--write") {
                std::ofstream f(argv[2], std::ios::binary);
                f.write(bytes.data(), bytes.size());
            }
            std::map<std::string, SaveChunk> save;
            Check(WorldPersistence::Capture(w, save, error), "ordinary full save: " + error);
            RuntimeWorld cold;
            Check(cold.Build(scene, &resources, error, nullptr, nullptr, true), "cold world build");
            Check(WorldPersistence::Restore(cold, save, error),
                  "ordinary full save restores terrain and native bodies: " + error);
            Check(!cold.Physics().Raycast({-.3f, 1.6f, -7}, {0, 0, 1}, 14).hit,
                  "full save retains physical tunnel");
            // Walk a native swept capsule through the opening, not just a ray.
            auto sweep = cold.Physics().CapsuleCast({{-.25f, 1.6f, -5}, {1, 0, 0, 0}}, .28f, .45f,
                                                    {0, 0, 1}, 10);
            Check(!sweep.hit,
                  "character-sized swept capsule traverses the real cross-chunk tunnel");
            Check(w.RequestOrigin({512, 0, 0}, error) && w.CommitOrigin(),
                  "live origin shift: " + error);
            Check(!w.Physics().Raycast({-512.3f, 1.6f, -7}, {0, 0, 1}, 14).hit,
                  "excavation survives shifted native chart");
            Check(w.DestroyEntity(1, &error) && !w.HasRuntimeTerrain(1),
                  "owner removal releases generated terrain and jobs");
            Check(w.Physics().AliveBodies().empty(), "no leaked chunk collision bodies");
            std::cout << "METRIC base_triangles=" << before.triangles
                      << " edited_triangles=" << state.triangles << " bytes=" << state.bytes
                      << " final_prepare_ms=" << state.prepareMs
                      << " final_publish_ms=" << state.publishMs << '\n';
        }
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
