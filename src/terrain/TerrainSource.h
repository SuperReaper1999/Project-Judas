#pragma once

#include "MeshData.h"
#include "TerrainPlanetChart.h"
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace TerrainV2 {
// One sampled landscape owns both ordinary surface patches and excavated
// patches. Positions in the source are local metres; world placement remains
// the world's double-precision origin plus its ordinary rigid transform.
struct Edit {
    enum class Kind { Remove, Add };
    Kind kind = Kind::Remove;
    glm::vec3 point{0};
    float radius = 1;
};
struct Source {
    unsigned version = 2;
    // Version3 sources own a bounded planetary coordinate mapping. Ordinary
    // version2 samples and their native encoding remain unchanged.
    std::optional<PlanetChart> chart;
    unsigned samplesX = 129, samplesZ = 129, chunkCells = 16;
    float spacing = .5f, bottom = -4096, top = 4096;
    std::vector<float> heights;
    std::array<std::string, 4> layerTextures;
    std::array<glm::vec3, 4> layerColours{
        {{.25f, .36f, .12f}, {.38f, .30f, .19f}, {.42f, .43f, .40f}, {.7f, .65f, .48f}}};
    std::vector<Edit> edits;
    // Four normalized material weights. These are data, not baked colours.
    std::vector<glm::vec4> weights;
    unsigned ChunksX() const { return (samplesX - 1) / chunkCells; }
    unsigned ChunksZ() const { return (samplesZ - 1) / chunkCells; }
};
struct ChunkKey {
    unsigned x = 0, z = 0;
    bool operator<(const ChunkKey &b) const { return x < b.x || (x == b.x && z < b.z); }
    bool operator==(const ChunkKey &b) const { return x == b.x && z == b.z; }
};
struct Chunk {
    ChunkKey key;
    glm::vec3 anchor{0};
    MeshData mesh;
    bool volumetric = false;
    size_t sampledBytes = 0;
};
Source Create(unsigned samplesX, unsigned samplesZ, float spacing, unsigned chunkCells = 16);
bool Validate(const Source &, std::string &);
float Height(const Source &, float x, float z);
float Density(const Source &, glm::vec3 point);
// Physical-radius volume brushes on a chart, with conservative broadphase reach.
// Ordinary extraction retains its existing binary64 brush calculation.
double EditDistance(const Source &, glm::vec3, const Edit &);
double BrushReach(const Source &, double radius);
bool SourcePoint(const Source &, glm::vec3 localPoint, glm::vec3 &, std::string &);
glm::vec4 Weights(const Source &, float x, float z);
std::vector<ChunkKey> Affected(const Source &, const Edit &);
bool Touches(const Source &, ChunkKey, const Edit &);
bool BuildChunk(const Source &, ChunkKey, Chunk &, std::string &,
                const std::function<bool()> &cancelled = {});
std::string Serialize(const Source &);
bool Parse(const std::string &, Source &, std::string &);
} // namespace TerrainV2
