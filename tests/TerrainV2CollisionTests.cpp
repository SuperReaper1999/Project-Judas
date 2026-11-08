#include "CollisionAsset.h"
#include "terrain/TerrainCollision.h"
#include "terrain/TerrainLod.h"
#include "terrain/TerrainMesher.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
}
std::string Read(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
} // namespace
int main() {
    try {
        namespace fs = std::filesystem;
        const auto root =
            fs::temp_directory_path() /
            ("terrain-grid-collision-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
        std::string error;
        for (unsigned cells : {4u, 5u, 8u, 16u, 24u, 32u, 63u, 64u})
            for (float spacing : {.125f, .3f, 2.f, 128.f}) {
                auto source = TerrainV2::Create(cells * 2 + 1, cells * 2 + 1, spacing, cells);
                for (unsigned z = 0; z < source.samplesZ; ++z)
                    for (unsigned x = 0; x < source.samplesX; ++x)
                        source.heights[size_t(z) * source.samplesX + x] =
                            3 * std::sin(x * .21f) + 2 * std::cos(z * .17f) +
                            float(int((x * 73856093u ^ z * 19349663u) % 101) - 50) * .01f;
                for (auto key : {TerrainV2::ChunkKey{0, 0}, TerrainV2::ChunkKey{1, 1}}) {
                    TerrainV2::Chunk chunk;
                    Check(TerrainV2::BuildChunk(source, key, chunk, error),
                          "build independent terrain grid: " + error);
                    CollisionAsset generic, structured;
                    const auto start = std::chrono::steady_clock::now();
                    Check(CookCollision(chunk.mesh, {}, generic, error),
                          "generic collision reference: " + error);
                    const auto middle = std::chrono::steady_clock::now();
                    Check(TerrainV2::CookHeightfieldCollision(chunk.mesh, cells, structured, error),
                          "structured grid collision: " + error);
                    const auto end = std::chrono::steady_clock::now();
                    Check(SaveCollisionAsset((root / "generic").string(), generic, error) &&
                              SaveCollisionAsset((root / "structured").string(), structured, error),
                          "serialize complete physical representations");
                    Check(Read(root / "generic") == Read(root / "structured") &&
                              generic.satEdges == structured.satEdges &&
                              generic.warnings == structured.warnings,
                          "all vertices, features, normals, adjacency, active edges, BVH/order and "
                          "bounds match byte exactly");
                    std::cout << "{\"cells\":" << cells << ",\"spacing\":" << spacing
                              << ",\"generic_ms\":"
                              << std::chrono::duration<double, std::milli>(middle - start).count()
                              << ",\"structured_ms\":"
                              << std::chrono::duration<double, std::milli>(end - middle).count()
                              << "}\n";
                }
            }
        auto mesh = TerrainV2::HeightGrid(4);
        CollisionAsset output;
        Check(CookCollision(mesh, {}, output, error), "negative control destination");
        Check(SaveCollisionAsset((root / "before").string(), output, error),
              "serialize old destination");
        for (unsigned fault = 0; fault < 8; ++fault) {
            auto bad = mesh;
            unsigned cells = 4;
            if (fault == 0)
                bad.indices[0] = 9000;
            if (fault == 1)
                bad.vertices[3].position.x = bad.vertices[2].position.x;
            if (fault == 2)
                bad.vertices[6].position.x += .1f;
            if (fault == 3)
                bad.vertices[6].position.y = NAN;
            if (fault == 4)
                bad.indices.resize(bad.indices.size() - 3);
            if (fault == 5)
                bad.sourceVertexIds.resize(bad.vertices.size());
            if (fault == 6)
                cells = 3;
            if (fault == 7)
                bad.primitives.emplace_back(3, unsigned(bad.indices.size()) - 3, -1, "partial");
            Check(!TerrainV2::CookHeightfieldCollision(bad, cells, output, error),
                  "reject unproved or malformed grid");
            Check(SaveCollisionAsset((root / "after").string(), output, error) &&
                      Read(root / "before") == Read(root / "after"),
                  "failed specialization preserves the previous complete asset");
        }
        fs::remove_all(root);
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "After " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
