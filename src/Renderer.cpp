#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {

const char* kVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec3 aLocalPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    gl_Position = uProjection * uView * uModel * vec4(aLocalPos, 1.0);
}
)";

const char* kFragmentShaderSource = R"(#version 330 core
out vec4 FragColor;
uniform vec4 uColor;

void main() {
    FragColor = uColor;
}
)";

bool CompileShader(GLenum type, const char* source, GLuint& outShader) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<size_t>(logLength > 0 ? logLength : 1));
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Shader compile error: %s\n", log.data());
        glDeleteShader(shader);
        return false;
    }

    outShader = shader;
    return true;
}

bool LinkProgram(GLuint vertexShader, GLuint fragmentShader, GLuint& outProgram) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<size_t>(logLength > 0 ? logLength : 1));
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Program link error: %s\n", log.data());
        glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

// A unit cube (36 vertices, position-only, two triangles per face) centered
// on the origin in local [-0.5, 0.5] space. Winding order is not consistent
// per face since face culling is not enabled — every face is drawn from
// either side, which is fine for solid-colored opaque cubes with depth
// testing and not worth the extra bookkeeping this milestone.
// clang-format off
const float kCubeVertices[] = {
    // back face
    -0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,
    // front face
    -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,
    // left face
    -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
    // right face
     0.5f,  0.5f,  0.5f,   0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,   0.5f,  0.5f,  0.5f,   0.5f, -0.5f,  0.5f,
    // bottom face
    -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f, -0.5f, -0.5f,
    // top face
    -0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,
     0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f,  0.5f,  0.5f,
};
// clang-format on

// A unit sphere (radius 1, position-only, non-indexed triangles), generated
// as a conventional UV sphere. Kept non-indexed (like the cube above) so
// drawing it needs no GL surface beyond what the cube already uses — no
// element buffer, no glDrawElements.
std::vector<float> GenerateUnitSphereVertices(int latitudeSegments, int longitudeSegments) {
    auto pointOnSphere = [](float latFraction, float lonFraction) {
        const float theta = latFraction * glm::pi<float>();       // 0 (top) .. pi (bottom)
        const float phi = lonFraction * 2.0f * glm::pi<float>();  // 0 .. 2pi around
        return glm::vec3(std::sin(theta) * std::cos(phi), std::cos(theta),
                          std::sin(theta) * std::sin(phi));
    };

    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(latitudeSegments) * longitudeSegments * 6 * 3);

    for (int lat = 0; lat < latitudeSegments; ++lat) {
        const float v0 = static_cast<float>(lat) / latitudeSegments;
        const float v1 = static_cast<float>(lat + 1) / latitudeSegments;
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            const float u0 = static_cast<float>(lon) / longitudeSegments;
            const float u1 = static_cast<float>(lon + 1) / longitudeSegments;

            const glm::vec3 p00 = pointOnSphere(v0, u0);
            const glm::vec3 p01 = pointOnSphere(v0, u1);
            const glm::vec3 p10 = pointOnSphere(v1, u0);
            const glm::vec3 p11 = pointOnSphere(v1, u1);

            const glm::vec3 quad[6] = {p00, p10, p11, p00, p11, p01};
            for (const glm::vec3& p : quad) {
                vertices.push_back(p.x);
                vertices.push_back(p.y);
                vertices.push_back(p.z);
            }
        }
    }
    return vertices;
}

constexpr int kSphereLatitudeSegments = 16;
constexpr int kSphereLongitudeSegments = 24;

}  // namespace

bool Renderer::Init() {
    GLuint vertexShader = 0;
    if (!CompileShader(GL_VERTEX_SHADER, kVertexShaderSource, vertexShader)) {
        return false;
    }

    GLuint fragmentShader = 0;
    if (!CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSource, fragmentShader)) {
        glDeleteShader(vertexShader);
        return false;
    }

    bool linked = LinkProgram(vertexShader, fragmentShader, m_shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    if (!linked) {
        return false;
    }

    m_uModel = glGetUniformLocation(m_shaderProgram, "uModel");
    m_uView = glGetUniformLocation(m_shaderProgram, "uView");
    m_uProjection = glGetUniformLocation(m_shaderProgram, "uProjection");
    m_uColor = glGetUniformLocation(m_shaderProgram, "uColor");

    glGenVertexArrays(1, &m_cubeVao);
    glBindVertexArray(m_cubeVao);

    glGenBuffers(1, &m_cubeVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVertices), kCubeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const std::vector<float> sphereVertices =
        GenerateUnitSphereVertices(kSphereLatitudeSegments, kSphereLongitudeSegments);
    m_sphereVertexCount = static_cast<GLsizei>(sphereVertices.size() / 3);

    glGenVertexArrays(1, &m_sphereVao);
    glBindVertexArray(m_sphereVao);

    glGenBuffers(1, &m_sphereVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_sphereVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sphereVertices.size() * sizeof(float)),
                 sphereVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    return true;
}

void Renderer::Shutdown() {
    if (m_cubeVbo) {
        glDeleteBuffers(1, &m_cubeVbo);
        m_cubeVbo = 0;
    }
    if (m_cubeVao) {
        glDeleteVertexArrays(1, &m_cubeVao);
        m_cubeVao = 0;
    }
    if (m_sphereVbo) {
        glDeleteBuffers(1, &m_sphereVbo);
        m_sphereVbo = 0;
    }
    if (m_sphereVao) {
        glDeleteVertexArrays(1, &m_sphereVao);
        m_sphereVao = 0;
    }
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
}

void Renderer::BeginFrame(int windowWidth, int windowHeight) {
    glViewport(0, 0, windowWidth, windowHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::SetCamera(const glm::mat4& view, const glm::mat4& projection) {
    m_view = view;
    m_projection = projection;
}

void Renderer::DrawBox(const glm::vec3& position, const glm::quat& rotation,
                        const glm::vec3& halfExtents, const glm::vec3& colorRgb) {
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) *
                             glm::scale(glm::mat4(1.0f), halfExtents * 2.0f);

    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(m_uView, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniform4f(m_uColor, colorRgb.r, colorRgb.g, colorRgb.b, 1.0f);

    glBindVertexArray(m_cubeVao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void Renderer::DrawSphere(const glm::vec3& position, float radius, const glm::vec3& colorRgb) {
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(radius));

    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(m_uView, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniform4f(m_uColor, colorRgb.r, colorRgb.g, colorRgb.b, 1.0f);

    glBindVertexArray(m_sphereVao);
    glDrawArrays(GL_TRIANGLES, 0, m_sphereVertexCount);
}

void Renderer::CaptureFrame(int width, int height, std::vector<unsigned char>& outRgbPixels) const {
    const size_t rowBytes = static_cast<size_t>(width) * 3;
    outRgbPixels.assign(rowBytes * static_cast<size_t>(height), 0);

    // glReadPixels has no alignment padding to worry about here since 3
    // bytes/pixel with typical widths doesn't need a custom GL_PACK_ALIGNMENT
    // for this tool's purposes (rows are read directly into the output
    // buffer, then flipped below).
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, outRgbPixels.data());

    // OpenGL's framebuffer origin is bottom-left; flip rows so row 0 of the
    // output is the top of the image, matching ordinary image conventions.
    std::vector<unsigned char> rowBuffer(rowBytes);
    for (int row = 0; row < height / 2; ++row) {
        unsigned char* top = outRgbPixels.data() + static_cast<size_t>(row) * rowBytes;
        unsigned char* bottom =
            outRgbPixels.data() + static_cast<size_t>(height - 1 - row) * rowBytes;
        std::copy(top, top + rowBytes, rowBuffer.begin());
        std::copy(bottom, bottom + rowBytes, top);
        std::copy(rowBuffer.begin(), rowBuffer.end(), bottom);
    }
}

void Renderer::EndFrame() {
    // Nothing to do yet; kept as an explicit boundary for future per-frame
    // work (batching, multiple draw calls, etc.) rather than for any
    // behavior this milestone needs.
}
