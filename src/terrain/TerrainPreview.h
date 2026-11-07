#pragma once
#include "TerrainSource.h"
#include "TextureData.h"
class AssetDatabase;
namespace TerrainV2 {
// Ordinary cooked preview for consumers that haven't opened the native terrain
// editor yet. Runtime/detail authoring keeps full tiled layers; this atlas does
// not become the physical or high-detail terrain representation.
bool BuildPreviewTexture(const Source &, const AssetDatabase *, TextureData &, std::string &);
std::string EncodePreviewTga(const TextureData &);
bool BuildPreviewMesh(const Source &, MeshData &, std::string &);
} // namespace TerrainV2
