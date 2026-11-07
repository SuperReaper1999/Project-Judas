#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "ScriptSystem.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainBrush.h"
#include "terrain/TerrainPersistence.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
namespace fs = std::filesystem;
unsigned checks = 0;
void Check(bool ok, const std::string &message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
void Write(const fs::path &path, const std::string &bytes) {
    std::ofstream file(path, std::ios::binary);
    if (!file.write(bytes.data(), bytes.size()))
        throw std::runtime_error("fixture write failed");
}
std::string Read(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
struct Snapshot {
    std::string id, digest, metadata;
    TerrainV2::RasterChanges raster;
    explicit Snapshot(const std::string &bytes) {
        Check(bytes.compare(0, 8, TerrainV2::RasterReferenceMagic) == 0,
              "changed raster uses its explicit versioned reference format");
        SaveArchive archive(bytes.substr(8));
        archive(id, digest, metadata, raster);
        archive.Finish();
    }
    std::string Encode() {
        SaveArchive archive;
        archive(id, digest, metadata, raster);
        return std::string(TerrainV2::RasterReferenceMagic) + archive.bytes;
    }
};
} // namespace
int main(int argc, char **argv) {
    try {
        const bool cold = argc == 3 && std::string(argv[1]) == "--read";
        const fs::path root =
            argc == 3
                ? fs::path(argv[2])
                : fs::temp_directory_path() /
                      ("terrain-v2-surface-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::string error;
        if (!cold) {
            Check(!fs::exists(root), "fixture must be fresh");
            fs::create_directories(root / "Assets");
            auto source = TerrainV2::Create(33, 33, .5f);
            std::fill(source.heights.begin(), source.heights.end(), 3);
            std::vector<uint8_t> native;
            Check(TerrainV2::EncodeAsset(source, native, error), "encode native base");
            Write(root / "Assets/ground.judasterrain", {native.begin(), native.end()});
        }
        AssetDatabase assets;
        assets.Scan(root.string(), (root / "Assets").string());
        AssetRecord asset;
        if (cold) {
            const auto *registered = assets.FindByRelativePath("Assets/ground.judasterrain");
            Check(registered && !registered->missing, "cold scan resolves existing native identity");
            asset = *registered;
        } else
            Check(assets.Track((root / "Assets/ground.judasterrain").string(), asset, error),
                  "register ordinary native base: " + error);
        JobSystem jobs(2);
        ResourceManager resources(nullptr, &assets, &jobs);
        Scene scene;
        auto &owner = scene.CreateObject("Editable landscape");
        owner.body = SceneBodyComponent{};
        owner.landscape = SceneLandscapeSettings{asset.id};
        owner.transform.position = {30, -2, 10};
        owner.transform.rotation = glm::angleAxis(.31f, glm::normalize(glm::vec3(1, 0, 2)));
        const auto pose = owner.transform;
        auto point = [&](glm::vec3 p) { return pose.position + pose.rotation * p; };
        RuntimeWorld world;
        world.legacyGameplay = false;
        Check(world.Build(scene, &resources, error), "build native landscape: " + error);
        auto hitHeight = [&](float x, float z) {
            auto hit =
                world.Physics().Raycast(point({x, 8, z}), pose.rotation * glm::vec3(0, -1, 0), 16);
            Check(hit.hit && world.EntityIdOfBody(hit.body) == 1,
                  "real collision resolves terrain");
            return (glm::conjugate(pose.rotation) * (hit.point - pose.position)).y;
        };
        auto wait = [&] {
            Check(world.LiveTerrain().Wait(1, error), "publish complete source: " + error);
        };
        if (cold) {
            Check(world.LiveTerrain().Restore(1, Read(root / "terrain.state"), error),
                  "separate process resolves native base and raster overrides: " + error);
            Check(std::abs(hitHeight(0, 0) - 4) < .001f, "cold sculpt survives");
            Check(hitHeight(3, 0) < 2.2f, "cold excavation survives alongside raster edits");
            std::string bytes;
            Check(world.CaptureRuntimeTerrain(1, bytes, error), "recapture cold native terrain");
            Snapshot state(bytes);
            Check(state.raster.at(16 * 33 + 16).weights == glm::vec4(0, 0, 1, 0),
                  "cold material weights survive exactly");
        } else {
            TerrainV2::SurfaceBrush brush;
            brush.radius = 2;
            brush.strength = 1;
            Check(world.LiveTerrain().Surface(1, point({0, 3, 0}), brush, error),
                  "queue source-local raise from a rotated world point");
            Check(world.LiveTerrain().Status(1)->pendingChunks == 4,
                  "a corner brush rebuilds all four adjacent chunks");
            Check(std::abs(hitHeight(0, 0) - 3) < .001f,
                  "old physical support remains until the complete revision publishes");
            std::string blocked;
            Check(!world.CaptureRuntimeTerrain(1, blocked, error), "pending changes defer saves");
            Check(!world.LiveTerrain().Surface(1, point({0, 3, 0}), brush, error),
                  "busy owner rejects a second native edit");
            wait();
            Check(std::abs(hitHeight(0, 0) - 4) < .001f, "sculpt changes real collision");
            const auto bodies = world.OwnedCollisionBodies(1);
            brush.kind = TerrainV2::SurfaceBrush::Kind::Paint;
            brush.layer = 2;
            Check(world.LiveTerrain().Surface(1, point({0, 3, 0}), brush, error),
                  "queue material-only change");
            Check(world.LiveTerrain().Status(1)->pendingChunks == 0,
                  "painting schedules no collision extraction");
            wait();
            const auto paintedBodies = world.OwnedCollisionBodies(1);
            Check(bodies.size() == paintedBodies.size() &&
                      std::equal(bodies.begin(), bodies.end(), paintedBodies.begin(),
                                 [](auto a, auto b) { return a.id == b.id; }),
                  "painting retains every real collider identity");
            std::string bytes;
            Check(world.CaptureRuntimeTerrain(1, bytes, error), "capture exact sparse overrides");
            Snapshot snapshot(bytes);
            Check(snapshot.raster.size() < 100 && bytes.size() < 8192,
                  "local dabs save bounded changed samples, not the full terrain");
            const auto center = snapshot.raster.at(16 * 33 + 16);
            Check(center.height == 4 && center.weights == glm::vec4(0, 0, 1, 0),
                  "height and paint on the same sample accumulate without duplication");
            snapshot.raster[999999] = center;
            Check(!world.LiveTerrain().Restore(1, snapshot.Encode(), error),
                  "out-of-bounds saved raster index is rejected");
            snapshot.raster.erase(999999);
            snapshot.raster.begin()->second.weights = {-1, 2, 0, 0};
            Check(!world.LiveTerrain().Restore(1, snapshot.Encode(), error),
                  "corrupt material weights are rejected before publication");
            Check(std::abs(hitHeight(0, 0) - 4) < .001f, "failed restores retain old collision");
            auto invalid = brush;
            invalid.radius = .01f;
            Check(!world.LiveTerrain().Surface(1, point({0, 3, 0}), invalid, error),
                  "subsample brush fails explicitly");
            Check(!world.LiveTerrain().Surface(1, {1e30f, 0, 0}, brush, error),
                  "far finite input is rejected before integer indexing");
            Check(world.LiveTerrain().Brush(1, point({3, 3, 0}),
                                            {VolumeBrushKind::Remove, 1.25f, 1}, error),
                  "excavate after surface edits");
            wait();
            Check(hitHeight(3, 0) < 2.2f, "real excavation coexists with sculpted samples");
            Check(world.CaptureRuntimeTerrain(1, bytes, error), "save combined native terrain");
            Write(root / "terrain.state", bytes);
            RuntimeWorld second;
            second.legacyGameplay = false;
            Check(second.Build(scene, &resources, error) &&
                      second.LiveTerrain().Restore(1, bytes, error),
                  "independent world restores combined native terrain");
            std::string recaptured;
            Check(second.CaptureRuntimeTerrain(1, recaptured, error) && recaptured == bytes,
                  "native reference/raster/journal recapture is byte identical");
            // Use the real public wrapper and native binding, including busy and
            // invalid-value behaviour. No central scripting dispatcher edit.
            Write(root / "Assets/surface.js", R"JS(import {world} from 'judas';
export default class {
 constructor(){this.state={};}
 fixedUpdate(){
  const terrain=world.entity('1').terrain;
  let rejected=false;
  try{terrain.surfaceBrush({x:0,y:0,z:0},{kind:'paint',radius:2,layer:2.5});}
  catch(e){rejected=e instanceof TypeError;}
  if(!rejected)throw new Error('invalid layer accepted');
  if(!terrain.surfaceBrush({x:0,y:0,z:0},{kind:'raise',radius:2,strength:1}))throw new Error('raise blocked');
  if(terrain.surfaceBrush({x:0,y:0,z:0},{kind:'lower',radius:2,strength:1}))throw new Error('busy accepted');
  this.state.passed=true;
 }
})JS");
            assets.Scan(root.string(), (root / "Assets").string());
            AssetRecord script;
            Check(assets.Track((root / "Assets/surface.js").string(), script, error),
                  "track public script");
            Scene scripted;
            auto &entry = scripted.CreateObject("Scripted terrain");
            entry.body = SceneBodyComponent{};
            entry.landscape = SceneLandscapeSettings{asset.id};
            entry.scripts.push_back({1, script.id, true, "{}"});
            RuntimeWorld live;
            live.legacyGameplay = false;
            Check(live.Build(scripted, &resources, error),
                  "ordinary scene initializes terrain before script");
            InputSystem input;
            live.UpdateScripts(&input, .016f);
            live.FixedScripts(&input, .016f);
            Check(
                live.Scripts() && live.Scripts()->Diagnostics().empty() &&
                    live.Scripts()->Capture().at(0).json.find("\"passed\":true") !=
                        std::string::npos,
                "real JudasJS validates fields, applies surface changes and reports backpressure");
            Check(live.LiveTerrain().Wait(1, error), "finish scripted sculpt");
            auto actual = live.Physics().Raycast({0, 8, 0}, {0, -1, 0}, 16);
            Check(actual.hit && std::abs(actual.point.y - 4) < .001f,
                  "script changed actual support");
            auto large = TerrainV2::Create(257, 257, .5f);
            std::vector<uint8_t> encoded;
            Check(TerrainV2::EncodeAsset(large, encoded, error), "encode save-budget fixture");
            Write(root / "Assets/large.judasterrain", {encoded.begin(), encoded.end()});
            AssetRecord largeAsset;
            Check(assets.Track((root / "Assets/large.judasterrain").string(), largeAsset, error),
                  "track independent large source");
            Scene limited;
            auto &limitedOwner = limited.CreateObject("Persistent edit budget");
            limitedOwner.body = SceneBodyComponent{};
            limitedOwner.landscape = SceneLandscapeSettings{largeAsset.id};
            RuntimeWorld budgetWorld;
            budgetWorld.legacyGameplay = false;
            Check(budgetWorld.Build(limited, &resources, error), "build actual large source");
            TerrainV2::SurfaceBrush wide;
            wide.radius = 128;
            wide.strength = 1;
            Check(!budgetWorld.LiveTerrain().Surface(1, {0, 0, 0}, wide, error) &&
                      error.find("65536") != std::string::npos,
                  "an edit beyond the persistence budget is rejected before queuing work");
            std::string unchanged;
            Check(!budgetWorld.LiveTerrain().Status(1)->busy &&
                      budgetWorld.CaptureRuntimeTerrain(1, unchanged, error) &&
                      unchanged.compare(0, 8, TerrainV2::ReferenceMagic) == 0,
                  "rejected wide edit retains the original saveable native base");
        }
        world.Destroy();
        resources.Shutdown();
        if (argc == 1)
            fs::remove_all(root);
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
