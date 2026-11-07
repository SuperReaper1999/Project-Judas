#include "CollisionAsset.h"
#include "PhysicsWorld.h"
#include "terrain/TerrainSource.h"
#include <chrono>
#include <iostream>
#include <set>
#include <stdexcept>
#include <tuple>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
}
TerrainV2::Source Hillside() {
    auto s = TerrainV2::Create(33, 33, .5f);
    s.bottom = -4;
    s.top = 8;
    for (unsigned z = 0; z < s.samplesZ; ++z)
        for (unsigned x = 0; x < s.samplesX; ++x) {
            float px = (float(x) - 16) * s.spacing, pz = (float(z) - 16) * s.spacing;
            s.heights[z * s.samplesX + x] =
                .8f + 3.f * std::exp(-pz * pz / 8.f) + .15f * std::sin(px * .5f);
        }
    return s;
}
std::shared_ptr<CollisionAsset> Cook(const TerrainV2::Chunk &c) {
    auto a = std::make_shared<CollisionAsset>();
    std::string error;
    Check(CookCollision(c.mesh, {}, *a, error), "cook actual chunk: " + error);
    return a;
}
using Point = std::tuple<int, int, int>;
std::set<Point> Boundary(const TerrainV2::Chunk &c, float x) {
    std::set<Point> out;
    for (auto &v : c.mesh.vertices) {
        auto p = v.position + c.anchor;
        if (std::abs(p.x - x) < 1e-6f)
            out.emplace(int(std::lround(p.x * 100000)), int(std::lround(p.y * 100000)),
                        int(std::lround(p.z * 100000)));
    }
    return out;
}
} // namespace
int main() {
    try {
        auto s = Hillside();
        std::string error;
        auto rectangular = TerrainV2::Create(129, 97, .5f);
        for (unsigned seed = 0; seed < 200; ++seed) {
            TerrainV2::Edit edit;
            edit.point = {float(int(seed % 31) - 15) * 2.3f, 0, float(int(seed % 29) - 14) * 1.9f};
            edit.radius = .5f + float(seed % 17) * .4f;
            std::vector<TerrainV2::ChunkKey> exhaustive;
            const float width = rectangular.chunkCells * rectangular.spacing;
            for (unsigned z = 0; z < rectangular.ChunksZ(); ++z)
                for (unsigned x = 0; x < rectangular.ChunksX(); ++x) {
                    glm::vec2 lo(x * width - 32, z * width - 24), hi = lo + glm::vec2(width),
                                                                  p(edit.point.x, edit.point.z);
                    auto delta = glm::max(glm::max(lo - p, p - hi), glm::vec2(0));
                    if (glm::length(delta) <= edit.radius + rectangular.spacing)
                        exhaustive.push_back({x, z});
                }
            Check(TerrainV2::Affected(rectangular, edit) == exhaustive,
                  "sparse invalidation preserves exhaustive footprint, halo and iteration order");
        }
        Check(
            TerrainV2::Affected(rectangular, {TerrainV2::Edit::Kind::Remove, {3e38f, 0, -3e38f}, 1})
                .empty(),
            "finite extreme brush coordinates cannot overflow chunk indices");
        TerrainV2::Chunk a, b;
        Check(TerrainV2::BuildChunk(s, {0, 0}, a, error), "first heightfield chunk");
        Check(TerrainV2::BuildChunk(s, {1, 0}, b, error), "second heightfield chunk");
        Check(Boundary(a, 0) == Boundary(b, 0), "base chunks share exact boundary samples");
        PhysicsWorld world;
        world.Init();
        auto ha = world.CreateShape(Shape::Cooked(Cook(a), ""), {a.anchor, {1, 0, 0, 0}}, false, 0,
                                    .7f, 0);
        auto hb = world.CreateShape(Shape::Cooked(Cook(b), ""), {b.anchor, {1, 0, 0, 0}}, false, 0,
                                    .7f, 0);
        Check(world.Raycast({-.3f, 1.6f, -7}, {0, 0, 1}, 7).hit,
              "hillside blocks a real MK2 query before excavation");
        // The tunnel straddles x=0, the boundary of two chunks. It is carved from
        // the HEIGHTFIELD, not placed in front of an unmodified base collider.
        for (float z = -5; z <= 5; z += .75f)
            s.edits.push_back({TerrainV2::Edit::Kind::Remove, {0, 1.6f, z}, 1.25f});
        const auto start = std::chrono::steady_clock::now();
        Check(TerrainV2::BuildChunk(s, {0, 0}, a, error) &&
                  TerrainV2::BuildChunk(s, {1, 0}, b, error),
              "extract two excavated chunks");
        Check(a.volumetric && b.volumetric, "both chunks change representation");
        Check(Boundary(a, 0) == Boundary(b, 0),
              "tunnel boundary has identical vertex positions across chunks");
        Check(world.ReplaceStaticCollision(ha, Cook(a)) &&
                  world.ReplaceStaticCollision(hb, Cook(b)),
              "replace real heightfield collision");
        const auto twoChunkMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        Check(!world.Raycast({-.3f, 1.6f, -7}, {0, 0, 1}, 6.9f).hit,
              "real tunnel opening has no surviving heightfield collider");
        Check(world.Raycast({-2, 1.6f, -7}, {0, 0, 1}, 6.9f).hit,
              "nearby undug hill remains solid");
        Check(world.Raycast({-.3f, 7, -.5f}, {0, -1, 0}, 6).hit,
              "tunnel roof is real physical terrain");
        auto bytes = TerrainV2::Serialize(s);
        TerrainV2::Source loaded;
        Check(TerrainV2::Parse(bytes, loaded, error) && TerrainV2::Serialize(loaded) == bytes,
              "source and excavation journal roundtrip exactly");
        TerrainV2::Chunk cold;
        Check(TerrainV2::BuildChunk(loaded, {0, 0}, cold, error) &&
                  cold.mesh.indices == a.mesh.indices,
              "reconstruction retains collision topology");
        Check(cold.mesh.vertices.size() == a.mesh.vertices.size(),
              "reconstruction retains geometry");
        auto mountain = s;
        mountain.bottom = -4096;
        mountain.top = 4096;
        for (auto &height : mountain.heights)
            height += 1000;
        for (auto &edit : mountain.edits)
            edit.point.y += 1000;
        TerrainV2::Chunk high;
        Check(TerrainV2::BuildChunk(mountain, {0, 0}, high, error),
              "excavation works at mountain elevations");
        Check(high.sampledBytes <= a.sampledBytes * 2,
              "volume storage follows local edits, not mountain height or global vertical bounds");
        float maximumHeight = -1e9f;
        for (auto &v : high.mesh.vertices)
            maximumHeight = std::max(maximumHeight, v.position.y);
        Check(maximumHeight > 1003, "sparse excavation retains the untouched mountain surface");
        for (unsigned seed = 0; seed < 24; ++seed) {
            auto stress = TerrainV2::Create(17, 17, .5f, 8);
            stress.bottom = -4;
            stress.top = 8;
            uint32_t random = seed + 1;
            for (unsigned z = 0; z < 17; ++z)
                for (unsigned x = 0; x < 17; ++x) {
                    random = random * 1664525u + 1013904223u;
                    stress.heights[z * 17 + x] = 2 + .3f * std::sin(x * .7f) +
                                                 .35f * std::cos(z * .6f) +
                                                 float(random & 255) / 4096;
                }
            stress.edits.push_back(
                {TerrainV2::Edit::Kind::Remove, {-.3f, 1.5f, float(int(seed % 5) - 2)}, 1.25f});
            if (seed % 2)
                stress.edits.push_back({TerrainV2::Edit::Kind::Add, {1, 3, 1}, .75f});
            for (unsigned z = 0; z < 2; ++z)
                for (unsigned x = 0; x < 2; ++x) {
                    TerrainV2::Chunk chunk;
                    Check(TerrainV2::BuildChunk(stress, {x, z}, chunk, error),
                          "adversarial edited chunk extraction");
                    if (!chunk.mesh.indices.empty())
                        Cook(chunk);
                }
        }
        auto invalid = s;
        invalid.edits.front().radius = -1;
        Check(!TerrainV2::Validate(invalid, error), "invalid edit rejected");
        TerrainV2::Source empty = s;
        empty.edits = {{TerrainV2::Edit::Kind::Remove, {0, 0, 0}, 64}};
        Check(TerrainV2::BuildChunk(empty, {0, 0}, cold, error) && cold.mesh.indices.empty(),
              "empty excavated chunk is a valid result");
        std::cout << "METRIC two_chunk_build_and_cook_ms=" << twoChunkMs
                  << " triangles=" << (a.mesh.indices.size() + b.mesh.indices.size()) / 3
                  << " sample_bytes=" << a.sampledBytes + b.sampledBytes << '\n';
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
