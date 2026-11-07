#include "EngineHost.h"
#include "terrain/TerrainLod.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>

int main() {
    unsigned checks = 0;
    try {
        auto check = [&](bool ok, const char *text) {
            ++checks;
            if (!ok)
                throw std::runtime_error(text);
        };
        EngineHost host;
        std::string error;
        check(host.Init("Terrain V2 GPU height and seam qualification", 128, 128, false, error),
              error.c_str());
        auto &renderer = host.GetRenderer();
        auto source = TerrainV2::Create(33, 33, .5f);
        for (unsigned z = 0; z < 33; ++z)
            for (unsigned x = 0; x < 33; ++x)
                source.heights[size_t(z) * 33 + x] =
                    .7f * std::sin(x * .23f) * std::cos(z * .17f) - .15f;
        const auto heights = renderer.CreateTexture(TerrainV2::HeightImage(source));
        auto referenceGrid = TerrainV2::HeightGrid(16);
        for (auto &v : referenceGrid.vertices)
            v.position.y = TerrainV2::LodHeight(source, v.uv * 32.f, 2);
        const auto referenceMesh = renderer.CreateMesh(referenceGrid);
        auto sourceFrame = glm::translate(glm::mat4(1), glm::vec3(31, -7, 19)) *
                           glm::mat4_cast(glm::angleAxis(.7f, glm::normalize(glm::vec3(1, 2, 3))));
        float maxError = 0;
        float maxShadowDifference = 0;
        for (unsigned placement = 0; placement < 2; ++placement) {
            const auto root = placement ? sourceFrame : glm::mat4(1);
            const auto invRoot = glm::inverse(root);
            auto view =
                glm::lookAt(glm::vec3(0, 10, 0), glm::vec3(0), glm::vec3(0, 0, -1)) * invRoot;
            auto projection = glm::ortho(-8.f, 8.f, -8.f, 8.f, 1.f, 20.f);
            auto inverse = glm::inverse(projection * view);
            renderer.BeginFrame(128, 128);
            renderer.SetCamera(view, projection);
            renderer.SetLighting({0, 1, 0}, {1, 1, 1}, {.2f, .2f, .2f});
            renderer.BeginTerrainMaterial(
                {}, {}, {glm::vec3(1), glm::vec3(1), glm::vec3(1), glm::vec3(1)}, invRoot);
            const glm::vec3 anchor = root * glm::vec4(-8, 0, -8, 1);
            const auto rotation = glm::quat_cast(root);
            renderer.DrawTerrainGrid(heights, 16, {0, 0, 2, 16}, {2, 2, 2, 2}, {33, 33}, .5f,
                                     anchor, rotation, 16, -1, 1, {1, 1, 1}, 1);
            renderer.EndTerrainMaterial();
            std::vector<float> depth(128 * 128);
            glReadPixels(0, 0, 128, 128, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
            for (int y = 4; y < 124; y += 7)
                for (int x = 4; x < 124; x += 7) {
                    const float d = depth[size_t(y) * 128 + x];
                    check(d > 0 && d < 1, "displaced shared grid covers the actual framebuffer");
                    glm::vec4 p =
                        inverse * glm::vec4((x + .5f) / 64 - 1, (y + .5f) / 64 - 1, d * 2 - 1, 1);
                    p /= p.w;
                    p = invRoot * p;
                    const float expected =
                        TerrainV2::LodHeight(source, {(p.x + 8) / .5f, (p.z + 8) / .5f}, 2);
                    maxError = std::max(maxError, std::abs(expected - p.y));
                }
            // The next ordinary draw must not inherit terrain vertex deformation.
            renderer.DrawBox(glm::vec3(root * glm::vec4(0, 2, 0, 1)), rotation, {1, .2f, 1},
                             {1, 0, 0}, 1);
            renderer.EndMeshBatch();
            float centre = 1;
            glReadPixels(64, 64, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &centre);
            glm::vec4 p = inverse * glm::vec4(1.f / 128, 1.f / 128, centre * 2 - 1, 1);
            p /= p.w;
            p = invRoot * p;
            check(std::abs(p.y - 2.2f) < .001f,
                  "ordinary geometry is restored after terrain rendering");
            renderer.EndFrame();
            auto shadow = [&](bool shared) {
                renderer.BeginShadowPass(0, projection * view);
                renderer.BeginTerrainMaterial(
                    {}, {}, {glm::vec3(1), glm::vec3(1), glm::vec3(1), glm::vec3(1)}, invRoot);
                if (shared)
                    renderer.DrawTerrainGrid(heights, 16, {0, 0, 2, 16}, {2, 2, 2, 2}, {33, 33},
                                             .5f, anchor, rotation, 16, -1, 1, {1, 1, 1}, 1);
                else
                    renderer.DrawMesh(referenceMesh, anchor, rotation, {16, 1, 16}, {}, {1, 1, 1},
                                      1);
                renderer.EndTerrainMaterial();
                renderer.EndMeshBatch();
                GLint viewport[4];
                glGetIntegerv(GL_VIEWPORT, viewport);
                std::vector<float> samples;
                for (unsigned y = 1; y < 10; ++y)
                    for (unsigned x = 1; x < 10; ++x) {
                        float depth = 1;
                        glReadPixels(int((x + .1) * viewport[2] / 11),
                                     int((y + .3) * viewport[3] / 11), 1, 1, GL_DEPTH_COMPONENT,
                                     GL_FLOAT, &depth);
                        check(depth > 0 && depth < 1,
                              "actual shadow map contains displaced terrain");
                        samples.push_back(depth);
                    }
                renderer.EndShadowPass();
                return samples;
            };
            const auto actualShadow = shadow(true), referenceShadow = shadow(false);
            for (size_t i = 0; i < actualShadow.size(); ++i)
                maxShadowDifference =
                    std::max(maxShadowDifference, std::abs(actualShadow[i] - referenceShadow[i]));
        }
        check(maxError < .0001f,
              "actual GPU heights agree with CPU source reconstruction in both owner frames");
        renderer.DestroyTexture(heights);
        renderer.DestroyMesh(referenceMesh);
        check(
            maxShadowDifference < .00001f,
            "actual shadow depth matches independent CPU-height geometry, including ordinary bias");
        check(glGetError() == GL_NO_ERROR, "terrain path produces no OpenGL error");
        std::cout << "METRIC maximum_GPU_height_error_m=" << maxError << '\n';
        std::cout << "METRIC maximum_shadow_depth_difference=" << maxShadowDifference << '\n';
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
