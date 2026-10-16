#include "EngineHost.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include "TextureLoader.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
using Clock = std::chrono::steady_clock;
static double ms(Clock::time_point t) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}
int main(int argc, char **argv) {
  if (argc < 4)
    return 2;
  std::filesystem::path root = std::filesystem::absolute(argv[1]),
                        out = argv[2];
  std::filesystem::create_directories(out);
  std::string error;
  EngineHost host;
  if (!host.Init("M93 asset observation", 960, 540, false, error)) {
    puts(error.c_str());
    return 1;
  }
  SDL_GL_SetSwapInterval(0);
  host.OpenProjectAssets(root.string(), (root / "Assets").string());
  auto &db = host.Assets();
  AssetRecord mesh, texture;
  std::string ext = argv[3];
  auto adopt = [&](std::string relative, AssetRecord &record) {
    if (auto *old = db.FindByRelativePath(relative)) {
      record = *old;
      return true;
    }
    return db.Track((root / relative).string(), record, error);
  };
  if (!adopt("Assets/column" + ext, mesh) ||
      !adopt(ext == ".obj" ? "Assets/tile0.png" : "Assets/tile0.judastex",
             texture)) {
    puts(error.c_str());
    return 1;
  }

  auto &assets = host.Resources();
  assets.SetBlockingMode(true);
  auto t = Clock::now();
  auto m = assets.GetMesh(mesh.id, error);
  auto tex = assets.GetTexture(texture.id, error);
  auto load = ms(t);
  if (!m.IsValid() || !tex.IsValid()) {
    puts(error.c_str());
    return 1;
  }
  auto &r = host.GetRenderer();
#if __has_include("AssetCook.h")
  r.SetMeshLodMultiplier(argc > 4 && std::string(argv[4]) == "full" ? 0 : 1);
#endif
  size_t physicalGpu=0;
#if __has_include("AssetCook.h")
  physicalGpu=r.MeshResidentBytes(m)+r.TextureResidentBytes(tex);
#else
  // This fixture has no materials/skin. The frozen mesh ledger is its exact
  // VBO/EBO payload; replace its approximate texture mip count analytically.
  // Read only PNG dimensions, avoiding a second diagnostic decode in RSS.
  unsigned char header[24]{};
  std::ifstream png(root/"Assets/tile0.png",std::ios::binary);
  if(!png.read(reinterpret_cast<char*>(header),24))return 3;
  auto be=[&](unsigned at){return unsigned(header[at])<<24|unsigned(header[at+1])<<16|unsigned(header[at+2])<<8|unsigned(header[at+3]);};
  unsigned w=be(16),h=be(20);size_t base=size_t(w)*h*4;
  physicalGpu=assets.Stats().bytesResident-(base+base/3);
  for(;;w=std::max(1u,w/2),h=std::max(1u,h/2)){physicalGpu+=size_t(w)*h*4;if(w==1&&h==1)break;}
#endif
  printf("PAYLOAD gpu_bytes=%zu accounting=%s\n", physicalGpu,
         ext == ".obj" ? "VBO/EBO plus full RGBA mip dimensions"
                       : "actual renderer owned payload");
  auto &profiler = PerformanceProfiler::Get();
  profiler.Enable(true);

  std::ofstream csv(out / "frames.csv");
  csv << "views,sample,ms,draws,triangles\n";
  for (int views : {1, 2}) {
    auto target = r.CreateRenderTarget(480, 270, error);
    std::vector<double> costs;
    for (int frame = -12; frame < 60; ++frame) {
      ProfileFrame profileFrame("M93 completed asset frame");
      t = Clock::now();
      r.ResetStats();
      r.BeginFrame(960, 540);
      for (int v = 0; v < views; ++v) {
        if (v)
          r.BeginRenderTarget(target);
        r.SetCamera(
            glm::lookAt(glm::vec3(v ? 15 : 0, 10, 42), glm::vec3(0, 2, 0),
                        glm::vec3(0, 1, 0)),
            glm::perspective(glm::radians(65.f), 960.f / 540, .1f, 400.f));
        r.SetLighting({0, 1, 0}, {.8f, .8f, .8f}, {.15f, .15f, .15f});
        r.SetDynamicLights({});
        r.SetMaterialBindings({});
        r.BeginMeshBatch();
        for (int i = 0; i < 128; ++i)
          r.DrawMesh(m, {float(i % 16 - 8) * 4, 0, float(i / 16 - 4) * 8},
                     {1, 0, 0, 0}, {1, 1, 1}, tex, {1, 1, 1}, 1);
        r.EndMeshBatch();
        if (v)
          r.EndRenderTarget();
      }
      r.EndFrame();
      r.FinishForDiagnostics();
      double elapsed = ms(t);
      host.GetWindow().SwapBuffers();
      if (frame >= 0) {
        costs.push_back(elapsed);
        csv << views << ',' << frame << ',' << elapsed << ','
            << r.Stats().drawCalls << ',' << r.Stats().triangles << '\n';
      }
    }
    std::sort(costs.begin(), costs.end());
    std::vector<unsigned char> pixels;
    r.CaptureFrame(960, 540, pixels);
    WriteRgbPng((out / ("view" + std::to_string(views) + ".png")).string(), 960,
                540, pixels);
    printf("ASSETS views=%d load_ms=%.6f resident=%llu median=%.6f p95=%.6f "
           "max=%.6f triangles=%u\n",
           views, load, (unsigned long long)assets.Stats().bytesResident,
           costs[29], costs[56], costs.back(), r.Stats().triangles);
    r.DestroyRenderTarget(target);
  }
  profiler.Export((out / "profile.json").string(), error);
  return 0;
}
