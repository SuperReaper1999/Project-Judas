#include "AssetCook.h"
#include "../third_party/meshoptimizer/src/meshoptimizer.h"
#include "../third_party/nlohmann/json.hpp"
#include "AssetBinary.h"
#include "AsyncFile.h"
#include "BoundedAssetCbor.h"
#include "CookedTexture.h"
#include "GltfLoader.h"
#include "ModelArchive.h"
#include "ModelLoader.h"
#include "SceneFingerprint.h"
#include "Tangents.h"
#include "TextureLoader.h"
#define STB_DXT_IMPLEMENTATION
#include "../third_party/stb_dxt.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numeric>
extern "C" unsigned char *stbi_zlib_compress(unsigned char *, int, int *, int);
using Json = nlohmann::json;
using AssetBinary::Bytes;
namespace {
Bytes Header(const char *magic) {
  Bytes b(magic, magic + 4);
  AssetBinary::U32(b, 1);
  return b;
}
Bytes Text(const std::string &s) { return Bytes(s.begin(), s.end()); }
Json Recipe(const AssetCookSettings &s, const std::string &hash) {
  return {{"sourceSha256", hash},
          {"cooker", 1},
          {"textureMax", s.maxTextureSize},
          {"textureMode", s.textureMode},
          {"semantic", s.textureSemantic},
          {"levels", s.lodLevels},
          {"ratio", s.lodRatio},
          {"error", s.lodMaxError},
          {"pixels", s.lodPixels}};
}
void Require(bool yes, const std::string &message) {
  if (!yes)
    throw std::runtime_error(message);
}
float Linear(float x) {
  return x <= .04045f ? x / 12.92f : std::pow((x + .055f) / 1.055f, 2.4f);
}
float Srgb(float x) {
  return x <= .0031308f ? x * 12.92f : 1.055f * std::pow(x, 1 / 2.4f) - .055f;
}
uint8_t Byte(float x) {
  return uint8_t(std::clamp(std::floor(x * 255 + .5f), 0.f, 255.f));
}
TextureMip Down(const TextureMip &a, unsigned semantic) {
  TextureMip b{std::max(1, a.width / 2), std::max(1, a.height / 2), {}};
  b.bytes.resize(size_t(b.width) * b.height * 4);
  for (int y = 0; y < b.height; ++y)
    for (int x = 0; x < b.width; ++x) {
      glm::vec4 sum(0);
      unsigned n = 0;
      for (int sy = y * a.height / b.height; sy < (y + 1) * a.height / b.height;
           ++sy)
        for (int sx = x * a.width / b.width; sx < (x + 1) * a.width / b.width;
             ++sx) {
          auto *p = a.bytes.data() + (size_t(sy) * a.width + sx) * 4;
          glm::vec4 v(p[0] / 255.f, p[1] / 255.f, p[2] / 255.f, p[3] / 255.f);
          if (semantic == 0) {
            v.x = Linear(v.x);
            v.y = Linear(v.y);
            v.z = Linear(v.z);
            v.x *= v.w;
            v.y *= v.w;
            v.z *= v.w;
          } else if (semantic == 2) {
            v.x = v.x * 2 - 1;
            v.y = v.y * 2 - 1;
            v.z = v.z * 2 - 1;
          }
          sum += v;
          ++n;
        }
      sum /= float(n);
      if (semantic == 0) {
        if (sum.w > 0) {
          sum.x /= sum.w;
          sum.y /= sum.w;
          sum.z /= sum.w;
        }
        sum.x = Srgb(sum.x);
        sum.y = Srgb(sum.y);
        sum.z = Srgb(sum.z);
      } else if (semantic == 2) {
        auto normal = glm::vec3(sum);
        normal = glm::length(normal) > 1e-6f ? glm::normalize(normal)
                                             : glm::vec3(0, 0, 1);
        sum = glm::vec4(normal * .5f + .5f, sum.w);
      }
      auto *p = b.bytes.data() + (size_t(y) * b.width + x) * 4;
      for (int k = 0; k < 4; ++k)
        p[k] = Byte(sum[k]);
    }
  return b;
}
Bytes BlockCompress(const TextureMip &a, bool alpha) {
  Bytes b(size_t((a.width + 3) / 4) * ((a.height + 3) / 4) * (alpha ? 16 : 8));
  size_t i = 0;
  for (int y = 0; y < a.height; y += 4)
    for (int x = 0; x < a.width; x += 4) {
      uint8_t block[64];
      for (int dy = 0; dy < 4; ++dy)
        for (int dx = 0; dx < 4; ++dx) {
          auto p = a.bytes.data() +
                   (size_t(std::min(y + dy, a.height - 1)) * a.width +
                    std::min(x + dx, a.width - 1)) *
                       4;
          std::copy(p, p + 4, block + (dy * 4 + dx) * 4);
        }
      stb_compress_dxt_block(b.data() + i, block, alpha, STB_DXT_HIGHQUAL);
      i += alpha ? 16 : 8;
    }
  return b;
}
Json Parts(const std::vector<MeshPrimitive> &parts) {
  Json j = Json::array();
  for (auto &p : parts)
    j.push_back({p.first, p.count, p.material, p.part, p.node});
  return j;
}
std::vector<MeshPrimitive> Parts(const Json &j, size_t indices,
                                 size_t materials) {
  Require(j.is_array() && j.size() <= 16384, "cooked primitive count");
  std::vector<MeshPrimitive> parts;
  for (auto &x : j) {
    Require(x.is_array() && x.size() == 5, "cooked primitive fields");
    MeshPrimitive p{x.at(0), x.at(1), x.at(2), x.at(3), x.at(4)};
    Require(size_t(p.first) + p.count <= indices && p.count % 3 == 0 &&
                p.material >= -1 && p.material < int(materials),
            "cooked primitive range/material");
    parts.push_back(std::move(p));
  }
  return parts;
}
} // namespace
bool CookTextureAsset(const TextureData &source, const AssetCookSettings &s,
                      const std::string &hash, Bytes &out, std::string &error) {
  try {
    Require(source.width > 0 && source.height > 0 &&
                uint64_t(source.width) * source.height * 4 ==
                    source.pixels.size(),
            "cook expects decoded RGBA source");
    Require(s.maxTextureSize >= 1 && s.maxTextureSize <= 4096 &&
                s.textureMode <= 1 && s.textureSemantic <= 2,
            "invalid texture cook settings");
    TextureMip level{source.width, source.height, source.pixels};
    while (unsigned(std::max(level.width, level.height)) > s.maxTextureSize)
      level = Down(level, s.textureSemantic);
    bool alpha = false;
    for (size_t i = 3; i < level.bytes.size(); i += 4)
      alpha |= level.bytes[i] != 255;
    auto format = (s.textureMode == 1 || s.textureSemantic == 2)
                      ? TextureStorage::Rgba8
                      : (alpha ? TextureStorage::BC3 : TextureStorage::BC1);
    std::vector<TextureMip> levels;
    for (;;) {
      levels.push_back(level);
      if (level.width == 1 && level.height == 1)
        break;
      level = Down(level, s.textureSemantic);
    }
    auto b = Header("JTX1");
    AssetBinary::U32(b, unsigned(format));
    AssetBinary::U32(b, unsigned(levels[0].width));
    AssetBinary::U32(b, unsigned(levels[0].height));
    AssetBinary::U32(b, unsigned(levels.size()));
    AssetBinary::Blob(b, Text(Recipe(s, hash).dump()));
    for (auto &l : levels) {
      auto raw =
          format == TextureStorage::Rgba8 ? l.bytes : BlockCompress(l, alpha);
      int count = 0;
      auto *compressed =
          stbi_zlib_compress(raw.data(), int(raw.size()), &count, 12);
      bool use = compressed && count > 0 && size_t(count) < raw.size();
      AssetBinary::U32(b, use ? 1 : 0);
      AssetBinary::U32(b, unsigned(raw.size()));
      AssetBinary::Blob(b, use ? Bytes(compressed, compressed + count) : raw);
      std::free(compressed);
    }
    AssetBinary::Seal(b);
    TextureData check;
    Require(DecodeCookedTexture(b.data(), b.size(), check, error), error);
    out = std::move(b);
    error.clear();
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool CookMeshAsset(const MeshData &source, const AssetCookSettings &s,
                   const std::string &hash, Bytes &out, std::string &error) {
  try {
    Require(!source.skeletal && source.skinVertices.empty(),
            "rigid LOD cooker: preserve animated imports unchanged");
    Require(!source.vertices.empty() && source.vertices.size() <= 2000000 &&
                source.indices.size() <= 6000000,
            "cook mesh count bound");
    Require(s.lodLevels >= 1 && s.lodLevels <= 8 && std::isfinite(s.lodRatio) &&
                s.lodRatio > 0 && s.lodRatio < 1 &&
                std::isfinite(s.lodMaxError) && s.lodMaxError >= 0 &&
                std::isfinite(s.lodPixels) && s.lodPixels >= 0 &&
                s.lodPixels <= 32,
            "invalid LOD cook settings");
    MeshData mesh = source;
    if (mesh.indices.empty()) {
      mesh.indices.resize(mesh.vertices.size());
      std::iota(mesh.indices.begin(), mesh.indices.end(), 0u);
    }
    Require(mesh.indices.size() % 3 == 0, "cook requires triangles");
    for (auto i : mesh.indices)
      Require(i < mesh.vertices.size(), "mesh index outside vertices");
    // Source identities describe physical topology independently of shading
    // seams. Preserve native identities when the importer has no explicit map.
    if (mesh.sourceVertexIds.empty()) {
      mesh.sourceVertexIds.resize(mesh.vertices.size());
      std::iota(mesh.sourceVertexIds.begin(), mesh.sourceVertexIds.end(), 0u);
    }
    Require(mesh.sourceVertexIds.size() == mesh.vertices.size(),
            "source vertex identity count");
    if (mesh.materials.empty()) {
      auto originalVertices = mesh.vertices;
      auto originalIds = mesh.sourceVertexIds;
      auto originalLocations = mesh.vertexLocations;
      std::vector<bool> referenced(mesh.vertices.size());
      for (auto i : mesh.indices)
        referenced[i] = true;
      Require(GenerateMeshTangents(mesh), "tangent generation failed");
      for (size_t i = 0; i < referenced.size(); ++i)
        if (!referenced[i]) {
          mesh.vertices.push_back(originalVertices[i]);
          mesh.sourceVertexIds.push_back(originalIds[i]);
          if (!originalLocations.empty())
            mesh.vertexLocations.push_back(originalLocations[i]);
        }
    }
    // Keep imported materials self-contained and cook each image by its actual
    // shader role. Separate asset IDs remain dependencies resolved normally.
    for (auto &material : mesh.materials)
      for (size_t role = 0; role < material.maps.size(); ++role) {
        auto &map = material.maps[role];
        if (!map.embedded.HasImage())
          continue;
        auto imageSettings = s;
        imageSettings.textureSemantic =
            role == 2 ? 2 : (role == 0 || role == 4 ? 0 : 1);
        Bytes encoded;
        Require(
            CookTextureAsset(map.embedded, imageSettings, hash, encoded, error),
            error);
        TextureData decoded;
        Require(DecodeTextureFromMemory(encoded.data(), encoded.size(),
                                        "cooked material", decoded, error),
                error);
        map.embedded = std::move(decoded);
        map.encodedImage = std::move(encoded);
      }
    // Full attribute equality preserves UV/normal seams. No positional-only
    // weld.
    static_assert(sizeof(MeshVertex) == 56,
                  "explicit cooked vertex layout requires 14 floats");
    std::vector<unsigned> remap(mesh.vertices.size());
    meshopt_Stream streams[] = {
        {mesh.vertices.data(), sizeof(MeshVertex), sizeof(MeshVertex)},
        {mesh.sourceVertexIds.data(), sizeof(uint32_t), sizeof(uint32_t)}};
    size_t count = meshopt_generateVertexRemapMulti(
        remap.data(), nullptr, mesh.vertices.size(), mesh.vertices.size(),
        streams, 2);
    std::vector<uint32_t> identities(count);
    std::vector<ModelSourceLocation> locations(
        mesh.vertexLocations.empty() ? 0 : count);
    for (size_t i = 0; i < remap.size(); ++i) {
      identities[remap[i]] = mesh.sourceVertexIds[i];
      if (!locations.empty())
        locations[remap[i]] = mesh.vertexLocations[i];
    }
    mesh.sourceVertexIds = std::move(identities);
    mesh.vertexLocations = std::move(locations);
    Require(count <= 2000000, "compacted mesh exceeds cooked vertex bound");
    std::vector<MeshVertex> vertices(count);
    meshopt_remapVertexBuffer(vertices.data(), mesh.vertices.data(),
                              mesh.vertices.size(), sizeof(MeshVertex),
                              remap.data());
    meshopt_remapIndexBuffer(mesh.indices.data(), mesh.indices.data(),
                             mesh.indices.size(), remap.data());
    mesh.vertices = std::move(vertices);
    if (mesh.primitives.empty())
      mesh.primitives.emplace_back(0, unsigned(mesh.indices.size()), -1,
                                   "whole");
    auto base = mesh.indices;
    auto baseParts = mesh.primitives;
    mesh.lods = {{0, 0, unsigned(base.size()), baseParts}};
    float priorError = 0;
    for (unsigned level = 1; level < s.lodLevels; ++level) {
      MeshLod lod;
      lod.first = unsigned(mesh.indices.size());
      float worst = 0;
      for (auto p : baseParts) {
        if (!p.count) {
          p.first = unsigned(mesh.indices.size());
          lod.primitives.push_back(p);
          continue;
        }
        Require(size_t(p.first) + p.count <= base.size(),
                "source primitive range");
        std::vector<unsigned> indices(p.count);
        size_t target =
            size_t(double(p.count) * std::pow(s.lodRatio, double(level))) / 3 *
            3;
        target = std::max(size_t(3), target);
        float actual = 0;
        size_t n = meshopt_simplifyWithAttributes(
            indices.data(), base.data() + p.first, p.count,
            &mesh.vertices[0].position.x, mesh.vertices.size(),
            sizeof(MeshVertex), &mesh.vertices[0].normal.x, sizeof(MeshVertex),
            std::array<float, 5>{1.f, 1.f, 1.f, .1f, .1f}.data(), 5, nullptr,
            target, s.lodMaxError,
            meshopt_SimplifyLockBorder | meshopt_SimplifyErrorAbsolute,
            &actual);
        indices.resize(n);
        meshopt_optimizeVertexCache(indices.data(), indices.data(),
                                    indices.size(), mesh.vertices.size());
        p.first = unsigned(mesh.indices.size());
        p.count = unsigned(n);
        lod.primitives.push_back(p);
        mesh.indices.insert(mesh.indices.end(), indices.begin(), indices.end());
        worst = std::max(worst, actual);
      }
      lod.count = unsigned(mesh.indices.size()) - lod.first;
      lod.error = std::max(priorError, worst);
      if (lod.count >= mesh.lods.back().count) {
        mesh.indices.resize(lod.first);
        break;
      }
      priorError = lod.error;
      mesh.lods.push_back(lod);
    }
    VisualBounds bounds;
    for (auto &v : mesh.vertices)
      bounds.Include(v.position);
    // Existing normalized metadata/material representation remains
    // authoritative.
    MeshData meta = source;
    meta.materials = mesh.materials;
    meta.importRecord.clear();
    meta.skeletal.reset();
    meta.skinVertices.clear();
    meta.vertices = {MeshVertex{}};
    meta.indices.clear();
    meta.primitives.clear();
    meta.sourceVertexIds.clear();
    meta.vertexLocations.clear();
    meta.faceLocations.clear();
    Bytes model;
    Require(EncodeModelArchive(meta, model, error), error);
    Json metadata = Recipe(s, hash);
    metadata["model"] = Json::binary(model);
    metadata["sourceIds"] = mesh.sourceVertexIds;
    metadata["vertexSources"] = Json::array();
    metadata["faceSources"] = Json::array();
    for (auto &p : mesh.vertexLocations)
      metadata["vertexSources"].push_back({p.node, p.element});
    for (auto &p : mesh.faceLocations)
      metadata["faceSources"].push_back({p.node, p.element});
    metadata["lods"] = Json::array();
    for (auto &l : mesh.lods)
      metadata["lods"].push_back({{"first", l.first},
                                  {"count", l.count},
                                  {"error", l.error},
                                  {"parts", Parts(l.primitives)}});
    Bytes vertexBytes;
    vertexBytes.reserve(mesh.vertices.size() * 56);
    for (auto &v : mesh.vertices)
      for (float f : {v.position.x, v.position.y, v.position.z, v.normal.x,
                      v.normal.y, v.normal.z, v.uv.x, v.uv.y, v.uv1.x, v.uv1.y,
                      v.tangent.x, v.tangent.y, v.tangent.z, v.tangent.w}) {
        Require(std::isfinite(f), "nonfinite mesh vertex");
        AssetBinary::Float(vertexBytes, f);
      }
    Bytes encodedVertices(
        meshopt_encodeVertexBufferBound(mesh.vertices.size(), 56));
    encodedVertices.resize(meshopt_encodeVertexBuffer(
        encodedVertices.data(), encodedVertices.size(), vertexBytes.data(),
        mesh.vertices.size(), 56));
    Bytes encodedIndices(meshopt_encodeIndexSequenceBound(
        mesh.indices.size(), mesh.vertices.size()));
    encodedIndices.resize(meshopt_encodeIndexSequence(
        encodedIndices.data(), encodedIndices.size(), mesh.indices.data(),
        mesh.indices.size()));
    Require(!encodedVertices.empty() && !encodedIndices.empty(),
            "mesh codec failed");
    auto b = Header("JMS1");
    for (float f : {bounds.min.x, bounds.min.y, bounds.min.z, bounds.max.x,
                    bounds.max.y, bounds.max.z})
      AssetBinary::Float(b, f);
    AssetBinary::U32(b, unsigned(mesh.vertices.size()));
    AssetBinary::U32(b, unsigned(mesh.indices.size()));
    AssetBinary::U32(b, uint32_t(AssetBinary::Digest(b.data(), b.size())));
    AssetBinary::Blob(b, Json::to_cbor(metadata));
    AssetBinary::Blob(b, encodedVertices);
    AssetBinary::Blob(b, encodedIndices);
    AssetBinary::Seal(b);
    MeshData check;
    Require(DecodeCookedMesh(b.data(), b.size(), check, error), error);
    out = std::move(b);
    error.clear();
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool DecodeCookedMesh(const void *data, size_t size, MeshData &out,
                      std::string &error) {
  try {
    AssetBinary::Reader r(data, size, "JMS1");
    VisualBounds bounds;
    bounds.min = {r.Float(), r.Float(), r.Float()};
    bounds.max = {r.Float(), r.Float(), r.Float()};
    Require(bounds.Valid(), "cooked bounds");
    auto nv = r.U32(), ni = r.U32();
    Require(nv > 0 && nv <= 2000000 && ni > 0 && ni <= 12000000 && ni % 3 == 0,
            "cooked mesh count bound");
    Require(r.U32() == uint32_t(AssetBinary::Digest(
                           static_cast<const uint8_t *>(data), 40)),
            "cooked header checksum");
    auto metadata = r.Blob(64 * 1024 * 1024), vertices = r.Blob(),
         indices = r.Blob();
    r.End();
    BoundedModelSax guard;
    Require(Json::sax_parse(metadata, &guard, Json::input_format_t::cbor),
            "malformed/bounded cooked metadata");
    auto j = Json::from_cbor(metadata);
    auto &model = j.at("model").get_binary();
    MeshData m;
    Require(DecodeModelArchive(model.data(), model.size(), m, error), error);
    Bytes decoded(size_t(nv) * 56);
    Require(meshopt_decodeVertexBuffer(decoded.data(), nv, 56, vertices.data(),
                                       vertices.size()) == 0,
            "invalid compressed vertices");
    m.indices.resize(ni);
    Require(meshopt_decodeIndexSequence(m.indices.data(), ni, 4, indices.data(),
                                        indices.size()) == 0,
            "invalid compressed indices");
    for (auto i : m.indices)
      Require(i < nv, "cooked vertex index");
    m.vertices.resize(nv);
    size_t offset = 0;
    auto f = [&]() {
      uint32_t u = 0;
      for (int k = 0; k < 4; ++k)
        u |= uint32_t(decoded[offset++]) << (8 * k);
      float value;
      std::memcpy(&value, &u, 4);
      Require(std::isfinite(value), "nonfinite decoded vertex");
      return value;
    };
    VisualBounds actual;
    for (auto &v : m.vertices) {
      v.position = {f(), f(), f()};
      v.normal = {f(), f(), f()};
      v.uv = {f(), f()};
      v.uv1 = {f(), f()};
      v.tangent = {f(), f(), f(), f()};
      actual.Include(v.position);
    }
    Require(actual.min == bounds.min && actual.max == bounds.max,
            "cooked bounds disagree with geometry");
    m.lodPixels = j.at("pixels");
    Require(std::isfinite(m.lodPixels) && m.lodPixels >= 0 && m.lodPixels <= 32,
            "cooked pixel tolerance");
    Require(j.at("lods").size() > 0 && j.at("lods").size() <= 8,
            "cooked LOD count");
    unsigned previousEnd = 0;
    float previousError = 0;
    for (auto &x : j.at("lods")) {
      MeshLod l;
      l.first = x.at("first");
      l.count = x.at("count");
      l.error = x.at("error");
      l.primitives = Parts(x.at("parts"), ni, m.materials.size());
      Require(l.first == previousEnd && size_t(l.first) + l.count <= ni &&
                  l.count > 0 && l.count % 3 == 0 && std::isfinite(l.error) &&
                  l.error >= previousError,
              "cooked LOD range/error");
      unsigned end = l.first;
      for (auto &p : l.primitives) {
        Require(p.first == end &&
                    size_t(p.first) + p.count <= size_t(l.first) + l.count,
                "cooked part partition");
        end += p.count;
      }
      Require(end == l.first + l.count, "cooked incomplete parts");
      previousEnd = end;
      previousError = l.error;
      m.lods.push_back(std::move(l));
    }
    Require(previousEnd == ni && m.lods[0].error == 0,
            "cooked base/LOD coverage");
    m.sourceVertexIds = j.at("sourceIds").get<std::vector<uint32_t>>();
    Require(m.sourceVertexIds.size() == nv, "cooked source identity count");
    auto provenance = [&](const char *key, size_t count,
                          std::vector<ModelSourceLocation> &target) {
      auto &records = j.at(key);
      Require(records.empty() || records.size() == count,
              "cooked provenance count");
      for (auto &p : records) {
        ModelSourceLocation record{p.at(0), p.at(1)};
        Require(record.node < m.sourceNodes.size(), "cooked provenance node");
        target.push_back(record);
      }
    };
    provenance("vertexSources", nv, m.vertexLocations);
    provenance("faceSources", m.lods[0].count / 3, m.faceLocations);
    m.primitives = m.lods[0].primitives;
    m.lodIndices.assign(m.indices.begin() + m.lods[0].count, m.indices.end());
    m.indices.resize(m.lods[0].count);
    out = std::move(m);
    error.clear();
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool ReadCookedMeshBounds(const std::string &path, VisualBounds &bounds,
                          std::string &error) {
  std::ifstream f(path, std::ios::binary);
  unsigned char b[44];
  if (!f.read(reinterpret_cast<char *>(b), 44) ||
      std::memcmp(b, "JMS1\1\0\0\0", 8)) {
    error = "invalid cooked bounds header";
    return false;
  }
  uint32_t digest = 0;
  for (int k = 0; k < 4; ++k)
    digest |= uint32_t(b[40 + k]) << (k * 8);
  if (digest != uint32_t(AssetBinary::Digest(b, 40)))
    return false;
  VisualBounds value;
  float *fields[] = {&value.min.x, &value.min.y, &value.min.z,
                     &value.max.x, &value.max.y, &value.max.z};
  for (int n = 0; n < 6; ++n) {
    uint32_t u = 0;
    for (int k = 0; k < 4; ++k)
      u |= uint32_t(b[8 + n * 4 + k]) << (k * 8);
    std::memcpy(fields[n], &u, 4);
    if (!std::isfinite(*fields[n]))
      return false;
  }
  if (!value.Valid())
    return false;
  bounds = value;
  return true;
}
namespace {
Json SourceFiles(const std::string &source) {
  std::vector<std::string> files{
      std::filesystem::absolute(source).lexically_normal().string()};
  auto extension = std::filesystem::path(source).extension();
  if (extension == ".gltf" || extension == ".glb") {
    std::vector<std::string> extra;
    std::string e;
    Require(GatherGltfDependencies(source, extra, e), e);
    files.insert(files.end(), extra.begin(), extra.end());
  }
  Json result = Json::object();
  for (auto &file : files) {
    std::string hash, error;
    Require(SceneFingerprintSha256File(file, hash, error), error);
    result[file] = hash;
  }
  return result;
}
void Write(const std::filesystem::path &p, const std::vector<uint8_t> &b) {
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char *>(b.data()), std::streamsize(b.size()));
  Require(bool(f), "cook write failed");
}
} // namespace
bool PrepareAssetCook(const std::string &source, const std::string &destination,
                      const AssetCookSettings &s, Bytes &bytes,
                      std::string &recipe, std::string &error) {
  try {
    Require(std::filesystem::absolute(source).lexically_normal() !=
                std::filesystem::absolute(destination).lexically_normal(),
            "cook must preserve source");
    auto files = SourceFiles(source);
    Json logical = Json::object();
    for (auto it = files.begin(); it != files.end(); ++it)
      logical[std::filesystem::path(it.key())
                  .lexically_relative(
                      std::filesystem::absolute(source).parent_path())
                  .generic_string()] = it.value();
    auto identity = files.size() == 1 ? files.begin().value().get<std::string>()
                                      : SceneFingerprintSha256(logical.dump());
    auto extension = std::filesystem::path(destination).extension();
    if (extension == ".judastex") {
      TextureData t;
      Require(LoadTextureFromFile(source, t, error) &&
                  CookTextureAsset(t, s, identity, bytes, error),
              error);
    } else if (extension == ".judasmesh") {
      MeshData mesh;
      Require(LoadModelMesh(source, mesh, error) &&
                  CookMeshAsset(mesh, s, identity, bytes, error),
              error);
    } else
      throw std::runtime_error(
          "cook destination requires .judastex or .judasmesh");
    Require(SourceFiles(source) == files,
            "source changed while cooking; retry");
    Json j = Recipe(s, identity);
    auto parent = std::filesystem::absolute(destination).parent_path();
    j["source"] = std::filesystem::absolute(source)
                      .lexically_normal()
                      .lexically_relative(parent)
                      .generic_string();
    j["files"] = Json::object();
    for (auto it = files.begin(); it != files.end(); ++it)
      j["files"][std::filesystem::path(it.key())
                     .lexically_relative(parent)
                     .generic_string()] = it.value();
    j["outputSha256"] = SceneFingerprintSha256(std::string_view(
        reinterpret_cast<const char *>(bytes.data()), bytes.size()));
    recipe = j.dump(2) + "\n";
    error.clear();
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool PublishAssetCook(const std::string &destination, const Bytes &bytes,
                      const std::string &recipe, std::string &error) {
  namespace fs = std::filesystem;
  fs::path path(destination),
      record = destination + ".judascook", temporary = destination + ".cooking",
      recordTemporary = destination + ".judascook.cooking",
      backup = destination + ".previous-cook",
      recordBackup = destination + ".judascook.previous-cook";
  bool had = false, hadRecord = false, published = false,
       publishedRecord = false, ownsTemps = false;
  try {
    auto j = Json::parse(recipe);
    for (auto it = j.at("files").begin(); it != j.at("files").end(); ++it) {
      std::string hash;
      Require(
          SceneFingerprintSha256File(
              (std::filesystem::absolute(destination).parent_path() / it.key())
                  .string(),
              hash, error) &&
              hash == it.value(),
          "source changed before cook publication");
    }
    Require(
        j.at("outputSha256") ==
            SceneFingerprintSha256(std::string_view(
                reinterpret_cast<const char *>(bytes.data()), bytes.size())),
        "cook payload/recipe mismatch");
    Require(
        !fs::exists(temporary) && !fs::exists(recordTemporary) &&
            !fs::exists(backup) && !fs::exists(recordBackup),
        "unfinished/concurrent cook exists; inspect its files before retrying");
    ownsTemps = true;
    if (!path.parent_path().empty())
      fs::create_directories(path.parent_path());
    Write(temporary, bytes);
    Write(recordTemporary, Text(recipe));
    if (fs::exists(path)) {
      fs::rename(path, backup);
      had = true;
    }
    if (fs::exists(record)) {
      fs::rename(record, recordBackup);
      hadRecord = true;
    }
    fs::rename(temporary, path);
    published = true;
    fs::rename(recordTemporary, record);
    publishedRecord = true;
    std::error_code ignored;
    if (had)
      fs::remove(backup, ignored);
    if (hadRecord)
      fs::remove(recordBackup, ignored);
    error.clear();
    return true;
  } catch (const std::exception &e) {
    std::error_code ec;
    if (published)
      fs::remove(path, ec);
    if (publishedRecord)
      fs::remove(record, ec);
    if (had)
      fs::rename(backup, path, ec);
    if (hadRecord)
      fs::rename(recordBackup, record, ec);
    if (ownsTemps) {
      fs::remove(temporary, ec);
      fs::remove(recordTemporary, ec);
    }
    error = e.what();
    return false;
  }
}
bool CookedAssetFresh(const std::string &destination, std::string &error) {
  try {
    auto side = destination + ".judascook";
    if (!std::filesystem::exists(side)) {
      error.clear();
      return true;
    }
    std::ifstream f(side);
    Json j;
    f >> j;
    std::string hash;
    Require(SceneFingerprintSha256File(destination, hash, error) &&
                hash == j.at("outputSha256"),
            "cooked output differs from recorded generation");
    for (auto it = j.at("files").begin(); it != j.at("files").end(); ++it)
      Require(
          SceneFingerprintSha256File(
              (std::filesystem::absolute(destination).parent_path() / it.key())
                  .string(),
              hash, error) &&
              hash == it.value(),
          "stale cooked asset; source changed or is missing: " + it.key());
    error.clear();
    return true;
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
}
bool CookAssetFile(const std::string &source, const std::string &destination,
                   const AssetCookSettings &s, std::string &error) {
  Bytes bytes;
  std::string recipe;
  return PrepareAssetCook(source, destination, s, bytes, recipe, error) &&
         PublishAssetCook(destination, bytes, recipe, error);
}
