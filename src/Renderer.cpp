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

// Milestone 15: one light-space transform PER SHADOW SLOT (see
// src/Light.h's kDirectionalShadowSlot/kTorchShadowSlot/
// kShipHeadlightShadowSlot) — always all three, computed unconditionally
// every vertex regardless of whether this frame's active lights actually
// use each one (e.g. the torch slot when the torch is off): cheap at this
// engine's tiny vertex counts, and far simpler than a dynamically-indexed
// varying (GLSL doesn't support indexing a varying array by a value that
// differs per dynamic light in the fragment shader's own per-light loop —
// see the fragment shader below for how the FIXED set of three is
// selected between instead).
uniform mat4 uLightSpaceMatrix[3];

out vec3 vWorldNormal;
out vec3 vWorldPos;
out vec2 vUV;
out vec4 vDirLightSpacePos;
out vec4 vTorchLightSpacePos;
out vec4 vShipLightSpacePos;

void main() {
    // uNormalMatrix = transpose(inverse(mat3(uModel))), computed on the CPU
    // once per draw call (see DrawMesh) — the standard correction so
    // normals stay perpendicular to their surface under non-uniform scale
    // (DrawBox's halfExtents are rarely a uniform scale), not just rotation.
    vWorldNormal = uNormalMatrix * aLocalNormal;
    // Milestone 14: the fragment's own world-space position, needed so the
    // fragment shader can compute a per-fragment vector TO each dynamic
    // point/spot light (distance-based attenuation, cone angle) — the
    // Milestone 9 directional light never needed this, since a directional
    // light's contribution doesn't depend on fragment position at all.
    vWorldPos = vec3(uModel * vec4(aLocalPos, 1.0));
    vUV = aUV;
    vDirLightSpacePos = uLightSpaceMatrix[0] * vec4(vWorldPos, 1.0);
    vTorchLightSpacePos = uLightSpaceMatrix[1] * vec4(vWorldPos, 1.0);
    vShipLightSpacePos = uLightSpaceMatrix[2] * vec4(vWorldPos, 1.0);
    gl_Position = uProjection * uView * uModel * vec4(aLocalPos, 1.0);
}
)";

const char* kFragmentShaderSource = R"(#version 330 core
in vec3 vWorldNormal;
in vec3 vWorldPos;
in vec2 vUV;
in vec4 vDirLightSpacePos;
in vec4 vTorchLightSpacePos;
in vec4 vShipLightSpacePos;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uColor;
uniform vec3 uLightDirection;  // world-space, normalized, points FROM the surface TOWARD the light
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;

// Milestone 15: one depth texture per shadow slot (see src/Light.h) —
// slot 0 is the directional "sun," slot 1 the player torch, slot 2 the
// spacecraft headlight. Bound to fixed texture units 1/2/3 (uTexture stays
// on unit 0 — see Renderer::Init) so all four textures this shader ever
// samples are bound simultaneously, no rebinding between them mid-draw.
uniform sampler2D uShadowMapDir;
uniform sampler2D uShadowMapTorch;
uniform sampler2D uShadowMapShip;

// Milestone 14: dynamic point/spot lights — see src/Light.h and
// docs/ARCHITECTURE.md, "Milestone 14," for the full model. A fixed-size
// array (kMaxDynamicLights on the CPU side, mirrored here) is
// deliberately small and explicit rather than unbounded — see "Milestone
// 14, Light limits." `uLightCount` (<= the array size) bounds the loop so
// unused slots are never touched, not merely zeroed.
#define MAX_DYNAMIC_LIGHTS 5
uniform int uLightCount;
uniform vec3 uDynamicLightPosition[MAX_DYNAMIC_LIGHTS];
uniform vec3 uDynamicLightDirection[MAX_DYNAMIC_LIGHTS];  // spot only; the direction the light FACES
uniform vec3 uDynamicLightColor[MAX_DYNAMIC_LIGHTS];      // already intensity-scaled, see Renderer.cpp
uniform float uDynamicLightRange[MAX_DYNAMIC_LIGHTS];
uniform float uDynamicLightInnerCos[MAX_DYNAMIC_LIGHTS];  // spot only
uniform float uDynamicLightOuterCos[MAX_DYNAMIC_LIGHTS];  // spot only
uniform int uDynamicLightIsSpot[MAX_DYNAMIC_LIGHTS];      // 0 = point, 1 = spot
// Milestone 15: -1 = this light casts no shadow; 1 = uses vTorchLightSpacePos
// / uShadowMapTorch; 2 = uses vShipLightSpacePos / uShadowMapShip (slot 0,
// the directional light, is applied separately below, not through this
// per-dynamic-light array — see src/Light.h's kDirectionalShadowSlot/
// kTorchShadowSlot/kShipHeadlightShadowSlot).
uniform int uDynamicLightShadowIndex[MAX_DYNAMIC_LIGHTS];

// Milestone 15: samples `shadowMap` at `lightSpacePos` (already multiplied
// by that light's own view*projection in the vertex shader) and returns
// how LIT this fragment is (1.0 = fully lit, 0.0 = fully shadowed) — a
// simple 3x3 percentage-closer-filter (9 taps) for a soft, non-aliased
// edge, the minimal sensible technique this milestone's brief allows (see
// docs/ARCHITECTURE.md, "Milestone 15," for why nothing fancier was
// built). A slope-scaled bias (steeper-facing surfaces need a larger
// bias) avoids most shadow-acne self-shadowing while limiting peter-
// panning; a fragment whose projected position falls outside the shadow
// map's own [0,1] coverage (or beyond its far plane) is treated as fully
// lit — there is no occluder DATA there, not evidence of no occluder, but
// this is the same "no light reaches unclaimed space" honesty this
// engine already applies elsewhere (see docs/ARCHITECTURE.md's gravity-
// context law) rather than guessing.
float ComputeShadowFactor(vec4 lightSpacePos, sampler2D shadowMap, float ndotl) {
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;  // NDC [-1,1] -> texture/depth [0,1]

    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0;
    }

    float bias = max(0.006 * (1.0 - ndotl), 0.0015);
    float currentDepth = projCoords.z - bias;

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float litSum = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            litSum += currentDepth <= closestDepth ? 1.0 : 0.0;
        }
    }
    return litSum / 9.0;
}

void main() {
    vec3 normal = normalize(vWorldNormal);

    float diffuseFactor = max(dot(normal, uLightDirection), 0.0);
    float dirShadow = ComputeShadowFactor(vDirLightSpacePos, uShadowMapDir, diffuseFactor);
    vec3 lighting = uAmbientColor + uLightColor * diffuseFactor * dirShadow;

    for (int i = 0; i < uLightCount; ++i) {
        vec3 toLight = uDynamicLightPosition[i] - vWorldPos;
        float distance = length(toLight);
        vec3 lightDir = distance > 1.0e-5 ? toLight / distance : vec3(0.0, 1.0, 0.0);

        // Smooth-windowed inverse-square attenuation (see Renderer.cpp,
        // SetDynamicLights, for the full derivation/citation) — genuinely
        // inverse-square close to the light, smoothly reaches exactly zero
        // at uDynamicLightRange[i] instead of a hard cliff or a never-zero
        // tail, and the "+1.0" keeps it finite as distance approaches 0.
        float rangeFraction = clamp(distance / max(uDynamicLightRange[i], 1.0e-4), 0.0, 1.0);
        float windowed = 1.0 - rangeFraction * rangeFraction * rangeFraction * rangeFraction;
        windowed = clamp(windowed, 0.0, 1.0);
        float attenuation = (windowed * windowed) / (distance * distance + 1.0);

        float spotFactor = 1.0;
        if (uDynamicLightIsSpot[i] != 0) {
            float cosAngle = dot(-lightDir, uDynamicLightDirection[i]);
            spotFactor = smoothstep(uDynamicLightOuterCos[i], uDynamicLightInnerCos[i], cosAngle);
        }

        float lightDiffuse = max(dot(normal, lightDir), 0.0);

        float shadow = 1.0;
        int shadowIndex = uDynamicLightShadowIndex[i];
        if (shadowIndex == 1) {
            shadow = ComputeShadowFactor(vTorchLightSpacePos, uShadowMapTorch, lightDiffuse);
        } else if (shadowIndex == 2) {
            shadow = ComputeShadowFactor(vShipLightSpacePos, uShadowMapShip, lightDiffuse);
        }

        lighting += uDynamicLightColor[i] * lightDiffuse * attenuation * spotFactor * shadow;
    }

    vec4 texColor = texture(uTexture, vUV);
    FragColor = vec4(lighting, 1.0) * texColor * uColor;
}
)";

// Milestone 15: the shadow pass's own minimal shader — position only, no
// normal/UV attributes read, no color output at all (the FBO it draws
// into has no color attachment, see Renderer::Init — only depth is
// written, by the fixed-function depth test/write GL already performs
// for every draw call). Deliberately separate from kVertexShaderSource/
// kFragmentShaderSource above, the same "a second, dedicated shader for a
// genuinely different pass" reasoning kUIVertexShaderSource/
// kUIFragmentShaderSource already established for the UI overlay.
const char* kShadowVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec3 aLocalPos;

uniform mat4 uModel;
uniform mat4 uLightViewProj;

void main() {
    gl_Position = uLightViewProj * uModel * vec4(aLocalPos, 1.0);
}
)";

const char* kShadowFragmentShaderSource = R"(#version 330 core
void main() {
    // Intentionally empty: this FBO has no color attachment (see
    // Renderer::Init's glDrawBuffer(GL_NONE)) — only gl_FragDepth's
    // implicit default (gl_FragCoord.z) is ever written, by the ordinary
    // depth test every draw call already performs.
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

// Milestone 15: shadow-map resolution, shared by Init (texture creation)
// and BeginShadowPass (viewport sizing) — see docs/ARCHITECTURE.md,
// "Milestone 15," for why 1024x1024 was judged sufficient (and not
// excessive) for this demo's world scale, for all three shadow slots
// alike (no separate resolution per light).
constexpr int kShadowMapResolution = 1024;

// Milestone 13: the UI overlay's own tiny shader — deliberately separate
// from kVertexShaderSource/kFragmentShaderSource above rather than a
// reused/branching variant of them. The 3D shader's whole shape (a
// view/projection matrix, a world-space normal, per-fragment lighting) is
// dead weight for a screen-space rectangle, and forcing UI draws through it
// would mean disabling lighting with uniform tricks instead of just not
// having it. `uPosition`/`uSize` are already in PIXELS — this shader
// converts straight to clip space using `uScreenSize`, no separate
// orthographic projection matrix needed. `uUVOffset`/`uUVScale` select a
// sub-rectangle of whatever texture is bound (the whole [0,1] rect for a
// solid-color panel via the white fallback texture, a font atlas glyph's
// own rect for text) — see Renderer::DrawUIRect/DrawUIText.
const char* kUIVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec2 aUnit;  // 0..1 unit quad, top-left origin

uniform vec2 uScreenSize;
uniform vec2 uPosition;  // pixels, top-left of this rect
uniform vec2 uSize;      // pixels

out vec2 vUnit;

void main() {
    vUnit = aUnit;
    vec2 pixelPos = uPosition + aUnit * uSize;
    // Pixel space is top-down (y grows downward, matching uPosition's own
    // "top-left corner" convention); NDC y grows upward, so it's flipped
    // here rather than by pre-flipping any texture data — see
    // src/FontLoader.h's own header comment for why the glyph atlas needs
    // no separate flip convention as a result.
    vec2 ndc = vec2(pixelPos.x / uScreenSize.x * 2.0 - 1.0,
                     1.0 - pixelPos.y / uScreenSize.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

const char* kUIFragmentShaderSource = R"(#version 330 core
in vec2 vUnit;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uColor;
uniform vec2 uUVOffset;
uniform vec2 uUVScale;

void main() {
    vec2 uv = uUVOffset + vUnit * uUVScale;
    FragColor = texture(uTexture, uv) * uColor;
}
)";

// Milestone 30: the debug-line shader — world-space position + colour per
// vertex, one view-projection uniform, no lighting, no texture. Kept apart
// from the lit mesh shader for the same reason the UI shader is: it is a
// genuinely different pass (GL_LINES, per-vertex colour, streamed data).
const char* kDebugVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
uniform mat4 uViewProjection;
out vec3 vColor;
void main() {
    vColor = aColor;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}
)";

const char* kDebugFragmentShaderSource = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

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

    // Milestone 14: one uniform location per dynamic-light field, per
    // array slot — see Renderer.h's own comment on why these can't be
    // cached as a single location the way a plain uniform can.
    m_uLightCount = glGetUniformLocation(m_shaderProgram, "uLightCount");
    for (int i = 0; i < kMaxDynamicLights; ++i) {
        const std::string prefix = "uDynamicLight";
        const std::string index = "[" + std::to_string(i) + "]";
        m_uDynamicLightPosition[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "Position" + index).c_str());
        m_uDynamicLightDirection[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "Direction" + index).c_str());
        m_uDynamicLightColor[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "Color" + index).c_str());
        m_uDynamicLightRange[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "Range" + index).c_str());
        m_uDynamicLightInnerCos[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "InnerCos" + index).c_str());
        m_uDynamicLightOuterCos[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "OuterCos" + index).c_str());
        m_uDynamicLightIsSpot[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "IsSpot" + index).c_str());
        m_uDynamicLightShadowIndex[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "ShadowIndex" + index).c_str());
    }

    // Milestone 15: one mat4 + one sampler2D location per shadow slot.
    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        const std::string matrixName = "uLightSpaceMatrix[" + std::to_string(slot) + "]";
        m_uLightSpaceMatrix[slot] = glGetUniformLocation(m_shaderProgram, matrixName.c_str());
    }
    m_uShadowMapSampler[0] = glGetUniformLocation(m_shaderProgram, "uShadowMapDir");
    m_uShadowMapSampler[1] = glGetUniformLocation(m_shaderProgram, "uShadowMapTorch");
    m_uShadowMapSampler[2] = glGetUniformLocation(m_shaderProgram, "uShadowMapShip");

    // Texture unit 0 is the diffuse/white-fallback texture (unchanged
    // since Milestone 9); units 1-3 are the three shadow maps (Milestone
    // 15) — all four bound once here rather than every draw call, since
    // which GL texture OBJECT each unit points at only ever changes when a
    // shadow map is re-rendered (see BeginShadowPass), not which UNIT a
    // given uniform samples from.
    glUseProgram(m_shaderProgram);
    glUniform1i(m_uTexture, 0);
    glUniform1i(m_uShadowMapSampler[0], 1);
    glUniform1i(m_uShadowMapSampler[1], 2);
    glUniform1i(m_uShadowMapSampler[2], 3);

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
    // Milestone 14: no dynamic lights until the caller's first
    // SetDynamicLights call — explicit rather than relying on GLSL's own
    // zero-initialized-uniform default, so this is true by construction,
    // not by an implementation detail of the driver.
    SetDynamicLights({});

    // --- Milestone 15: shadow-mapping resources ---
    GLuint shadowVertexShader = 0;
    if (!CompileShader(GL_VERTEX_SHADER, kShadowVertexShaderSource, shadowVertexShader)) {
        return false;
    }
    GLuint shadowFragmentShader = 0;
    if (!CompileShader(GL_FRAGMENT_SHADER, kShadowFragmentShaderSource, shadowFragmentShader)) {
        glDeleteShader(shadowVertexShader);
        return false;
    }
    const bool shadowLinked = LinkProgram(shadowVertexShader, shadowFragmentShader, m_shadowShaderProgram);
    glDeleteShader(shadowVertexShader);
    glDeleteShader(shadowFragmentShader);
    if (!shadowLinked) {
        return false;
    }
    m_uShadowModel = glGetUniformLocation(m_shadowShaderProgram, "uModel");
    m_uShadowLightViewProj = glGetUniformLocation(m_shadowShaderProgram, "uLightViewProj");

    // One depth-texture/FBO pair per shadow slot (src/Light.h), created
    // once and reused every frame — never allocated/freed per-light or
    // per-draw (see docs/ARCHITECTURE.md, "Milestone 15, Resource
    // ownership"). GL_NEAREST filtering (not the usual GL_LINEAR) because
    // this engine does its own multi-tap PCF filtering in the fragment
    // shader (see kFragmentShaderSource's ComputeShadowFactor) — linearly
    // filtering raw, unblended depth VALUES before comparison would
    // average depths together in a way that's meaningless (and wrong) for
    // a shadow test, unlike ordinary color filtering. GL_CLAMP_TO_EDGE
    // wrapping plus an explicit in-shader bounds check (rather than
    // GL_CLAMP_TO_BORDER with a border color) keeps this to GL surface
    // this engine already has — see ComputeShadowFactor's own "outside
    // the shadow map's coverage counts as fully lit" comment.
    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        glGenTextures(1, &m_shadowMapTexture[slot]);
        glBindTexture(GL_TEXTURE_2D, m_shadowMapTexture[slot]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, kShadowMapResolution, kShadowMapResolution, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &m_shadowFbo[slot]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo[slot]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                m_shadowMapTexture[slot], 0);
        // No color attachment exists for this FBO at all — tell GL not to
        // expect or provide one, or GL_FRAMEBUFFER_COMPLETE would
        // (correctly) fail on some drivers.
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "Shadow framebuffer %d is incomplete.\n", slot);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            return false;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // --- Milestone 13: UI overlay shader + quad ---
    GLuint uiVertexShader = 0;
    if (!CompileShader(GL_VERTEX_SHADER, kUIVertexShaderSource, uiVertexShader)) {
        return false;
    }
    GLuint uiFragmentShader = 0;
    if (!CompileShader(GL_FRAGMENT_SHADER, kUIFragmentShaderSource, uiFragmentShader)) {
        glDeleteShader(uiVertexShader);
        return false;
    }
    const bool uiLinked = LinkProgram(uiVertexShader, uiFragmentShader, m_uiShaderProgram);
    glDeleteShader(uiVertexShader);
    glDeleteShader(uiFragmentShader);
    if (!uiLinked) {
        return false;
    }

    m_uiUScreenSize = glGetUniformLocation(m_uiShaderProgram, "uScreenSize");
    m_uiUPosition = glGetUniformLocation(m_uiShaderProgram, "uPosition");
    m_uiUSize = glGetUniformLocation(m_uiShaderProgram, "uSize");
    m_uiUColor = glGetUniformLocation(m_uiShaderProgram, "uColor");
    m_uiUTexture = glGetUniformLocation(m_uiShaderProgram, "uTexture");
    m_uiUUVOffset = glGetUniformLocation(m_uiShaderProgram, "uUVOffset");
    m_uiUUVScale = glGetUniformLocation(m_uiShaderProgram, "uUVScale");
    glUseProgram(m_uiShaderProgram);
    glUniform1i(m_uiUTexture, 0);

    // A single non-indexed unit quad (two triangles, top-left origin,
    // [0,1]x[0,1]) reused for every DrawUIRect/DrawUIText glyph call —
    // per-draw placement/size/UV-rect come entirely from uniforms (see
    // kUIVertexShaderSource), so no per-call vertex upload is needed, the
    // same "one shared mesh, transform via uniforms" shape DrawBox/
    // DrawSphere already use with m_cubeMesh/m_sphereMesh.
    const float kUnitQuadVertices[12] = {
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
    };
    glGenVertexArrays(1, &m_uiQuadVao);
    glBindVertexArray(m_uiQuadVao);
    glGenBuffers(1, &m_uiQuadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_uiQuadVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(kUnitQuadVertices)),
                 kUnitQuadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // --- Milestone 30: debug-line shader + streamed VBO ---
    GLuint debugVertexShader = 0;
    if (!CompileShader(GL_VERTEX_SHADER, kDebugVertexShaderSource, debugVertexShader)) return false;
    GLuint debugFragmentShader = 0;
    if (!CompileShader(GL_FRAGMENT_SHADER, kDebugFragmentShaderSource, debugFragmentShader)) {
        glDeleteShader(debugVertexShader);
        return false;
    }
    const bool debugLinked = LinkProgram(debugVertexShader, debugFragmentShader, m_debugShaderProgram);
    glDeleteShader(debugVertexShader);
    glDeleteShader(debugFragmentShader);
    if (!debugLinked) return false;
    m_debugUViewProjection = glGetUniformLocation(m_debugShaderProgram, "uViewProjection");
    glGenVertexArrays(1, &m_debugVao);
    glBindVertexArray(m_debugVao);
    glGenBuffers(1, &m_debugVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugVbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Milestone 15: until the first real BeginShadowPass call each frame
    // (every interactive-loop frame calls it three times — see
    // Application.cpp; TestHarness.cpp's own render path never does,
    // since shadows remain an interactive-loop-only concern, the same
    // scoping Milestone 13/14 already established for UI/dynamic
    // lighting), each shadow slot's light-space matrix must still map any
    // real-world point to an OUT-OF-RANGE shadow coordinate (z > 1), so
    // ComputeShadowFactor's own bounds check falls back to "fully lit"
    // rather than an identity matrix that could coincidentally land
    // in-range for points near the origin. This matrix maps EVERY input
    // point to the fixed point (0, 0, 2, 1) regardless of its own
    // position (every column affecting x/y/z is zero except a constant
    // z-translation of 2) — deliberately degenerate, only ever used as
    // this "definitely out of range" placeholder, never a real shadow
    // transform.
    glm::mat4 alwaysOutOfRangeMatrix(0.0f);
    alwaysOutOfRangeMatrix[3][2] = 2.0f;
    alwaysOutOfRangeMatrix[3][3] = 1.0f;
    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        m_shadowLightSpaceMatrix[slot] = alwaysOutOfRangeMatrix;
    }

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

    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        if (m_shadowFbo[slot]) {
            glDeleteFramebuffers(1, &m_shadowFbo[slot]);
            m_shadowFbo[slot] = 0;
        }
        if (m_shadowMapTexture[slot]) {
            glDeleteTextures(1, &m_shadowMapTexture[slot]);
            m_shadowMapTexture[slot] = 0;
        }
    }
    if (m_shadowShaderProgram) {
        glDeleteProgram(m_shadowShaderProgram);
        m_shadowShaderProgram = 0;
    }

    if (m_uiQuadVbo) {
        glDeleteBuffers(1, &m_uiQuadVbo);
        m_uiQuadVbo = 0;
    }
    if (m_uiQuadVao) {
        glDeleteVertexArrays(1, &m_uiQuadVao);
        m_uiQuadVao = 0;
    }
    if (m_uiShaderProgram) {
        glDeleteProgram(m_uiShaderProgram);
        m_uiShaderProgram = 0;
    }
    if (m_debugVbo) { glDeleteBuffers(1, &m_debugVbo); m_debugVbo = 0; }
    if (m_debugVao) { glDeleteVertexArrays(1, &m_debugVao); m_debugVao = 0; }
    if (m_debugShaderProgram) { glDeleteProgram(m_debugShaderProgram); m_debugShaderProgram = 0; }
    m_fontLoaded = false;
}

void Renderer::DrawDebugLines(const std::vector<DebugLine>& lines, bool depthTest) {
    if (lines.empty() || m_shadowPassActive || !m_debugShaderProgram) return;
    if (!depthTest) glDisable(GL_DEPTH_TEST);
    std::vector<float> vertices;
    vertices.reserve(lines.size() * 12);
    for (const DebugLine& line : lines) {
        for (const glm::vec3* p : {&line.a, &line.b}) {
            vertices.push_back(p->x); vertices.push_back(p->y); vertices.push_back(p->z);
            vertices.push_back(line.color.r); vertices.push_back(line.color.g); vertices.push_back(line.color.b);
        }
    }
    glUseProgram(m_debugShaderProgram);
    const glm::mat4 viewProjection = m_projection * m_view;
    glUniformMatrix4fv(m_debugUViewProjection, 1, GL_FALSE, glm::value_ptr(viewProjection));
    glBindVertexArray(m_debugVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(),
                 GL_STREAM_DRAW);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lines.size() * 2));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    if (!depthTest) glEnable(GL_DEPTH_TEST);
    ++m_stats.drawCalls;
    m_stats.debugLines += static_cast<unsigned int>(lines.size());
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

void Renderer::SetDynamicLights(const std::vector<DynamicLight>& lights) {
    glUseProgram(m_shaderProgram);

    const int count = std::min(static_cast<int>(lights.size()), kMaxDynamicLights);
    glUniform1i(m_uLightCount, count);
    m_stats.dynamicLights = static_cast<unsigned int>(count);

    for (int i = 0; i < count; ++i) {
        const DynamicLight& light = lights[static_cast<size_t>(i)];
        glUniform3f(m_uDynamicLightPosition[i], light.position.x, light.position.y, light.position.z);
        const glm::vec3 direction =
            glm::length(light.direction) > 1.0e-6f ? glm::normalize(light.direction) : glm::vec3(0.0f, 0.0f, -1.0f);
        glUniform3f(m_uDynamicLightDirection[i], direction.x, direction.y, direction.z);
        glUniform3f(m_uDynamicLightColor[i], light.color.r, light.color.g, light.color.b);
        glUniform1f(m_uDynamicLightRange[i], std::max(light.range, 1.0e-3f));
        // Cosines, not degrees: computed once here (CPU) rather than once
        // per fragment (GPU) — see the fragment shader's own
        // smoothstep(outerCos, innerCos, cosAngle) call. Clamped so a
        // misconfigured inner > outer doesn't silently invert the falloff
        // direction (smoothstep requires edge0 <= edge1).
        const float innerCos = std::cos(glm::radians(std::min(light.innerConeDegrees, light.outerConeDegrees)));
        const float outerCos = std::cos(glm::radians(light.outerConeDegrees));
        glUniform1f(m_uDynamicLightInnerCos[i], innerCos);
        glUniform1f(m_uDynamicLightOuterCos[i], outerCos);
        glUniform1i(m_uDynamicLightIsSpot[i], light.kind == LightKind::Spot ? 1 : 0);
        glUniform1i(m_uDynamicLightShadowIndex[i], light.shadowMapIndex);
    }
}

void Renderer::BeginShadowPass(int shadowSlot, const glm::mat4& lightViewProjection) {
    m_shadowLightSpaceMatrix[shadowSlot] = lightViewProjection;
    m_shadowPassActive = true;
    m_currentShadowSlot = shadowSlot;
    ++m_stats.shadowPasses;

    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo[shadowSlot]);
    glViewport(0, 0, kShadowMapResolution, kShadowMapResolution);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(m_shadowShaderProgram);
    glUniformMatrix4fv(m_uShadowLightViewProj, 1, GL_FALSE, glm::value_ptr(lightViewProjection));
}

void Renderer::EndShadowPass() {
    m_shadowPassActive = false;
    m_currentShadowSlot = -1;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

bool Renderer::UpdateMeshVertices(MeshHandle handle, const std::vector<MeshVertex>& vertices) {
    GpuMesh* mesh = GetMesh(handle);
    if (!mesh || mesh->ebo != 0) return false;
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(MeshVertex)),
                 vertices.empty() ? nullptr : vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    mesh->vertexCount = static_cast<GLsizei>(vertices.size());
    return true;
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
                         const glm::vec3& tintColor, float alpha) {
    GpuMesh* gpuMesh = GetMesh(mesh);
    if (!gpuMesh) return;

    const glm::mat4 model = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) *
                             glm::scale(glm::mat4(1.0f), scale);

    // Milestone 15: while a shadow pass is active (see BeginShadowPass),
    // every DrawMesh call writes depth only, from that light's own view/
    // projection, through the separate minimal shadow shader — normals,
    // UVs, textures, and every lighting uniform are irrelevant to a depth-
    // only pass, so none of them are touched here.
    const unsigned int triangles =
        static_cast<unsigned int>((gpuMesh->ebo ? gpuMesh->indexCount : gpuMesh->vertexCount) / 3);
    ++m_stats.drawCalls;
    m_stats.triangles += triangles;

    if (m_shadowPassActive) {
        glUseProgram(m_shadowShaderProgram);
        glUniformMatrix4fv(m_uShadowModel, 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(gpuMesh->vao);
        if (gpuMesh->ebo) {
            glDrawElements(GL_TRIANGLES, gpuMesh->indexCount, GL_UNSIGNED_INT, nullptr);
        } else {
            glDrawArrays(GL_TRIANGLES, 0, gpuMesh->vertexCount);
        }
        return;
    }

    // Standard correction for non-uniform scale — see the vertex shader's
    // own comment. glm::inverseTranspose is glm's dedicated helper for
    // exactly this (normal-matrix) computation.
    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(model));

    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(m_uNormalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    glUniformMatrix4fv(m_uView, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniform4f(m_uColor, tintColor.r, tintColor.g, tintColor.b, alpha);
    // Milestone 15: this frame's three shadow light-space matrices — see
    // BeginShadowPass's own comment for why every normal-mode draw simply
    // reuses whatever this frame's shadow passes most recently cached,
    // with no separate "apply shadow data" call needed from the caller.
    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        glUniformMatrix4fv(m_uLightSpaceMatrix[slot], 1, GL_FALSE,
                            glm::value_ptr(m_shadowLightSpaceMatrix[slot]));
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ResolveTexture(texture));
    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        glActiveTexture(GL_TEXTURE0 + 1 + slot);
        glBindTexture(GL_TEXTURE_2D, m_shadowMapTexture[slot]);
    }
    glActiveTexture(GL_TEXTURE0);  // restore the default active unit other calls (UI, texture creation) assume

    glBindVertexArray(gpuMesh->vao);
    if (gpuMesh->ebo) {
        glDrawElements(GL_TRIANGLES, gpuMesh->indexCount, GL_UNSIGNED_INT, nullptr);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, gpuMesh->vertexCount);
    }
}

void Renderer::DrawBox(const glm::vec3& position, const glm::quat& rotation,
                        const glm::vec3& halfExtents, const glm::vec3& colorRgb, float alpha) {
    DrawMesh(m_cubeMesh, position, rotation, halfExtents * 2.0f, TextureHandle{}, colorRgb, alpha);
}

void Renderer::BeginTransparentPass() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
}

void Renderer::EndTransparentPass() {
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::DrawSphere(const glm::vec3& position, float radius, const glm::vec3& colorRgb,
                          float alpha) {
    DrawMesh(m_sphereMesh, position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(radius),
             TextureHandle{}, colorRgb, alpha);
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

bool Renderer::LoadFont(const char* path, float pixelHeight, std::string& outError) {
    FontAtlasData atlasData;
    if (!LoadFontAtlas(path, pixelHeight, atlasData, outError)) {
        return false;
    }
    m_fontAtlasTexture = CreateTexture(atlasData.atlasTexture);
    for (int i = 0; i < kFontGlyphCount; ++i) {
        m_fontGlyphs[i] = atlasData.glyphs[i];
    }
    m_fontPixelHeight = atlasData.pixelHeight;
    m_fontAscent = atlasData.ascent;
    m_fontLineHeight = atlasData.lineHeight;
    m_fontLoaded = true;
    return true;
}

void Renderer::BeginUIFrame(int windowWidth, int windowHeight) {
    m_uiScreenSize = glm::vec2(static_cast<float>(std::max(windowWidth, 1)),
                                static_cast<float>(std::max(windowHeight, 1)));
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_uiShaderProgram);
    glUniform2f(m_uiUScreenSize, m_uiScreenSize.x, m_uiScreenSize.y);
    glBindVertexArray(m_uiQuadVao);
}

void Renderer::DrawUIRect(const glm::vec2& position, const glm::vec2& size,
                           const glm::vec4& colorRgba) {
    glUniform2f(m_uiUPosition, position.x, position.y);
    glUniform2f(m_uiUSize, size.x, size.y);
    glUniform4f(m_uiUColor, colorRgba.r, colorRgba.g, colorRgba.b, colorRgba.a);
    glUniform2f(m_uiUUVOffset, 0.0f, 0.0f);
    glUniform2f(m_uiUUVScale, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, ResolveTexture(m_whiteTexture));
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::DrawUIText(const std::string& text, const glm::vec2& position, float scale,
                           const glm::vec4& colorRgba) {
    if (!m_fontLoaded) return;

    glUniform4f(m_uiUColor, colorRgba.r, colorRgba.g, colorRgba.b, colorRgba.a);
    glBindTexture(GL_TEXTURE_2D, ResolveTexture(m_fontAtlasTexture));

    const float baselineY = position.y + m_fontAscent * scale;
    float penX = position.x;
    for (const char c : text) {
        const int index = static_cast<int>(c) - kFontFirstChar;
        if (index < 0 || index >= kFontGlyphCount) {
            // Unsupported/control character: advance by a rough space width
            // (this font's own space-glyph advance) rather than drawing
            // nothing at zero width, so e.g. a stray tab doesn't overlap
            // the next character. Sufficient for M13's plain ASCII HUD/menu
            // text — no Unicode/fallback-glyph support is being built here.
            penX += m_fontGlyphs[0].advanceX * scale;
            continue;
        }
        const FontGlyph& glyph = m_fontGlyphs[index];
        if (glyph.width > 0.0f && glyph.height > 0.0f) {
            const glm::vec2 glyphPosition(penX + glyph.offsetX * scale,
                                            baselineY + glyph.offsetY * scale);
            const glm::vec2 glyphSize(glyph.width * scale, glyph.height * scale);
            glUniform2f(m_uiUPosition, glyphPosition.x, glyphPosition.y);
            glUniform2f(m_uiUSize, glyphSize.x, glyphSize.y);
            glUniform2f(m_uiUUVOffset, glyph.u0, glyph.v0);
            glUniform2f(m_uiUUVScale, glyph.u1 - glyph.u0, glyph.v1 - glyph.v0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        penX += glyph.advanceX * scale;
    }
}

glm::vec2 Renderer::MeasureUIText(const std::string& text, float scale) const {
    if (!m_fontLoaded) return glm::vec2(0.0f);
    float width = 0.0f;
    for (const char c : text) {
        const int index = static_cast<int>(c) - kFontFirstChar;
        width += (index >= 0 && index < kFontGlyphCount ? m_fontGlyphs[index].advanceX
                                                          : m_fontGlyphs[0].advanceX) *
                 scale;
    }
    return glm::vec2(width, m_fontLineHeight * scale);
}

float Renderer::GetUITextLineHeight(float scale) const {
    return m_fontLoaded ? m_fontLineHeight * scale : 0.0f;
}

void Renderer::EndUIFrame() {
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
