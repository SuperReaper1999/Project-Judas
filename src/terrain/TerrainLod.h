#pragma once
#include "TerrainSource.h"
#include "TextureData.h"
#include <map>
#include <set>
#include <tuple>

namespace TerrainV2 {
struct View {
    glm::vec3 position{0}; // source-local metres
    double pixelsPerRadian = 600;
    double orthographicPixelsPerMetre = 0;
};
struct PatchKey {
    unsigned x = 0, z = 0, level = 0; // x/z in finest chunks
    bool operator<(const PatchKey &b) const {
        return std::tie(level, z, x) < std::tie(b.level, b.z, b.x);
    }
};
struct HeightPatch {
    PatchKey key;
    // Neighbour sample stride; only the finer edge interpolates to the coarser
    // edge. All coordinates refer to the same integer source lattice.
    glm::vec4 edgeStrides{1}; // -X,+X,-Z,+Z
    float minimum = 0, maximum = 0;
};
// Immutable geometric-error hierarchy. No graphics/physics handles or camera
// state. Worker-owned construction; camera union only selects existing nodes.
class HeightLod {
  public:
    explicit HeightLod(const Source &);
    std::vector<HeightPatch> Select(const std::vector<View> &, const std::set<ChunkKey> &volume,
                                    double pixelError = 2) const;
    size_t Bytes() const;

  private:
    struct Node {
        float minimum = 0, maximum = 0, error = 0;
        bool complete = false;
    };
    unsigned nx, nz, cells, rootLevel;
    float spacing;
    std::map<PatchKey, Node> nodes;
    Node Build(const Source &, PatchKey);
    void Choose(PatchKey, const std::vector<View> &, const std::vector<unsigned> &, double,
                std::vector<HeightPatch> &) const;
};
// CPU mirror of the edge interpolation contract, used by geometric seam tests.
float LodHeight(const Source &, glm::vec2 sample, unsigned stride);
MeshData HeightGrid(unsigned cells);
TextureData HeightImage(const Source &);
} // namespace TerrainV2
