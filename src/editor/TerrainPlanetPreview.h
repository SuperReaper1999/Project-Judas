#pragma once
#include "PlanetTiles.h"
#include "terrain/TerrainAsset.h"
#include <filesystem>
class Scene;
class ResourceManager;

// Scene view owns its planetary geometry and native replacement coverage here.
// Cached source headers avoid source decoding or baking on ordinary frames.
class TerrainPlanetPreview {
  public:
    void Draw(Renderer &, const Scene &, ResourceManager &, std::string &error);
    void Clear();

  private:
    struct Planet {
        std::string descriptor;
        std::unique_ptr<PlanetTiles> tiles;
    };
    struct Header {
        std::string path, error;
        std::filesystem::file_time_type modified{};
        uintmax_t bytes = 0;
        TerrainV2::Source source;
    };
    std::map<uint64_t, Planet> planets;
    std::map<std::string, Header> sources;
    const Header &ReadHeader(const std::string &, ResourceManager &);
};
