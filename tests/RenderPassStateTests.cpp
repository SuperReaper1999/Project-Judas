// Independent colour/continuation controls for public render-target scopes.
// This fixture intentionally also links against the frozen baseline: it must
// expose the historical outer-HDR feedback, not merely compare two new paths.
#include "EngineHost.h"
#include "GraphicsApi.h"
#include "ScreenshotWriter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

namespace {
constexpr int Width = 96, Height = 64;
unsigned checks = 0, failures = 0;
using Pixels = std::vector<unsigned char>;
void Check(bool okay, const char *message) {
    ++checks;
    failures += !okay;
    std::printf("%s %s\n", okay ? "PASS" : "FAIL", message);
    std::fflush(stdout);
}
glm::vec3 Display(glm::vec3 linear) {
    linear /= 1.f + glm::dot(linear, glm::vec3(.2126f, .7152f, .0722f));
    for (unsigned channel = 0; channel < 3; ++channel)
        linear[channel] = linear[channel] <= .0031308f
                              ? 12.92f * linear[channel]
                              : 1.055f * std::pow(linear[channel], 1.f / 2.4f) - .055f;
    return linear * 255.f;
}
glm::vec3 Pixel(const Pixels &pixels, int x, int y, int width) {
    const auto i = std::size_t(y * width + x) * 3;
    if (i + 2 >= pixels.size())
        return {-1000, -1000, -1000};
    return {pixels[i], pixels[i + 1], pixels[i + 2]};
}
bool Near(glm::vec3 a, glm::vec3 b) {
    return glm::all(glm::lessThanEqual(glm::abs(a - b), glm::vec3(3)));
}
Pixels Capture(Renderer &renderer, int width, int height) {
    Pixels result;
    renderer.CaptureFrame(width, height, result);
    return result;
}
void Camera(Renderer &renderer, float x = 0) {
    renderer.SetCamera(glm::lookAt(glm::vec3(x, 0, 4), glm::vec3(x, 0, 0), {0, 1, 0}),
                       glm::ortho(-2.f, 2.f, -1.3333333f, 1.3333333f, .1f, 10.f));
}
MaterialHandle Material(Renderer &renderer, glm::vec3 colour) {
    MaterialDefinition material;
    material.model = MaterialModel::Unlit;
    material.baseColor = {colour, 1};
    return renderer.CreateMaterial(material);
}
void Box(Renderer &renderer, MaterialHandle material, glm::vec3 position) {
    MaterialBinding binding;
    binding.handle = material;
    renderer.SetMaterialBindings({binding});
    renderer.DrawBox(position, {1, 0, 0, 0}, {.7f, .7f, .1f}, {1, 1, 1});
}
void BeginMain(Renderer &renderer) {
    renderer.SetSceneAppearance(true, 1, {}, 1, {1, 0, 0, 0}, false, {0, 0, 0});
    renderer.ClearWeather();
    renderer.SetMaterialBindings({});
    renderer.BeginFrame(Width, Height);
    Camera(renderer);
    renderer.SetDynamicLights({});
    renderer.SetLighting({0, 1, 0}, {0, 0, 0}, {0, 0, 0});
}
} // namespace

int main(int argc, char **argv) {
    EngineHost host;
    std::string error;
    if (!host.Init("Scoped HDR render passes", Width, Height, false, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    auto &renderer = host.GetRenderer();
    renderer.SetShadowBudget(0);
    renderer.SetSceneAppearance(true, 1, {}, 1, {1, 0, 0, 0}, false, {0, 0, 0});
    const glm::vec3 redColour{.5f, .015f, .01f}, greenColour{.01f, .5f, .015f},
        blueColour{.015f, .01f, .5f};
    const auto red = Material(renderer, redColour), green = Material(renderer, greenColour),
               blue = Material(renderer, blueColour);
    Check(red.IsValid() && green.IsValid() && blue.IsValid(),
          "independent unlit colour materials are available");
    std::vector<RenderTargetHandle> targets;
    for (unsigned index = 0; index < 12; ++index) {
        const int width = index ? 43 + int(index) * 3 : Width;
        const int height = index ? 29 + int(index) * 2 : Height;
        targets.push_back(renderer.CreateRenderTarget(width, height, error, true));
    }
    Check(std::all_of(targets.begin(), targets.end(),
                      [](auto target) { return target.IsValid(); }),
          "same-size and twelve distinct auxiliary outputs are allocated");
    if (failures)
        return 1;

    const auto auxiliary = [&](RenderTargetHandle target, bool explicitEnd = true) {
        if (!renderer.BeginRenderTarget(target))
            return Pixels{};
        Camera(renderer, 10);
        Box(renderer, green, {10, 0, 0});
        Pixels pixels;
        if (explicitEnd) {
            renderer.EndFrame();
            const auto size = renderer.RenderTargetSize(target);
            pixels = Capture(renderer, size.x, size.y);
        }
        renderer.EndRenderTarget();
        return pixels;
    };
    const auto mainAnswer = [&](const Pixels &pixels) {
        return Near(Pixel(pixels, Width / 4, Height / 2, Width), Display(redColour)) &&
               Near(Pixel(pixels, 3 * Width / 4, Height / 2, Width), Display(blueColour)) &&
               Near(Pixel(pixels, Width / 2, 2, Width), {0, 0, 0});
    };

    // Legal auxiliary-before-main order is an independent positive control.
    auto pixels = auxiliary(targets[0]);
    Check(Near(Pixel(pixels, Width / 2, Height / 2, Width), Display(greenColour)),
          "sequential auxiliary display matches independent tone-map/sRGB arithmetic");
    Check(renderer.RenderTargetDiagnostics().defaultFramebuffer,
          "sequential auxiliary returns to the external default framebuffer");
    BeginMain(renderer);
    Box(renderer, red, {-1, 0, 0});
    Box(renderer, blue, {1, 0, 0});
    renderer.EndFrame();
    const auto sequential = Capture(renderer, Width, Height);
    Check(mainAnswer(sequential), "sequential main colours agree with known physical linear inputs");

    Pixels nested;
    for (unsigned frame = 0; frame < 4; ++frame) {
        BeginMain(renderer);
        Box(renderer, red, {-1, 0, 0});
        const auto parent = renderer.RenderTargetDiagnostics();
        const auto parentView = renderer.ViewMatrix(), parentProjection = renderer.ProjectionMatrix();
        GLint parentFbo = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &parentFbo);
        Check(!renderer.BeginRenderTarget({999999}) &&
                  renderer.RenderTargetDiagnostics().viewport == parent.viewport &&
                  renderer.ViewMatrix() == parentView,
              "invalid target leaves the active main HDR view untouched");
        pixels = auxiliary(targets[0]);
        Check(Near(Pixel(pixels, Width / 2, Height / 2, Width), Display(greenColour)),
              "same-size child uses independent scratch and resolves only its own green image");
        GLint restored = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &restored);
        Check(parentFbo != 0 && restored == parentFbo && glIsFramebuffer(GLuint(parentFbo)) &&
                  renderer.RenderTargetDiagnostics().viewport == parent.viewport &&
                  renderer.ViewMatrix() == parentView &&
                  renderer.ProjectionMatrix() == parentProjection,
              "child restores the suspended main chart, viewport and camera");
        Box(renderer, blue, {1, 0, 0});
        renderer.EndFrame();
        nested = Capture(renderer, Width, Height);
        Check(renderer.RenderTargetDiagnostics().defaultFramebuffer && mainAnswer(nested) &&
                  nested == sequential,
              "main continuation resolves once to the original output without frame-history feedback");
    }

    // Eviction must not delete the oldest main chart while it is suspended.
    BeginMain(renderer);
    Box(renderer, red, {-1, 0, 0});
    GLint original = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &original);
    bool protectedParent = true;
    for (std::size_t index = 1; index < targets.size(); ++index) {
        auxiliary(targets[index], index + 1 < targets.size());
        GLint current = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &current);
        protectedParent &= current == original && glIsFramebuffer(GLuint(original));
    }
    Check(protectedParent, "more than eight child sizes cannot evict the suspended main HDR chart");
    Box(renderer, blue, {1, 0, 0});
    renderer.EndFrame();
    pixels = Capture(renderer, Width, Height);
    Check(renderer.RenderTargetDiagnostics().defaultFramebuffer && mainAnswer(pixels) &&
              pixels == sequential,
          "cache churn and implicit child resolve preserve original main content and continuation");

    const auto last = targets.back();
    Check(renderer.BeginRenderTarget(last, true), "resolved child can be reopened for a read-only overlay");
    const auto beforeRejected = renderer.RenderTargetDiagnostics();
    Check(!renderer.BeginRenderTarget(targets[0]) &&
              renderer.RenderTargetDiagnostics().viewport == beforeRejected.viewport &&
              renderer.RenderTargetDiagnostics().passActive,
          "a second nested target is explicitly rejected without disturbing the first");
    const auto size = renderer.RenderTargetSize(last);
    const auto lastPixels = Capture(renderer, size.x, size.y);
    Check(Near(Pixel(lastPixels, size.x / 2, size.y / 2, size.x), Display(greenColour)),
          "EndRenderTarget implicitly resolves an unfinished child frame");
    renderer.EndRenderTarget();
    Check(renderer.RenderTargetDiagnostics().defaultFramebuffer,
          "preserved target readback restores the external framebuffer");
    Check(glGetError() == GL_NO_ERROR, "scoped render passes leave no graphics errors");

    if (argc > 1) {
        const std::filesystem::path output(argv[1]);
        std::filesystem::create_directories(output);
        Check(WriteRgbPng((output / "sequential.png").string(), Width, Height, sequential) &&
                  WriteRgbPng((output / "nested.png").string(), Width, Height, nested) &&
                  WriteRgbPng((output / "cache-churn.png").string(), Width, Height, pixels),
              "independent main-view comparisons are recorded");
    }
    for (auto target : targets)
        renderer.DestroyRenderTarget(target);
    renderer.DestroyMaterial(red);
    renderer.DestroyMaterial(green);
    renderer.DestroyMaterial(blue);
    host.Shutdown();
    std::printf("Scoped HDR passes: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
