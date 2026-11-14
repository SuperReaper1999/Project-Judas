#pragma once
#include "MeshData.h"
#include <vector>

namespace JudasRender {
// Four packed scalar entries per uniform vector fit the portable vertex budget.
constexpr unsigned kStructuralSectionLimit = 256;

// Resident structural bindings use the otherwise unused second influence lane
// for the face's physical owner. Ordinary eight-influence skins never opt in.
inline MeshSkinVertex ResolveStructuralWeights(const MeshSkinVertex& source,
                                              const std::vector<int>& sections) {
    MeshSkinVertex result = source;
    result.joints1 = glm::uvec4(0);
    result.weights1 = glm::vec4(0);
    const auto owner = source.joints1.x;
    float sum = 0;
    for (unsigned k = 0; k < 4; ++k) {
        if (sections[source.joints[k]] != sections[owner]) result.weights[k] = 0;
        sum += result.weights[k];
    }
    if (sum > 0) result.weights /= sum;
    else { result.joints = glm::uvec4(owner); result.weights = {1,0,0,0}; }
    return result;
}
}
