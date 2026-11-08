#pragma once
#include "TerrainCache.h"
#include "TerrainLod.h"
#include "render/RenderTypes.h"
#include <map>
#include <tuple>

class Renderer;
namespace TerrainV2 {
struct RenderPageKey {
    PatchKey patch;
    glm::vec4 edges{1};
    bool grid = true;
    unsigned stride = 0, cells = 0;
    unsigned firstX = UINT32_MAX, firstZ = UINT32_MAX, extentX = 0, extentZ = 0, cellsZ = 0;
    bool operator<(const RenderPageKey &b) const {
        if (patch < b.patch)
            return true;
        if (b.patch < patch)
            return false;
        return std::tie(grid, edges.x, edges.y, edges.z, edges.w, stride, cells, firstX, firstZ,
                        extentX, extentZ, cellsZ) <
               std::tie(b.grid, b.edges.x, b.edges.y, b.edges.z, b.edges.w, b.stride, b.cells,
                        b.firstX, b.firstZ, b.extentX, b.extentZ, b.cellsZ);
    }
};
struct RenderPage {
    TextureData heights, weights;
    glm::vec4 weightRect{0, 0, 1, 1};
    size_t Bytes() const;
};
// Render resolution follows selected visual LOD, not physical residency.
// Height and normal samples are exact at every selected grid vertex. Materials
// retain native resolution independently of geometric error or camera distance.
std::vector<RenderPageKey> PartitionRenderPatch(const Source &, const HeightPatch &);
RenderPage BuildRenderPage(const Source &, const RenderPageKey &, bool includeWeights = true);
using TerrainWeightKey = std::array<unsigned, 4>;
struct GpuTerrainPage {
    TextureHandle heights, weights;
    TerrainWeightKey weightKey{};
    MeshHandle mesh;
    glm::vec4 weightRect{0, 0, 1, 1};
    size_t bytes = 0;
    uint64_t used = 0;
};
class RenderPages {
  public:
    explicit RenderPages(size_t budget = 32 * 1024 * 1024) { stats.budget = budget; }
    const GpuTerrainPage &Acquire(Renderer &, const Source &, const RenderPageKey &,
                                  const std::function<std::shared_ptr<const Chunk>()> &mesh = {});
    void Clear(Renderer &);
    // Preserve unaffected images/meshes across publication. Sample bounds are
    // inclusive; stitched heights and central-difference normals include a halo.
    void Invalidate(Renderer &, const Source &, std::optional<glm::uvec4> samples,
                    const std::vector<ChunkKey> &meshes);
    CacheStatistics Statistics() const {
        auto s = stats;
        s.entries = pages.size();
        return s;
    }
    double prepareMs = 0, worstPrepareMs = 0;

  private:
    struct WeightPage {
        TextureHandle texture;
        glm::vec4 rect;
        size_t bytes = 0, references = 0;
    };
    std::map<TerrainWeightKey, WeightPage> weights;
    std::map<RenderPageKey, GpuTerrainPage> pages;
    CacheStatistics stats;
    uint64_t clock = 0;
    void Release(Renderer &, GpuTerrainPage &);
};
} // namespace TerrainV2
