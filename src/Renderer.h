#include <array>
#pragma once
#include "Classification.h"

#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "FontLoader.h"
#include "TextLayout.h"
#include "Light.h"
#include "MeshData.h"
#include "DebugDraw.h"
#include "TextureData.h"
#include "Material.h"
#include "Environment.h"
#include <map>
#include "ResourceTrace.h"
#include "Visibility.h"
#include "VisualParticles.h"
#include <glad/gl.h>

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
struct RenderTargetHandle {
    unsigned int id = 0xFFFFFFFFu;
    bool IsValid() const { return id != 0xFFFFFFFFu; }
};

struct MaterialHandle {unsigned id=0xFFFFFFFFu;bool IsValid()const{return id!=0xFFFFFFFFu;}};
struct EnvironmentHandle {unsigned id=0xFFFFFFFFu;bool IsValid()const{return id!=0xFFFFFFFFu;}};
struct MaterialBinding {MaterialHandle handle;MaterialOverride overrides;bool explicitAsset=false,failed=false;};
struct TextureHandle {
    static constexpr unsigned int kInvalidId = 0xFFFFFFFFu;
    unsigned int id = kInvalidId;
    bool IsValid() const { return id != kInvalidId; }
};

// Milestone 30: what Renderer actually submitted since the last
// ResetStats — counted at the draw call, not estimated. `drawCalls` and
// `triangles` include shadow-pass submissions (each shadow pass re-draws
// the scene); `shadowPasses` says how many such passes ran; `debugLines`
// counts line segments, which are not triangles.
struct RenderStats {
    unsigned int drawCalls = 0;
    unsigned int layerRejectedDraws=0,layerRejectedEmitters=0;
    unsigned int renderablesConsidered=0,renderablesVisible=0,renderablesCulled=0;
    unsigned int particleEmittersConsidered=0,particleEmittersVisible=0,particleEmittersCulled=0,particlesSubmitted=0;
    unsigned int triangles = 0;
    unsigned int shadowPasses = 0;
    unsigned int dynamicLights = 0;  // as last set by SetDynamicLights (after truncation)
    unsigned int offscreenPasses = 0;
    unsigned int feedbackFallbacks = 0;
    unsigned int targetDeletionFailures = 0;
    unsigned int debugLines = 0;
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
    MaterialHandle CreateMaterial(const MaterialDefinition&);
    void DestroyMaterial(MaterialHandle);
    EnvironmentHandle CreateEnvironment(const EnvironmentData&);
    void DestroyEnvironment(EnvironmentHandle);
    void SetMaterialBindings(const std::vector<MaterialBinding>& slots){m_materialBindings=slots;}
    void SetSceneAppearance(bool linear,float exposure,EnvironmentHandle environment,float intensity,const glm::quat& rotation,bool background,const glm::vec3& colour={.08f,.09f,.11f});
    void FlushMaterialBlends();
    unsigned TextureUnitLimit()const{return m_textureUnitLimit;}
    std::size_t AppearanceBytes()const;

    unsigned BeginProfilePass(const char* name,std::uint64_t camera=0);
    void EndProfilePass(unsigned token);
    void PollProfileGPU();
    // Generated resources belong to runtime presentation, not disk assets.
    RenderTargetHandle CreateRenderTarget(int width, int height, std::string& error);
    bool ResizeRenderTarget(RenderTargetHandle& target, int width, int height, std::string& error);
    void DestroyRenderTarget(RenderTargetHandle target);
    TextureHandle RenderTargetTexture(RenderTargetHandle target) const;
    std::uintptr_t EditorImageToken(TextureHandle) const;
    glm::ivec2 RenderTargetSize(RenderTargetHandle target) const;
    void FinishForDiagnostics() const;
    bool BeginRenderTarget(RenderTargetHandle target);
    void EndRenderTarget();
    struct TargetDiagnostics { glm::ivec4 viewport{0}; bool defaultFramebuffer = false;
        std::size_t liveTargets = 0; bool passActive = false; };
    TargetDiagnostics RenderTargetDiagnostics() const;
    void Shutdown();
    // Optional observation of actual GPU operations; installed before Init.
    void SetResourceTrace(ResourceTrace trace) { m_resourceTrace = std::move(trace); }

    // Clears the color and depth buffers and sets the viewport to the given
    // window size.
    void BeginFrame(int windowWidth, int windowHeight);

    // Sets the view/projection matrices used by all Draw* calls until the
    // next SetCamera call. Recomputing this every frame (rather than
    // reacting to a resize event) is what keeps the projection's aspect
    // ratio correct across window resizes.
    void SetCamera(const glm::mat4& view, const glm::mat4& projection);
    const glm::mat4& ViewMatrix()const{return m_view;}
    const glm::mat4& ProjectionMatrix()const{return m_projection;}
    void SetWaterPaths(unsigned columns,unsigned rows,const std::vector<glm::vec2>& paths);
    void SetCullingEnabled(bool enabled){m_cullingEnabled=enabled;}
    void SetRenderMask(CategoryMask mask){m_renderMask=mask;}
    void SetRenderLayer(unsigned layer){m_renderLayer=layer;}
    bool AllowsLayer(unsigned layer)const{return (m_renderMask&CategoryBit(layer))!=0;}
    bool IsVisible(const VisualBounds& bounds)const{return !m_cullingEnabled || m_frustum.IsVisible(bounds);}
    void DrawSphereTransformed(const glm::vec3& p,const glm::quat& q,const glm::vec3& scale,float radius,const glm::vec3& color,float alpha,TextureHandle texture){DrawMesh(m_sphereMesh,p,q,scale*radius,texture,color,alpha);}
    void DrawParticles(const std::vector<ParticleBillboard>& particles,const VisualBounds& bounds,TextureHandle texture={});

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
    // Owner-thread staged upload. The caller keeps the handle private until every
    // material map is installed; cancellation destroys it normally.
    MeshHandle BeginMeshUpload(const MeshData& data);
    bool UploadMeshMap(MeshHandle, const MeshData&, size_t material, size_t map);
    // Replaces a non-indexed mesh's vertices without changing its handle.
    // Used for a surface derived each presentation frame from simulated
    // fluid positions. Returns false for indexed or invalid handles.
    bool UpdateMeshVertices(MeshHandle handle, const std::vector<MeshVertex>& vertices);
    void DestroyMesh(MeshHandle handle);

    // Uploads `data` as a 2D RGBA texture (linear filtering, mipmapped,
    // repeat wrapping — see docs/ARCHITECTURE.md for why these were judged
    // sufficient for one demo texture) and returns a handle. Same ownership
    // split as CreateMesh.
    TextureHandle CreateTexture(const TextureData& data,bool srgb=false);
    void DestroyTexture(TextureHandle handle);

    // Read the actual GPU payload, not a retained CPU copy. Diagnostic use on
    // the context-owning thread only; bindings and pixel-pack state are restored.
    bool ReadMeshForDiagnostics(MeshHandle handle, MeshData& outData) const;
    bool ReadTextureForDiagnostics(TextureHandle handle, TextureData& outData) const;

    // Draws any mesh created via CreateMesh with an arbitrary
    // position/rotation/scale, modulated by `tintColor` and by `texture`'s
    // sampled color — an invalid `texture` handle draws with a solid 1x1
    // white fallback texture instead (see Init), so a caller with no real
    // texture (every existing primitive) still goes through the exact same
    // shader/lighting path as a textured model, just with texColor
    // effectively 1. This is the single generalized draw path DrawBox/
    // DrawSphere are now thin wrappers over.
    void DrawMesh(MeshHandle mesh, const glm::vec3& position, const glm::quat& rotation,
                  const glm::vec3& scale, TextureHandle texture, const glm::vec3& tintColor,
                  float alpha = 1.0f,const std::vector<glm::mat4>* skin = nullptr,const std::vector<std::string>* hiddenParts=nullptr);

    // Draws a box mesh: `halfExtents` sets its size along each axis (the
    // local unit cube is scaled by 2*halfExtents), `rotation` its
    // orientation. Callers pass whatever position/rotation they have —
    // Milestone 3's physics-driven cube passes values read straight from
    // PhysicsWorld::GetTransform, with no separate rendering-side motion
    // logic. Unchanged signature since Milestone 3; internally a thin
    // DrawMesh wrapper as of Milestone 9 (see above) — every existing call
    // site needed zero changes for this migration.
    void DrawBox(const glm::vec3& position, const glm::quat& rotation,
                 const glm::vec3& halfExtents, const glm::vec3& colorRgb, float alpha = 1.0f,
                 TextureHandle texture = {});

    // A small world-space transparent pass for viewing fluid through a
    // cup's ordinary solid walls. Depth is tested but not written; calls
    // must follow opaque geometry and be followed by EndTransparentPass.
    // Generic transient CPU geometry; Renderer retains/reuses its GPU buffer.
    void DrawTransientSurface(const MeshData&,const glm::vec3& tint,float alpha);
    void BeginTransparentPass();
    void EndTransparentPass();
    bool IsShadowPass() const { return m_shadowPassActive; }

    // Draws a sphere mesh of the given world-space radius. Added in
    // Milestone 5 for the spherical test world; a sphere looks identical
    // under any (single-axis) rotation, so unlike DrawBox there is no
    // rotation parameter. DrawMesh wrapper as of Milestone 9. As with any
    // transparent mesh, alpha below 1 requires BeginTransparentPass first.
    void DrawSphere(const glm::vec3& position, float radius, const glm::vec3& colorRgb,
                    float alpha = 1.0f, TextureHandle texture = {});

    void EndFrame();

    // --- Milestone 30: debug line overlay + submission statistics ---
    //
    // Draws world-space line segments unlit, depth-tested against whatever
    // the frame already drew (so a collision shape hidden behind a wall is
    // hidden), through a third small shader/VBO pair kept out of the lit
    // mesh path. Lines are uploaded per call (a debug view is rebuilt every
    // frame it is enabled; nothing is retained). Call between SetCamera and
    // EndFrame; a no-op during a shadow pass. `depthTest` false draws on
    // top of everything (the editor's gizmo handles).
    void DrawDebugLines(const std::vector<DebugLine>& lines, bool depthTest = true);

    void ResetStats() { m_stats = RenderStats{}; }
    const RenderStats& Stats() const { return m_stats; }

    // --- Milestone 15: shadow mapping ---
    //
    // Renders depth-only, from one shadow-casting light's own point of
    // view, into that light's dedicated depth texture (see src/Light.h's
    // kDirectionalShadowSlot/kTorchShadowSlot/kShipHeadlightShadowSlot).
    // Call once per shadow-casting light, BEFORE the frame's normal
    // BeginFrame/SetCamera/color-pass sequence: BeginShadowPass, then the
    // SAME drawScene-style sequence of DrawMesh/DrawBox/DrawSphere calls
    // the color pass itself will issue (every mesh drawn while a shadow
    // pass is active writes depth only, through a separate minimal
    // shader — see Renderer.cpp), then EndShadowPass. `lightViewProjection`
    // is cached internally (see src/ShadowTransforms.h for how to build
    // one) and reused automatically by every subsequent NORMAL (non-
    // shadow-pass) DrawMesh call this same frame, so the color pass needs
    // no separate call to "use" this slot's shadow map — it always samples
    // whichever light-space matrix/depth texture this frame's own
    // BeginShadowPass calls most recently produced for each slot.
    void BeginShadowPass(int shadowSlot, const glm::mat4& lightViewProjection);

    // Restores normal (default framebuffer) rendering. The next
    // BeginFrame call resets the viewport back to the window's own size —
    // this does not need to do so itself.
    void EndShadowPass();

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
    void SetUIClip(glm::vec2 position,glm::vec2 size);
    void ClearUIClip();
    void DrawUIImage(glm::vec2 position,glm::vec2 size,TextureHandle texture,glm::vec4 tint,bool fit);
    bool SelectUIFont(const std::string& path,std::string& error);
    std::shared_ptr<const TextFont> DefaultTextFont()const{return m_defaultTextFont;}
    void SelectTextFonts(std::vector<std::shared_ptr<const TextFont>> fonts);
    std::shared_ptr<const TextLayout> LayoutText(const std::string&,const TextOptions&) const;
    void DrawTextLayout(const TextLayout&,glm::vec2 position,glm::vec4 tint);
    void ResetProjectText();
    TextCacheStats TextStats()const{return m_textEngine.Stats();}
    size_t TextAtlasBytes()const{return m_textPages.size()*1024*1024*4;}
    unsigned TextAtlasGeneration()const{return m_textAtlasGeneration;}
    unsigned UIDrawCalls()const{return m_uiDrawCalls;}

    // Reads back the current color buffer as tightly-packed 8-bit RGB rows,
    // top row first (`glReadPixels` itself returns bottom row first — this
    // flips it, since that's what every common image format/library
    // expects). Developer tooling only (see src/TestHarness.h); nothing
    // in the normal game loop calls this; it exists so the engine's actual
    // rendered output can be inspected without a way to see the window.
    void CaptureFrame(int width, int height, std::vector<unsigned char>& outRgbPixels) const;

private:
    struct ProfileQuery {unsigned begin=0,end=0;std::uint64_t frame=0,record=0;bool pending=false,ended=false;};
    std::array<ProfileQuery,128> m_profileQueries{};
    bool m_profileQueriesReady=false;
    unsigned m_profileShadow=0,m_profileUI=0;

    // One GPU-resident mesh: a VAO/VBO pair, an optional EBO (0 if the mesh
    // is drawn non-indexed — see MeshData's own convention), and enough
    // count/type information to issue the right draw call.
    struct GpuMesh {
        GLuint vao = 0;
        GLuint vbo = 0,skinVbo=0;
        GLuint ebo = 0;          // 0 if non-indexed
        GLsizei vertexCount = 0;  // used when ebo == 0 (glDrawArrays)
        GLsizei indexCount = 0;   // used when ebo != 0 (glDrawElements)
        bool alive = false;
        VisualBounds bounds;
        std::vector<glm::mat4> restSkin;
        std::vector<MeshSkinVertex> partOrientation;
        std::vector<MeshPrimitive> primitives;
        std::vector<MaterialHandle> materials;
    };
    struct GpuTarget { GLuint framebuffer = 0, depth = 0; TextureHandle color; int width = 0, height = 0; };
    std::vector<GpuTarget> m_targets;
    RenderTargetHandle m_activeTarget;
    GLint m_savedDrawFramebuffer = 0, m_savedReadFramebuffer = 0;
    GLint m_savedViewport[4] = {0, 0, 0, 0};
    CategoryMask m_savedRenderMask=kAllCategories;
    unsigned m_savedRenderLayer=0;
    glm::mat4 m_savedView{1.0f}, m_savedProjection{1.0f};

    struct GpuTexture {
        GLuint textureId = 0;
        int width=0,height=0;
        std::size_t uploadedBytes = 0;  // base-level RGBA payload, excluding generated mipmaps
        bool alive = false,sceneLinear=false,srgb=false,renderTarget=false;
        TextureHandle colourView;
    };

    void TraceResourceOperation(ResourceTracePoint point, unsigned int handle = 0, std::size_t bytes = 0) const;
    ResourceTrace m_resourceTrace;

    GpuMesh* GetMesh(MeshHandle handle);
    GLuint ResolveTexture(TextureHandle handle) const;

    std::vector<GpuMesh> m_meshes;
    std::vector<GpuTexture> m_textures;

    struct GpuMaterial {bool alive=false;MaterialDefinition definition;std::array<TextureHandle,5> textures;std::array<GLuint,5> samplers{};};
    struct GpuEnvironment {bool alive=false;GLuint diffuse=0,specular=0,brdf=0;unsigned levels=0;std::size_t bytes=0;};
    std::vector<GpuMaterial> m_materials;std::vector<GpuEnvironment> m_environments;
    std::vector<MaterialBinding> m_materialBindings;
    MaterialDefinition m_fallbackMaterial;
    std::map<std::string,GLint> m_materialUniforms;
    GLint MaterialUniform(const char* name);
    TextureHandle ColourTexture(TextureHandle);
    void BindMaterial(const GpuMaterial*,const MaterialOverride&,TextureHandle,const glm::vec3&,float,bool shadow);
    void BeginLinearPass(int width,int height);
    void ResolveLinearPass(bool sceneLinear=false);
    void ShutdownAppearance();
    bool m_linearRendering=false,m_linearPass=false,m_environmentBackground=false;
    float m_exposure=1,m_environmentIntensity=1;glm::quat m_environmentRotation{1,0,0,0};EnvironmentHandle m_environment;
    struct LinearTarget {GLuint fbo=0,color=0,depth=0;int width=0,height=0;};
    std::vector<LinearTarget> m_linearTargets;
    GLuint m_hdrFbo=0,m_hdrColor=0,m_hdrDepth=0,m_outputProgram=0,m_outputVao=0;
    GLint m_outputFramebuffer=0;int m_hdrWidth=0,m_hdrHeight=0;unsigned m_textureUnitLimit=0;
    struct BlendDraw {MeshHandle mesh;glm::vec3 position,scale,tint;glm::quat rotation;TextureHandle texture;float alpha;std::vector<glm::mat4> skin;std::vector<MaterialBinding> materials;float depth;unsigned layer;std::vector<std::string> hiddenParts;};
    std::vector<BlendDraw> m_blendDraws;bool m_flushingBlends=false;
    GLuint m_waterPathTexture=0;unsigned m_waterColumns=0,m_waterRows=0;
    GLuint m_shaderProgram = 0;

    MeshHandle m_cubeMesh;
    MeshHandle m_transientSurface;
    MeshHandle m_sphereMesh;
    TextureHandle m_whiteTexture;  // 1x1 white pixel — the "no real texture" fallback, see DrawMesh

    GLuint m_poseBuffer=0,m_poseTexture=0;
    void UploadPosePalette(const std::vector<glm::mat4>&);
    GLint m_uModel = -1,m_uSkinned=-1,m_uBones=-1;
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
    // Milestone 15: which shadow slot (see src/Light.h) each dynamic
    // light uses, or -1 for none — the main shader uses this to select
    // between the fixed vTorchLightSpacePos/vShipLightSpacePos varyings
    // (see Renderer.cpp's fragment shader) when applying this light's own
    // shadow factor.
    GLint m_uDynamicLightShadowIndex[kMaxDynamicLights];

    CategoryMask m_renderMask=kAllCategories;
    unsigned m_renderLayer=0;
    Frustum m_frustum;
    bool m_cullingEnabled=true;
    GLuint m_particleProgram=0,m_particleVao=0,m_particleVbo=0;
    struct ParticleVertex {glm::vec3 position;glm::vec2 uv;glm::vec4 color;};
    std::vector<ParticleVertex> m_particleVertices;
    std::vector<std::size_t> m_particleOrder;
    glm::mat4 m_view{1.0f};
    glm::mat4 m_projection{1.0f};

    // --- Milestone 15: shadow mapping state ---
    //
    // One depth-only shader (separate from the main lit-mesh shader and
    // the UI shader — see Renderer.cpp) plus one FBO/depth-texture pair
    // PER shadow slot (kShadowMapCount, src/Light.h) — created once in
    // Init, reused every frame, never allocated/freed per-draw or per-
    // light. `m_shadowLightSpaceMatrix` is cached by BeginShadowPass and
    // read by every subsequent normal-mode DrawMesh call this same frame
    // (see DrawMesh's own comment).
    GLuint m_shadowShaderProgram = 0;
    GLint m_uShadowModel = -1,m_uShadowSkinned=-1,m_uShadowBones=-1;
    GLint m_uShadowLightViewProj = -1;
    GLuint m_shadowFbo[kShadowMapCount] = {0, 0, 0};
    GLuint m_shadowMapTexture[kShadowMapCount] = {0, 0, 0};
    glm::mat4 m_shadowLightSpaceMatrix[kShadowMapCount] = {glm::mat4(1.0f), glm::mat4(1.0f),
                                                            glm::mat4(1.0f)};
    // Main shader's own shadow-sampling uniform locations — one mat4 +
    // one sampler2D location PER SLOT, fetched once in Init (see the
    // per-dynamic-light-field locations above for why these can't be a
    // single cached array location).
    GLint m_uLightSpaceMatrix[kShadowMapCount] = {-1, -1, -1};
    GLint m_uShadowMapSampler[kShadowMapCount] = {-1, -1, -1};
    bool m_shadowPassActive = false;
    int m_currentShadowSlot = -1;

    // --- Milestone 13: UI overlay state ---
    GLuint m_uiShaderProgram = 0;
    GLuint m_textVao=0,m_textVbo=0;
    GLint m_uiUTextVertices=-1;
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
    unsigned m_uiDrawCalls=0;

    // --- Milestone 30: debug lines + stats ---
    GLuint m_debugShaderProgram = 0;
    GLuint m_debugVao = 0;
    GLuint m_debugVbo = 0;
    GLint m_debugUViewProjection = -1;
    RenderStats m_stats;

    mutable TextEngine m_textEngine;
    std::map<std::string,std::shared_ptr<const TextFont>> m_uiFonts;
    std::string m_defaultUIFont;
    std::vector<std::shared_ptr<const TextFont>> m_textFonts;
    std::shared_ptr<const TextFont> m_defaultTextFont;
    struct TextPage {TextureHandle texture;int x=1,y=1,row=0;};
    struct AtlasGlyph {unsigned page;int x,y;TextRaster raster;};
    std::vector<TextPage> m_textPages;
    std::map<std::string,AtlasGlyph> m_atlasGlyphs;
    unsigned m_textAtlasGeneration=1;
    float m_fontPixelHeight=48;
    bool m_fontLoaded=false;
};

// Coarse timestamp intervals may nest; Renderer owns every GL query.
class RendererProfileScope {
public:
    RendererProfileScope(Renderer& r,const char* pass,std::uint64_t camera=0):renderer(r),token(r.BeginProfilePass(pass,camera)){}
    ~RendererProfileScope(){End();}
    void End(){if(token){renderer.EndProfilePass(token);token=0;}}
private:Renderer& renderer;unsigned token;
};
