#include "AssetCook.h"
#include "EngineHost.h"
#include "ModelLoader.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "ScreenshotWriter.h"
#include "WorldPersistence.h"
#include "WorldPresentation.h"
#include "ScriptSystem.h"
#include "WorldGraphicsState.h"
#include "terrain/TerrainSource.h"
#include "vulkan/VulkanBackend.h"
#include "fixtures/M93AssetFixtures.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
static int checks = 0, failures = 0;
static void Check(bool yes, const char *message) {
  ++checks;
  failures += !yes;
  printf("%s %s\n", yes ? "PASS" : "FAIL", message);
}
static void CheckTerrainStartup(EngineHost& host, const std::filesystem::path& root) {
  auto& assets = host.Resources();
  assets.ConfigureVisualStreaming(0);
  assets.SetBudgetBytes(1);
  assets.SetBlockingMode(false);
  std::string error;
  AssetRecord layer, normal;
  auto track = [&](const char* file, const glm::u8vec3& color, AssetRecord& asset) {
    const auto path = root / "Assets" / file;
    std::vector<unsigned char> rgb;
    for (int i = 0; i < 16; ++i)
      rgb.insert(rgb.end(), {color.r, color.g, color.b});
    return WriteRgbPng(path.string(), 4, 4, rgb) &&
           host.Assets().Track(path.string(), asset, error);
  };
  if (!track("terrain-layer.png", {160, 70, 20}, layer) ||
      !track("terrain-normal.png", {128, 128, 255}, normal)) {
    Check(false, "register terrain layer and detail normal");
    return;
  }
  Scene scene;
  auto& terrain = scene.CreateObject("Terrain under startup cache pressure");
  const auto id = terrain.id;
  terrain.body.emplace();
  terrain.render.emplace();
  RuntimeWorld world;
  auto source = TerrainV2::Create(17, 17, .5f);
  source.layerTextures[0] = source.layerTextures[1] = layer.id;
  source.materialDetail.normalTexture = normal.id;
  if (!world.Build(scene, &assets, error) ||
      !world.LiveTerrain().CreateLandscape(id, std::move(source), error) ||
      !world.LiveTerrain().Wait(id, error)) {
    Check(false, error.c_str());
    return;
  }
  auto& renderer = host.GetRenderer();
  SceneTransform view;
  view.position = {0, 5, 6};
  view.rotation = glm::quatLookAt(glm::normalize(-view.position), glm::vec3(0, 1, 0));
  Check(world.SetRuntimeView(view, 60), "startup uses a camera looking at the terrain");
  const auto before = assets.Stats().misses;
  unsigned stable = 0;
  bool ok = true;
  for (unsigned pass = 0; pass < 180 && stable < 2; ++pass) {
    host.PumpResources();
    bool ready = false;
    if (!world.PrepareStartup(renderer, 640, 360, ready, error)) {
      ok = false;
      break;
    }
    stable = ready ? stable + 1 : 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Check(ok && stable == 2, "async terrain startup reaches two stable passes below the working set budget");
  Check(assets.RefCount(layer.id) == 1 && assets.RefCount(normal.id) == 1 &&
        assets.TryGetTexture(layer.id).IsValid() && assets.TryGetTexture(normal.id).IsValid(),
        "duplicate layer slots and the detail normal hold one reference per texture owner");
  bool ready = false;
  host.PumpResources();
  Check(world.PrepareStartup(renderer, 640, 360, ready, error) && ready &&
        assets.Stats().misses - before == 2 && assets.Stats().overBudgetBytes > 0,
        "subsequent prep keeps both textures without more loads under cache pressure");
  std::vector<unsigned char> visible, hidden;
  renderer.CaptureFrame(640, 360, visible);
  world.SetRenderVisible(id, true, false);
  Check(world.PrepareStartup(renderer, 640, 360, ready, error) && ready,
        "hidden terrain no longer participates in the startup draw");
  renderer.CaptureFrame(640, 360, hidden);
  Check(!visible.empty() && visible.size() == hidden.size() && visible != hidden,
        "prepared terrain produced visible pixels");
  host.PumpResources();
  Check(assets.RefCount(layer.id) == 0 && assets.RefCount(normal.id) == 0 &&
        assets.StateOf(layer.id) == ResourceState::Unloaded &&
        assets.StateOf(normal.id) == ResourceState::Unloaded,
        "hidden terrain releases residency and both textures can be evicted");
  world.SetRenderVisible(id, true, true);
  world.PrepareRuntimeTerrain({});
  Check(assets.RefCount(layer.id) == 1 && assets.RefCount(normal.id) == 1,
        "showing terrain again restores exactly one residency reference per texture");
  world.Destroy();
  Check(assets.RefCount(layer.id) == 0 && assets.RefCount(normal.id) == 0,
        "terrain teardown releases both material references");
}

int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  auto root =
      std::filesystem::absolute(argc > 1 ? argv[1] : ".cache/m93/residency");
  std::filesystem::create_directories(root / "Assets");
  std::string error;
  AssetCookSettings settings;
  const auto column = root / "SourceArt/column.obj";
  Check(M93AssetFixtures::WriteColumn(column, error),
        "original detailed source fixture is generated in this test's output tree");
  if (failures) {
    puts(error.c_str());
    return 1;
  }
  for (int i = 0; i < 3; ++i)
    Check(CookAssetFile(
              column.string(),
              (root / ("Assets/form" + std::to_string(i) + ".judasmesh"))
                  .string(),
              settings, error),
          "independent fixture cook succeeds");
  EngineHost host;
  Check(host.Init("M93 multi-view residency", 640, 360, false, error),
        "native host initializes");
  if (failures) {
    puts(error.c_str());
    return 1;
  }
  SDL_GL_SetSwapInterval(0);
  host.OpenProjectAssets(root.string(), (root / "Assets").string());
  auto &assets = host.Resources();
  assets.ConfigureVisualStreaming(1);
  assets.SetBlockingMode(true);
  std::vector<AssetRecord> models(3);
  for (int i = 0; i < 3; ++i) {
    auto relative = "Assets/form" + std::to_string(i) + ".judasmesh";
    if (auto *old = host.Assets().FindByRelativePath(relative))
      models[i] = *old;
    else
      Check(host.Assets().Track((root / relative).string(), models[i], error),
            "ordinary cooked asset registration");
  }
  Scene scene;
  scene.Settings().sunEnabled = false;
  scene.Settings().ambientColor = {1, 1, 1};
  for (int i = 0; i < 3; ++i) {
    auto &o = scene.CreateObject("Independent geometry");
    o.render = SceneRenderComponent{};
    o.render->shape = SceneShape::Mesh;
    o.render->meshAsset = models[i].id;
    o.transform.position = {float(i == 0 ? 0 : i == 1 ? 180 : -180), 0, 0};
  }
  auto &c = scene.CreateObject("Long-cadence camera");
  auto cameraId = c.id;
  c.transform.position = {180, 8, 20};
  c.transform.rotation = glm::quatLookAt(glm::normalize(glm::vec3(0, -6, -20)),
                                         glm::vec3(0, 1, 0));
  c.renderCamera = SceneRenderCameraComponent{};
  c.renderCamera->width = 128;
  c.renderCamera->height = 96;
  c.renderCamera->updateEveryFrames = 1000;
  c.renderCamera->farPlane = 80;
  RuntimeWorld world;

  Check(world.Build(scene, &assets, error),
        "ordinary optional residency world builds");
  Check(assets.RefCount(models[0].id) == 0 &&
            assets.StateOf(models[0].id) == ResourceState::Unloaded,
        "optional pure visual ownership does not eagerly pin or load geometry");
  auto view = glm::lookAt(glm::vec3(0, 8, 20), glm::vec3(0, 2, 0),
                          glm::vec3(0, 1, 0)),
       projection =
           glm::perspective(glm::radians(65.f), 640.f / 360, .1f, 80.f);
  auto &renderer = host.GetRenderer();
  auto frame = [&] {
    assets.AdvanceVisualFrame();
    assets.Pump();
    RenderWorldFrame(renderer, 640, 360, world, view, projection,
                     {0, 0, 0}, 1);
  };
  frame();
  Check(assets.StateOf(models[0].id) == ResourceState::Ready &&
            assets.StateOf(models[1].id) == ResourceState::Ready &&
            assets.StateOf(models[2].id) == ResourceState::Unloaded,
        "main and secondary viewpoints demand their distinct assets; unseen "
        "negative control stays unloaded");
  assets.SetBudgetBytes(0);
  for (int i = 0; i < 7; ++i)
    frame();
  Check(
      assets.StateOf(models[0].id) == ResourceState::Ready &&
          assets.StateOf(models[1].id) == ResourceState::Ready,
      "cached secondary view retains residency between sparse render updates");
  Check(assets.Stats().overBudgetBytes > 0,
        "visible working-set overcommit is observable rather than silently "
        "discarded");
  for (auto &camera : world.PresentationCameras())
    if (camera.id == cameraId)
      camera.settings.enabled = false;
  for (int i = 0; i < 7; ++i)
    frame();
  Check(assets.StateOf(models[1].id) == ResourceState::Unloaded &&
            assets.StateOf(models[0].id) == ResourceState::Ready,
        "disabled secondary view releases its demand and allows eviction while "
        "main stays intact");
  auto &players = world.Players();
  auto *first = players.Create("Main", InputMap{}, error);
  auto *second = players.Create("Independent", InputMap{}, error);
  SceneTransform firstPose, secondPose;
  firstPose.position = {0, 8, 20};
  secondPose.position = {-180, 8, 20};
  firstPose.rotation = secondPose.rotation = glm::quatLookAt(
      glm::normalize(glm::vec3(0, -6, -20)), glm::vec3(0, 1, 0));
  players.SetViewport(first->id, {0, 0, .5f, 1}, error);
  players.SetViewport(second->id, {.5f, 0, .5f, 1}, error);
  players.SetView(first->id, firstPose, 65, .1f, 80, error);
  players.SetView(second->id, secondPose, 65, .1f, 80, error);
  for (int i = 0; i < 7; ++i)
    frame();
  Check(assets.StateOf(models[2].id) == ResourceState::Ready &&
            assets.StateOf(models[0].id) == ResourceState::Ready,
        "independent split viewpoints retain distinct actual resources");
  second->renderMask = 0;
  for (int i = 0; i < 7; ++i)
    frame();
  Check(assets.StateOf(models[2].id) == ResourceState::Unloaded &&
            assets.StateOf(models[0].id) == ResourceState::Ready,
        "masked split view releases demand without disturbing the other view");
  auto firstId = first->id, secondId = second->id;
  players.Remove(firstId);
  players.Remove(secondId);
  std::map<std::string, SaveChunk> chunks;
  Scene saved;
  Check(WorldPersistence::Capture(world, chunks, error) &&
            WorldPersistence::SceneFrom(chunks, saved, error) &&
            saved.Find(1)->render->meshAsset == models[0].id,
        "durable world keeps asset identity rather than GPU slot/generation");
  world.Destroy();
  assets.ReleaseAll();
  RuntimeWorld restored;

  Check(restored.Build(saved, &assets, error),
        "restored world reconstructs ordinary visual owners");
  assets.AdvanceVisualFrame();
  RenderWorldFrame(renderer, 640, 360, restored, view, projection,
                   {0, 0, 0}, 1);
  Check(
      assets.StateOf(models[0].id) == ResourceState::Ready,
      "restored physical/presentation world reloads actual required geometry");
  restored.Destroy();
  assets.ReleaseAll();
  assets.ConfigureVisualStreaming(0);
  RuntimeWorld legacy;
  const auto probe = root / "Assets/continuity.js";
  std::ofstream(probe) << R"JS(export default class {
    constructor() { this.state = {ticks: 0}; }
    fixedUpdate() { ++this.state.ticks; }
  })JS";
  AssetRecord probeRecord;
  Check(host.Assets().Track(probe.string(),probeRecord,error), "world continuity probe is a tracked script asset");
  scene.Objects()[0].scripts.push_back({1,probeRecord.id,true,"{}"});
  const bool hasGpuPool = host.GraphicsCaps().modernTier;
  if (hasGpuPool) {
    auto& particles = scene.Objects()[0].particleEmitter.emplace();
    particles.backend = ParticleBackend::Gpu;
    particles.maxParticles = 16;
    particles.burst = 4;
    particles.rate = 0;
    particles.loop = false;
    particles.collision = false;
  }

  Check(legacy.Build(scene, &assets, error) &&
            assets.RefCount(models[0].id) > 0 &&
            assets.StateOf(models[2].id) == ResourceState::Ready,
        "default projects retain prior eager/pinned visual behaviour");
  legacy.FixedScripts(nullptr,1.f/60);
  auto* const scripts = legacy.Scripts();
  if (!scripts || scripts->Capture().empty()) {
    Check(false,"asset-backed JudasJS continuity fixture started");
    return 1;
  }
  const auto hudImage = root / "Assets/continuity.png";
  AssetRecord hudAsset;
  if (!M93AssetFixtures::WriteTile1(hudImage,error) ||
      !host.Assets().Track(hudImage.string(),hudAsset,error)) {
    Check(false,error.c_str());
    return 1;
  }
  UIDocument hud;
  UIElement canvas, image, textElement;
  canvas.id = "canvas";
  canvas.kind = UIKind::Canvas;
  image.id = "hud-image";
  image.parent = "canvas";
  image.kind = UIKind::Image;
  image.texture = hudAsset.id;
  textElement.id = "hud-text";
  textElement.parent = "canvas";
  textElement.kind = UIKind::Text;
  textElement.offset = {0,42};
  textElement.text = "Retained HUD text";
  hud.elements = {canvas,image,textElement};
  auto& ui = legacy.UI();
  const auto hudHandle = ui.Add(hud,"continuity-hud",0,error);
  bool uiReady = false;
  TextureData hudPixels;
  if (!hudHandle || !ui.PrepareStartup(640,360,uiReady,error) || !uiReady ||
      !host.GetRenderer().ReadTextureForDiagnostics(assets.TryGetTexture(hudAsset.id),hudPixels)) {
    Check(false,"HUD and tracked image are resident before graphics recreation");
    return 1;
  }
  const auto hudState = ui.CaptureState(hudHandle);
  legacy.UpdateVisualParticles(1.f/60);
  if (hasGpuPool && (legacy.VisualEmitters().empty() || !legacy.VisualEmitters()[0].gpu)) {
    Check(false,"expected a resident GPU pool in the live asset-backed world");
    return 1;
  }
  WorldGraphicsState checkpoint;
  auto detach = [&] {
    return legacy.CaptureGraphicsState(checkpoint,error) &&
           legacy.ReleaseWorldGraphics(host.GetRenderer(),error) && assets.DetachGraphics(error);
  };
  auto restore = [&] {
    assets.WaitForAll();
    bool ready = false;
    return assets.GraphicsRestorationReady(ready,error) && ready &&
           legacy.RestoreGraphicsState(checkpoint,host.GraphicsCaps(),error);
  };
  const auto references = assets.RefCount(models[0].id);
#if defined(__linux__) && !defined(__ANDROID__)
  const bool previous = host.GetWindow().UsesVulkan();
  const auto previousScript = scripts->Capture()[0].json;
  const char* configuredLights = std::getenv("JUDAS_LIGHT_LISTS");
  const auto savedLights = configuredLights ? std::optional<std::string>(configuredLights) : std::nullopt;
  if (!detach()) { Check(false,error.c_str()); return 1; }
  SDL_setenv("JUDAS_LIGHT_LISTS","invalid",1);
  const bool rejected = !host.RebuildDesktopGraphics("Injected renderer failure",640,360,false,previous,error);
  if (savedLights) SDL_setenv("JUDAS_LIGHT_LISTS",savedLights->c_str(),1);
  else unsetenv("JUDAS_LIGHT_LISTS");
  Check(rejected && error.find("Renderer initialization failed")!=std::string::npos &&
            !host.GetWindow().NativeWindow() && !assets.GetRenderer() &&
            scripts->Capture()[0].json == previousScript,
        "failed renderer initialization leaves the paused world and asset manager alive");
  if (!host.RebuildDesktopGraphics("Previous backend recovery",640,360,false,previous,error) || !restore()) {
    Check(false,error.c_str());
    return 1;
  }
  Check(host.GetWindow().UsesVulkan() == previous && assets.TryGetMesh(models[0].id).IsValid() &&
            scripts->Capture()[0].json == previousScript,
        "previous backend and resident assets recover without a scene reload");
#endif
  const bool verifyBoth = JudasVulkan::Requested() && JudasVulkan::Compiled();
  const std::vector<bool> backends = verifyBoth ? std::vector<bool>{false,true,false} :
      std::vector<bool>{host.GetWindow().UsesVulkan()};
  for (const bool useVulkan : backends) {
    const auto retiring = assets.TryGetMesh(models[0].id);
    const auto scriptState = scripts->Capture()[0].json;
    MeshData readback;
    Check(retiring.IsValid() && detach() &&
              !host.GetRenderer().ReadMeshForDiagnostics(retiring, readback) && !assets.GetRenderer() &&
              assets.RefCount(models[0].id) == references,
          "graphics detachment retires the mesh without releasing live world demand");
#if defined(__linux__) && !defined(__ANDROID__)
    const bool rebuilt = host.RebuildDesktopGraphics("Asset-backed world continuity",640,360,false,useVulkan,error);
#else
    const bool rebuilt = assets.AttachGraphics(host.GetRenderer(),error);
#endif
    if (!rebuilt || !restore()) {
      Check(false,error.c_str());
      return 1;
    }
    Check(host.GetWindow().UsesVulkan() == useVulkan &&
              host.GetRenderer().ReadMeshForDiagnostics(assets.TryGetMesh(models[0].id), readback) &&
              assets.RefCount(models[0].id) == references && legacy.Scripts() == scripts &&
              scripts->Capture()[0].json == scriptState &&
              (!hasGpuPool || (legacy.VisualEmitters()[0].gpu &&
                               legacy.VisualEmitters()[0].gpu->Diagnostics().emitted == 4)),
          "recreated graphics restores resident meshes and particles without changing the live script or asset demand");
    auto& replacement = host.GetRenderer();
    RenderWorldFrame(replacement,640,360,legacy,view,projection,{0,0,0},1);
    replacement.BeginUIFrame(640,360);
    ui.Draw(replacement,640,360,&legacy);
    const auto draws = replacement.UIDrawCalls();
    replacement.EndUIFrame();
    TextureData restoredHud;
    Check(draws > 0 && ui.Find("continuity-hud") == hudHandle && ui.CaptureState(hudHandle)==hudState &&
              replacement.ReadTextureForDiagnostics(assets.TryGetTexture(hudAsset.id),restoredHud) &&
              restoredHud.pixels == hudPixels.pixels,
          "the same HUD document draws after its texture and text renderer are restored");
    legacy.FixedScripts(nullptr,1.f/60);
    Check(scripts->Capture()[0].json != scriptState, "the same script instance resumes ticking after graphics recreation");
  }
  legacy.Destroy();
  assets.ReleaseAll();
  CheckTerrainStartup(host, root);
  assets.ReleaseAll();
  std::filesystem::create_directories(root / "SourceArt");
  auto source = root / "SourceArt/image.png";
  const bool tileReady = M93AssetFixtures::WriteTile1(source, error);
  Check(tileReady,
        "original painted tile fixture is generated in this test's output tree");
  if (!tileReady) {
    puts(error.c_str());
    return 1;
  }
  auto destination = root / "Assets/image.judastex";
  Check(CookAssetFile(source.string(), destination.string(), settings, error) &&
            CookedAssetFresh(destination.string(), error),
        "published cook has a verifiable source/output generation");
  auto moved = root / "moved";
  std::filesystem::create_directories(moved / "SourceArt");
  std::filesystem::create_directories(moved / "Assets");
  for (auto p : {source, destination,
                 std::filesystem::path(destination.string() + ".judascook")}) {
    auto relative = p.lexically_relative(root);
    std::filesystem::copy_file(
        p, moved / relative, std::filesystem::copy_options::overwrite_existing);
  }
  Check(CookedAssetFresh((moved / "Assets/image.judastex").string(), error),
        "authoring cook recipes survive project relocation");
  std::vector<uint8_t> bytes;
  std::string recipe;
  Check(PrepareAssetCook(source.string(), destination.string(), settings, bytes,
                         recipe, error),
        "cook preparation is separate from publication");
  auto temporary = destination.string() + ".cooking";
  {
    std::ofstream f(temporary);
    f << "KEEP";
  }
  Check(!PublishAssetCook(destination.string(), bytes, recipe, error) &&
            std::filesystem::file_size(temporary) == 4 &&
            CookedAssetFresh(destination.string(), error),
        "conflicting publication preserves prior output and another writer's "
        "temporary file");
  std::filesystem::remove(temporary);
  {
    std::ofstream f(source, std::ios::app | std::ios::binary);
    f.put('x');
  }
  Check(!CookedAssetFresh(destination.string(), error) &&
            !PublishAssetCook(destination.string(), bytes, recipe, error),
        "changed source is detected and stale prepared generation cannot "
        "publish");
  Check(glGetError() == GL_NO_ERROR, "no GL errors through camera demand, save "
                                     "restoration and scene teardown");
  printf("SUMMARY %d checks %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
