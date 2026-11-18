#pragma once
// Explicit Vulkan renderer interface (branch Vulkan-Exp).
//
// Renderer passes written against this interface state what they draw in
// Vulkan's own terms: a pipeline created from a shader, vertex layout and
// blend mode; a pass with named attachments and load operations; a binding
// group naming every texture and buffer the shader reads; and a draw. Nothing
// is inferred from OpenGL-style global state and no OpenGL-named entry point
// is involved. See docs/VULKAN_NATIVE.md.
//
// Resources (textures, buffers, programs) are still created by the engine's
// existing resource code and are referred to here by their engine handles.
// Shaders are the same GLSL sources OpenGL compiles, so both backends remain
// comparable pixel for pixel.
//
// Every function is a no-op returning null/false when the Vulkan backend is
// not active, so callers only need `if (JudasVulkan::Active())` once.
#include <cstdint>

namespace JudasVulkan::Gfx {

using Handle = unsigned; // engine object name: texture, sampler, buffer or program

// ---- shaders ----
// A linked program viewed as an explicit object: a parameter block (the
// shader's loose uniforms, addressed by index instead of by name per draw),
// texture slots and buffer slots.
struct Shader;
struct VertexLayout;
struct Pipeline;
struct Transient {
    void *data = nullptr;       // write `size` bytes here
    std::uint64_t buffer = 0;   // opaque
    std::uint64_t serial = 0;   // opaque: the submission it was allocated in
    std::uint32_t offset = 0, size = 0;
    explicit operator bool() const { return data != nullptr; }
};
enum class Format : std::uint8_t { Float2, Float3, Float4, UInt4 };
enum class StreamKind : std::uint8_t {
    PerVertex,
    PerInstance,
    ConstantFloat, // every vertex reads (0,0,0,1): the attribute has no array
    ConstantUInt,  // every vertex reads (0,0,0,0)
};
struct VertexAttribute {
    std::uint8_t location = 0, stream = 0;
    Format format = Format::Float4;
    std::uint16_t offset = 0;
};
struct VertexLayoutDesc {
    VertexAttribute attributes[16];
    std::uint32_t attributeCount = 0;
    struct Stream { StreamKind kind = StreamKind::PerVertex; std::uint16_t stride = 0; } streams[4];
    std::uint32_t streamCount = 0;
};
enum class Blend : std::uint8_t { Opaque, Alpha }; // Alpha: src_alpha, one_minus_src_alpha
struct PipelineDesc {
    Shader *shader = nullptr;
    VertexLayout *layout = nullptr;
    Blend blend = Blend::Opaque;
};
enum class TargetFormat : std::uint8_t { None, Rgba8, Rgba16F };
struct PassDesc {
    // Depth-only pass into one layer of a depth texture array.
    Handle depthTexture = 0;
    std::uint32_t depthLayer = 0;
    bool clearDepth = true;
    float clearDepthValue = 1.0f;
};
struct TextureBinding { Handle texture = 0, sampler = 0; }; // texture 0: a neutral placeholder
struct BufferBinding {
    Handle buffer = 0;      // engine buffer, or
    Transient transient;    // transient data; neither: a block of zeros
    std::uint32_t offset = 0, size = 0; // range within `buffer` (size 0 = whole buffer)
};
struct Bindings {
    static constexpr int kTextures = 24, kBuffers = 8;
    TextureBinding textures[kTextures];
    BufferBinding buffers[kBuffers];
};
struct VertexSource {
    Handle buffer = 0;   // engine buffer, or
    Transient transient; // transient data (ignored for constant streams)
    std::uint32_t offset = 0;
};
struct DrawDesc {
    Pipeline *pipeline = nullptr;
    const Bindings *bindings = nullptr;
    VertexSource streams[4];
    Handle indexBuffer = 0; // 32-bit indices; 0 = non-indexed
    std::uint32_t first = 0, count = 0, instances = 1;
    bool cull = true;            // back faces
    bool frontCounterClockwise = true; // OpenGL window convention, as the engine computes it
    bool depthTest = true, depthWrite = true; // compare: less
};
struct Statistics {
    std::uint64_t draws = 0, bindingGroupsBuilt = 0, bindingGroupsReused = 0, pipelinesCreated = 0, passes = 0;
};

#if defined(JUDAS_VULKAN)
Shader *AdoptShader(Handle program);
void ReleaseShader(Shader *shader);
int Param(const Shader *shader, const char *name);       // -1 when the shader has no such parameter
int TextureSlot(const Shader *shader, const char *name); // index into Bindings::textures, -1 if absent
int BufferSlot(const Shader *shader, const char *name);  // index into Bindings::buffers, -1 if absent
void SetInt(Shader *shader, int param, int value);
void SetFloats(Shader *shader, int param, const float *values, unsigned components, unsigned count = 1);
void SetMat3(Shader *shader, int param, const float *columnMajor);
void SetMat4(Shader *shader, int param, const float *columnMajor);

// ---- transient data ----
// Memory valid for the current frame's submission: per-draw tables (bone
// palettes, instance arrays) are written here and bound by offset.
Transient AllocateTransient(std::uint32_t bytes);

// ---- vertex layouts ----
VertexLayout *CreateVertexLayout(const VertexLayoutDesc &desc);

// ---- pipelines ----
// Triangle lists. The Vulkan pipeline for a set of attachment formats is
// created by Prewarm or at the first draw into such a pass.
Pipeline *CreatePipeline(const PipelineDesc &desc);
void Prewarm(Pipeline *pipeline, TargetFormat color, bool depth);

// ---- passes ----
// Begins an explicit pass covering the whole attachment (viewport included).
bool BeginPass(const PassDesc &desc);
void EndPass();
// Draws outside an explicit pass go to the frame's current colour target and
// viewport. Those are still selected by frame code that has not been migrated
// to this interface yet.

// ---- bindings ----

// ---- draws ----
void Draw(const DrawDesc &draw);

Statistics Stats();
#else
inline Shader *AdoptShader(Handle) { return nullptr; }
inline void ReleaseShader(Shader *) {}
inline int Param(const Shader *, const char *) { return -1; }
inline int TextureSlot(const Shader *, const char *) { return -1; }
inline int BufferSlot(const Shader *, const char *) { return -1; }
inline void SetInt(Shader *, int, int) {}
inline void SetFloats(Shader *, int, const float *, unsigned, unsigned = 1) {}
inline void SetMat3(Shader *, int, const float *) {}
inline void SetMat4(Shader *, int, const float *) {}
inline Transient AllocateTransient(std::uint32_t) { return {}; }
inline VertexLayout *CreateVertexLayout(const VertexLayoutDesc &) { return nullptr; }
inline Pipeline *CreatePipeline(const PipelineDesc &) { return nullptr; }
inline void Prewarm(Pipeline *, TargetFormat, bool) {}
inline bool BeginPass(const PassDesc &) { return false; }
inline void EndPass() {}
inline void Draw(const DrawDesc &) {}
inline Statistics Stats() { return {}; }
#endif

} // namespace JudasVulkan::Gfx
