#include "TerrainCollision.h"
#include "CollisionAsset.h"
#include "collision/CollisionBuild.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace TerrainV2 {
bool CookHeightfieldCollision(const MeshData &mesh, unsigned cells, CollisionAsset &output,
                              std::string &error) {
    try {
        auto require = [](bool ok, const char *text) {
            if (!ok)
                throw std::runtime_error(text);
        };
        const size_t side = cells + 1;
        require(cells >= 4 && cells <= 64 && mesh.vertices.size() == side * side &&
                    mesh.indices.size() == size_t(cells) * cells * 6 &&
                    mesh.sourceVertexIds.empty(),
                "terrain collision requires an identity-free complete 4..64-cell height grid");
        require(mesh.primitives.empty() ||
                    (mesh.primitives.size() == 1 && mesh.primitives[0].first == 0 &&
                     mesh.primitives[0].count == mesh.indices.size()),
                "terrain collision requires the complete grid primitive");
        // A strictly monotone tensor-product X/Z lattice proves distinct
        // positions and no boundary T junctions, regardless of height values.
        for (unsigned z = 0; z <= cells; ++z)
            for (unsigned x = 0; x <= cells; ++x) {
                const auto p = mesh.vertices[z * side + x].position;
                require(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z),
                        "nonfinite terrain grid vertex");
                require(p.x == mesh.vertices[x].position.x &&
                            p.z == mesh.vertices[z * side].position.z &&
                            (!x || p.x > mesh.vertices[x - 1].position.x) &&
                            (!z || p.z > mesh.vertices[(z - 1) * side].position.z),
                        "terrain vertices are not a monotone tensor-product X/Z grid");
            }
        CollisionAsset next;
        next.vertices.reserve(mesh.vertices.size());
        next.faces.reserve(size_t(cells) * cells * 2);
        std::vector<unsigned> remap(mesh.vertices.size(), UINT32_MAX);
        for (unsigned z = 0; z < cells; ++z)
            for (unsigned x = 0; x < cells; ++x) {
                const unsigned a = z * side + x, b = a + 1, c = a + side, d = c + 1;
                const std::array<unsigned, 6> pattern{a, c, b, b, c, d};
                const auto first = (z * cells + x) * 6;
                for (unsigned triangle = 0; triangle < 2; ++triangle) {
                    CollisionFace face;
                    face.source = first / 3 + triangle;
                    for (unsigned corner = 0; corner < 3; ++corner) {
                        const auto index = pattern[triangle * 3 + corner];
                        require(mesh.indices[first + triangle * 3 + corner] == index,
                                "terrain grid triangle pattern or winding changed");
                        auto &mapped = remap[index];
                        if (mapped == UINT32_MAX) {
                            mapped = unsigned(next.vertices.size());
                            // Keep the generic cook's exact conversion and first-use order.
                            next.vertices.push_back(glm::dvec3(
                                glm::dmat4(1) * glm::dvec4(mesh.vertices[index].position, 1)));
                        }
                        face.vertices[corner] = mapped;
                    }
                    const auto p = next.vertices[face.vertices[0]],
                               q = next.vertices[face.vertices[1]],
                               r = next.vertices[face.vertices[2]];
                    require(glm::length(glm::cross(q - p, r - p)) > 1e-12,
                            "degenerate terrain grid triangle");
                    face.normal = glm::normalize(glm::cross(q - p, r - p));
                    next.faces.push_back(face);
                }
                const int f = int(first / 3);
                next.faces[f].adjacent = {x ? f - 1 : -1, f + 1, z ? f - 2 * int(cells) + 1 : -1};
                next.faces[f + 1].adjacent = {f, z + 1 < cells ? f + 2 * int(cells) : -1,
                                              x + 1 < cells ? f + 2 : -1};
            }
        for (unsigned f = 0; f < next.faces.size(); ++f)
            for (unsigned e = 0; e < 3; ++e) {
                auto &face = next.faces[f];
                const int g = face.adjacent[e];
                if (g < 0 || unsigned(g) < f)
                    continue;
                auto &other = next.faces[g];
                unsigned j = 0;
                while (j < 3 && other.adjacent[j] != int(f))
                    ++j;
                require(j < 3 && face.vertices[e] == other.vertices[(j + 1) % 3] &&
                            face.vertices[(e + 1) % 3] == other.vertices[j],
                        "terrain grid adjacency is inconsistent");
                const auto third = next.vertices[other.vertices[(j + 2) % 3]];
                const double planeSide =
                    glm::dot(face.normal, third - next.vertices[face.vertices[0]]);
                const bool active =
                    planeSide < -1e-9 && glm::dot(face.normal, other.normal) < 1 - 1e-10;
                face.active[e] = other.active[j] = active;
            }
        JudasCollisionDetail::BuildTree(next);
        output = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
} // namespace TerrainV2
