// Production submission controls: independent compositing answers and matched
// public batch/immediate paths. No renderer implementation is copied here.
#include "EngineHost.h"
#include "GraphicsApi.h"
#include "ScreenshotWriter.h"
#include "SkeletalAnimation.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <limits>
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
    {
        auto data=Triangle();data.indices={0,1,2};data.primitives={{0,3,-1}};
        auto skeleton=std::make_shared<SkeletalAsset>();auto& s=skeleton->skeleton;
        s.names={"first","second"};s.parents={-1,-1};s.order={0,1};s.rest.local.resize(2);s.skinNodes={0,1};s.inverseBind.assign(2,glm::mat4(1));
        data.skeletal=skeleton;data.skinVertices.resize(data.vertices.size());
        for(auto& v:data.skinVertices){v.joints=glm::uvec4(0);v.weights={1,0,0,0};v.joints1=glm::uvec4(0);v.weights1=glm::vec4(0);}
        const auto handle=r.CreateMesh(data);Check(handle.IsValid(),"original detailed-skin topology fixture uploads");
        std::vector<glm::mat4> palette={glm::mat4(1),glm::translate(glm::mat4(1),glm::vec3(4,0,0))};
        auto draw=[&](){Begin(r);r.DrawMesh(handle,{0,0,0},{1,0,0,0},{1,1,1},{},{1,0,0},1,&palette);r.EndFrame();return Capture(r);};
        Check(Pixel(draw()).x>200,"original physical section palette renders at the authored location");
        for(auto& v:data.skinVertices)v.joints=glm::uvec4(1);
        Check(r.UpdateMeshSkinTopology(handle,data.indices,data.skinVertices,data.primitives),"physical tearing updates skin weights without replacing the mesh handle or materials");
        Check(Near(Pixel(draw()),{0,0,0}),"actual GPU pixels follow updated section weights instead of stale pre-fracture bindings");
        auto invalid=data.skinVertices;invalid[0].joints.x=99;
        Check(!r.UpdateMeshSkinTopology(handle,data.indices,invalid,data.primitives)&&Near(Pixel(draw()),{0,0,0}),"invalid topology update rejects transactionally and preserves the previous GPU state");
        Check(r.UpdateMeshSkinTopology(handle,{}, {},{}),"complete physical removal can publish an empty indexed skin without resurrecting nonindexed triangles");
        Check(Near(Pixel(draw()),{0,0,0}),"fully removed physical exterior produces no stale GPU geometry");
        Check(r.UpdateMeshSkinTopology(handle,data.indices,data.skinVertices,data.primitives),"cold-restored visible topology can reuse the same resident exterior");
        r.DestroyMesh(handle);
        Check(!r.UpdateMeshSkinTopology(handle,data.indices,data.skinVertices,data.primitives),"retired skin handles cannot modify later mesh generations");
    }

    {
        auto data=Triangle();data.indices={0,1,2};data.primitives={{0,3,-1}};
        auto skeleton=std::make_shared<SkeletalAsset>();auto& sk=skeleton->skeleton;
        sk.names={"owner","neighbour"};sk.parents={-1,-1};sk.order={0,1};sk.rest.local.resize(2);sk.skinNodes={0,1};sk.inverseBind.assign(2,glm::mat4(1));
        data.skeletal=skeleton;data.skinVertices.resize(data.vertices.size());
        for(auto& v:data.skinVertices){v.joints={0,1,0,0};v.weights={.5f,.5f,0,0};v.joints1=glm::uvec4(0);v.weights1=glm::vec4(0);}
        const auto handle=r.CreateMesh(data);
        std::vector<glm::mat4> palette={glm::mat4(1),glm::translate(glm::mat4(1),glm::vec3(8,0,0))};
        auto draw=[&](){Begin(r);r.DrawMesh(handle,{0,0,0},{1,0,0,0},{1,1,1},{},{1,0,0},1,&palette);r.EndFrame();return Capture(r);};
        Check(r.UpdateMeshStructuralSections(handle,{0,0})&&Near(Pixel(draw()),{0,0,0}),"connected resident influences retain the physical blend");
        Check(r.UpdateMeshStructuralSections(handle,{0,1})&&Pixel(draw()).x>200,"separation updates actual pixels immediately without a vertex/index upload");
        Check(!r.UpdateMeshStructuralSections(handle,{0,2})&&Pixel(draw()).x>200,"invalid section IDs reject transactionally");
        Check(!r.UpdateMeshStructuralSections(handle,{0})&&Pixel(draw()).x>200,"wrong section count preserves published ownership");
        Check(r.UpdateMeshStructuralSections(handle,{-1,1})&&Near(Pixel(draw()),{0,0,0}),"removed physical owner clips its complete triangle immediately");
        Check(r.UpdateMeshStructuralSections(handle,{0,1})&&Pixel(draw()).x>200,"cold-restored resident ownership reveals the original geometry again");
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
        auto depth=[&](){r.BeginShadowPass(0,glm::ortho(-3.f,3.f,-3.f,3.f,.1f,100.f)*glm::lookAt(glm::vec3(0,0,10),glm::vec3(0),glm::vec3(0,1,0)));r.DrawMesh(handle,{0,0,0},{1,0,0,0},{1,1,1},{},{1,0,0},1,&palette);GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);float value=1;glReadPixels(viewport[2]/2,viewport[3]/2,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&value);r.EndShadowPass();return value;};
        Check(depth()<.9f,"resident separated owner casts the current shadow silhouette");
        Check(r.UpdateMeshStructuralSections(handle,{-1,1})&&depth()>.999f,"removed owner stops casting shadow in the same publication");
#endif
        r.DestroyMesh(handle);
        Check(!r.UpdateMeshStructuralSections(handle,{0,1}),"retired handles cannot publish section state into reused meshes");
    }

    {
        auto data=Triangle();data.indices={0,1,2};data.primitives={{0,3,-1}};
        auto skeleton=std::make_shared<SkeletalAsset>();auto& sk=skeleton->skeleton;
        for(unsigned i=0;i<256;++i){sk.names.push_back(std::to_string(i));sk.parents.push_back(-1);sk.order.push_back(i);sk.skinNodes.push_back(i);sk.rest.local.emplace_back();sk.inverseBind.emplace_back(1);}
        data.skeletal=skeleton;data.skinVertices.resize(3);
        for(auto& v:data.skinVertices){v.joints=glm::uvec4(255);v.weights={1,0,0,0};v.joints1={255,0,0,0};v.weights1=glm::vec4(0);}
        auto handle=r.CreateMesh(data);std::vector<int> state(256,0);std::vector<glm::mat4> pose(256,glm::mat4(1));
        auto draw=[&](){Begin(r);r.DrawMesh(handle,{0,0,0},{1,0,0,0},{1,1,1},{},{1,0,0},1,&pose);r.EndFrame();return Capture(r);};
        Check(r.UpdateMeshStructuralSections(handle,state)&&Pixel(draw()).x>200,"portable packed ownership reaches physical section255");
        state[255]=-1;Check(r.UpdateMeshStructuralSections(handle,state)&&Near(Pixel(draw()),{0,0,0}),"last packed section removal reaches the actual GPU vertex path");
        r.DestroyMesh(handle);
        data.skinVertices[0].joints1.x=0;handle=r.CreateMesh(data);
        Check(!r.UpdateMeshStructuralSections(handle,state),"mixed owners within one physical face cannot publish a partial clipping convention");
        r.DestroyMesh(handle);
    }

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
    Begin(r);
    const auto validView=r.ViewMatrix(),validProjection=r.ProjectionMatrix();
    r.SetCamera(glm::mat4(0),validProjection);
    auto invalidProjection=validProjection;invalidProjection[0][0]=std::numeric_limits<float>::quiet_NaN();
    r.SetCamera(validView,invalidProjection);
    r.QueueSprite(sprite,{0,0,0},{1,0,0,0},{1,1,1},texture,{0,0,1,1},3);
    r.FlushSprites();r.EndFrame();
    Check(r.ViewMatrix()==validView&&r.ProjectionMatrix()==validProjection&&Near(Pixel(Capture(r)),{0,0,255}),
          "rejected camera input preserves coherent public matrices, culling and actual GPU pixels");
    Check(r.BeginRenderTarget(target),"target scope remains available for shadow failure controls");
    const auto beforeInvalidShadow=r.RenderTargetDiagnostics();
    r.BeginShadowPass(kShadowMapCount,glm::mat4(1));r.EndShadowPass();
    auto invalidShadow=glm::mat4(1);invalidShadow[0][0]=std::numeric_limits<float>::quiet_NaN();
    r.BeginShadowPass(0,invalidShadow);r.EndShadowPass();
    const auto afterInvalidShadow=r.RenderTargetDiagnostics();
    Check(beforeInvalidShadow.viewport==afterInvalidShadow.viewport&&!afterInvalidShadow.defaultFramebuffer&&afterInvalidShadow.passActive,
          "invalid shadow scopes cannot reset an active target framebuffer or viewport");
    r.EndRenderTarget();
    r.DestroyRenderTarget(target);
    r.DestroyTexture(texture);
    Check(glGetError() == GL_NO_ERROR,
          "no graphics errors after submission, generation reuse and target retirement");
    std::printf("Renderer V2 submission: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
