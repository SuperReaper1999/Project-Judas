#include "Renderer.h"

#include <cstdio>
#include <vector>

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

void Renderer::DrawCube(const glm::vec3& position, const glm::vec3& colorRgb) {
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), position);

    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(m_uView, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniform4f(m_uColor, colorRgb.r, colorRgb.g, colorRgb.b, 1.0f);

    glBindVertexArray(m_cubeVao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void Renderer::EndFrame() {
    // Nothing to do yet; kept as an explicit boundary for future per-frame
    // work (batching, multiple draw calls, etc.) rather than for any
    // behavior this milestone needs.
}
