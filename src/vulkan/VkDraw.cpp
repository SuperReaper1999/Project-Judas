// Vulkan backend: buffer objects, vertex arrays, render-pass management,
// pipeline/descriptor construction, draws, clears and compute dispatch.
#include <map>
#include "VkCore.h"

#include <algorithm>
#include <cstdio>

namespace JudasVulkan::detail {

std::size_t PipelineKeyHash::operator()(const PipelineKey &key) const {
    std::uint64_t hash = 1469598103934665603ull;
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(&key);
    for (std::size_t i = 0; i < sizeof(PipelineKey); ++i) hash = (hash ^ bytes[i]) * 1099511628211ull;
    return std::size_t(hash);
}

Buffer *GetBuffer(GLuint name) { return Lookup(g->buffers, name); }

GLuint *BufferBinding(GLenum target) {
    GlState &gl = g->gl;
    switch (target) {
    case GL_ARRAY_BUFFER: return &gl.arrayBuffer;
    case GL_ELEMENT_ARRAY_BUFFER: return &g->vertexArrays[gl.vao].elementBuffer;
    case GL_UNIFORM_BUFFER: return &gl.uniformBuffer;
    case GL_COPY_READ_BUFFER: return &gl.copyRead;
    case GL_COPY_WRITE_BUFFER: return &gl.copyWrite;
    case GL_PIXEL_PACK_BUFFER: return &gl.pixelPack;
    case GL_PIXEL_UNPACK_BUFFER: return &gl.pixelUnpack;
    case GL_TEXTURE_BUFFER: return &gl.textureBuffer;
    case GL_SHADER_STORAGE_BUFFER: return &gl.storageBuffer;
    case GL_DRAW_INDIRECT_BUFFER: return &gl.indirectBuffer;
    case GL_QUERY_BUFFER: return &gl.queryBuffer;
    case GL_DISPATCH_INDIRECT_BUFFER: return &gl.dispatchIndirect;
    case GL_ATOMIC_COUNTER_BUFFER: return &gl.atomicBuffer;
    case GL_TRANSFORM_FEEDBACK_BUFFER: return &gl.feedbackBuffer;
    default: return nullptr;
    }
}

namespace {

Buffer *BoundBuffer(GLenum target) {
    GLuint *binding = BufferBinding(target);
    return binding ? GetBuffer(*binding) : nullptr;
}

void DropTexelView(Buffer &buffer) {
    Retire(Retired::BufferViewKind, std::uint64_t(buffer.texelView), std::max(buffer.lastUse, g->serial));
    buffer.texelView = VK_NULL_HANDLE;
}

// GPU-owned per the application's own hint: *_COPY usage (the GL meaning is
// "written by the GL, read by the GL"), or immutable storage the CPU cannot map
// or update. Everything else stays in host memory, which the CPU reads fast.
bool GpuOwned(const Buffer &buffer) {
    if (buffer.storageFlags || buffer.persistent)
        return !(buffer.storageFlags & (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT |
                                        GL_MAP_PERSISTENT_BIT | GL_CLIENT_STORAGE_BIT));
    return buffer.usage == GL_STATIC_COPY || buffer.usage == GL_DYNAMIC_COPY || buffer.usage == GL_STREAM_COPY;
}

void ReplaceStorage(Buffer &buffer, VkDeviceSize size, bool copy) {
    // New storage means new VkBuffer/offset for every binding of this buffer;
    // a texel view of the old storage also changes descriptor contents.
    MarkDirty(DirtyBuffers | (buffer.texelView ? DirtyTextures : 0u));
    Range replacement = AllocateBuffer(size, GpuOwned(buffer));
    if (copy && buffer.storage) std::memcpy(replacement.Data(), buffer.storage.Data(), std::size_t(std::min(size, buffer.size)));
    DropTexelView(buffer);
    RetireRange(buffer.storage, buffer.lastUse);
    buffer.storage = replacement;
    buffer.size = size;
    buffer.lastUse = 0;
    buffer.lastGpuWrite = 0;
}

void BufferDataImpl(Buffer *buffer, GLsizeiptr size, const void *data, GLenum usage) {
    if (!buffer || size < 0) return;
    const VkDeviceSize bytes = VkDeviceSize(size);
    const bool busy = buffer->lastUse > g->completedSerial || buffer->lastGpuWrite > g->completedSerial;
    const bool placementChanged = buffer->storage && (GpuOwned(*buffer) !=
        (usage == GL_STATIC_COPY || usage == GL_DYNAMIC_COPY || usage == GL_STREAM_COPY));
    const bool wasPersistent = buffer->persistent;
    buffer->usage = usage;
    buffer->persistent = false;
    buffer->storageFlags = 0;
    if (!buffer->storage || bytes != buffer->size || busy || wasPersistent || placementChanged)
        ReplaceStorage(*buffer, bytes, false);
    if (data && bytes) std::memcpy(buffer->storage.Data(), data, std::size_t(bytes));
}
void BufferSubDataImpl(Buffer *buffer, GLintptr offset, GLsizeiptr size, const void *data) {
    if (!buffer || !buffer->storage || !data || size <= 0 || offset < 0 || VkDeviceSize(offset + size) > buffer->size) return;
    PrepareBufferWrite(*buffer, offset == 0 && VkDeviceSize(size) == buffer->size);
    std::memcpy(buffer->storage.Data() + offset, data, std::size_t(size));
}
void BufferStorageImpl(Buffer *buffer, GLsizeiptr size, const void *data, GLbitfield flags) {
    if (!buffer || size < 0) return;
    buffer->storageFlags = flags;
    buffer->persistent = (flags & GL_MAP_PERSISTENT_BIT) != 0;
    ReplaceStorage(*buffer, VkDeviceSize(size), false);
    if (data && size) std::memcpy(buffer->storage.Data(), data, std::size_t(size));
    else if (size) std::memset(buffer->storage.Data(), 0, std::size_t(size));
}

void GLAD_API_PTR GenBuffers(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) names[i] = AllocateName(g->buffers);
}
void GLAD_API_PTR CreateBuffers(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) {
        names[i] = AllocateName(g->buffers);
        g->buffers[names[i]].bound = true;
    }
}
void GLAD_API_PTR BindBuffer(GLenum target, GLuint name) {
    GLuint *binding = BufferBinding(target);
    if (!binding) return;
    *binding = name;
    if (Buffer *buffer = GetBuffer(name)) buffer->bound = true;
}
void GLAD_API_PTR DeleteBuffers(GLsizei count, const GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) {
        Buffer *buffer = GetBuffer(names[i]);
        if (!buffer) continue;
        DropTexelView(*buffer);
        RetireRange(buffer->storage, std::max(buffer->lastUse, buffer->lastGpuWrite));
        GlState &gl = g->gl;
        for (GLuint *binding : {&gl.arrayBuffer, &gl.uniformBuffer, &gl.copyRead, &gl.copyWrite, &gl.pixelPack,
                                &gl.pixelUnpack, &gl.textureBuffer, &gl.storageBuffer, &gl.indirectBuffer,
                                &gl.queryBuffer, &gl.dispatchIndirect})
            if (*binding == names[i]) *binding = 0;
        for (auto &indexed : gl.uniformBindings) if (indexed.buffer == names[i]) indexed = {};
        for (auto &indexed : gl.storageBindings) if (indexed.buffer == names[i]) indexed = {};
        // As in OpenGL, only the current vertex array forgets the buffer.
        VertexArray &vao = g->vertexArrays[gl.vao];
        if (vao.elementBuffer == names[i]) vao.elementBuffer = 0;
        for (auto &attribute : vao.attribs) if (attribute.buffer == names[i]) attribute.buffer = 0;
        *buffer = Buffer{};
    }
}
GLboolean GLAD_API_PTR IsBuffer(GLuint name) {
    Buffer *buffer = GetBuffer(name);
    return buffer && buffer->bound;
}
void GLAD_API_PTR BufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) { BufferDataImpl(BoundBuffer(target), size, data, usage); }
void GLAD_API_PTR NamedBufferData(GLuint name, GLsizeiptr size, const void *data, GLenum usage) { BufferDataImpl(GetBuffer(name), size, data, usage); }
void GLAD_API_PTR BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) { BufferSubDataImpl(BoundBuffer(target), offset, size, data); }
void GLAD_API_PTR NamedBufferSubData(GLuint name, GLintptr offset, GLsizeiptr size, const void *data) { BufferSubDataImpl(GetBuffer(name), offset, size, data); }
void GLAD_API_PTR BufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags) { BufferStorageImpl(BoundBuffer(target), size, data, flags); }
void GLAD_API_PTR NamedBufferStorage(GLuint name, GLsizeiptr size, const void *data, GLbitfield flags) { BufferStorageImpl(GetBuffer(name), size, data, flags); }

void WaitForGpuWrites(Buffer &buffer) {
    if (buffer.lastGpuWrite > g->completedSerial) {
        if (buffer.lastGpuWrite >= g->serial) Flush(true);
        else PollCompleted(true, buffer.lastGpuWrite);
    }
}
void *GLAD_API_PTR MapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    Buffer *buffer = BoundBuffer(target);
    if (!buffer || !buffer->storage || offset < 0 || length < 0 || VkDeviceSize(offset + length) > buffer->size) return nullptr;
    if (access & GL_MAP_WRITE_BIT) {
        if (!buffer->persistent && !(access & GL_MAP_UNSYNCHRONIZED_BIT))
            PrepareBufferWrite(*buffer, (access & GL_MAP_INVALIDATE_BUFFER_BIT) != 0);
    } else
        WaitForGpuWrites(*buffer);
    buffer->mapped = true;
    buffer->mapOffset = VkDeviceSize(offset);
    buffer->mapLength = VkDeviceSize(length);
    buffer->mapAccess = access;
    return buffer->storage.Data() + offset;
}
void *GLAD_API_PTR MapBuffer(GLenum target, GLenum access) {
    Buffer *buffer = BoundBuffer(target);
    if (!buffer) return nullptr;
    return MapBufferRange(target, 0, GLsizeiptr(buffer->size),
                          access == GL_READ_ONLY ? GL_MAP_READ_BIT : access == GL_WRITE_ONLY ? GL_MAP_WRITE_BIT
                                                                                              : (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT));
}
GLboolean GLAD_API_PTR UnmapBuffer(GLenum target) {
    if (Buffer *buffer = BoundBuffer(target)) buffer->mapped = false;
    return GL_TRUE;
}
void GLAD_API_PTR FlushMappedBufferRange(GLenum, GLintptr, GLsizeiptr) {}
void BufferParameter(Buffer *buffer, GLenum pname, GLint64 *value) {
    *value = 0;
    if (!buffer) return;
    switch (pname) {
    case GL_BUFFER_SIZE: *value = GLint64(buffer->storage ? buffer->size : 0); break;
    case GL_BUFFER_USAGE: *value = GLint64(buffer->usage); break;
    case GL_BUFFER_MAPPED: *value = buffer->mapped; break;
    case GL_BUFFER_MAP_OFFSET: *value = GLint64(buffer->mapOffset); break;
    case GL_BUFFER_MAP_LENGTH: *value = GLint64(buffer->mapLength); break;
    case GL_BUFFER_ACCESS_FLAGS: *value = GLint64(buffer->mapAccess); break;
    case GL_BUFFER_IMMUTABLE_STORAGE: *value = buffer->storageFlags != 0 || buffer->persistent; break;
    case GL_BUFFER_STORAGE_FLAGS: *value = GLint64(buffer->storageFlags); break;
    default: break;
    }
}
void GLAD_API_PTR GetBufferParameteriv(GLenum target, GLenum pname, GLint *value) {
    GLint64 wide = 0;
    BufferParameter(BoundBuffer(target), pname, &wide);
    if (value) *value = GLint(wide);
}
void GLAD_API_PTR GetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *value) {
    if (value) BufferParameter(BoundBuffer(target), pname, value);
}
void GLAD_API_PTR GetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data) {
    Buffer *buffer = BoundBuffer(target);
    if (!buffer || !buffer->storage || !data || offset < 0 || size < 0 || VkDeviceSize(offset + size) > buffer->size) return;
    WaitForGpuWrites(*buffer);
    std::memcpy(data, buffer->storage.Data() + offset, std::size_t(size));
}
void GLAD_API_PTR CopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    Buffer *source = BoundBuffer(readTarget), *destination = BoundBuffer(writeTarget);
    if (!source || !destination || !source->storage || !destination->storage || size <= 0) return;
    if (VkDeviceSize(readOffset + size) > source->size || VkDeviceSize(writeOffset + size) > destination->size) return;
    // Recorded in command order so pending GPU writes to the source are seen.
    EndPass();
    VkBufferCopy region{source->storage.offset + VkDeviceSize(readOffset), destination->storage.offset + VkDeviceSize(writeOffset), VkDeviceSize(size)};
    vkCmdCopyBuffer(g->cmd, source->storage.block->buffer, destination->storage.block->buffer, 1, &region);
    MemoryBarrierAll();
    source->lastUse = g->serial;
    destination->lastUse = destination->lastGpuWrite = g->serial;
}
void BindIndexed(GLenum target, GLuint index, GLuint name, VkDeviceSize offset, VkDeviceSize size) {
    if (target == GL_TRANSFORM_FEEDBACK_BUFFER && index == 0) {
        g->gl.feedbackBinding = {name, offset, size};
        BindBuffer(target, name);
        return;
    }
    auto *list = target == GL_UNIFORM_BUFFER ? &g->gl.uniformBindings : target == GL_SHADER_STORAGE_BUFFER ? &g->gl.storageBindings : nullptr;
    if (!list || index >= list->size()) return;
    auto &indexed = (*list)[index];
    if (indexed.buffer != name || indexed.offset != offset || indexed.size != size) {
        indexed = {name, offset, size};
        MarkDirty(DirtyBuffers);
    }
    BindBuffer(target, name);
}
void GLAD_API_PTR BindBufferBase(GLenum target, GLuint index, GLuint name) { BindIndexed(target, index, name, 0, 0); }
void GLAD_API_PTR BindBufferRange(GLenum target, GLuint index, GLuint name, GLintptr offset, GLsizeiptr size) {
    BindIndexed(target, index, name, VkDeviceSize(offset), VkDeviceSize(size));
}

// ---- vertex arrays ----
void GLAD_API_PTR GenVertexArrays(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) names[i] = AllocateName(g->vertexArrays);
}
void GLAD_API_PTR CreateVertexArrays(GLsizei count, GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) {
        names[i] = AllocateName(g->vertexArrays);
        g->vertexArrays[names[i]].bound = true;
    }
}
void GLAD_API_PTR BindVertexArray(GLuint name) {
    if (name && !Lookup(g->vertexArrays, name)) return;
    if (g->gl.vao != name) MarkDirty(DirtyVertex);
    g->gl.vao = name;
    g->vertexArrays[name].bound = true;
}
void GLAD_API_PTR DeleteVertexArrays(GLsizei count, const GLuint *names) {
    for (GLsizei i = 0; i < count; ++i) {
        if (!Lookup(g->vertexArrays, names[i])) continue;
        if (g->gl.vao == names[i]) g->gl.vao = 0;
        g->vertexArrays[names[i]] = VertexArray{};
    }
}
GLboolean GLAD_API_PTR IsVertexArray(GLuint name) {
    VertexArray *vao = Lookup(g->vertexArrays, name);
    return vao && vao->bound;
}
VertexArray &CurrentVao() { return g->vertexArrays[g->gl.vao]; }
void GLAD_API_PTR EnableVertexAttribArray(GLuint index) { if (index < 16) CurrentVao().attribs[index].enabled = true; }
void GLAD_API_PTR DisableVertexAttribArray(GLuint index) { if (index < 16) CurrentVao().attribs[index].enabled = false; }
void GLAD_API_PTR EnableVertexArrayAttrib(GLuint vao, GLuint index) {
    if (VertexArray *object = Lookup(g->vertexArrays, vao)) if (index < 16) object->attribs[index].enabled = true;
}
void Pointer(GLuint index, GLint size, GLenum type, bool normalized, bool integer, GLsizei stride, const void *pointer) {
    if (index >= 16) return;
    VertexAttrib &attribute = CurrentVao().attribs[index];
    attribute.size = size;
    attribute.type = type;
    attribute.normalized = normalized;
    attribute.integer = integer;
    attribute.stride = stride;
    attribute.offset = reinterpret_cast<std::uintptr_t>(pointer);
    attribute.buffer = g->gl.arrayBuffer;
}
void GLAD_API_PTR VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
    Pointer(index, size, type, normalized != GL_FALSE, false, stride, pointer);
}
void GLAD_API_PTR VertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer) {
    Pointer(index, size, type, false, true, stride, pointer);
}
void GLAD_API_PTR VertexAttribDivisor(GLuint index, GLuint divisor) { if (index < 16) CurrentVao().attribs[index].divisor = divisor; }
void GLAD_API_PTR VertexAttribI4ui(GLuint, GLuint, GLuint, GLuint, GLuint) {}
void GLAD_API_PTR VertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    if (index >= 16) return;
    float *value = g->gl.genericAttrib[index];
    value[0] = x; value[1] = y; value[2] = z; value[3] = w;
}
void GLAD_API_PTR VertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) { VertexAttrib4f(index, x, y, z, 1.0f); }
void GLAD_API_PTR VertexAttrib2f(GLuint index, GLfloat x, GLfloat y) { VertexAttrib4f(index, x, y, 0.0f, 1.0f); }
void GLAD_API_PTR VertexAttrib1f(GLuint index, GLfloat x) { VertexAttrib4f(index, x, 0.0f, 0.0f, 1.0f); }
void GLAD_API_PTR VertexAttrib4fv(GLuint index, const GLfloat *v) { VertexAttrib4f(index, v[0], v[1], v[2], v[3]); }
void GLAD_API_PTR VertexAttrib3fv(GLuint index, const GLfloat *v) { VertexAttrib4f(index, v[0], v[1], v[2], 1.0f); }
void GLAD_API_PTR GetVertexAttribiv(GLuint index, GLenum pname, GLint *value) {
    if (index >= 16 || !value) return;
    const VertexAttrib &attribute = CurrentVao().attribs[index];
    switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED: *value = attribute.enabled; break;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE: *value = attribute.size; break;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE: *value = GLint(attribute.type); break;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: *value = attribute.normalized; break;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE: *value = attribute.stride; break;
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: *value = GLint(attribute.buffer); break;
    case GL_VERTEX_ATTRIB_ARRAY_INTEGER: *value = attribute.integer; break;
    case GL_VERTEX_ATTRIB_ARRAY_DIVISOR: *value = GLint(attribute.divisor); break;
    default: *value = 0; break;
    }
}
void GLAD_API_PTR GetVertexAttribPointerv(GLuint index, GLenum, void **pointer) {
    if (index < 16 && pointer) *pointer = reinterpret_cast<void *>(CurrentVao().attribs[index].offset);
}
// Direct-state vertex buffer binding: binding index i feeds attribute i, the
// association every caller in the engine uses.
void GLAD_API_PTR VertexArrayVertexBuffer(GLuint vao, GLuint binding, GLuint buffer, GLintptr offset, GLsizei stride) {
    VertexArray *object = Lookup(g->vertexArrays, vao);
    if (!object || binding >= 16) return;
    VertexAttrib &attribute = object->attribs[binding];
    attribute.buffer = buffer;
    attribute.stride = stride;
    attribute.offset = std::uintptr_t(offset);
}
void GLAD_API_PTR VertexArrayAttribFormat(GLuint vao, GLuint index, GLint size, GLenum type, GLboolean normalized, GLuint relative) {
    VertexArray *object = Lookup(g->vertexArrays, vao);
    if (!object || index >= 16) return;
    VertexAttrib &attribute = object->attribs[index];
    attribute.size = size; attribute.type = type; attribute.normalized = normalized != GL_FALSE; attribute.integer = false;
    attribute.offset += relative;
}
void GLAD_API_PTR VertexArrayAttribIFormat(GLuint vao, GLuint index, GLint size, GLenum type, GLuint relative) {
    VertexArray *object = Lookup(g->vertexArrays, vao);
    if (!object || index >= 16) return;
    VertexAttrib &attribute = object->attribs[index];
    attribute.size = size; attribute.type = type; attribute.normalized = false; attribute.integer = true;
    attribute.offset += relative;
}
void GLAD_API_PTR VertexArrayAttribBinding(GLuint, GLuint, GLuint) {}
void GLAD_API_PTR VertexArrayElementBuffer(GLuint vao, GLuint buffer) {
    if (VertexArray *object = Lookup(g->vertexArrays, vao)) object->elementBuffer = buffer;
}

// ---- pipelines ----
VkFormat AttributeFormat(const VertexAttrib &a) {
    const int n = std::clamp(a.size, 1, 4) - 1;
    switch (a.type) {
    case GL_FLOAT: { static const VkFormat f[] = {VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT}; return f[n]; }
    case GL_HALF_FLOAT: { static const VkFormat f[] = {VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT}; return f[n]; }
    case GL_UNSIGNED_BYTE:
        if (a.integer) { static const VkFormat f[] = {VK_FORMAT_R8_UINT, VK_FORMAT_R8G8_UINT, VK_FORMAT_R8G8B8_UINT, VK_FORMAT_R8G8B8A8_UINT}; return f[n]; }
        if (a.normalized) { static const VkFormat f[] = {VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}; return f[n]; }
        { static const VkFormat f[] = {VK_FORMAT_R8_USCALED, VK_FORMAT_R8G8_USCALED, VK_FORMAT_R8G8B8_USCALED, VK_FORMAT_R8G8B8A8_USCALED}; return f[n]; }
    case GL_BYTE:
        if (a.integer) { static const VkFormat f[] = {VK_FORMAT_R8_SINT, VK_FORMAT_R8G8_SINT, VK_FORMAT_R8G8B8_SINT, VK_FORMAT_R8G8B8A8_SINT}; return f[n]; }
        { static const VkFormat f[] = {VK_FORMAT_R8_SNORM, VK_FORMAT_R8G8_SNORM, VK_FORMAT_R8G8B8_SNORM, VK_FORMAT_R8G8B8A8_SNORM}; return f[n]; }
    case GL_UNSIGNED_SHORT:
        if (a.integer) { static const VkFormat f[] = {VK_FORMAT_R16_UINT, VK_FORMAT_R16G16_UINT, VK_FORMAT_R16G16B16_UINT, VK_FORMAT_R16G16B16A16_UINT}; return f[n]; }
        { static const VkFormat f[] = {VK_FORMAT_R16_UNORM, VK_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16B16_UNORM, VK_FORMAT_R16G16B16A16_UNORM}; return f[n]; }
    case GL_SHORT:
        if (a.integer) { static const VkFormat f[] = {VK_FORMAT_R16_SINT, VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16B16_SINT, VK_FORMAT_R16G16B16A16_SINT}; return f[n]; }
        { static const VkFormat f[] = {VK_FORMAT_R16_SNORM, VK_FORMAT_R16G16_SNORM, VK_FORMAT_R16G16B16_SNORM, VK_FORMAT_R16G16B16A16_SNORM}; return f[n]; }
    case GL_UNSIGNED_INT: { static const VkFormat f[] = {VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32A32_UINT}; return f[n]; }
    case GL_INT: { static const VkFormat f[] = {VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32A32_SINT}; return f[n]; }
    default: return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
}
std::uint32_t AttributeBytes(const VertexAttrib &a) {
    const std::uint32_t channel = (a.type == GL_UNSIGNED_BYTE || a.type == GL_BYTE) ? 1 : (a.type == GL_UNSIGNED_SHORT || a.type == GL_SHORT || a.type == GL_HALF_FLOAT) ? 2 : 4;
    return channel * std::uint32_t(std::clamp(a.size, 1, 4));
}

VkBlendFactor BlendFactor(GLenum factor) {
    switch (factor) {
    case GL_ZERO: return VK_BLEND_FACTOR_ZERO;
    case GL_ONE: return VK_BLEND_FACTOR_ONE;
    case GL_SRC_COLOR: return VK_BLEND_FACTOR_SRC_COLOR;
    case GL_ONE_MINUS_SRC_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case GL_DST_COLOR: return VK_BLEND_FACTOR_DST_COLOR;
    case GL_ONE_MINUS_DST_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case GL_SRC_ALPHA: return VK_BLEND_FACTOR_SRC_ALPHA;
    case GL_ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case GL_DST_ALPHA: return VK_BLEND_FACTOR_DST_ALPHA;
    case GL_ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case GL_SRC_ALPHA_SATURATE: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case GL_CONSTANT_COLOR: return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case GL_ONE_MINUS_CONSTANT_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case GL_CONSTANT_ALPHA: return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case GL_ONE_MINUS_CONSTANT_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    default: return VK_BLEND_FACTOR_ONE;
    }
}
VkBlendOp BlendOp(GLenum equation) {
    switch (equation) {
    case GL_FUNC_SUBTRACT: return VK_BLEND_OP_SUBTRACT;
    case GL_FUNC_REVERSE_SUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
    case GL_MIN: return VK_BLEND_OP_MIN;
    case GL_MAX: return VK_BLEND_OP_MAX;
    default: return VK_BLEND_OP_ADD;
    }
}
VkCompareOp CompareOp(GLenum function) {
    switch (function) {
    case GL_NEVER: return VK_COMPARE_OP_NEVER;
    case GL_LESS: return VK_COMPARE_OP_LESS;
    case GL_EQUAL: return VK_COMPARE_OP_EQUAL;
    case GL_LEQUAL: return VK_COMPARE_OP_LESS_OR_EQUAL;
    case GL_GREATER: return VK_COMPARE_OP_GREATER;
    case GL_NOTEQUAL: return VK_COMPARE_OP_NOT_EQUAL;
    case GL_GEQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    default: return VK_COMPARE_OP_ALWAYS;
    }
}
bool Topology(GLenum mode, VkPrimitiveTopology &topology) {
    switch (mode) {
    case GL_TRIANGLES: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; return true;
    case GL_TRIANGLE_STRIP: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; return true;
    case GL_TRIANGLE_FAN: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; return true;
    case GL_LINES: topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; return true;
    case GL_LINE_STRIP: topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; return true;
    case GL_POINTS: topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; return true;
    default: return false;
    }
}

struct VertexSetup {
    std::array<VkVertexInputBindingDescription, 16> bindings{};
    std::array<VkVertexInputAttributeDescription, 16> attributes{};
    std::array<VkBuffer, 16> buffers{};
    std::array<VkDeviceSize, 16> offsets{};
    std::uint32_t count = 0;
    std::uint64_t hash = 1469598103934665603ull;
};

bool BuildVertexSetup(Program &program, VertexSetup &setup) {
    VertexArray &vao = CurrentVao();
    const auto mix = [&](std::uint64_t value) { setup.hash = (setup.hash ^ value) * 1099511628211ull; };
    for (const AttributeInfo &input : program.attributes) {
        if (input.location >= 16 || setup.count >= 16) continue;
        const VertexAttrib &attribute = vao.attribs[input.location];
        Buffer *buffer = attribute.enabled ? GetBuffer(attribute.buffer) : nullptr;
        const std::uint32_t index = setup.count++;
        auto &binding = setup.bindings[index];
        auto &description = setup.attributes[index];
        binding.binding = index;
        description.binding = index;
        description.location = input.location;
        description.offset = 0;
        if (buffer && buffer->storage) {
            binding.stride = attribute.stride ? std::uint32_t(attribute.stride) : AttributeBytes(attribute);
            binding.inputRate = attribute.divisor ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
            description.format = AttributeFormat(attribute);
            setup.buffers[index] = buffer->storage.block->buffer;
            setup.offsets[index] = buffer->storage.offset + attribute.offset;
            buffer->lastUse = g->serial;
        } else {
            // No array: OpenGL supplies the generic attribute value (0,0,0,1).
            binding.stride = 0;
            binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
            const bool integer = input.base == UniformBase::Int || input.base == UniformBase::UInt;
            description.format = input.base == UniformBase::UInt ? VK_FORMAT_R32G32B32A32_UINT
                                 : input.base == UniformBase::Int ? VK_FORMAT_R32G32B32A32_SINT
                                                                  : VK_FORMAT_R32G32B32A32_SFLOAT;
            setup.buffers[index] = g->dummyVertex.block->buffer;
            setup.offsets[index] = g->dummyVertex.offset + (integer ? 16 : 0);
            const float *generic = g->gl.genericAttrib[input.location];
            if (!integer && (generic[0] != 0.0f || generic[1] != 0.0f || generic[2] != 0.0f || generic[3] != 1.0f)) {
                const Range value = RingAllocate(16, 16);
                std::memcpy(value.Data(), generic, 16);
                setup.buffers[index] = value.block->buffer;
                setup.offsets[index] = value.offset;
            }
        }
        mix(description.location); mix(std::uint64_t(description.format)); mix(binding.stride); mix(std::uint64_t(binding.inputRate));
    }
    return true;
}

VkPipeline GetPipeline(Program &program, const VertexSetup &setup, VkPrimitiveTopology topology) {
    const GlState &gl = g->gl;
    PipelineKey key;
    std::memset(&key, 0, sizeof(key));
    key.program = program.generation;
    key.vertexHash = setup.hash;
    key.topology = std::uint32_t(topology);
    if (gl.blend) {
        key.blend = 1u | (std::uint32_t(BlendFactor(gl.blendSrcRgb)) << 1) | (std::uint32_t(BlendFactor(gl.blendDstRgb)) << 7) |
                    (std::uint32_t(BlendFactor(gl.blendSrcAlpha)) << 13) | (std::uint32_t(BlendFactor(gl.blendDstAlpha)) << 19);
        key.blendEq = std::uint32_t(BlendOp(gl.blendEqRgb)) | (std::uint32_t(BlendOp(gl.blendEqAlpha)) << 8);
    }
    key.colorCount = g->pass.colorCount;
    for (std::uint32_t i = 0; i < g->pass.colorCount; ++i) key.colorFormats[i] = std::uint32_t(g->pass.colorFormats[i]);
    key.depthFormat = std::uint32_t(g->pass.depthFormat);
    key.colorMask = (gl.colorMask[0] ? 1u : 0u) | (gl.colorMask[1] ? 2u : 0u) | (gl.colorMask[2] ? 4u : 0u) | (gl.colorMask[3] ? 8u : 0u);
    key.polygonMode = gl.polygonMode == GL_LINE && g->features.fillModeNonSolid ? 1u : 0u;
    key.depthBias = gl.polygonOffsetFill ? 1u : 0u;
    const auto found = g->pipelines.find(key);
    if (found != g->pipelines.end()) return found->second;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = program.modules[0];
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = program.modules[1];
    stages[1].pName = "main";
    const std::uint32_t stageCount = program.modules[1] ? 2u : 1u; // vertex-only programs capture or write depth
    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = setup.count;
    vertexInput.pVertexBindingDescriptions = setup.bindings.data();
    vertexInput.vertexAttributeDescriptionCount = setup.count;
    vertexInput.pVertexAttributeDescriptions = setup.attributes.data();
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = topology;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineViewportDepthClipControlCreateInfoEXT clipControl{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT};
    clipControl.negativeOneToOne = VK_TRUE;
    if (g->depthClipControl) viewport.pNext = &clipControl;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = key.polygonMode ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.0f;
    raster.depthBiasEnable = key.depthBias ? VK_TRUE : VK_FALSE;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.maxDepthBounds = 1.0f;
    std::array<VkPipelineColorBlendAttachmentState, 8> attachments{};
    for (std::uint32_t i = 0; i < g->pass.colorCount; ++i) {
        auto &a = attachments[i];
        a.colorWriteMask = key.colorMask;
        if (gl.blend) {
            a.blendEnable = VK_TRUE;
            a.srcColorBlendFactor = BlendFactor(gl.blendSrcRgb);
            a.dstColorBlendFactor = BlendFactor(gl.blendDstRgb);
            a.srcAlphaBlendFactor = BlendFactor(gl.blendSrcAlpha);
            a.dstAlphaBlendFactor = BlendFactor(gl.blendDstAlpha);
            a.colorBlendOp = BlendOp(gl.blendEqRgb);
            a.alphaBlendOp = BlendOp(gl.blendEqAlpha);
        }
    }
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = g->pass.colorCount;
    blend.pAttachments = attachments.data();
    const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_CULL_MODE,
                                            VK_DYNAMIC_STATE_FRONT_FACE, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
                                            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE, VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
                                            VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY, VK_DYNAMIC_STATE_DEPTH_BIAS,
                                            VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = std::uint32_t(std::size(dynamicStates));
    dynamic.pDynamicStates = dynamicStates;
    std::array<VkFormat, 8> colorFormats{};
    for (std::uint32_t i = 0; i < g->pass.colorCount; ++i) colorFormats[i] = g->pass.colorFormats[i];
    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount = g->pass.colorCount;
    rendering.pColorAttachmentFormats = colorFormats.data();
    rendering.depthAttachmentFormat = g->pass.depthFormat;
    if (AspectOf(g->pass.depthFormat) & VK_IMAGE_ASPECT_STENCIL_BIT) rendering.stencilAttachmentFormat = g->pass.depthFormat;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.pNext = &rendering;
    info.stageCount = stageCount;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = program.layout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(g->device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
        Warn("graphics pipeline creation failed");
        return VK_NULL_HANDLE;
    }
    g->pipelines.emplace(key, pipeline);
    return pipeline;
}

// ---- descriptors ----
struct BoundImage {
    std::shared_ptr<Image> image;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkBufferView bufferView = VK_NULL_HANDLE;
};

VkFormat TexelFormat(GLenum internal) {
    switch (internal) {
    case GL_RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case GL_RG32F: return VK_FORMAT_R32G32_SFLOAT;
    case GL_R32F: return VK_FORMAT_R32_SFLOAT;
    case GL_RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case GL_RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
    case GL_R32UI: return VK_FORMAT_R32_UINT;
    case GL_R32I: return VK_FORMAT_R32_SINT;
    case GL_RGBA32UI: return VK_FORMAT_R32G32B32A32_UINT;
    default: return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
}
VkBufferView MakeBufferView(VkBuffer buffer, VkFormat format, VkDeviceSize offset, VkDeviceSize range) {
    VkBufferViewCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO};
    info.buffer = buffer;
    info.format = format;
    info.offset = offset;
    info.range = range;
    VkBufferView view = VK_NULL_HANDLE;
    if (vkCreateBufferView(g->device, &info, nullptr, &view) != VK_SUCCESS) Fatal("vkCreateBufferView failed");
    return view;
}

bool ImageInPass(const Image *image) {
    if (!g->pass.active) return false;
    for (std::uint32_t i = 0; i < g->pass.colorCount; ++i)
        if (g->pass.color[i].get() == image) return true;
    return g->pass.depth.get() == image;
}

// Resolves every sampler of `program` to a view/sampler pair and brings the
// images into a readable layout. May end the current pass (layout changes
// cannot be recorded inside one); the caller re-establishes it afterwards.
void ResolveSamplers(Program &program, std::vector<BoundImage> &out) {
    const GlState &gl = g->gl;
    out.clear();
    for (const auto index : program.samplerUniforms) {
        const Uniform &uniform = program.uniforms[index];
        for (std::uint32_t element = 0; element < uniform.arraySize; ++element) {
            BoundImage bound;
            const int unit = uniform.units[element];
            const bool validUnit = unit >= 0 && unit < kTextureUnits;
            if (uniform.samplerBuffer) {
                Texture *texture = validUnit ? GetTexture(gl.textures[std::size_t(unit)][SlotBuffer]) : nullptr;
                Buffer *buffer = texture ? GetBuffer(texture->buffer) : nullptr;
                if (buffer && buffer->storage) {
                    const VkFormat format = TexelFormat(texture->bufferFormat);
                    if (!buffer->texelView || buffer->texelFormat != format) {
                        DropTexelView(*buffer);
                        const VkDeviceSize texel = format == VK_FORMAT_R32G32B32A32_SFLOAT || format == VK_FORMAT_R32G32B32A32_UINT ? 16
                                                   : format == VK_FORMAT_R32G32_SFLOAT || format == VK_FORMAT_R16G16B16A16_SFLOAT ? 8 : 4;
                        const VkDeviceSize range = std::max<VkDeviceSize>(buffer->size / texel * texel, texel);
                        buffer->texelView = MakeBufferView(buffer->storage.block->buffer, format, buffer->storage.offset,
                                                           std::min(range, buffer->storage.size));
                        buffer->texelFormat = format;
                    }
                    buffer->lastUse = g->serial;
                    bound.bufferView = buffer->texelView;
                } else {
                    if (!g->dummyTexelView)
                        g->dummyTexelView = MakeBufferView(g->dummyVertex.block->buffer, VK_FORMAT_R32G32B32A32_SFLOAT, g->dummyVertex.offset, 64);
                    bound.bufferView = g->dummyTexelView;
                }
                out.push_back(std::move(bound));
                continue;
            }
            const int slot = SlotForTarget(uniform.samplerTarget);
            Texture *texture = validUnit && slot >= 0 ? GetTexture(gl.textures[std::size_t(unit)][std::size_t(slot)]) : nullptr;
            VkImageView view = texture ? SamplingView(*texture) : VK_NULL_HANDLE;
            if (view && uniform.samplerTarget == GL_TEXTURE_CUBE_MAP_ARRAY && texture->image->layers - texture->viewBaseLayer < 6) view = VK_NULL_HANDLE;
            if (view && ImageInPass(texture->image.get())) {
                // Sampling an image while rendering to it is undefined in OpenGL.
                static bool warned = false;
                if (!warned) { warned = true; Warn("a draw samples a texture attached to its own framebuffer; substituting black"); }
                view = VK_NULL_HANDLE;
            }
            if (!view) {
                texture = GetTexture(g->dummyTextures[std::size_t(std::max(slot, 0))]);
                view = SamplingView(*texture);
            }
            Image &image = *texture->image;
            const VkImageLayout wanted = IsDepthFormat(image.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                                     : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            if (image.layout != wanted) {
                EndPass();
                Transition(image, wanted);
            }
            image.lastUse = g->serial;
            const SamplerObject *object = validUnit ? Lookup(g->samplerObjects, gl.samplers[std::size_t(unit)]) : nullptr;
            bound.image = texture->image;
            bound.view = view;
            const std::uint32_t levels = texture->maxLevel >= 0 ? std::min<std::uint32_t>(image.levels, std::uint32_t(texture->maxLevel) + 1) : image.levels;
            bound.sampler = GetSampler(object ? object->state : texture->sampler, levels > 1, uniform.samplerShadow);
            out.push_back(std::move(bound));
        }
    }
}

struct DescriptorWork {
    std::vector<BoundImage> images;
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<std::uint32_t> dynamicOffsets;
    // Reused per draw to avoid heap traffic in the submission path.
    struct BufferWrite { std::uint32_t binding; VkDescriptorType type; std::size_t info; };
    std::vector<BufferWrite> bufferWrites;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> dynamic; // binding -> offset
};

// Copies the program's default-block uniforms into this frame's ring when they
// changed (or belong to an earlier submission). True when the range moved.
bool RefreshDefaults(Program &program) {
    if (!program.hasDefaultBlock) return false;
    if (!program.defaultsDirty && program.defaultsSerial == g->serial && program.defaultsRange) return false;
    program.defaultsRange = RingAllocate(std::max<std::size_t>(program.defaults.size(), 16), g->bufferAlignment);
    std::memcpy(program.defaultsRange.Data(), program.defaults.data(), program.defaults.size());
    program.defaultsDirty = false;
    program.defaultsSerial = g->serial;
    return true;
}

// The last graphics draw's resolved state. Reused while nothing it depends on
// is marked dirty (Context::dirty); see docs/VULKAN_NATIVE.md.
struct DrawCache {
    bool valid = false;
    std::uint64_t program = 0; // Program::generation
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkBuffer defaultsBuffer = VK_NULL_HANDLE;
    int defaultsSlot = -1; // index of the default block in `offsets`
    std::vector<std::uint32_t> offsets;
    std::uint64_t vertexHash = 0;
    std::uint64_t bindings = 0; // BindingSignature of the resolved samplers
};
// The texture and sampler object names the program's samplers read right now.
std::uint64_t BindingSignature(const Program &program) {
    const GlState &gl = g->gl;
    std::uint64_t hash = 1469598103934665603ull;
    for (const auto index : program.samplerUniforms) {
        const Uniform &uniform = program.uniforms[index];
        const int slot = uniform.samplerBuffer ? int(SlotBuffer) : SlotForTarget(uniform.samplerTarget);
        for (const GLint unit : uniform.units) {
            std::uint64_t value = ~0ull;
            if (unit >= 0 && unit < kTextureUnits && slot >= 0)
                value = (std::uint64_t(gl.textures[std::size_t(unit)][std::size_t(slot)]) << 32) | gl.samplers[std::size_t(unit)];
            hash = (hash ^ value) * 1099511628211ull;
        }
    }
    for (const auto index : program.texelUniforms) {
        const GLint unit = program.uniforms[index].units[0];
        const std::uint64_t value = unit >= 0 && unit < kTextureUnits ? gl.textures[std::size_t(unit)][SlotBuffer] : ~0ull;
        hash = (hash ^ value) * 1099511628211ull;
    }
    return hash;
}
DrawCache &Cache() {
    static DrawCache cache;
    return cache;
}

// Builds (or reuses) the descriptor set for the current bindings and binds it.
bool BindDescriptors(Program &program, VkPipelineBindPoint bindPoint, DescriptorWork &work, bool markGpuWrites,
                     DrawCache *cache = nullptr) {
    GlState &gl = g->gl;
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&](std::uint64_t value) { hash = (hash ^ value) * 1099511628211ull; };
    mix(program.generation);
    work.bufferInfos.clear();
    work.imageInfos.clear();
    work.writes.clear();
    auto &dynamic = work.dynamic;
    auto &bufferWrites = work.bufferWrites;
    dynamic.clear();
    bufferWrites.clear();
    if (program.hasDefaultBlock) {
        RefreshDefaults(program);
        work.bufferInfos.push_back({program.defaultsRange.block->buffer, 0, std::max<VkDeviceSize>(program.defaults.size(), 16)});
        bufferWrites.push_back({program.defaultBinding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, work.bufferInfos.size() - 1});
        dynamic.emplace_back(program.defaultBinding, std::uint32_t(program.defaultsRange.offset));
        mix(std::uint64_t(program.defaultsRange.block->buffer));
    }
    const auto addBlock = [&](const BlockInfo &block, bool storage, bool active) {
        const auto &indexed = storage ? gl.storageBindings[std::min<std::size_t>(block.glBinding, gl.storageBindings.size() - 1)]
                                      : gl.uniformBindings[std::min<std::size_t>(block.glBinding, gl.uniformBindings.size() - 1)];
        Buffer *buffer = active ? GetBuffer(indexed.buffer) : nullptr;
        VkDescriptorBufferInfo info{};
        std::uint32_t offset = 0;
        if (buffer && buffer->storage && indexed.offset < buffer->size) {
            const VkDeviceSize available = indexed.size ? std::min(indexed.size, buffer->size - indexed.offset) : buffer->size - indexed.offset;
            info.buffer = buffer->storage.block->buffer;
            info.range = storage ? available : std::min<VkDeviceSize>(available, std::max<std::uint32_t>(block.size, 16));
            offset = std::uint32_t(buffer->storage.offset + indexed.offset);
            buffer->lastUse = g->serial;
            if (storage && markGpuWrites) buffer->lastGpuWrite = g->serial;
        } else {
            // Nothing bound: OpenGL reads undefined data; provide zeros.
            const Range zero = RingAllocate(std::max<std::uint32_t>(block.size, 16), g->bufferAlignment);
            std::memset(zero.Data(), 0, std::size_t(zero.size));
            info.buffer = zero.block->buffer;
            info.range = zero.size;
            offset = std::uint32_t(zero.offset);
        }
        work.bufferInfos.push_back(info);
        bufferWrites.push_back({block.binding, storage ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                work.bufferInfos.size() - 1});
        dynamic.emplace_back(block.binding, offset);
        mix(std::uint64_t(info.buffer));
        mix(info.range);
    };
    for (const auto &block : program.uniformBlocks) addBlock(block, false, true);
    for (const auto &block : program.storageBlocks) addBlock(block, true, true);
    for (const auto &block : program.inactiveBlocks) addBlock(block, block.storage, false);
    for (const auto index : program.texelUniforms) {
        // Float table of the buffer texture bound to the sampler's unit.
        const Uniform &uniform = program.uniforms[index];
        const GLint unit = uniform.units[0];
        Texture *texture = unit >= 0 && unit < kTextureUnits ? GetTexture(gl.textures[std::size_t(unit)][SlotBuffer]) : nullptr;
        Buffer *buffer = texture ? GetBuffer(texture->buffer) : nullptr;
        VkDescriptorBufferInfo info{};
        std::uint32_t offset = 0;
        if (buffer && buffer->storage && buffer->size >= 16) {
            if (texture->bufferFormat != GL_RGBA32F) {
                static bool warned = false;
                if (!warned) { warned = true; Warn("a samplerBuffer read as a float table is bound to a non-RGBA32F buffer texture"); }
            }
            info.buffer = buffer->storage.block->buffer;
            info.range = buffer->size / 16 * 16;
            offset = std::uint32_t(buffer->storage.offset);
            buffer->lastUse = g->serial;
        } else {
            const Range zero = RingAllocate(64, g->bufferAlignment);
            std::memset(zero.Data(), 0, 64);
            info.buffer = zero.block->buffer;
            info.range = 64;
            offset = std::uint32_t(zero.offset);
        }
        work.bufferInfos.push_back(info);
        bufferWrites.push_back({uniform.binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, work.bufferInfos.size() - 1});
        dynamic.emplace_back(uniform.binding, offset);
        mix(std::uint64_t(info.buffer));
        mix(info.range);
    }
    for (const auto &image : work.images) {
        mix(std::uint64_t(image.view));
        mix(std::uint64_t(image.sampler));
        mix(std::uint64_t(image.bufferView));
    }
    std::sort(dynamic.begin(), dynamic.end());
    work.dynamicOffsets.clear();
    for (const auto &entry : dynamic) work.dynamicOffsets.push_back(entry.second);

    VkDescriptorSet set = VK_NULL_HANDLE;
    const auto cached = g->setCache.find(hash);
    if (cached != g->setCache.end()) set = cached->second;
    else {
        set = AllocateSet(program.setLayout);
        for (const auto &write : bufferWrites) {
            VkWriteDescriptorSet entry{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            entry.dstSet = set;
            entry.dstBinding = write.binding;
            entry.descriptorCount = 1;
            entry.descriptorType = write.type;
            entry.pBufferInfo = &work.bufferInfos[write.info];
            work.writes.push_back(entry);
        }
        work.imageInfos.reserve(work.images.size());
        std::size_t cursor = 0;
        for (const auto index : program.samplerUniforms) {
            const Uniform &uniform = program.uniforms[index];
            VkWriteDescriptorSet entry{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            entry.dstSet = set;
            entry.dstBinding = uniform.binding;
            entry.descriptorCount = uniform.arraySize;
            if (uniform.samplerBuffer) {
                entry.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                entry.pTexelBufferView = &work.images[cursor].bufferView;
            } else {
                entry.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                const std::size_t first = work.imageInfos.size();
                for (std::uint32_t element = 0; element < uniform.arraySize; ++element) {
                    const BoundImage &bound = work.images[cursor + element];
                    work.imageInfos.push_back({bound.sampler, bound.view,
                                               IsDepthFormat(bound.image->format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                                                  : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
                }
                entry.pImageInfo = &work.imageInfos[first];
            }
            cursor += uniform.arraySize;
            work.writes.push_back(entry);
        }
        if (!work.writes.empty())
            vkUpdateDescriptorSets(g->device, std::uint32_t(work.writes.size()), work.writes.data(), 0, nullptr);
        g->setCache.emplace(hash, set);
    }
    vkCmdBindDescriptorSets(g->cmd, bindPoint, program.layout, 0, 1, &set, std::uint32_t(work.dynamicOffsets.size()),
                            work.dynamicOffsets.data());
    if (cache) {
        cache->set = set;
        cache->layout = program.layout;
        cache->offsets = work.dynamicOffsets;
        cache->defaultsSlot = -1;
        cache->defaultsBuffer = VK_NULL_HANDLE;
        if (program.hasDefaultBlock) {
            cache->defaultsBuffer = program.defaultsRange.block->buffer;
            for (std::size_t i = 0; i < dynamic.size(); ++i)
                if (dynamic[i].first == program.defaultBinding) cache->defaultsSlot = int(i);
        }
    }
    return true;
}

DescriptorWork &Work() {
    static DescriptorWork work;
    return work;
}

void ApplyDynamicState(VkPrimitiveTopology topology) {
    const GlState &gl = g->gl;
    auto &dynamic = g->dynamic;
    VkViewport viewport{float(gl.viewport[0]), float(gl.viewport[1]), float(gl.viewport[2]), float(gl.viewport[3]), 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, {g->pass.width, g->pass.height}};
    if (gl.scissorTest) {
        const int x0 = std::clamp(gl.scissor[0], 0, int(g->pass.width)), y0 = std::clamp(gl.scissor[1], 0, int(g->pass.height));
        const int x1 = std::clamp(gl.scissor[0] + gl.scissor[2], x0, int(g->pass.width));
        const int y1 = std::clamp(gl.scissor[1] + gl.scissor[3], y0, int(g->pass.height));
        scissor = {{x0, y0}, {std::uint32_t(x1 - x0), std::uint32_t(y1 - y0)}};
    }
    const VkCullModeFlags cull = !gl.cull ? VkCullModeFlags(VK_CULL_MODE_NONE)
                                 : gl.cullMode == GL_FRONT ? VkCullModeFlags(VK_CULL_MODE_FRONT_BIT)
                                 : gl.cullMode == GL_FRONT_AND_BACK ? VkCullModeFlags(VK_CULL_MODE_FRONT_AND_BACK)
                                                                    : VkCullModeFlags(VK_CULL_MODE_BACK_BIT);
    // Vulkan's signed-area convention is the negative of OpenGL's for the same
    // window coordinates, so counter-clockwise front faces map to clockwise.
    const VkFrontFace front = gl.frontFace == GL_CCW ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    const bool hasDepth = g->pass.depthFormat != VK_FORMAT_UNDEFINED;
    const VkBool32 depthTest = gl.depthTest && hasDepth;
    const VkBool32 depthWrite = depthTest && gl.depthMask;
    const VkCompareOp depthOp = CompareOp(gl.depthFunc);
    const bool all = !dynamic.valid;
    if (all || std::memcmp(&dynamic.viewport, &viewport, sizeof(viewport)) != 0) { vkCmdSetViewport(g->cmd, 0, 1, &viewport); dynamic.viewport = viewport; }
    if (all || std::memcmp(&dynamic.scissor, &scissor, sizeof(scissor)) != 0) { vkCmdSetScissor(g->cmd, 0, 1, &scissor); dynamic.scissor = scissor; }
    if (all || dynamic.cull != cull) { vkCmdSetCullMode(g->cmd, cull); dynamic.cull = cull; }
    if (all || dynamic.front != front) { vkCmdSetFrontFace(g->cmd, front); dynamic.front = front; }
    if (all || dynamic.depthTest != depthTest) { vkCmdSetDepthTestEnable(g->cmd, depthTest); dynamic.depthTest = depthTest; }
    if (all || dynamic.depthWrite != depthWrite) { vkCmdSetDepthWriteEnable(g->cmd, depthWrite); dynamic.depthWrite = depthWrite; }
    if (all || dynamic.depthOp != depthOp) { vkCmdSetDepthCompareOp(g->cmd, depthOp); dynamic.depthOp = depthOp; }
    if (all || dynamic.topology != topology) { vkCmdSetPrimitiveTopology(g->cmd, topology); dynamic.topology = topology; }
    if (all || dynamic.depthBiasConstant != gl.polygonOffsetUnits || dynamic.depthBiasSlope != gl.polygonOffsetFactor) {
        vkCmdSetDepthBias(g->cmd, gl.polygonOffsetUnits, 0.0f, gl.polygonOffsetFactor);
        dynamic.depthBiasConstant = gl.polygonOffsetUnits;
        dynamic.depthBiasSlope = gl.polygonOffsetFactor;
    }
    const VkBool32 discard = gl.rasterizerDiscard ? VK_TRUE : VK_FALSE;
    if (all || dynamic.rasterizerDiscard != discard) { vkCmdSetRasterizerDiscardEnable(g->cmd, discard); dynamic.rasterizerDiscard = discard; }
    dynamic.valid = true;
}

bool PrepareDraw(GLenum mode) {
    Program *program = CurrentProgram();
    if (!program || !program->linked || program->compute) return false;
    VkPrimitiveTopology topology;
    if (!Topology(mode, topology)) {
        Warn("unsupported primitive mode 0x%x", mode);
        return false;
    }
    if (g->gl.viewport[2] <= 0 || g->gl.viewport[3] <= 0) return false;
    DescriptorWork &work = Work();
    DrawCache &cache = Cache();
    EnsurePass();
    if (!g->pass.active) return false;
    const bool reusable = g->drawCache && cache.valid && cache.program == program->generation;
    if (!reusable) g->dirty = DirtyAll;
    if (g->dirty & (DirtyBindings | DirtyTextures)) {
        const std::uint64_t signature = BindingSignature(*program);
        if (signature != cache.bindings) g->dirty |= DirtyTextures;
        cache.bindings = signature;
        g->dirty &= ~std::uint32_t(DirtyBindings);
    }
    if (g->dirty & DirtyTextures) {
        // May end the pass to change image layouts; restarting it marks more.
        ResolveSamplers(*program, work.images);
        EnsurePass();
        if (!g->pass.active) return false;
    }
    if (cache.topology != topology) g->dirty |= DirtyPipeline;
    const std::uint32_t dirty = g->dirty;
    if (dirty) ++g->drawCacheMisses; else ++g->drawCacheHits;
    if (dirty & (DirtyVertex | DirtyBuffers)) {
        VertexSetup setup;
        BuildVertexSetup(*program, setup);
        if ((dirty & (DirtyVertex | DirtyPipeline)) || setup.hash != cache.vertexHash || !cache.pipeline) {
            cache.pipeline = GetPipeline(*program, setup, topology);
            cache.vertexHash = setup.hash;
        }
        if (setup.count) vkCmdBindVertexBuffers(g->cmd, 0, setup.count, setup.buffers.data(), setup.offsets.data());
    } else if (dirty & DirtyPipeline) {
        // Same vertex layout: only the pipeline's fixed state or formats changed.
        VertexSetup setup;
        BuildVertexSetup(*program, setup);
        cache.pipeline = GetPipeline(*program, setup, topology);
        cache.vertexHash = setup.hash;
    }
    if (!cache.pipeline) {
        cache.valid = false;
        return false;
    }
    if (g->dynamic.pipeline != cache.pipeline) {
        vkCmdBindPipeline(g->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cache.pipeline);
        g->dynamic.pipeline = cache.pipeline;
    }
    const bool moved = RefreshDefaults(*program);
    const bool newDefaultsBuffer = program->hasDefaultBlock && program->defaultsRange.block->buffer != cache.defaultsBuffer;
    if ((dirty & (DirtyTextures | DirtyBuffers)) || newDefaultsBuffer || !cache.set) {
        BindDescriptors(*program, VK_PIPELINE_BIND_POINT_GRAPHICS, work, false, &cache);
    } else if (moved && cache.defaultsSlot >= 0) {
        cache.offsets[std::size_t(cache.defaultsSlot)] = std::uint32_t(program->defaultsRange.offset);
        vkCmdBindDescriptorSets(g->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cache.layout, 0, 1, &cache.set,
                                std::uint32_t(cache.offsets.size()), cache.offsets.data());
    }
    ApplyDynamicState(topology);
    cache.valid = true;
    cache.program = program->generation;
    cache.topology = topology;
    g->dirty = 0;
    ++g->draws;
    return true;
}

bool BindIndices(GLenum type, const void *indices, VkDeviceSize &firstIndexBytes) {
    Buffer *buffer = GetBuffer(CurrentVao().elementBuffer);
    if (!buffer || !buffer->storage) return false;
    if (type != GL_UNSIGNED_INT && type != GL_UNSIGNED_SHORT) {
        Warn("unsupported index type 0x%x", type);
        return false;
    }
    buffer->lastUse = g->serial;
    firstIndexBytes = reinterpret_cast<std::uintptr_t>(indices);
    vkCmdBindIndexBuffer(g->cmd, buffer->storage.block->buffer, buffer->storage.offset,
                         type == GL_UNSIGNED_INT ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
    return true;
}

void GLAD_API_PTR DrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instances, GLuint baseInstance) {
    if (count <= 0 || instances <= 0 || !PrepareDraw(mode)) return;
    Program *program = CurrentProgram();
    Buffer *capture = g->gl.feedbackActive && program->feedbackStride ? GetBuffer(g->gl.feedbackBinding.buffer) : nullptr;
    if (capture && capture->storage) {
        // Captured vertices append after those of earlier draws in this capture.
        const VkDeviceSize offset = capture->storage.offset + g->gl.feedbackBinding.offset + g->gl.feedbackWritten;
        const VkDeviceSize bytes = VkDeviceSize(count) * VkDeviceSize(instances) * program->feedbackStride;
        const VkDeviceSize limit = g->gl.feedbackBinding.size ? g->gl.feedbackBinding.size : capture->size - g->gl.feedbackBinding.offset;
        if (g->gl.feedbackWritten + bytes <= limit) {
            g->bindFeedbackBuffers(g->cmd, 0, 1, &capture->storage.block->buffer, &offset, &bytes);
            g->beginFeedback(g->cmd, 0, 0, nullptr, nullptr);
            vkCmdDraw(g->cmd, std::uint32_t(count), std::uint32_t(instances), std::uint32_t(first), baseInstance);
            g->endFeedback(g->cmd, 0, 0, nullptr, nullptr);
            g->gl.feedbackWritten += bytes;
            capture->lastUse = capture->lastGpuWrite = g->serial;
            return;
        }
    }
    vkCmdDraw(g->cmd, std::uint32_t(count), std::uint32_t(instances), std::uint32_t(first), baseInstance);
}
void GLAD_API_PTR BeginTransformFeedback(GLenum) {
    g->gl.feedbackActive = true;
    g->gl.feedbackWritten = 0;
}
void GLAD_API_PTR EndTransformFeedback() { g->gl.feedbackActive = false; }
void GLAD_API_PTR DrawArrays(GLenum mode, GLint first, GLsizei count) { DrawArraysInstancedBaseInstance(mode, first, count, 1, 0); }
void GLAD_API_PTR DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instances) {
    DrawArraysInstancedBaseInstance(mode, first, count, instances, 0);
}
void DrawIndexed(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instances, GLint baseVertex) {
    if (count <= 0 || instances <= 0 || !PrepareDraw(mode)) return;
    VkDeviceSize offset = 0;
    if (!BindIndices(type, indices, offset)) return;
    vkCmdDrawIndexed(g->cmd, std::uint32_t(count), std::uint32_t(instances),
                     std::uint32_t(offset / (type == GL_UNSIGNED_INT ? 4 : 2)), baseVertex, 0);
}
void GLAD_API_PTR DrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) { DrawIndexed(mode, count, type, indices, 1, 0); }
void GLAD_API_PTR DrawRangeElements(GLenum mode, GLuint, GLuint, GLsizei count, GLenum type, const void *indices) { DrawIndexed(mode, count, type, indices, 1, 0); }
void GLAD_API_PTR DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instances) { DrawIndexed(mode, count, type, indices, instances, 0); }
void GLAD_API_PTR DrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint baseVertex) { DrawIndexed(mode, count, type, indices, 1, baseVertex); }
void GLAD_API_PTR DrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instances, GLint baseVertex) {
    DrawIndexed(mode, count, type, indices, instances, baseVertex);
}
void GLAD_API_PTR MultiDrawArraysIndirect(GLenum mode, const void *indirect, GLsizei count, GLsizei stride) {
    Buffer *buffer = GetBuffer(g->gl.indirectBuffer);
    if (!buffer || !buffer->storage || count <= 0 || !PrepareDraw(mode)) return;
    buffer->lastUse = g->serial;
    vkCmdDrawIndirect(g->cmd, buffer->storage.block->buffer, buffer->storage.offset + reinterpret_cast<std::uintptr_t>(indirect),
                      std::uint32_t(count), stride ? std::uint32_t(stride) : 16u);
}
void GLAD_API_PTR DrawArraysIndirect(GLenum mode, const void *indirect) { MultiDrawArraysIndirect(mode, indirect, 1, 16); }
void GLAD_API_PTR MultiDrawElementsIndirect(GLenum mode, GLenum type, const void *indirect, GLsizei count, GLsizei stride) {
    Buffer *buffer = GetBuffer(g->gl.indirectBuffer);
    if (!buffer || !buffer->storage || count <= 0 || !PrepareDraw(mode)) return;
    VkDeviceSize unused = 0;
    if (!BindIndices(type, nullptr, unused)) return;
    buffer->lastUse = g->serial;
    vkCmdDrawIndexedIndirect(g->cmd, buffer->storage.block->buffer, buffer->storage.offset + reinterpret_cast<std::uintptr_t>(indirect),
                             std::uint32_t(count), stride ? std::uint32_t(stride) : 20u);
}
void GLAD_API_PTR DrawElementsIndirect(GLenum mode, GLenum type, const void *indirect) { MultiDrawElementsIndirect(mode, type, indirect, 1, 20); }

void GLAD_API_PTR Clear(GLbitfield mask) {
    const GlState &gl = g->gl;
    const bool color = (mask & GL_COLOR_BUFFER_BIT) && (gl.colorMask[0] || gl.colorMask[1] || gl.colorMask[2] || gl.colorMask[3]);
    const bool depth = (mask & GL_DEPTH_BUFFER_BIT) && gl.depthMask;
    if (!color && !depth) return;
    const GLuint fbo = gl.drawFbo;
    const std::uint64_t revision = fbo ? g->framebuffers[fbo].revision : 0;
    const bool matching = g->pass.active && g->pass.fbo == fbo && g->pass.fboRevision == revision;
    if (!gl.scissorTest && !matching) {
        BeginPass(color, depth);
        return;
    }
    EnsurePass();
    if (!g->pass.active) return;
    std::array<VkClearAttachment, 9> attachments{};
    std::uint32_t count = 0;
    if (color)
        for (std::uint32_t i = 0; i < g->pass.colorCount; ++i) {
            if (!g->pass.color[i]) continue;
            auto &a = attachments[count++];
            a.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            a.colorAttachment = i;
            std::memcpy(a.clearValue.color.float32, gl.clearColor, sizeof(float) * 4);
        }
    if (depth && g->pass.depth) {
        auto &a = attachments[count++];
        a.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        a.clearValue.depthStencil = {float(gl.clearDepth), 0};
    }
    if (!count) return;
    VkClearRect rect{};
    rect.layerCount = 1;
    rect.rect = {{0, 0}, {g->pass.width, g->pass.height}};
    if (gl.scissorTest) {
        const int x0 = std::clamp(gl.scissor[0], 0, int(g->pass.width)), y0 = std::clamp(gl.scissor[1], 0, int(g->pass.height));
        const int x1 = std::clamp(gl.scissor[0] + gl.scissor[2], x0, int(g->pass.width));
        const int y1 = std::clamp(gl.scissor[1] + gl.scissor[3], y0, int(g->pass.height));
        if (x1 == x0 || y1 == y0) return;
        rect.rect = {{x0, y0}, {std::uint32_t(x1 - x0), std::uint32_t(y1 - y0)}};
    }
    vkCmdClearAttachments(g->cmd, count, attachments.data(), 1, &rect);
}

struct DispatchTiming {
    bool enabled = false, checked = false;
    VkQueryPool pool = VK_NULL_HANDLE;
    static constexpr std::uint32_t kSlots = 8192;
    struct Pending { std::uint32_t slot; std::uint64_t serial; std::string key; };
    std::vector<Pending> pending;
    std::uint32_t cursor = 0;
    struct Total { std::uint64_t count = 0; double ms = 0, worst = 0; };
    std::map<std::string, Total> totals;
};
DispatchTiming &Timing() {
    static DispatchTiming timing;
    if (!timing.checked) {
        timing.checked = true;
        const char *value = std::getenv("JUDAS_VULKAN_DISPATCH_TIMING");
        timing.enabled = value && std::strcmp(value, "1") == 0;
    }
    return timing;
}

void GLAD_API_PTR DispatchCompute(GLuint x, GLuint y, GLuint z) {
    Program *program = CurrentProgram();
    if (!program || !program->linked || !program->compute || !x || !y || !z) return;
    EndPass();
    DescriptorWork &work = Work();
    ResolveSamplers(*program, work.images);
    vkCmdBindPipeline(g->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, program->computePipeline);
    BindDescriptors(*program, VK_PIPELINE_BIND_POINT_COMPUTE, work, true);
    DispatchTiming &timing = Timing();
    std::uint32_t slot = 0;
    if (timing.enabled) {
        if (!timing.pool) {
            VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            info.queryType = VK_QUERY_TYPE_TIMESTAMP;
            info.queryCount = DispatchTiming::kSlots;
            if (vkCreateQueryPool(g->device, &info, nullptr, &timing.pool) != VK_SUCCESS) { timing.enabled = false; timing.pool = VK_NULL_HANDLE; }
        }
    }
    if (timing.enabled) {
        if (timing.pending.size() * 2 + 2 > DispatchTiming::kSlots) ResolveDispatchTiming(true);
        slot = timing.cursor;
        timing.cursor = (timing.cursor + 2) % DispatchTiming::kSlots;
        vkCmdResetQueryPool(g->cmd, timing.pool, slot, 2);
        vkCmdWriteTimestamp(g->cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timing.pool, slot);
    }
    vkCmdDispatch(g->cmd, x, y, z);
    if (timing.enabled) {
        vkCmdWriteTimestamp(g->cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timing.pool, slot + 1);
        char key[96];
        std::snprintf(key, sizeof key, "program %u grid %ux%ux%u", g->gl.program, x, y, z);
        timing.pending.push_back({slot, g->serial, key});
    }
    MemoryBarrierAll();
}
void GLAD_API_PTR MemoryBarrier(GLbitfield) {
    // Every dispatch and buffer copy is already followed by a full barrier in
    // command order; consumers need no additional synchronization.
}

template <class F> void (*Erase(F function))(void) { return reinterpret_cast<void (*)(void)>(function); }
} // namespace

void ClearDrawScratch() {
    Work() = DescriptorWork{};
    Cache() = DrawCache{};
}

void ResolveDispatchTiming(bool wait) {
    DispatchTiming &timing = Timing();
    if (!timing.enabled || timing.pending.empty()) return;
    if (wait && timing.pending.back().serial >= g->serial) Flush(true);
    else if (wait) PollCompleted(true, timing.pending.back().serial);
    std::size_t kept = 0;
    for (auto &entry : timing.pending) {
        if (entry.serial > g->completedSerial) { timing.pending[kept++] = std::move(entry); continue; }
        std::uint64_t ticks[2] = {};
        vkGetQueryPoolResults(g->device, timing.pool, entry.slot, 2, sizeof ticks, ticks, sizeof(std::uint64_t),
                              VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        const double ms = double(ticks[1] - ticks[0]) * double(g->properties.limits.timestampPeriod) / 1e6;
        auto &total = timing.totals[entry.key];
        ++total.count;
        total.ms += ms;
        total.worst = std::max(total.worst, ms);
    }
    timing.pending.resize(kept);
}
void PrintDispatchTiming() {
    DispatchTiming &timing = Timing();
    if (!timing.enabled) return;
    ResolveDispatchTiming(true);
    std::vector<std::pair<std::string, DispatchTiming::Total>> rows(timing.totals.begin(), timing.totals.end());
    std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.second.ms > b.second.ms; });
    std::fprintf(stderr, "[Vulkan] dispatch timing (GPU ms): total / count / mean / worst\n");
    for (const auto &[key, total] : rows)
        std::fprintf(stderr, "[Vulkan]   %-40s %10.3f %8llu %8.4f %8.4f\n", key.c_str(), total.ms,
                     static_cast<unsigned long long>(total.count), total.ms / double(total.count), total.worst);
    if (timing.pool) vkDestroyQueryPool(g->device, timing.pool, nullptr);
    timing.pool = VK_NULL_HANDLE;
}

void PrepareBufferWrite(Buffer &buffer, bool discard) {
    if (buffer.persistent) return;
    if (buffer.lastGpuWrite > g->completedSerial) {
        // GPU-written contents must exist before the CPU modifies part of them.
        if (discard) {
            ReplaceStorage(buffer, buffer.size, false);
            return;
        }
        WaitForGpuWrites(buffer);
    }
    if (buffer.lastUse > g->completedSerial) ReplaceStorage(buffer, buffer.size, !discard);
}

void BeginPass(bool clearColor, bool clearDepth) {
    EndPass();
    const GlState &gl = g->gl;
    const GLuint fbo = gl.drawFbo;
    Framebuffer *framebuffer = Lookup(g->framebuffers, fbo);
    PassState &pass = g->pass;
    pass.fbo = fbo;
    pass.fboRevision = framebuffer ? framebuffer->revision : 0;
    pass.colorCount = 0;
    pass.depth.reset();
    pass.depthFormat = VK_FORMAT_UNDEFINED;
    std::array<VkRenderingAttachmentInfo, 8> colors{};
    std::uint32_t width = UINT32_MAX, height = UINT32_MAX;
    const int drawBuffers = framebuffer ? framebuffer->drawBufferCount : 1;
    for (int i = 0; i < drawBuffers; ++i) {
        auto &info = colors[std::size_t(i)];
        info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        info.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        pass.color[std::size_t(i)].reset();
        pass.colorFormats[std::size_t(i)] = VK_FORMAT_UNDEFINED;
        ResolvedAttachment resolved;
        if (!ResolveFramebufferColor(fbo, i, resolved) || !(resolved.image->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) continue;
        Transition(*resolved.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        resolved.image->lastUse = g->serial;
        info.imageView = resolved.view;
        info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        info.loadOp = clearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        std::memcpy(info.clearValue.color.float32, gl.clearColor, sizeof(float) * 4);
        pass.color[std::size_t(i)] = resolved.image;
        pass.colorFormats[std::size_t(i)] = resolved.image->format;
        pass.colorCount = std::uint32_t(i) + 1;
        width = std::min(width, resolved.width);
        height = std::min(height, resolved.height);
    }
    VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    ResolvedAttachment resolvedDepth;
    const bool hasDepth = ResolveFramebufferDepth(fbo, resolvedDepth) &&
                          (resolvedDepth.image->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    if (hasDepth) {
        Transition(*resolvedDepth.image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        resolvedDepth.image->lastUse = g->serial;
        depth.imageView = resolvedDepth.view;
        depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth.loadOp = clearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth.clearValue.depthStencil = {float(gl.clearDepth), 0};
        pass.depth = resolvedDepth.image;
        pass.depthFormat = resolvedDepth.image->format;
        width = std::min(width, resolvedDepth.width);
        height = std::min(height, resolvedDepth.height);
    }
    if (!pass.colorCount && !hasDepth) return; // incomplete framebuffer: draws are dropped
    pass.width = width;
    pass.height = height;
    VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering.renderArea = {{0, 0}, {width, height}};
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = pass.colorCount;
    rendering.pColorAttachments = colors.data();
    if (hasDepth) {
        rendering.pDepthAttachment = &depth;
        if (AspectOf(pass.depthFormat) & VK_IMAGE_ASPECT_STENCIL_BIT) rendering.pStencilAttachment = &depth;
    }
    vkCmdBeginRendering(g->cmd, &rendering);
    pass.active = true;
    MarkDirty(DirtyPipeline); // attachment formats are part of the pipeline
    ++g->renderPasses;
}

void EnsurePass() {
    const GLuint fbo = g->gl.drawFbo;
    const std::uint64_t revision = fbo && fbo < g->framebuffers.size() ? g->framebuffers[fbo].revision : 0;
    if (g->pass.active && g->pass.fbo == fbo && g->pass.fboRevision == revision) return;
    BeginPass(false, false);
}

void InstallDrawEntries(std::unordered_map<std::string, void (*)(void)> &table) {
    table["glGenBuffers"] = Erase(&Invalidating<&GenBuffers>::Call);
    table["glCreateBuffers"] = Erase(&Invalidating<&CreateBuffers>::Call);
    table["glBindBuffer"] = Erase(&BindBuffer);
    table["glDeleteBuffers"] = Erase(&Invalidating<&DeleteBuffers>::Call);
    table["glIsBuffer"] = Erase(&IsBuffer);
    table["glBufferData"] = Erase(&BufferData);
    table["glNamedBufferData"] = Erase(&NamedBufferData);
    table["glBufferSubData"] = Erase(&BufferSubData);
    table["glNamedBufferSubData"] = Erase(&NamedBufferSubData);
    table["glBufferStorage"] = Erase(&Invalidating<&BufferStorage>::Call);
    table["glNamedBufferStorage"] = Erase(&Invalidating<&NamedBufferStorage>::Call);
    table["glMapBufferRange"] = Erase(&MapBufferRange);
    table["glMapBuffer"] = Erase(&MapBuffer);
    table["glUnmapBuffer"] = Erase(&UnmapBuffer);
    table["glFlushMappedBufferRange"] = Erase(&FlushMappedBufferRange);
    table["glGetBufferParameteriv"] = Erase(&GetBufferParameteriv);
    table["glGetBufferParameteri64v"] = Erase(&GetBufferParameteri64v);
    table["glGetBufferSubData"] = Erase(&Invalidating<&GetBufferSubData>::Call);
    table["glCopyBufferSubData"] = Erase(&Invalidating<&CopyBufferSubData>::Call);
    table["glBindBufferBase"] = Erase(&BindBufferBase);
    table["glBindBufferRange"] = Erase(&BindBufferRange);
    table["glGenVertexArrays"] = Erase(&Invalidating<&GenVertexArrays>::Call);
    table["glCreateVertexArrays"] = Erase(&Invalidating<&CreateVertexArrays>::Call);
    table["glBindVertexArray"] = Erase(&BindVertexArray);
    table["glDeleteVertexArrays"] = Erase(&Invalidating<&DeleteVertexArrays>::Call);
    table["glIsVertexArray"] = Erase(&IsVertexArray);
    table["glEnableVertexAttribArray"] = Erase(&VertexInvalidating<&EnableVertexAttribArray>::Call);
    table["glDisableVertexAttribArray"] = Erase(&VertexInvalidating<&DisableVertexAttribArray>::Call);
    table["glEnableVertexArrayAttrib"] = Erase(&Invalidating<&EnableVertexArrayAttrib>::Call);
    table["glVertexAttribPointer"] = Erase(&VertexInvalidating<&VertexAttribPointer>::Call);
    table["glVertexAttribIPointer"] = Erase(&VertexInvalidating<&VertexAttribIPointer>::Call);
    table["glVertexAttribDivisor"] = Erase(&VertexInvalidating<&VertexAttribDivisor>::Call);
    table["glVertexAttribI4ui"] = Erase(&Invalidating<&VertexAttribI4ui>::Call);
    table["glVertexAttrib4f"] = Erase(&Invalidating<&VertexAttrib4f>::Call);
    table["glVertexAttrib3f"] = Erase(&Invalidating<&VertexAttrib3f>::Call);
    table["glVertexAttrib2f"] = Erase(&Invalidating<&VertexAttrib2f>::Call);
    table["glVertexAttrib1f"] = Erase(&Invalidating<&VertexAttrib1f>::Call);
    table["glVertexAttrib4fv"] = Erase(&Invalidating<&VertexAttrib4fv>::Call);
    table["glVertexAttrib3fv"] = Erase(&Invalidating<&VertexAttrib3fv>::Call);
    table["glGetVertexAttribiv"] = Erase(&GetVertexAttribiv);
    table["glGetVertexAttribPointerv"] = Erase(&GetVertexAttribPointerv);
    table["glVertexArrayVertexBuffer"] = Erase(&VertexInvalidating<&VertexArrayVertexBuffer>::Call);
    table["glVertexArrayAttribFormat"] = Erase(&Invalidating<&VertexArrayAttribFormat>::Call);
    table["glVertexArrayAttribIFormat"] = Erase(&Invalidating<&VertexArrayAttribIFormat>::Call);
    table["glVertexArrayAttribBinding"] = Erase(&Invalidating<&VertexArrayAttribBinding>::Call);
    table["glVertexArrayElementBuffer"] = Erase(&Invalidating<&VertexArrayElementBuffer>::Call);
    table["glDrawArrays"] = Erase(&DrawArrays);
    table["glDrawArraysInstanced"] = Erase(&DrawArraysInstanced);
    table["glDrawArraysInstancedBaseInstance"] = Erase(&DrawArraysInstancedBaseInstance);
    table["glDrawElements"] = Erase(&DrawElements);
    table["glDrawRangeElements"] = Erase(&DrawRangeElements);
    table["glDrawElementsInstanced"] = Erase(&DrawElementsInstanced);
    table["glDrawElementsBaseVertex"] = Erase(&DrawElementsBaseVertex);
    table["glDrawElementsInstancedBaseVertex"] = Erase(&DrawElementsInstancedBaseVertex);
    table["glDrawArraysIndirect"] = Erase(&DrawArraysIndirect);
    table["glDrawElementsIndirect"] = Erase(&DrawElementsIndirect);
    table["glMultiDrawArraysIndirect"] = Erase(&MultiDrawArraysIndirect);
    table["glMultiDrawElementsIndirect"] = Erase(&MultiDrawElementsIndirect);
    table["glClear"] = Erase(&Invalidating<&Clear>::Call);
    table["glBeginTransformFeedback"] = Erase(&Invalidating<&BeginTransformFeedback>::Call);
    table["glEndTransformFeedback"] = Erase(&Invalidating<&EndTransformFeedback>::Call);
    table["glDispatchCompute"] = Erase(&Invalidating<&DispatchCompute>::Call);
    table["glMemoryBarrier"] = Erase(&Invalidating<&MemoryBarrier>::Call);
}

} // namespace JudasVulkan::detail
