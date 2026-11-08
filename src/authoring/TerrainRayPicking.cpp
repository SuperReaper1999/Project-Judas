#include "TerrainRayPicking.h"
#include <cmath>
#include <limits>

bool PickTerrainRange(const MeshData &m, size_t first, size_t count, glm::vec3 o, glm::vec3 d,
                      float &nearest) {
    if (first > m.indices.size() || count > m.indices.size() - first || first % 3 || count % 3)
        return false;
    bool found = false;
    for (size_t i = first; i < first + count; i += 3) {
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
        if (dist >= 0 && dist < nearest) {
            nearest = dist;
            found = true;
        }
    }
    return found;
}

bool PickTerrain(const MeshData &m, glm::vec3 o, glm::vec3 d, glm::vec3 &hit) {
    float nearest = std::numeric_limits<float>::infinity();
    if (!PickTerrainRange(m, 0, m.indices.size(), o, d, nearest))
        return false;
    hit = o + d * nearest;
    return true;
}
