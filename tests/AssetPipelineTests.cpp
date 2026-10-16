#include "AssetBinary.h"
#include "AssetCook.h"
#include "CookedTexture.h"
#include "EngineHost.h"
#include "ModelLoader.h"
#include "NamedAuthoring.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneFingerprint.h"
#include "SceneSerialization.h"
#include "ScreenshotWriter.h"
#include "Tangents.h"
#include "TextureLoader.h"
#include "WorldPersistence.h"
#include "WorldPresentation.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
static unsigned checks = 0, failures = 0;
static void Check(bool yes, const char *text) {
  ++checks;
  failures += !yes;
  printf("%s %s\n", yes ? "PASS" : "FAIL", text);
}
using Bytes = std::vector<uint8_t>;
static void Write(const std::filesystem::path &p, const Bytes &b) {
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char *>(b.data()), std::streamsize(b.size()));
}
static double Rms(const Bytes &a, const Bytes &b) {
  if (a.size() != b.size() || a.empty())
    return 1e9;
  double v = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    double d = int(a[i]) - int(b[i]);
    v += d * d;
  }
  return std::sqrt(v / a.size());
}
int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  auto root =
      std::filesystem::absolute(argc > 1 ? argv[1] : ".cache/m93/checks");
  std::filesystem::create_directories(root / "Assets");
  std::string error;
  AssetCookSettings settings;
  Bytes bytes, again;
  TextureData texture;
  texture.width = 8;
  texture.height = 8;
  texture.pixels.resize(256);
  for (size_t i = 0; i < 64; ++i) {
    texture.pixels[i * 4] = uint8_t(i * 3);
    texture.pixels[i * 4 + 1] = 80;
    texture.pixels[i * 4 + 2] = 170;
    texture.pixels[i * 4 + 3] = 255;
  }
  Check(CookTextureAsset(texture, settings, "source", bytes, error) &&
            CookTextureAsset(texture, settings, "source", again, error) &&
            bytes == again,
        "BC texture cooking is byte deterministic");
  TextureData cooked;
  Check(DecodeCookedTexture(bytes.data(), bytes.size(), cooked, error) &&
            cooked.storage == TextureStorage::BC1 &&
            cooked.levels.size() == 4 && cooked.levels.back().width == 1,
        "full BC1 chain includes sub-block tail mips");
  auto expanded = ExpandTextureMip(cooked.levels[0], cooked.storage);
  Check(Rms(expanded.bytes, texture.pixels) < 9,
        "BC1 keeps representative authored colour detail within measured RMS "
        "bound");
  auto corrupt = bytes;
  corrupt[25] ^= 64;
  TextureData guard = texture;
  Check(!DecodeCookedTexture(corrupt.data(), corrupt.size(), guard, error) &&
            guard.pixels == texture.pixels,
        "corrupt cook fails without publishing partial texture");
  texture.pixels[3] = 40;
  Check(CookTextureAsset(texture, settings, "alpha", bytes, error) &&
            DecodeCookedTexture(bytes.data(), bytes.size(), cooked, error) &&
            cooked.storage == TextureStorage::BC3,
        "alpha content selects BC3 rather than discarding transparency");
  Check(std::abs(
            int(ExpandTextureMip(cooked.levels[0], cooked.storage).bytes[3]) -
            40) <= 2,
        "BC3 transparent pixel survives decoding");
  TextureData pattern;
  pattern.width = 2;
  pattern.height = 1;
  pattern.pixels = {0, 0, 0, 255, 255, 255, 255, 255};
  settings.textureMode = 1;
  Check(CookTextureAsset(pattern, settings, "gamma", bytes, error) &&
            DecodeCookedTexture(bytes.data(), bytes.size(), cooked, error) &&
            std::abs(int(cooked.levels[1].bytes[0]) - 188) <= 1,
        "sRGB mip filter averages linear light");
  settings.textureSemantic = 1;
  CookTextureAsset(pattern, settings, "data", bytes, error);
  DecodeCookedTexture(bytes.data(), bytes.size(), cooked, error);
  Check(cooked.levels[1].bytes[0] == 128,
        "linear data does not receive colour gamma");
  settings.textureSemantic = 2;
  pattern.pixels = {255, 128, 128, 255, 128, 255, 128, 255};
  CookTextureAsset(pattern, settings, "normal", bytes, error);
  DecodeCookedTexture(bytes.data(), bytes.size(), cooked, error);
  auto p = cooked.levels[1].bytes;
  glm::vec3 normal{p[0] / 127.5f - 1, p[1] / 127.5f - 1, p[2] / 127.5f - 1};
  Check(std::abs(glm::length(normal) - 1) < .02f &&
            cooked.storage == TextureStorage::Rgba8,
        "normal mips renormalize and retain lossless vector channels");
  settings = AssetCookSettings{};
  MeshData source;
  Check(LoadModelMesh(".cache/m93/fixtures/Assets/column.obj", source, error),
        "detailed source fixture loads through current importer");
  if (source.vertices.empty())
    return 1;
  Check(CookMeshAsset(source, settings, "column", bytes, error) &&
            CookMeshAsset(source, settings, "column", again, error) &&
            bytes == again,
        "attribute-aware LOD mesh cooking is byte deterministic");
  MeshData mesh;
  Check(DecodeCookedMesh(bytes.data(), bytes.size(), mesh, error) &&
            mesh.lods.size() > 1 &&
            mesh.lods.back().count < mesh.indices.size(),
        "cooked mesh contains genuinely reduced estimated-error LOD geometry");
  if (mesh.lods.empty()) {
    puts(error.c_str());
    return 1;
  }
  Check(mesh.vertices.size() < source.vertices.size() / 2,
        "full-attribute welding removes importer expansion without "
        "positional-only seam merging");
  bool same = source.indices.size() == mesh.indices.size();
  for (size_t i = 0; same && i < source.indices.size(); ++i)
    same = source.vertices[source.indices[i]].position ==
           mesh.vertices[mesh.indices[i]].position;
  bool topology = mesh.sourceVertexIds.size() == mesh.vertices.size();
  for (size_t i = 0; topology && i < source.indices.size(); ++i)
    topology = source.sourceVertexIds[source.indices[i]] ==
               mesh.sourceVertexIds[mesh.indices[i]];
  Check(topology,
        "base physical topology identities survive shading compaction");
  Check(same && !mesh.lodIndices.empty(),
        "physics/import consumers retain exact base triangles; render LOD "
        "tails are separate");
  corrupt = bytes;
  corrupt.back() ^= 1;
  MeshData unchanged = source;
  Check(!DecodeCookedMesh(corrupt.data(), corrupt.size(), unchanged, error) &&
            unchanged.vertices.size() == source.vertices.size(),
        "corrupt mesh cannot partially replace accepted geometry");
  auto bad = settings;
  bad.lodRatio = 1.5;
  Check(!CookMeshAsset(source, bad, "x", again, error),
        "invalid simplification settings fail explicitly");
  auto animated = source;
  animated.skinVertices.push_back({});
  Check(!CookMeshAsset(animated, settings, "x", again, error),
        "animated streams are preserved rather than silently rigidified by "
        "cooking");
  auto finalGeometry = mesh;
  Check(!GenerateMeshTangents(finalGeometry) &&
            finalGeometry.indices == mesh.indices &&
            finalGeometry.lodIndices == mesh.lodIndices &&
            finalGeometry.vertices.size() == mesh.vertices.size(),
        "final LOD geometry rejects a partial tangent reindex without mutating "
        "its streams");
  auto withMaps = source;
  withMaps.materials.emplace_back();
  withMaps.materials[0].maps[0].embedded = texture;
  withMaps.materials[0].maps[2].embedded = pattern;
  MeshData mapped;
  Bytes mappedBytes;
  Check(CookMeshAsset(withMaps, AssetCookSettings{}, "material", mappedBytes,
                      error) &&
            DecodeCookedMesh(mappedBytes.data(), mappedBytes.size(), mapped,
                             error) &&
            mapped.materials.size() == 1 &&
            mapped.materials[0].maps[0].embedded.storage ==
                TextureStorage::BC3 &&
            mapped.materials[0].maps[2].embedded.storage ==
                TextureStorage::Rgba8 &&
            !mapped.materials[0].maps[0].embedded.levels.empty(),
        "imported material maps cook self-contained color/normal roles and mip "
        "chains");
  Write(root / "Assets/column.judasmesh", bytes);
  VisualBounds bounds;
  Check(ReadCookedMeshBounds((root / "Assets/column.judasmesh").string(),
                             bounds, error) &&
            bounds.Valid(),
        "bounded metadata read supplies actual local-space residency bounds");
  auto original = bytes;
  original[8] ^= 1;
  Write(root / "Assets/bad.judasmesh", original);
  Check(!ReadCookedMeshBounds((root / "Assets/bad.judasmesh").string(), bounds,
                              error),
        "corrupt bounds metadata fails open instead of hiding an asset");
  settings = AssetCookSettings{};
  CookTextureAsset(texture, settings, "source", again, error);
  Write(root / "Assets/tile.judastex", again);
  DecodeCookedTexture(again.data(), again.size(), cooked, error);
  Scene scene;
  auto &object = scene.CreateObject("Ordinary cooked object");
  object.render = SceneRenderComponent{};
  object.render->lodMultiplier = 2;
  std::string text;
  SaveSceneToString(scene, text);
  Scene parsed;
  Check(LoadSceneFromString(text, parsed, error) && ScenesEqual(scene, parsed),
        "scene/prefab property path preserves authorable LOD tolerance");
  ProjectSettings ps;
  ps.name = "Asset lab";
  ps.startupScene = "Scenes/a.judas";
  ps.visualBudgetMiB = 8;
  ProjectSettings copy;
  Check(Project::ParseFromString(Project::SerializeToString(ps), copy, error) &&
            copy.visualBudgetMiB == 8,
        "project residency policy survives normal project serialization");
  std::string named, legacy;
  Check(
      LegacyToNamed(Project::SerializeToString(ps), "project", named, error) &&
          NamedToLegacy(named, "project", legacy, error) &&
          Project::ParseFromString(legacy, copy, error) &&
          copy.visualBudgetMiB == 8,
      "named project format retains residency configuration");
  EngineHost host;
  Check(host.Init("M93 pipeline contracts", 640, 360, false, error),
        "native renderer initializes");
  if (failures) {
    puts(error.c_str());
    return 1;
  }
  SDL_GL_SetSwapInterval(0);
  auto &r = host.GetRenderer();
  auto gpu = r.CreateTexture(cooked);
  TextureData read;
  Check(r.ReadTextureForDiagnostics(gpu, read) &&
            Rms(read.pixels, texture.pixels) < 10,
        "actual GPU BC sampling preserves image/alpha content");
  auto mappedGpu = r.CreateMesh(mapped);
  Check(mappedGpu.IsValid() &&
            r.MeshResidentBytes(mappedGpu) >
                mapped.vertices.size() * sizeof(MeshVertex) +
                    (mapped.indices.size() + mapped.lodIndices.size()) * 4,
        "cooked imported material mip chains create actual GPU-owned images");
  r.DestroyMesh(mappedGpu);
  auto compressedBytes = r.TextureResidentBytes(gpu);
  r.SetBlockCompressionEnabled(false);
  auto fallback = r.CreateTexture(cooked);
  Check(r.ReadTextureForDiagnostics(fallback, read) &&
            r.TextureResidentBytes(fallback) > compressedBytes &&
            Rms(read.pixels,
                ExpandTextureMip(cooked.levels[0], cooked.storage).bytes) < 1,
        "unsupported/disabled BC uploads real decoded full mip chain and "
        "accounts for larger fallback");
  r.SetBlockCompressionEnabled(true);
  auto handle = r.CreateMesh(mesh);
  auto proj = glm::perspective(glm::radians(65.f), 640.f / 360, .1f, 2000.f);
  auto draw = [&](float distance, float tolerance, int height = 360) {
    r.ResetStats();
    r.BeginFrame(640, height);
    r.SetCamera(glm::lookAt(glm::vec3(0, 2, distance), glm::vec3(0, 2, 0),
                            glm::vec3(0, 1, 0)),
                proj);
    r.SetLighting({0, 1, 0}, {.5f, .5f, .5f}, {.3f, .3f, .3f});
    r.SetDynamicLights({});
    r.SetMaterialBindings({});
    r.SetMeshLodMultiplier(tolerance);
    r.BeginMeshBatch();
    r.DrawMesh(handle, {0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, gpu, {1, 1, 1}, 1);
    r.EndMeshBatch();
    r.EndFrame();
    return r.Stats().triangles;
  };
  auto full = draw(100, 0), reduced = draw(100, 1), near = draw(5, 1),
       highResolution = draw(100, 1, 1440);
  Check(full == mesh.indices.size() / 3 && reduced < full,
        "zero tolerance forces base while distant view actually submits fewer "
        "triangles");
  Check(near >= reduced && highResolution >= reduced,
        "closer and higher-resolution views never select less detail");
  r.SetMeshLodMultiplier(1);
  MeshData diagnostic;
  Check(r.ReadMeshForDiagnostics(handle, diagnostic) &&
            diagnostic.indices.size() == mesh.indices.size() &&
            diagnostic.lodIndices.size() == mesh.lodIndices.size(),
        "GPU upload retains full base and shared LOD index tails");
  auto oldMesh = handle;
  r.DestroyMesh(handle);
  auto newMesh = r.CreateMesh(mesh);
  Check(oldMesh.id == newMesh.id && oldMesh.generation != newMesh.generation &&
            !r.ReadMeshForDiagnostics(oldMesh, diagnostic) &&
            r.ReadMeshForDiagnostics(newMesh, diagnostic),
        "reused mesh slot rejects retained stale handle");
  auto oldTexture = gpu;
  r.DestroyTexture(gpu);
  auto newTexture = r.CreateTexture(cooked);
  Check(oldTexture.id == newTexture.id &&
            oldTexture.generation != newTexture.generation &&
            !r.ReadTextureForDiagnostics(oldTexture, read) &&
            r.ReadTextureForDiagnostics(newTexture, read),
        "reused texture slot rejects retained stale handle");
  r.DestroyTexture(oldTexture);
  Check(r.ReadTextureForDiagnostics(newTexture, read),
        "stale texture destruction cannot destroy replacement");
  MaterialDefinition mat;
  mat.maps[0].embedded = texture;
  auto oldMaterial = r.CreateMaterial(mat);
  r.DestroyMaterial(oldMaterial);
  auto newMaterial = r.CreateMaterial(mat);
  auto materialBytes = r.MaterialResidentBytes(newMaterial);
  r.DestroyMaterial(oldMaterial);
  Check(oldMaterial.id == newMaterial.id &&
            oldMaterial.generation != newMaterial.generation &&
            r.MaterialResidentBytes(oldMaterial) == 0 && materialBytes > 0 &&
            r.MaterialResidentBytes(newMaterial) == materialBytes,
        "material slot reuse is generation-safe including owned texture "
        "lifetime");
  r.DestroyMaterial(newMaterial);
  size_t slots = r.ResourceSlotCount();
  for (unsigned i = 0; i < 1000; ++i) {
    auto m = r.CreateMesh(mesh);
    auto t = r.CreateTexture(cooked);
    r.DestroyMesh(m);
    r.DestroyTexture(t);
  }
  Check(r.ResourceSlotCount() <= slots + 2,
        "1000 residency churn cycles reuse bounded handle storage");
  host.OpenProjectAssets(root.string(), (root / "Assets").string());
  AssetRecord model, image;
  auto adopt = [&](const std::string &relative, AssetRecord &record) {
    if (auto *existing = host.Assets().FindByRelativePath(relative)) {
      record = *existing;
      return true;
    }
    return host.Assets().Track((root / relative).string(), record, error);
  };
  Check(adopt("Assets/column.judasmesh", model) &&
            adopt("Assets/tile.judastex", image),
        "cooked formats register through ordinary asset database");
  auto &resources = host.Resources();
  resources.ConfigureVisualStreaming(1);
  resources.SetBlockingMode(true);
  resources.DemandVisual(model.id);
  resources.DemandVisual(image.id);
  auto first = resources.TryGetMesh(model.id);
  MeshData published;
  bool publication = r.ReadMeshForDiagnostics(first, published) &&
                     published.vertices.size() == mesh.vertices.size() &&
                     published.indices == mesh.indices &&
                     published.lodIndices == mesh.lodIndices;
  for (size_t i = 0; publication && i < mesh.vertices.size(); ++i)
    publication = published.vertices[i].position == mesh.vertices[i].position &&
                  published.vertices[i].normal == mesh.vertices[i].normal &&
                  published.vertices[i].uv == mesh.vertices[i].uv &&
                  published.vertices[i].tangent == mesh.vertices[i].tangent;
  printf("PUBLICATION vertices=%zu expected=%zu\n", published.vertices.size(),
         mesh.vertices.size());
  Check(publication, "ordinary resource loading preserves the cooked "
                     "vertex/LOD address space exactly");
  resources.SetBudgetBytes(0);
  resources.Pump();
  Check(resources.TryGetMesh(model.id).IsValid(),
        "currently demanded visual survives budget pressure");
  for (int i = 0; i < 4; ++i)
    resources.AdvanceVisualFrame();
  resources.Pump();
  Check(resources.StateOf(model.id) == ResourceState::Unloaded &&
            resources.Stats().evictions > 0,
        "inactive unreferenced cooked visual is actually evicted");
  resources.AddRef(model.id);
  resources.RequestMesh(model.id);
  for (int i = 0; i < 4; ++i)
    resources.AdvanceVisualFrame();
  resources.Pump();
  Check(resources.StateOf(model.id) == ResourceState::Ready &&
            resources.Stats().overBudgetBytes > 0,
        "explicitly pinned dependencies remain resident and oversubscription "
        "is reported");
  resources.ReleaseRef(model.id);
  resources.Pump();
  resources.DemandVisual(model.id);
  Check(resources.TryGetMesh(model.id).IsValid() &&
            resources.TryGetMesh(model.id).generation != first.generation,
        "evicted resource reloads safely with a fresh GPU generation");
  Check(
      glGetError() == GL_NO_ERROR,
      "no GL errors after compressed upload, LOD batching, reuse and eviction");
  printf("SUMMARY %u checks %u failures\n", checks, failures);
  return failures ? 1 : 0;
}
