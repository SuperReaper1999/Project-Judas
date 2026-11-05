#include "EngineHost.h"
#include "GraphicsApi.h"
#include "render/ParticlePass.h"
#include "render/ComputeDevice.h"
#include "render/ShaderLibrary.h"
#include "render/StreamArena.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <limits>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char *message) {
    ++checks;
    failures += !value;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", message);
    std::fflush(stdout);
}
using JudasRender::ParticleDepth;
using JudasRender::ParticlePass;

void PureControls() {
    std::vector<ParticleBillboard> particles = {{{0, 0, .5f}, 1, {1, 0, 0, .5f}},
                                                {{0, 0, -.5f}, 1, {0, 0, 1, .5f}},
                                                {{1, 0, -.5f}, 2, {0, 1, 0, 1}}};
    std::vector<ParticleDepth> order;
    std::string error;
    Check(JudasRender::PrepareParticleOrder(glm::mat4(1), particles, order, error) &&
              order.size() == 3 && order[0].index == 1 && order[1].index == 2 && order[2].index == 0,
          "back-to-front order uses original indices as its exact equal-depth tie breaker");
    glm::mat4 view(1);
    view[0][2] = .1234567f;
    view[1][2] = -1.234567f;
    view[2][2] = .33333334f;
    view[3][2] = 100000000;
    particles.clear();
    for (unsigned index = 0; index < 64; ++index)
        particles.push_back(
            {{float(index) * 10000003.f, float(index % 7) * 30000011.f, float(index % 11) * -900001.f},
             .5f,
             {1, 1, 1, .5f}});
    Check(JudasRender::PrepareParticleOrder(view, particles, order, error),
          "finite cancellation-heavy fixture produces a complete depth permutation");
    std::vector<std::size_t> original(particles.size());
    std::iota(original.begin(), original.end(), 0);
    // Frozen legacy comparison: the complete matrix expression was formerly
    // recomputed on each comparison rather than once per input particle.
    std::sort(original.begin(), original.end(), [&](std::size_t a, std::size_t b) {
        const float za = (view * glm::vec4(particles[a].position, 1)).z;
        const float zb = (view * glm::vec4(particles[b].position, 1)).z;
        return za == zb ? a < b : za < zb;
    });
    bool identical = order.size() == original.size();
    for (std::size_t index = 0; identical && index < order.size(); ++index) {
        const float depth = (view * glm::vec4(particles[original[index]].position, 1)).z;
        identical &= order[index].index == original[index] &&
                     std::memcmp(&order[index].depth, &depth, sizeof(depth)) == 0;
    }
    Check(identical, "precomputed depths and total ordering are bit-identical to the original "
                     "expression/comparator");
    particles[4].position.x = std::numeric_limits<float>::quiet_NaN();
    Check(!JudasRender::PrepareParticleOrder(view, particles, order, error) && order.empty() &&
              !error.empty(),
          "nonfinite particle input rejects rather than leaving a partial ordering available for "
          "drawing");
    particles.clear();
    Check(JudasRender::PrepareParticleOrder(glm::mat4(1), particles, order, error) && order.empty(),
          "empty presentation produces an empty successful order");
    ParticlePass pass;
    Check(pass.Draw(glm::mat4(1), glm::mat4(1), {}, 0, false, false, false,
                    JudasRender::Shaders::PortableProfile(), error) &&
              pass.Stats().arenaBytes == 0 && pass.Stats().draws == 0,
          "empty optional particle presentation creates no GPU storage or commands");
    const std::vector<ParticleBillboard> one = {{{0, 0, 0}, 1, {1, 1, 1, 1}}};
    Check(!pass.Draw(glm::mat4(1), glm::mat4(1), one, 0, false, false, false,
                     JudasRender::Shaders::PortableProfile(), error) &&
              !error.empty() && pass.Stats().arenaBytes == 0,
          "nonempty draw without an owning context fails before GPU allocation");
}

// Deliberately retained expanded-vertex path from the former renderer, with
// independent analytic alpha controls below. It is not another use of the new
// instance shader or depth-preparation helper.
constexpr char LegacyVertex[] = R"GLSL(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec2 uv;layout(location=2)in vec4 color;
uniform mat4 vp;out vec2 texcoord;out vec4 tint;
void main(){gl_Position=vp*vec4(position,1);texcoord=uv;tint=color;}
)GLSL";
constexpr char LegacyFragment[] = R"GLSL(#version 330 core
in vec2 texcoord;in vec4 tint;uniform sampler2D image;uniform bool modern,imageLinear;out vec4 result;
vec3 linear(vec3 c){return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
void main(){vec4 sampleColour=texture(image,texcoord);result=sampleColour*tint;if(modern)result.rgb=(imageLinear?sampleColour.rgb:linear(sampleColour.rgb))*linear(tint.rgb);}
)GLSL";
struct ExpandedVertex {
    glm::vec3 position;
    glm::vec2 uv;
    glm::vec4 colour;
};
struct Fixture {
    unsigned fbo = 0, colour = 0, depth = 0, texture = 0, program = 0, vao = 0, vbo = 0;
    bool Init() {
        namespace shaders = JudasRender::Shaders;
        unsigned vertex = 0, fragment = 0;
        if (!shaders::CompileShader(GL_VERTEX_SHADER, LegacyVertex, vertex, shaders::PortableProfile(),
                                    "Legacy expanded particles") ||
            !shaders::CompileShader(GL_FRAGMENT_SHADER, LegacyFragment, fragment,
                                    shaders::PortableProfile(), "Legacy particle colour")) {
            if (vertex)
                glDeleteShader(vertex);
            if (fragment)
                glDeleteShader(fragment);
            return false;
        }
        const bool linked =
            shaders::LinkProgram(vertex, fragment, program, "Legacy particle reference");
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        if (!linked)
            return false;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &colour);
        glBindTexture(GL_TEXTURE_2D, colour);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour, 0);
        glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            return false;
        glGenTextures(1, &texture);
        Image({255, 255, 255, 255});
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        return true;
    }
    void Image(std::array<unsigned char, 4> pixel) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    void Clear(float clearDepth = 1) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, 64, 64);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
        glClearDepthf(clearDepth);
#else
        glClearDepth(clearDepth);
#endif
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    std::array<unsigned char, 4> Pixel() const {
        std::array<unsigned char, 4> pixel{};
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        return pixel;
    }
    std::vector<unsigned char> Pixels() const {
        std::vector<unsigned char> pixels(64 * 64 * 4);
        glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    }
    void Legacy(const glm::mat4 &view, const std::vector<ParticleBillboard> &particles,
                bool imageLinear = false, bool sceneLinear = false) {
        std::vector<std::size_t> order(particles.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            const float za = (view * glm::vec4(particles[a].position, 1)).z;
            const float zb = (view * glm::vec4(particles[b].position, 1)).z;
            return za == zb ? a < b : za < zb;
        });
        const auto inverse = glm::inverse(view);
        const auto right = glm::vec3(inverse[0]), up = glm::vec3(inverse[1]);
        const glm::vec2 corners[4] = {{-.5f, -.5f}, {.5f, -.5f}, {.5f, .5f}, {-.5f, .5f}};
        const unsigned indices[6] = {0, 1, 2, 0, 2, 3};
        std::vector<ExpandedVertex> vertices;
        for (auto index : order) {
            const auto &particle = particles[index];
            for (auto corner : indices) {
                const auto c = corners[corner];
                vertices.push_back({particle.position + particle.size * (right * c.x + up * c.y),
                                    c + glm::vec2(.5f), particle.color});
            }
        }
        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "vp"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform1i(glGetUniformLocation(program, "modern"), sceneLinear);
        glUniform1i(glGetUniformLocation(program, "imageLinear"), imageLinear);
        glUniform1i(glGetUniformLocation(program, "image"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindSampler(0, 0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size() * sizeof(ExpandedVertex)),
                     vertices.data(), GL_STREAM_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ExpandedVertex), nullptr);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ExpandedVertex),
                              reinterpret_cast<void *>(offsetof(ExpandedVertex, uv)));
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ExpandedVertex),
                              reinterpret_cast<void *>(offsetof(ExpandedVertex, colour)));
        for (unsigned index = 0; index < 3; ++index)
            glEnableVertexAttribArray(index);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    ~Fixture() {
        if (vbo)
            glDeleteBuffers(1, &vbo);
        if (vao)
            glDeleteVertexArrays(1, &vao);
        if (program)
            glDeleteProgram(program);
        if (texture)
            glDeleteTextures(1, &texture);
        if (colour)
            glDeleteTextures(1, &colour);
        if (depth)
            glDeleteRenderbuffers(1, &depth);
        if (fbo)
            glDeleteFramebuffers(1, &fbo);
    }
};

bool Near(std::array<unsigned char, 4> actual, std::array<unsigned char, 4> expected) {
    for (unsigned index = 0; index < 4; ++index)
        if (std::abs(int(actual[index]) - int(expected[index])) > 1)
            return false;
    return true;
}

void NativeControls() {
    EngineHost host;
    std::string error;
    if (!host.Init("Particle pass qualification", 320, 240, false, error)) {
        Check(false, "particle test owns a real platform graphics context");
        std::fprintf(stderr, "%s\n", error.c_str());
        return;
    }
    {
        Fixture fixture;
        if (!fixture.Init()) {
            Check(false, "independent expanded reference and colour/depth target initialize");
            return;
        }
        ParticlePass pass;
        const auto profile = JudasRender::Shaders::PortableProfile();
        auto draw = [&](const glm::mat4 &view, const std::vector<ParticleBillboard> &particles,
                        bool imageLinear = false, bool sceneLinear = false, bool persistent = false,
                        JudasRender::Shaders::Profile shaderProfile =
                            JudasRender::Shaders::PortableProfile()) {
            return pass.Draw(view, glm::mat4(1), particles, fixture.texture, imageLinear, sceneLinear,
                             persistent, shaderProfile, error);
        };
        std::vector<ParticleBillboard> particles = {{{0, 0, .25f}, 1.5f, {1, 0, 0, .5f}},
                                                    {{0, 0, -.25f}, 1.5f, {0, 0, 1, .5f}}};
        fixture.Clear();
        fixture.Legacy(glm::mat4(1), particles);
        const auto reference = fixture.Pixels();
        Check(Near(fixture.Pixel(), {128, 0, 64, 96}),
              "expanded reference independently gives the analytic back-to-front RGB and legacy alpha "
              "factors");
        fixture.Clear();
        Check(draw(glm::mat4(1), particles) && fixture.Pixels() == reference,
              "compact instanced particles are pixel-identical to the frozen expanded reference");
        Check(pass.Stats().draws == 1 && pass.Stats().particles == 2 && pass.Stats().triangles == 4 &&
                  pass.Stats().uploadCalls == 1 && pass.Stats().uploadedBytes == 64,
              "actual submission uploads one 32-byte record per particle rather than six expanded "
              "vertices");
        std::reverse(particles.begin(), particles.end());
        fixture.Clear();
        Check(draw(glm::mat4(1), particles) && fixture.Pixels() == reference,
              "reversing input cannot reverse unequal-depth transparency ordering");
        particles[0].position.z = particles[1].position.z = 0;
        fixture.Clear();
        Check(draw(glm::mat4(1), particles) && Near(fixture.Pixel(), {128, 0, 64, 96}),
              "equal-depth instances retain current input-index ordering");
        std::reverse(particles.begin(), particles.end());
        fixture.Clear();
        Check(draw(glm::mat4(1), particles) && Near(fixture.Pixel(), {64, 0, 128, 96}),
              "reversing a tie changes compositing exactly as the original index tie breaker requires");

        glm::mat4 side(1);
        side[0] = {0, 0, -1, 0};
        side[2] = {1, 0, 0, 0};
        const std::vector<ParticleBillboard> one = {{{0, 0, 0}, 1.5f, {.8f, .6f, .4f, 1}}};
        fixture.Image({128, 64, 32, 255});
        for (bool sceneLinear : {false, true})
            for (bool imageLinear : {false, true}) {
                fixture.Clear();
                fixture.Legacy(side, one, imageLinear, sceneLinear);
                const auto image = fixture.Pixels();
                fixture.Clear();
                Check(draw(side, one, imageLinear, sceneLinear) && fixture.Pixels() == image &&
                          fixture.Pixel()[3] == 255,
                      "side-facing camera basis and every colour-space flag preserve the expanded "
                      "reference image");
            }
        fixture.Image({255, 255, 255, 255});
        fixture.Clear();
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ZERO, GL_ONE);
        const bool stateDraw = draw(glm::mat4(1), one);
        GLboolean depthMask = GL_FALSE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        GLint sourceRgb = 0, destinationRgb = 0, sourceAlpha = 0, destinationAlpha = 0;
        glGetIntegerv(GL_BLEND_SRC_RGB, &sourceRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &destinationRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &sourceAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &destinationAlpha);
        Check(stateDraw && glIsEnabled(GL_CULL_FACE) && !glIsEnabled(GL_BLEND) && depthMask &&
                  sourceRgb == GL_ONE && destinationRgb == GL_ZERO && sourceAlpha == GL_ZERO &&
                  destinationAlpha == GL_ONE && !glIsEnabled(GL_DEPTH_TEST),
              "particle pass restores culling, blending, separate factors and depth mask without "
              "taking depth-test ownership");
        fixture.Clear();
        glEnable(GL_DEPTH_TEST);
        const std::vector<ParticleBillboard> near = {{{0, 0, -.4f}, 1.5f, {1, 0, 0, .5f}}};
        const std::vector<ParticleBillboard> far = {{{0, 0, .4f}, 1.5f, {0, 0, 1, .5f}}};
        Check(draw(glm::mat4(1), near) && draw(glm::mat4(1), far) &&
                  Near(fixture.Pixel(), {64, 0, 128, 96}),
              "separate emitter draws preserve their order and never write transparent depth");
        fixture.Clear(.25f);
        glEnable(GL_DEPTH_TEST);
        Check(draw(glm::mat4(1), one) && fixture.Pixel() == std::array<unsigned char, 4>{},
              "existing opaque depth still occludes particles");
        const auto beforeRejected = pass.Stats();
        auto invalid = one;
        invalid[0].position.x = std::numeric_limits<float>::quiet_NaN();
        Check(!draw(glm::mat4(1), invalid) && !error.empty() &&
                  pass.Stats().draws == beforeRejected.draws &&
                  pass.Stats().uploadedBytes == beforeRejected.uploadedBytes,
              "invalid particle data fails before issuing draws or uploads");
        Check(!draw(glm::mat4(0), one) && !error.empty() &&
                  !draw(glm::mat4(1), one, false, false, false, JudasRender::Shaders::Profile(999)),
              "singular views and unknown shader profiles fail explicitly");
        bool rejectedThread = false;
        std::thread thread([&] {
            std::string reason;
            rejectedThread = !pass.Draw(glm::mat4(1), glm::mat4(1), one, fixture.texture, false, false,
                                        false, profile, reason) &&
                             !reason.empty();
        });
        thread.join();
        Check(rejectedThread && pass.Stats().draws == beforeRejected.draws,
              "a foreign thread without the owning context cannot touch the particle GPU resources");

        const std::size_t fullPage = JudasRender::StreamArena::PageBytes / ParticlePass::InstanceBytes;
        std::vector<ParticleBillboard> many(fullPage + 7, {{10, 0, -.5f}, .01f, {1, 0, 0, 1}});
        for (std::size_t index = fullPage; index < many.size(); ++index)
            many[index] = {{0, 0, 0}, 1.5f, {0, 1, 0, 1}};
        const bool persistent = host.GraphicsCaps().modernTier && host.GraphicsCaps().bufferStorage;
        const auto largeProfile =
            host.GraphicsCaps().modernTier ? JudasRender::Shaders::Profile::Desktop450 : profile;
        auto beforeLarge = pass.Stats();
        for (unsigned repeat = 0; repeat < 4; ++repeat) {
            fixture.Clear();
            Check(draw(glm::mat4(1), many, false, false, persistent, largeProfile) &&
                      fixture.Pixel() == std::array<unsigned char, 4>{0, 255, 0, 255},
                  "page-chunked and wrapped particle draws retain the final visible records beyond a "
                  "full page");
        }
        const auto afterLarge = pass.Stats();
        Check(afterLarge.draws - beforeLarge.draws == 8 &&
                  afterLarge.uploadCalls - beforeLarge.uploadCalls == 8 &&
                  afterLarge.particles - beforeLarge.particles == many.size() * 4 &&
                  afterLarge.uploadedBytes - beforeLarge.uploadedBytes == many.size() * 32 * 4 &&
                  afterLarge.arenaBytes <= JudasRender::StreamArena::PageBytes * 4,
              "all large input particles are submitted with bounded arena storage and exact "
              "compact-upload accounting");
        Check(pass.Shutdown(&error) && pass.Stats().arenaBytes == 0 && glIsTexture(fixture.texture),
              "explicit particle shutdown releases owned storage and preserves the borrowed texture");
        if(host.GraphicsCaps().modernTier){
            JudasRender::ComputeDevice compute;
            const bool initialized=compute.Initialize(host.GraphicsCaps(),{1024,1,1},error);
            const char* source="#version 450 core\nlayout(local_size_x=1)in;void main(){}";
            const auto kernel=initialized?compute.CreateKernel(source,"particle-shutdown/compute-continuation",error):JudasRender::ComputeKernel{};
            Check(initialized&&kernel.IsValid()&&compute.Dispatch(kernel,{1,1,1},error)&&glGetError()==GL_NO_ERROR,
                  "drawing then retiring a particle program leaves no deleted current name for the next compute dispatch");
            Check(compute.Shutdown(error),"cross-pass particle/compute lifetime control releases its kernel");
        }
        Check(glGetError() == GL_NO_ERROR, "particle known-answer operations leave no graphics errors");
    }
    host.Shutdown();
    ParticlePass stale;
    const std::vector<ParticleBillboard> one = {{{0, 0, 0}, 1, {1, 1, 1, 1}}};
    Check(!stale.Draw(glm::mat4(1), glm::mat4(1), one, 0, false, false, false,
                      JudasRender::Shaders::PortableProfile(), error) &&
              !error.empty() && stale.Stats().arenaBytes == 0 && stale.Shutdown(&error),
          "nonempty presentation after context teardown cannot issue commands or leak new resources");
}
} // namespace

int main(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && std::string(argv[1]) != "--native")) {
        std::fprintf(stderr, "usage: ParticlePassTests [--native]\n");
        return 2;
    }
    PureControls();
    if (argc == 2)
        NativeControls();
    std::printf("Particle pass: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
