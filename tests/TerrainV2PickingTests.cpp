#include "JobSystem.h"
#include "PlanetTerrain.h"
#include "authoring/TerrainEditorPreview.h"
#include "authoring/TerrainRayPicking.h"
#include "terrain/TerrainMesher.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <thread>

bool OriginalTerrainPick(const MeshData &, glm::vec3, glm::vec3, glm::vec3 &);
namespace {
unsigned checks = 0;
void Check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
std::vector<TerrainV2::ChunkKey> All(const TerrainV2::Source &s) {
    std::vector<TerrainV2::ChunkKey> keys;
    for (unsigned z = 0; z < s.ChunksZ(); ++z)
        for (unsigned x = 0; x < s.ChunksX(); ++x)
            keys.push_back({x, z});
    return keys;
}
void Compare(const TerrainV2::EditorPreview &preview) {
    const auto &mesh = *preview.Mesh();
    std::mt19937 random(0x7185);
    size_t tested = 0, hits = 0;
    for (unsigned i = 0; i < 400; ++i) {
        glm::vec3 origin(float(int(random() % 360) - 180) / 10, 20,
                         float(int(random() % 360) - 180) / 10);
        glm::vec3 direction = i % 2 ? glm::vec3(0, -1, 0) : glm::vec3(.1f, -.6f, -.1f);
        if (i % 13 == 0)
            direction.y *= -1;
        glm::vec3 expected(999), actual(999), extracted(999);
        const bool original = OriginalTerrainPick(mesh, origin, direction, expected);
        Check(PickTerrain(mesh, origin, direction, extracted) == original && extracted == expected,
              "extracted primitive preserves original result and exact point");
        Check(preview.Pick(origin, direction, actual, &tested) == original && actual == expected,
              "cached ranges match independent frozen whole-mesh picker exactly");
        hits += original;
    }
    Check(hits > 50 && tested < mesh.indices.size() / 3 * 400,
          "fixture exercises real geometry and rejects irrelevant triangles");
    for (auto point : {glm::vec3(-16, 10, -16), glm::vec3(0, 10, 0), glm::vec3(16, 10, 16),
                       glm::vec3(-16, 0, 16)}) {
        for (auto direction : {glm::vec3(0, -1, 0), glm::vec3(1, 0, 0), glm::vec3(1, -1e-9f, 0),
                               glm::vec3(-.1f, -2, .1f), glm::vec3(0, 0, 0)}) {
            glm::vec3 expected(99), actual(99);
            Check(preview.Pick(point, direction, actual) ==
                          OriginalTerrainPick(mesh, point, direction, expected) &&
                      actual == expected,
                  "seam, corner, parallel and non-normalized rays preserve old semantics");
        }
    }
    glm::vec3 preserved(12);
    Check(!preview.Pick({NAN, 0, 0}, {0, -1, 0}, preserved) && preserved == glm::vec3(12),
          "invalid ray does not publish a partial result");
}
MeshData OriginalMerge(const TerrainV2::EditorPreview &preview) {
    MeshData mesh;
    for (const auto &[_, c] : preview.Chunks()) {
        const auto offset = uint32_t(mesh.vertices.size());
        for (auto v : c->mesh.vertices) {
            v.position += c->anchor;
            mesh.vertices.push_back(v);
        }
        for (auto index : c->mesh.indices)
            mesh.indices.push_back(offset + index);
    }
    return mesh;
}
void SameMesh(const MeshData &a, const MeshData &b) {
    Check(a.indices == b.indices && a.vertices.size() == b.vertices.size(),
          "complete preview retains original vertex/index ordering");
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        const auto &x = a.vertices[i], &y = b.vertices[i];
        if (x.position != y.position || x.normal != y.normal || x.uv != y.uv || x.uv1 != y.uv1 ||
            x.tangent != y.tangent)
            throw std::runtime_error("preview vertex stream changed");
    }
    Check(true, "all original preview vertex attributes match exactly");
}
} // namespace

int main() {
    try {
        using namespace TerrainV2;
        Source source = Create(65, 65, .5f);
        std::fill(source.heights.begin(), source.heights.end(), 3);
        std::shared_ptr<const EditorPreview> initial;
        std::string error;
        JobSystem jobs(1);
        const auto owner = std::this_thread::get_id();
        bool worker = false;
        const auto job = jobs.Submit(
            [&](JobContext &context) {
                worker = std::this_thread::get_id() != owner;
                if (!PrepareEditorPreview(source, All(source), {}, initial, error))
                    context.SetError(error);
            },
            JobPriority::High, "actual terrain preview worker");
        Check(job.IsValid(), "preview job is scheduled");
        jobs.Wait(job);
        Check(worker && jobs.StateOf(job) == JobState::Completed && initial,
              "geometry, merging and picking data finish on the worker without renderer access");
        jobs.Forget(job);
        SameMesh(*initial->Mesh(), OriginalMerge(*initial));
        Compare(*initial);
        glm::vec3 hit;
        Check(initial->Pick({5, 10, 3}, {0, -2, 0}, hit) && hit == glm::vec3(5, 3, 3),
              "independent plane answer and non-normalized direction");
        Check(initial->Pick({5, -2, 3}, {0, 1, 0}, hit) && hit == glm::vec3(5, 3, 3),
              "brush picking remains two-sided for underground authoring");

        auto changed = source;
        changed.edits.push_back({Edit::Kind::Remove, {0, 2.5f, 0}, 2});
        auto keys = Affected(changed, changed.edits.back());
        auto previous = initial;
        std::shared_ptr<const EditorPreview> edited;
        Check(PrepareEditorPreview(changed, keys, initial, edited, error),
              "dirty terrain prepares a complete new snapshot");
        size_t reused = 0;
        for (const auto &[key, chunk] : initial->Chunks()) {
            const bool dirty = std::find(keys.begin(), keys.end(), key) != keys.end();
            Check((edited->Chunks().at(key) == chunk) == !dirty,
                  "only dirty chunk ownership changes; unchanged GPU geometry can be retained");
            reused += !dirty;
        }
        Check(reused > 0, "fixture has actual reused chunks");
        SameMesh(*edited->Mesh(), OriginalMerge(*edited));
        Compare(*edited);
        Check(initial->Pick({0, 10, 0}, {0, -1, 0}, hit) && hit.y == 3,
              "live snapshot remains intact until publication");
        Check(edited->Pick({0, 10, 0}, {0, -1, 0}, hit) && hit.y < 1,
              "new snapshot picks the excavated bottom, not the old surface");
        std::shared_ptr<const EditorPreview> full;
        Check(PrepareEditorPreview(changed, All(changed), {}, full, error), "full-build control");
        SameMesh(*edited->Mesh(), *full->Mesh());

        auto output = edited;
        Check(!PrepareEditorPreview(changed, {{999, 0}}, edited, output, error) && output == edited,
              "invalid dirty work cannot replace the previous complete preview");
        auto bad = changed;
        bad.heights.back() = NAN;
        Check(!PrepareEditorPreview(bad, keys, edited, output, error) && output == edited,
              "invalid source keeps live picking and geometry intact");
        unsigned polls = 0;
        Check(!PrepareEditorPreview(changed, All(changed), edited, output, error,
                                    [&] { return ++polls >= 8; }) &&
                  output == edited,
              "cancellation during preparation cannot publish a partial index");
        auto small = Create(17, 17, .5f);
        Check(PrepareEditorPreview(small, All(small), {}, output, error) &&
                  output->Chunks().size() == 1 && !output->Pick({12, 10, 0}, {0, -1, 0}, hit),
              "source switch removes obsolete outer support from editor picking");
        std::weak_ptr<const EditorPreview> retired = initial;
        auto retainedMesh = initial->Mesh();
        std::weak_ptr<const MeshData> retiredMesh = retainedMesh;
        previous.reset();
        initial.reset();
        Check(retired.expired() && !retiredMesh.expired(),
              "legacy mesh readers retain safe ownership without retaining the old snapshot");
        retainedMesh.reset();
        Check(retiredMesh.expired(), "retired preview storage is released after its last reader");

        PlanetSettings settings;
        settings.radius = 1000;
        settings.collisionSpacing = 1;
        PlanetSurface planet(settings);
        Source mapped;
        PlanetChart chart;
        Check(CreatePlanetChart(planet, {2, 4, 8, 8}, 128, mapped, chart, error),
              "actual mapped planetary source control");
        Check(PrepareEditorPreview(mapped, All(mapped), {}, output, error),
              "planetary geometry uses the same preview builder");
        SameMesh(*output->Mesh(), OriginalMerge(*output));
        Compare(*output);
        std::cout << "METRIC cached_bounds_bytes=" << edited->BoundsBytes()
                  << " reused_chunks=" << reused << '\n';
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
