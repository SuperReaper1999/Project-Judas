// Vulkan backend: fixed-function state, queries about that state, timestamp
// queries, sync objects and explicit flushes.
#include "VkCore.h"

#include <algorithm>
#include <cstdio>

namespace JudasVulkan::detail {
namespace {

bool *Capability(GLenum cap) {
    GlState &gl = g->gl;
    switch (cap) {
    case GL_DEPTH_TEST: return &gl.depthTest;
    case GL_BLEND: return &gl.blend;
    case GL_CULL_FACE: return &gl.cull;
    case GL_SCISSOR_TEST: return &gl.scissorTest;
    case GL_FRAMEBUFFER_SRGB: return &gl.framebufferSrgb;
    case GL_DEPTH_CLAMP: return &gl.depthClamp;
    case GL_STENCIL_TEST: return &gl.stencilTest;
    case GL_MULTISAMPLE: return &gl.multisample;
    case GL_POLYGON_OFFSET_FILL: return &gl.polygonOffsetFill;
    case GL_PRIMITIVE_RESTART: case GL_PRIMITIVE_RESTART_FIXED_INDEX: return &gl.primitiveRestart;
    case GL_PROGRAM_POINT_SIZE: return &gl.programPointSize;
    case GL_DITHER: return &gl.dither;
    case GL_LINE_SMOOTH: return &gl.lineSmooth;
    case GL_TEXTURE_CUBE_MAP_SEAMLESS: return &gl.seamlessCube;
    case GL_DEBUG_OUTPUT: return &gl.debugOutput;
    case GL_DEBUG_OUTPUT_SYNCHRONOUS: return &gl.debugSync;
    case GL_RASTERIZER_DISCARD: return &gl.rasterizerDiscard;
    case GL_SAMPLE_ALPHA_TO_COVERAGE: return &gl.sampleAlpha;
    default: return nullptr;
    }
}
void GLAD_API_PTR Enable(GLenum cap) {
    if (bool *value = Capability(cap)) *value = true;
    if (cap == GL_FRAMEBUFFER_SRGB) {
        static bool warned = false;
        if (!warned) { warned = true; Warn("GL_FRAMEBUFFER_SRGB encoding is not implemented by the Vulkan backend"); }
    }
}
void GLAD_API_PTR Disable(GLenum cap) { if (bool *value = Capability(cap)) *value = false; }
GLboolean GLAD_API_PTR IsEnabled(GLenum cap) {
    const bool *value = Capability(cap);
    return value && *value;
}
void GLAD_API_PTR DepthFunc(GLenum function) { g->gl.depthFunc = function; }
void GLAD_API_PTR DepthMask(GLboolean flag) { g->gl.depthMask = flag; }
void GLAD_API_PTR DepthRange(GLdouble, GLdouble) {}
void GLAD_API_PTR DepthRangef(GLfloat, GLfloat) {}
void GLAD_API_PTR BlendFunc(GLenum source, GLenum destination) {
    g->gl.blendSrcRgb = g->gl.blendSrcAlpha = source;
    g->gl.blendDstRgb = g->gl.blendDstAlpha = destination;
}
void GLAD_API_PTR BlendFuncSeparate(GLenum sourceRgb, GLenum destinationRgb, GLenum sourceAlpha, GLenum destinationAlpha) {
    g->gl.blendSrcRgb = sourceRgb; g->gl.blendDstRgb = destinationRgb;
    g->gl.blendSrcAlpha = sourceAlpha; g->gl.blendDstAlpha = destinationAlpha;
}
void GLAD_API_PTR BlendEquation(GLenum mode) { g->gl.blendEqRgb = g->gl.blendEqAlpha = mode; }
void GLAD_API_PTR BlendEquationSeparate(GLenum rgb, GLenum alpha) { g->gl.blendEqRgb = rgb; g->gl.blendEqAlpha = alpha; }
void GLAD_API_PTR BlendColor(GLfloat, GLfloat, GLfloat, GLfloat) {}
void GLAD_API_PTR CullFace(GLenum mode) { g->gl.cullMode = mode; }
void GLAD_API_PTR FrontFace(GLenum mode) { g->gl.frontFace = mode; }
void GLAD_API_PTR Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    g->gl.viewport[0] = x; g->gl.viewport[1] = y; g->gl.viewport[2] = width; g->gl.viewport[3] = height;
}
void GLAD_API_PTR Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    g->gl.scissor[0] = x; g->gl.scissor[1] = y; g->gl.scissor[2] = width; g->gl.scissor[3] = height;
}
void GLAD_API_PTR ClearColor(GLfloat r, GLfloat gValue, GLfloat b, GLfloat a) {
    g->gl.clearColor[0] = r; g->gl.clearColor[1] = gValue; g->gl.clearColor[2] = b; g->gl.clearColor[3] = a;
}
void GLAD_API_PTR ClearDepth(GLdouble depth) { g->gl.clearDepth = depth; }
void GLAD_API_PTR ClearDepthf(GLfloat depth) { g->gl.clearDepth = depth; }
void GLAD_API_PTR ClearStencil(GLint value) { g->gl.clearStencil = value; }
void GLAD_API_PTR ColorMask(GLboolean r, GLboolean gValue, GLboolean b, GLboolean a) {
    g->gl.colorMask[0] = r; g->gl.colorMask[1] = gValue; g->gl.colorMask[2] = b; g->gl.colorMask[3] = a;
}
void GLAD_API_PTR PolygonMode(GLenum, GLenum mode) { g->gl.polygonMode = mode; }
void GLAD_API_PTR PolygonOffset(GLfloat factor, GLfloat units) { g->gl.polygonOffsetFactor = factor; g->gl.polygonOffsetUnits = units; }
void GLAD_API_PTR LineWidth(GLfloat width) { g->gl.lineWidth = width; }
void GLAD_API_PTR PointSize(GLfloat) {}
void GLAD_API_PTR Hint(GLenum, GLenum) {}
void GLAD_API_PTR ClipControl(GLenum origin, GLenum depth) {
    if (origin != 0x8CA1 || depth != 0x935E) Warn("glClipControl: only the OpenGL default clip convention is implemented");
}
void GLAD_API_PTR StencilMask(GLuint) {}
void GLAD_API_PTR StencilFunc(GLenum, GLint, GLuint) {}
void GLAD_API_PTR StencilOp(GLenum, GLenum, GLenum) {}
void GLAD_API_PTR PixelStorei(GLenum pname, GLint value) {
    GlState &gl = g->gl;
    switch (pname) {
    case GL_UNPACK_ALIGNMENT: gl.unpackAlignment = value; break;
    case GL_UNPACK_ROW_LENGTH: gl.unpackRowLength = value; break;
    case GL_UNPACK_SKIP_ROWS: gl.unpackSkipRows = value; break;
    case GL_UNPACK_SKIP_PIXELS: gl.unpackSkipPixels = value; break;
    case GL_UNPACK_IMAGE_HEIGHT: gl.unpackImageHeight = value; break;
    case GL_UNPACK_SKIP_IMAGES: gl.unpackSkipImages = value; break;
    case GL_PACK_ALIGNMENT: gl.packAlignment = value; break;
    case GL_PACK_ROW_LENGTH: gl.packRowLength = value; break;
    case GL_PACK_SKIP_ROWS: gl.packSkipRows = value; break;
    case GL_PACK_SKIP_PIXELS: gl.packSkipPixels = value; break;
    default: break;
    }
}

GLuint BoundTexture(int slot) { return g->gl.textures[std::size_t(g->gl.activeUnit)][std::size_t(slot)]; }

// Returns the number of values written, 0 when `pname` is unknown.
int GetIntegers(GLenum pname, GLint64 *v) {
    const GlState &gl = g->gl;
    const auto &limits = g->properties.limits;
    switch (pname) {
    case GL_VIEWPORT: for (int i = 0; i < 4; ++i) v[i] = gl.viewport[i]; return 4;
    case GL_SCISSOR_BOX: for (int i = 0; i < 4; ++i) v[i] = gl.scissor[i]; return 4;
    case GL_MAJOR_VERSION: v[0] = g->glMajor; return 1;
    case GL_MINOR_VERSION: v[0] = g->glMinor; return 1;
    case GL_NUM_EXTENSIONS: v[0] = GLint64(g->extensions.size()); return 1;
    case GL_CONTEXT_FLAGS: v[0] = 0; return 1;
    case GL_CONTEXT_PROFILE_MASK: v[0] = GL_CONTEXT_CORE_PROFILE_BIT; return 1;
    case GL_MAX_TEXTURE_SIZE: v[0] = std::min<GLint64>(limits.maxImageDimension2D, 16384); return 1;
    case GL_MAX_3D_TEXTURE_SIZE: v[0] = std::min<GLint64>(limits.maxImageDimension3D, 2048); return 1;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE: v[0] = std::min<GLint64>(limits.maxImageDimensionCube, 16384); return 1;
    case GL_MAX_ARRAY_TEXTURE_LAYERS: v[0] = std::min<GLint64>(limits.maxImageArrayLayers, 2048); return 1;
    case GL_MAX_RENDERBUFFER_SIZE: v[0] = std::min<GLint64>(limits.maxFramebufferWidth, 16384); return 1;
    case GL_MAX_TEXTURE_IMAGE_UNITS: v[0] = 32; return 1;
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS: v[0] = 32; return 1;
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: v[0] = kTextureUnits; return 1;
    case GL_MAX_VERTEX_ATTRIBS: v[0] = 16; return 1;
    case GL_MAX_DRAW_BUFFERS: case GL_MAX_COLOR_ATTACHMENTS: v[0] = std::min<GLint64>(limits.maxColorAttachments, 8); return 1;
    case GL_MAX_UNIFORM_BUFFER_BINDINGS: v[0] = 36; return 1;
    case GL_MAX_UNIFORM_BLOCK_SIZE: v[0] = std::min<GLint64>(limits.maxUniformBufferRange, 65536); return 1;
    case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT: v[0] = GLint64(g->bufferAlignment); return 1;
    case GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS: v[0] = 16; return 1;
    case GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT: v[0] = GLint64(g->bufferAlignment); return 1;
    case GL_MAX_SHADER_STORAGE_BLOCK_SIZE: v[0] = GLint64(std::min<std::uint64_t>(limits.maxStorageBufferRange, 1ull << 30)); return 1;
    case GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS: v[0] = limits.maxComputeWorkGroupInvocations; return 1;
    case GL_MAX_COMPUTE_SHARED_MEMORY_SIZE: v[0] = std::min<GLint64>(limits.maxComputeSharedMemorySize, 1 << 30); return 1;
    case GL_MAX_TEXTURE_BUFFER_SIZE: v[0] = std::min<GLint64>(limits.maxTexelBufferElements, 1 << 27); return 1;
    case GL_MAX_VIEWPORT_DIMS: v[0] = limits.maxViewportDimensions[0]; v[1] = limits.maxViewportDimensions[1]; return 2;
    case GL_MAX_ELEMENTS_VERTICES: case GL_MAX_ELEMENTS_INDICES: v[0] = 1 << 24; return 1;
    case GL_MAX_SAMPLES: v[0] = 1; return 1;
    case GL_MAX_VARYING_COMPONENTS: v[0] = 124; return 1;
    case GL_MAX_VERTEX_UNIFORM_COMPONENTS: case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS: v[0] = 16384; return 1;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS: v[0] = g->bcSupported ? 8 : 0; return 1;
    case GL_COMPRESSED_TEXTURE_FORMATS:
        if (!g->bcSupported) return 1;
        { static const GLint64 formats[] = {0x83F0, 0x83F1, 0x83F2, 0x83F3, 0x8C4C, 0x8C4D, 0x8C4E, 0x8C4F};
          for (int i = 0; i < 8; ++i) v[i] = formats[i]; }
        return 8;
    case GL_ARRAY_BUFFER_BINDING: v[0] = gl.arrayBuffer; return 1;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING: v[0] = g->vertexArrays[gl.vao].elementBuffer; return 1;
    case GL_UNIFORM_BUFFER_BINDING: v[0] = gl.uniformBuffer; return 1;
    case GL_COPY_READ_BUFFER_BINDING: v[0] = gl.copyRead; return 1;
    case GL_COPY_WRITE_BUFFER_BINDING: v[0] = gl.copyWrite; return 1;
    case GL_PIXEL_PACK_BUFFER_BINDING: v[0] = gl.pixelPack; return 1;
    case GL_PIXEL_UNPACK_BUFFER_BINDING: v[0] = gl.pixelUnpack; return 1;
    case GL_TEXTURE_BUFFER_BINDING: v[0] = gl.textureBuffer; return 1;
    case GL_SHADER_STORAGE_BUFFER_BINDING: v[0] = gl.storageBuffer; return 1;
    case GL_DRAW_INDIRECT_BUFFER_BINDING: v[0] = gl.indirectBuffer; return 1;
    case GL_QUERY_BUFFER_BINDING: v[0] = gl.queryBuffer; return 1;
    case GL_DISPATCH_INDIRECT_BUFFER_BINDING: v[0] = gl.dispatchIndirect; return 1;
    case GL_VERTEX_ARRAY_BINDING: v[0] = gl.vao; return 1;
    case GL_CURRENT_PROGRAM: v[0] = gl.program; return 1;
    case GL_DRAW_FRAMEBUFFER_BINDING: v[0] = gl.drawFbo; return 1;
    case GL_READ_FRAMEBUFFER_BINDING: v[0] = gl.readFbo; return 1;
    case GL_RENDERBUFFER_BINDING: v[0] = gl.renderbuffer; return 1;
    case GL_ACTIVE_TEXTURE: v[0] = GL_TEXTURE0 + gl.activeUnit; return 1;
    case GL_TEXTURE_BINDING_2D: v[0] = BoundTexture(Slot2D); return 1;
    case GL_TEXTURE_BINDING_2D_ARRAY: v[0] = BoundTexture(Slot2DArray); return 1;
    case GL_TEXTURE_BINDING_CUBE_MAP: v[0] = BoundTexture(SlotCube); return 1;
    case GL_TEXTURE_BINDING_CUBE_MAP_ARRAY: v[0] = BoundTexture(SlotCubeArray); return 1;
    case GL_TEXTURE_BINDING_3D: v[0] = BoundTexture(Slot3D); return 1;
    case GL_TEXTURE_BINDING_BUFFER: v[0] = BoundTexture(SlotBuffer); return 1;
    case GL_SAMPLER_BINDING: v[0] = gl.samplers[std::size_t(gl.activeUnit)]; return 1;
    case GL_PACK_ALIGNMENT: v[0] = gl.packAlignment; return 1;
    case GL_PACK_ROW_LENGTH: v[0] = gl.packRowLength; return 1;
    case GL_PACK_SKIP_ROWS: v[0] = gl.packSkipRows; return 1;
    case GL_PACK_SKIP_PIXELS: v[0] = gl.packSkipPixels; return 1;
    case GL_UNPACK_ALIGNMENT: v[0] = gl.unpackAlignment; return 1;
    case GL_UNPACK_ROW_LENGTH: v[0] = gl.unpackRowLength; return 1;
    case GL_UNPACK_SKIP_ROWS: v[0] = gl.unpackSkipRows; return 1;
    case GL_UNPACK_SKIP_PIXELS: v[0] = gl.unpackSkipPixels; return 1;
    case GL_UNPACK_IMAGE_HEIGHT: v[0] = gl.unpackImageHeight; return 1;
    case GL_UNPACK_SKIP_IMAGES: v[0] = gl.unpackSkipImages; return 1;
    case GL_DEPTH_FUNC: v[0] = gl.depthFunc; return 1;
    case GL_DEPTH_WRITEMASK: v[0] = gl.depthMask; return 1;
    case GL_COLOR_WRITEMASK: for (int i = 0; i < 4; ++i) v[i] = gl.colorMask[i]; return 4;
    case GL_BLEND_SRC_RGB: v[0] = gl.blendSrcRgb; return 1;
    case GL_BLEND_DST_RGB: v[0] = gl.blendDstRgb; return 1;
    case GL_BLEND_SRC_ALPHA: v[0] = gl.blendSrcAlpha; return 1;
    case GL_BLEND_DST_ALPHA: v[0] = gl.blendDstAlpha; return 1;
    case GL_BLEND_EQUATION_RGB: v[0] = gl.blendEqRgb; return 1;
    case GL_BLEND_EQUATION_ALPHA: v[0] = gl.blendEqAlpha; return 1;
    case GL_CULL_FACE_MODE: v[0] = gl.cullMode; return 1;
    case GL_FRONT_FACE: v[0] = gl.frontFace; return 1;
    case GL_POLYGON_MODE: v[0] = v[1] = gl.polygonMode; return 2;
    case GL_DEPTH_TEST: case GL_BLEND: case GL_CULL_FACE: case GL_SCISSOR_TEST: case GL_STENCIL_TEST:
    case GL_FRAMEBUFFER_SRGB: case GL_DEPTH_CLAMP: case GL_POLYGON_OFFSET_FILL: case GL_PRIMITIVE_RESTART:
    case GL_PROGRAM_POINT_SIZE: case GL_MULTISAMPLE:
        v[0] = IsEnabled(pname); return 1;
    case GL_DRAW_BUFFER: v[0] = gl.drawFbo ? GLint64(g->framebuffers[gl.drawFbo].drawBuffers[0]) : GL_BACK; return 1;
    case GL_READ_BUFFER: v[0] = gl.readFbo ? GLint64(g->framebuffers[gl.readFbo].readBuffer) : GL_BACK; return 1;
    case GL_TIMESTAMP: v[0] = 0; return 1;
    case GL_SUBPIXEL_BITS: v[0] = limits.subPixelPrecisionBits; return 1;
    case 0x0D57: v[0] = 0; return 1;  // GL_STENCIL_BITS (compatibility query)
    case 0x0D56: v[0] = 24; return 1; // GL_DEPTH_BITS (compatibility query)
    case GL_SAMPLES: case GL_SAMPLE_BUFFERS: v[0] = 0; return 1;
    case 0x935C: v[0] = 0x8CA1; return 1; // GL_CLIP_ORIGIN -> GL_LOWER_LEFT
    case 0x935D: v[0] = 0x935E; return 1; // GL_CLIP_DEPTH_MODE -> GL_NEGATIVE_ONE_TO_ONE
    case 0x8256: v[0] = 0x8261; return 1; // GL_RESET_NOTIFICATION_STRATEGY -> GL_NO_RESET_NOTIFICATION
    case 0x84FF: v[0] = GLint64(g->anisotropy ? limits.maxSamplerAnisotropy : 1.0f); return 1;
    default: return 0;
    }
}
void GLAD_API_PTR GetIntegerv(GLenum pname, GLint *values) {
    GLint64 wide[16] = {};
    const int count = GetIntegers(pname, wide);
    if (!count) {
        static unsigned warned = 0;
        if (warned++ < 16) Warn("glGetIntegerv(0x%x) is not implemented", pname);
        if (values) *values = 0;
        return;
    }
    for (int i = 0; i < count; ++i) values[i] = GLint(wide[i]);
}
void GLAD_API_PTR GetInteger64v(GLenum pname, GLint64 *values) {
    GLint64 wide[16] = {};
    const int count = GetIntegers(pname, wide);
    for (int i = 0; i < std::max(count, 1); ++i) values[i] = wide[i];
}
void GLAD_API_PTR GetFloatv(GLenum pname, GLfloat *values) {
    if (pname == GL_COLOR_CLEAR_VALUE) { std::memcpy(values, g->gl.clearColor, sizeof(float) * 4); return; }
    if (pname == GL_DEPTH_CLEAR_VALUE) { *values = float(g->gl.clearDepth); return; }
    if (pname == GL_LINE_WIDTH) { *values = g->gl.lineWidth; return; }
    if (pname == GL_POLYGON_OFFSET_FACTOR) { *values = g->gl.polygonOffsetFactor; return; }
    if (pname == GL_POLYGON_OFFSET_UNITS) { *values = g->gl.polygonOffsetUnits; return; }
    GLint64 wide[16] = {};
    const int count = GetIntegers(pname, wide);
    for (int i = 0; i < std::max(count, 1); ++i) values[i] = float(wide[i]);
}
void GLAD_API_PTR GetBooleanv(GLenum pname, GLboolean *values) {
    GLint64 wide[16] = {};
    const int count = GetIntegers(pname, wide);
    for (int i = 0; i < std::max(count, 1); ++i) values[i] = wide[i] != 0;
}
void GLAD_API_PTR GetIntegeri_v(GLenum pname, GLuint index, GLint *value) {
    const auto &limits = g->properties.limits;
    *value = 0;
    if (pname == GL_MAX_COMPUTE_WORK_GROUP_COUNT && index < 3) *value = GLint(std::min<std::uint32_t>(limits.maxComputeWorkGroupCount[index], 65535));
    else if (pname == GL_MAX_COMPUTE_WORK_GROUP_SIZE && index < 3) *value = GLint(limits.maxComputeWorkGroupSize[index]);
    else if (pname == GL_SAMPLER_BINDING && index < kTextureUnits) *value = GLint(g->gl.samplers[index]);
    else if (pname == GL_TEXTURE_BINDING_2D && index < kTextureUnits) *value = GLint(g->gl.textures[index][Slot2D]);
    else if (pname == GL_TEXTURE_BINDING_2D_ARRAY && index < kTextureUnits) *value = GLint(g->gl.textures[index][Slot2DArray]);
    else if (pname == GL_TEXTURE_BINDING_3D && index < kTextureUnits) *value = GLint(g->gl.textures[index][Slot3D]);
    else if (pname == GL_TEXTURE_BINDING_CUBE_MAP && index < kTextureUnits) *value = GLint(g->gl.textures[index][SlotCube]);
    else if (pname == GL_TRANSFORM_FEEDBACK_BUFFER_BINDING && index == 0) *value = GLint(g->gl.feedbackBinding.buffer);
    else if (pname == GL_UNIFORM_BUFFER_BINDING && index < g->gl.uniformBindings.size()) *value = GLint(g->gl.uniformBindings[index].buffer);
    else if (pname == GL_SHADER_STORAGE_BUFFER_BINDING && index < g->gl.storageBindings.size()) *value = GLint(g->gl.storageBindings[index].buffer);
    else {
        // Non-indexed state queried through the indexed entry point.
        GLint64 wide[16] = {};
        if (GetIntegers(pname, wide) > 0) *value = GLint(wide[0]);
        else Warn("glGetIntegeri_v(0x%x, %u) is not implemented", pname, index);
    }
}
const GLubyte *GLAD_API_PTR GetString(GLenum name) {
    switch (name) {
    case GL_VERSION: return reinterpret_cast<const GLubyte *>(g->versionString.c_str());
    case GL_RENDERER: return reinterpret_cast<const GLubyte *>(g->rendererString.c_str());
    case GL_VENDOR: return reinterpret_cast<const GLubyte *>(g->vendorString.c_str());
    case GL_SHADING_LANGUAGE_VERSION: return reinterpret_cast<const GLubyte *>(g->glMajor >= 4 ? "4.60" : "3.30");
    default: return nullptr;
    }
}
const GLubyte *GLAD_API_PTR GetStringi(GLenum name, GLuint index) {
    if (name != GL_EXTENSIONS || index >= g->extensions.size()) return nullptr;
    return reinterpret_cast<const GLubyte *>(g->extensions[index].c_str());
}
GLenum GLAD_API_PTR GetError() { return GL_NO_ERROR; }
void GLAD_API_PTR FlushCommands() { Flush(false); }
void GLAD_API_PTR Finish() { Flush(true); }

// ---- timestamp queries ----
void GLAD_API_PTR GenQueries(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) names[i] = AllocateName(g->queries);
}
void GLAD_API_PTR DeleteQueries(GLsizei count, const GLuint *names) {
    for (GLsizei i = 0; i < count; ++i)
        if (Lookup(g->queries, names[i])) g->queries[names[i]] = Query{};
}
GLboolean GLAD_API_PTR IsQuery(GLuint name) { return Lookup(g->queries, name) != nullptr; }
std::uint32_t TimestampSlot() {
    std::uint32_t slot = g->queryCursor;
    for (std::uint32_t attempt = 0; attempt < Context::kQuerySlots; ++attempt) {
        slot = (g->queryCursor + attempt) % Context::kQuerySlots;
        if (g->querySlotSerial[slot] <= g->completedSerial) break;
    }
    g->queryCursor = (slot + 1) % Context::kQuerySlots;
    vkResetQueryPool(g->device, g->queryPool, slot, 1);
    vkCmdWriteTimestamp(g->cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, g->queryPool, slot);
    g->querySlotSerial[slot] = g->serial;
    return slot;
}
void GLAD_API_PTR QueryCounter(GLuint name, GLenum target) {
    Query *query = Lookup(g->queries, name);
    if (!query || target != GL_TIMESTAMP) return;
    const std::uint32_t slot = TimestampSlot();
    query->elapsed = false;
    query->slot = slot;
    query->serial = g->serial;
    query->issued = true;
}
// GL_TIME_ELAPSED is the interval between two timestamps in command order.
void GLAD_API_PTR BeginQuery(GLenum target, GLuint name) {
    Query *query = Lookup(g->queries, name);
    if (!query || target != GL_TIME_ELAPSED) return;
    query->beginSlot = TimestampSlot();
    query->elapsed = true;
    query->issued = false;
    g->gl.elapsedQuery = name;
}
void GLAD_API_PTR EndQuery(GLenum target) {
    Query *query = Lookup(g->queries, g->gl.elapsedQuery);
    if (!query || target != GL_TIME_ELAPSED) return;
    query->slot = TimestampSlot();
    query->serial = g->serial;
    query->issued = true;
    g->gl.elapsedQuery = 0;
}
bool QueryAvailable(const Query &query) {
    if (!query.issued) return false;
    if (query.serial > g->completedSerial) PollCompleted(false);
    return query.serial <= g->completedSerial;
}
std::uint64_t QueryResult(Query &query) {
    if (!query.issued) return 0;
    if (query.serial > g->completedSerial) {
        if (query.serial >= g->serial) Flush(true);
        else PollCompleted(true, query.serial);
    }
    std::uint64_t ticks = 0, begin = 0;
    vkGetQueryPoolResults(g->device, g->queryPool, query.slot, 1, sizeof(ticks), &ticks, sizeof(ticks), VK_QUERY_RESULT_64_BIT);
    if (query.elapsed) {
        vkGetQueryPoolResults(g->device, g->queryPool, query.beginSlot, 1, sizeof(begin), &begin, sizeof(begin), VK_QUERY_RESULT_64_BIT);
        ticks = ticks >= begin ? ticks - begin : 0;
    }
    return std::uint64_t(double(ticks) * double(g->properties.limits.timestampPeriod));
}
void GLAD_API_PTR GetQueryObjectui64v(GLuint name, GLenum pname, GLuint64 *value) {
    Query *query = Lookup(g->queries, name);
    if (!query || !value) return;
    *value = pname == GL_QUERY_RESULT_AVAILABLE ? GLuint64(QueryAvailable(*query)) : QueryResult(*query);
}
void GLAD_API_PTR GetQueryObjecti64v(GLuint name, GLenum pname, GLint64 *value) {
    GLuint64 result = 0;
    GetQueryObjectui64v(name, pname, &result);
    if (value) *value = GLint64(result);
}
void GLAD_API_PTR GetQueryObjectiv(GLuint name, GLenum pname, GLint *value) {
    GLuint64 result = 0;
    GetQueryObjectui64v(name, pname, &result);
    if (value) *value = GLint(result);
}
void GLAD_API_PTR GetQueryObjectuiv(GLuint name, GLenum pname, GLuint *value) {
    GLuint64 result = 0;
    GetQueryObjectui64v(name, pname, &result);
    if (value) *value = GLuint(result);
}
void GLAD_API_PTR GetQueryiv(GLenum target, GLenum pname, GLint *value) {
    if (!value) return;
    *value = 0;
    if (target == GL_TIMESTAMP && pname == GL_QUERY_COUNTER_BITS) {
        std::uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(g->physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(g->physical, &count, families.data());
        *value = g->queueFamily < count ? GLint(std::min<std::uint32_t>(families[g->queueFamily].timestampValidBits, 64)) : 0;
    }
}

// ---- sync objects ----
GLsync GLAD_API_PTR FenceSync(GLenum, GLbitfield) {
    auto *sync = new Sync;
    sync->serial = g->serial;
    return reinterpret_cast<GLsync>(sync);
}
GLboolean GLAD_API_PTR IsSync(GLsync sync) { return sync != nullptr; }
void GLAD_API_PTR DeleteSync(GLsync sync) { delete reinterpret_cast<Sync *>(sync); }
GLenum GLAD_API_PTR ClientWaitSync(GLsync handle, GLbitfield flags, GLuint64 timeout) {
    auto *sync = reinterpret_cast<Sync *>(handle);
    if (!sync) return GL_WAIT_FAILED;
    PollCompleted(false);
    if (sync->serial <= g->completedSerial) return GL_ALREADY_SIGNALED;
    if (sync->serial >= g->serial) {
        if (!(flags & GL_SYNC_FLUSH_COMMANDS_BIT) && timeout == 0) return GL_TIMEOUT_EXPIRED;
        Flush(false);
    }
    if (timeout == 0) return sync->serial <= g->completedSerial ? GL_CONDITION_SATISFIED : GL_TIMEOUT_EXPIRED;
    PollCompleted(true, sync->serial);
    return sync->serial <= g->completedSerial ? GL_CONDITION_SATISFIED : GL_TIMEOUT_EXPIRED;
}
void GLAD_API_PTR GetSynciv(GLsync handle, GLenum pname, GLsizei, GLsizei *length, GLint *values) {
    auto *sync = reinterpret_cast<Sync *>(handle);
    if (length) *length = 1;
    if (!sync || !values) return;
    PollCompleted(false);
    *values = pname == GL_SYNC_STATUS ? (sync->serial <= g->completedSerial ? GL_SIGNALED : GL_UNSIGNALED) : 0;
}

template <class F> void (*Erase(F function))(void) { return reinterpret_cast<void (*)(void)>(function); }
} // namespace

void InstallStateEntries(std::unordered_map<std::string, void (*)(void)> &table) {
    table["glEnable"] = Erase(&Enable);
    table["glDisable"] = Erase(&Disable);
    table["glIsEnabled"] = Erase(&IsEnabled);
    table["glDepthFunc"] = Erase(&DepthFunc);
    table["glDepthMask"] = Erase(&DepthMask);
    table["glDepthRange"] = Erase(&DepthRange);
    table["glDepthRangef"] = Erase(&DepthRangef);
    table["glBlendFunc"] = Erase(&BlendFunc);
    table["glBlendFuncSeparate"] = Erase(&BlendFuncSeparate);
    table["glBlendEquation"] = Erase(&BlendEquation);
    table["glBlendEquationSeparate"] = Erase(&BlendEquationSeparate);
    table["glBlendColor"] = Erase(&BlendColor);
    table["glCullFace"] = Erase(&CullFace);
    table["glFrontFace"] = Erase(&FrontFace);
    table["glViewport"] = Erase(&Viewport);
    table["glScissor"] = Erase(&Scissor);
    table["glClearColor"] = Erase(&ClearColor);
    table["glClearDepth"] = Erase(&ClearDepth);
    table["glClearDepthf"] = Erase(&ClearDepthf);
    table["glClearStencil"] = Erase(&ClearStencil);
    table["glColorMask"] = Erase(&ColorMask);
    table["glPolygonMode"] = Erase(&PolygonMode);
    table["glPolygonOffset"] = Erase(&PolygonOffset);
    table["glLineWidth"] = Erase(&LineWidth);
    table["glPointSize"] = Erase(&PointSize);
    table["glHint"] = Erase(&Hint);
    table["glStencilMask"] = Erase(&StencilMask);
    table["glStencilFunc"] = Erase(&StencilFunc);
    table["glStencilOp"] = Erase(&StencilOp);
    table["glPixelStorei"] = Erase(&PixelStorei);
    table["glGetIntegerv"] = Erase(&GetIntegerv);
    table["glGetInteger64v"] = Erase(&GetInteger64v);
    table["glGetFloatv"] = Erase(&GetFloatv);
    table["glGetBooleanv"] = Erase(&GetBooleanv);
    table["glGetIntegeri_v"] = Erase(&GetIntegeri_v);
    table["glGetString"] = Erase(&GetString);
    table["glGetStringi"] = Erase(&GetStringi);
    table["glGetError"] = Erase(&GetError);
    table["glFlush"] = Erase(&FlushCommands);
    table["glFinish"] = Erase(&Finish);
    table["glGenQueries"] = Erase(&GenQueries);
    table["glDeleteQueries"] = Erase(&DeleteQueries);
    table["glIsQuery"] = Erase(&IsQuery);
    table["glQueryCounter"] = Erase(&QueryCounter);
    table["glBeginQuery"] = Erase(&BeginQuery);
    table["glEndQuery"] = Erase(&EndQuery);
    table["glClipControl"] = Erase(&ClipControl);
    table["glGetQueryObjectui64v"] = Erase(&GetQueryObjectui64v);
    table["glGetQueryObjecti64v"] = Erase(&GetQueryObjecti64v);
    table["glGetQueryObjectiv"] = Erase(&GetQueryObjectiv);
    table["glGetQueryObjectuiv"] = Erase(&GetQueryObjectuiv);
    table["glGetQueryiv"] = Erase(&GetQueryiv);
    table["glFenceSync"] = Erase(&FenceSync);
    table["glIsSync"] = Erase(&IsSync);
    table["glDeleteSync"] = Erase(&DeleteSync);
    table["glClientWaitSync"] = Erase(&ClientWaitSync);
    table["glGetSynciv"] = Erase(&GetSynciv);
}

} // namespace JudasVulkan::detail
