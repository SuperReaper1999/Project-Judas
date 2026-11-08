#include "CollisionAsset.h"
#include "EngineHost.h"
#include "PlanetTiles.h"
#include "ScreenshotWriter.h"
#include "terrain/TerrainMesher.h"
#include "terrain/TerrainPlanetChart.h"
#include <chrono>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

int main(int argc, char **argv) {
    unsigned checks = 0;
    try {
        auto check = [&](bool ok, const std::string &message) {
            ++checks;
            if (!ok)
                throw std::runtime_error(message);
        };
        namespace fs = std::filesystem;
        const auto root =
            argc > 1
                ? fs::path(argv[1])
                : fs::temp_directory_path() /
                      ("terrain-planet-render-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(!fs::exists(root), "render evidence directory is fresh");
        fs::create_directories(root);
        EngineHost host;
        std::string error;
        check(host.Init("Terrain V2 planet cutout and native rim", 256, 256, false, error), error);
        auto &renderer = host.GetRenderer();
        PlanetSettings settings;
        settings.radius = 1000;
        settings.collisionSpacing = 1;
        settings.collisionRadius = 16;
        const PlanetTileKey key{2, 4, 8, 8};
        PlanetSurface sphere(settings);
        TerrainV2::Source source;
        TerrainV2::PlanetChart chart;
        check(TerrainV2::CreatePlanetChart(sphere, key, 128, source, chart, error), error);
        source.edits.push_back({TerrainV2::Edit::Kind::Remove, {0, -.5f, 0}, 3});
        std::vector<TerrainV2::ChunkKey> keys;
        for (unsigned z = 0; z < source.ChunksZ(); ++z)
            for (unsigned x = 0; x < source.ChunksX(); ++x)
                keys.push_back({x, z});
        std::vector<TerrainV2::Chunk> chunks;
        check(TerrainV2::BuildChunks(source, keys, chunks, error), error);
        PhysicsWorld physics;
        check(physics.Init(), "native physics initializes");
        PlanetTiles planet(settings, &physics);
        const auto origin = chart.ModelOrigin();
        const auto rotation = glm::quat(chart.ModelRotation());
        std::vector<std::pair<MeshHandle, glm::vec3>> meshes;
        for (auto &chunk : chunks) {
            check(TerrainV2::ProjectPlanetChunk(chart, chunk, error), error);
            auto collision = std::make_shared<CollisionAsset>();
            check(CookCollision(chunk.mesh, {}, *collision, error), error);
            physics.CreateShape(Shape::Cooked(collision, ""), {rotation * chunk.anchor, rotation},
                                false, 0, .7f, 0);
            meshes.emplace_back(renderer.CreateMesh(chunk.mesh), rotation * chunk.anchor);
        }
        auto position = [&](glm::dvec3 point) {
            return glm::vec3(chart.ModelPoint(point) - origin);
        };
        auto camera = [&](double x) {
            const auto eye = position({x, 30, 0}), target = position({x, 0, 0});
            const auto view = glm::lookAt(eye, target, rotation * glm::vec3(0, 0, -1));
            const auto projection = glm::ortho(-8.f, 8.f, -8.f, 8.f, 1.f, 60.f);
            check(planet.Collision(chart.ModelPoint({x, 0, 0}), origin), planet.State().error);
            planet.Prepare(renderer, {{chart.ModelPoint({x, 30, 0}), 1., 256, 16}}, origin, 1);
            return std::pair(projection, view);
        };
        auto draw = [&](bool native, const glm::mat4 &projection, const glm::mat4 &view) {
            renderer.BeginFrame(256, 256);
            renderer.SetCamera(view, projection);
            renderer.SetLighting(rotation * glm::normalize(glm::vec3(.2f, 1, .3f)), {1, 1, 1},
                                 {.25f, .25f, .25f});
            planet.Draw(renderer, origin);
            if (native)
                for (auto [mesh, point] : meshes)
                    renderer.DrawMesh(mesh, point, rotation, {1, 1, 1}, {}, {.24f, .43f, .31f}, 1);
            renderer.EndMeshBatch();
        };
        auto capture = [&](const char *name) {
            std::vector<unsigned char> rgb;
            renderer.CaptureFrame(256, 256, rgb);
            check(WriteRgbPng((root / name).string(), 256, 256, rgb), "write actual GPU capture");
        };
        auto [projection, view] = camera(0);
        draw(false, projection, view);
        float depth = 1;
        glReadPixels(128, 128, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        check(depth > 0 && depth < 1, "negative control: original planet draws over the pit");
        renderer.EndFrame();
        check(planet.SetTerrainCutouts({{key, 7}}, error), error);
        std::tie(projection, view) = camera(0);
        draw(false, projection, view);
        glReadPixels(128, 128, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        check(depth == 1, "planet cutout has no hidden base draw over replacement footprint");
        size_t drawnLeaves = 0;
        for (auto leaf : planet.Leaves())
            if (!(leaf.face == key.face && leaf.level >= key.level &&
                  (leaf.x >> (leaf.level - key.level)) == key.x &&
                  (leaf.y >> (leaf.level - key.level)) == key.y))
                ++drawnLeaves;
        check(planet.State().triangles == drawnLeaves * settings.segments * settings.segments * 2,
              "triangle diagnostics exclude the suppressed base surface");
        renderer.EndFrame();
        draw(true, projection, view);
        glReadPixels(128, 128, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        const auto inverse = glm::inverse(projection * view);
        auto point = inverse * glm::vec4(1.f / 256, 1.f / 256, depth * 2 - 1, 1);
        point /= point.w;
        const auto chartPoint = chart.ChartPoint(glm::inverse(rotation) * glm::vec3(point));
        check(depth < 1 && chartPoint.y < -2.5,
              "actual framebuffer sees excavated floor, not base planet");
        capture("pit.png");
        renderer.EndFrame();
        renderer.BeginShadowPass(0, projection * view);
        planet.Draw(renderer, origin);
        renderer.EndMeshBatch();
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        glReadPixels(viewport[2] / 2, viewport[3] / 2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        check(depth == 1, "shadow pass also removes base surface inside footprint");
        renderer.EndShadowPass();
        std::tie(projection, view) = camera(chart.span * .5);
        draw(true, projection, view);
        std::vector<float> depths(256 * 256);
        glReadPixels(0, 0, 256, 256, GL_DEPTH_COMPONENT, GL_FLOAT, depths.data());
        const auto inverseRim = glm::inverse(projection * view);
        double maximumError = 0;
        for (int y = 16; y < 240; y += 16)
            for (int x = 16; x < 240; x += 16) {
                const float d = depths[size_t(y) * 256 + x];
                auto near = inverseRim * glm::vec4((x + .5f) / 128 - 1, (y + .5f) / 128 - 1, -1, 1);
                auto far = inverseRim * glm::vec4((x + .5f) / 128 - 1, (y + .5f) / 128 - 1, 1, 1);
                near /= near.w;
                far /= far.w;
                const auto direction = glm::normalize(glm::vec3(far - near));
                const auto hit =
                    physics.Raycast(glm::vec3(near), direction, glm::length(glm::vec3(far - near)));
                check(d > 0 && d < 1 && hit.hit,
                      "native/base rim has both rendered and physical support");
                auto actual =
                    inverseRim * glm::vec4((x + .5f) / 128 - 1, (y + .5f) / 128 - 1, d * 2 - 1, 1);
                actual /= actual.w;
                maximumError =
                    std::max(maximumError, double(glm::length(glm::vec3(actual) - hit.point)));
            }
        check(maximumError < .005,
              "rendered native/base rim matches independent MK2 depth below5mm");
        check(planet.State().resident <= settings.maxResident,
              "replacement activation retains visual residency bound");
        capture("rim.png");
        renderer.EndFrame();
        check(glGetError() == GL_NO_ERROR, "planet replacement has no GL error");
        for (auto [mesh, _] : meshes)
            renderer.DestroyMesh(mesh);
        std::cout << "METRIC rim_maximum_render_physics_error_m=" << maximumError << '\n';
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "FAIL " << exception.what() << " after " << checks << " checks\n";
        return 1;
    }
}
