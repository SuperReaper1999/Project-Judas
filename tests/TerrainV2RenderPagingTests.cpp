#include "EngineHost.h"
#include "terrain/TerrainRenderPages.h"
#include "terrain/TerrainSamples.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>

int main() {
    unsigned checks = 0;
    try {
        auto check = [&](bool ok, const std::string &text) {
            ++checks;
            if (!ok)
                throw std::runtime_error(text);
        };
        EngineHost host;
        std::string error;
        check(host.Init("Terrain GPU paging parity", 128, 128, false, error), error);
        auto &renderer = host.GetRenderer();
        auto source = TerrainV2::Create(65, 65, .5f, 16);
        for (unsigned z = 0; z < 65; ++z)
            for (unsigned x = 0; x < 65; ++x) {
                const size_t i = size_t(z) * 65 + x;
                source.heights[i] = .5f * std::sin(x * .2f) * std::cos(z * .3f);
                const float w = float((x + z) % 17) / 16;
                source.weights[i] = {w, 1 - w, 0, 0};
            }
        const auto originalHeight = renderer.CreateTexture(TerrainV2::HeightImage(source));
        TextureData weight;
        weight.width = weight.height = 65;
        for (auto w : source.weights)
            for (unsigned c = 0; c < 4; ++c)
                weight.pixels.push_back(uint8_t(std::lround(w[c] * 255)));
        const auto originalWeight = renderer.CreateTexture(weight);
        TerrainV2::RenderPages pages(32 * 1024);
        size_t maxColourError = 0;
        double maxDepthError = 0;
        for (unsigned repeat = 0; repeat < 3; ++repeat)
            for (unsigned z = 0; z < 4; ++z)
                for (unsigned x = 0; x < 4; ++x) {
                    TerrainV2::RenderPageKey key{{x, z, 0}, {2, 1, 2, 1}, true};
                    const auto &page = pages.Acquire(renderer, source, key);
                    const glm::vec3 anchor(float(x) * 8 - 16, 0, float(z) * 8 - 16);
                    const auto view = glm::lookAt(anchor + glm::vec3(4, 10, 4),
                                                  anchor + glm::vec3(4, 0, 4), glm::vec3(0, 0, -1));
                    const auto projection = glm::ortho(-3.8f, 3.8f, -3.8f, 3.8f, 1.f, 20.f);
                    std::vector<uint8_t> colour[2];
                    std::vector<float> depth[2];
                    for (unsigned path = 0; path < 2; ++path) {
                        renderer.BeginFrame(128, 128);
                        renderer.SetCamera(view, projection);
                        renderer.SetLighting({0, 1, 0}, {1, 1, 1}, {.2f, .2f, .2f});
                        renderer.BeginTerrainMaterial(path ? page.weights : originalWeight, {},
                                                      source.layerColours, glm::mat4(1));
                        if (path)
                            renderer.SetTerrainWeightPage(page.weights, page.weightRect);
                        renderer.DrawTerrainGrid(path ? page.heights : originalHeight, 16,
                                                 {float(x * 16), float(z * 16), 1, 16}, key.edges,
                                                 {65, 65}, .5f, anchor, {1, 0, 0, 0}, 8, -1, 1,
                                                 {1, 1, 1}, 1, {0, 1}, bool(path));
                        renderer.EndTerrainMaterial();
                        renderer.EndMeshBatch();
                        colour[path].resize(128 * 128 * 4);
                        depth[path].resize(128 * 128);
                        glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE,
                                     colour[path].data());
                        glReadPixels(0, 0, 128, 128, GL_DEPTH_COMPONENT, GL_FLOAT,
                                     depth[path].data());
                        renderer.EndFrame();
                    }
                    for (size_t i = 0; i < colour[0].size(); ++i)
                        maxColourError =
                            std::max(maxColourError,
                                     size_t(std::abs(int(colour[0][i]) - int(colour[1][i]))));
                    for (size_t i = 0; i < depth[0].size(); ++i)
                        maxDepthError =
                            std::max(maxDepthError, std::abs(double(depth[0][i]) - depth[1][i]));
                    if (!repeat && !x && !z) {
                        auto shadow = [&](bool paged) {
                            renderer.BeginShadowPass(0, projection * view);
                            renderer.BeginTerrainMaterial(paged ? page.weights : originalWeight, {},
                                                          source.layerColours, glm::mat4(1));
                            if (paged)
                                renderer.SetTerrainWeightPage(page.weights, page.weightRect);
                            renderer.DrawTerrainGrid(paged ? page.heights : originalHeight, 16,
                                                     {float(x * 16), float(z * 16), 1, 16},
                                                     key.edges, {65, 65}, .5f, anchor, {1, 0, 0, 0},
                                                     8, -1, 1, {1, 1, 1}, 1, {0, 1}, paged);
                            renderer.EndTerrainMaterial();
                            renderer.EndMeshBatch();
                            GLint viewport[4];
                            glGetIntegerv(GL_VIEWPORT, viewport);
                            std::vector<float> values;
                            for (unsigned y = 1; y < 8; ++y)
                                for (unsigned x = 1; x < 8; ++x) {
                                    float value = 1;
                                    glReadPixels(int((x + .1) * viewport[2] / 9),
                                                 int((y + .3) * viewport[3] / 9), 1, 1,
                                                 GL_DEPTH_COMPONENT, GL_FLOAT, &value);
                                    check(value > 0 && value < 1,
                                          "paged shadow map contains real terrain");
                                    values.push_back(value);
                                }
                            renderer.EndShadowPass();
                            return values;
                        };
                        const auto legacy = shadow(false), paged = shadow(true);
                        double error = 0;
                        for (size_t i = 0; i < legacy.size(); ++i)
                            error = std::max(error, std::abs(double(legacy[i]) - paged[i]));
                        check(error < 1e-6,
                              "paged shadow depth matches complete native height source");
                    }
                    check(pages.Statistics().bytes <= 32 * 1024,
                          "GPU residency stays within 32 KiB across traversal");
                }
        check(maxDepthError < 2e-6, "paged height/stitched geometry matches full source depth");
        check(maxColourError <= 2, "paged near material weights/normals match full source colour");
        const auto stats = pages.Statistics();
        check(stats.evictions > 20, "repeated native page loading exercises actual GPU eviction");
        std::cout << "METRIC gpu_bytes=" << stats.bytes << " gpu_peak=" << stats.peakBytes
                  << " budget=" << stats.budget << " evictions=" << stats.evictions
                  << " pages=" << stats.entries << " max_colour_error=" << maxColourError
                  << " max_depth_error=" << maxDepthError
                  << " worst_prepare_ms=" << pages.worstPrepareMs << "\n";
        pages.Clear(renderer);
        check(pages.Statistics().bytes == 0 && pages.Statistics().entries == 0,
              "release every cached GPU resource");
        // Flat landscapes can select a coarse HEIGHT grid right beside the
        // camera. Small painted details must retain native material resolution.
        for (unsigned authoredCells : {32u, 9u}) {
            const unsigned side = authoredCells == 32 ? 16 : 32,
                           dimensions = authoredCells * side + 1;
            const float extent = dimensions - 1, half = extent * .5f;
            auto flat = TerrainV2::Create(dimensions, dimensions, 1, authoredCells);
            flat.weights[(dimensions / 2) * dimensions + dimensions / 2] = {0, 1, 0, 0};
            flat.weights[(dimensions / 2) * dimensions + dimensions / 2 + 128] = {0, 1, 0, 0};
            if (authoredCells == 9)
                for (unsigned z = 0; z < dimensions; ++z)
                    for (unsigned x = 0; x < dimensions; ++x)
                        flat.heights[size_t(z) * dimensions + x] =
                            .05f * std::sin(x * .1f) * std::cos(z * .1f);
            flat.layerColours = {glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0),
                                 glm::vec3(0)};
            TextureData flatWeights;
            flatWeights.width = flatWeights.height = dimensions;
            for (auto w : flat.weights)
                for (unsigned c = 0; c < 4; ++c)
                    flatWeights.pixels.push_back(uint8_t(std::lround(w[c] * 255)));
            const auto flatWeight = renderer.CreateTexture(flatWeights);
            const auto flatHeight = renderer.CreateTexture(TerrainV2::HeightImage(flat));
            TerrainV2::RenderPages native(2 * 1024 * 1024);
            TerrainV2::HeightPatch root{{0, 0, side == 16 ? 4u : 5u}, glm::vec4(side), 0, 0};
            const auto partitions = TerrainV2::PartitionRenderPatch(flat, root);
            check(partitions.size() == 4,
                  "large flat geometric patch partitions native material coverage");
            for (float offset : {0.f, 128.f}) {
                std::vector<uint8_t> paint[2];
                std::vector<float> paintDepth[2];
                for (unsigned path = 0; path < 2; ++path) {
                    renderer.BeginFrame(128, 128);
                    renderer.SetCamera(glm::lookAt(glm::vec3(offset, 10, 0),
                                                   glm::vec3(offset, 0, 0), glm::vec3(0, 0, -1)),
                                       glm::ortho(-1.5f, 1.5f, -1.5f, 1.5f, 1.f, 20.f));
                    renderer.SetLighting({0, 1, 0}, {1, 1, 1}, {.2f, .2f, .2f});
                    renderer.BeginTerrainMaterial(flatWeight, {}, flat.layerColours, glm::mat4(1));
                    if (!path)
                        renderer.DrawTerrainGrid(
                            flatHeight, authoredCells, {0, 0, float(side), float(authoredCells)},
                            glm::vec4(side), glm::vec2(dimensions), 1, {-half, 0, -half},
                            {1, 0, 0, 0}, extent, 0, 0, {1, 1, 1}, 1);
                    else
                        for (const auto &key : partitions) {
                            const auto &page = native.Acquire(renderer, flat, key);
                            renderer.SetTerrainWeightPage(page.weights, page.weightRect);
                            renderer.DrawTerrainGrid(page.heights, key.cells,
                                                     {float(key.firstX), float(key.firstZ),
                                                      float(key.stride), float(key.cells)},
                                                     key.edges, glm::vec2(dimensions), 1,
                                                     {key.firstX - half, 0, key.firstZ - half},
                                                     {1, 0, 0, 0}, float(key.extentX), 0, 0,
                                                     {1, 1, 1}, 1, {0, 1}, true, key.cellsZ,
                                                     float(key.extentZ));
                        }
                    renderer.EndTerrainMaterial();
                    renderer.EndMeshBatch();
                    paint[path].resize(128 * 128 * 4);
                    glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE, paint[path].data());
                    paintDepth[path].resize(128 * 128);
                    glReadPixels(0, 0, 128, 128, GL_DEPTH_COMPONENT, GL_FLOAT,
                                 paintDepth[path].data());
                    renderer.EndFrame();
                }
                unsigned colourError = 0;
                for (size_t i = 0; i < paint[0].size(); ++i)
                    colourError = std::max(colourError,
                                           unsigned(std::abs(int(paint[0][i]) - int(paint[1][i]))));
                double depthError = 0;
                for (size_t i = 0; i < paintDepth[0].size(); ++i)
                    depthError =
                        std::max(depthError, std::abs(double(paintDepth[0][i]) - paintDepth[1][i]));
                check(depthError < 2e-6,
                      "odd rectangular strips retain original coarse height and depth");
                check(colourError <= 2, "single-sample paint remains within RGBA8 tolerance beside "
                                        "a coarse height grid");
                const auto centre = (64 * 128 + 64) * 4;
                check(paint[1][centre + 1] > paint[1][centre],
                      "known-answer native green paint is visible at the camera");
            }
            auto alternate = partitions.front();
            alternate.stride /= 2;
            alternate.cells *= 2;
            alternate.cellsZ *= 2;
            const auto texture = native.Acquire(renderer, flat, partitions.front()).weights;
            const auto sharedTexture = native.Acquire(renderer, flat, alternate).weights;
            check(sharedTexture.id == texture.id && sharedTexture.generation == texture.generation,
                  "different geometric LODs share one native material page");
            native.Clear(renderer);
            check(native.Statistics().bytes == 0, "shared material pages release exactly once");
            renderer.DestroyTexture(flatWeight);
            renderer.DestroyTexture(flatHeight);
        }
        renderer.DestroyTexture(originalHeight);
        renderer.DestroyTexture(originalWeight);
        host.Shutdown();
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
