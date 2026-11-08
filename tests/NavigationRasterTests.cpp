#include "NavigationAsset.h"
#include "SceneFingerprint.h"
#include "navigation/NavigationRasterBins.h"
#include "terrain/TerrainMesher.h"
#include <DetourNavMesh.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>

bool ReferenceBakeNavigationGeometry(const NavigationGeometry &, const NavigationSurfaceSettings &,
                                     const NavigationProfile &, NavigationData &, std::string &,
                                     bool, const std::function<bool()> &);
namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
NavigationGeometry Terrain(bool cave, unsigned samples = 33) {
    const bool large = samples > 33;
    auto source = TerrainV2::Create(samples, samples, large ? 2 : 1);
    source.bottom = large ? -256 : -8;
    source.top = large ? 256 : 12;
    for (unsigned z = 0; z < samples; ++z)
        for (unsigned x = 0; x < samples; ++x)
            source.heights[z * samples + x] =
                large ? 30 + 12 * std::sin(float(x) * .035f) * std::cos(float(z) * .027f)
                      : 2 + std::sin(x * .2f) * std::cos(z * .3f);
    if (cave)
        for (float z = -6; z <= 6; z += 1)
            source.edits.push_back({TerrainV2::Edit::Kind::Remove, {0, 1, z}, 2});
    std::vector<TerrainV2::ChunkKey> keys;
    for (unsigned z = 0; z < source.ChunksZ(); ++z)
        for (unsigned x = 0; x < source.ChunksX(); ++x)
            keys.push_back({x, z});
    std::vector<TerrainV2::Chunk> chunks;
    std::string error;
    Check(TerrainV2::BuildChunks(source, keys, chunks, error),
          "prepare real physical terrain fixture");
    NavigationGeometry geometry;
    geometry.fingerprint = SceneFingerprintSha256(cave ? "cave" : "heightfield");
    for (const auto &chunk : chunks) {
        const auto offset = int(geometry.vertices.size() / 3);
        for (const auto &vertex : chunk.mesh.vertices) {
            const auto point = vertex.position + chunk.anchor;
            geometry.vertices.insert(geometry.vertices.end(), {point.x, point.y, point.z});
        }
        for (auto index : chunk.mesh.indices)
            geometry.triangles.push_back(offset + int(index));
    }
    return geometry;
}
void Membership(float origin) {
    constexpr float cell = .23f;
    constexpr int size = 16, border = 5, nx = 5, nz = 4;
    const glm::vec3 minimum(origin - 15.1f, -8, origin - 16.7f);
    NavigationGeometry geometry;
    std::mt19937 random(887);
    std::uniform_real_distribution<float> offset(-40, 40);
    for (unsigned triangle = 0; triangle < 300; ++triangle) {
        float x = origin + offset(random), z = origin + offset(random);
        // Include exact and one-ULP tangent cases on nonbinary tile boundaries.
        if (triangle < 90) {
            const float edge = minimum.x + (int(triangle % nx) * size - border) * cell;
            x = triangle % 3 == 0 ? edge
                                  : std::nextafter(edge, triangle % 3 == 1 ? INFINITY : -INFINITY);
        }
        for (auto point :
             {glm::vec3(x, 0, z), glm::vec3(x, 1, z + .75f), glm::vec3(x + .5f, 0, z)}) {
            geometry.triangles.push_back(int(geometry.vertices.size() / 3));
            geometry.vertices.insert(geometry.vertices.end(), {point.x, point.y, point.z});
        }
    }
    std::vector<unsigned char> areas(300, 1);
    NavigationRasterBins bins;
    std::string error;
    Check(bins.Build(geometry, areas, minimum, cell, size, border, nx, nz, error),
          "build conservative bins");
    for (int z = 0; z < nz; ++z)
        for (int x = 0; x < nx; ++x) {
            const float x0 = minimum.x + (x * size - border) * cell,
                        x1 = minimum.x + ((x + 1) * size + border) * cell,
                        z0 = minimum.z + (z * size - border) * cell,
                        z1 = minimum.z + ((z + 1) * size + border) * cell;
            std::vector<int> expected;
            for (unsigned t = 0; t < 300; ++t) {
                glm::vec2 lo(INFINITY), hi(-INFINITY);
                for (unsigned i = 0; i < 3; ++i) {
                    const auto at = size_t(geometry.triangles[t * 3 + i]) * 3;
                    glm::vec2 point(geometry.vertices[at], geometry.vertices[at + 2]);
                    lo = glm::min(lo, point);
                    hi = glm::max(hi, point);
                }
                if (lo.x <= x1 && hi.x >= x0 && lo.y <= z1 && hi.y >= z0)
                    expected.insert(expected.end(), geometry.triangles.begin() + t * 3,
                                    geometry.triangles.begin() + t * 3 + 3);
            }
            Check(bins.At(x, z).indices == expected,
                  "bins preserve exact inclusive membership and source order");
        }
    Check(bins.Build(geometry, areas, minimum, cell, size, border, nx, nz, error, 1) &&
              bins.FullScan(),
          "index budget overflow explicitly retains the whole original raster scan");
}
} // namespace
int main(int argc, char **argv) {
    try {
        Check(sizeof(dtPolyRef) == 8,
              "engine and native consumers share supported 64-bit Detour references");
        if (argc == 2 && std::string(argv[1]) == "--full") {
            const auto geometry = Terrain(false, 513);
            NavigationSurfaceSettings settings;
            settings.halfExtents = {512, 64, 512};
            settings.cellSize = 1;
            settings.minRegion = 0;
            for (unsigned pair = 0; pair < 5; ++pair) {
                NavigationData before, after;
                std::string error;
                double referenceMs = 0, indexedMs = 0;
                auto run = [&](bool reference) {
                    const auto start = std::chrono::steady_clock::now();
                    const bool ok =
                        reference
                            ? ReferenceBakeNavigationGeometry(geometry, settings, {}, before, error,
                                                              true, {})
                            : BakeNavigationGeometry(geometry, settings, {}, after, error, true);
                    const auto ms = std::chrono::duration<double, std::milli>(
                                        std::chrono::steady_clock::now() - start)
                                        .count();
                    Check(ok, "full kilometre physical terrain bake: " + error);
                    (reference ? referenceMs : indexedMs) = ms;
                };
                run(pair % 2 == 0);
                run(pair % 2 != 0);
                Check(before.layers == after.layers,
                      "full kilometre native output bytes exactly preserved");
                std::cout << "{\"pair\":" << pair << ",\"reference_ms\":" << referenceMs
                          << ",\"indexed_ms\":" << indexedMs
                          << ",\"triangles\":" << geometry.triangles.size() / 3
                          << ",\"layers\":" << after.layers.size() << "}" << std::endl;
            }
            std::cout << "SUMMARY " << checks << " checks 0 failures\n";
            return 0;
        }
        Membership(0);
        Membership(32768);
        for (bool cave : {false, true}) {
            auto geometry = Terrain(cave);
            for (float cell : {.2f, .37f, .5f}) {
                NavigationSurfaceSettings settings;
                settings.halfExtents = {17.1f, 12, 17.7f};
                settings.cellSize = cell;
                settings.minRegion = 0;
                NavigationGeometry::Volume modifier;
                modifier.blocked = true;
                modifier.bottom = -8;
                modifier.top = 10;
                modifier.polygon = {3, 0, -5, 3, 0, 5, 5, 0, 5, 5, 0, -5};
                geometry.modifiers = {modifier};
                NavigationData reference, candidate;
                std::string oldError, error;
                const auto start = std::chrono::steady_clock::now();
                const bool old = ReferenceBakeNavigationGeometry(geometry, settings, {}, reference,
                                                                 oldError, true, {});
                const auto middle = std::chrono::steady_clock::now();
                const bool fresh =
                    BakeNavigationGeometry(geometry, settings, {}, candidate, error, true);
                const auto end = std::chrono::steady_clock::now();
                Check(old && fresh,
                      "both complete physical operators bake fixture: " + oldError + " / " + error);
                Check(candidate.layers == reference.layers &&
                          candidate.fingerprint == reference.fingerprint,
                      "all native compressed height/layer bytes exactly match the full-scan "
                      "reference");
                std::cout << "{\"cave\":" << cave << ",\"cell\":" << cell << ",\"reference_ms\":"
                          << std::chrono::duration<double, std::milli>(middle - start).count()
                          << ",\"indexed_ms\":"
                          << std::chrono::duration<double, std::milli>(end - middle).count()
                          << ",\"layers\":" << candidate.layers.size() << "}\n";
            }
        }
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
