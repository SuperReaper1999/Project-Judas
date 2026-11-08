#pragma once
#include <glm/glm.hpp>
#include <string>

namespace TerrainV2 {
// Presentation metadata only. Metres are in the rigid source frame, so detail
// does not move when the world origin shifts. A single shared normal detail is
// intentional: four albedo layers plus detail fit the portable sampler budget.
struct MaterialDetail {
    glm::vec4 tileMetres{4};
    glm::vec4 roughness{1}; // Multiplies the owner's PBR roughness per layer.
    std::string normalTexture;
    float normalMetres = 1;
    float normalStrength = 1;
    bool operator==(const MaterialDetail &other) const {
        return tileMetres == other.tileMetres && roughness == other.roughness &&
               normalTexture == other.normalTexture && normalMetres == other.normalMetres &&
               normalStrength == other.normalStrength;
    }
    bool operator!=(const MaterialDetail &other) const { return !(*this == other); }
};
} // namespace TerrainV2
