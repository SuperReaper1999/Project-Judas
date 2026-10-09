// M72 goes through the real M59 residency coordinator: snapshot, unregister,
// private reconstruction and publication. A copied ordinary lab supplies assets.
#include "EngineHost.h"
#include "Prefab.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "WorldStreaming.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>

namespace {
namespace fs = std::filesystem;
int checks = 0, failures = 0;
void Check(bool ok, const std::string& label) {
    ++checks; failures += !ok; std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str());
}
bool Pump(EngineHost& host, WorldStreaming& stream, const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        host.PumpResources(); stream.Advance(false);
        if (done()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (const auto& r : stream.Regions())
        std::printf("REGION id=%s state=%s retained=%zu visual=%d error=%s\n", r.id.c_str(), r.state.c_str(), r.retained, int(r.visualReady), r.error.c_str());
    return false;
}
int Result() { std::printf("SUMMARY %d checks %d failures\n", checks, failures); return failures ? 1 : 0; }
} // namespace

int main(int argc, char** argv) {
    const fs::path output = argc > 1 ? argv[1] : ".cache/m72/retained-region";
    fs::create_directories(output);
    const auto fixture = fs::absolute(output / "fixture");
    fs::remove_all(fixture);
    fs::copy("projects/render_control_lab", fixture, fs::copy_options::recursive);
    std::string error;
    EngineHost host;
    bool setup = host.Init("M72 retained region", 320, 180, false, error);
    Project project;
    setup = setup && project.Load((fixture / "render_control_lab.judasproj").string(), error);
    if (!setup) { Check(false, "copied ordinary lab/host startup: " + error); return Result(); }
    host.OpenProjectAssets(project.RootDir(), project.AssetsDir());
    Scene authored;
    setup = LoadSceneFromFile(project.StartupScenePath(), authored, error) && authored.Find(40) && authored.Find(40)->render;
    if (!setup) { Check(false, "copied multipart source: " + error); return Result(); }
    Scene region;
    region.Settings().name = "Retained M72 region";
    // Region lighting is intentionally different: its install must preserve
    // the one persistent world's root appearance during both visits.
    region.Settings().sunDirection = glm::vec3(-1, 0, 0); region.Settings().sunIntensity = 9;
    auto object = *authored.Find(40); object.parent = 0; object.scripts.clear();
    region.InsertObject(object);
    setup = SaveSceneToFile(region, project.Resolve("Scenes/retained-region.judas"), error);
    const auto manifestPath = fixture / "retained-region.judasworld";
    std::ofstream(manifestPath) <<
        "JudasWorld 1\nbudget 10 8 1 1048576 4194304 1048576 4194304\n"
        "region \"appearance\" \"Scenes/retained-region.judas\" 0 0 0 0 0 0 1 8 8 8 0 \"snapshot\" 65536 \"\"\n";
    WorldManifest manifest;
    setup = setup && LoadWorldManifest(manifestPath.string(), manifest, error) && ValidateWorldManifest(manifest, project, error);
    Scene bootstrap;
    bootstrap.Settings().sunDirection = glm::vec3(.25f, .625f, 1);
    bootstrap.Settings().sunEnabled = false; bootstrap.Settings().sunIntensity = 2;
    RuntimeWorld world;
    setup = setup && world.Build(bootstrap, &host.Resources(), error);
    if (!setup) { Check(false, "ordinary manifest/root build: " + error); return Result(); }
    const auto rootAppearance = EncodeAppearanceState(world.Settings());
    const std::string cutout = "42f619a708d0f7677faae196b78100d3";
    const std::string grid = "470c6f3632444d0cf62bddca0cd64bfe";
    const std::string shared = "f0e962dcb7d38319d3f6c3334f9ab367";
    const auto mesh = object.render->meshAsset;
    {
        WorldStreaming stream(world, host.Resources(), project, manifest);
        auto request = stream.Request("appearance", false, error);
        const bool active = request && Pump(host, stream, [&] {
            const auto status = stream.Status(request); return status && status->state == "active" && status->visualReady;
        });
        const auto first = stream.Resolve("appearance", 40);
        const auto* parts = host.Resources().TryGetModelParts(mesh);
        Check(active && first && world.RuntimeDefinition(first) && parts && parts->size() >= 2 &&
              host.Resources().RefCount(cutout) == 0 && host.Resources().RefCount(grid) == 0 && host.Resources().RefCount(shared) == 0,
              "ordinary public request installs copied multipart region with independent override resources: " + error);
        if (failures) return Result();
        const auto part = parts->front().part;
        MaterialSlot whole; whole.asset = shared;
        whole.overrides.baseColor = glm::vec4(.25f, .5f, .75f, .375f);
        whole.overrides.alpha = MaterialAlpha::Blend; whole.overrides.normalStrength = .75f;
        whole.overrides.doubleSided = true; whole.overrides.textures[0] = cutout;
        MaterialSlot perPart; perPart.useSource = true;
        perPart.overrides.textures[2] = grid; perPart.overrides.alphaCutoff = .625f;
        perPart.overrides.emissive = glm::vec3(.5f, .25f, .125f); perPart.overrides.uvOffset = glm::vec2(.25f, .5f);
        const bool changed = world.SetRenderVisible(first, true, false) && world.SetRenderVisible(first, false, false) &&
            world.SetModelPartVisible(first, part, false) && world.SetRuntimeMaterial(first, "*", whole) &&
            world.SetRuntimeMaterial(first, part, perPart);
        host.Resources().WaitForAll();
        const auto expected = *world.RuntimeDefinition(first);
        Check(changed && !expected.renderVisible && !expected.render->visible && expected.render->hiddenParts == std::vector<std::string>{part} &&
              expected.render->runtimeMaterials.size() == 2 && host.Resources().RefCount(cutout) == 1 &&
              host.Resources().RefCount(grid) == 1 && host.Resources().RefCount(shared) == 1,
              "runtime visibility/part and full material texture changes acquire one bounded demand each");
        const bool released = stream.Release(request) && stream.Unload("appearance");
        const bool unloaded = released && Pump(host, stream, [&] { return stream.Regions().front().state == "unloaded"; });
        const auto retained = stream.Stats().retainedBytes;
        std::printf("RETAINED bytes=%zu old_id=%llu mesh_refs=%u material_refs=%u cutout_refs=%u grid_refs=%u\n",
                    retained, static_cast<unsigned long long>(first), host.Resources().RefCount(mesh), host.Resources().RefCount(shared),
                    host.Resources().RefCount(cutout), host.Resources().RefCount(grid));
        Check(unloaded && retained > 0 && !world.RuntimeDefinition(first) && !stream.Resolve("appearance", 40) &&
              host.Resources().RefCount(mesh) == 0 && host.Resources().RefCount(shared) == 0 &&
              host.Resources().RefCount(cutout) == 0 && host.Resources().RefCount(grid) == 0,
              "real retained snapshot unload retires old identity and all region/override resource refs");
        request = stream.Request("appearance", false, error);
        const bool revisited = request && Pump(host, stream, [&] {
            const auto status = stream.Status(request); return status && status->state == "active" && status->visualReady;
        });
        const auto returned = stream.Resolve("appearance", 40);
        const auto* restored = world.RuntimeDefinition(returned);
        bool restoredFields = restored && restored->render && !restored->renderVisible && !restored->render->visible &&
            restored->render->hiddenParts == expected.render->hiddenParts && restored->render->runtimeMaterials.size() == 2;
        if (restoredFields) {
            restoredFields = ObjectProperties(*restored).at("render.runtime-materials-v1") == ObjectProperties(expected).at("render.runtime-materials-v1") &&
                restored->render->runtimeMaterials.at("*").overrides.textures[0] == cutout &&
                restored->render->runtimeMaterials.at(part).overrides.textures[2] == grid && restored->render->runtimeMaterials.at(part).useSource;
        }
        std::printf("REVISIT new_id=%llu retained=%zu material_refs=%u cutout_refs=%u grid_refs=%u\n",
                    static_cast<unsigned long long>(returned), stream.Stats().retainedBytes, host.Resources().RefCount(shared),
                    host.Resources().RefCount(cutout), host.Resources().RefCount(grid));
        Check(revisited && returned && returned != first && restoredFields && !world.RenderVisible(returned) &&
              host.Resources().RefCount(shared) == 1 && host.Resources().RefCount(cutout) == 1 && host.Resources().RefCount(grid) == 1,
              "actual revisit reconstructs fresh identity, full retained M72 intent and resource demands");
        Check(EncodeAppearanceState(world.Settings()) == rootAppearance && world.Settings().sunDirection == bootstrap.Settings().sunDirection &&
              !world.Settings().sunEnabled && world.Settings().sunIntensity == 2,
              "region install, retained unload and revisit preserve the persistent root sun/environment");
    }
    world.Destroy(); host.Shutdown();
    return Result();
}
