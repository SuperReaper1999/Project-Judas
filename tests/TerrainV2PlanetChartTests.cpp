#include "CollisionAsset.h"
#include "PhysicsWorld.h"
#include "PlanetTiles.h"
#include "terrain/TerrainMesher.h"
#include "terrain/TerrainPlanetChart.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <tuple>

PlanetSelection ReferencePlanetSelection(const PlanetSurface &, const std::vector<PlanetView> &);

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
        std::string error;
        double maximumInverse = 0, maximumBoundary = 0, maximumDerivative = 0;
        for (unsigned face = 0; face < 6; ++face)
            for (const auto xy : {glm::uvec2(0), glm::uvec2(3, 5), glm::uvec2(7)}) {
                PlanetSettings settings;
                settings.radius = 4000;
                settings.bands.push_back({4, {.03, -.02, .01}});
                PlanetSurface planet(settings);
                TerrainV2::Source source;
                TerrainV2::PlanetChart chart;
                Check(TerrainV2::CreatePlanetChart(planet, {face, 3, xy.x, xy.y}, 32, source, chart,
                                                   error),
                      "create aligned source footprint on every cube-face orientation: " + error);
                for (unsigned z = 0; z <= 32; z += 4)
                    for (unsigned x = 0; x <= 32; x += 4) {
                        const glm::dvec3 p((double(x) / 32 - .5) * chart.span,
                                           source.heights[z * 33 + x],
                                           (double(z) / 32 - .5) * chart.span);
                        const auto model = chart.ModelPoint(p);
                        const auto expected = planet.GridPoint(chart.tile, x, 32 - z, 32);
                        const auto expectedDirection = glm::normalize(expected);
                        // Source heights are deliberately binary32, unlike planetary positions.
                        const double radialError =
                            std::abs(glm::length(expected) - (chart.radius + p.y));
                        maximumBoundary =
                            std::max(maximumBoundary, glm::length(model - expected) - radialError);
                        maximumInverse = std::max(
                            maximumInverse, glm::length(chart.ChartPoint(chart.LocalPoint(p)) - p));
                        Check(
                            glm::length(glm::normalize(model) - expectedDirection) < 2e-15 &&
                                glm::length(model - expected) < radialError + 2e-11 &&
                                glm::determinant(chart.Jacobian(p)) > 0,
                            "same geographic ray, declared height precision and positive mapping");
                    }
                const glm::dvec3 p(chart.span * .13, 2, -chart.span * .21);
                const auto jacobian = chart.Jacobian(p);
                for (unsigned axis = 0; axis < 3; ++axis) {
                    auto a = p, b = p;
                    a[axis] -= .001;
                    b[axis] += .001;
                    maximumDerivative =
                        std::max(maximumDerivative,
                                 glm::length((chart.LocalPoint(b) - chart.LocalPoint(a)) / .002 -
                                             jacobian[axis]));
                }
            }
        Check(maximumInverse < 3e-11 && maximumDerivative < 2e-9,
              "independent finite-difference Jacobian and inverse round trip");
        for (const double radius : {6371000., 1e10}) {
            PlanetSettings settings;
            settings.radius = radius;
            PlanetSurface planet(settings);
            TerrainV2::Source source;
            TerrainV2::PlanetChart chart;
            const unsigned level = radius > 1e9 ? 24 : 18;
            Check(TerrainV2::CreatePlanetChart(planet,
                                               {4, level, (1u << level) / 2, (1u << level) / 2}, 64,
                                               source, chart, error),
                  "large-radius chart remains local");
            const glm::dvec3 p(5.25, -2, -3.5);
            const auto origin = glm::dvec3(1e12, -1e12, 1e12);
            const auto absolute = origin + chart.ModelPoint(p);
            const auto local =
                glm::inverse(chart.ModelRotation()) * ((absolute - origin) - chart.ModelOrigin());
            Check(glm::length(chart.ChartPoint(local) - p) < .001,
                  "absolute1e12 origin narrows only after double subtraction, submillimetre error");
        }
        {
            PlanetSettings high;
            high.radius = 6371000;
            PlanetSurface highGround(high, [](glm::dvec3) { return 12000.; }, 12000, 0);
            TerrainV2::Source source;
            TerrainV2::PlanetChart chart;
            Check(TerrainV2::CreatePlanetChart(highGround, {2, 18, 131072, 131072}, 32, source,
                                               chart, error),
                  "high-altitude source has local placement origin");
            TerrainV2::Chunk chunk;
            Check(TerrainV2::BuildChunk(source, {0, 0}, chunk, error) &&
                      TerrainV2::ProjectPlanetChunk(chart, chunk, error),
                  "project high-altitude chunk");
            float largest = 0;
            for (const auto &vertex : chunk.mesh.vertices)
                largest = std::max(largest, glm::length(vertex.position));
            Check(glm::length(chunk.anchor) < 50 && largest < 50 && chart.originHeight == 12000,
                  "both body anchor and vertices stay local at12km elevation, not cancelling large "
                  "floats");
        }
        TerrainV2::PlanetChart invalid;
        invalid.tile = {0, 27, 0, 0};
        Check(!TerrainV2::ValidatePlanetChart(invalid, error),
              "invalid depth rejects before shift");
        invalid.tile = {6, 4, 0, 0};
        Check(!TerrainV2::ValidatePlanetChart(invalid, error), "invalid face rejects");
        invalid.tile = {0, 2, 0, 0};
        Check(!TerrainV2::ValidatePlanetChart(invalid, error),
              "unbounded hemisphere patches reject");

        PlanetSettings settings;
        settings.radius = 1000;
        const PlanetTileKey key{2, 4, 8, 8};
        // A real height provider owns the same hill sampled by the patch.
        const double centerV = 2 * (key.y + .5) / double(1u << key.level) - 1;
        PlanetSurface planet(
            settings,
            [=](glm::dvec3 direction) {
                const auto uv = PlanetSurface::FaceCoordinates(key.face, direction);
                const double z = (centerV - uv.y) * settings.radius;
                return 6 * std::exp(-z * z / 64);
            },
            6, 1);
        TerrainV2::Source source;
        TerrainV2::PlanetChart chart;
        Check(TerrainV2::CreatePlanetChart(planet, key, 128, source, chart, error),
              "create source from original curved hill provider");
        const auto uncutSource = source;
        const float scale = float(chart.span / (2 * settings.radius / (1u << key.level)));
        for (float z = -11 * scale; z <= 11 * scale; z += 1.5f * scale)
            source.edits.push_back({TerrainV2::Edit::Kind::Remove, {0, 1.8f, z}, 3.f});
        std::vector<TerrainV2::ChunkKey> keys;
        for (unsigned z = 0; z < source.ChunksZ(); ++z)
            for (unsigned x = 0; x < source.ChunksX(); ++x)
                keys.push_back({x, z});
        const auto start = std::chrono::steady_clock::now();
        std::vector<TerrainV2::Chunk> chunks;
        Check(TerrainV2::BuildChunks(source, keys, chunks, error),
              "extract actual cross-chunk cave: " + error);
        PhysicsWorld physics;
        Check(physics.Init(), "initialize authoritative collision world");
        size_t triangles = 0, volumes = 0;
        double seamError = 0;
        std::map<std::tuple<float, float, float>, glm::vec3> shared;
        for (auto &chunk : chunks) {
            std::vector<glm::vec3> points;
            for (const auto &vertex : chunk.mesh.vertices)
                points.push_back(vertex.position + chunk.anchor);
            Check(TerrainV2::ProjectPlanetChunk(chart, chunk, error), "project chunk: " + error);
            for (size_t i = 0; i < points.size(); ++i) {
                const auto p = points[i];
                auto [it, inserted] = shared.emplace(
                    std::tuple(p.x, p.y, p.z), chunk.anchor + chunk.mesh.vertices[i].position);
                if (!inserted)
                    seamError = std::max(
                        seamError,
                        double(glm::length(it->second -
                                           (chunk.anchor + chunk.mesh.vertices[i].position))));
            }
            if (!chunk.mesh.indices.empty()) {
                auto collision = std::make_shared<CollisionAsset>();
                Check(CookCollision(chunk.mesh, {}, *collision, error),
                      "strict cook curved cave: " + error);
                physics.CreateShape(Shape::Cooked(collision, ""), {chunk.anchor, {1, 0, 0, 0}},
                                    false, 0, .7f, 0);
            }
            triangles += chunk.mesh.indices.size() / 3;
            volumes += chunk.volumetric;
        }
        auto local = [&](glm::dvec3 p) { return glm::vec3(chart.LocalPoint(p)); };
        const auto a = local({0, 1.8, -20 * scale}), b = local({0, 1.8, 20 * scale});
        PhysicsWorld control;
        Check(control.Init(), "initialize independent uncut control");
        std::vector<TerrainV2::Chunk> uncutChunks;
        Check(TerrainV2::BuildChunks(uncutSource, keys, uncutChunks, error),
              "extract same planet hillside without excavation");
        for (auto &chunk : uncutChunks) {
            Check(TerrainV2::ProjectPlanetChunk(chart, chunk, error), "project uncut control");
            auto collision = std::make_shared<CollisionAsset>();
            Check(CookCollision(chunk.mesh, {}, *collision, error), "cook uncut curved control");
            control.CreateShape(Shape::Cooked(collision, ""), {chunk.anchor, {1, 0, 0, 0}}, false,
                                0, .7f, 0);
        }
        Check(control.Raycast(a, glm::normalize(b - a), glm::length(b - a)).hit,
              "negative control: same path is blocked by the original uncut hill");
        Check(!physics.Raycast(a, glm::normalize(b - a), glm::length(b - a)).hit,
              "actual MK2 query traverses curved hillside tunnel across chunk seams");
        const auto position = local({0, 1.8, 0}), down = local({0, -5, 0});
        const auto floor = physics.Raycast(position, glm::normalize(down - position), 10);
        Check(floor.hit && chart.ChartPoint(floor.point).y < 0 && volumes >= 4 && seamError < 1e-5,
              "curved tunnel retains real floor and shared seam positions");
        // Compose with the ACTUAL planetary collision owner, not a duplicated
        // test sphere. Disabled backing bodies retain safe restoration support.
        PlanetSettings sphereSettings;
        sphereSettings.radius = 1000;
        sphereSettings.collisionSpacing = 1;
        sphereSettings.collisionRadius = 12;
        PlanetSurface sphere(sphereSettings);
        TerrainV2::Source pit;
        TerrainV2::PlanetChart pitChart;
        Check(TerrainV2::CreatePlanetChart(sphere, key, 128, pit, pitChart, error),
              "create matching native replacement for actual PlanetTiles");
        pit.edits.push_back({TerrainV2::Edit::Kind::Remove, {0, -.5f, 0}, 3});
        std::vector<TerrainV2::Chunk> pitChunks;
        Check(TerrainV2::BuildChunks(pit, keys, pitChunks, error), "extract real curved pit");
        PhysicsWorld composed;
        Check(composed.Init(), "initialize composed planetary world");
        PlanetTiles base(sphereSettings, &composed);
        const auto origin = pitChart.ModelOrigin();
        const auto rotation = glm::quat(pitChart.ModelRotation());
        auto worldPoint = [&](glm::dvec3 p) { return glm::vec3(pitChart.ModelPoint(p) - origin); };
        const auto focus = pitChart.ModelPoint({0, 0, 0});
        Check(base.Collision(focus, origin), "original planetary ground is ready");
        for (auto &chunk : pitChunks) {
            Check(TerrainV2::ProjectPlanetChunk(pitChart, chunk, error), "project pit chunk");
            auto collision = std::make_shared<CollisionAsset>();
            Check(CookCollision(chunk.mesh, {}, *collision, error), "cook curved pit support");
            composed.CreateShape(Shape::Cooked(collision, ""), {rotation * chunk.anchor, rotation},
                                 false, 0, .7f, 0);
        }
        auto radialHit = [&](glm::dvec3 point) {
            const auto a = worldPoint(point + glm::dvec3(0, 8, 0));
            const auto b = worldPoint(point - glm::dvec3(0, 8, 0));
            return composed.Raycast(a, glm::normalize(b - a), glm::length(b - a));
        };
        auto hit = radialHit({0, 0, 0});
        Check(hit.hit && std::abs(pitChart.ChartPoint(glm::inverse(rotation) * hit.point).y) < .002,
              "negative control: intact base planet still blocks the excavated replacement");
        const auto bodyCount = composed.AliveBodyCount();
        Check(base.SetTerrainCutouts({{key, 7}}, error), "install geographic cutout: " + error);
        hit = radialHit({0, 0, 0});
        Check(hit.hit && pitChart.ChartPoint(glm::inverse(rotation) * hit.point).y < -2.5 &&
                  base.Bodies().empty() && composed.AliveBodyCount() == bodyCount,
              "cutout removes base collision; actual native pit floor becomes authoritative");
        auto invalidCuts = base.TerrainCutouts();
        invalidCuts[0].tile.x = UINT32_MAX;
        Check(!base.SetTerrainCutouts(invalidCuts, error) &&
                  base.TerrainCutouts().front().tile == key && base.Bodies().empty(),
              "malformed cutout preserves the previous complete support selection");
        invalidCuts = {{key, 26}};
        Check(!base.SetTerrainCutouts(invalidCuts, error) && base.Bodies().empty(),
              "excessive protected topology rejects without partially restoring ground");
        const auto edge = pitChart.ModelPoint({pitChart.span * .5, 0, 0});
        Check(base.Collision(edge, origin), "move real physical interest across replacement rim");
        for (const double offset : {-1., -.01, 0., .01, 1.}) {
            hit = radialHit({pitChart.span * .5 + offset, 0, 0});
            Check(hit.hit &&
                      std::abs(pitChart.ChartPoint(glm::inverse(rotation) * hit.point).y) < .002,
                  "MK2 support is continuous across native/base geographic seam");
        }
        const auto countAtRim = composed.AliveBodyCount();
        Check(base.SetTerrainCutouts({}, error) && composed.AliveBodyCount() == countAtRim,
              "removing replacement restores retained original support without recooking");
        const auto centerFocus = pitChart.ModelPoint({0, 0, 0});
        Check(base.Collision(centerFocus, origin), "return interest to restored original ground");
        hit = radialHit({0, 0, 0});
        Check(hit.hit && std::abs(pitChart.ChartPoint(glm::inverse(rotation) * hit.point).y) < .002,
              "restored original planet surface blocks the former pit as expected");
        for (unsigned i = 0; i < 20; ++i) {
            const double altitude = std::pow(3., i * .5);
            std::vector<PlanetView> views{{{0, 1000 + altitude, 0}, 1., 1080}};
            if (i % 2)
                views.push_back({{1001, 0, 0}, .8, 720, 50});
            const auto before = ReferencePlanetSelection(sphere, views);
            const auto after = SelectPlanetTiles(sphere, views);
            Check(before.leaves == after.leaves &&
                      before.maximumPixelError == after.maximumPixelError &&
                      before.budgetLimited == after.budgetLimited,
                  "no-cutout camera union and LOD are exact against frozen accepted selection");
        }
        std::vector<PlanetTileKey> pinned{{2, 7, 64, 64}, {2, 7, 65, 64}};
        auto selection = SelectPlanetTiles(sphere, {{centerFocus, 1., 1080}}, pinned);
        for (auto pin : pinned)
            Check(std::find(selection.leaves.begin(), selection.leaves.end(), pin) !=
                      selection.leaves.end(),
                  "authored rim stays at its exact grid depth");
        Check(selection.leaves.size() <= sphereSettings.maxTiles,
              "protected rims obey existing global visual residency budget");
        for (unsigned face = 0; face < 6; ++face) {
            double area = 0;
            for (auto leaf : selection.leaves)
                if (leaf.face == face)
                    area += std::ldexp(1., -2 * int(leaf.level));
            Check(std::abs(area - 1) < 1e-12, "protected topology retains complete face coverage");
        }
        std::cout << "CHART max_inverse_m=" << maximumInverse
                  << " max_boundary_excess_m=" << maximumBoundary
                  << " max_jacobian_error=" << maximumDerivative << " seam_m=" << seamError
                  << " triangles=" << triangles << " volume_chunks=" << volumes
                  << " fixture_work_ms="
                  << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                               start)
                         .count()
                  << '\n';
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &exception) {
        std::cerr << "FAIL " << exception.what() << " after " << checks << " checks\n";
        return 1;
    }
}
