#include "EngineHost.h"
#include "ScreenshotWriter.h"
#include "terrain/TerrainLodTransition.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
using Selection = std::vector<TerrainV2::HeightPatch>;

float CoverageSample(unsigned x, unsigned y) {
    uint32_t h = x * 1664525u + y * 1013904223u;
    h ^= h >> 16;
    h *= 2246822519u;
    h ^= h >> 13;
    return (float(h & 1023u) + .5f) / 1024.f;
}

void History(const Selection &coarse, const Selection &fine) {
    TerrainV2::LodTransition history;
    history.Update(coarse, 1.f / 60);
    Check(!history.Active() && history.Current().size() == coarse.size(),
          "first view is immediately complete, without a fade from empty geometry");
    history.Update(fine, 1.f / 60);
    Check(history.Active() && history.Fraction() == 0 && history.Incoming().size() == fine.size(),
          "different detail starts one bounded transition between complete selections");
    history.Update(coarse, TerrainV2::LodTransition::Duration / 2);
    Check(std::abs(history.Fraction() - .5f) < 1e-6f && history.Incoming().size() == fine.size(),
          "reversing the requested camera does not restart or grow transition history");
    history.Update(fine, TerrainV2::LodTransition::Duration);
    Check(!history.Active() && history.Current().size() == fine.size(),
          "completed transition releases the outgoing selection");
    for (unsigned i = 0; i < 500; ++i) {
        history.Update(i % 2 ? fine : coarse, 1.f / 60);
        Check(history.Current().size() + history.Incoming().size() <=
                  2 * std::max(fine.size(), coarse.size()),
              "rapid multi-camera changes retain at most two complete selections");
    }
    history.Update(coarse, 0);
    Check(!history.Active() && history.Current().size() == coarse.size(),
          "zero-time observer gets its current union without advancing gameplay history");
    history.Update(fine, std::numeric_limits<float>::quiet_NaN());
    Check(!history.Active() && history.Current().size() == fine.size(),
          "invalid presentation time cannot poison a persistent transition");
    auto changedEdge = fine;
    changedEdge.front().edgeStrides.x *= 2;
    history.Update(changedEdge, .01f);
    size_t shared = 0;
    for (const auto &p : fine)
        shared += history.Shared(p.key);
    Check(history.Active() && shared == fine.size() - 1 && !history.Shared(fine.front().key),
          "unchanged patches render once; same key with changed seam cannot be reused");
    history.Reset();
    Check(history.Current().empty() && history.Incoming().empty() &&
              !history.Shared(fine.back().key),
          "source publication drops all old-detail history before drawing new topology");
    history.Update(coarse, .01f);
    history.Update({}, .01f);
    Check(history.Current().empty() && !history.Active(),
          "an empty published selection cannot retain obsolete terrain");
}
} // namespace

int main(int argc, char **argv) {
    try {
        auto source = TerrainV2::Create(33, 33, .5f);
        for (unsigned z = 0; z < 33; ++z)
            for (unsigned x = 0; x < 33; ++x)
                source.heights[size_t(z) * 33 + x] = .6f * std::sin(x * 1.7f) * std::cos(z * 1.3f);
        TerrainV2::HeightLod lod(source);
        auto coarse = lod.Select({{{0, 100000, 0}, 1, 0}}, {});
        auto fine = lod.Select({{{0, 8, 0}, 600, 1000}}, {});
        Check(coarse.size() == 1 && fine.size() == 4,
              "fixture genuinely changes quadtree topology and reconstructed heights");
        History(coarse, fine);

        EngineHost host;
        std::string error;
        Check(host.Init("Terrain V2 complementary colour/shadow coverage", 960, 600, false, error),
              error.c_str());
        auto &renderer = host.GetRenderer();
        auto heights = renderer.CreateTexture(TerrainV2::HeightImage(source));
        auto view = glm::lookAt(glm::vec3(0, 10, 0), glm::vec3(0), glm::vec3(0, 0, -1));
        auto projection = glm::ortho(-7.f, 7.f, -7.f, 7.f, .1f, 30.f);
        auto draw = [&](const Selection &patches, glm::vec2 coverage) {
            for (const auto &p : patches) {
                const auto k = p.key;
                const float step = float(1u << k.level);
                renderer.DrawTerrainGrid(heights, 16, {k.x * 16.f, k.z * 16.f, step, 16},
                                         p.edgeStrides, {33, 33}, .5f,
                                         {k.x * 8.f - 8, 0, k.z * 8.f - 8}, glm::quat(1, 0, 0, 0),
                                         8 * step, -1, 1, {1, 1, 1}, 1, coverage);
            }
        };
        struct Image {
            std::vector<float> depth;
            unsigned width = 0, height = 0, stepX = 1, stepY = 1;
        };
        auto capture = [&](bool shadow, float alpha, bool reverse) {
            renderer.BeginFrame(128, 128);
            renderer.SetCamera(view, projection);
            renderer.SetLighting({0, 1, 0}, {1, 1, 1}, {.2f, .2f, .2f});
            if (shadow) {
                renderer.BeginShadowPass(0, projection * view);
                // Exercise the actual shadow FBO/program with a bounded test
                // viewport; the existing terrain render suite checks full maps.
                glViewport(0, 0, 128, 128);
            }
            renderer.BeginTerrainMaterial(
                {}, {}, {glm::vec3(1), glm::vec3(1), glm::vec3(1), glm::vec3(1)}, glm::mat4(1));
            if (reverse) {
                draw(fine, {0, alpha});
                draw(coarse, {alpha, 1});
            } else {
                draw(coarse, {alpha, 1});
                draw(fine, {0, alpha});
            }
            renderer.EndTerrainMaterial();
            renderer.EndMeshBatch();
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);
            Image image;
            image.width = viewport[2];
            image.height = viewport[3];
            image.stepX = std::max(1u, image.width / 128);
            image.stepY = std::max(1u, image.height / 128);
            std::vector<float> pixels(size_t(image.width) * image.height);
            glReadPixels(0, 0, image.width, image.height, GL_DEPTH_COMPONENT, GL_FLOAT,
                         pixels.data());
            for (unsigned y = 0; y < image.height; y += image.stepY)
                for (unsigned x = 0; x < image.width; x += image.stepX) {
                    image.depth.push_back(pixels[size_t(y) * image.width + x]);
                }
            if (shadow)
                renderer.EndShadowPass();
            renderer.EndFrame();
            return image;
        };
        size_t changed = 0;
        float largestError = 0;
        for (bool shadow : {false, true}) {
            const auto before = capture(shadow, 0, false);
            const auto after = capture(shadow, 1, false);
            Check(before.depth.size() == after.depth.size(), "stable capture dimensions");
            for (size_t i = 0; i < before.depth.size(); ++i)
                changed += std::abs(before.depth[i] - after.depth[i]) > 1e-5f;
            for (float alpha : {.125f, .5f, .875f}) {
                auto image = capture(shadow, alpha, false);
                auto reverse = capture(shadow, alpha, true);
                size_t index = 0;
                for (unsigned y = 0; y < image.height; y += image.stepY)
                    for (unsigned x = 0; x < image.width; x += image.stepX, ++index) {
                        const float expected =
                            CoverageSample(x, y) < alpha ? after.depth[index] : before.depth[index];
                        largestError =
                            std::max(largestError, std::abs(image.depth[index] - expected));
                        Check(image.depth[index] < 1 &&
                                  std::abs(image.depth[index] - expected) < 2e-6f &&
                                  image.depth[index] == reverse.depth[index],
                              "each colour/shadow sample owns exactly the expected detail, no seam "
                              "holes or draw-order dependence");
                    }
            }
        }
        Check(changed > 1000, "negative control: compared detail levels are actually different");
        Check(glGetError() == GL_NO_ERROR, "transition draws produce no OpenGL error");
        renderer.DestroyTexture(heights);
        if (argc > 1) {
            const std::filesystem::path output(argv[1]);
            Check(!std::filesystem::exists(output), "visual evidence directory is fresh");
            std::filesystem::create_directories(output);
            auto landscape = TerrainV2::Create(257, 257, 2);
            for (unsigned z = 0; z < 257; ++z)
                for (unsigned x = 0; x < 257; ++x)
                    landscape.heights[size_t(z) * 257 + x] = 25 * std::sin(x * .037f) +
                                                             17 * std::cos(z * .051f) +
                                                             7 * std::sin((x + z) * .11f);
            TerrainV2::HeightLod hierarchy(landscape);
            const glm::vec3 eye(170, 100, 190);
            const auto distant = hierarchy.Select({{{0, 100000, 0}, 1, 0}}, {});
            const auto detailed = hierarchy.Select({{eye, 460, 0}}, {});
            Check(detailed.size() > distant.size(), "visual landscape actually changes detail");
            heights = renderer.CreateTexture(TerrainV2::HeightImage(landscape));
            auto drawLandscape = [&](const Selection &selected, glm::vec2 coverage) {
                for (const auto &p : selected) {
                    const auto k = p.key;
                    const float step = float(1u << k.level);
                    renderer.DrawTerrainGrid(
                        heights, 16, {k.x * 16.f, k.z * 16.f, step, 16}, p.edgeStrides, {257, 257},
                        2, {k.x * 32.f - 256, 0, k.z * 32.f - 256}, glm::quat(1, 0, 0, 0),
                        32 * step, p.minimum, p.maximum, {.45f, .64f, .32f}, 1, coverage);
                }
            };
            for (unsigned frame = 0; frame <= 12; ++frame) {
                renderer.BeginFrame(960, 600);
                renderer.SetCamera(glm::lookAt(eye, {0, 10, 0}, {0, 1, 0}),
                                   glm::perspective(glm::radians(65.f), 1.6f, .1f, 2000.f));
                renderer.SetLighting({.4f, 1, .2f}, {1, 1, 1}, {.3f, .3f, .3f});
                renderer.BeginTerrainMaterial(
                    {}, {}, {glm::vec3(1), glm::vec3(1), glm::vec3(1), glm::vec3(1)}, glm::mat4(1));
                float alpha = float(frame) / 12;
                drawLandscape(distant, {alpha, 1});
                drawLandscape(detailed, {0, alpha});
                renderer.EndTerrainMaterial();
                renderer.EndFrame();
                std::vector<unsigned char> rgb;
                renderer.CaptureFrame(960, 600, rgb);
                Check(WriteRgbPng((output / (std::to_string(frame) + ".png")).string(), 960, 600,
                                  rgb),
                      "capture actual landscape transition frame");
            }
            renderer.DestroyTexture(heights);
        }
        std::cout << "METRIC distinct_depth_samples=" << changed
                  << " max_coverage_depth_error=" << largestError << '\n';
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
