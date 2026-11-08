#pragma once
#include "terrain/TerrainSource.h"
#include <map>
#include <memory>

namespace TerrainV2 {
// Complete worker product. GPU handles remain editor-owned. Geometry is immutable
// after publication, including the shared MeshData view used by legacy picking.
class EditorPreview {
  public:
    const auto &Chunks() const { return chunks; }
    const std::shared_ptr<const MeshData> &Mesh() const { return mesh; }
    bool Pick(glm::vec3 origin, glm::vec3 direction, glm::vec3 &hit,
              size_t *testedTriangles = nullptr) const;
    size_t BoundsBytes() const;

  private:
    struct Range {
        size_t first = 0, count = 0;
        glm::dvec3 minimum{0}, maximum{0};
    };
    std::map<ChunkKey, std::shared_ptr<const Chunk>> chunks;
    std::shared_ptr<const MeshData> mesh;
    std::vector<Range> ranges;
    friend bool PrepareEditorPreview(const Source &, const std::vector<ChunkKey> &,
                                     std::shared_ptr<const EditorPreview>,
                                     std::shared_ptr<const EditorPreview> &, std::string &,
                                     const std::function<bool()> &);
};
// Previous belongs to the same source mapping; keys must include every changed
// chunk (the normal authoring brush supplies Affected). New sources pass null.
// Failure/cancellation keeps both previous and the caller's output intact.
bool PrepareEditorPreview(const Source &, const std::vector<ChunkKey> &,
                          std::shared_ptr<const EditorPreview> previous,
                          std::shared_ptr<const EditorPreview> &output, std::string &error,
                          const std::function<bool()> &cancelled = {});
} // namespace TerrainV2
