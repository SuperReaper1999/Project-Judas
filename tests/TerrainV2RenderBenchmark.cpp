#include "EngineHost.h"
#include "terrain/TerrainLod.h"
#include "terrain/TerrainLodTransition.h"
#include <algorithm>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <numeric>
#include <stdexcept>
using Clock = std::chrono::steady_clock;
namespace {
double Ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
struct Row {
    double cpu = 0, gpu = 0;
    unsigned draws = 0, triangles = 0;
};
void Report(const char *path, const std::vector<Row> &rows) {
    std::cout << "{\"path\":\"" << path << "\",\"samples\":" << rows.size();
    for (bool gpu : {false, true}) {
        std::vector<double> v;
        for (auto r : rows)
            v.push_back(gpu ? r.gpu : r.cpu);
        std::sort(v.begin(), v.end());
        std::cout << ",\"" << (gpu ? "gpu" : "cpu_submission")
                  << "_ms\":{\"median\":" << v[v.size() / 2]
                  << ",\"p95\":" << v[size_t(std::ceil(v.size() * .95)) - 1]
                  << ",\"worst\":" << v.back() << '}';
    }
    std::cout << ",\"draws\":" << rows.back().draws << ",\"triangles\":" << rows.back().triangles
              << "}\n";
}
} // namespace
int main(int argc, char **) {
    try {
        std::string error;
        EngineHost host;
        if (!host.Init("Terrain V2 matched landscape submission benchmark", 1280, 800, false,
                       error))
            throw std::runtime_error(error);
        auto &r = host.GetRenderer();
        auto s = TerrainV2::Create(513, 513, 2);
        for (unsigned z = 0; z < 513; ++z)
            for (unsigned x = 0; x < 513; ++x)
                s.heights[size_t(z) * 513 + x] =
                    35 * std::sin(x * .017f) + 20 * std::sin(z * .023f) +
                    8 * std::sin((x + z) * .04f) + 2 * std::cos(x * .1f) * std::sin(z * .08f);
        auto start = Clock::now();
        TerrainV2::HeightLod lod(s);
        const double metadataMs = Ms(start);
        std::vector<TerrainV2::View> interests{{{0, 900, 1300}, 570, 0},
                                               {{-900, 600, -900}, 570, 0}};
        if (argc > 1)
            interests = {{{0, 35, 80}, 570, 0}, {{-300, 65, -200}, 570, 0}};
        auto patches = lod.Select(interests, {});
        const auto distantPatches = lod.Select({{{0, 10000, 0}, 570, 0}}, {});
        TerrainV2::LodTransition arrival, walking;
        arrival.Update(distantPatches, 1.f / 60);
        arrival.Update(patches, 1.f / 60);
        arrival.Update(patches, TerrainV2::LodTransition::Duration / 2);
        auto movedViews = interests;
        movedViews.front().position.x += 60;
        auto movedPatches = lod.Select(movedViews, {});
        walking.Update(patches, 1.f / 60);
        walking.Update(movedPatches, 1.f / 60);
        walking.Update(movedPatches, TerrainV2::LodTransition::Duration / 2);
        std::cout << "{\"camera_distance_case\":\"" << (argc > 1 ? "near" : "far") << "\"}\n";
        std::vector<std::pair<MeshHandle, glm::vec3>> ordinary;
        size_t uniqueBytes = 0;
        start = Clock::now();
        for (unsigned z = 0; z < s.ChunksZ(); ++z)
            for (unsigned x = 0; x < s.ChunksX(); ++x) {
                TerrainV2::Chunk c;
                if (!TerrainV2::BuildChunk(s, {x, z}, c, error))
                    throw std::runtime_error(error);
                uniqueBytes +=
                    c.mesh.vertices.size() * sizeof(MeshVertex) + c.mesh.indices.size() * 4;
                ordinary.push_back({r.CreateMesh(c.mesh), c.anchor});
            }
        const double referencePrepareAndUploadMs = Ms(start);
        start = Clock::now();
        auto heights = r.CreateTexture(TerrainV2::HeightImage(s));
        const double heightUploadMs = Ms(start);
        auto grid = TerrainV2::HeightGrid(16);
        const size_t gridBytes =
            grid.vertices.size() * sizeof(MeshVertex) + grid.indices.size() * 4;
        std::cout
            << "{\"workload\":\"1024m-square landscape; two independent cameras; identical source; "
               "2px geometric-error target\",\"source_samples\":263169,\"reference_meshes\":"
            << ordinary.size() << ",\"selected_patches\":" << patches.size()
            << ",\"reference_geometry_bytes\":" << uniqueBytes
            << ",\"shared_grid_and_height_bytes\":" << gridBytes + s.heights.size() * 4
            << ",\"metadata_prepare_ms\":" << metadataMs
            << ",\"reference_prepare_and_upload_ms\":" << referencePrepareAndUploadMs
            << ",\"height_upload_ms\":" << heightUploadMs << "}\n";
        GLuint query;
        glGenQueries(1, &query);
        auto render = [&](bool shared, const TerrainV2::LodTransition *transition = nullptr) {
            if (transition && !transition->Active())
                transition = nullptr;
            Row row;
            r.ResetStats();
            glFinish();
            const auto begin = Clock::now();
            glBeginQuery(GL_TIME_ELAPSED, query);
            for (const auto &camera : interests) {
                r.BeginFrame(1280, 800);
                r.SetCamera(glm::lookAt(camera.position, glm::vec3(0), glm::vec3(0, 1, 0)),
                            glm::perspective(glm::radians(70.f), 1.6f, 1.f, 5000.f));
                r.SetLighting({.4f, 1, .2f}, {1, 1, 1}, {.3f, .3f, .3f});
                r.BeginTerrainMaterial(
                    {}, {},
                    {glm::vec3(.35f, .5f, .24f), glm::vec3(.5f), glm::vec3(.5f), glm::vec3(.5f)},
                    glm::mat4(1));
                if (shared) {
                    auto drawPatches = [&](const auto &selected, glm::vec2 coverage,
                                           bool incoming) {
                        for (const auto &p : selected) {
                            const auto k = p.key;
                            const bool common = transition && transition->Shared(k);
                            if (common && incoming)
                                continue;
                            float stride = float(1u << k.level), width = 16 * 2 * stride;
                            r.DrawTerrainGrid(heights, 16,
                                              {float(k.x * 16), float(k.z * 16), stride, 16},
                                              p.edgeStrides, {513, 513}, 2,
                                              {float(k.x * 32) - 512, 0, float(k.z * 32) - 512},
                                              {1, 0, 0, 0}, width, p.minimum, p.maximum, {1, 1, 1},
                                              1, common ? glm::vec2(0, 1) : coverage);
                        }
                    };
                    drawPatches(transition ? transition->Current() : patches,
                                {transition ? .5f : 0, 1}, false);
                    if (transition && transition->Active())
                        drawPatches(transition->Incoming(), {0, .5f}, true);
                } else
                    for (const auto &[mesh, anchor] : ordinary)
                        r.DrawMesh(mesh, anchor, {1, 0, 0, 0}, {1, 1, 1}, {}, {1, 1, 1}, 1);
                r.EndTerrainMaterial();
                r.EndFrame();
            }
            glEndQuery(GL_TIME_ELAPSED);
            row.cpu = Ms(begin);
            GLuint64 ns = 0;
            glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns);
            row.gpu = ns / 1e6;
            row.draws = r.Stats().drawCalls;
            row.triangles = r.Stats().triangles;
            return row;
        };
        for (unsigned i = 0; i < 4; ++i) {
            render(false);
            render(true);
        }
        std::vector<Row> before, after;
        for (unsigned i = 0; i < 30; ++i) {
            if (i % 2) {
                after.push_back(render(true));
                before.push_back(render(false));
            } else {
                before.push_back(render(false));
                after.push_back(render(true));
            }
        }
        Report("unique_full_resolution_meshes", before);
        Report("shared_grid_with_camera_union_LOD", after);
        for (unsigned i = 0; i < 4; ++i)
            render(true, &arrival);
        std::vector<Row> transitioning;
        for (unsigned i = 0; i < 30; ++i)
            transitioning.push_back(render(true, &arrival));
        Report("shared_grid_mid_transition", transitioning);
        for (unsigned i = 0; i < 4; ++i)
            render(true, &walking);
        transitioning.clear();
        for (unsigned i = 0; i < 30; ++i)
            transitioning.push_back(render(true, &walking));
        Report("shared_grid_mid_walking_transition", transitioning);
        unsigned common = 0;
        for (const auto &p : walking.Current())
            common += walking.Shared(p.key);
        std::cout << "{\"walking_old_patches\":" << walking.Current().size()
                  << ",\"walking_new_patches\":" << walking.Incoming().size()
                  << ",\"walking_shared_patches\":" << common << "}\n";
        for (auto [mesh, _] : ordinary)
            r.DestroyMesh(mesh);
        r.DestroyTexture(heights);
        glDeleteQueries(1, &query);
        if (glGetError() != GL_NO_ERROR)
            throw std::runtime_error("OpenGL error during terrain measurement");
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
