// Production submission controls: independent compositing answers and matched
// public batch/immediate paths. No renderer implementation is copied here.
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
constexpr int Width = 640, Height = 360;
unsigned checks = 0, failures = 0;
using Pixels = std::vector<unsigned char>;
void Check(bool value, const char *message) {
    ++checks;
    failures += !value;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", message);
    std::fflush(stdout);
}
glm::vec3 Pixel(const Pixels &image, int x = Width / 2, int y = Height / 2, int width = Width) {
    const auto offset = std::size_t(y * width + x) * 3;
    if (offset + 2 >= image.size())
        return {-1000, -1000, -1000};
    return {image[offset], image[offset + 1], image[offset + 2]};
}
double MeanDifference(const Pixels &a, const Pixels &b) {
    if (a.size() != b.size() || a.empty())
        return 1000;
    double sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        sum += std::abs(int(a[i]) - int(b[i]));
    return sum / double(a.size());
}
bool Near(glm::vec3 actual, glm::vec3 expected, float tolerance = 2) {
    return glm::all(glm::lessThanEqual(glm::abs(actual - expected), glm::vec3(tolerance)));
}
Pixels Capture(Renderer &r, int width = Width, int height = Height) {
    Pixels result;
    r.CaptureFrame(width, height, result);
    return result;
}
void Begin(Renderer &r, int width = Width, int height = Height) {
    r.ResetStats();
    r.SetMaterialBindings({});
    r.SetSceneAppearance(false, 1, {}, 1, {1, 0, 0, 0}, false, {0, 0, 0});
    r.ClearWeather();
    r.SetRenderLayer(0);
    r.SetRenderMask(kAllCategories);
    r.BeginFrame(width, height);
    CameraLens lens;
    lens.projection = CameraProjectionKind::Orthographic;
    lens.height = 6;
    r.SetCameraFrame(BuildCameraFrame({0, 0, 10}, {1, 0, 0, 0}, 60, .1f, 100, lens, width, height));
    r.SetDynamicLights({});
    r.SetLighting({0, 1, 0}, {0, 0, 0}, {1, 1, 1});
}
Pixels SpritePair(Renderer &r, TextureHandle texture, bool reversed, bool wrongOrder = false) {
    Begin(r);
    SpriteSettings bottom, top;
    bottom.size = {3, 3};
    top.size = {2, 2};
    top.order = wrongOrder ? -1 : 1;
    const auto red = [&] {
        r.QueueSprite(bottom, {0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, texture, {1, 0, 0, 1}, 1);
    };
    const auto blue = [&] {
        r.QueueSprite(top, {0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, texture, {0, 0, 1, .5f}, 2);
    };
    if (reversed) {
        blue();
        red();
    } else {
        red();
        blue();
    }
    r.FlushSprites();
    r.EndFrame();
    return Capture(r);
}
Pixels Particles(Renderer &r, TextureHandle texture, bool reversed, bool swappedDepth = false) {
    Begin(r);
    ParticleBillboard red{{0, 0, 0}, 3, {1, 0, 0, 1}}, blue{{0, 0, 1}, 2, {0, 0, 1, .5f}};
    if (swappedDepth)
        std::swap(red.position.z, blue.position.z);
    VisualBounds bounds;
    bounds.Include({-2, -2, -1});
    bounds.Include({2, 2, 2});
    const std::vector<ParticleBillboard> input = reversed ? std::vector<ParticleBillboard>{blue, red}
                                                          : std::vector<ParticleBillboard>{red, blue};
    r.DrawParticles(input, bounds, texture);
    r.EndFrame();
    return Capture(r);
}
Pixels Materials(Renderer &r, const std::vector<MaterialHandle> &materials, bool batching) {
    r.SetBatchingEnabled(batching);
    Begin(r);
    r.SetSceneAppearance(true, 1, {}, 1, {1, 0, 0, 0}, false, {.025f, .03f, .04f});
    r.SetCamera(glm::lookAt(glm::vec3(7, 6, 11), glm::vec3(0, 0, 0), {0, 1, 0}),
                glm::perspective(glm::radians(55.f), float(Width) / Height, .1f, 100.f));
    r.SetLighting(glm::normalize(glm::vec3(.4f, 1, .2f)), {.8f, .7f, .6f}, {.14f, .17f, .2f});
    r.BeginMeshBatch();
    for (unsigned i = 0; i < 64; ++i) {
        MaterialBinding binding;
        binding.handle = materials[i % materials.size()];
        binding.overrides.roughness = .15f + float(i % 6) * .13f;
        r.SetMaterialBindings({binding});
        r.DrawBox({float(int(i % 8) - 4), 0, float(int(i / 8) - 4)},
                  glm::angleAxis(float(i % 5) * .23f, glm::vec3(0, 1, 0)),
                  {i % 3 ? .35f : -.35f, .3f + float(i % 5) * .04f, .35f},
                  {.7f + float(i % 3) * .1f, .8f, .9f});
    }
    r.EndMeshBatch();
    r.FlushMaterialBlends();
    r.EndFrame();
    return Capture(r);
}
MeshData Triangle() {
    MeshData data;
    data.vertices = {{{-1, -1, 0}, {0, 0, 1}, {0, 0}},
                     {{1, -1, 0}, {0, 0, 1}, {1, 0}},
                     {{0, 1, 0}, {0, 0, 1}, {.5f, 1}}};
    return data;
}
} // namespace

int main(int argc, char **argv) {
    const auto out = std::filesystem::absolute(argc > 1 ? argv[1] : ".cache/renderer-v2-submission");
    std::filesystem::create_directories(out);
    EngineHost host;
    std::string error;
    if (!host.Init("Renderer V2 submission controls", Width, Height, false, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    SDL_GL_SetSwapInterval(0);
    auto &r = host.GetRenderer();
    TextureData white;
    white.width = white.height = 1;
    white.pixels = {255, 255, 255, 255};
    const auto texture = r.CreateTexture(white);
    Check(texture.IsValid(), "ordinary shared texture is created through the renderer owner");
    const auto ordered = SpritePair(r, texture, false);
    const auto reversed = SpritePair(r, texture, true);
    const auto wrong = SpritePair(r, texture, false, true);
    Check(Near(Pixel(ordered), {127.5f, 0, 127.5f}),
          "sprite ordering reproduces independently known opaque-red / half-blue compositing");
    Check(MeanDifference(ordered, reversed) == 0,
          "insertion permutation preserves explicit sprite layer/order/identity compositing");
    Check(Near(Pixel(wrong), {255, 0, 0}) && glm::length(Pixel(wrong) - Pixel(ordered)) > 100,
          "negative order control changes real pixels rather than merely reported draw counts");
    WriteRgbPng((out / "sprites.png").string(), Width, Height, ordered);

    const auto particles = Particles(r, texture, false);
    const auto particlesReversed = Particles(r, texture, true);
    const auto wrongDepth = Particles(r, texture, false, true);
    Check(Near(Pixel(particles), {127.5f, 0, 127.5f}),
          "generic particle presentation reproduces the same independent alpha-compositing answer");
    Check(MeanDifference(particles, particlesReversed) == 0,
          "particle input permutation retains actual camera-depth ordering");
    Check(Near(Pixel(wrongDepth), {255, 0, 0}) &&
              glm::length(Pixel(wrongDepth) - Pixel(particles)) > 100,
          "negative particle-depth control produces the independently different front-most result");
    Check(r.Stats().particlesSubmitted == 2, "particle control exercises two real presented particles");
    WriteRgbPng((out / "particles.png").string(), Width, Height, particles);

    std::vector<MaterialHandle> materials;
    for (unsigned i = 0; i < 8; ++i) {
        MaterialDefinition material;
        material.model = MaterialModel::PBR;
        material.baseColor = {.2f + float(i % 4) * .2f, .35f, .75f - float(i % 3) * .2f, 1};
        material.metallic = float(i % 3) * .3f;
        material.maps[2].embedded.width = material.maps[2].embedded.height = 2;
        material.maps[2].embedded.pixels = {136, 128, 254, 255, 120, 128, 254, 255,
                                            128, 136, 254, 255, 128, 120, 254, 255};
        materials.push_back(r.CreateMaterial(material));
    }
    const auto immediate = Materials(r, materials, false);
    const auto immediateDraws = r.Stats().drawCalls;
    const auto batched = Materials(r, materials, true);
    Check(MeanDifference(immediate, batched) < .02,
          "mixed normal-mapped PBR material overrides, mirrored and nonuniform transforms retain "
          "matched pixels");
    Check(r.Stats().instances == 64 && r.Stats().instancedDraws > 0 &&
              r.Stats().drawCalls < immediateDraws,
          "mixed-material control exercises actual bounded instanced submission with all 64 instances");
    WriteRgbPng((out / "mixed.png").string(), Width, Height, batched);
    r.SetMaterialBindings({});
    for (auto material : materials)
        r.DestroyMaterial(material);

    auto old = r.CreateMesh(Triangle());
    r.DestroyMesh(old);
    const auto replacement = r.CreateMesh(Triangle());
    Begin(r);
    r.BeginMeshBatch();
    r.DrawMesh(old, {0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, {}, {1, 0, 0}, 1);
    r.EndMeshBatch();
    r.EndFrame();
    Check(r.Stats().drawCalls == 0 && Near(Pixel(Capture(r)), {0, 0, 0}),
          "destroyed mesh generation cannot alias replacement resources through a queued draw");
    Begin(r);
    r.BeginMeshBatch();
    r.DrawMesh(replacement, {0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, {}, {1, 0, 0}, 1);
    r.EndMeshBatch();
    r.EndFrame();
    Check(r.Stats().drawCalls > 0 && Pixel(Capture(r)).x > 200,
          "replacement mesh positive control draws using its new live generation");
    r.DestroyMesh(replacement);

    const auto target = r.CreateRenderTarget(320, 180, error);
    Check(target.IsValid(), "independent secondary render target is created");
    Begin(r);
    SpriteSettings sprite;
    sprite.size = {2, 2};
    r.QueueSprite(sprite, {0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, texture, {1, 0, 0, 1}, 1);
    r.FlushSprites();
    Check(r.BeginRenderTarget(target), "secondary view begins after primary sprite submission");
    CameraLens lens;
    lens.projection = CameraProjectionKind::Orthographic;
    lens.height = 6;
    r.SetCameraFrame(BuildCameraFrame({0, 0, 10}, {1, 0, 0, 0}, 60, .1f, 100, lens, 320, 180));
    r.QueueSprite(sprite, {0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, texture, {0, 1, 0, 1}, 2);
    r.FlushSprites();
    const auto secondary = Capture(r, 320, 180);
    r.EndRenderTarget();
    r.EndFrame();
    Check(Near(Pixel(Capture(r)), {255, 0, 0}) && Near(Pixel(secondary, 160, 90, 320), {0, 255, 0}),
          "sprite queues and colour state remain isolated across actual primary and secondary targets");
    r.DestroyRenderTarget(target);
    r.DestroyTexture(texture);
    Check(glGetError() == GL_NO_ERROR,
          "no graphics errors after submission, generation reuse and target retirement");
    std::printf("Renderer V2 submission: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
