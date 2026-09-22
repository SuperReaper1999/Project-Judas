#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {

// Milestone 9: every mesh (built-in primitives and imported models alike)
// now carries position + normal + UV, and is lit by one small ambient term
// plus one directional light, then modulated by a sampled texture (a 1x1
// white fallback for a mesh with no real one — see Renderer::DrawMesh) and
// a per-draw tint color. See docs/ARCHITECTURE.md, "Milestone 9, Lighting,"
// for the exact model and why nothing here reads gravity/local-up/any
// Judas-specific concept — this shader only ever sees plain world-space
// vectors handed to it by SetLighting.
const char* kVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec3 aLocalPos;
layout(location = 1) in vec3 aLocalNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

out vec3 vWorldNormal;
out vec2 vUV;

void main() {
    // uNormalMatrix = transpose(inverse(mat3(uModel))), computed on the CPU
    // once per draw call (see DrawMesh) — the standard correction so
    // normals stay perpendicular to their surface under non-uniform scale
    // (DrawBox's halfExtents are rarely a uniform scale), not just rotation.
    vWorldNormal = uNormalMatrix * aLocalNormal;
    vUV = aUV;
    gl_Position = uProjection * uView * uModel * vec4(aLocalPos, 1.0);
}
)";

const char* kFragmentShaderSource = R"(#version 330 core
in vec3 vWorldNormal;
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uColor;
uniform vec3 uLightDirection;  // world-space, normalized, points FROM the surface TOWARD the light
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;

void main() {
    vec3 normal = normalize(vWorldNormal);
    float diffuseFactor = max(dot(normal, uLightDirection), 0.0);
    vec3 lighting = uAmbientColor + uLightColor * diffuseFactor;
    vec4 texColor = texture(uTexture, vUV);
    FragColor = vec4(lighting, 1.0) * texColor * uColor;
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

// A unit cube (36 vertices, non-indexed, two triangles per face) centered on
// the origin in local [-0.5, 0.5] space — the same geometry Judas has used
// since Milestone 2, now carrying a real per-face normal (flat-shaded, no
// vertex-normal averaging across faces — an ordinary box's edges are
// genuinely sharp) and a UV per vertex. UVs use a fixed corner pattern
// (documented below) rather than a geometrically considered box unwrap:
// every existing caller draws boxes with the white fallback texture (see
// Renderer::DrawMesh), so UV correctness here is not yet exercised by
// anything — revisit if a future milestone textures a box for real. Winding
// order is not consistent per face since face culling is not enabled, kept
// unchanged from every milestone before this one.
MeshData BuildCubeMeshData() {
    struct FaceVertex {
        glm::vec3 position;
    };
    // clang-format off
    const FaceVertex kPositions[36] = {
        // back face (normal -Z)
        {{-0.5f, -0.5f, -0.5f}}, {{0.5f, 0.5f, -0.5f}},  {{0.5f, -0.5f, -0.5f}},
        {{0.5f, 0.5f, -0.5f}},   {{-0.5f, -0.5f, -0.5f}}, {{-0.5f, 0.5f, -0.5f}},
        // front face (normal +Z)
        {{-0.5f, -0.5f, 0.5f}},  {{0.5f, -0.5f, 0.5f}},  {{0.5f, 0.5f, 0.5f}},
        {{0.5f, 0.5f, 0.5f}},    {{-0.5f, 0.5f, 0.5f}},  {{-0.5f, -0.5f, 0.5f}},
        // left face (normal -X)
        {{-0.5f, 0.5f, 0.5f}},   {{-0.5f, 0.5f, -0.5f}}, {{-0.5f, -0.5f, -0.5f}},
        {{-0.5f, -0.5f, -0.5f}}, {{-0.5f, -0.5f, 0.5f}}, {{-0.5f, 0.5f, 0.5f}},
        // right face (normal +X)
        {{0.5f, 0.5f, 0.5f}},    {{0.5f, -0.5f, -0.5f}}, {{0.5f, 0.5f, -0.5f}},
        {{0.5f, -0.5f, -0.5f}},  {{0.5f, 0.5f, 0.5f}},   {{0.5f, -0.5f, 0.5f}},
        // bottom face (normal -Y)
        {{-0.5f, -0.5f, -0.5f}}, {{0.5f, -0.5f, -0.5f}}, {{0.5f, -0.5f, 0.5f}},
        {{0.5f, -0.5f, 0.5f}},   {{-0.5f, -0.5f, 0.5f}}, {{-0.5f, -0.5f, -0.5f}},
        // top face (normal +Y)
        {{-0.5f, 0.5f, -0.5f}},  {{0.5f, 0.5f, 0.5f}},   {{0.5f, 0.5f, -0.5f}},
        {{0.5f, 0.5f, 0.5f}},    {{-0.5f, 0.5f, -0.5f}}, {{-0.5f, 0.5f, 0.5f}},
    };
    // clang-format on
    const glm::vec3 kFaceNormals[6] = {
        {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},  {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    };
    // A fixed 2-triangle-quad UV pattern, reused identically for every face
    // — see the function comment above for why exact per-face unwrapping
    // isn't needed yet.
    const glm::vec2 kFaceUVs[6] = {
        {0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f},
    };

    MeshData mesh;
    mesh.vertices.reserve(36);
    for (int face = 0; face < 6; ++face) {
        for (int v = 0; v < 6; ++v) {
            MeshVertex vertex;
            vertex.position = kPositions[face * 6 + v].position;
            vertex.normal = kFaceNormals[face];
            vertex.uv = kFaceUVs[v];
            mesh.vertices.push_back(vertex);
        }
    }
    return mesh;  // indices left empty: drawn non-indexed, as always
}

// A unit sphere (radius 1, non-indexed triangles), generated as a
// conventional UV sphere. A unit sphere's own outward normal at any point
// is simply that point itself (already unit length) — no separate normal
// computation needed. UV uses the ordinary lat/lon parameterization
// (u = longitude fraction, v = 1 - latitude fraction so v=1 is the pole at
// local +Y, matching the same bottom-is-v0 convention documented for
// textures — see docs/ARCHITECTURE.md).
MeshData GenerateUnitSphereMeshData(int latitudeSegments, int longitudeSegments) {
    auto pointOnSphere = [](float latFraction, float lonFraction) {
        const float theta = latFraction * glm::pi<float>();       // 0 (top) .. pi (bottom)
        const float phi = lonFraction * 2.0f * glm::pi<float>();  // 0 .. 2pi around
        return glm::vec3(std::sin(theta) * std::cos(phi), std::cos(theta),
                          std::sin(theta) * std::sin(phi));
    };

    MeshData mesh;
    mesh.vertices.reserve(static_cast<size_t>(latitudeSegments) * longitudeSegments * 6);

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

            const glm::vec3 positions[6] = {p00, p10, p11, p00, p11, p01};
            const glm::vec2 uvs[6] = {{u0, 1.0f - v0}, {u0, 1.0f - v1}, {u1, 1.0f - v1},
                                       {u0, 1.0f - v0}, {u1, 1.0f - v1}, {u1, 1.0f - v0}};
            for (int i = 0; i < 6; ++i) {
                MeshVertex vertex;
                vertex.position = positions[i];
                vertex.normal = positions[i];  // unit sphere: normal == position
                vertex.uv = uvs[i];
                mesh.vertices.push_back(vertex);
            }
        }
    }
    return mesh;  // indices left empty: drawn non-indexed, as always
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
    m_uNormalMatrix = glGetUniformLocation(m_shaderProgram, "uNormalMatrix");
    m_uView = glGetUniformLocation(m_shaderProgram, "uView");
    m_uProjection = glGetUniformLocation(m_shaderProgram, "uProjection");
    m_uColor = glGetUniformLocation(m_shaderProgram, "uColor");
    m_uTexture = glGetUniformLocation(m_shaderProgram, "uTexture");
    m_uLightDirection = glGetUniformLocation(m_shaderProgram, "uLightDirection");
    m_uLightColor = glGetUniformLocation(m_shaderProgram, "uLightColor");
    m_uAmbientColor = glGetUniformLocation(m_shaderProgram, "uAmbientColor");

    // Texture unit 0 is the only one this engine ever uses — bound once
    // here rather than every draw call, since it never changes.
    glUseProgram(m_shaderProgram);
    glUniform1i(m_uTexture, 0);

    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    m_cubeMesh = CreateMesh(BuildCubeMeshData());
    m_sphereMesh = CreateMesh(GenerateUnitSphereMeshData(kSphereLatitudeSegments, kSphereLongitudeSegments));

    // The "no real texture" fallback DrawMesh substitutes for an invalid
    // TextureHandle (see ResolveTexture) — a single opaque white pixel, so
    // `texColor * tintColor` reduces to exactly `tintColor`, reproducing
    // every pre-Milestone-9 solid-color DrawBox/DrawSphere call exactly.
    TextureData white;
    white.width = 1;
    white.height = 1;
    white.pixels = {255, 255, 255, 255};
    m_whiteTexture = CreateTexture(white);

    // A small default so a mesh drawn before the caller's first SetLighting
    // call (shouldn't happen in practice, but costs nothing to guard) is at
    // least dimly visible rather than pitch black.
    SetLighting(glm::vec3(0.3f, 0.6f, 0.4f), glm::vec3(1.0f), glm::vec3(0.15f));

    return true;
}

void Renderer::Shutdown() {
    for (GpuMesh& mesh : m_meshes) {
        if (!mesh.alive) continue;
        if (mesh.ebo) glDeleteBuffers(1, &mesh.ebo);
        glDeleteBuffers(1, &mesh.vbo);
        glDeleteVertexArrays(1, &mesh.vao);
        mesh.alive = false;
    }
    m_meshes.clear();

    for (GpuTexture& texture : m_textures) {
        if (!texture.alive) continue;
        glDeleteTextures(1, &texture.textureId);
        texture.alive = false;
    }
    m_textures.clear();

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

void Renderer::SetLighting(const glm::vec3& direction, const glm::vec3& lightColor,
                            const glm::vec3& ambientColor) {
    glUseProgram(m_shaderProgram);
    const glm::vec3 normalizedDirection =
        glm::length(direction) > 1.0e-6f ? glm::normalize(direction) : glm::vec3(0.0f, 1.0f, 0.0f);
    glUniform3f(m_uLightDirection, normalizedDirection.x, normalizedDirection.y,
                normalizedDirection.z);
    glUniform3f(m_uLightColor, lightColor.r, lightColor.g, lightColor.b);
    glUniform3f(m_uAmbientColor, ambientColor.r, ambientColor.g, ambientColor.b);
}

MeshHandle Renderer::CreateMesh(const MeshData& data) {
    GpuMesh mesh;
    mesh.alive = true;
    mesh.vertexCount = static_cast<GLsizei>(data.vertices.size());

    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.vertices.size() * sizeof(MeshVertex)),
                 data.vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                           reinterpret_cast<const void*>(offsetof(MeshVertex, position)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                           reinterpret_cast<const void*>(offsetof(MeshVertex, normal)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                           reinterpret_cast<const void*>(offsetof(MeshVertex, uv)));
    glEnableVertexAttribArray(2);

    if (!data.indices.empty()) {
        mesh.indexCount = static_cast<GLsizei>(data.indices.size());
        glGenBuffers(1, &mesh.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(data.indices.size() * sizeof(std::uint32_t)),
                     data.indices.data(), GL_STATIC_DRAW);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // Deliberately NOT unbinding GL_ELEMENT_ARRAY_BUFFER here — it is part
    // of the VAO's own state, and unbinding it while this VAO is still
    // bound would detach it from the VAO itself.

    MeshHandle handle;
    handle.id = static_cast<unsigned int>(m_meshes.size());
    m_meshes.push_back(mesh);
    return handle;
}

Renderer::GpuMesh* Renderer::GetMesh(MeshHandle handle) {
    if (!handle.IsValid() || handle.id >= m_meshes.size() || !m_meshes[handle.id].alive) {
        return nullptr;
    }
    return &m_meshes[handle.id];
}

void Renderer::DestroyMesh(MeshHandle handle) {
    GpuMesh* mesh = GetMesh(handle);
    if (!mesh) return;
    if (mesh->ebo) glDeleteBuffers(1, &mesh->ebo);
    glDeleteBuffers(1, &mesh->vbo);
    glDeleteVertexArrays(1, &mesh->vao);
    *mesh = GpuMesh{};
}

TextureHandle Renderer::CreateTexture(const TextureData& data) {
    GpuTexture texture;
    texture.alive = true;

    glGenTextures(1, &texture.textureId);
    glBindTexture(GL_TEXTURE_2D, texture.textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, data.width, data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 data.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);

    // Linear filtering both ways (mipmapped minification), repeat wrapping —
    // ordinary, sufficient defaults for one UV-mapped demo texture; nothing
    // here yet needs per-texture control over these (see
    // docs/ARCHITECTURE.md's remaining-limitations note).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);

    TextureHandle handle;
    handle.id = static_cast<unsigned int>(m_textures.size());
    m_textures.push_back(texture);
    return handle;
}

void Renderer::DestroyTexture(TextureHandle handle) {
    if (!handle.IsValid() || handle.id >= m_textures.size() || !m_textures[handle.id].alive) {
        return;
    }
    glDeleteTextures(1, &m_textures[handle.id].textureId);
    m_textures[handle.id] = GpuTexture{};
}

GLuint Renderer::ResolveTexture(TextureHandle handle) const {
    if (handle.IsValid() && handle.id < m_textures.size() && m_textures[handle.id].alive) {
        return m_textures[handle.id].textureId;
    }
    // Falls back to the white 1x1 texture — including, harmlessly, during
    // Init() itself while m_whiteTexture is still being constructed (the
    // fallback-of-the-fallback is textureId 0, an unbound texture object,
    // which GL_TEXTURE_2D simply samples as opaque black; never reached in
    // practice since nothing draws before Init finishes).
    if (m_whiteTexture.IsValid() && m_whiteTexture.id < m_textures.size()) {
        return m_textures[m_whiteTexture.id].textureId;
    }
    return 0;
}

void Renderer::DrawMesh(MeshHandle mesh, const glm::vec3& position, const glm::quat& rotation,
                         const glm::vec3& scale, TextureHandle texture,
                         const glm::vec3& tintColor) {
    GpuMesh* gpuMesh = GetMesh(mesh);
    if (!gpuMesh) return;

    const glm::mat4 model = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) *
                             glm::scale(glm::mat4(1.0f), scale);
    // Standard correction for non-uniform scale — see the vertex shader's
    // own comment. glm::inverseTranspose is glm's dedicated helper for
    // exactly this (normal-matrix) computation.
    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(model));

    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(m_uNormalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    glUniformMatrix4fv(m_uView, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniform4f(m_uColor, tintColor.r, tintColor.g, tintColor.b, 1.0f);

    glBindTexture(GL_TEXTURE_2D, ResolveTexture(texture));
    glBindVertexArray(gpuMesh->vao);
    if (gpuMesh->ebo) {
        glDrawElements(GL_TRIANGLES, gpuMesh->indexCount, GL_UNSIGNED_INT, nullptr);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, gpuMesh->vertexCount);
    }
}

void Renderer::DrawBox(const glm::vec3& position, const glm::quat& rotation,
                        const glm::vec3& halfExtents, const glm::vec3& colorRgb) {
    DrawMesh(m_cubeMesh, position, rotation, halfExtents * 2.0f, TextureHandle{}, colorRgb);
}

void Renderer::DrawSphere(const glm::vec3& position, float radius, const glm::vec3& colorRgb) {
    DrawMesh(m_sphereMesh, position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(radius),
             TextureHandle{}, colorRgb);
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
