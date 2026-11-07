#include "TerrainTopology.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace TerrainV2 {
void ConformChunkTopology(MeshData &mesh, float binWidth) {
    using Position = std::tuple<float, float, float>;
    std::map<Position, unsigned> positions;
    std::vector<unsigned> remap(mesh.vertices.size());
    std::vector<MeshVertex> vertices;
    for (unsigned i = 0; i < mesh.vertices.size(); ++i) {
        const auto p = mesh.vertices[i].position;
        auto [it, inserted] = positions.emplace(Position(p.x, p.y, p.z), unsigned(vertices.size()));
        if (inserted)
            vertices.push_back(mesh.vertices[i]);
        remap[i] = it->second;
    }
    for (auto &i : mesh.indices)
        i = remap[i];
    mesh.vertices = std::move(vertices);
    using Bin = std::pair<int, int>;
    std::map<Bin, std::vector<unsigned>> bins;
    auto bin = [&](glm::vec3 p) {
        return Bin(int(std::floor(p.x / binWidth)), int(std::floor(p.z / binWidth)));
    };
    for (unsigned i = 0; i < mesh.vertices.size(); ++i)
        bins[bin(mesh.vertices[i].position)].push_back(i);
    std::map<std::pair<unsigned, unsigned>, std::vector<unsigned>> cuts;
    auto edgeCuts = [&](unsigned a, unsigned b) -> const std::vector<unsigned> & {
        auto key = std::minmax(a, b);
        auto found = cuts.find(key);
        if (found != cuts.end())
            return found->second;
        const glm::dvec3 p = mesh.vertices[key.first].position,
                         q = mesh.vertices[key.second].position, d = q - p;
        const double length = glm::dot(d, d);
        std::vector<std::pair<double, unsigned>> sorted;
        const auto lo = bin(glm::vec3(glm::min(p, q))), hi = bin(glm::vec3(glm::max(p, q)));
        if (length > 1e-20)
            for (int z = lo.second - 1; z <= hi.second + 1; ++z)
                for (int x = lo.first - 1; x <= hi.first + 1; ++x) {
                    auto candidates = bins.find({x, z});
                    if (candidates == bins.end())
                        continue;
                    for (auto i : candidates->second) {
                        if (i == key.first || i == key.second)
                            continue;
                        const glm::dvec3 offset = glm::dvec3(mesh.vertices[i].position) - p;
                        const double t = glm::dot(offset, d) / length;
                        if (t > 1e-9 && t < 1 - 1e-9 && glm::length(offset - t * d) < 1e-9)
                            sorted.emplace_back(t, i);
                    }
                }
        std::sort(sorted.begin(), sorted.end());
        std::vector<unsigned> out;
        for (auto [_, i] : sorted)
            out.push_back(i);
        return cuts.emplace(key, std::move(out)).first->second;
    };
    std::vector<uint32_t> result;
    result.reserve(mesh.indices.size());
    for (size_t t = 0; t < mesh.indices.size(); t += 3) {
        unsigned ids[3] = {mesh.indices[t], mesh.indices[t + 1], mesh.indices[t + 2]};
        std::vector<unsigned> boundary;
        for (unsigned e = 0; e < 3; ++e) {
            auto a = ids[e], b = ids[(e + 1) % 3];
            boundary.push_back(a);
            const auto &split = edgeCuts(a, b);
            if (a < b)
                boundary.insert(boundary.end(), split.begin(), split.end());
            else
                boundary.insert(boundary.end(), split.rbegin(), split.rend());
        }
        if (boundary.size() == 3) {
            result.insert(result.end(), {ids[0], ids[1], ids[2]});
            continue;
        }
        MeshVertex center;
        const auto &a = mesh.vertices[ids[0]];
        const auto &b = mesh.vertices[ids[1]];
        const auto &c = mesh.vertices[ids[2]];
        center.position = (a.position + b.position + c.position) / 3.f;
        center.normal = glm::normalize(a.normal + b.normal + c.normal);
        center.uv = (a.uv + b.uv + c.uv) / 3.f;
        center.uv1 = (a.uv1 + b.uv1 + c.uv1) / 3.f;
        center.tangent = a.tangent;
        unsigned pivot = unsigned(mesh.vertices.size());
        mesh.vertices.push_back(center);
        for (unsigned i = 0; i < boundary.size(); ++i)
            result.insert(result.end(), {pivot, boundary[i], boundary[(i + 1) % boundary.size()]});
    }
    mesh.indices = std::move(result);
}
} // namespace TerrainV2
