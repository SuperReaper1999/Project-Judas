#include "AssetCook.h"
#include "EngineHost.h"
#include "ModelLoader.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "WorldPersistence.h"
#include "WorldPresentation.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
static int checks = 0, failures = 0;
static void Check(bool yes, const char *message) {
  ++checks;
  failures += !yes;
  printf("%s %s\n", yes ? "PASS" : "FAIL", message);
}
int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  auto root =
      std::filesystem::absolute(argc > 1 ? argv[1] : ".cache/m93/residency");
  std::filesystem::create_directories(root / "Assets");
  std::string error;
  AssetCookSettings settings;
  for (int i = 0; i < 3; ++i)
    Check(CookAssetFile(
              ".cache/m93/fixtures/Assets/column.obj",
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
  world.legacyGameplay = false;
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
    RenderWorldFrame(renderer, 640, 360, world, nullptr, view, projection,
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
  restored.legacyGameplay = false;
  Check(restored.Build(saved, &assets, error),
        "restored world reconstructs ordinary visual owners");
  assets.AdvanceVisualFrame();
  RenderWorldFrame(renderer, 640, 360, restored, nullptr, view, projection,
                   {0, 0, 0}, 1);
  Check(
      assets.StateOf(models[0].id) == ResourceState::Ready,
      "restored physical/presentation world reloads actual required geometry");
  restored.Destroy();
  assets.ReleaseAll();
  assets.ConfigureVisualStreaming(0);
  RuntimeWorld legacy;
  legacy.legacyGameplay = false;
  Check(legacy.Build(scene, &assets, error) &&
            assets.RefCount(models[0].id) > 0 &&
            assets.StateOf(models[2].id) == ResourceState::Ready,
        "default projects retain prior eager/pinned visual behaviour");
  legacy.Destroy();
  assets.ReleaseAll();
  std::filesystem::create_directories(root / "SourceArt");
  auto source = root / "SourceArt/image.png";
  std::filesystem::copy_file(".cache/m93/fixtures/Assets/tile1.png", source,
                             std::filesystem::copy_options::overwrite_existing);
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
