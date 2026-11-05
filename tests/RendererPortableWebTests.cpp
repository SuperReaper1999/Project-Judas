// Small real-WebGL2 qualification of the production owners; no SDL, Renderer,
// EngineHost, scene, physics or application bootstrap is linked into this probe.
#if defined(__EMSCRIPTEN__)
#include "GraphicsApi.h"
#include "render/ComputeDevice.h"
#include "render/GraphicsCapabilities.h"
#include "render/ParticlePass.h"
#include "render/ShaderLibrary.h"
#include "render/StreamArena.h"
#include "render/ViewUniforms.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <emscripten.h>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace {
struct Row { std::string name; bool passed; };
std::vector<Row> rows;
unsigned failures = 0;
std::string Quote(const std::string &text) {
    std::string result = "\"";
    for (unsigned char byte : text) {
        switch (byte) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (byte < 32) {
                char escaped[7];
                std::snprintf(escaped, sizeof(escaped), "\\u%04x", byte);
                result += escaped;
            } else result += char(byte);
        }
    }
    return result + "\"";
}
void Check(bool passed, const std::string &name) {
    rows.push_back({name, passed});
    failures += !passed;
    std::printf("%s %s\n", passed ? "PASS" : "FAIL", name.c_str());
}
bool Near(const std::array<unsigned char, 4> &actual, std::array<int, 4> expected) {
    for (unsigned channel = 0; channel < 4; ++channel)
        if (std::abs(int(actual[channel]) - expected[channel]) > 1) return false;
    return true;
}
unsigned Program(const std::string &vertex, const std::string &fragment, const char *label) {
    namespace shaders = JudasRender::Shaders;
    unsigned v = 0, f = 0, program = 0;
    if (!shaders::CompileShader(GL_VERTEX_SHADER, vertex.c_str(), v, shaders::Profile::ES300, label))
        return 0;
    if (!shaders::CompileShader(GL_FRAGMENT_SHADER, fragment.c_str(), f, shaders::Profile::ES300, label)) {
        glDeleteShader(v);
        return 0;
    }
    const bool linked = shaders::LinkProgram(v, f, program, label);
    glDeleteShader(v);
    glDeleteShader(f);
    return linked ? program : 0;
}
struct Target {
    unsigned framebuffer = 0, colour = 0, white = 0, volume = 0, array = 0, vao = 0, vertices = 0;
    bool Init() {
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glGenTextures(1, &colour);
        glBindTexture(GL_TEXTURE_2D, colour);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
        const std::array<unsigned char, 4> pixel{255, 255, 255, 255};
        glGenTextures(1, &white);
        glBindTexture(GL_TEXTURE_2D, white);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenTextures(1, &volume);
        glBindTexture(GL_TEXTURE_3D, volume);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenTextures(1, &array);
        glBindTexture(GL_TEXTURE_2D_ARRAY, array);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 9, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        const std::array<unsigned char, 36> shadow{
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 1, 1, 9, GL_RGBA, GL_UNSIGNED_BYTE, shadow.data());
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vertices);
        glBindBuffer(GL_ARRAY_BUFFER, vertices);
        const float positions[] = {-1,-1,0, 3,-1,0, -1,3,0};
        glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttrib3f(1, 0, 1, 0);
        glVertexAttrib4f(5, 1, 0, 0, 1);
        glVertexAttribI4ui(3, 0, 0, 0, 0);
        glVertexAttribI4ui(6, 0, 0, 0, 0);
        Clear();
        return glGetError() == GL_NO_ERROR;
    }
    void Clear() const {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, 8, 8);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    std::array<unsigned char, 4> Pixel() const {
        std::array<unsigned char, 4> result{};
        glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
        return result;
    }
    void Shutdown() {
        glBindVertexArray(0);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (vertices) glDeleteBuffers(1, &vertices);
        const unsigned textures[] = {colour, white, volume, array};
        glDeleteTextures(4, textures);
        if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
        framebuffer = colour = white = volume = array = vao = vertices = 0;
    }
};
constexpr char KnownVertex[] = R"GLSL(#version 330 core
uniform mat4 uView;
uniform mat4 uProjection;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2.0-1.0;gl_Position=uProjection*uView*vec4(p,0,1);}
)GLSL";
constexpr char KnownFragment[] = R"GLSL(#version 330 core
uniform mat4 uLightSpaceMatrix[9];
uniform vec3 uEmission,uCameraPosition;
out vec4 colour;
void main(){colour=vec4(vec3(uCameraPosition.x*.1,uLightSpaceMatrix[8][3].y*.1,uProjection[0][0]*.5)+uEmission,1);}
)GLSL";
constexpr char StreamVertex[] = R"GLSL(#version 330 core
layout(location=0) in vec4 inputColour;
out vec4 tint;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2.0-1.0;gl_Position=vec4(p,0,1);tint=inputColour;}
)GLSL";
constexpr char StreamFragment[] = R"GLSL(#version 330 core
in vec4 tint;out vec4 colour;
void main(){colour=tint;}
)GLSL";

void Qualify(Target &target, const GraphicsCapabilities &caps) {
    namespace shaders = JudasRender::Shaders;
    std::string error;
    JudasRender::ViewUniforms view;
    const auto vertex = JudasRender::MakeViewBlockSource(shaders::Source(shaders::Builtin::MeshVertex), error);
    const auto fragment = JudasRender::MakeViewBlockSource(shaders::Source(shaders::Builtin::MeshFragment), error);
    const auto mesh = Program(vertex, fragment, "Actual canonical mesh / WebGL2 view block");
    Check(mesh != 0, "actual canonical mesh stages compile and link through ES300 with their view block");
    if (mesh) {
        glUseProgram(mesh);
        Check(view.Bind(mesh, error), "actual canonical mesh reflects and binds the 720-byte std140 block");
        const auto identity = glm::mat4(1);
        glUniformMatrix4fv(glGetUniformLocation(mesh, "uModel"), 1, GL_FALSE, glm::value_ptr(identity));
        const auto normal = glm::mat3(1);
        glUniformMatrix3fv(glGetUniformLocation(mesh, "uNormalMatrix"), 1, GL_FALSE, glm::value_ptr(normal));
        for (const auto *name : {"uSkinned", "uInstanced", "uBasicWater", "uWaterEnabled",
                                 "uWeatherEnabled", "uHasDynamicLights", "uModern", "uEnvironmentEnabled",
                                 "uHasMR", "uHasNormal", "uHasOcclusion", "uHasEmissive", "uFlipV"})
            glUniform1i(glGetUniformLocation(mesh, name), 0);
        for (const auto *name : {"uTexture", "uBones", "uMR", "uNormal", "uOcclusion", "uEmissive",
                                 "uEnvDiffuse", "uEnvSpecular", "uBRDF", "uWaterPaths", "uLightGrid"})
            glUniform1i(glGetUniformLocation(mesh, name), 0);
        glUniform1i(glGetUniformLocation(mesh, "uMaterialModel"), 0);
        glUniform1i(glGetUniformLocation(mesh, "uAlphaMode"), 0);
        glUniform1i(glGetUniformLocation(mesh, "uBaseLinear"), 1);
        glUniform1i(glGetUniformLocation(mesh, "uEmissiveLinear"), 1);
        glUniform1f(glGetUniformLocation(mesh, "uAlphaCutoff"), .5f);
        glUniform1f(glGetUniformLocation(mesh, "uMetallic"), 0);
        glUniform1f(glGetUniformLocation(mesh, "uRoughness"), 1);
        glUniform1f(glGetUniformLocation(mesh, "uNormalStrength"), 1);
        glUniform1f(glGetUniformLocation(mesh, "uOcclusionStrength"), 1);
        glUniform1f(glGetUniformLocation(mesh, "uEmissionIntensity"), 0);
        glUniform1f(glGetUniformLocation(mesh, "uEnvironmentIntensity"), 0);
        glUniform1f(glGetUniformLocation(mesh, "uEnvLevels"), 1);
        glUniform3f(glGetUniformLocation(mesh, "uEmission"), 0, 0, 0);
        glUniform3f(glGetUniformLocation(mesh, "uLightColor"), 0, 0, 0);
        glUniform4f(glGetUniformLocation(mesh, "uColor"), 1, 1, 1, 1);
        glUniform4f(glGetUniformLocation(mesh, "uBaseFactor"), .25f, .5f, .75f, 1);
        glUniform4f(glGetUniformLocation(mesh, "uUVTransform"), 1, 1, 0, 0);
        glUniform3f(glGetUniformLocation(mesh, "uAmbientColor"), 1, 1, 1);
        glUniform3f(glGetUniformLocation(mesh, "uLightDirection"), 0, 1, 0);
        glUniform1i(glGetUniformLocation(mesh, "uWeatherNoise"), 1);
        glUniform1i(glGetUniformLocation(mesh, "uShadowMaps"), 2);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D, target.volume);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D_ARRAY, target.array);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, target.white);
        target.Clear();
        glBindVertexArray(target.vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        Check(Near(target.Pixel(), {64,128,191,255}) && glGetError() == GL_NO_ERROR,
              "actual canonical Legacy material draws the independently known ambient-only colour on WebGL2");

        // Keep the camera outside the geometry and shadows outside the light
        // chart. White texture/base/tint and zero direct/environment light make
        // these analytic controls independent of BRDF/shadow approximations.
        glm::mat4 outsideView(1), outsideProjection(1), outsideShadow(1);
        outsideView[3].z = -4;
        outsideProjection[3].z = 4;
        outsideShadow[3].w = -1;
        Check(view.SetCamera(outsideView, outsideProjection) && view.SetShadow(0, outsideShadow) &&
                  view.Bind(mesh, error),
              "material controls use a finite exterior camera and explicitly excluded shadow chart");
        glUniform4f(glGetUniformLocation(mesh, "uBaseFactor"), 1, 1, 1, 1);
        glUniform3f(glGetUniformLocation(mesh, "uAmbientColor"), .25f, .25f, .25f);
        glUniform1i(glGetUniformLocation(mesh, "uMaterialModel"), 2);
        target.Clear();
        glDrawArrays(GL_TRIANGLES, 0, 3);
        Check(Near(target.Pixel(), {255,255,255,255}) && glGetError() == GL_NO_ERROR,
              "actual canonical Unlit material ignores quarter-strength ambient and preserves white");
        glUniform1i(glGetUniformLocation(mesh, "uMaterialModel"), 1);
        target.Clear();
        glDrawArrays(GL_TRIANGLES, 0, 3);
        // Independent scalar oracle: nonmetal ambient is .25; the existing
        // luminance tone map gives .25/1.25=.2, then sRGB encodes to 123.55/255.
        const auto encoded = int(std::lround((1.055 * std::pow(.2, 1.0 / 2.4) - .055) * 255));
        Check(encoded == 124 && Near(target.Pixel(), {encoded,encoded,encoded,255}) &&
                  glGetError() == GL_NO_ERROR,
              "actual canonical PBR material matches quarter ambient, luminance tone mapping and sRGB");
    }

    const auto knownVertex = JudasRender::MakeViewBlockSource(KnownVertex, error);
    const auto knownFragment = JudasRender::MakeViewBlockSource(KnownFragment, error);
    const auto known = Program(knownVertex, knownFragment, "Independent view block RGB / WebGL2");
    Check(known != 0, "independent view-block known-answer program links on the browser driver");
    if (known) {
        glm::mat4 camera(1), projection(1), shadow(1);
        camera[3] = {-2,-3,-4,1};
        projection[0][0] = .5f;
        projection[3] = {1,3,4,1};
        shadow[3][1] = 3;
        Check(view.SetCamera(camera, projection) && view.SetShadow(8, shadow) && view.Bind(known, error),
              "camera, projection and final shadow slot publish through the production UBO owner");
        target.Clear();
        glUseProgram(known);
        glBindVertexArray(target.vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        Check(Near(target.Pixel(), {51,77,64,255}) && glGetError() == GL_NO_ERROR,
              "camera red, ninth-shadow green and projection blue agree with independent numeric answers");
        const auto uploaded = view.Stats().bytesUploaded;
        glBindBufferBase(GL_UNIFORM_BUFFER, JudasRender::ViewUniforms::BindingPoint, 0);
        const bool rebound = view.Bind(known, error);
        target.Clear();
        glDrawArrays(GL_TRIANGLES, 0, 3);
        Check(rebound && Near(target.Pixel(), {51,77,64,255}) && view.Stats().bytesUploaded == uploaded,
              "view binding recovers from another pass without a redundant unchanged upload");
        shadow[3][1] = 7;
        const bool changed = view.SetShadow(8, shadow) && view.Bind(known, error);
        target.Clear();
        glDrawArrays(GL_TRIANGLES, 0, 3);
        Check(changed && Near(target.Pixel(), {51,179,64,255}) &&
                  view.Stats().bytesUploaded == uploaded + 720,
              "changing the last shadow updates only the expected known-answer channel with one upload");
    }
    Check(view.Shutdown(&error), "view UBO is explicitly retired while its WebGL2 context still exists");
    if (mesh) glDeleteProgram(mesh);
    if (known) glDeleteProgram(known);

    JudasRender::ParticlePass particles;
    std::vector<ParticleBillboard> pair = {{{0,0,-.5f}, 1.5f, {1,0,0,1}},
                                           {{0,0,.5f}, 1.5f, {0,0,1,.5f}}};
    auto particleDraw = [&] {
        target.Clear();
        return particles.Draw(glm::mat4(1), glm::mat4(1), pair, target.white, false, false, false,
                              shaders::Profile::ES300, error);
    };
    Check(particleDraw() && Near(target.Pixel(), {128,0,128,191}),
          "production compact billboards render the independently expected red-plus-half-blue colour");
    std::reverse(pair.begin(), pair.end());
    Check(particleDraw() && Near(target.Pixel(), {128,0,128,191}),
          "reversing input preserves the depth-sorted particle image");
    pair[0].position.z = -.5f;
    pair[1].position.z = .5f;
    Check(particleDraw() && Near(target.Pixel(), {255,0,0,255}),
          "reversing physical depth changes the visible result rather than silently ignoring ordering");
    pair[0].position.z = pair[1].position.z = 0;
    Check(particleDraw() && Near(target.Pixel(), {255,0,0,255}),
          "equal-depth particle ordering retains original-index tie semantics");
    const auto particleStats = particles.Stats();
    Check(particleStats.draws == 4 && particleStats.particles == 8 &&
              particleStats.uploadedBytes == 8 * 32 && particleStats.arenaBytes == 2 * 32 &&
              particleStats.waitNs == 0 && glGetError() == GL_NO_ERROR,
          "portable particles submit all records in 32-byte instances without persistent buffers or waits");
    Check(particles.Shutdown(&error) && particles.Stats().arenaBytes == 0,
          "particle shutdown retires all owned shaders, vertex arrays and portable storage");

    JudasRender::StreamArena arena;
    arena.SetWorkingSetHint(JudasRender::StreamArena::PageBytes);
    Check(arena.AllocatedBytes() == 0, "stream working-set hint remains CPU-only on WebGL2");
    JudasRender::StreamSlice slice;
    const std::array<float, 8> payload{.25f,.5f,.75f,1, 7,7,7,7};
    const bool streamed = arena.Upload(payload.data(), sizeof(payload), true, slice, error);
    Check(streamed && slice.offset == 0 && arena.AllocatedBytes() == sizeof(payload),
          "a persistent request falls back to exact-size portable upload without desktop entry points");
    const auto streamProgram = Program(StreamVertex, StreamFragment, "Portable stream payload / WebGL2");
    Check(streamProgram != 0, "independent GPU stream-payload consumer links");
    unsigned streamVao = 0;
    if (streamed && streamProgram) {
        glGenVertexArrays(1, &streamVao);
        glBindVertexArray(streamVao);
        glBindBuffer(GL_ARRAY_BUFFER, slice.buffer);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(payload), nullptr);
        glVertexAttribDivisor(0, 1);
        glUseProgram(streamProgram);
        target.Clear();
        glDrawArraysInstanced(GL_TRIANGLES, 0, 3, 1);
        Check(Near(target.Pixel(), {64,128,191,255}) && glGetError() == GL_NO_ERROR,
              "the real portable stream buffer reaches the GPU with the exact known colour payload");
    }
    const auto allocated = arena.AllocatedBytes();
    Check(!arena.Upload(nullptr, 1, true, slice, error) && !error.empty() && !slice.buffer &&
              arena.AllocatedBytes() == allocated,
          "invalid stream uploads preserve allocation and cannot publish a stale slice");
    glBindVertexArray(0);
    if (streamVao) glDeleteVertexArrays(1, &streamVao);
    if (streamProgram) glDeleteProgram(streamProgram);
    arena.Shutdown();
    Check(arena.AllocatedBytes() == 0, "stream storage is retired before context teardown");

    JudasRender::ComputeDevice compute;
    auto forged = caps;
    forged.api = GraphicsApi::Desktop;
    forged.modernTier = forged.compute = forged.storageBuffers = true;
    Check(!compute.Initialize(forged, {}, error) && !error.empty() && !compute.Supported() &&
              compute.AllocatedBytes() == 0 && compute.LiveBuffers() == 0 && compute.LiveKernels() == 0,
          "forged desktop capability flags cannot enable compute on the compiled WebGL2 backend");
    const auto unsupported = compute.CreateBuffer(1024, JudasRender::ComputeBufferUse::Storage,
                                                   nullptr, error);
    Check(!unsupported.IsValid() && !error.empty() && compute.AllocatedBytes() == 0 &&
              compute.Shutdown(error),
          "unsupported compute allocation fails explicitly and remains allocation-free");
    Check(glGetError() == GL_NO_ERROR, "all production-owner browser controls leave no GL errors");
}

void Publish(const GraphicsCapabilities &caps) {
    std::ostringstream json;
    json << "{\"passed\":" << (failures ? "false" : "true") << ",\"checks\":" << rows.size()
         << ",\"failures\":" << failures << ",\"profile\":" << Quote(caps.profile)
         << ",\"version\":" << Quote(caps.versionString) << ",\"renderer\":" << Quote(caps.renderer)
         << ",\"contextDestroyed\":" << (emscripten_webgl_get_current_context() ? "false" : "true")
         << ",\"rows\":[";
    for (std::size_t index = 0; index < rows.size(); ++index) {
        if (index) json << ',';
        json << "{\"name\":" << Quote(rows[index].name) << ",\"passed\":"
             << (rows[index].passed ? "true" : "false") << '}';
    }
    json << "]}";
    const auto result = json.str();
    EM_ASM({
        const result = JSON.parse(UTF8ToString($0));
        result.heapBytes = HEAPU8.length;
        window.rendererPortableResult = result;
        document.body.dataset.result = result.passed ? 'pass' : 'fail';
        document.getElementById('result').textContent = JSON.stringify(result, null, 2);
        document.title = 'Renderer WebGL2 proof: ' + (result.passed ? 'PASS' : 'FAIL');
    }, result.c_str());
}
} // namespace

int main() {
    EmscriptenWebGLContextAttributes attributes;
    emscripten_webgl_init_context_attributes(&attributes);
    attributes.majorVersion = 2;
    attributes.minorVersion = 0;
    attributes.alpha = true;
    attributes.depth = attributes.stencil = attributes.antialias = false;
    attributes.premultipliedAlpha = false;
    attributes.preserveDrawingBuffer = true;
    attributes.powerPreference = EM_WEBGL_POWER_PREFERENCE_LOW_POWER;
    const auto context = emscripten_webgl_create_context("#canvas", &attributes);
    Check(context && emscripten_webgl_make_context_current(context) == EMSCRIPTEN_RESULT_SUCCESS,
          "a real WebGL2 canvas context starts without SDL or application bootstrap");
    const auto caps = QueryGraphicsCapabilities();
    std::string error;
    Check(ValidateGraphicsCapabilities(caps, error) && caps.api == GraphicsApi::WebGL &&
              caps.profile == "webgl2" && caps.uniformBuffers && !caps.modernTier && !caps.compute &&
              !caps.storageBuffers && !caps.bufferStorage,
          "real browser capability query exposes the portable tier and rejects desktop-only features");
    if (context && GraphicsContextCurrent()) {
        Target target;
        const bool ready = target.Init();
        Check(ready, "bounded independent colour target, canonical inputs and complete textures initialize");
        if (ready) Qualify(target, caps);
        target.Shutdown();
        Check(glGetError() == GL_NO_ERROR, "explicit target shutdown leaves no graphics errors");
    }
    if (context) {
        emscripten_webgl_make_context_current(0);
        Check(emscripten_webgl_destroy_context(context) == EMSCRIPTEN_RESULT_SUCCESS,
              "the single test context is explicitly destroyed without a background rendering loop");
    }
    Check(!GraphicsContextCurrent(), "no graphics context remains current after qualification");
    Publish(caps);
    return failures ? 1 : 0;
}
#else
#include <cstdio>
int main() {
    std::puts("RendererPortableWebTests requires the bounded Emscripten/WebGL2 reproduction command");
    return 77;
}
#endif
