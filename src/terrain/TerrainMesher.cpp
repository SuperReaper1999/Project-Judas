#include "TerrainMesher.h"
#include "TerrainTopology.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace TerrainV2 {
namespace {
struct Field {
    const Source &source;
    std::vector<const Edit *> edits;
    std::vector<glm::dvec3> physicalCenters;
    double operator()(glm::vec3 p) const {
        double d = double(p.y) - double(Height(source, p.x, p.z));
        if (source.chart) {
            const auto physical = source.chart->LocalPoint(p);
            for (size_t i = 0; i < edits.size(); ++i) {
                const auto *e = edits[i];
                const double sphere = glm::length(physical - physicalCenters[i]) - e->radius;
                d = e->kind == Edit::Kind::Remove ? std::max(d, -sphere) : std::min(d, sphere);
            }
        } else for (const auto *e : edits) {
            const double sphere = glm::length(glm::dvec3(p) - glm::dvec3(e->point)) - double(e->radius);
            d = e->kind == Edit::Kind::Remove ? std::max(d, -sphere) : std::min(d, sphere);
        }
        return d;
    }
};
glm::vec3 Normal(const Field &field, glm::vec3 p) {
    const float h = field.source.spacing * .5f;
    glm::vec3 g;
    for (unsigned a = 0; a < 3; ++a) {
        auto lo = p, hi = p;
        lo[a] -= h;
        hi[a] += h;
        g[a] = field(hi) - field(lo);
    }
    return glm::length(g) > 1e-8f ? glm::normalize(g) : glm::vec3(0, 1, 0);
}
MeshVertex Vertex(const Field &field, glm::vec3 p, glm::vec3 anchor) {
    const auto &s = field.source;
    MeshVertex v;
    v.position = p - anchor;
    v.normal = Normal(field, p);
    v.uv = {p.x * .25f, p.z * .25f};
    v.uv1 = {p.x / ((s.samplesX - 1) * s.spacing) + .5f,
             p.z / ((s.samplesZ - 1) * s.spacing) + .5f};
    auto tangent = glm::cross(glm::vec3(0, 0, 1), v.normal);
    if (glm::length(tangent) < 1e-6f)
        tangent = glm::cross(glm::vec3(0, 1, 0), v.normal);
    v.tangent = glm::vec4(glm::normalize(tangent), 1);
    return v;
}
} // namespace
static bool ExtractChunk(const Source &input, ChunkKey key, Chunk &output, std::string &error,
                         const std::function<bool()> &cancelled) {
    try {
        if (key.x >= input.ChunksX() || key.z >= input.ChunksZ())
            throw std::invalid_argument("terrain chunk outside source");
        const Source &s = input;
        Field field{input, {}, {}};
        const unsigned n = s.chunkCells;
        const float width = n * s.spacing;
        const glm::vec3 anchor(key.x * width - (s.samplesX - 1) * s.spacing * .5f, 0,
                               key.z * width - (s.samplesZ - 1) * s.spacing * .5f);
        for (const auto &e : input.edits) {
            if (Touches(input, key, e))
                field.edits.push_back(&e);
        }
        if (s.chart)
            for (const auto *edit : field.edits)
                field.physicalCenters.push_back(s.chart->LocalPoint(edit->point));
        Chunk chunk;
        chunk.key = key;
        chunk.anchor = anchor;
        chunk.volumetric = !field.edits.empty();
        auto &mesh = chunk.mesh;
        auto require = [&](bool ok, const char *reason) {
            if (!ok)
                throw std::runtime_error(reason);
        };
        auto triangle = [&](unsigned a, unsigned b, unsigned c, glm::vec3 outward) {
            auto normal = glm::cross(mesh.vertices[b].position - mesh.vertices[a].position,
                                     mesh.vertices[c].position - mesh.vertices[a].position);
            if (glm::dot(normal, normal) < 1e-16f)
                return;
            if (glm::dot(normal, outward) < 0)
                std::swap(b, c);
            require(mesh.indices.size() < 393216, "terrain chunk triangle budget");
            mesh.indices.insert(mesh.indices.end(), {a, b, c});
        };
        // Volume work follows edit bands, never the entire elevation range of
        // a mountain. The untouched heightfield remains outside those bands.
        std::vector<std::pair<int, int>> bands;
        for (const auto *e : field.edits) {
            int lo = int(std::floor((e->point.y - e->radius) / s.spacing)) - 1;
            int hi = int(std::ceil((e->point.y + e->radius) / s.spacing)) + 1;
            lo = std::max(lo, int(std::floor(s.bottom / s.spacing)));
            hi = std::min(hi, int(std::ceil(s.top / s.spacing)));
            if (hi > lo)
                bands.emplace_back(lo, hi);
        }
        std::sort(bands.begin(), bands.end());
        std::vector<std::pair<int, int>> merged;
        for (auto band : bands) {
            if (!merged.empty() && band.first <= merged.back().second)
                merged.back().second = std::max(merged.back().second, band.second);
            else
                merged.push_back(band);
        }
        unsigned layers = 0;
        for (auto band : merged)
            layers += unsigned(band.second - band.first);
        require(layers <= 512,
                "terrain edited vertical-band budget exceeded; use coarser source spacing");
        if (!chunk.volumetric || merged.empty()) {
            for (unsigned z = 0; z <= n; ++z)
                for (unsigned x = 0; x <= n; ++x) {
                    auto p = anchor + glm::vec3(x * s.spacing, 0, z * s.spacing);
                    p.y = Height(s, p.x, p.z);
                    mesh.vertices.push_back(Vertex(field, p, anchor));
                }
            for (unsigned z = 0; z < n; ++z)
                for (unsigned x = 0; x < n; ++x) {
                    unsigned a = z * (n + 1) + x, b = a + 1, c = a + n + 1, d = c + 1;
                    triangle(a, c, b, {0, 1, 0});
                    triangle(b, c, d, {0, 1, 0});
                }
        } else {
            auto clip = [](const std::vector<glm::vec3> &polygon, float y, bool keepBelow) {
                std::vector<glm::vec3> out;
                if (polygon.empty())
                    return out;
                for (size_t i = 0; i < polygon.size(); ++i) {
                    auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
                    const bool inA = keepBelow ? a.y <= y : a.y > y,
                               inB = keepBelow ? b.y <= y : b.y > y;
                    if (inA)
                        out.push_back(a);
                    if (inA != inB)
                        out.push_back(glm::vec3(glm::mix(glm::dvec3(a), glm::dvec3(b),
                                                         (double(y) - a.y) / (double(b.y) - a.y))));
                }
                return out;
            };
            for (unsigned z = 0; z < n; ++z)
                for (unsigned x = 0; x < n; ++x) {
                    glm::vec3 p[4];
                    for (unsigned k = 0; k < 4; ++k) {
                        p[k] = anchor +
                               glm::vec3((x + (k & 1)) * s.spacing, 0, (z + (k >> 1)) * s.spacing);
                        p[k].y = Height(s, p[k].x, p[k].z);
                    }
                    for (auto corners :
                         {std::array<unsigned, 3>{0, 2, 1}, std::array<unsigned, 3>{1, 2, 3}}) {
                        std::vector<std::vector<glm::vec3>> polygons{
                            {p[corners[0]], p[corners[1]], p[corners[2]]}};
                        for (auto band : merged) {
                            std::vector<std::vector<glm::vec3>> outside;
                            for (const auto &polygon : polygons) {
                                auto lower = clip(polygon, band.first * s.spacing, true),
                                     upper = clip(polygon, band.second * s.spacing, false);
                                if (lower.size() >= 3)
                                    outside.push_back(std::move(lower));
                                if (upper.size() >= 3)
                                    outside.push_back(std::move(upper));
                            }
                            polygons = std::move(outside);
                        }
                        for (const auto &polygon : polygons) {
                            const unsigned first = unsigned(mesh.vertices.size());
                            for (auto p : polygon)
                                mesh.vertices.push_back(Vertex(field, p, anchor));
                            for (unsigned i = 1; i + 1 < polygon.size(); ++i)
                                triangle(first, first + i, first + i + 1, {0, 1, 0});
                        }
                    }
                }
            for (auto band : merged)
                for (int yBegin = band.first; yBegin < band.second; yBegin += 32) {
                    const unsigned ny = unsigned(std::min(32, band.second - yBegin));
                    const float dy = s.spacing;
                    const unsigned nx = n + 1, nz = n + 1;
                    std::vector<double> values(size_t(nx) * (ny + 1) * nz);
                    auto index = [&](unsigned x, unsigned y, unsigned z) {
                        return uint32_t((size_t(z) * (ny + 1) + y) * nx + x);
                    };
                    auto point = [&](uint32_t i) {
                        unsigned x = i % nx, y = (i / nx) % (ny + 1), z = i / (nx * (ny + 1));
                        return anchor +
                               glm::vec3(x * s.spacing, float(yBegin + int(y)) * dy, z * s.spacing);
                    };
                    for (unsigned z = 0; z < nz; ++z) {
                        require(!cancelled || !cancelled(), "terrain meshing cancelled");
                        for (unsigned y = 0; y <= ny; ++y)
                            for (unsigned x = 0; x < nx; ++x) {
                                auto i = index(x, y, z);
                                values[i] = field(point(i));
                            }
                    }
                    chunk.sampledBytes =
                        std::max(chunk.sampledBytes, values.size() * sizeof(double));
                    std::unordered_map<uint64_t, unsigned> edges;
                    auto vertex = [&](uint32_t a, uint32_t b) {
                        if (a > b)
                            std::swap(a, b);
                        uint64_t edge = (uint64_t(a) << 32) | b;
                        auto it = edges.find(edge);
                        if (it != edges.end())
                            return it->second;
                        const double t = values[a] / (values[a] - values[b]);
                        auto p = glm::vec3(glm::mix(glm::dvec3(point(a)), glm::dvec3(point(b)), t));
                        const unsigned result = unsigned(mesh.vertices.size());
                        mesh.vertices.push_back(Vertex(field, p, anchor));
                        edges.emplace(edge, result);
                        return result;
                    };
                    constexpr unsigned tetrahedra[6][4] = {{1, 0, 2, 6}, {1, 2, 3, 6},
                                                           {1, 3, 7, 6}, {1, 7, 5, 6},
                                                           {1, 5, 4, 6}, {1, 4, 0, 6}};
                    for (unsigned z = 0; z < n; ++z) {
                        require(!cancelled || !cancelled(), "terrain meshing cancelled");
                        for (unsigned y = 0; y < ny; ++y)
                            for (unsigned x = 0; x < n; ++x) {
                                uint32_t cube[8];
                                for (unsigned k = 0; k < 8; ++k)
                                    cube[k] =
                                        index(x + (k & 1), y + ((k >> 1) & 1), z + ((k >> 2) & 1));
                                for (const auto &tet : tetrahedra) {
                                    uint32_t solid[4], air[4];
                                    unsigned ns = 0, na = 0;
                                    glm::vec3 ps(0), pa(0);
                                    for (auto k : tet) {
                                        auto i = cube[k];
                                        if (values[i] < 0) {
                                            solid[ns++] = i;
                                            ps += point(i);
                                        } else {
                                            air[na++] = i;
                                            pa += point(i);
                                        }
                                    }
                                    if (!ns || !na)
                                        continue;
                                    const auto outward = pa / float(na) - ps / float(ns);
                                    if (ns == 1)
                                        triangle(vertex(solid[0], air[0]), vertex(solid[0], air[1]),
                                                 vertex(solid[0], air[2]), outward);
                                    else if (ns == 3)
                                        triangle(vertex(air[0], solid[0]), vertex(air[0], solid[1]),
                                                 vertex(air[0], solid[2]), outward);
                                    else {
                                        auto a = vertex(solid[0], air[0]),
                                             b = vertex(solid[0], air[1]),
                                             c = vertex(solid[1], air[0]),
                                             d = vertex(solid[1], air[1]);
                                        triangle(a, b, d, outward);
                                        triangle(a, d, c, outward);
                                    }
                                }
                            }
                    }
                }
        }
        if (chunk.volumetric)
            ConformChunkTopology(mesh, s.spacing);
        if (s.chart && !ProjectPlanetChunk(*s.chart, chunk, error))
            return false;
        if (!mesh.indices.empty())
            mesh.primitives.emplace_back(0, unsigned(mesh.indices.size()), -1, "terrain");
        output = std::move(chunk);
        error.clear();
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool BuildChunk(const Source &source, ChunkKey key, Chunk &output, std::string &error,
                const std::function<bool()> &cancelled) {
    return Validate(source, error) && ExtractChunk(source, key, output, error, cancelled);
}
bool BuildChunks(const Source &source, const std::vector<ChunkKey> &keys,
                 std::vector<Chunk> &output, std::string &error,
                 const std::function<bool()> &cancelled, size_t maximumTriangles) {
    if (!Validate(source, error))
        return false;
    std::vector<Chunk> prepared;
    size_t triangles = 0;
    prepared.reserve(keys.size());
    for (auto key : keys) {
        if (cancelled && cancelled()) {
            error = "terrain preparation cancelled";
            return false;
        }
        Chunk chunk;
        if (!ExtractChunk(source, key, chunk, error, cancelled))
            return false;
        const size_t count = chunk.mesh.indices.size() / 3;
        if (count > maximumTriangles - triangles) {
            error = "terrain preparation exceeds its triangle budget; previous product retained";
            return false;
        }
        triangles += count;
        prepared.push_back(std::move(chunk));
    }
    output = std::move(prepared);
    error.clear();
    return true;
}
} // namespace TerrainV2
