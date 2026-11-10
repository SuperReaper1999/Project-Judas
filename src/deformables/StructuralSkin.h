#pragma once
#include "MeshData.h"
#include <array>
#include <string>
struct DeformableAsset;
struct DeformableInstance;

// Presentation only. Every corner has an owning section for tears, with a
// compact smooth blend of nearby physical frames while those sections connect.
// No damage state, collision body or scripted displacement lives in the skin.
// Map/Palette require a live instance of the prepared partition used by Bind.
class StructuralSkin {
  public:
    bool Bind(const DeformableAsset &, const MeshData &, std::string &error, bool panelSeams=false);
    void Map(const DeformableInstance &, double alpha, MeshData &) const;
    void BuildGpuMesh(const DeformableInstance &, MeshData &) const;
    void Palette(const DeformableInstance &, double alpha, std::vector<glm::mat4> &) const;
    // Retained numeric and image payload, excluding allocator overhead.
    size_t Bytes() const;

  private:
    struct Corner {
        unsigned owner = 0;
        std::array<unsigned, 4> parts{};
        std::array<double, 4> weights{};
    };
    MeshData rest;
    std::vector<Corner> corners;
    std::vector<glm::dvec3> centers;
    // Three non-collinear rest nodes per section recover its actual rigid frame.
    struct Frame {
        glm::uvec3 nodes{0};
        glm::dmat3 inverse{1};
    };
    std::vector<Frame> frames;
};
