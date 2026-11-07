#include "GameWaterSurface.h"
#include <algorithm>
namespace JudasWater {
MeshData BuildPoolSurface(const Grid &grid) {
    MeshData mesh;
    const auto s = grid.Configuration();
    const float h = s.cellSize;
    const int nx = s.cells.x, nz = s.cells.z;
    std::vector<float> heights(nx * nz, -1), corners((nx + 1) * (nz + 1), 0);
    for (int z = 0; z < nz; ++z)
        for (int x = 0; x < nx; ++x)
            grid.Surface(x, z, heights[z * nx + x]);
    for (int z = 0; z <= nz; ++z)
        for (int x = 0; x <= nx; ++x) {
            float sum = 0;
            int count = 0;
            for (int b = z - 1; b <= z; ++b)
                for (int a = x - 1; a <= x; ++a)
                    if (a >= 0 && b >= 0 && a < nx && b < nz && heights[b * nx + a] >= 0) {
                        sum += heights[b * nx + a];
                        ++count;
                    }
            corners[z * (nx + 1) + x] = count ? sum / count : 0;
        }
    auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
        const auto normal = s.rotation * glm::normalize(glm::cross(b - a, c - a));
        for (auto p : {a, b, c, a, c, d}) {
            // Presentation-only inset: avoid coplanar depth fighting against
            // the inside of the tank. Physical volume and collisions stay put.
            p.x = glm::clamp(p.x, h * .002f, nx * h - h * .002f);
            p.z = glm::clamp(p.z, h * .002f, nz * h - h * .002f);
            mesh.vertices.push_back({grid.ToWorld(p), normal, {p.x, p.z}});
        }
    };
    for (int z = 0; z < nz; ++z)
        for (int x = 0; x < nx; ++x)
            if (heights[z * nx + x] >= 0) {
                auto point = [&](int a, int b) {
                    return glm::vec3(a * h, corners[b * (nx + 1) + a], b * h);
                };
                quad(point(x, z), point(x, z + 1), point(x + 1, z + 1), point(x + 1, z));
                if (x == 0)
                    quad({0, 0, z * h}, {0, 0, (z + 1) * h}, point(x, z + 1), point(x, z));
                if (x == nx - 1)
                    quad({nx * h, 0, (z + 1) * h}, {nx * h, 0, z * h}, point(x + 1, z),
                         point(x + 1, z + 1));
                if (z == 0)
                    quad({(x + 1) * h, 0, 0}, {x * h, 0, 0}, point(x, z), point(x + 1, z));
                if (z == nz - 1)
                    quad({x * h, 0, nz * h}, {(x + 1) * h, 0, nz * h}, point(x + 1, z + 1),
                         point(x, z + 1));
            }
    return mesh;
}
} // namespace JudasWater
