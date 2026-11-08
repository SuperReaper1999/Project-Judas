#include "CollisionGeometry.h"
#include "Narrowphase.h"
#include "terrain/TerrainCollision.h"
#include "terrain/TerrainGeometryPages.h"
#include "terrain/TerrainMesher.h"
#include "terrain/TerrainSamples.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        using namespace TerrainV2;
        auto dense = Create(257, 257, .5f, 16);
        for (unsigned z = 0; z < 257; ++z)
            for (unsigned x = 0; x < 257; ++x)
                dense.heights[size_t(z) * 257 + x] = 2 * std::sin(x * .03f) * std::cos(z * .04f);
        Source source;
        std::string error;
        Check(SampleStorage::Spill(dense, 512 * 1024, source, error), error);
        auto cache = std::make_shared<GeometryCache>(512 * 1024);
        std::vector<std::shared_ptr<GeometryRecipe>> recipes;
        for (unsigned z = 0; z < 16; ++z)
            for (unsigned x = 0; x < 16; ++x) {
                auto recipe = PrepareRecipe(source, {x, z}, cache, error);
                Check(bool(recipe), "prepare local immutable recipe: " + error);
                recipes.push_back(std::move(recipe));
            }
        auto pinned = recipes.front()->Acquire();
        const auto first = pinned->collision.vertices;
        for (unsigned pass = 0; pass < 3; ++pass)
            for (const auto &recipe : recipes) {
                auto page = recipe->Acquire();
                Check(page->triangles == 512 && page->chunk.mesh.vertices.empty(),
                      "ordinary page retains exact cook without duplicate render streams");
                Chunk reference;
                Check(BuildChunk(dense, recipe->key, reference, error), error);
                CollisionAsset expected;
                Check(CookHeightfieldCollision(reference.mesh, 16, expected, error), error);
                Check(expected.vertices == page->collision.vertices &&
                          expected.order == page->collision.order &&
                          expected.faces.size() == page->collision.faces.size(),
                      "recooked page exactly matches complete source collision");
                for (size_t i = 0; i < expected.faces.size(); ++i) {
                    const auto &a = expected.faces[i], &b = page->collision.faces[i];
                    Check(a.vertices == b.vertices && a.adjacent == b.adjacent &&
                              a.active == b.active && a.source == b.source && a.normal == b.normal,
                          "eviction preserves triangle identity, adjacency, normals and sidedness");
                }
            }
        Check(pinned->collision.vertices == first,
              "consumer lease remains valid across repeated eviction");
        const auto stats = cache->Statistics();
        Check(stats.bytes <= stats.budget && stats.peakBytes <= stats.budget &&
                  stats.evictions > 500,
              "sustained collision traversal stays within 512 KiB");
        auto recipe = recipes.back();
        auto page = recipe->Acquire();
        auto proxy = CollisionPage(recipe, *page);
        auto shape = Shape::Cooked(proxy, "");
        Check(proxy->vertices.empty() && proxy->nodes.empty(),
              "body descriptor owns no heavy cook arrays");
        auto full = Shape::Cooked(AcquireCollision(proxy), "");
        Check(ShapeBoundingRadius(shape) == ShapeBoundingRadius(full),
              "paged bounds preserve exact original radius");
        RigidBody body;
        auto lease = PrimitiveAt(shape, body, 0);
        Check(lease.shape.asset->vertices == page->collision.vertices,
              "narrowphase primitive acquires and holds complete immutable geometry");
        Source carved = source;
        carved.edits.push_back({Edit::Kind::Remove, {-.5f, .5f, -.5f}, 2});
        auto volumeCache = std::make_shared<GeometryCache>(8 * 1024 * 1024);
        auto cave = PrepareRecipe(carved, {7, 7}, volumeCache, error);
        Check(cave && cave->stored && volumeCache->SpillBytes() > 0,
              "expensive cave page is persisted before it can be evicted");
        auto live = cave->Acquire();
        auto restored = cave->stored->Load();
        Check(live->collision.vertices == restored->collision.vertices &&
                  live->collision.order == restored->collision.order &&
                  live->chunk.mesh.indices == restored->chunk.mesh.indices,
              "private derived page restores exact collision and render topology");
        for (size_t i = 0; i < live->collision.faces.size(); ++i) {
            const auto &a = live->collision.faces[i], &b = restored->collision.faces[i];
            Check(a.vertices == b.vertices && a.adjacent == b.adjacent && a.active == b.active &&
                      a.source == b.source && a.normal == b.normal,
                  "derived page preserves complete collision feature contract");
        }
        Check(live->collision.nodes.size() == restored->collision.nodes.size(),
              "derived tree size");
        for (size_t i = 0; i < live->collision.nodes.size(); ++i) {
            const auto &a = live->collision.nodes[i], &b = restored->collision.nodes[i];
            Check(a.minimum == b.minimum && a.maximum == b.maximum && a.first == b.first &&
                      a.count == b.count && a.left == b.left && a.right == b.right,
                  "derived page preserves exact BVH tree and bounds");
        }
        Check(live->chunk.mesh.vertices.size() == restored->chunk.mesh.vertices.size() &&
                  live->chunk.mesh.primitives.size() == restored->chunk.mesh.primitives.size(),
              "cave presentation retains its vertex streams and source part");
        for (size_t i = 0; i < live->chunk.mesh.vertices.size(); ++i) {
            const auto &a = live->chunk.mesh.vertices[i], &b = restored->chunk.mesh.vertices[i];
            Check(a.position == b.position && a.normal == b.normal && a.uv == b.uv &&
                      a.uv1 == b.uv1 && a.tangent == b.tangent,
                  "derived paging preserves all render vertex values");
        }
        const auto spillPath = cave->stored->Path();
        {
            std::fstream file(spillPath, std::ios::binary | std::ios::in | std::ios::out);
            char byte;
            file.read(&byte, 1);
            byte ^= 1;
            file.seekp(0);
            file.write(&byte, 1);
        }
        bool corrupt = false;
        try {
            cave->stored->Load();
        } catch (const std::runtime_error &) {
            corrupt = true;
        }
        Check(corrupt && live->collision.vertices == restored->collision.vertices,
              "corrupt derived data rejects without modifying a complete leased page");
        cave.reset();
        Check(volumeCache->SpillBytes() == 0 && !std::filesystem::exists(spillPath),
              "retiring a recipe releases disk quota and its own file despite leased CPU geometry");
        std::cout << "METRIC collision_bytes=" << stats.bytes << " peak=" << stats.peakBytes
                  << " budget=" << stats.budget << " evictions=" << stats.evictions
                  << " entries=" << stats.entries << " worst_prepare_ms=" << stats.worstLoadMs
                  << "\n";
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
