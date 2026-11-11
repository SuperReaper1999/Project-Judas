#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "ScreenshotWriter.h"
#include "WorldPresentation.h"
#include "ShadowTransforms.h"
#include "render/ShadowPassCache.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool okay, const char *label) {
    ++checks;
    failures += !okay;
    std::printf("%s %s\n", okay ? "PASS" : "FAIL", label);
    std::fflush(stdout);
}
void Reuse(bool enabled) {
#ifdef _WIN32
    _putenv_s("JUDAS_SHADOW_REUSE", enabled ? "1" : "0");
#else
    setenv("JUDAS_SHADOW_REUSE", enabled ? "1" : "0", 1);
#endif
}
void Pure() {
    using namespace JudasRender;
    const auto stable=ComputeStableDirectionalShadowMatrix({0,0,0},{0,1,0},16,48,1024);
    const auto subtexel=ComputeStableDirectionalShadowMatrix({.001f,.001f,.001f},{0,1,0},16,48,1024);
    const auto travelled=ComputeStableDirectionalShadowMatrix({1,0,0},{0,1,0},16,48,1024);
    Check(stable==subtexel,"sub-texel camera motion preserves an identical directional chart");
    Check(stable!=travelled,"meaningful camera travel advances the stabilized directional chart");
    ShadowPassCache allocation;
    auto group=[](std::initializer_list<unsigned> ids){std::vector<DynamicLight> result;for(auto id:ids){DynamicLight light;light.identity=id;light.shadowMapIndex=kLocalShadowBase;result.push_back(light);}return result;};
    auto first=group({1,2}),other=group({3,4});allocation.AssignLocalSlots(first);allocation.AssignLocalSlots(other);
    Check(first[0].shadowMapIndex==6&&first[1].shadowMapIndex==12&&other[0].shadowMapIndex==18&&other[1].shadowMapIndex==24,"distinct views retain independent local-source allocations within the shared pool");
    auto again=group({1,2});allocation.AssignLocalSlots(again);
    Check(again[0].shadowMapIndex==first[0].shadowMapIndex&&again[1].shadowMapIndex==first[1].shadowMapIndex,"returning camera retains its unchanged depth slots after another view");
    auto fill=group({5,6,7,8});allocation.AssignLocalSlots(fill);auto replacement=group({9});allocation.AssignLocalSlots(replacement);
    Check(replacement[0].shadowMapIndex==18,"pool pressure evicts the least-recent inactive source, not a current-view source");
    ShadowPassCache cache;
    ShadowPassKey key;
    key.slot = 1;
    key.source = 29;
    Check(!cache.Matches(key), "fresh render-sample cache has no completed shadow content");
    Check(cache.Remember(key) && cache.Matches(key),
          "completed exact chart/source/options can reuse the same physical slot");
    auto changed = key;
    changed.source++;
    Check(!cache.Matches(changed), "a different shadow source cannot inherit another light's slot");
    changed = key;
    changed.slot = 2;
    Check(!cache.Matches(changed) && cache.Remember(changed) && cache.Matches(key),
          "physical slots have independent latest-content keys");
    changed = key;
    changed.lightViewProjection[0][0] = std::nextafter(1.f, 2.f);
    Check(!cache.Matches(changed), "one-ULP light projection change invalidates exact chart reuse");
    changed = key;

    changed = key;
    changed.source = 55;
    cache.Remember(changed);
    Check(cache.Matches(changed) && !cache.Matches(key),
          "overwriting a physical slot retires its older chart key");
    key.viewDependentLod = true;
    key.viewport = {0, 0, 640, 360};
    cache.Remember(key);
    Check(cache.Matches(key), "view-dependent LOD permits reuse with an exactly identical colour view");
    changed = key;
    changed.cameraView[3][0] = .00001f;
    Check(!cache.Matches(changed),
          "different colour-eye position invalidates view-dependent caster LOD");
    changed = key;
    changed.cameraProjection[1][1] = std::nextafter(1.f, 2.f);
    Check(!cache.Matches(changed), "different colour projection invalidates view-dependent caster LOD");
    changed = key;
    changed.viewport.z++;
    Check(!cache.Matches(changed), "viewport resolution invalidates screen-error caster LOD");
    changed = key;
    changed.viewport.x++;
    Check(!cache.Matches(changed), "viewport origin is part of the exact view-dependent key");
    changed = key;
    changed.viewDependentLod = false;
    Check(!cache.Matches(changed), "LOD-policy changes cannot inherit a differently qualified chart");
    key.viewDependentLod = false;
    cache.Remember(key);
    changed = key;
    changed.cameraView[3][0] = 100;
    changed.cameraProjection[1][1] = 2;
    changed.viewport = {5, 8, 320, 240};
    Check(cache.Matches(changed),
          "view-independent caster geometry can share an identical spotlight chart across eyes");
    changed = key;
    changed.lightViewProjection[1][0] = -0.f;
    Check(!cache.Matches(changed),
          "exact matching conservatively distinguishes signed-zero chart bits");
    changed = key;
    changed.slot = -1;
    Check(!cache.Matches(changed) && !cache.Remember(changed),
          "invalid negative slots cannot index or publish cache storage");
    changed.slot = kShadowMapCount;
    Check(!cache.Remember(changed), "out-of-range slots cannot alias the final physical shadow layer");
    changed = key;
    changed.lightViewProjection[0][0] = std::numeric_limits<float>::quiet_NaN();
    Check(!cache.Matches(changed) && !cache.Remember(changed) && cache.Matches(key),
          "non-finite light charts cannot overwrite valid completed content");
    changed = key;
    changed.viewDependentLod = true;
    changed.viewport = {0, 0, 0, 360};
    Check(!cache.Remember(changed), "view-dependent reuse requires a nonempty actual viewport");
    changed.viewport = {0, 0, 640, 360};
    changed.cameraProjection[0][0] = std::numeric_limits<float>::infinity();
    Check(!cache.Remember(changed), "non-finite colour projections cannot qualify LOD-dependent reuse");
    cache.Clear();
    Check(!cache.Matches(key), "explicit clear retires every physical slot");
    ShadowPassCache nextFrame;
    Check(!nextFrame.Matches(key), "a new render sample cannot inherit last frame's moving casters");
}

void Native(const std::filesystem::path &output) {
    EngineHost host;
    std::string error;
    if (!host.Init("Frame-local shadow reuse", 320, 240, false, error)) {
        Check(false, error.c_str());
        return;
    }
    auto &renderer = host.GetRenderer();
    renderer.SetShadowBudget(1);
    if (!renderer.ShadowsEnabled()) {
        Check(false, "native shadow qualification requires enabled shadow maps");
        return;
    }
    Scene scene;
    scene.Settings().sunEnabled = false;
    scene.Settings().ambientColor = {.08f, .08f, .08f};
    auto &floor = scene.CreateObject("Receiving floor");
    floor.transform.position = {0, -.15f, 0};
    floor.render = SceneRenderComponent{};
    floor.render->halfExtents = {5, .15f, 5};
    floor.render->color = {.65f, .65f, .65f};
    auto &object = scene.CreateObject("Physical caster");
    const auto objectId = object.id;
    object.transform.position = {0, .75f, 0};
    object.render = SceneRenderComponent{};
    object.render->halfExtents = {.6f, .75f, .6f};
    object.render->color = {.6f, .2f, .1f};
    object.body = SceneBodyComponent{};
    object.body->motion = SceneBodyMotion::Dynamic;
    auto &lamp = scene.CreateObject("Shared spotlight");
    lamp.transform.position = {0, 6, 4};
    lamp.transform.rotation = glm::quat_cast(
        glm::inverse(glm::lookAt(lamp.transform.position, glm::vec3(0), glm::vec3(0, 1, 0))));
    lamp.light = SceneLightComponent{};
    lamp.light->kind = SceneLightKind::Spot;
    lamp.light->range = 30;
    lamp.light->color = {30, 30, 30};
    lamp.light->innerConeDegrees = 25;
    lamp.light->outerConeDegrees = 42;
    lamp.light->castsShadow = true;
    RuntimeWorld world;

    if (!world.Build(scene, nullptr, error)) {
        Check(false, error.c_str());
        return;
    }
    const auto views = [&]() {
        auto &players = world.Players();
        const auto first = players.Create("Left", InputMap::Defaults(), error)->id,
                   second = players.Create("Right", InputMap::Defaults(), error)->id;
        players.SetViewport(first, {0, 0, .5f, 1}, error);
        players.SetViewport(second, {.5f, 0, .5f, 1}, error);
        for (const auto id : {first, second}) {
            SceneTransform pose;
            pose.position = {id == first ? 5.f : -5.f, 4, 8};
            pose.rotation = glm::quat_cast(
                glm::inverse(glm::lookAt(pose.position, glm::vec3(0), glm::vec3(0, 1, 0))));
            players.SetView(id, pose, 60, .1f, 50, error);
        }
    };
    views();
    const auto view = glm::lookAt(glm::vec3(0, 4, 8), glm::vec3(0), glm::vec3(0, 1, 0)),
               projection = glm::perspective(glm::radians(60.f), 320.f / 240, .1f, 50.f);
    const auto frame = [&](bool reuse) {
        Reuse(reuse);
        renderer.ResetStats();
        RenderWorldFrame(renderer, 320, 240, world, view, projection, {0, 0, 0}, 1);
        std::vector<unsigned char> pixels;
        renderer.CaptureFrame(320, 240, pixels);
        return pixels;
    };
    EntityPhysicalState before, after;
    Check(world.GetEntityState(objectId, before), "native fixture owns a real physical shadow caster");
    const auto baseline = frame(false);
    const auto originalStats = renderer.Stats();
    Check(originalStats.shadowPasses == 2 && originalStats.reusedShadowPasses == 0,
          "negative control redraws the shared spotlight once per independent view");
    const auto reused = frame(true);
    const auto reusedStats = renderer.Stats();
    Check(reusedStats.shadowPasses == 1 && reusedStats.reusedShadowPasses == 1,
          "two real world views reuse exactly one identical completed spotlight chart");
    Check(!baseline.empty() && baseline == reused,
          "frame-local shadow reuse preserves exact native multi-view pixels");
    world.GetEntityState(objectId, after);
    Check(before.position == after.position && before.linearVelocity == after.linearVelocity &&
              before.rotation == after.rotation,
          "view/shadow reuse never advances or changes the physical sample");
    frame(true);
    Check(renderer.Stats().shadowPasses==0&&renderer.Stats().reusedShadowPasses==2,
          "unchanged exact casters persist across presented frames without a redraw");
    auto moved=scene.Find(objectId)->transform;moved.position.x+=1.5f;
    Check(world.SetRuntimeTransform(objectId,moved),"move physical caster through the current runtime contract");
    const auto movingFrame=frame(true);
    Check(renderer.Stats().shadowPasses==1&&renderer.Stats().reusedShadowPasses==1&&movingFrame!=baseline,
          "moving character/body invalidates its affected shadow in the same presented frame");
    world.SetRuntimeTransform(objectId,scene.Find(objectId)->transform);
    frame(true);
    auto lightSettings=*scene.Objects().back().light;
    lightSettings.intensity=2;
    Check(world.SetLight(scene.Objects().back().id,lightSettings),"change light radiance through authoritative state");
    frame(true);
    Check(renderer.Stats().shadowPasses==0,"radiance changes reuse depth while changing the consuming light immediately");
    lightSettings.intensity=1;world.SetLight(scene.Objects().back().id,lightSettings);
    lightSettings.enabled=false;world.SetLight(scene.Objects().back().id,lightSettings);
    frame(true);
    Check(renderer.Stats().shadowPasses==0&&renderer.Stats().dynamicLights==0,"destroyed/disabled illumination disappears immediately without a stale chart source");
    lightSettings.enabled=true;world.SetLight(scene.Objects().back().id,lightSettings);
    frame(true);
    lightSettings.kind=SceneLightKind::Point;world.SetLight(scene.Objects().back().id,lightSettings);
    const auto pointShadowed=frame(true);
    Check(renderer.Stats().shadowPasses==6&&renderer.Stats().reusedShadowPasses==6,"point light renders all six occlusion directions and shares them across views");
    renderer.SetShadowBudget(0);const auto pointUnshadowed=frame(true);
    Check(pointShadowed!=pointUnshadowed,"point shadow-off negative control changes real rendered pixels");
    renderer.SetShadowBudget(1);lightSettings.kind=SceneLightKind::Spot;world.SetLight(scene.Objects().back().id,lightSettings);frame(true);
    renderer.SetShadowBudget(0);
    const auto unshadowed = frame(true);
    unsigned changed = 0;
    for (std::size_t i = 0; i < baseline.size(); ++i)
        changed += baseline[i] != unshadowed[i];
    Check(
        renderer.Stats().shadowPasses == 0 && changed > 24,
        "independent shadow-off control changes actual pixels, excluding a blank/no-shadow false pass");
    renderer.SetShadowBudget(1);
    MeshData lod;
    lod.vertices = {
        {{0, 0, 0}, {0, 0, 1}, {0, 0}}, {{1, 0, 0}, {0, 0, 1}, {1, 0}}, {{0, 1, 0}, {0, 0, 1}, {0, 1}}};
    lod.indices = {0, 1, 2};
    lod.lodIndices = {0, 1, 2};
    lod.lods = {{0, 0, 3, {}}, {1, 3, 3, {}}};
    renderer.SetMeshLodMultiplier(1);
    auto lodMesh = renderer.CreateMesh(lod);
    Check(lodMesh.IsValid() && renderer.HasViewDependentMeshLods(),
          "native LOD fixture activates the renderer's actual view-dependent-mesh predicate");
    renderer.SetMeshLodMultiplier(0);
    Check(renderer.HasViewDependentMeshLods(),
          "the previous object's disabled LOD cannot hide other view-dependent casters");
    const auto guarded = frame(true);
    Check(
        renderer.Stats().shadowPasses == 2 && renderer.Stats().reusedShadowPasses == 0 &&
            guarded == baseline,
        "different eyes retain independent shadow draws when caster LOD can depend on the colour view");
    renderer.DestroyMesh(lodMesh);
    const auto secondSample = frame(true);
    Check(renderer.Stats().shadowPasses == 1 && renderer.Stats().reusedShadowPasses == 1 &&
              secondSample == baseline,
          "the next render sample performs a fresh real shadow pass before reusing within that sample");
    world.Destroy();
    auto &camera = scene.CreateObject("Dormant camera texture source");
    camera.renderCamera = SceneRenderCameraComponent{};
    camera.renderCamera->enabled = false;
    scene.Find(objectId)->render->textureCamera = camera.id;
    if (!world.Build(scene, nullptr, error)) {
        Check(false, error.c_str());
        return;
    }
    Check(true, "camera-backed-caster guard fixture rebuilds through the normal world path");
    views();
    frame(true);
    Check(renderer.Stats().shadowPasses == 2 && renderer.Stats().reusedShadowPasses == 0,
          "camera-backed caster textures conservatively disable frame-local content reuse");
    if (!output.empty()) {
        std::filesystem::create_directories(output);
        WriteRgbPng((output / "baseline.png").string(), 320, 240, baseline);
        WriteRgbPng((output / "reused.png").string(), 320, 240, reused);
        WriteRgbPng((output / "unshadowed.png").string(), 320, 240, unshadowed);
    }
    world.Destroy();
}
} // namespace
int main(int argc, char **argv) {
    Pure();
    if (argc < 2 || std::strcmp(argv[1], "--pure") != 0)
        Native(argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path{});
    std::printf("Shadow pass cache: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
