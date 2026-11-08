// Frozen original authoring picker from 03927f71. Do not update with production.
// Original function SHA256: 77e382f10d31b84566f871ad49a0ff3ae05d75d2b3c6ac56f965059834317729
#include "MeshData.h"
#include <cmath>
bool OriginalTerrainPick(const MeshData &m, glm::vec3 o, glm::vec3 d, glm::vec3 &hit) {
    float nearest = INFINITY;
    for (size_t i = 0; i < m.indices.size(); i += 3) {
        auto a = m.vertices[m.indices[i]].position, b = m.vertices[m.indices[i + 1]].position,
             c = m.vertices[m.indices[i + 2]].position;
        auto e1 = b - a, e2 = c - a, p = glm::cross(d, e2);
        float det = glm::dot(e1, p);
        if (std::abs(det) < 1e-8f)
            continue;
        float inv = 1 / det;
        auto t = o - a;
        float u = glm::dot(t, p) * inv;
        if (u < 0 || u > 1)
            continue;
        auto q = glm::cross(t, e1);
        float v = glm::dot(d, q) * inv;
        if (v < 0 || u + v > 1)
            continue;
        float dist = glm::dot(e2, q) * inv;
        if (dist >= 0 && dist < nearest)
            nearest = dist;
    }
    if (!std::isfinite(nearest))
        return false;
    hit = o + d * nearest;
    return true;
}
