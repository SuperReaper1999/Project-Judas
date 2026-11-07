#include "GameWaterSurface.h"
#include <algorithm>
#include <cmath>

namespace JudasWater {
MeshData BuildPoolSurface(const Grid &grid) {
    MeshData mesh;
    const auto &s = grid.Configuration();
    const float h = s.cellSize;
    // Each resolved wet run owns a surface. Do not bridge a cave ceiling or
    // connect vertically separated pools just because they share an XZ column.
    const auto &cells = grid.Cells();
    const auto n = s.cells;
    std::vector<float> levels(cells.size(), -1);
    std::vector<bool> top(cells.size());
    auto wet = [&](int i) {
        return i >= 0 &&
               cells[i].water > std::max(1e-7 * std::pow(h, 3),
                                         double(s.minimumMobileFraction) * cells[i].capacity);
    };
    for (int i = 0; i < int(cells.size()); ++i)
        if (wet(i)) {
            const auto at = grid.Coordinates(i);
            grid.CellSurface(i, levels[i]);
            top[i] =
                !wet(grid.Index(at.x, at.y + 1, at.z)) || levels[i] < (at.y + 1) * h - h * 1e-4f;
        }
    auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
        const auto normal = s.rotation * glm::normalize(glm::cross(b - a, c - a));
        for (auto p : {a, b, c, a, c, d}) {
            p.x = glm::clamp(p.x, h * .002f, n.x * h - h * .002f);
            p.z = glm::clamp(p.z, h * .002f, n.z * h - h * .002f);
            mesh.vertices.push_back({grid.ToWorld(p), normal, {p.x, p.z}});
        }
    };
    for (int y = 0; y < n.y; ++y)
        for (int z = 0; z < n.z; ++z)
            for (int x = 0; x < n.x; ++x) {
                const int i = grid.Index(x, y, z);
                if (levels[i] < 0)
                    continue;
                auto corner = [&](int a, int b) {
                    float sum = 0;
                    unsigned count = 0;
                    for (int dz = -1; dz <= 0; ++dz)
                        for (int dx = -1; dx <= 0; ++dx) {
                            const int j = grid.Index(a + dx, y, b + dz);
                            if (j >= 0 && levels[j] >= 0 && top[j] == top[i]) {
                                sum += levels[j];
                                ++count;
                            }
                        }
                    return glm::vec3(a * h, count ? sum / count : levels[i], b * h);
                };
                const auto a = corner(x, z), b = corner(x, z + 1), c = corner(x + 1, z + 1),
                           d = corner(x + 1, z);
                if (top[i])
                    quad(a, b, c, d);
                auto skirt = [&](int neighbour, glm::vec3 from, glm::vec3 to) {
                    if (wet(neighbour))
                        return;
                    const float bottom = y * h;
                    if (std::max(from.y, to.y) - bottom < h * 1e-5f)
                        return;
                    quad({to.x, bottom, to.z}, {from.x, bottom, from.z}, from, to);
                };
                skirt(grid.Index(x - 1, y, z), a, b);
                skirt(grid.Index(x + 1, y, z), c, d);
                skirt(grid.Index(x, y, z - 1), d, a);
                skirt(grid.Index(x, y, z + 1), b, c);
            }
    return mesh;
}
} // namespace JudasWater
