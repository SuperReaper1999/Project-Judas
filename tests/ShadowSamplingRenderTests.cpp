// Compile the production sampling function in a real GL context. Synthetic
// depth planes/occluders provide independent answers, without scene physics,
// browser automation or a second copy of the shader under test.
#include "EngineHost.h"
#include "render/ShaderLibrary.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int kSize = 64;
unsigned checks = 0, failures = 0;

void Check(bool condition, const std::string& description) {
    ++checks;
    failures += !condition;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", description.c_str());
}

GLuint Compile(GLenum kind, const std::string& source) {
    const auto shader = glCreateShader(kind);
    const auto* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char message[8192]{};
        glGetShaderInfoLog(shader, sizeof(message), nullptr, message);
        glDeleteShader(shader);
        throw std::runtime_error(message);
    }
    return shader;
}

std::string ProductionFunction() {
    const std::string text=JudasRender::Shaders::Source(JudasRender::Shaders::Builtin::MeshFragment);
    const auto begin=text.find("float ComputeShadowFactor(");
    const auto end=text.find("\n}",begin);
    if(begin==std::string::npos||end==std::string::npos)
        throw std::runtime_error("Production shadow function not found in ShaderLibrary");
    return text.substr(begin,end+2-begin);
}

glm::mat4 LightMatrix(float projective) {
    glm::mat4 result(1);
    result[0][3] = projective;
    return result;
}
} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    try {
        const auto function = ProductionFunction();
        EngineHost host;
        std::string error;
        if (!host.Init("Shadow sampling contracts", kSize, kSize, false, error))
            throw std::runtime_error(error);

        const auto vertex = Compile(GL_VERTEX_SHADER, R"GLSL(#version 330 core
out vec2 uv;
void main() {
    vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    uv = p * .5 + .5;
    gl_Position = vec4(p, 0, 1);
})GLSL");
        const auto fragment = Compile(GL_FRAGMENT_SHADER, std::string(R"GLSL(#version 330 core
uniform sampler2DArray uShadowMaps;
uniform mat4 uLightSpaceMatrix[54];
uniform vec3 plane;
uniform bool tiled, fixedPoint;
uniform vec3 point;
uniform int requestedSlot;
in vec2 uv;
out vec4 color;
vec3 shadowWorldDx, shadowWorldDy;
)GLSL") + function + R"GLSL(
void main() {
    vec2 xy = uv * 2.0 - 1.0;
    vec3 p = fixedPoint ? point : vec3(xy, dot(plane.xy, xy) + plane.z);
    shadowWorldDx = dFdx(p);
    shadowWorldDy = dFdy(p);
    int slot = tiled && uv.x >= .5 ? 1 : requestedSlot;
    color = vec4(vec3(ComputeShadowFactor(slot, p, 1.0)), 1);
})GLSL");
        const auto program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char message[8192]{};
            glGetProgramInfoLog(program, sizeof(message), nullptr, message);
            throw std::runtime_error(message);
        }

        GLuint vao = 0, depths = 0, output = 0, framebuffer = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenTextures(1, &output);
        glBindTexture(GL_TEXTURE_2D, output);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, kSize, kSize, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Float oracle framebuffer is incomplete");
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0, 0, kSize, kSize);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &depths);
        glBindTexture(GL_TEXTURE_2D_ARRAY, depths);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        const auto uniform = [&](const char* name) { return glGetUniformLocation(program, name); };
        glUniform1i(uniform("uShadowMaps"), 0);
        glUniform1i(uniform("requestedSlot"), 0);

        std::vector<float> pixels(kSize * kSize * 4), atlas(kSize * kSize * 2);
        const auto upload = [&]() {
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, kSize, kSize, 2,
                         0, GL_DEPTH_COMPONENT, GL_FLOAT, atlas.data());
        };
        const auto draw = [&]() {
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glReadPixels(0, 0, kSize, kSize, GL_RGBA, GL_FLOAT, pixels.data());
        };

        struct PlaneCase {
            const char* name;
            float a, b, c, projective, occlusion;
            bool tiled;
        };
        for (const auto& sample : std::vector<PlaneCase>{
                 {"flat receiver", 0, 0, 0, 0, 0, false},
                 {"oblique receiver", 3.2f, 1.7f, .02f, 0, 0, false},
                 {"opposite receiver slope", -2.8f, 1.3f, -.03f, 0, 0, false},
                 {"projective receiver", 2, -.8f, .1f, .45f, 0, false},
                 {"real occluder retained", 2, -.8f, .1f, .45f, .03f, false},
                 {"adjacent tiles select different lights", 2, -.8f, .1f, 0, 0, true}}) {
            for (int layer = 0; layer < 2; ++layer) {
                const float k = sample.tiled && layer ? .45f : sample.projective;
                const auto matrix = LightMatrix(k);
                glUniformMatrix4fv(uniform(layer ? "uLightSpaceMatrix[1]" : "uLightSpaceMatrix[0]"),
                                   1, GL_FALSE, glm::value_ptr(matrix));
                for (int y = 0; y < kSize; ++y) {
                    for (int x = 0; x < kSize; ++x) {
                        // Independent inverse-chart ray/plane intersection.
                        const double u = (x + .5) / kSize * 2 - 1;
                        const double v = (y + .5) / kSize * 2 - 1;
                        const double w = 1 / (1 - k * u);
                        const double z = (sample.a * u * w + sample.b * v * w + sample.c) / w * .5 + .5;
                        atlas[(layer * kSize + y) * kSize + x] = float(std::clamp(
                            z - sample.occlusion - (sample.tiled && layer ? .03 : 0), 0.0, 1.0));
                    }
                }
            }
            upload();
            glUniform3f(uniform("plane"), sample.a, sample.b, sample.c);
            glUniform1i(uniform("tiled"), sample.tiled);
            glUniform1i(uniform("fixedPoint"), false);
            draw();
            int bad = 0;
            for (int y = 30; y < 34; ++y)
                for (int x = 30; x < 34; ++x) {
                    const float expected = sample.occlusion > 0 || (sample.tiled && x >= 32) ? 0 : 1;
                    bad += std::abs(pixels[(y * kSize + x) * 4] - expected) > 1e-4f;
                }
            Check(bad == 0, sample.name);
        }

        // A vertical half-plane occluder cuts a three-texel-wide box filter.
        // The exact overlap length, not a reimplementation of the sampler,
        // gives visibility clamp((33.5 - u*64)/3, 0, 1). It varies continuously
        // even as the nearest depth-texel identity changes. The old integer
        // 3x3 kernel jumps by 1/3 and must fail this contract.
        for (int layer = 0; layer < 2; ++layer)
            for (int y = 0; y < kSize; ++y)
                for (int x = 0; x < kSize; ++x)
                    atlas[(layer * kSize + y) * kSize + x] = x < 32 ? .7f : .3f;
        upload();
        const auto identity = LightMatrix(0);
        glUniformMatrix4fv(uniform("uLightSpaceMatrix[0]"), 1, GL_FALSE, glm::value_ptr(identity));
        glUniform1i(uniform("fixedPoint"), true);
        glUniform1i(uniform("tiled"), false);
        float previous = 1;
        float maxError = 0, maxJump = 0;
        bool monotone = true;
        for (int sample = 0; sample <= 64; ++sample) {
            const float texel = 30.f + sample / 16.f;
            glUniform3f(uniform("point"), 2 * texel / kSize - 1, 0, 0);
            draw();
            const float visibility = pixels[0];
            const float expected = std::clamp((33.5f - texel) / 3, 0.f, 1.f);
            maxError = std::max(maxError, std::abs(visibility - expected));
            if (sample) maxJump = std::max(maxJump, std::abs(visibility - previous));
            monotone &= visibility <= previous + 1e-5f;
            previous = visibility;
        }
        std::printf("MEASURE edge_max_error=%.9g edge_max_adjacent_jump=%.9g\n", maxError, maxJump);
        Check(maxError < 1e-4f, "fractional shadow-edge coverage matches analytic filter overlap");
        Check(maxJump <= 1.f / 48 + 1e-4f, "subtexel motion has no whole-texel shadow jump");
        Check(monotone, "occluder transition remains monotone");

        // Rotate the independent half-plane by 90 degrees. An implementation
        // that interpolates only horizontal sample weights must fail here.
        for (int layer = 0; layer < 2; ++layer)
            for (int y = 0; y < kSize; ++y)
                for (int x = 0; x < kSize; ++x)
                    atlas[(layer * kSize + y) * kSize + x] = y < 32 ? .7f : .3f;
        upload();
        previous = 1;
        maxError = maxJump = 0;
        monotone = true;
        for (int sample = 0; sample <= 64; ++sample) {
            const float texel = 30.f + sample / 16.f;
            glUniform3f(uniform("point"), 0, 2 * texel / kSize - 1, 0);
            draw();
            const float visibility = pixels[0];
            const float expected = std::clamp((33.5f - texel) / 3, 0.f, 1.f);
            maxError = std::max(maxError, std::abs(visibility - expected));
            if (sample) maxJump = std::max(maxJump, std::abs(visibility - previous));
            monotone &= visibility <= previous + 1e-5f;
            previous = visibility;
        }
        std::printf("MEASURE horizontal_edge_max_error=%.9g horizontal_edge_max_adjacent_jump=%.9g\n",
                    maxError, maxJump);
        Check(maxError < 1e-4f, "horizontal shadow-edge coverage matches analytic filter overlap");
        Check(maxJump <= 1.f / 48 + 1e-4f, "vertical subtexel motion has no whole-texel shadow jump");
        Check(monotone, "horizontal occluder transition remains monotone");

        // For a lit quadrant, exact box-filter coverage is its rectangular
        // intersection area divided by nine. Sweep both axes independently,
        // including unequal phases, to reject incorrect corner/product weights.
        for (int layer = 0; layer < 2; ++layer)
            for (int y = 0; y < kSize; ++y)
                for (int x = 0; x < kSize; ++x)
                    atlas[(layer * kSize + y) * kSize + x] = x < 32 && y < 32 ? .7f : .3f;
        upload();
        float cornerError = 0;
        for (int y = 0; y <= 8; ++y) {
            for (int x = 0; x <= 8; ++x) {
                const float tx = 31.f + x * .25f;
                const float ty = 31.f + y * .25f;
                glUniform3f(uniform("point"), 2 * tx / kSize - 1, 2 * ty / kSize - 1, 0);
                draw();
                const float expected = std::clamp((33.5f - tx) / 3, 0.f, 1.f) *
                                       std::clamp((33.5f - ty) / 3, 0.f, 1.f);
                cornerError = std::max(cornerError, std::abs(pixels[0] - expected));
            }
        }
        std::printf("MEASURE corner_max_error=%.9g\n", cornerError);
        Check(cornerError < 1e-4f, "two-axis shadow-corner coverage matches analytic intersection area");

        // Nine was the old atlas limit. It is now a valid chart, so exercise
        // both it and the last V2 chart against real depth before testing the
        // first out-of-range index. Out-of-bounds uniform reads are undefined
        // and happened to look lit on one native driver in the old harness.
        atlas.assign(kSize * kSize * 54, .25f);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, kSize, kSize, 54,
                     0, GL_DEPTH_COMPONENT, GL_FLOAT, atlas.data());
        for (const int slot : {9, 53}) {
            const auto name = "uLightSpaceMatrix[" + std::to_string(slot) + "]";
            glUniformMatrix4fv(uniform(name.c_str()), 1, GL_FALSE, glm::value_ptr(identity));
            glUniform1i(uniform("requestedSlot"), slot);
            draw();
            Check(std::abs(pixels[0]) < 1e-5f, "valid V2 shadow chart " + std::to_string(slot) + " remains occluded");
        }
        for (const int slot : {-1, 54}) {
            glUniform1i(uniform("requestedSlot"), slot);
            draw();
            Check(std::abs(pixels[0] - 1) < 1e-5f, "unassigned/out-of-range shadow slot remains lit");
        }
        glUniform1i(uniform("requestedSlot"), 0);
        glUniform3f(uniform("point"), 3, 0, 0);
        draw();
        Check(std::abs(pixels[0] - 1) < 1e-5f, "outside-chart receiver remains lit");
        Check(glGetError() == GL_NO_ERROR, "no GL errors during production sampling controls");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &output);
        glDeleteTextures(1, &depths);
        glDeleteVertexArrays(1, &vao);
        glDeleteProgram(program);
        host.Shutdown();
        std::printf("SUMMARY %u checks %u failures\n", checks, failures);
        return failures ? 1 : 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Shadow sampling test failed: %s\n", error.what());
        return 1;
    }
}
