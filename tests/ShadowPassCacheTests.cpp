#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "ScreenshotWriter.h"
#include "WorldPresentation.h"
#include "WorldGraphicsState.h"
#include "ShadowTransforms.h"
#include "render/ShadowPassCache.h"
#include "lighting/ShadowSystem.h"
#include "terrain/TerrainBrush.h"
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
    cache.BeginCasterSnapshot();
    const auto token=cache.CasterToken("caster A");
    Check(token && cache.CasterToken("caster A")==token && cache.CasterToken("caster B")!=token,
          "compact caster tokens still compare exact content");
    Check(!cache.CasterToken(std::string(8*1024*1024+1,'a')),
          "an oversized caster fails closed to a redraw");
    cache.BeginCasterSnapshot();
    cache.BeginCasterSnapshot();
    Check(cache.CasterToken(std::string(8*1024*1024,'z')) && cache.CasterToken("caster A")==0,
          "old caster bytes can be reclaimed without exceeding the live budget");
    cache.BeginCasterSnapshot();
    cache.BeginCasterSnapshot();
    Check(cache.CasterToken("caster A")!=token,"retired token numbers never alias old chart keys");
    ShadowPassCache nextFrame;
    Check(!nextFrame.Matches(key), "a new render sample cannot inherit last frame's moving casters");
}


// A visible emitter is not the same thing as illumination of a receiving surface.
// Compare against an independent no-enclosure reference, not a golden image.
void EmitterContracts(const std::filesystem::path& output) {
    EngineHost host;
    std::string error;
    if (!host.Init("Point-emitter lighting contracts", 320, 240, false, error)) {
        Check(false, error.c_str());
        return;
    }
    auto& renderer = host.GetRenderer();
    Check(renderer.ShadowBudget() == 8,
          "default shadow capacity covers the Workshop's eight concurrent sources");
    renderer.SetShadowBudget(1);
    Scene scene;
    scene.Settings().sunEnabled = false;
    auto& floor = scene.CreateObject("Receiving floor");
    floor.render = SceneRenderComponent{};
    floor.render->halfExtents = {5, .15f, 5};
    floor.transform.position = {0, -.15f, 0};
    auto& bulb = scene.CreateObject("Point source inside a rendered bulb");
    bulb.render = SceneRenderComponent{};
    bulb.render->shape = SceneShape::Sphere;
    bulb.render->radius = .23f;
    bulb.transform.position = {0, 3.7f, 0};
    bulb.light = SceneLightComponent{};
    bulb.light->castsShadow = true;
    bulb.light->intensity = 65;
    bulb.light->range = 13;
    const auto bulbId = bulb.id;
    MaterialDefinition material;
    material.model = MaterialModel::PBR;
    material.roughness = .65f;
    MaterialBinding surface, emission, glass;
    surface.handle = renderer.CreateMaterial(material);
    material.emissive = {2, 1.2f, .35f};
    emission.handle = renderer.CreateMaterial(material);
    material.alpha = MaterialAlpha::Blend;
    material.baseColor.a = .8f;
    glass.handle = renderer.CreateMaterial(material);
    const auto view = glm::lookAt(glm::vec3(0, 3, 8), glm::vec3(0), glm::vec3(0, 1, 0));
    const auto projection = glm::perspective(glm::radians(60.f), 320.f / 240, .1f, 50.f);
    auto frame = [&](bool enclosure, bool enabled, bool singleSided, bool transparent) {
        scene.Find(bulbId)->light->enabled = enabled;
        renderer.ResetStats();
        renderer.SetSceneAppearance(true, 1, {}, 1, {1, 0, 0, 0}, false);
        renderer.BeginFrame(320, 240);
        renderer.SetCamera(view, projection);
        renderer.SetLighting({0, 1, 0}, {0, 0, 0}, {.015f, .02f, .035f});
        auto draw = [&]() {
            renderer.SetMaterialBindings({surface});
            renderer.DrawBox({0, -.15f, 0}, {1, 0, 0, 0}, {5, .15f, 5}, {.65f, .65f, .65f});
            if (enclosure) {
                renderer.SetMaterialBindings(singleSided ? std::vector<MaterialBinding>{transparent ? glass : emission}
                                                        : std::vector<MaterialBinding>{});
                renderer.DrawSphere({0, 3.7f, 0}, .23f, {1, .75f, .35f});
            }
        };
        auto casters = JudasLighting::AuthoredShadowScene(scene, host.Resources());
        casters.volatileContent = true; // Each independent control must actually redraw.
        JudasLighting::PrepareShadows(renderer, scene.Settings(), BuildAuthoredLights(scene), casters, draw);
        draw();
        renderer.EndFrame();
        std::vector<unsigned char> image;
        renderer.CaptureFrame(320, 240, image);
        return image;
    };
    std::filesystem::create_directories(output);
    double means[5]{};
    for (int mode = 0; mode < 5; ++mode) {
        const auto image = frame(mode != 1, mode != 2, mode != 3, mode == 4);
        Check(image.size() == 320 * 240 * 3, "point-emitter control captures the actual framebuffer");
        if (image.empty()) return;
        double energy = 0;
        for (auto channel : image) energy += channel;
        means[mode] = energy / image.size();
        std::printf("EMITTER mode=%d mean=%f\n", mode, means[mode]);
        WriteRgbPng((output / ("emitter-" + std::to_string(mode) + ".png")).string(), 320, 240, image);
    }
    Check(means[1] > means[2] + 20,
          "independent unobstructed point light visibly illuminates the receiving floor");
    Check(means[0] > means[2] + 20 && std::abs(means[0] - means[1]) < 2,
          "outward-facing PBR bulb does not shadow its own point source");
    Check(means[3] < means[1] - 20,
          "double-sided opaque enclosure remains an independent blocking negative control");
    Check(means[4] > means[2] + 20,
          "transmissive bulb material preserves emitted floor illumination");

    renderer.SetShadowBudget(8);
    auto& second = scene.CreateObject("Independent neighbouring source");
    second.transform.position = {-3, 3.7f, -2};
    second.light = SceneLightComponent{};
    second.light->castsShadow = true;
    second.light->color = {35, 25, 15};
    second.light->range = 13;
    const auto both = frame(true, true, true, false);
    const auto remaining = frame(true, false, true, false);
    unsigned brighter = 0, darker = 0;
    for (std::size_t i = 0; i < both.size(); ++i) {
        brighter += int(remaining[i]) > int(both[i]) + 1;
        darker += int(both[i]) > int(remaining[i]) + 1;
    }
    Check(brighter == 0 && darker > 100,
          "destroying a bulb removes light without brightening unrelated surfaces after list compaction");
    Check(renderer.Stats().shadowPasses == 6 && renderer.Stats().dynamicLights == 1,
          "the other point source retains illumination and its six correct shadow faces after destruction");
    WriteRgbPng((output / "two-sources.png").string(), 320, 240, both);
    WriteRgbPng((output / "one-source.png").string(), 320, 240, remaining);
    Check(glGetError() == GL_NO_ERROR, "emitter and multi-light shutoff controls complete without graphics errors");
    renderer.DestroyMaterial(glass.handle);
    renderer.DestroyMaterial(emission.handle);
    renderer.DestroyMaterial(surface.handle);
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

    // Observe the live quality settings through the same world colour and depth
    // passes that consume the resized array. Keep lamp and simulation intact.
    scene.Settings().sunEnabled = true;
    scene.Find(objectId)->render->textureCamera = kInvalidSceneObjectId;
    if (!world.Build(scene, nullptr, error)) {
        Check(false, error.c_str());
        return;
    }
    const auto defaults = renderer.Quality();
    const auto defaultPixels = frame(false);
    const auto defaultStats = renderer.Stats();
    auto quality = defaults;
    quality.softShadows = false;
    Check(renderer.SetGraphicsQuality(quality,error),"select four-tap filtering on the existing shadow maps");
    const auto fastPixels = frame(false);
    Check(fastPixels != defaultPixels && renderer.Stats().shadowBytes == defaultStats.shadowBytes,
          "filter quality changes visible shadow edges without changing depth storage");
    quality.directionalCascades = 1;
    quality.localShadowBudget = 0;
    quality.shadowResolution = 256;
    quality.shadowDistanceScale = .5f;
    Check(renderer.SetGraphicsQuality(quality,error),"rebuild smaller depth storage and invalidate cached charts");
    frame(true);
    const auto low = renderer.Stats();
    Check(defaultStats.shadowPasses == 4 && low.shadowPasses == 1 && low.reusedShadowPasses == 0 &&
          low.dynamicLights == defaultStats.dynamicLights && low.shadowBytes == 256u*256u*4u,
          "one redrawn small cascade and zero local charts retain direct lighting");
    frame(true);
    Check(renderer.Stats().reusedShadowPasses == 1,"the reconstructed depth chart can be reused");
    quality.shadowDistanceScale = 1.5f;
    Check(renderer.SetGraphicsQuality(quality,error),"extend directional shadow coverage");
    frame(true);
    Check(renderer.Stats().shadowPasses == 1 && renderer.Stats().reusedShadowPasses == 0,
          "a range change invalidates the cached light-space projection");
    const auto clock = world.SimulationTimeSeconds();
    Check(renderer.SetGraphicsQuality(defaults,error) && frame(false) == defaultPixels &&
          world.SimulationTimeSeconds() == clock,
          "restoring default quality recovers exact pixels with the same simulation clock");
    world.Destroy();
}
void TerrainCacheContracts() {
    EngineHost host;
    std::string error;
    if (!host.Init("Terrain directional shadow-cache contracts", 160, 128, false, error)) {
        Check(false, error.c_str());
        return;
    }
    Scene scene;
    scene.Settings().sunEnabled=true;
    scene.Settings().sunDirection=glm::normalize(glm::vec3(.15f,1,.08f));
    auto terrainOwner=[&](float x) {
        auto& owner=scene.CreateObject("Runtime heightfield");
        owner.body=SceneBodyComponent{};
        owner.render=SceneRenderComponent{};
        owner.transform.position.x=x;
        return owner.id;
    };
    const auto near=terrainOwner(0), far=terrainOwner(85);
    auto& body=scene.CreateObject("Moving caster");
    body.body=SceneBodyComponent{};
    body.body->motion=SceneBodyMotion::Dynamic;
    body.render=SceneRenderComponent{};
    body.transform.position={80,2,0};
    const auto bodyId=body.id;
    const auto bodyPose=body.transform;
    RuntimeWorld world;
    auto makeTerrain=[&](SceneObjectId owner) {
        return world.LiveTerrain().CreateLandscape(owner,TerrainV2::Create(17,17,.5f),error) &&
               world.LiveTerrain().Wait(owner,error);
    };
    if (!world.Build(scene,&host.Resources(),error) || !makeTerrain(near)) {
        Check(false,error.c_str());
        return;
    }
    const auto projection=glm::perspective(glm::radians(60.f),160.f/128,.1f,150.f);
    auto eye=glm::vec3(0,9,10);
    auto frame=[&](bool reuse) {
        Reuse(reuse);
        auto& renderer=host.GetRenderer();
        renderer.ResetStats();
        const auto view=glm::lookAt(eye,eye+glm::vec3(0,-.6f,-1),glm::vec3(0,1,0));
        RenderWorldFrame(renderer,160,128,world,view,projection,eye,1);
        std::vector<unsigned char> pixels;
        renderer.CaptureFrame(160,128,pixels);
        return pixels;
    };
    const auto reused=[&](unsigned redraws) {
        const auto& stats=host.GetRenderer().Stats();
        return stats.shadowPasses==redraws && stats.reusedShadowPasses==3-redraws;
    };
    frame(true);
    auto single=frame(true);
    Check(reused(0) && !single.empty() && frame(false)==single,
          "one stationary terrain reuses three charts with exact pixels");
    Check(makeTerrain(far),"publish a second terrain in the far cascade");
    frame(true);
    const auto both=frame(true);
    Check(reused(0),"two unchanged landscapes reuse all directional depth");

    TerrainV2::SurfaceBrush sculpt;
    sculpt.kind=TerrainV2::SurfaceBrush::Kind::Raise;
    sculpt.radius=1.8f;
    sculpt.strength=1.f;
    Check(world.LiveTerrain().Surface(far,{85,0,0},sculpt,error) && world.LiveTerrain().Wait(far,error),
          "publish a height edit on the remote landscape");
    const auto edited=frame(true);
    Check(reused(1),"remote terrain edit redraws only its intersecting cascade");
    frame(true);
    Check(reused(0),"terrain page allocation on edit cannot invalidate unrelated depth");
    Check(frame(false)==edited,"height edit cache matches a forced redraw exactly");
    frame(true);

    auto paint=sculpt;
    paint.kind=TerrainV2::SurfaceBrush::Kind::Paint;
    paint.layer=1;
    Check(world.LiveTerrain().Surface(far,{85,0,0},paint,error) && world.LiveTerrain().Wait(far,error),
          "publish a terrain material edit");
    frame(true);
    Check(reused(1),"remote terrain paint preserves the other two depth charts");
    const auto painted=frame(true);
    Check(reused(0) && frame(false)==painted,"settled paint and cached depth match a full redraw");

    auto appearance=world.Settings();
    const auto originalDirection=appearance.sunDirection;
    for(unsigned index=1;index<=3;++index) {
        appearance.sunDirection=glm::normalize(originalDirection+glm::vec3(index*.0005f,0,0));
        Check(world.SetAppearance(appearance),"advance the sun over static terrain");
        const auto rotating=frame(true);
        Check(reused(3) && frame(false)==rotating,
              "a rotating sun over static terrain redraws all exact projections");
    }
    frame(true);
    auto moved=bodyPose;
    moved.position={5,2,0};
    Check(world.SetRuntimeTransform(bodyId,moved),"move a caster from the far into the near cascade");
    const auto entered=frame(true);
    Check(reused(3) && entered!=both && frame(false)==entered,
          "caster entering nested cascades redraws each affected chart without changing pixels");
    frame(true);
    for(unsigned index=1;index<=4;++index) {
        appearance.sunDirection=glm::normalize(originalDirection+glm::vec3(.002f+index*.0004f,0,0));
        Check(world.SetAppearance(appearance),"publish current continuous sunlight direction");
        moved.position.x+=.12f;
        world.SetRuntimeTransform(bodyId,moved);
        const auto rotating=frame(true);
        Check(reused(3) && frame(false)==rotating,
              "continuous sun and caster motion use the exact current projection");
    }
    frame(true);
    frame(true);
    frame(true);
    Check(reused(0),"sun and caster stopped on an exact projection become cacheable");
    eye.x+=1;
    const auto walked=frame(true);
    Check(reused(3) && frame(false)==walked,"camera travel redraws the changed stabilized charts");

    frame(true);
    world.LiveTerrain().Remove(far);
    frame(true);
    Check(reused(1),"retiring remote terrain leaves two exact charts reusable");
    Check(makeTerrain(far),"republish a terrain with the same entity identity");
    frame(true);
    Check(reused(1),"replacement cannot inherit the previous terrain generation");
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    const auto before=frame(false);
    const bool startedOnVulkan=host.GetWindow().UsesVulkan();
    const auto clock=world.SimulationTimeSeconds();
    for(const bool useVulkan:startedOnVulkan?std::vector<bool>{false,true}:std::vector<bool>{false}) {
        WorldGraphicsState state;
        if(!world.CaptureGraphicsState(state,error) ||
           !world.ReleaseWorldGraphics(host.GetRenderer(),error) ||
           !host.Resources().DetachGraphics(error) ||
           !host.RebuildDesktopGraphics("Recreated terrain shadow charts",160,128,false,useVulkan,error) ||
           !world.RestoreGraphicsState(state,host.GraphicsCaps(),error)) {
            Check(false,error.c_str());
            return;
        }
        const auto first=frame(true);
        Check(reused(3),"new graphics context redraws every directional chart");
        const auto second=frame(true);
        Check(reused(0) && first==second && frame(false)==second,
              "recreated shadow charts reuse exact depth on the active backend");
        if(useVulkan==startedOnVulkan)
            Check(before==second,"returning to the original backend restores the exact terrain image");
        Check(clock==world.SimulationTimeSeconds(),"graphics reconstruction leaves the fixed clock untouched");
    }
#endif
    Check(glGetError()==GL_NO_ERROR,"terrain shadow-cache controls finish without graphics errors");
    world.Destroy();
}
} // namespace
int main(int argc, char **argv) {
    Pure();
    if(argc>1&&std::strcmp(argv[1],"--emitter-probe")==0){EmitterContracts(argc>2?argv[2]:".cache/emitter-probe");return failures?1:0;}
    if (argc < 2 || std::strcmp(argv[1], "--pure") != 0) {
        const auto output = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(".cache/shadow-reuse");
        EmitterContracts(output / "emitters");
        Native(output);
        TerrainCacheContracts();
    }
    std::printf("Shadow pass cache: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
