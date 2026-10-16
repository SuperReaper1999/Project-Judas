#pragma once
#include "MeshData.h"
#include "TextureData.h"
#include "Visibility.h"
#include <string>
struct AssetCookSettings {
  unsigned maxTextureSize = 4096,
           textureMode = 0; // 0 auto BC1/3, 1 lossless RGBA
  unsigned textureSemantic =
      0; // 0 sRGB colour, 1 linear data, 2 tangent normal
  unsigned lodLevels = 4;
  float lodRatio = .35f, lodMaxError = .1f, lodPixels = .5f;
};
bool CookTextureAsset(const TextureData &, const AssetCookSettings &,
                      const std::string &sourceHash, std::vector<uint8_t> &,
                      std::string &error);
bool CookMeshAsset(const MeshData &, const AssetCookSettings &,
                   const std::string &sourceHash, std::vector<uint8_t> &,
                   std::string &error);
bool DecodeCookedMesh(const void *, size_t, MeshData &, std::string &error);
bool ReadCookedMeshBounds(const std::string &, VisualBounds &,
                          std::string &error);
bool CookAssetFile(const std::string &source, const std::string &destination,
                   const AssetCookSettings &, std::string &error);

bool PrepareAssetCook(const std::string &source, const std::string &destination,
                      const AssetCookSettings &, std::vector<uint8_t> &,
                      std::string &recipe, std::string &error);
bool PublishAssetCook(const std::string &destination,
                      const std::vector<uint8_t> &, const std::string &recipe,
                      std::string &error);
bool CookedAssetFresh(const std::string &destination, std::string &error);
