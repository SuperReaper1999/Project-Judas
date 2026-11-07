#include "BuildIdentity.h"
#include "CollisionAsset.h"
#include "authoring/TerrainAuthoring.h"
#include "terrain/TerrainSource.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <stdexcept>

using Clock = std::chrono::steady_clock;
namespace {
double Milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
void Report(unsigned n, const char *name, std::vector<double> times, size_t vertices,
            size_t triangles) {
    double sum = std::accumulate(times.begin(), times.end(), 0.);
    std::sort(times.begin(), times.end());
    std::cout << "{\"grid\":" << n << ",\"path\":\"" << name << "\",\"samples\":" << times.size()
              << ",\"mean_ms\":" << sum / times.size()
              << ",\"median_ms\":" << times[times.size() / 2]
              << ",\"p95_ms\":" << times[size_t(std::ceil(times.size() * .95)) - 1]
              << ",\"worst_ms\":" << times.back() << ",\"prepared_vertices\":" << vertices
              << ",\"prepared_triangles\":" << triangles << "}\n";
}
} // namespace
int main(int argc, char **argv) {
    if (HandleBuildIdentityArguments(argc, argv, "terrain_v2_benchmark"))
        return 0;
    try {
        for (unsigned n : {65u, 129u, 181u}) {
            unsigned cells = n == 181 ? 15 : 16;
            auto old = CreateTerrain(n, n, (n - 1) * .5f, (n - 1) * .5f);
            auto next = TerrainV2::Create(n, n, .5f, cells);
            next.bottom = -4;
            next.top = 12;
            for (unsigned z = 0; z < n; ++z)
                for (unsigned x = 0; x < n; ++x) {
                    float h = 2 + .7f * std::cos(x * .1f) + .8f * std::cos(z * .13f);
                    old.heights[z * n + x] = next.heights[z * n + x] = h;
                }
            std::vector<double> full, dirty;
            size_t vertices = 0, triangles = 0;
            std::string error;
            for (unsigned sample = 0; sample < 30; ++sample) {
                TerrainV2::Edit area;
                area.point = {float(int(sample % 5) - 2), 2, float(int(sample % 3) - 1)};
                area.radius = 1.5f;
                const auto keys = TerrainV2::Affected(next, area);
                const unsigned ix = unsigned(area.point.x / next.spacing + (n - 1) * .5f),
                               iz = unsigned(area.point.z / next.spacing + (n - 1) * .5f);
                old.heights[size_t(iz) * n + ix] += .01f;
                next.heights[size_t(iz) * n + ix] = old.heights[size_t(iz) * n + ix];
                auto before = [&] {
                    auto start = Clock::now();
                    auto mesh = BuildTerrainMesh(old);
                    CollisionAsset cook;
                    if (!CookCollision(mesh, {}, cook, error))
                        throw std::runtime_error(error);
                    full.push_back(Milliseconds(start));
                };
                auto after = [&] {
                    auto start = Clock::now();
                    size_t v = 0, t = 0;
                    for (auto key : keys) {
                        TerrainV2::Chunk chunk;
                        CollisionAsset cook;
                        if (!TerrainV2::BuildChunk(next, key, chunk, error) ||
                            !CookCollision(chunk.mesh, {}, cook, error))
                            throw std::runtime_error(error);
                        v += chunk.mesh.vertices.size();
                        t += chunk.mesh.indices.size() / 3;
                    }
                    vertices = v;
                    triangles = t;
                    dirty.push_back(Milliseconds(start));
                };
                if (sample % 2) {
                    after();
                    before();
                } else {
                    before();
                    after();
                }
            }
            for (unsigned i = 0; i < 100; ++i) {
                const float x = std::sin(i * 1.37f) * (n - 1) * .24f,
                            z = std::cos(i * 2.23f) * (n - 1) * .24f;
                glm::vec3 p;
                if (!TerrainPoint(old, x, z, p) ||
                    std::abs(p.y - TerrainV2::Height(next, x, z)) > 2e-6f)
                    throw std::runtime_error("V1/V2 physical surface agreement");
            }
            Report(n, "V1_full_geometry_and_BVH", full, old.heights.size(),
                   size_t(n - 1) * (n - 1) * 2);
            Report(n, "V2_dirty_geometry_and_BVH", dirty, vertices, triangles);
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
