#include "Application.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "PerformanceProfiler.h"
#include "RuntimeWorld.h"
#include "SaveService.h"
#include "SceneSession.h"
#include "ScreenshotWriter.h"
#include <cstdio>
#include <filesystem>
static unsigned checks = 0, failures = 0;
static void Check(bool yes, const char *label) {
  ++checks;
  failures += !yes;
  printf("%s %s\n", yes ? "PASS" : "FAIL", label);
}
int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  namespace fs = std::filesystem;
  auto out = fs::absolute(argc > 1 ? argv[1] : ".cache/m93/lab-check");
  fs::create_directories(out);
  auto copy = out / "project";
  if (fs::exists(copy)) {
    fprintf(stderr, "Use a fresh owned output\n");
    return 2;
  }
  fs::copy("projects/asset_lab", copy, fs::copy_options::recursive);
  std::string project = (copy / "asset_lab.judasproj").string(), error;
  unsigned stage = 0, age = 0, frames = 0;
  glm::vec3 savedEye{};
  const char *keys[] = {"",       "key:1",      "key:V",      "key:V",
                        "key:F6", "key:3",      "key:F7",     "key:F2",
                        "key:F2", "key:Escape", "key:Escape", "key:R"};
  ApplicationControl c;
  c.hidden = true;
  c.frameSeconds = [](float) { return 1.f / 60; };
  c.hostReady = [](EngineHost &h) {
    h.GetWindow().SetTestInputMode(true);
    SDL_GL_SetSwapInterval(0);
  };
  c.beforeFrame = [&](EngineHost &h, RuntimeWorld &, InteractivePlay &) {
    h.GetRenderer().ResetStats();
    if (stage && stage < 12 && age < 2)
      h.GetWindow().QueueTestPhysical(keys[stage], age == 0 ? 1 : 0);
    SDL_Delay(1);
  };
  c.afterFrame = [&](EngineHost &h, RuntimeWorld &w, InteractivePlay &play) {
    ++age;
    ++frames;
    if (frames > 1800) {
      Check(false, "bounded application lifecycle completion");
      w.UI().RequestQuit();
      return;
    }
    if (w.Scripts() && !w.Scripts()->Diagnostics().empty()) {
      for (auto &d : w.Scripts()->Diagnostics())
        printf("FAULT %s %s\n", d.callback.c_str(), d.message.c_str());
      Check(false, "healthy project callbacks");
      w.UI().RequestQuit();
      return;
    }
    if (age < 30)
      return;
    if (w.SceneControl() && (w.SceneControl()->Pending() ||
                             w.SceneControl()->Saves(h.Resources())->Busy()))
      return;
    h.Resources().WaitForAll();
    if (stage == 0) {
      Check(w.view.has_value() && w.StaticRenderables().size() > 576,
            "ordinary asset project starts with script-owned view and authored "
            "scenery");
      Check(h.Resources().VisualStreaming() &&
                h.Resources().RefCount(
                    w.RuntimeDefinition(100)->render->meshAsset) == 0,
            "real project enables soft visual ownership");
    }
    if (stage == 1) {
      savedEye = w.view->pose.position;
      Check(savedEye.x == 0 && savedEye.z == 44,
            "logical district control changes real camera state");
    }
    if (stage == 2)
      Check(w.PlayersIfLoaded() && w.PlayersIfLoaded()->All().size() == 2 &&
                h.GetRenderer().Stats().offscreenPasses > 0,
            "independent split views render alongside cached remote camera");
    if (stage == 3)
      Check(!w.PlayersIfLoaded()->HasViews(),
            "leaving split views clears their interests");
    if (stage == 4) {
      bool found = false;
      for (auto &p : fs::recursive_directory_iterator(w.SceneControl()->Saves(h.Resources())->Root()))
        if (p.is_regular_file() && p.path().extension() == ".save")
          found = true;
      Check(found, "ordinary save actually publishes durable bytes");
    }
    if (stage == 5)
      Check(w.view->pose.position.x > 100 && w.view->pose.position.z > 100,
            "post-save district change establishes different physical view");
    if (stage == 6)
      Check(glm::length(w.view->pose.position - savedEye) < .001f,
            "load restores actual saved camera state rather than a cached "
            "status");
    if (stage == 7)
      Check(w.SceneControl()->Current() == "Scenes/full_detail.judas" &&
                w.RuntimeDefinition(100)->render->lodMultiplier == 0,
            "full detail control reloads scene with real base-geometry policy");
    if (stage == 8)
      Check(w.SceneControl()->Current() == "Scenes/museum.judas" &&
                w.RuntimeDefinition(100)->render->lodMultiplier == 1,
            "automatic LOD scene restores its actual policy");
    if (stage == 9)
      Check(w.UI().Paused() && !h.GetWindow().IsMouseCaptured(),
            "pause owns focus and releases pointer");
    if (stage == 10)
      Check(!w.UI().Paused(), "resume releases modal focus");
    if (stage == 11) {
      Check(w.SceneControl()->Current() == "Scenes/museum.judas" &&
                w.Scripts()->Diagnostics().empty(),
            "reload starts a healthy new world without stale callback state");
      Check(h.GetRenderer().Stats().instancedDraws > 0,
            "cooked assets use actual instanced rendering");
      std::vector<unsigned char> pixels;
      h.GetRenderer().CaptureFrame(h.GetWindow().Width(),
                                   h.GetWindow().Height(), pixels);
      WriteRgbPng((out / "museum.png").string(), h.GetWindow().Width(),
                  h.GetWindow().Height(), pixels);
      printf("LAB fixed_ms=%.6f gpu_resource_bytes=%llu over_budget=%llu "
             "draws=%u triangles=%u\n",
             play.LastFixedStepMilliseconds(),
             (unsigned long long)h.Resources().Stats().gpuBytesResident,
             (unsigned long long)h.Resources().Stats().overBudgetBytes,
             h.GetRenderer().Stats().drawCalls,
             h.GetRenderer().Stats().triangles);
      w.UI().RequestQuit();
    }
    ++stage;
    age = 0;
  };
  c.beforeShutdown = [&](EngineHost &, RuntimeWorld &, InteractivePlay &) {
    PerformanceProfiler::Get().Export((out / "profile.json").string(), error);
  };
  char *args[] = {const_cast<char *>("judas"), project.data()};
  Application app;
  Check(app.Run(2, args, &c) == 0 && stage == 12,
        "ordinary application startup, async scene/save services and Stop");
  printf("SUMMARY %u checks %u failures\n", checks, failures);
  return failures ? 1 : 0;
}
