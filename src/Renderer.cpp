#include "render/RendererInfrastructure.h"
#include "PerformanceProfiler.h"
#include "CookedTexture.h"
#include "Tangents.h"
#include "Renderer.h"
#include "render/ShaderLibrary.h"
#include "SkeletalAnimation.h"
#include "Weather.h"
#include "BasicWater.h"
#include "particles/GpuParticles.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <limits>

#include <SDL2/SDL.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
using namespace JudasRender::Shaders;

// A unit cube (36 vertices, non-indexed, two triangles per face) centered on
// the origin in local [-0.5, 0.5] space — the same geometry Judas has used
// since Milestone 2, now carrying a real per-face normal (flat-shaded, no
// vertex-normal averaging across faces — an ordinary box's edges are
// genuinely sharp) and geometrically consistent planar UVs per face.
// Winding remains unchanged; face culling is not enabled.
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
    MeshData mesh;
    mesh.vertices.reserve(36);
    for (int face = 0; face < 6; ++face) {
        for (int v = 0; v < 6; ++v) {
            MeshVertex vertex;
            vertex.position = kPositions[face * 6 + v].position;
            vertex.normal = kFaceNormals[face];
            const glm::vec3 p = vertex.position;
            // Same position gets the same UV on both triangles of a face.
            // U points right when the face is viewed from outside; V is up.
            switch (face) {
                case 0: vertex.uv = {0.5f - p.x, p.y + 0.5f}; break;
                case 1: vertex.uv = {p.x + 0.5f, p.y + 0.5f}; break;
                case 2: vertex.uv = {p.z + 0.5f, p.y + 0.5f}; break;
                case 3: vertex.uv = {0.5f - p.z, p.y + 0.5f}; break;
                case 4: vertex.uv = {p.x + 0.5f, p.z + 0.5f}; break;
                default: vertex.uv = {p.x + 0.5f, 0.5f - p.z}; break;
            }
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
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
constexpr int kShadowMapResolution = 512;
#else
constexpr int kShadowMapResolution = 1024;
#endif

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


// Milestone 30: the debug-line shader — world-space position + colour per
// vertex, one view-projection uniform, no lighting, no texture. Kept apart
// from the lit mesh shader for the same reason the UI shader is: it is a
// genuinely different pass (GL_LINES, per-vertex colour, streamed data).


}  // namespace

Renderer::Renderer():m_render(std::make_unique<JudasRender::RendererInfrastructure>()){}
Renderer::~Renderer()=default;

bool Renderer::CompileShader(GLenum type, const char* source, GLuint& result) const {
    const auto profile = m_capabilities.modernTier ? JudasRender::Shaders::Profile::Desktop450
                                                   : JudasRender::Shaders::PortableProfile();
    std::string error;
    if(source==kVertexShaderSource || source==kFragmentShaderSource) {
        auto transformed=JudasRender::MakeViewBlockSource(source,error);
        if(transformed.empty()){std::fprintf(stderr,"View shader: %s\n",error.c_str());return false;}
        return JudasRender::Shaders::CompileShader(type,transformed.c_str(),result,profile,"mesh-view-block");
    }
    return JudasRender::Shaders::CompileShader(type, source, result, profile);
}

bool Renderer::Init() {
    m_capabilities = QueryGraphicsCapabilities();
    m_render->persistentUploads=true;
    if(const char* setting=std::getenv("JUDAS_PERSISTENT_UPLOADS"))m_render->persistentUploads=std::strcmp(setting,"0")!=0;
    if(const char* batching=std::getenv("JUDAS_RENDER_BATCHING"))m_batchingEnabled=std::strcmp(batching,"0")!=0;
    const char* shadows=std::getenv("JUDAS_SHADOWS");m_shadowsEnabled=!(shadows&&std::string(shadows)=="0");
    std::fprintf(stderr,"Shadow maps: %s\n",m_shadowsEnabled?"on":"off");
    std::fprintf(stderr,"Graphics: %s / %s\n",glGetString(GL_VERSION),glGetString(GL_RENDERER));
    GLint units=0;glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&units);m_textureUnitLimit=unsigned(units);std::fprintf(stderr,"Renderer fragment texture units: %d (material path uses 12)\n",units);if(units<12){std::fprintf(stderr,"M57 requires 12 fragment texture units, available %d\n",units);return false;}
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
    m_uSkinned=glGetUniformLocation(m_shaderProgram,"uSkinned");m_uBones=glGetUniformLocation(m_shaderProgram,"uBones");
    m_uNormalMatrix = glGetUniformLocation(m_shaderProgram, "uNormalMatrix");
    m_uView = glGetUniformLocation(m_shaderProgram, "uView");
    m_uProjection = glGetUniformLocation(m_shaderProgram, "uProjection");
    m_uColor = glGetUniformLocation(m_shaderProgram, "uColor");
    m_uTexture = glGetUniformLocation(m_shaderProgram, "uTexture");
    m_uLightDirection = glGetUniformLocation(m_shaderProgram, "uLightDirection");
    m_uLightColor = glGetUniformLocation(m_shaderProgram, "uLightColor");
    m_uAmbientColor = glGetUniformLocation(m_shaderProgram, "uAmbientColor");

    m_uInstanced=glGetUniformLocation(m_shaderProgram,"uInstanced");
    for(int slot=0;slot<kShadowMapCount;++slot)m_uLightSpaceMatrix[slot]=glGetUniformLocation(m_shaderProgram,("uLightSpaceMatrix["+std::to_string(slot)+"]").c_str());
    // Unit 1: depth array; 14: tiled lights. Material, weather and pose units
    // retain their prior assignments, within the GLES3 sixteen-unit minimum.
    glUseProgram(m_shaderProgram);
    glUniform1i(m_uTexture, 0);
    // Sampler types must use distinct units even when uSkinned is false.
    // Initialize this before the first ordinary draw; a project need not
    // render a skeleton first to make its boxes and spheres valid GL draws.
#ifdef __EMSCRIPTEN__
    // WebGL validates the type of active integer attributes even when the
    // skinning branch is disabled. Unskinned VAOs use these generic constants;
    // the GL default is a float and makes their otherwise valid draws fail.
    glVertexAttribI4ui(3, 0, 0, 0, 0);
    glVertexAttribI4ui(6, 0, 0, 0, 0);
#endif
    glUniform1i(m_uBones, 15);
    glUniform1i(glGetUniformLocation(m_shaderProgram,"uWeatherNoise"),13);
    glUniform1i(glGetUniformLocation(m_shaderProgram,"uShadowMaps"),1);
    glUniform1i(glGetUniformLocation(m_shaderProgram,"uLightGrid"),14);

    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    auto cube=BuildCubeMeshData();GenerateMeshTangents(cube);m_cubeMesh=CreateMesh(cube);
    auto sphere=GenerateUnitSphereMeshData(kSphereLatitudeSegments,kSphereLongitudeSegments);GenerateMeshTangents(sphere);m_sphereMesh=CreateMesh(sphere);

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
    m_uShadowInstanced=glGetUniformLocation(m_shadowShaderProgram,"uInstanced");
    m_uShadowSkinned=glGetUniformLocation(m_shadowShaderProgram,"uSkinned");m_uShadowBones=glGetUniformLocation(m_shadowShaderProgram,"uBones");
    m_uShadowLightViewProj = glGetUniformLocation(m_shadowShaderProgram, "uLightViewProj");
    glUseProgram(m_shadowShaderProgram);
    glUniform1i(m_uShadowBones, 15);

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
    ReserveShadowLayers(1);
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

    m_uiUTextVertices=glGetUniformLocation(m_uiShaderProgram,"uTextVertices");
    m_uiUScreenSize = glGetUniformLocation(m_uiShaderProgram, "uScreenSize");
    m_uiUPosition = glGetUniformLocation(m_uiShaderProgram, "uPosition");
    m_uiUSize = glGetUniformLocation(m_uiShaderProgram, "uSize");
    m_uiUColor = glGetUniformLocation(m_uiShaderProgram, "uColor");
    m_uiUTexture = glGetUniformLocation(m_uiShaderProgram, "uTexture");
    m_uiUSceneLinear = glGetUniformLocation(m_uiShaderProgram, "uSceneLinear");
    m_uiUExposure = glGetUniformLocation(m_uiShaderProgram, "uExposure");
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

    glGenVertexArrays(1,&m_textVao);glBindVertexArray(m_textVao);
    glGenBuffers(1,&m_textVbo);glBindBuffer(GL_ARRAY_BUFFER,m_textVbo);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),nullptr);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),reinterpret_cast<void*>(2*sizeof(float)));glEnableVertexAttribArray(1);
    glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);

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
        m_render->viewUniforms.SetShadow(unsigned(slot),alwaysOutOfRangeMatrix);
    }

    m_render->viewUniforms.SetCamera(m_view,m_projection);
    return true;
}

void Renderer::TraceResourceOperation(ResourceTracePoint point, unsigned int handle, std::size_t bytes) const {
    if (!m_resourceTrace) return;
    ResourceTraceEvent event{point, {}, {}};
    event.handle = handle;
    event.bytes = bytes;
    event.contextCurrent = GraphicsContextCurrent();
    m_resourceTrace(event);
}

void Renderer::UploadPosePalette(const std::vector<glm::mat4>& pose) {
    JUDAS_PROFILE_SCOPE("Pose palette upload");
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    if(!m_poseTexture)glGenTextures(1,&m_poseTexture);
    glActiveTexture(GL_TEXTURE15);glBindTexture(GL_TEXTURE_2D,m_poseTexture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,4,GLsizei(pose.size()),0,GL_RGBA,GL_FLOAT,pose.data());
    glActiveTexture(GL_TEXTURE0);
#else
    if(!m_poseBuffer)glGenBuffers(1,&m_poseBuffer);
    if(!m_poseTexture)glGenTextures(1,&m_poseTexture);
    glBindBuffer(GL_TEXTURE_BUFFER,m_poseBuffer);
    glBufferData(GL_TEXTURE_BUFFER,GLsizeiptr(pose.size()*sizeof(glm::mat4)),pose.data(),GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE15);glBindTexture(GL_TEXTURE_BUFFER,m_poseTexture);
    glTexBuffer(GL_TEXTURE_BUFFER,GL_RGBA32F,m_poseBuffer);glActiveTexture(GL_TEXTURE0);
#endif
}
void Renderer::Shutdown() {
    m_render->submission.Clear();if(m_instanceBuffer)glDeleteBuffers(1,&m_instanceBuffer);m_instanceBuffer=0;
    if(m_lightTexture)glDeleteTextures(1,&m_lightTexture);
    m_lightTexture=0;m_lightTextureRows=0;
    if(m_shadowArray)glDeleteTextures(1,&m_shadowArray);
    m_shadowArray=0;m_shadowLayers=0;
    ShutdownWeather();
    if(m_poseTexture){glDeleteTextures(1,&m_poseTexture);m_poseTexture=0;}
    if(m_poseBuffer){glDeleteBuffers(1,&m_poseBuffer);m_poseBuffer=0;}
    ResetProjectText();
    for(auto& q:m_profileQueries){if(q.begin)glDeleteQueries(1,&q.begin);if(q.end)glDeleteQueries(1,&q.end);q={};}
    m_profileQueriesReady=false;
    PerformanceProfiler::Get().GPUAvailability(false,"Renderer/context shut down");
    TraceResourceOperation(ResourceTracePoint::RendererShutdownBegin);
    EndRenderTarget();
    ShutdownAppearance();
    m_spriteDraws.clear();m_spriteQuad={};m_spriteMaterials={};m_spritePixelGrid=0;
    m_render->particlePass.Shutdown();
    m_render->gpuParticlePass.Shutdown();
    m_render->waterPass.Shutdown();
    m_terrainGrids.clear();m_terrainGrid=false;
    m_render->instanceArena.Shutdown();
    m_render->viewUniforms.Shutdown();
    m_shadowContentValid.fill(false);
    for (unsigned int i = 0; i < m_targets.size(); ++i) DestroyRenderTarget(RenderTargetHandle{i});
    m_targets.clear();
    for (std::size_t i = 0; i < m_meshes.size(); ++i) {
        DestroyMesh(MeshHandle{static_cast<unsigned int>(i),m_meshes[i].generation});
    }
    m_meshes.clear();m_freeMeshes.clear();

    for (std::size_t i = 0; i < m_textures.size(); ++i) {
        DestroyTexture(TextureHandle{static_cast<unsigned int>(i),m_textures[i].generation});
    }
    m_textures.clear();m_freeTextures.clear();

    if (m_shaderProgram) {
        if(m_waterPathTexture){glDeleteTextures(1,&m_waterPathTexture);m_waterPathTexture=0;m_waterColumns=m_waterRows=0;}
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }

    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        if (m_shadowFbo[slot]) {
            glDeleteFramebuffers(1, &m_shadowFbo[slot]);
            m_shadowFbo[slot] = 0;
        }
    }
    if (m_shadowShaderProgram) {
        glDeleteProgram(m_shadowShaderProgram);
        m_shadowShaderProgram = 0;
    }

    if(m_textVbo){glDeleteBuffers(1,&m_textVbo);m_textVbo=0;}
    if(m_textVao){glDeleteVertexArrays(1,&m_textVao);m_textVao=0;}
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
    m_transientSurface={};m_basicWaterMesh={};
    m_fontLoaded=false;m_uiFonts.clear();m_defaultUIFont.clear();m_defaultTextFont.reset();m_textFonts.clear();
    TraceResourceOperation(ResourceTracePoint::RendererShutdownEnd);
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
    PollProfileGPU();
    glViewport(0, 0, windowWidth, windowHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if(m_linearRendering)BeginLinearPass(windowWidth,windowHeight);
}

void Renderer::SetCamera(const glm::mat4& view, const glm::mat4& projection) {
    if(!m_render->viewUniforms.SetCamera(view,projection)){
        std::fprintf(stderr,"Camera rejected: view must be invertible and matrices finite\n");
        return;
    }
    m_spritePixelGrid=0;
    m_view = view;
    m_projection = projection;
    m_frustum=Frustum(projection*view);
    if(m_shaderProgram){glUseProgram(m_shaderProgram);glUniform1i(glGetUniformLocation(m_shaderProgram,"uWaterEnabled"),0);}
}

void Renderer::SetWaterPaths(unsigned columns,unsigned rows,const std::vector<glm::vec2>& paths){
 JUDAS_PROFILE_SCOPE("Water boundary upload");
    if(!m_shaderProgram)return;
    if(paths.empty()){glUseProgram(m_shaderProgram);glUniform1i(glGetUniformLocation(m_shaderProgram,"uWaterEnabled"),0);return;}
    if(paths.size()!=size_t(columns)*rows||!columns||!rows)return;
    if(!m_waterPathTexture){glGenTextures(1,&m_waterPathTexture);}
    glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,m_waterPathTexture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    if(columns!=m_waterColumns||rows!=m_waterRows){glTexImage2D(GL_TEXTURE_2D,0,GL_RG32F,columns,rows,0,GL_RG,GL_FLOAT,paths.data());m_waterColumns=columns;m_waterRows=rows;}
    else glTexSubImage2D(GL_TEXTURE_2D,0,0,0,columns,rows,GL_RG,GL_FLOAT,paths.data());
    glUseProgram(m_shaderProgram);glUniform1i(glGetUniformLocation(m_shaderProgram,"uWaterEnabled"),1);
    glUniform1i(glGetUniformLocation(m_shaderProgram,"uWaterPaths"),4);
    glm::mat4 matrix=m_projection*m_view,inverse=glm::inverse(matrix);
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram,"uWaterViewProjection"),1,GL_FALSE,glm::value_ptr(matrix));
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram,"uWaterInverseViewProjection"),1,GL_FALSE,glm::value_ptr(inverse));
    glActiveTexture(GL_TEXTURE0);
}

void Renderer::SetLighting(const glm::vec3& direction, const glm::vec3& lightColor,
                            const glm::vec3& ambientColor) {
    m_weatherSun=glm::length(direction)>1.e-6f?glm::normalize(direction):glm::vec3(0,1,0);
    m_weatherSunColour=lightColor;
    m_weatherAmbient=ambientColor;
    glUseProgram(m_shaderProgram);
    const glm::vec3 normalizedDirection =
        glm::length(direction) > 1.0e-6f ? glm::normalize(direction) : glm::vec3(0.0f, 1.0f, 0.0f);
    glUniform3f(m_uLightDirection, normalizedDirection.x, normalizedDirection.y,
                normalizedDirection.z);
    glUniform3f(m_uLightColor, lightColor.r, lightColor.g, lightColor.b);
    glUniform3f(m_uAmbientColor, ambientColor.r, ambientColor.g, ambientColor.b);
}

#include "RendererScale.inl"

void Renderer::BeginShadowPass(int shadowSlot, const glm::mat4& lightViewProjection) {
    if(shadowSlot<0||shadowSlot>=kShadowMapCount||!m_shadowsEnabled||m_shadowPassActive)return;
    if(!m_render->viewUniforms.SetShadow(unsigned(shadowSlot),lightViewProjection)){
        std::fprintf(stderr,"Shadow chart rejected: matrix must be finite\n");return;
    }
    ReserveShadowLayers(unsigned(shadowSlot+1));
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&m_shadowSavedFramebuffer);glGetIntegerv(GL_VIEWPORT,&m_shadowSavedViewport.x);
    m_profileShadow=BeginProfilePass("Shadow",std::uint64_t(shadowSlot));
    m_shadowLightSpaceMatrix[shadowSlot] = lightViewProjection;
    m_shadowPassActive = true;
    m_currentShadowSlot = shadowSlot;
    m_frustum=Frustum(lightViewProjection);
    ++m_stats.shadowPasses;

    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo[shadowSlot]);
    glViewport(0, 0, kShadowMapResolution, kShadowMapResolution);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(m_shadowShaderProgram);
    glUniformMatrix4fv(m_uShadowLightViewProj, 1, GL_FALSE, glm::value_ptr(lightViewProjection));
}

void Renderer::EndShadowPass() {
    if(!m_shadowPassActive)return;
    if(m_shadowPassActive && m_currentShadowSlot>=0)m_shadowContentValid[unsigned(m_currentShadowSlot)]=true;
    EndProfilePass(m_profileShadow);m_profileShadow=0;
    m_shadowPassActive = false;
    m_frustum=Frustum(m_projection*m_view);
    m_currentShadowSlot = -1;
    glBindFramebuffer(GL_FRAMEBUFFER,GLuint(m_shadowSavedFramebuffer));glViewport(m_shadowSavedViewport.x,m_shadowSavedViewport.y,m_shadowSavedViewport.z,m_shadowSavedViewport.w);
}

void Renderer::InvalidateShadowSlot(int shadowSlot) {
    if(shadowSlot<0||shadowSlot>=kShadowMapCount)return;
    glm::mat4 outside(0.0f);outside[3][2]=2.0f;outside[3][3]=1.0f;
    m_shadowLightSpaceMatrix[shadowSlot]=outside;
    m_render->viewUniforms.SetShadow(unsigned(shadowSlot),outside);
}

MeshHandle Renderer::CreateMesh(const MeshData& data) {
    auto handle=BeginMeshUpload(data);
    if(!handle.IsValid())return {};
    for(size_t i=0;i<data.materials.size();++i)for(size_t j=0;j<5;++j)
        if(!UploadMeshMap(handle,data,i,j)){DestroyMesh(handle);return {};}
    return handle;
}

MeshHandle Renderer::BeginMeshUpload(const MeshData& data) {
    JUDAS_PROFILE_SCOPE("Mesh geometry upload");
    if (data.skeletal) {
        const auto count=data.skeletal->skeleton.skinNodes.size();
        GLint texels=0;
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,&texels);texels*=4;
#else
        glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,&texels);
#endif
        if(count==0||count>size_t(texels/4)||count>kModelPaletteLimit||data.skinVertices.size()!=data.vertices.size())return {};
        for(const auto& v:data.skinVertices){float sum=0;
            for(int i=0;i<4;++i){if(v.joints[i]>=count||!std::isfinite(v.weights[i])||v.weights[i]<0)return {};sum+=v.weights[i];if(v.joints1[i]>=count||!std::isfinite(v.weights1[i])||v.weights1[i]<0)return {};sum+=v.weights1[i];}
            if(std::abs(sum-1.f)>1e-4f)return {};
        }
    }else if(!data.skinVertices.empty())return {};
    GpuMesh mesh;
    mesh.lods=data.lods;mesh.lodPixels=data.lodPixels;
    mesh.primitives=data.primitives;for(const auto& material:data.materials)mesh.materials.push_back(CreateMaterial(MaterialSettings(material)));
    mesh.alive = true;
    if(data.skeletal){mesh.restSkin=ResolveSkinMatrices(data.skeletal->skeleton,data.skeletal->skeleton.rest);for(auto& part:data.primitives)if(part.count)mesh.partOrientation.push_back(data.skinVertices.at(data.indices.empty()?part.first:data.indices.at(part.first)));else mesh.partOrientation.emplace_back();}
    for(const auto& v:data.vertices)mesh.bounds.Include(v.position);
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
    glVertexAttribPointer(8,2,GL_FLOAT,GL_FALSE,sizeof(MeshVertex),reinterpret_cast<void*>(offsetof(MeshVertex,uv1)));glEnableVertexAttribArray(8);
    glVertexAttribPointer(5,4,GL_FLOAT,GL_FALSE,sizeof(MeshVertex),reinterpret_cast<void*>(offsetof(MeshVertex,tangent)));glEnableVertexAttribArray(5);
    if(!data.skinVertices.empty()){
        glGenBuffers(1,&mesh.skinVbo);glBindBuffer(GL_ARRAY_BUFFER,mesh.skinVbo);
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(data.skinVertices.size()*sizeof(MeshSkinVertex)),data.skinVertices.data(),GL_STATIC_DRAW);
        glVertexAttribIPointer(3,4,GL_UNSIGNED_INT,sizeof(MeshSkinVertex),reinterpret_cast<void*>(offsetof(MeshSkinVertex,joints)));glEnableVertexAttribArray(3);
        glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(MeshSkinVertex),reinterpret_cast<void*>(offsetof(MeshSkinVertex,weights)));glEnableVertexAttribArray(4);
        glVertexAttribIPointer(6,4,GL_UNSIGNED_INT,sizeof(MeshSkinVertex),reinterpret_cast<void*>(offsetof(MeshSkinVertex,joints1)));glEnableVertexAttribArray(6);
        glVertexAttribPointer(7,4,GL_FLOAT,GL_FALSE,sizeof(MeshSkinVertex),reinterpret_cast<void*>(offsetof(MeshSkinVertex,weights1)));glEnableVertexAttribArray(7);
    }

    if (!data.indices.empty()) {
        auto indices=data.indices;indices.insert(indices.end(),data.lodIndices.begin(),data.lodIndices.end());
        mesh.indexCount = static_cast<GLsizei>(indices.size());
        glGenBuffers(1, &mesh.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                     indices.data(), GL_STATIC_DRAW);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // Deliberately NOT unbinding GL_ELEMENT_ARRAY_BUFFER here — it is part
    // of the VAO's own state, and unbinding it while this VAO is still
    // bound would detach it from the VAO itself.

    MeshHandle handle;
    if(m_freeMeshes.empty()){handle.id=unsigned(m_meshes.size());m_meshes.push_back(std::move(mesh));}
    else{handle.id=m_freeMeshes.back();m_freeMeshes.pop_back();mesh.generation=m_meshes[handle.id].generation;handle.generation=mesh.generation;m_meshes[handle.id]=std::move(mesh);}
    TraceResourceOperation(ResourceTracePoint::MeshCreated, handle.id,
                           data.vertices.size() * sizeof(MeshVertex) + (data.indices.size()+data.lodIndices.size()) * sizeof(std::uint32_t) + data.skinVertices.size()*sizeof(MeshSkinVertex));
    return handle;
}

bool Renderer::UpdateMeshVertices(MeshHandle handle, const std::vector<MeshVertex>& vertices) {
    GpuMesh* mesh = GetMesh(handle);
    // Indexed deformables keep immutable topology; only their vertex stream
    // changes. Existing non-indexed transient surfaces may still resize.
    if (!mesh || (mesh->ebo != 0 && vertices.size()!=size_t(mesh->vertexCount))) return false;
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(MeshVertex)),
                 vertices.empty() ? nullptr : vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    mesh->vertexCount = static_cast<GLsizei>(vertices.size());
    if(!mesh->lods.empty())mesh->lodPixels=0; // changed geometry invalidates offline error estimates
    mesh->bounds={};for(const auto& v:vertices)mesh->bounds.Include(v.position);
    return true;
}

bool Renderer::UpdateMeshSkinTopology(MeshHandle handle,const std::vector<uint32_t>& indices,
                                     const std::vector<MeshSkinVertex>& skin,const std::vector<MeshPrimitive>& primitives){
    auto* mesh=GetMesh(handle);
    if(!mesh||!mesh->skinVbo||!mesh->ebo||!mesh->lods.empty()||indices.size()%3||indices.size()>size_t(std::numeric_limits<GLsizei>::max()))return false;
    if(!indices.empty()&&skin.size()!=size_t(mesh->vertexCount))return false;
    for(auto i:indices)if(i>=unsigned(mesh->vertexCount))return false;
    for(const auto& p:primitives)if(p.first%3||p.count%3||p.first>indices.size()||p.count>indices.size()-p.first||p.material>=int(mesh->materials.size())||p.material< -1)return false;
    for(const auto& v:skin){float sum=0;
        for(unsigned k=0;k<4;++k){if(v.joints[k]>=mesh->restSkin.size()||v.joints1[k]>=mesh->restSkin.size()||!std::isfinite(v.weights[k])||!std::isfinite(v.weights1[k])||v.weights[k]<0||v.weights1[k]<0)return false;sum+=v.weights[k]+v.weights1[k];}
        if(std::abs(sum-1)>1e-4f)return false;
    }
    JUDAS_PROFILE_SCOPE("Structural topology upload");
    GLint previousVao=0,previousArray=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&previousVao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previousArray);
    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,GLsizeiptr(indices.size()*sizeof(uint32_t)),indices.empty()?nullptr:indices.data(),GL_DYNAMIC_DRAW);
    if(!skin.empty()){glBindBuffer(GL_ARRAY_BUFFER,mesh->skinVbo);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(skin.size()*sizeof(MeshSkinVertex)),skin.data(),GL_DYNAMIC_DRAW);}
    glBindVertexArray(GLuint(previousVao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(previousArray));
    mesh->indexCount=GLsizei(indices.size());mesh->primitives=primitives;mesh->partOrientation.clear();
    for(const auto& p:primitives)mesh->partOrientation.push_back(p.count?skin[indices[p.first]]:MeshSkinVertex{});
    return true;
}

Renderer::GpuMesh* Renderer::GetMesh(MeshHandle handle) {
    if (!handle.IsValid() || handle.id >= m_meshes.size() || !m_meshes[handle.id].alive || m_meshes[handle.id].generation!=handle.generation) {
        return nullptr;
    }
    return &m_meshes[handle.id];
}

void Renderer::DestroyMesh(MeshHandle handle) {
    GpuMesh* mesh = GetMesh(handle);
    if (!mesh) return;
    m_render->waterPass.Forget((std::uint64_t(handle.generation)<<32)|handle.id);
    if (mesh->ebo) glDeleteBuffers(1, &mesh->ebo);
    glDeleteBuffers(1, &mesh->vbo);
    for(auto material:mesh->materials)DestroyMaterial(material);
    if(mesh->skinVbo)glDeleteBuffers(1,&mesh->skinVbo);
    glDeleteVertexArrays(1, &mesh->vao);
    const std::size_t bytes = static_cast<std::size_t>(mesh->vertexCount) * sizeof(MeshVertex) +
                              static_cast<std::size_t>(mesh->indexCount) * sizeof(std::uint32_t);
    auto generation=mesh->generation;*mesh=GpuMesh{};mesh->generation=generation;
    if(generation!=0xFFFFFFFFu){++mesh->generation;m_freeMeshes.push_back(handle.id);}
    TraceResourceOperation(ResourceTracePoint::MeshDestroyed, handle.id, bytes);
}

TextureHandle Renderer::CreateTexture(const TextureData& data,bool srgb) {
    GpuTexture texture;texture.width=data.width;texture.height=data.height;texture.srgb=srgb;
    texture.alive = true;
    texture.storage=data.storage;texture.mipCount=data.levels.empty()?1:unsigned(data.levels.size());
    glGenTextures(1, &texture.textureId);
    glBindTexture(GL_TEXTURE_2D, texture.textureId);
    if(data.levels.empty()){
        glTexImage2D(GL_TEXTURE_2D,0,srgb?GL_SRGB8_ALPHA8:GL_RGBA8,data.width,data.height,0,GL_RGBA,GL_UNSIGNED_BYTE,data.pixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);for(int w=data.width,h=data.height;;w=std::max(1,w/2),h=std::max(1,h/2)){texture.uploadedBytes+=size_t(w)*h*4;if(w==1&&h==1)break;}
    }else{
        GLenum format=data.storage==TextureStorage::BC1?(srgb?0x8C4C:0x83F0):(srgb?0x8C4F:0x83F3);
        GLint count=0;glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS,&count);std::vector<GLint> formats(static_cast<size_t>(std::max(0,count)));if(count)glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS,formats.data());
        bool compressed=m_allowBlockCompression&&data.storage!=TextureStorage::Rgba8&&std::find(formats.begin(),formats.end(),GLint(format))!=formats.end();
        texture.storage=compressed?data.storage:TextureStorage::Rgba8;
        for(size_t i=0;i<data.levels.size();++i){const auto& source=data.levels[i];if(compressed){glCompressedTexImage2D(GL_TEXTURE_2D,GLint(i),format,source.width,source.height,0,GLsizei(source.bytes.size()),source.bytes.data());texture.uploadedBytes+=source.bytes.size();}else{auto level=ExpandTextureMip(source,data.storage);glTexImage2D(GL_TEXTURE_2D,GLint(i),srgb?GL_SRGB8_ALPHA8:GL_RGBA8,level.width,level.height,0,GL_RGBA,GL_UNSIGNED_BYTE,level.bytes.data());texture.uploadedBytes+=level.bytes.size();}}
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,GLint(data.levels.size()-1));
    }

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
    handle=InstallTexture(std::move(texture));
    TraceResourceOperation(ResourceTracePoint::TextureCreated, handle.id, texture.uploadedBytes);
    return handle;
}

void Renderer::DestroyTexture(TextureHandle handle) {
    if (!ValidTexture(handle)) {
        return;
    }
    auto colour=m_textures[handle.id].colourView;if(colour.IsValid())DestroyTexture(colour);
    glDeleteTextures(1, &m_textures[handle.id].textureId);
    const std::size_t bytes = m_textures[handle.id].uploadedBytes;
    auto generation=m_textures[handle.id].generation;m_textures[handle.id]=GpuTexture{};m_textures[handle.id].generation=generation;
    if(generation!=0xFFFFFFFFu){++m_textures[handle.id].generation;m_freeTextures.push_back(handle.id);}
    TraceResourceOperation(ResourceTracePoint::TextureDestroyed, handle.id, bytes);
}

bool Renderer::ReadMeshForDiagnostics(MeshHandle handle, MeshData& outData) const {
    if (!GraphicsContextCurrent() || !handle.IsValid() || handle.id >= m_meshes.size()) return false;
    const GpuMesh& mesh = m_meshes[handle.id];
    if (!mesh.alive || mesh.generation!=handle.generation || !glIsBuffer(mesh.vbo) || (mesh.ebo && !glIsBuffer(mesh.ebo))) return false;
    GLint previousBuffer = 0;
    glGetIntegerv(GL_COPY_READ_BUFFER, &previousBuffer);
    MeshData data;
    glBindBuffer(GL_COPY_READ_BUFFER, mesh.vbo);
    GLint64 vertexBytes = 0;
    glGetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &vertexBytes);
    if (vertexBytes < 0 || vertexBytes % sizeof(MeshVertex) != 0) {
        glBindBuffer(GL_COPY_READ_BUFFER, static_cast<GLuint>(previousBuffer));
        return false;
    }
    data.vertices.resize(static_cast<std::size_t>(vertexBytes) / sizeof(MeshVertex));
    if (vertexBytes) {
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
        const void* mapped=glMapBufferRange(GL_COPY_READ_BUFFER,0,GLsizeiptr(vertexBytes),GL_MAP_READ_BIT);
        if(!mapped){glBindBuffer(GL_COPY_READ_BUFFER,GLuint(previousBuffer));return false;}
        std::memcpy(data.vertices.data(),mapped,std::size_t(vertexBytes));glUnmapBuffer(GL_COPY_READ_BUFFER);
#else
        glGetBufferSubData(GL_COPY_READ_BUFFER,0,GLsizeiptr(vertexBytes),data.vertices.data());
#endif
    }
    if (mesh.ebo) {
        glBindBuffer(GL_COPY_READ_BUFFER, mesh.ebo);
        GLint64 indexBytes = 0;
        glGetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &indexBytes);
        if (indexBytes < 0 || indexBytes % sizeof(std::uint32_t) != 0) {
            glBindBuffer(GL_COPY_READ_BUFFER, static_cast<GLuint>(previousBuffer));
            return false;
        }
        data.indices.resize(static_cast<std::size_t>(indexBytes) / sizeof(std::uint32_t));
        if (indexBytes) {
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
            const void* mapped=glMapBufferRange(GL_COPY_READ_BUFFER,0,GLsizeiptr(indexBytes),GL_MAP_READ_BIT);
            if(!mapped){glBindBuffer(GL_COPY_READ_BUFFER,GLuint(previousBuffer));return false;}
            std::memcpy(data.indices.data(),mapped,std::size_t(indexBytes));glUnmapBuffer(GL_COPY_READ_BUFFER);
#else
            glGetBufferSubData(GL_COPY_READ_BUFFER,0,GLsizeiptr(indexBytes),data.indices.data());
#endif
        }
    }
    glBindBuffer(GL_COPY_READ_BUFFER, static_cast<GLuint>(previousBuffer));
    data.lods=mesh.lods;data.lodPixels=mesh.lodPixels;data.primitives=mesh.primitives;if(!data.lods.empty()){data.lodIndices.assign(data.indices.begin()+data.lods[0].count,data.indices.end());data.indices.resize(data.lods[0].count);}
    outData = std::move(data);
    return true;
}

bool Renderer::ReadTextureForDiagnostics(TextureHandle handle, TextureData& outData) const {
    if (!GraphicsContextCurrent() || !handle.IsValid() || handle.id >= m_textures.size()) return false;
    const GpuTexture& texture = m_textures[handle.id];
    if (!texture.alive || texture.generation!=handle.generation || !glIsTexture(texture.textureId)) return false;
    GLint previousTexture = 0, previousPackBuffer = 0;
    GLint alignment = 0, rowLength = 0, skipRows = 0, skipPixels = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previousPackBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &rowLength);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &skipRows);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &skipPixels);
    glBindTexture(GL_TEXTURE_2D, texture.textureId);
    TextureData data;
    data.width=texture.width;data.height=texture.height;
    if (data.width <= 0 || data.height <= 0) {
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
        return false;
    }
    data.pixels.resize(static_cast<std::size_t>(data.width) * static_cast<std::size_t>(data.height) * TextureData::kChannels);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    GLint previousRead=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&previousRead);
    GLuint fbo=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture.textureId,0);
    const bool complete=glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    if(complete)glReadPixels(0,0,data.width,data.height,GL_RGBA,GL_UNSIGNED_BYTE,data.pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(previousRead));glDeleteFramebuffers(1,&fbo);
#else
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,data.pixels.data());
#endif
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    glPixelStorei(GL_PACK_ROW_LENGTH, rowLength);
    glPixelStorei(GL_PACK_SKIP_ROWS, skipRows);
    glPixelStorei(GL_PACK_SKIP_PIXELS, skipPixels);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(previousPackBuffer));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    if(!complete)return false;
#endif
    outData = std::move(data);
    return true;
}

std::uintptr_t Renderer::EditorImageToken(TextureHandle texture) const {return static_cast<std::uintptr_t>(ResolveTexture(texture));}
glm::ivec2 Renderer::TextureSize(TextureHandle texture) const {
    if (!ValidTexture(texture)) return {0, 0};
    const auto &stored = m_textures[texture.id];
    return {stored.width, stored.height};
}

GLuint Renderer::ResolveTexture(TextureHandle handle) const {
    if (ValidTexture(handle)) {
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
                         const glm::vec3& tintColor, float alpha,const std::vector<glm::mat4>* skin,const std::vector<std::string>* hiddenParts) {
    GpuMesh* gpuMesh = GetMesh(mesh);
    if (!gpuMesh) return;
    if(!m_submittingBatch&&!m_shadowPassActive&&!AllowsLayer(m_renderLayer)){++m_stats.layerRejectedDraws;return;}

    unsigned lod=0;
    if(!gpuMesh->lods.empty()){
        if(m_submittingBatch)lod=m_activeBatchLod;
        else if(m_meshLodMultiplier>0&&gpuMesh->lodPixels>0){
            GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);
            auto centre=position+rotation*((gpuMesh->bounds.min+gpuMesh->bounds.max)*.5f*scale);
            float maxScale=std::max({std::abs(scale.x),std::abs(scale.y),std::abs(scale.z)});
            float distance=std::max(.001f,glm::length(centre-glm::vec3(glm::inverse(m_view)[3]))-glm::length(gpuMesh->bounds.max-gpuMesh->bounds.min)*.5f*maxScale);
            float projected=maxScale*float(viewport[3])*std::abs(m_projection[1][1])*.5f;
            if(std::abs(m_projection[3][3])<.5f)projected/=distance;
            for(unsigned i=1;i<gpuMesh->lods.size();++i)if(gpuMesh->lods[i].error*projected<=gpuMesh->lodPixels*m_meshLodMultiplier)lod=i;
        }
    }
    const auto& primitives=lod?gpuMesh->lods[lod].primitives:gpuMesh->primitives;
    static const GpuMaterial failed=[](){GpuMaterial m;m.definition.model=MaterialModel::Unlit;m.definition.baseColor={.8f,.02f,.65f,1};return m;}();
    static const GpuMaterial pending=[](){GpuMaterial m;m.definition.model=MaterialModel::Unlit;m.definition.baseColor={.25f,.25f,.28f,1};return m;}();
    auto materialAt=[&](size_t slot)->const GpuMaterial* {MaterialHandle handle;if(slot<m_materialBindings.size())handle=m_materialBindings[slot].handle;if(!handle.IsValid()&&slot<m_materialBindings.size()&&m_materialBindings[slot].explicitAsset)return m_materialBindings[slot].failed?&failed:&pending;if(!handle.IsValid()&&slot<primitives.size()&&m_linearRendering){int index=primitives[slot].material;if(index>=0&&size_t(index)<gpuMesh->materials.size())handle=gpuMesh->materials[index];}return ValidMaterial(handle)?&m_materials[handle.id]:nullptr;};
    size_t parts=std::max(size_t(1),primitives.size());
    auto bindingAt=[&](size_t slot){return slot<m_materialBindings.size()?m_materialBindings[slot]:MaterialBinding{};};
    auto alphaAt=[&](size_t slot,const GpuMaterial* material){auto binding=bindingAt(slot);return binding.overrides.alpha.value_or(material?material->definition.alpha:MaterialAlpha::Opaque);};
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
    const auto* palette=skin&&!skin->empty()?skin:&gpuMesh->restSkin;
    auto submit=[&](bool shadow){for(size_t i=0;i<parts;++i){if(hiddenParts&&!primitives.empty()&&std::find(hiddenParts->begin(),hiddenParts->end(),primitives[i].part)!=hiddenParts->end())continue;auto* m=materialAt(i);bool isBlend=alphaAt(i,m)==MaterialAlpha::Blend;if((shadow&&isBlend)||(!shadow&&isBlend!=m_flushingBlends))continue;BindMaterial(m,bindingAt(i),texture,tintColor,alpha,shadow);if(!shadow)BindTerrainMaterial();BindTerrainGeometry(shadow);glm::mat4 orientation(1);if(i<gpuMesh->partOrientation.size()&&!palette->empty()){orientation=glm::mat4(0);auto& w=gpuMesh->partOrientation[i];for(int k=0;k<4;++k)orientation+=palette->at(w.joints[k])*w.weights[k]+palette->at(w.joints1[k])*w.weights1[k];}glFrontFace((m_submittingBatch?m_activeBatchReversed:glm::determinant(glm::mat3(model*orientation))<0)?GL_CW:GL_CCW);unsigned first=0,count=unsigned(gpuMesh->ebo?gpuMesh->indexCount:gpuMesh->vertexCount);if(!primitives.empty()){first=primitives[i].first;count=primitives[i].count;}glBindVertexArray(gpuMesh->vao);if(m_submittingBatch){if(gpuMesh->ebo)glDrawElementsInstanced(GL_TRIANGLES,GLsizei(count),GL_UNSIGNED_INT,reinterpret_cast<void*>(size_t(first)*4),GLsizei(m_activeInstances));else glDrawArraysInstanced(GL_TRIANGLES,GLint(first),GLsizei(count),GLsizei(m_activeInstances));++m_stats.instancedDraws;m_stats.instances+=m_activeInstances;}else {if(gpuMesh->ebo)glDrawElements(GL_TRIANGLES,GLsizei(count),GL_UNSIGNED_INT,reinterpret_cast<void*>(size_t(first)*4));else glDrawArrays(GL_TRIANGLES,GLint(first),GLsizei(count));}if(lod)m_stats.lodReducedInstances+=(m_submittingBatch?m_activeInstances:1);++m_stats.drawCalls;m_stats.triangles+=count/3*(m_submittingBatch?m_activeInstances:1);}glFrontFace(GL_CCW);};
    if(palette->size()>kModelPaletteLimit||palette->size()!=gpuMesh->restSkin.size())return;
    VisualBounds bounds=gpuMesh->bounds;
    if(!palette->empty()){bounds={};for(const auto& matrix:*palette){auto b=TransformBounds(gpuMesh->bounds,matrix);bounds.Include(b.min);bounds.Include(b.max);}}
    if(!m_submittingBatch)++m_stats.renderablesConsidered;
    if(!m_submittingBatch&&!IsVisible(TransformBounds(bounds,model))){++m_stats.renderablesCulled;return;}
    if(!m_submittingBatch)++m_stats.renderablesVisible;
    bool blended=false;for(size_t i=0;i<parts;++i)blended|=alphaAt(i,materialAt(i))==MaterialAlpha::Blend;
    if(blended&&!m_shadowPassActive&&!m_flushingBlends){BlendDraw draw{mesh,position,scale,tintColor,rotation,texture,alpha,skin?*skin:std::vector<glm::mat4>{},m_materialBindings,-(m_view*glm::vec4(position,1)).z,m_renderLayer,hiddenParts?*hiddenParts:std::vector<std::string>{}};m_blendDraws.push_back(std::move(draw));}

    if(m_collectingBatch&&m_batchingEnabled&&!m_submittingBatch&&palette->empty()&&(!hiddenParts||hiddenParts->empty())){
        bool opaque=true;for(size_t i=0;i<parts;++i)opaque&=alphaAt(i,materialAt(i))==MaterialAlpha::Opaque;
        if((opaque||m_render->orderedInstances)&&std::abs(glm::determinant(glm::mat3(model)))>1e-12f){QueueMeshInstance(mesh,texture,model,{tintColor,alpha},lod);return;}
    }
    // Milestone 15: while a shadow pass is active (see BeginShadowPass),
    // every DrawMesh call writes depth only, from that light's own view/
    // projection, through the separate minimal shadow shader — normals,
    // UVs, textures, and every lighting uniform are irrelevant to a depth-
    // only pass, so none of them are touched here.


    if (m_shadowPassActive) {
        glUseProgram(m_shadowShaderProgram);
        glUniform1i(m_uShadowInstanced,m_submittingBatch);
        glUniform1i(m_uShadowSkinned,!palette->empty());if(!palette->empty()){UploadPosePalette(*palette);glUniform1i(m_uShadowBones,15);}
        glUniformMatrix4fv(m_uShadowModel, 1, GL_FALSE, glm::value_ptr(model));
        submit(true);
        return;
    }

    // Standard correction for non-uniform scale — see the vertex shader's
    // own comment. glm::inverseTranspose is glm's dedicated helper for
    // exactly this (normal-matrix) computation.
    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(model));

    glUseProgram(m_shaderProgram);
    glUniform1i(MaterialUniform("uInstanced"),m_submittingBatch);
    glUniform1i(MaterialUniform("uBones"),15);
    glUniform1i(MaterialUniform("uSkinned"),!palette->empty());if(!palette->empty()){UploadPosePalette(*palette);glUniform1i(MaterialUniform("uBones"),15);}
    glUniformMatrix4fv(MaterialUniform("uModel"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(MaterialUniform("uNormalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    glUniform4f(MaterialUniform("uColor"), tintColor.r, tintColor.g, tintColor.b, alpha);
    std::string viewError;const auto viewBefore=m_render->viewUniforms.Stats();
    if(!m_render->viewUniforms.Bind(m_shaderProgram,viewError)){std::fprintf(stderr,"View upload failed: %s\n",viewError.c_str());return;}
    const auto viewAfter=m_render->viewUniforms.Stats();
    m_stats.uploadedBytes+=viewAfter.bytesUploaded-viewBefore.bytesUploaded;
    m_stats.uploadCalls+=viewAfter.uploads-viewBefore.uploads;

    glActiveTexture(GL_TEXTURE0);
    if (m_activeTarget.IsValid() && texture.IsValid() &&
        texture.id == RenderTargetTexture(m_activeTarget).id) {
        texture = m_whiteTexture; ++m_stats.feedbackFallbacks;
    }
    glBindTexture(GL_TEXTURE_2D, ResolveTexture(texture));
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D_ARRAY,m_shadowArray);
    glActiveTexture(GL_TEXTURE14);glBindTexture(GL_TEXTURE_2D,m_lightTexture);
    glActiveTexture(GL_TEXTURE0);  // restore the default active unit other calls (UI, texture creation) assume

    submit(false);
}

void Renderer::DrawBox(const glm::vec3& position, const glm::quat& rotation,
                        const glm::vec3& halfExtents, const glm::vec3& colorRgb, float alpha, TextureHandle texture) {
    DrawMesh(m_cubeMesh, position, rotation, halfExtents * 2.0f, texture, colorRgb, alpha);
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
                          float alpha, TextureHandle texture) {
    DrawMesh(m_sphereMesh, position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(radius),
             texture, colorRgb, alpha);
}

void Renderer::CaptureFrame(int width, int height, std::vector<unsigned char>& outRgbPixels) const {
    const size_t rowBytes = static_cast<size_t>(width) * 3;
    outRgbPixels.assign(rowBytes * static_cast<size_t>(height), 0);

    // RGB rows need not be four-byte aligned at arbitrary test/window sizes.
    // Preserve the caller's pack state and read tightly into this exact buffer.
    GLint packAlignment = 4;
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, outRgbPixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);

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
    JUDAS_PROFILE_COUNTER("Presentation upload bytes (view/light/instances/billboards)",double(m_stats.uploadedBytes),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Presentation upload calls",double(m_stats.uploadCalls),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Material applications",double(m_stats.materialApplications),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Reused shadow passes",double(m_stats.reusedShadowPasses),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Instance arena bytes",double(m_render->instanceArena.AllocatedBytes()),ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Billboard arena bytes",double(m_render->particlePass.Stats().arenaBytes),ProfileCounterMode::Latest);
    FlushMaterialBlends();FinishGridWater();const bool background=m_environmentBackground;
    if(m_weather.enabled)m_environmentBackground=false;
    ResolveLinearPass();m_environmentBackground=background;
}

bool Renderer::LoadFont(const char* path,float pixels,std::string& error){
    auto it=m_uiFonts.find(path);if(it==m_uiFonts.end()){std::shared_ptr<const TextFont> data;if(!LoadTextFont(path,data,error))return false;if(m_uiFonts.size()>=64){error="font compatibility cache full";return false;}it=m_uiFonts.emplace(path,data).first;}
    if(!m_defaultTextFont){m_defaultTextFont=it->second;m_defaultUIFont=path;}
    m_textFonts={it->second};m_fontPixelHeight=pixels;m_fontLoaded=true;return true;
}

void Renderer::BeginUIFrame(int windowWidth, int windowHeight) {
    // UI coordinates cover the output, independently of a letterboxed world view.
    glViewport(0,0,std::max(1,windowWidth),std::max(1,windowHeight));
    glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glDisable(GL_CULL_FACE);
    m_profileUI=BeginProfilePass("Runtime UI");
    m_uiDrawCalls=0;
    m_uiScreenSize = glm::vec2(static_cast<float>(std::max(windowWidth, 1)),
                                static_cast<float>(std::max(windowHeight, 1)));
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_uiShaderProgram);
    glUniform2f(m_uiUScreenSize, m_uiScreenSize.x, m_uiScreenSize.y);
    glUniform1i(m_uiUTextVertices,0);
    glUniform1i(m_uiUSceneLinear,0);
    glUniform1f(m_uiUExposure,m_exposure);
    glBindVertexArray(m_uiQuadVao);
}

void Renderer::DrawUIRect(const glm::vec2& position, const glm::vec2& size,
                           const glm::vec4& colorRgba) {
    glUniform1i(m_uiUSceneLinear,0);
    glUniform2f(m_uiUPosition, position.x, position.y);
    glUniform2f(m_uiUSize, size.x, size.y);
    glUniform4f(m_uiUColor, colorRgba.r, colorRgba.g, colorRgba.b, colorRgba.a);
    glUniform2f(m_uiUUVOffset, 0.0f, 0.0f);
    glUniform2f(m_uiUUVScale, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, ResolveTexture(m_whiteTexture));
    glDrawArrays(GL_TRIANGLES, 0, 6);++m_uiDrawCalls;
}

void Renderer::DrawUIText(const std::string& text,const glm::vec2& position,float scale,const glm::vec4& tint){TextOptions o;o.pixels=m_fontPixelHeight*scale;DrawTextLayout(*LayoutText(text,o),position,tint);}
glm::vec2 Renderer::MeasureUIText(const std::string& text,float scale) const{TextOptions o;o.pixels=m_fontPixelHeight*scale;auto l=LayoutText(text,o);return {l->width,l->height};}
float Renderer::GetUITextLineHeight(float scale)const{return MeasureUIText("Mg",scale).y;}

void Renderer::EndUIFrame() {
    EndProfilePass(m_profileUI);m_profileUI=0;
    JUDAS_PROFILE_COUNTER("Runtime UI draws",double(UIDrawCalls()),ProfileCounterMode::Latest);
    ClearUIClip();
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::DrawParticles(const std::vector<ParticleBillboard>& particles,const VisualBounds& bounds,TextureHandle texture){
    if(m_shadowPassActive||particles.empty())return;
    if(!AllowsLayer(m_renderLayer)){++m_stats.layerRejectedEmitters;return;}
    ++m_stats.particleEmittersConsidered;
    if(!IsVisible(bounds)){++m_stats.particleEmittersCulled;return;}
    ++m_stats.particleEmittersVisible;
    if(m_linearRendering)texture=ColourTexture(texture);
    const bool imageLinear=ValidTexture(texture)&&(m_textures[texture.id].srgb||m_textures[texture.id].sceneLinear);
    if(m_activeTarget.IsValid()&&texture.id==RenderTargetTexture(m_activeTarget).id){texture={};++m_stats.feedbackFallbacks;}
    const auto before=m_render->particlePass.Stats();
    std::string error;
    const auto profile=m_capabilities.modernTier?JudasRender::Shaders::Profile::Desktop450:JudasRender::Shaders::PortableProfile();
    if(!m_render->particlePass.Draw(m_view,m_projection,particles,ResolveTexture(texture),imageLinear,m_linearRendering,
                           m_render->persistentUploads&&m_capabilities.modernTier&&m_capabilities.bufferStorage,profile,error))
        std::fprintf(stderr,"Particle presentation failed: %s\n",error.c_str());
    const auto after=m_render->particlePass.Stats();
    m_stats.drawCalls+=unsigned(after.draws-before.draws);
    m_stats.triangles+=unsigned(after.triangles-before.triangles);
    m_stats.particlesSubmitted+=unsigned(after.particles-before.particles);
    m_stats.uploadedBytes+=after.uploadedBytes-before.uploadedBytes;
    m_stats.uploadCalls+=after.uploadCalls-before.uploadCalls;
}

void Renderer::DrawGpuParticles(JudasParticles::GpuParticles& particles,const VisualBounds& bounds,TextureHandle texture){
    if(m_shadowPassActive)return;
    if(!AllowsLayer(m_renderLayer)){++m_stats.layerRejectedEmitters;return;}
    ++m_stats.particleEmittersConsidered;
    if(!IsVisible(bounds)){++m_stats.particleEmittersCulled;return;}
    ++m_stats.particleEmittersVisible;
    if(m_linearRendering)texture=ColourTexture(texture);
    const bool imageLinear=ValidTexture(texture)&&(m_textures[texture.id].srgb||m_textures[texture.id].sceneLinear);
    if(m_activeTarget.IsValid()&&texture.id==RenderTargetTexture(m_activeTarget).id){texture={};++m_stats.feedbackFallbacks;}
    const auto before=m_render->gpuParticlePass.Stats();std::string error;
    if(!m_render->gpuParticlePass.Draw(particles,m_view,m_projection,ResolveTexture(texture),imageLinear,m_linearRendering,error))
        std::fprintf(stderr,"GPU particle presentation failed: %s\n",error.c_str());
    const auto after=m_render->gpuParticlePass.Stats();
    m_stats.drawCalls+=unsigned(after.draws-before.draws);m_stats.gpuParticleDraws+=unsigned(after.draws-before.draws);
    m_stats.gpuParticleLiveSample=particles.Stats().live;m_stats.gpuParticleSampleTick=particles.Diagnostics().sampleTick;
    JUDAS_PROFILE_COUNTER("GPU particle draws",double(after.draws-before.draws),ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("GPU particle live (delayed sample)",double(particles.Stats().live),ProfileCounterMode::Latest);
}

bool Renderer::SelectUIFont(const std::string& path,std::string& error){
    auto selected=path.empty()?m_defaultUIFont:path;if(selected.empty())return false;
    return LoadFont(selected.c_str(),48,error);
}
void Renderer::SetUIClip(glm::vec2 position,glm::vec2 size){
    auto lo=glm::max(glm::vec2(0),position),hi=glm::min(m_uiScreenSize,position+size);
    glEnable(GL_SCISSOR_TEST);glScissor(int(std::floor(lo.x)),int(std::floor(m_uiScreenSize.y-hi.y)),std::max(0,int(std::ceil(hi.x)-std::floor(lo.x))),std::max(0,int(std::ceil(hi.y)-std::floor(lo.y))));
}
void Renderer::ClearUIClip(){glDisable(GL_SCISSOR_TEST);}
void Renderer::DrawUIImage(glm::vec2 position,glm::vec2 size,TextureHandle texture,glm::vec4 tint,bool fit,bool flipVertical){
    if(!texture.IsValid())return;
    // Camera surfaces stay linear for world-material consumers. UI is a display
    // boundary: convert only textures whose allocation carries linear ownership.
    glUniform1i(m_uiUSceneLinear,ValidTexture(texture)&&m_textures[texture.id].sceneLinear);
    if(fit){const auto* image=ValidTexture(texture)?&m_textures[texture.id]:nullptr;
        if(image&&image->width>0&&image->height>0){auto scaled=glm::vec2(image->width,image->height);scaled*=std::min(size.x/scaled.x,size.y/scaled.y);position+=(size-scaled)*.5f;size=scaled;}}
    glUniform2f(m_uiUPosition,position.x,position.y);glUniform2f(m_uiUSize,size.x,size.y);
    // Imported TextureData stores the authored bottom row first for world
    // materials. UI's unit quad starts at its top-left, so compensate here;
    // fonts use their separate top-down atlas path. Preserve render-target UVs.
    const bool imported=ValidTexture(texture)&&!m_textures[texture.id].renderTarget;
    glUniform4f(m_uiUColor,tint.r,tint.g,tint.b,tint.a);
    const bool invert=imported!=flipVertical;glUniform2f(m_uiUUVOffset,0,invert?1:0);glUniform2f(m_uiUUVScale,1,invert?-1:1);
    glBindTexture(GL_TEXTURE_2D,ResolveTexture(texture));glDrawArrays(GL_TRIANGLES,0,6);++m_uiDrawCalls;
}

void Renderer::DrawBasicWater(const BasicWaterSettings& water,const glm::vec3& position,
                              const glm::quat& rotation,double seconds) {
    if(m_shadowPassActive||!water.enabled||!AllowsLayer(m_renderLayer))return;
    JUDAS_PROFILE_SCOPE("Basic water surface");
    RendererProfileScope gpu(*this,"Basic water surface");
    if(!m_basicWaterMesh.IsValid()){
        MeshData grid;constexpr unsigned cells=48;
        auto vertex=[&](unsigned x,unsigned z){MeshVertex v{};v.position={2.f*x/cells-1,0,2.f*z/cells-1};v.normal={0,1,0};v.uv={float(x)/cells,float(z)/cells};return v;};
        for(unsigned z=0;z<cells;++z)for(unsigned x=0;x<cells;++x){
            const auto a=vertex(x,z),b=vertex(x+1,z),c=vertex(x,z+1),d=vertex(x+1,z+1);
            for(const auto& v:{a,c,b,b,c,d})grid.vertices.push_back(v);
        }
        m_basicWaterMesh=CreateMesh(grid);
        if(!m_basicWaterMesh.IsValid())return;
        // Conservative shader-displacement bounds, independent of visual phase.
        m_meshes[m_basicWaterMesh.id].bounds.Include({-1,-5,-1});
        m_meshes[m_basicWaterMesh.id].bounds.Include({1,5,1});
    }
    glUseProgram(m_shaderProgram);
    glUniform1i(glGetUniformLocation(m_shaderProgram,"uBasicWater"),1);
    glUniform4f(glGetUniformLocation(m_shaderProgram,"uBasicWaterWave"),water.waveAmplitude,
                water.wavelength,water.waveSpeed,float(std::fmod(seconds,65536.)));
    glUniform2f(glGetUniformLocation(m_shaderProgram,"uBasicWaterSize"),water.halfExtents.x,water.halfExtents.z);
    const auto up=rotation*glm::vec3(0,1,0);
    glUniform3f(glGetUniformLocation(m_shaderProgram,"uBasicWaterUp"),up.x,up.y,up.z);
    GLboolean cull=glIsEnabled(GL_CULL_FACE);glDisable(GL_CULL_FACE);
    DrawMesh(m_basicWaterMesh,position,rotation,{water.halfExtents.x,1,water.halfExtents.z},{},water.color,water.opacity);
    if(cull)glEnable(GL_CULL_FACE);
    glUseProgram(m_shaderProgram);glUniform1i(glGetUniformLocation(m_shaderProgram,"uBasicWater"),0);
}

void Renderer::DrawTransientSurface(const MeshData& data,const glm::vec3& tint,float alpha){
 JUDAS_PROFILE_SCOPE("Water surface upload and submission");
 if(m_shadowPassActive||data.vertices.empty())return;
 if(!m_transientSurface.IsValid())m_transientSurface=CreateMesh(data);else UpdateMeshVertices(m_transientSurface,data.vertices);
 GLboolean cull=glIsEnabled(GL_CULL_FACE);glDisable(GL_CULL_FACE);
 DrawMesh(m_transientSurface,{0,0,0},{1,0,0,0},{1,1,1},{},tint,alpha);
 if(cull)glEnable(GL_CULL_FACE);
}

void Renderer::DrawWaterSurface(MeshHandle mesh,const BasicWaterSettings& style,glm::vec3 up){
 if(m_shadowPassActive||!mesh.IsValid()||!style.enabled||!AllowsLayer(m_renderLayer))return;
 RendererProfileScope gpu(*this,"Physical water surface");
 glUseProgram(m_shaderProgram);
 glUniform1i(glGetUniformLocation(m_shaderProgram,"uBasicWater"),1);
 glUniform1i(glGetUniformLocation(m_shaderProgram,"uWaterSurfaceMesh"),1);
 glUniform3f(glGetUniformLocation(m_shaderProgram,"uBasicWaterUp"),up.x,up.y,up.z);
 GLboolean cull=glIsEnabled(GL_CULL_FACE);glDisable(GL_CULL_FACE);
 DrawMesh(mesh,{0,0,0},{1,0,0,0},{1,1,1},{},style.color,style.opacity);
 if(cull)glEnable(GL_CULL_FACE);
 glUseProgram(m_shaderProgram);
 glUniform1i(glGetUniformLocation(m_shaderProgram,"uBasicWater"),0);
 glUniform1i(glGetUniformLocation(m_shaderProgram,"uWaterSurfaceMesh"),0);
}

void Renderer::DrawGridWater(MeshHandle handle,const JudasWater::SurfaceField& field,
                             const BasicWaterSettings& style,double seconds,std::uint64_t revision){
    if(m_shadowPassActive||!style.enabled||!AllowsLayer(m_renderLayer))return;
    const auto* mesh=GetMesh(handle);if(!mesh||!mesh->vertexCount)return;
    JUDAS_PROFILE_SCOPE("Grid water presentation");
    RendererProfileScope gpu(*this,"Grid water presentation");
    JudasRender::WaterPass::View camera;
    camera.view=m_view;camera.projection=m_projection;camera.sun=m_weatherSun;camera.sunColour=m_weatherSunColour;
    camera.colour=style.color;camera.opacity=style.opacity;camera.seconds=float(std::fmod(seconds,65536.));
    camera.background=m_weatherBackground;camera.ambient=m_weatherAmbient;
    camera.environmentInverse=glm::mat3_cast(glm::conjugate(m_environmentRotation));
    camera.environmentIntensity=m_environmentIntensity;
    if(m_environment.IsValid()&&m_environment.id<m_environments.size()&&m_environments[m_environment.id].alive)
        camera.environment=m_environments[m_environment.id].specular;
    camera.linear=m_linearRendering;
    std::string error;
    const auto key=(std::uint64_t(handle.generation)<<32)|handle.id;
    const auto before=m_render->waterPass.Draws(),underwater=m_render->waterPass.UnderwaterDraws();
    if(!m_render->waterPass.Draw(key,revision,mesh->vao,mesh->vertexCount,field,camera,error))
        std::fprintf(stderr,"Grid water rendering failed: %s\n",error.c_str());
    const auto draws=m_render->waterPass.Draws()-before;
    m_stats.drawCalls+=unsigned(draws);
    if(draws)m_stats.triangles+=unsigned(mesh->vertexCount/3)+unsigned(m_render->waterPass.UnderwaterDraws()-underwater);
    JUDAS_PROFILE_COUNTER("Grid water render GPU bytes",double(m_render->waterPass.Bytes()),ProfileCounterMode::Latest);
}

void Renderer::FinishGridWater(){
    if(!m_render->waterPass.HasPending())return;
    JUDAS_PROFILE_SCOPE("Grid water underwater");
    RendererProfileScope gpu(*this,"Grid water underwater");
    const auto before=m_render->waterPass.Draws();
    std::string error;
    if(!m_render->waterPass.Composite(error))std::fprintf(stderr,"Grid water optics failed: %s\n",error.c_str());
    const auto draws=unsigned(m_render->waterPass.Draws()-before);
    m_stats.drawCalls+=draws;m_stats.triangles+=draws;
}

#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
unsigned Renderer::BeginProfilePass(const char*,std::uint64_t){
    PerformanceProfiler::Get().GPUAvailability(false,"ES 3 timestamp queries not enabled");return 0;
}
void Renderer::EndProfilePass(unsigned){}
void Renderer::PollProfileGPU(){}
#else
unsigned Renderer::BeginProfilePass(const char* name,std::uint64_t camera){
    auto& p=PerformanceProfiler::Get();if(!p.Active())return 0;
    if(!glQueryCounter||!glGetQueryObjectui64v){p.GPUAvailability(false,"GL timestamp entry points unavailable");return 0;}
    p.GPUAvailability(true,"Asynchronous GL 3.3 timestamp pairs");
    if(!m_profileQueriesReady){for(auto& q:m_profileQueries){glGenQueries(1,&q.begin);glGenQueries(1,&q.end);}m_profileQueriesReady=true;}
    for(unsigned i=0;i<m_profileQueries.size();++i){auto& q=m_profileQueries[i];if(q.pending)continue;
        auto record=p.GPUPending(name,camera);if(!record)return 0;
        q.frame=p.FrameId();q.record=record;q.pending=true;q.ended=false;
        glQueryCounter(q.begin,GL_TIMESTAMP);return i+1;
    }p.DropGPU();return 0;
}
void Renderer::EndProfilePass(unsigned token){if(!token||token>m_profileQueries.size())return;auto& q=m_profileQueries[token-1];if(!q.pending||q.ended)return;glQueryCounter(q.end,GL_TIMESTAMP);q.ended=true;}
void Renderer::PollProfileGPU(){
    if(!m_profileQueriesReady)return;
    auto& p=PerformanceProfiler::Get();
    for(auto& q:m_profileQueries){if(!q.pending||!q.ended||q.frame==p.FrameId())continue;
        GLint available=0;glGetQueryObjectiv(q.end,GL_QUERY_RESULT_AVAILABLE,&available);if(!available)continue;
        GLuint64 a=0,b=0;glGetQueryObjectui64v(q.begin,GL_QUERY_RESULT,&a);glGetQueryObjectui64v(q.end,GL_QUERY_RESULT,&b);
        p.GPUComplete(q.frame,q.record,double(b>=a?b-a:0)/1e6,a,b);q.pending=q.ended=false;
    }
}

#endif

#include "RendererMaterials.inl"

#include "RendererText.inl"

#include "RendererWeather.inl"

#include "RendererSprites.inl"
