#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "FontLoader.h"
#include "Light.h"
#include "MeshData.h"
#include "TextureData.h"
#include "gl_core33.h"

// Opaque handles into Renderer's own GPU resource tables — the same
// convention PhysicsWorld::BodyHandle already established (a small integer
// id, kInvalid sentinel, no concrete type exposed to callers). Deliberately
// not tied to a raw GLuint in any caller-visible way, so no file outside
// Renderer.cpp needs to know what a VAO/VBO/texture object even is.
struct MeshHandle {
    static constexpr unsigned int kInvalidId = 0xFFFFFFFFu;
    unsigned int id = kInvalidId;
    bool IsValid() const { return id != kInvalidId; }
};
struct TextureHandle {
    static constexpr unsigned int kInvalidId = 0xFFFFFFFFu;
    unsigned int id = kInvalidId;
    bool IsValid() const { return id != kInvalidId; }
};

// Owns the GL objects and draw calls. Low-level calls (glClear, glDrawArrays,
// ...) come from an external API, but the *concept* of "begin a frame / set
// the active camera / draw a mesh / end a frame" is this engine's own
// boundary, and is what has to survive a future move to a different graphics
// API. Callers (Application, gameplay code, model/texture loaders) never
// touch OpenGL directly — see docs/ARCHITECTURE.md, "Milestone 9," for why
// this boundary held even once textures/lighting/indexed meshes arrived.
class Renderer {
public:
    bool Init();
    void Shutdown();

    // Clears the color and depth buffers and sets the viewport to the given
    // window size.
    void BeginFrame(int windowWidth, int windowHeight);

    // Sets the view/projection matrices used by all Draw* calls until the
    // next SetCamera call. Recomputing this every frame (rather than
    // reacting to a resize event) is what keeps the projection's aspect
    // ratio correct across window resizes.
    void SetCamera(const glm::mat4& view, const glm::mat4& projection);

    // Milestone 9: the demo's one directional light, plus a small constant
    // ambient term. `direction` points FROM a lit surface TOWARD the light
    // (i.e. already negated from "the direction the light travels") — see
    // docs/ARCHITECTURE.md, "Milestone 9, Lighting," for the exact
    // convention and why it is a plain world-space vector with no relation
    // to gravity, local up, or any other Judas concept. Called once per
    // frame alongside SetCamera; this demo's light never changes, but nothing
    // stops a future caller from varying it frame to frame the same way the
    // camera already does.
    void SetLighting(const glm::vec3& direction, const glm::vec3& lightColor,
                      const glm::vec3& ambientColor);

    // Milestone 14: the current frame's dynamic point/spot lights, already
    // in WORLD space (see src/Light.h's own doc comment for who's
    // responsible for getting them there). Called once per frame,
    // alongside SetLighting/SetCamera, BEFORE the frame's DrawMesh/DrawBox/
    // DrawSphere calls — every mesh drawn until the next SetDynamicLights
    // call is lit by exactly this light set, on top of the existing
    // ambient + directional terms SetLighting already provides (see
    // docs/ARCHITECTURE.md, "Milestone 14, Combining lights" — dynamic
    // lights are additive, never a replacement for the Milestone 9
    // directional "sun"). `lights.size()` beyond kMaxDynamicLights (see
    // src/Light.h) is silently truncated, not an error — M14 doesn't need
    // more than that many at once, see "Milestone 14, Light limits."
    void SetDynamicLights(const std::vector<DynamicLight>& lights);

    // --- Judas-owned mesh/texture resources (Milestone 9) ---
    //
    // Uploads `data` to the GPU (VAO/VBO, plus an EBO if `data.indices` is
    // non-empty) and returns a handle Application/gameplay code can hold and
    // pass to DrawMesh — the CPU-side MeshData itself is not retained after
    // this call returns; Renderer owns only the GPU-side copy from this
    // point on. See docs/ARCHITECTURE.md for the full CPU/GPU ownership
    // split (ModelLoader produces MeshData; Renderer uploads and owns the
    // GPU resource; the caller owns only the opaque handle).
    MeshHandle CreateMesh(const MeshData& data);
    void DestroyMesh(MeshHandle handle);

    // Uploads `data` as a 2D RGBA texture (linear filtering, mipmapped,
    // repeat wrapping — see docs/ARCHITECTURE.md for why these were judged
    // sufficient for one demo texture) and returns a handle. Same ownership
    // split as CreateMesh.
    TextureHandle CreateTexture(const TextureData& data);
    void DestroyTexture(TextureHandle handle);

    // Draws any mesh created via CreateMesh with an arbitrary
    // position/rotation/scale, modulated by `tintColor` and by `texture`'s
    // sampled color — an invalid `texture` handle draws with a solid 1x1
    // white fallback texture instead (see Init), so a caller with no real
    // texture (every existing primitive) still goes through the exact same
    // shader/lighting path as a textured model, just with texColor
    // effectively 1. This is the single generalized draw path DrawBox/
    // DrawSphere are now thin wrappers over.
    void DrawMesh(MeshHandle mesh, const glm::vec3& position, const glm::quat& rotation,
                  const glm::vec3& scale, TextureHandle texture, const glm::vec3& tintColor);

    // Draws a box mesh: `halfExtents` sets its size along each axis (the
    // local unit cube is scaled by 2*halfExtents), `rotation` its
    // orientation. Callers pass whatever position/rotation they have —
    // Milestone 3's physics-driven cube passes values read straight from
    // PhysicsWorld::GetTransform, with no separate rendering-side motion
    // logic. Unchanged signature since Milestone 3; internally a thin
    // DrawMesh wrapper as of Milestone 9 (see above) — every existing call
    // site needed zero changes for this migration.
    void DrawBox(const glm::vec3& position, const glm::quat& rotation,
                 const glm::vec3& halfExtents, const glm::vec3& colorRgb);

    // Draws a sphere mesh of the given world-space radius. Added in
    // Milestone 5 for the spherical test world; a sphere looks identical
    // under any (single-axis) rotation, so unlike DrawBox there is no
    // rotation parameter. Unchanged signature; DrawMesh wrapper as of
    // Milestone 9.
    void DrawSphere(const glm::vec3& position, float radius, const glm::vec3& colorRgb);

    void EndFrame();

    // --- Milestone 13: UI overlay rendering ---
    //
    // A second, deliberately separate draw path from DrawMesh/DrawBox/
    // DrawSphere above: screen-space pixel coordinates (not world-space +
    // a view/projection matrix), unlit, alpha-blended, depth-test-disabled
    // — exactly what a 2D HUD/menu overlay needs and nothing the 3D path
    // already provides. Still entirely behind this class's own raw-GL
    // boundary (see docs/ARCHITECTURE.md, "Milestone 13, Text rendering")
    // — gameplay/UI code never touches OpenGL, only these calls.
    //
    // Call BeginUIFrame once after EndFrame, issue any number of
    // DrawUIRect/DrawUIText calls, then EndUIFrame before SwapBuffers.

    // Loads and uploads the one font this engine's UI text needs (see
    // src/FontLoader.h) — same load-CPU-data-then-upload split as
    // CreateMesh/CreateTexture. Call once, near Init; DrawUIText silently
    // draws nothing if no font has been loaded yet.
    bool LoadFont(const char* path, float pixelHeight, std::string& outError);

    // Sets the screen-space pixel dimensions used to convert every
    // subsequent DrawUIRect/DrawUIText call's pixel coordinates into clip
    // space, disables depth testing (UI always draws on top, in call
    // order — no 3D occlusion concept applies here), and enables standard
    // alpha blending (source-alpha, one-minus-source-alpha) so translucent
    // panels and anti-aliased glyph edges composite correctly over
    // whatever DrawMesh/DrawBox/DrawSphere already rendered this frame.
    void BeginUIFrame(int windowWidth, int windowHeight);

    // Draws a solid (or translucent, via colorRgba's alpha) axis-aligned
    // rectangle, `position` = top-left corner in pixels, `size` in pixels.
    // Used for menu panels/button backgrounds — no border, no rounding, no
    // texture: the minimum a pause menu actually needs (see
    // docs/ARCHITECTURE.md, "Milestone 13").
    void DrawUIRect(const glm::vec2& position, const glm::vec2& size, const glm::vec4& colorRgba);

    // Draws `text` with its top-left corner at `position` (pixels), scaled
    // relative to the font's own baked pixel size (see FontAtlasData::
    // pixelHeight — `scale = 1.0` draws at exactly that baked size).
    // Single-line only: a caller that needs multiple lines (see src/HUD.cpp,
    // src/UIWidgets.cpp) calls this once per line at its own computed Y
    // offset, using GetUITextLineHeight below — kept this simple
    // deliberately, per M13's "minimum reusable capability" instruction,
    // rather than teaching this one call about line-wrapping/alignment.
    // No-op (draws nothing) if LoadFont was never called or failed.
    void DrawUIText(const std::string& text, const glm::vec2& position, float scale,
                     const glm::vec4& colorRgba);

    // The pixel width/height `text` would occupy if drawn via DrawUIText at
    // the same `scale` — used by menu layout (centering button labels,
    // sizing button backgrounds to fit their text) and HUD layout. Returns
    // (0, 0) if no font is loaded.
    glm::vec2 MeasureUIText(const std::string& text, float scale) const;

    // The font's own recommended baseline-to-baseline distance at `scale`
    // — what a caller drawing several DrawUIText lines should advance Y by
    // between them. Returns 0 if no font is loaded.
    float GetUITextLineHeight(float scale) const;

    // Re-enables depth testing (so the next frame's 3D draws behave
    // exactly as before UI rendering existed) and disables blending. Call
    // once after the last DrawUIRect/DrawUIText this frame, before
    // SwapBuffers.
    void EndUIFrame();

    // Reads back the current color buffer as tightly-packed 8-bit RGB rows,
    // top row first (`glReadPixels` itself returns bottom row first — this
    // flips it, since that's what every common image format/library
    // expects). Developer tooling only (see src/TestHarness.h); nothing
    // in the normal game loop calls this; it exists so the engine's actual
    // rendered output can be inspected without a way to see the window.
    void CaptureFrame(int width, int height, std::vector<unsigned char>& outRgbPixels) const;

private:
    // One GPU-resident mesh: a VAO/VBO pair, an optional EBO (0 if the mesh
    // is drawn non-indexed — see MeshData's own convention), and enough
    // count/type information to issue the right draw call.
    struct GpuMesh {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;          // 0 if non-indexed
        GLsizei vertexCount = 0;  // used when ebo == 0 (glDrawArrays)
        GLsizei indexCount = 0;   // used when ebo != 0 (glDrawElements)
        bool alive = false;
    };
    struct GpuTexture {
        GLuint textureId = 0;
        bool alive = false;
    };

    GpuMesh* GetMesh(MeshHandle handle);
    GLuint ResolveTexture(TextureHandle handle) const;

    std::vector<GpuMesh> m_meshes;
    std::vector<GpuTexture> m_textures;

    GLuint m_shaderProgram = 0;

    MeshHandle m_cubeMesh;
    MeshHandle m_sphereMesh;
    TextureHandle m_whiteTexture;  // 1x1 white pixel — the "no real texture" fallback, see DrawMesh

    GLint m_uModel = -1;
    GLint m_uNormalMatrix = -1;
    GLint m_uView = -1;
    GLint m_uProjection = -1;
    GLint m_uColor = -1;
    GLint m_uTexture = -1;
    GLint m_uLightDirection = -1;
    GLint m_uLightColor = -1;
    GLint m_uAmbientColor = -1;

    // --- Milestone 14: dynamic point/spot lights ---
    //
    // One uniform location per FIELD, per ARRAY SLOT (kMaxDynamicLights of
    // each) — GLSL struct-array uniforms are addressed by their own
    // "uLights[i].field" name per element; there is no single "array"
    // location to cache the way a plain vec3/float uniform has one. Fetched
    // once in Init, reused every SetDynamicLights call.
    GLint m_uLightCount = -1;
    GLint m_uDynamicLightPosition[kMaxDynamicLights];
    GLint m_uDynamicLightDirection[kMaxDynamicLights];
    GLint m_uDynamicLightColor[kMaxDynamicLights];
    GLint m_uDynamicLightRange[kMaxDynamicLights];
    GLint m_uDynamicLightInnerCos[kMaxDynamicLights];
    GLint m_uDynamicLightOuterCos[kMaxDynamicLights];
    GLint m_uDynamicLightIsSpot[kMaxDynamicLights];

    glm::mat4 m_view{1.0f};
    glm::mat4 m_projection{1.0f};

    // --- Milestone 13: UI overlay state ---
    GLuint m_uiShaderProgram = 0;
    GLuint m_uiQuadVao = 0;
    GLuint m_uiQuadVbo = 0;
    GLint m_uiUScreenSize = -1;
    GLint m_uiUPosition = -1;
    GLint m_uiUSize = -1;
    GLint m_uiUColor = -1;
    GLint m_uiUTexture = -1;
    GLint m_uiUUVOffset = -1;
    GLint m_uiUUVScale = -1;
    glm::vec2 m_uiScreenSize{0.0f, 0.0f};

    TextureHandle m_fontAtlasTexture;
    FontGlyph m_fontGlyphs[kFontGlyphCount];
    float m_fontPixelHeight = 0.0f;
    float m_fontAscent = 0.0f;
    float m_fontLineHeight = 0.0f;
    bool m_fontLoaded = false;
};
