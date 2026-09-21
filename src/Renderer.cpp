#include "Renderer.h"

#include <cstdio>
#include <vector>

namespace {

const char* kVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec2 aLocalPos;

uniform mat4 uProjection;
uniform vec2 uPosition;
uniform vec2 uSize;

void main() {
    vec2 worldPos = aLocalPos * uSize + uPosition;
    gl_Position = uProjection * vec4(worldPos, 0.0, 1.0);
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

// Builds a standard OpenGL orthographic projection for [0,w] x [0,h] with
// the origin at the bottom-left and +y pointing up, written in GL's
// column-major uniform layout.
void BuildOrthographicProjection(float width, float height, float outMatrix[16]) {
    for (int i = 0; i < 16; ++i) outMatrix[i] = 0.0f;
    outMatrix[0] = 2.0f / width;
    outMatrix[5] = 2.0f / height;
    outMatrix[10] = -1.0f;
    outMatrix[12] = -1.0f;
    outMatrix[13] = -1.0f;
    outMatrix[15] = 1.0f;
}

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

    m_uProjection = glGetUniformLocation(m_shaderProgram, "uProjection");
    m_uPosition = glGetUniformLocation(m_shaderProgram, "uPosition");
    m_uSize = glGetUniformLocation(m_shaderProgram, "uSize");
    m_uColor = glGetUniformLocation(m_shaderProgram, "uColor");

    // Unit quad (two triangles), centered at the origin, in local [-0.5, 0.5] space.
    const float quadVertices[] = {
        -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,

        -0.5f, -0.5f, 0.5f,  0.5f, -0.5f, 0.5f,
    };

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);

    return true;
}

void Renderer::Shutdown() {
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_shaderProgram) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
}

void Renderer::BeginFrame(int windowWidth, int windowHeight) {
    glViewport(0, 0, windowWidth, windowHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    BuildOrthographicProjection(static_cast<float>(windowWidth),
                                 static_cast<float>(windowHeight), m_projection);
}

void Renderer::DrawRect(float centerX, float centerY, float width, float height, float r,
                         float g, float b, float a) {
    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, m_projection);
    glUniform2f(m_uPosition, centerX, centerY);
    glUniform2f(m_uSize, width, height);
    glUniform4f(m_uColor, r, g, b, a);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::EndFrame() {
    // Nothing to do yet; this exists as an explicit boundary for future
    // per-frame work (batching, multiple draw calls, etc.) rather than for
    // any behavior Milestone 1 needs.
}
