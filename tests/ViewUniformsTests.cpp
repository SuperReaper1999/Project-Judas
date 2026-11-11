#include "EngineHost.h"
#include "GraphicsApi.h"
#include "render/ShaderLibrary.h"
#include "render/ViewUniforms.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <limits>
#include <string>
#include <thread>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char *message) {
    ++checks;
    failures += !value;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", message);
    std::fflush(stdout);
}
using JudasRender::ViewUniformData;
using JudasRender::ViewUniforms;

void PureControls() {
    ViewUniforms view;
    const auto &data = view.Data();
    Check(sizeof(data) == 3600 && offsetof(ViewUniformData, projection) == 64 &&
              offsetof(ViewUniformData, shadows) == 128 && offsetof(ViewUniformData, camera) == 3584,
          "CPU view block has the explicit 3600-byte std140 offsets independent of GLM alignment");
    Check(data.view[0] == 1 && data.view[5] == 1 && data.projection[15] == 1 &&
              data.shadows[53][10] == 1 && data.camera == std::array<float, 4>{0, 0, 0, 1},
          "CPU construction initializes both camera matrices, every shadow and camera homogeneous "
          "component");
    glm::mat4 camera(1), projection(1);
    camera[3] = {-2, -3, -4, 1};
    projection[0][0] = .5f;
    projection[1][1] = .25f;
    projection[2][2] = .125f;
    Check(view.SetCamera(camera, projection) &&
              view.Data().camera == std::array<float, 4>{2, 3, 4, 1} && view.Data().view[12] == -2 &&
              view.Data().view[13] == -3 && view.Data().projection[0] == .5f &&
              view.Data().projection[10] == .125f,
          "camera update preserves column-major matrices and independently known eye coordinates");
    glm::mat4 shadow(1);
    for (unsigned column = 0; column < 4; ++column)
        for (unsigned row = 0; row < 4; ++row)
            shadow[column][row] = float(column * 10 + row) + .25f;
    Check(view.SetShadow(53, shadow) && view.Data().shadows[53][13] == 31.25f &&
              view.Data().shadows[53][7] == 13.25f,
          "last shadow slot retains nonsymmetric column-major coefficients without transposition");
    const auto before = view.Data();
    Check(!view.SetShadow(54, glm::mat4(1)) &&
              !view.SetShadow(std::numeric_limits<unsigned>::max(), shadow) &&
              std::memcmp(&before, &view.Data(), sizeof(before)) == 0,
          "invalid shadow slots reject without changing any published CPU value");
    shadow[2][1] = std::numeric_limits<float>::quiet_NaN();
    projection[1][0] = std::numeric_limits<float>::infinity();
    Check(!view.SetShadow(0, shadow) && !view.SetCamera(camera, projection) &&
              !view.SetCamera(glm::mat4(0), glm::mat4(1)) &&
              std::memcmp(&before, &view.Data(), sizeof(before)) == 0,
          "nonfinite matrices and a singular camera preserve the previous coherent block");
    Check(view.Stats().uploads == 0 && view.Stats().bytesUploaded == 0,
          "CPU setters allocate and upload no graphics resources");

    namespace shaders = JudasRender::Shaders;
    const std::string vertex = shaders::Source(shaders::Builtin::MeshVertex);
    const std::string fragment = shaders::Source(shaders::Builtin::MeshFragment);
    std::string error;
    const auto preparedVertex = JudasRender::MakeViewBlockSource(vertex, error);
    Check(!preparedVertex.empty() && error.empty() &&
              preparedVertex.find("layout(std140) uniform JudasView") != std::string::npos &&
              preparedVertex.find("uniform mat4 uView;") == std::string::npos &&
              preparedVertex.find("gl_Position = uProjection * uView") != std::string::npos,
          "actual canonical mesh vertex declarations become one std140 block without rewriting its "
          "arithmetic");
    const auto preparedFragment = JudasRender::MakeViewBlockSource(fragment, error);
    Check(!preparedFragment.empty() && error.empty() &&
              preparedFragment.find("mat4 uLightSpaceMatrix[54];") != std::string::npos &&
              preparedFragment.find("#define uCameraPosition judasViewCamera.xyz") !=
                  std::string::npos &&
              preparedFragment.find("uniform vec3 uEmission;\n") != std::string::npos,
          "actual fragment keeps all 54 shadow charts and maps the vec3 camera onto its explicit vec4 storage");
    Check(vertex == shaders::Source(shaders::Builtin::MeshVertex) &&
              fragment == shaders::Source(shaders::Builtin::MeshFragment),
          "block preparation never mutates canonical shader storage");
    std::string es;
    Check(
        shaders::PrepareSource(preparedVertex, shaders::Profile::ES300, es, error) &&
            es.find("#version 300 es") != std::string::npos &&
            es.find("layout(std140) uniform JudasView") != std::string::npos &&
            es.find("sampler2D uBones") != std::string::npos,
        "the existing GLES3/WebGL2 profile adaptation preserves the standard block and bone fallback");
    Check(JudasRender::MakeViewBlockSource("#version 330 core\nvoid main(){}", error).empty() &&
              !error.empty(),
          "unrelated shader declarations are rejected instead of broadly rewritten");
    auto wrongCount = fragment;
    wrongCount.replace(wrongCount.find("uLightSpaceMatrix[54]"), std::strlen("uLightSpaceMatrix[54]"),
                       "uLightSpaceMatrix[8]");
    Check(
        JudasRender::MakeViewBlockSource(wrongCount, error).empty() && !error.empty(),
        "a changed canonical shadow count fails explicitly rather than silently using a wrong layout");
    auto duplicate = vertex + "\nuniform mat4 uView;\nuniform mat4 uProjection;\n";
    Check(JudasRender::MakeViewBlockSource(duplicate, error).empty() &&
              JudasRender::MakeViewBlockSource(preparedVertex, error).empty(),
          "duplicate declarations and an already prepared block cannot be transformed twice");
}

constexpr char KnownVertex[] = R"GLSL(#version 330 core
uniform mat4 uView;
uniform mat4 uProjection;
void main() {
    vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2.0-1.0;
    gl_Position=uProjection*uView*vec4(p,0,1);
}
)GLSL";
constexpr char KnownFragment[] = R"GLSL(#version 330 core
uniform mat4 uLightSpaceMatrix[54];
uniform vec3 uEmission,uCameraPosition;
out vec4 result;
void main() {
    result=vec4(uCameraPosition*.1+vec3(uLightSpaceMatrix[53][3].y*.01)+uEmission,1);
}
)GLSL";

unsigned Program(const std::string &vertex, const std::string &fragment) {
    namespace shaders = JudasRender::Shaders;
    unsigned v = 0, f = 0, program = 0;
    if (!shaders::CompileShader(GL_VERTEX_SHADER, vertex.c_str(), v, shaders::PortableProfile(),
                                "View block known-answer vertex"))
        return 0;
    if (!shaders::CompileShader(GL_FRAGMENT_SHADER, fragment.c_str(), f, shaders::PortableProfile(),
                                "View block known-answer fragment")) {
        glDeleteShader(v);
        return 0;
    }
    const bool success = shaders::LinkProgram(v, f, program, "View block known-answer program");
    glDeleteShader(v);
    glDeleteShader(f);
    return success ? program : 0;
}
std::array<unsigned char, 4> Pixel(unsigned program) {
    glUseProgram(program);
    glUniform3f(glGetUniformLocation(program, "uEmission"), 0, 0, 0);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    std::array<unsigned char, 4> result{};
    glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
    return result;
}

void NativeControls() {
    EngineHost host;
    std::string error;
    const bool initialized = host.Init("View uniform qualification", 320, 240, false, error);
    Check(initialized, "view block test owns a real platform context and initialized renderer");
    if (!initialized) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return;
    }
    const auto vertex = JudasRender::MakeViewBlockSource(KnownVertex, error);
    const auto fragment = JudasRender::MakeViewBlockSource(KnownFragment, error);
    const auto classic = Program(KnownVertex, KnownFragment), block = Program(vertex, fragment);
    Check(classic && block,
          "real native compiler links traditional uniforms and the transformed std140 shaders");
    if (!classic || !block) {
        if (classic)
            glDeleteProgram(classic);
        if (block)
            glDeleteProgram(block);
        return;
    }
    unsigned target = 0, colour = 0, vao = 0;
    glGenFramebuffers(1, &target);
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glGenTextures(1, &colour);
    glBindTexture(GL_TEXTURE_2D, colour);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour, 0);
    Check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
          "independent known-answer colour target is complete");
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glViewport(0, 0, 8, 8);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glm::mat4 camera(1), projection(1), shadow(1);
    camera[3] = {-2, -3, -4, 1};
    projection[3] = {2, 3, 4, 1};
    shadow[3][1] = 5;
    glUseProgram(classic);
    glUniformMatrix4fv(glGetUniformLocation(classic, "uView"), 1, GL_FALSE, glm::value_ptr(camera));
    glUniformMatrix4fv(glGetUniformLocation(classic, "uProjection"), 1, GL_FALSE,
                       glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(classic, "uLightSpaceMatrix[53]"), 1, GL_FALSE,
                       glm::value_ptr(shadow));
    glUniform3f(glGetUniformLocation(classic, "uCameraPosition"), 2, 3, 4);
    const auto reference = Pixel(classic);
    std::printf("CLASSIC_CONTROL program=%u shadowLocation=%d rgba=%u,%u,%u,%u\n",classic,glGetUniformLocation(classic,"uLightSpaceMatrix[53]"),reference[0],reference[1],reference[2],reference[3]);
    Check(std::abs(int(reference[0]) - 64) <= 1 && std::abs(int(reference[1]) - 89) <= 1 &&
              std::abs(int(reference[2]) - 115) <= 1 && reference[3] == 255,
          "traditional shader independently produces the analytic camera/shadow colour");

    ViewUniforms view;
    Check(view.SetCamera(camera, projection) && view.SetShadow(53, shadow) && view.Bind(block, error),
          "view values configure and bind the actual standard uniform block");
    Check(view.Stats().uploads == 1 && view.Stats().bytesUploaded == 3600 &&
              view.Stats().lastUploadBytes == 3600,
          "first consumer uploads exactly one complete 3600-byte block");
    const auto actual = Pixel(block);
    std::printf("BLOCK_CONTROL program=%u rgba=%u,%u,%u,%u\n",block,actual[0],actual[1],actual[2],actual[3]);
    Check(actual == reference,
          "actual GPU std140 result is pixel-identical to the traditional uniforms");
    GLint buffer = 0;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, ViewUniforms::BindingPoint, &buffer);
    Check(buffer != 0, "indexed uniform binding names the real allocated context-owned buffer");
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    ViewUniformData uploaded;
    glBindBuffer(GL_UNIFORM_BUFFER, GLuint(buffer));
    glGetBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(uploaded), &uploaded);
    Check(std::memcmp(&uploaded, &view.Data(), sizeof(uploaded)) == 0,
          "independent GPU readback matches every CPU matrix and camera byte");
#endif
    const char *members[] = {"uView", "uProjection", "uLightSpaceMatrix[0]", "judasViewCamera"};
    unsigned indices[4]{};
    glGetUniformIndices(block, 4, members, indices);
    bool allActive = true;
    for (auto index : indices)
        allActive &= index != GL_INVALID_INDEX;
    Check(allActive, "native reflection exposes the intended block members");
    if (allActive) {
        GLint offsets[4]{};
        glGetActiveUniformsiv(block, 4, indices, GL_UNIFORM_OFFSET, offsets);
        Check(offsets[0] == 0 && offsets[1] == 64 && offsets[2] == 128 && offsets[3] == 3584,
              "native std140 reflection independently agrees with all declared CPU offsets");
    }
    glBindBufferBase(GL_UNIFORM_BUFFER, ViewUniforms::BindingPoint, 0);
    Check(view.SetCamera(camera, projection) && view.SetShadow(53, shadow) && view.Bind(block, error) &&
              view.Stats().uploads == 1 && view.Stats().lastUploadBytes == 0,
          "unchanged values rebind after foreign presentation state without uploading again");
    Check(Pixel(block) == reference, "recovering a clobbered indexed binding preserves actual output");

    bool rejectedThread = false;
    std::thread foreign([&] {
        std::string foreignError;
        rejectedThread = !view.Bind(block, foreignError) && !foreignError.empty();
    });
    foreign.join();
    Check(rejectedThread && view.Stats().uploads == 1,
          "a thread without the owning context cannot issue a view upload");
    shadow[3][1] = 4;
    Check(view.SetShadow(53, shadow) && view.Bind(block, error) && view.Stats().uploads == 2 &&
              view.Stats().bytesUploaded == 7200,
          "a changed shadow uploads one new block at its next consumer");
    const auto changed = Pixel(block);
    Check(changed != reference && std::abs(int(changed[0]) - 61) <= 1 &&
              std::abs(int(changed[1]) - 87) <= 1 && std::abs(int(changed[2]) - 112) <= 1,
          "changed block data produces the independent updated physical camera/shadow colour");
    const auto wrong = Program("#version 330 core\nlayout(std140) uniform JudasView{vec4 wrong;};\n"
                               "void main(){gl_Position=wrong;}\n",
                               "#version 330 core\nout vec4 result;void main(){result=vec4(1);}\n");
    Check(wrong && !view.Configure(wrong, error) && error.find("3600-byte") != std::string::npos &&
              !view.Configure(0, error),
          "wrong block layout and invalid programs are rejected instead of receiving mismatched data");
    const auto uploads = view.Stats().uploads;
    Check(view.Bind(block, error) && view.Stats().uploads == uploads && Pixel(block) == changed,
          "a rejected program configuration preserves the previous valid view publication");
    Check(view.Shutdown(&error) && !glIsBuffer(GLuint(buffer)),
          "explicit shutdown retires the actual GPU buffer while its owner context still exists");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &target);
    glDeleteTextures(1, &colour);
    glDeleteProgram(classic);
    glDeleteProgram(block);
    if (wrong)
        glDeleteProgram(wrong);
    Check(glGetError() == GL_NO_ERROR, "known-answer view-block operations leave no graphics errors");
    host.Shutdown();
    Check(view.Shutdown(&error),
          "shutdown after explicit retirement performs no GL work after context teardown");
}
} // namespace

int main(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && std::string(argv[1]) != "--native")) {
        std::fprintf(stderr, "usage: ViewUniformsTests [--native]\n");
        return 2;
    }
    PureControls();
    if (argc == 2)
        NativeControls();
    std::printf("View uniforms: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
