#include "ComputeDevice.h"
#include "../GraphicsApi.h"
#include "ShaderLibrary.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <limits>
#include <thread>
#include <vector>

namespace JudasRender {
namespace {
bool Fail(std::string &error, const char *reason) {
    error = reason;
    return false;
}
} // namespace

#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
namespace {
constexpr unsigned allBufferUses = 7, allBarriers = 63;
std::atomic<std::uint64_t> nextOwner{1};
struct BufferSlot {
    unsigned name = 0, generation = 1, uses = 0, pending = 0;
    std::size_t bytes = 0;
    // Token only: GL deletion occurs explicitly on the validated owner context,
    // never from an arbitrary shared_ptr destructor/thread.
    std::shared_ptr<unsigned char> immutable;
};
void ReleaseBuffer(BufferSlot &entry) {
    if (!entry.immutable || entry.immutable.use_count() == 1)
        glDeleteBuffers(1, &entry.name);
    entry.immutable.reset();
    entry.name = 0;
}
struct KernelBinding {
    unsigned index = 0;
    std::size_t minimumBytes = 0;
};
struct KernelSlot {
    unsigned name = 0, generation = 1;
    std::array<int, 3> local{};
    std::vector<KernelBinding> bindings;
    std::vector<std::pair<std::string, int>> uint2Uniforms;
};
struct ReadbackSlot {
    unsigned buffer = 0, generation = 1;
    GLsync fence = nullptr;
    std::size_t bytes = 0;
};
struct StorageBinding {
    ComputeBuffer buffer;
    ComputeAccess access = ComputeAccess::ReadWrite;
};
constexpr unsigned NextGeneration(unsigned generation) {
    return generation == std::numeric_limits<unsigned>::max() ? generation : generation + 1;
}
static_assert(NextGeneration(1) == 2 &&
              NextGeneration(std::numeric_limits<unsigned>::max()) == std::numeric_limits<unsigned>::max(),
              "Retired compute handles must never wrap and alias an old generation");
void RetireGeneration(unsigned &generation) { generation = NextGeneration(generation); }
} // namespace
struct ComputeDevice::State {
    GraphicsCapabilities caps;
    ComputeBudget budget;
    SDL_GLContext context = nullptr;
    std::thread::id thread;
    std::uint64_t owner = 0;
    std::vector<BufferSlot> buffers;
    std::vector<KernelSlot> kernels;
    std::vector<StorageBinding> bindings;
    std::array<ReadbackSlot, MaxReadbacks> readbacks{};
    std::size_t bytes = 0, readbackBytes = 0;
    unsigned liveBuffers = 0, liveKernels = 0;

    BufferSlot *Buffer(ComputeBuffer handle, std::string &error) {
        if (handle.owner != owner || handle.slot >= buffers.size() || !buffers[handle.slot].name ||
            buffers[handle.slot].generation != handle.generation) {
            Fail(error, "compute buffer handle is stale or belongs to another device");
            return nullptr;
        }
        return &buffers[handle.slot];
    }
    KernelSlot *Kernel(ComputeKernel handle, std::string &error) {
        if (handle.owner != owner || handle.slot >= kernels.size() || !kernels[handle.slot].name ||
            kernels[handle.slot].generation != handle.generation) {
            Fail(error, "compute kernel handle is stale or belongs to another device");
            return nullptr;
        }
        return &kernels[handle.slot];
    }
    ReadbackSlot *Readback(ComputeReadback handle, std::string &error) {
        if (handle.owner != owner || handle.slot >= readbacks.size() ||
            !readbacks[handle.slot].buffer || readbacks[handle.slot].generation != handle.generation) {
            Fail(error, "compute readback is stale or belongs to another device");
            return nullptr;
        }
        return &readbacks[handle.slot];
    }
    void RetireReadback(ReadbackSlot &entry) {
        if (entry.fence) glDeleteSync(entry.fence);
        if (entry.buffer) glDeleteBuffers(1, &entry.buffer);
        readbackBytes -= entry.bytes;
        const auto generation = NextGeneration(entry.generation);
        entry = {};
        entry.generation = generation;
    }
};

bool ComputeDevice::Initialize(const GraphicsCapabilities &caps, ComputeBudget budget,
                               std::string &error) {
    error.clear();
    if (state)
        return Fail(error,
                    "compute device is already initialized; shut it down on its owning context first");
    if (!budget.bytes || !budget.buffers || !budget.kernels || budget.buffers > 4096 ||
        budget.kernels > 1024)
        return Fail(error, "compute budget needs nonzero bytes and bounded buffer/kernel slots");
    if (caps.api != GraphicsApi::Desktop || !caps.modernTier || !caps.compute || !caps.storageBuffers) {
        diagnostic = "compute is disabled for the selected graphics profile; desktop modern "
                     "compute/SSBO support is required";
        error = diagnostic;
        return false;
    }
    if (!GraphicsContextCurrent() || !glGetProgramInterfaceiv || !glGetProgramResourceiv ||
        !glGetBufferParameteri64v || !glGetBufferSubData || !glGetIntegeri_v || !glBindBufferBase ||
        !glDispatchCompute || !glMemoryBarrier)
        return Fail(error, "compute requires the owning context and complete "
                           "dispatch/storage/reflection entry points");
    // The supplied policy can disable hardware facilities, but cannot invent
    // support or enlarge driver limits for the actual owning context.
    const auto actual = QueryGraphicsCapabilities(caps.requestedProfile);
    if (!actual.modernTier || !actual.compute || !actual.storageBuffers)
        return Fail(error, "the current context does not qualify for modern compute");
    if (actual.limits.storageBufferBindings <= 0 || actual.limits.storageBlockSize == 0 ||
        actual.limits.computeInvocations <= 0 ||
        std::any_of(actual.limits.computeGroupCount.begin(), actual.limits.computeGroupCount.end(),
                    [](int n) { return n <= 0; }) ||
        std::any_of(actual.limits.computeGroupSize.begin(), actual.limits.computeGroupSize.end(),
                    [](int n) { return n <= 0; }))
        return Fail(error, "compute capability limits are incomplete");
    auto candidate = std::make_unique<State>();
    candidate->caps = actual;
    candidate->budget = budget;
    candidate->context = SDL_GL_GetCurrentContext();
    candidate->thread = std::this_thread::get_id();
    candidate->owner = nextOwner.fetch_add(1, std::memory_order_relaxed);
    candidate->bindings.resize(unsigned(actual.limits.storageBufferBindings));
    state = std::move(candidate);
    diagnostic = "desktop compute available";
    return true;
}

bool ComputeDevice::Current(std::string &error) const {
    error.clear();
    if (!state) {
        error = diagnostic.empty() ? "compute device is not initialized" : diagnostic;
        return false;
    }
    if (std::this_thread::get_id() != state->thread || SDL_GL_GetCurrentContext() != state->context)
        return Fail(error, "compute operation requires its owning graphics context and thread");
    return true;
}

ComputeBuffer ComputeDevice::CreateBuffer(std::size_t bytes, ComputeBufferUse use, const void *initial,
                                          std::string &error) {
    if (!Current(error))
        return {};
    const auto uses = unsigned(use);
    if (!bytes || !uses || (uses & ~allBufferUses) || bytes > state->caps.limits.storageBlockSize ||
        bytes > std::size_t(std::numeric_limits<GLsizeiptr>::max()) ||
        bytes > state->budget.bytes - state->bytes - state->readbackBytes ||
        state->liveBuffers == state->budget.buffers) {
        Fail(error, "compute buffer size, use or allocation budget is invalid");
        return {};
    }
    unsigned slot = 0;
    while (slot < state->buffers.size() && (state->buffers[slot].name ||
           state->buffers[slot].generation == std::numeric_limits<unsigned>::max()))
        ++slot;
    if (slot == state->buffers.size()) {
        if(slot == state->budget.buffers){Fail(error,"compute buffer slots exhausted after generation retirement");return {};}
        state->buffers.emplace_back();
    }
    GLint previous = 0;
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &previous);
    GLuint name = 0;
    glGenBuffers(1, &name);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, name);
    glBufferData(GL_SHADER_STORAGE_BUFFER, GLsizeiptr(bytes), initial, GL_DYNAMIC_COPY);
    GLint64 actual = 0;
    glGetBufferParameteri64v(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &actual);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, GLuint(previous));
    if (!name || actual < 0 || std::uint64_t(actual) != bytes) {
        if (name)
            glDeleteBuffers(1, &name);
        Fail(error, "driver did not allocate the requested compute buffer");
        return {};
    }
    auto &entry = state->buffers[slot];
    entry.name = name;
    entry.bytes = bytes;
    entry.uses = uses;
    entry.pending = 0;
    state->bytes += bytes;
    ++state->liveBuffers;
    return {slot, entry.generation, state->owner};
}

ComputeBuffer ComputeDevice::ShareReadOnlyBuffer(ComputeDevice &source, ComputeBuffer handle,
                                                 std::string &error) {
    if (!Current(error) || !source.Current(error)) return {};
    auto *original = source.state->Buffer(handle, error);
    if (!original) return {};
    if (&source == this || state->context != source.state->context || original->pending ||
        original->uses != unsigned(ComputeBufferUse::Storage) ||
        original->bytes > state->budget.bytes - state->bytes - state->readbackBytes ||
        state->liveBuffers == state->budget.buffers) {
        Fail(error, "immutable storage sharing requires same-context, completed storage and available budget");
        return {};
    }
    unsigned slot = 0;
    while (slot < state->buffers.size() && (state->buffers[slot].name ||
           state->buffers[slot].generation == std::numeric_limits<unsigned>::max())) ++slot;
    if (slot == state->buffers.size()) state->buffers.emplace_back();
    if (!original->immutable) original->immutable = std::make_shared<unsigned char>(0);
    auto &entry = state->buffers[slot];
    entry.name = original->name; entry.bytes = original->bytes;
    entry.uses = unsigned(ComputeBufferUse::Storage); entry.pending = 0;
    entry.immutable = original->immutable;
    state->bytes += entry.bytes; ++state->liveBuffers;
    return {slot, entry.generation, state->owner};
}

bool ComputeDevice::DestroyBuffer(ComputeBuffer handle, std::string &error) {
    if (!Current(error))
        return false;
    auto *entry = state->Buffer(handle, error);
    if (!entry)
        return false;
    for (auto &binding : state->bindings)
        if (binding.buffer.owner == handle.owner && binding.buffer.slot == handle.slot &&
            binding.buffer.generation == handle.generation)
            binding.buffer = {};
    ReleaseBuffer(*entry);
    state->bytes -= entry->bytes;
    --state->liveBuffers;
    entry->name = 0;
    entry->bytes = 0;
    entry->pending = 0;
    RetireGeneration(entry->generation);
    return true;
}

bool ComputeDevice::UpdateBuffer(ComputeBuffer handle, std::size_t offset, const void *data,
                                 std::size_t bytes, std::string &error) {
    if (!Current(error))
        return false;
    auto *entry = state->Buffer(handle, error);
    if (!entry)
        return false;
    if (entry->immutable) return Fail(error, "shared immutable storage cannot be updated");
    if (!data || !bytes || offset > entry->bytes || bytes > entry->bytes - offset)
        return Fail(error, "compute buffer update lies outside its allocation");
    if (entry->pending & unsigned(ComputeBarrier::BufferUpdate))
        return Fail(error, "compute buffer update requires a BufferUpdate barrier after shader writes");
    GLint previous = 0;
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &previous);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, entry->name);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, GLintptr(offset), GLsizeiptr(bytes), data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, GLuint(previous));
    return true;
}

bool ComputeDevice::BindStorage(ComputeBuffer handle, unsigned binding, ComputeAccess access,
                                std::string &error) {
    if (!Current(error))
        return false;
    auto *entry = state->Buffer(handle, error);
    if (!entry)
        return false;
    if (!(entry->uses & unsigned(ComputeBufferUse::Storage)) || binding >= state->bindings.size() ||
        (access != ComputeAccess::ReadOnly && access != ComputeAccess::WriteOnly &&
         access != ComputeAccess::ReadWrite))
        return Fail(error, "compute storage binding, access or buffer use is invalid");
    if (entry->immutable && access != ComputeAccess::ReadOnly)
        return Fail(error, "shared immutable storage requires read-only access");
    state->bindings[binding] = {handle, access};
    return true;
}

bool ComputeDevice::BindGraphicsStorage(ComputeBuffer handle,unsigned binding,std::string& error){
    if(!Current(error))return false;
    auto* entry=state->Buffer(handle,error);if(!entry)return false;
    if(!(entry->uses&unsigned(ComputeBufferUse::Storage))||binding>=state->bindings.size())return Fail(error,"graphics storage binding requires storage use and a valid binding");
    if(entry->pending&unsigned(ComputeBarrier::Storage))return Fail(error,"graphics storage consumer requires a Storage barrier");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,binding,entry->name);return true;
}

bool ComputeDevice::BindVertex(ComputeBuffer handle, std::string &error) {
    if (!Current(error))
        return false;
    auto *entry = state->Buffer(handle, error);
    if (!entry)
        return false;
    if (!(entry->uses & unsigned(ComputeBufferUse::Vertex)))
        return Fail(error, "buffer has no vertex use");
    if (entry->pending & unsigned(ComputeBarrier::Vertex))
        return Fail(error, "vertex consumer requires a Vertex barrier");
    glBindBuffer(GL_ARRAY_BUFFER, entry->name);
    return true;
}

bool ComputeDevice::BindIndirect(ComputeBuffer handle, std::string &error) {
    if (!Current(error))
        return false;
    auto *entry = state->Buffer(handle, error);
    if (!entry)
        return false;
    if (!(entry->uses & unsigned(ComputeBufferUse::Indirect)))
        return Fail(error, "buffer has no indirect use");
    if (entry->pending & unsigned(ComputeBarrier::Indirect))
        return Fail(error, "indirect consumer requires an Indirect barrier");
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, entry->name);
    return true;
}

ComputeKernel ComputeDevice::CreateKernel(const char *source, const char *identity,
                                          std::string &error) {
    if (!Current(error))
        return {};
    if (!source || state->liveKernels == state->budget.kernels) {
        Fail(error, "compute kernel source or program budget is invalid");
        return {};
    }
    std::size_t sourceBytes = 0;
    while (sourceBytes <= 1024 * 1024 && source[sourceBytes])
        ++sourceBytes;
    if (sourceBytes > 1024 * 1024) {
        Fail(error, "compute shader exceeds the source-size limit");
        return {};
    }
    GLuint shader = 0, program = 0;
    if (!Shaders::CompileShader(GL_COMPUTE_SHADER, source, shader, Shaders::Profile::Desktop450,
                                identity)) {
        Fail(error, "compute shader compilation failed; see named shader diagnostics");
        return {};
    }
    const bool linked = Shaders::LinkComputeProgram(shader, program, identity);
    glDeleteShader(shader);
    if (!linked) {
        Fail(error, "compute program linking failed; see named shader diagnostics");
        return {};
    }
    KernelSlot entry;
    entry.name = program;
    glGetProgramiv(program, GL_COMPUTE_WORK_GROUP_SIZE, entry.local.data());
    std::uint64_t invocations = 1;
    for (unsigned axis = 0; axis < 3; ++axis) {
        if (entry.local[axis] <= 0 || entry.local[axis] > state->caps.limits.computeGroupSize[axis]) {
            glDeleteProgram(program);
            Fail(error, "compute kernel local group size exceeds device limits");
            return {};
        }
        invocations *= unsigned(entry.local[axis]);
    }
    if (invocations > unsigned(state->caps.limits.computeInvocations)) {
        glDeleteProgram(program);
        Fail(error, "compute kernel local invocation count exceeds device limits");
        return {};
    }
    GLint blocks = 0;
    glGetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &blocks);
    const GLenum properties[] = {GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE};
    for (GLint block = 0; block < blocks; ++block) {
        GLint values[2] = {};
        glGetProgramResourceiv(program, GL_SHADER_STORAGE_BLOCK, GLuint(block), 2, properties, 2,
                               nullptr, values);
        if (values[0] < 0 || std::size_t(values[0]) >= state->bindings.size() || values[1] < 0) {
            glDeleteProgram(program);
            Fail(error, "compute kernel declares an invalid storage binding");
            return {};
        }
        entry.bindings.push_back({unsigned(values[0]), std::size_t(values[1])});
    }
    unsigned slot = 0;
    while (slot < state->kernels.size() && (state->kernels[slot].name ||
           state->kernels[slot].generation == std::numeric_limits<unsigned>::max()))
        ++slot;
    if (slot == state->kernels.size()) {
        if(slot == state->budget.kernels){glDeleteProgram(program);Fail(error,"compute kernel slots exhausted after generation retirement");return {};}
        state->kernels.emplace_back();
    }
    entry.generation = state->kernels[slot].generation;
    state->kernels[slot] = std::move(entry);
    ++state->liveKernels;
    return {slot, state->kernels[slot].generation, state->owner};
}

bool ComputeDevice::DestroyKernel(ComputeKernel handle, std::string &error) {
    if (!Current(error))
        return false;
    auto *entry = state->Kernel(handle, error);
    if (!entry)
        return false;
    glDeleteProgram(entry->name);
    entry->name = 0;
    entry->bindings.clear();
    RetireGeneration(entry->generation);
    --state->liveKernels;
    return true;
}

bool ComputeDevice::Dispatch(ComputeKernel handle, std::array<unsigned, 3> groups, std::string &error) {
    if (!Current(error))
        return false;
    auto *entry = state->Kernel(handle, error);
    if (!entry)
        return false;
    for (unsigned axis = 0; axis < 3; ++axis)
        if (!groups[axis] || groups[axis] > unsigned(state->caps.limits.computeGroupCount[axis]))
            return Fail(error, "compute dispatch group count is zero or exceeds device limits");
    for (const auto &required : entry->bindings) {
        const auto &binding = state->bindings[required.index];
        auto *buffer = state->Buffer(binding.buffer, error);
        if (!buffer)
            return false;
        if (buffer->immutable && binding.access != ComputeAccess::ReadOnly)
            return Fail(error, "shared immutable storage cannot be dispatched through a writable binding");
        if (buffer->bytes < required.minimumBytes)
            return Fail(error, "compute storage allocation is smaller than the linked block");
        if (buffer->pending & unsigned(ComputeBarrier::Storage))
            return Fail(error, "storage consumer requires a Storage barrier after shader writes");
    }
    GLint previous = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
    glUseProgram(entry->name);
    for (const auto &required : entry->bindings) {
        const auto &binding = state->bindings[required.index];
        const auto *buffer = state->Buffer(binding.buffer, error);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, required.index, buffer->name);
    }
    glDispatchCompute(groups[0], groups[1], groups[2]);
    glUseProgram(GLuint(previous));
    for (const auto &required : entry->bindings) {
        const auto &binding = state->bindings[required.index];
        if (binding.access != ComputeAccess::ReadOnly)
            state->Buffer(binding.buffer, error)->pending |= allBarriers;
    }
    return true;
}

bool ComputeDevice::Barrier(ComputeBarrier barrier, std::string &error) {
    if (!Current(error))
        return false;
    const auto bits = unsigned(barrier);
    if (!bits || (bits & ~allBarriers))
        return Fail(error, "compute barrier mask is empty or invalid");
    GLbitfield native = 0;
    if (bits & unsigned(ComputeBarrier::Storage))
        native |= GL_SHADER_STORAGE_BARRIER_BIT;
    if (bits & unsigned(ComputeBarrier::Vertex))
        native |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
    if (bits & unsigned(ComputeBarrier::Indirect))
        native |= GL_COMMAND_BARRIER_BIT;
    if (bits & unsigned(ComputeBarrier::Image))
        native |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
    if (bits & unsigned(ComputeBarrier::Texture))
        native |= GL_TEXTURE_FETCH_BARRIER_BIT;
    if (bits & unsigned(ComputeBarrier::BufferUpdate))
        native |= GL_BUFFER_UPDATE_BARRIER_BIT;
    glMemoryBarrier(native);
    for (auto &entry : state->buffers)
        entry.pending &= ~bits;
    return true;
}

bool ComputeDevice::SetKernelUInt2(ComputeKernel handle, const char *name,
                                   std::array<unsigned, 2> values, std::string &error) {
    if (!Current(error)) return false;
    auto *kernel = state->Kernel(handle, error);
    if (!kernel) return false;
    if (!name || !*name || !glProgramUniform2ui)
        return Fail(error, "compute uint parameters require a name and program-uniform support");
    for (const auto &uniform : kernel->uint2Uniforms)
        if (uniform.first == name) {
            glProgramUniform2ui(kernel->name, uniform.second, values[0], values[1]);
            return true;
        }
    const auto location = glGetUniformLocation(kernel->name, name);
    GLuint index = GL_INVALID_INDEX;
    glGetUniformIndices(kernel->name, 1, &name, &index);
    GLint type = 0, size = 0;
    if (index != GL_INVALID_INDEX) {
        glGetActiveUniformsiv(kernel->name, 1, &index, GL_UNIFORM_TYPE, &type);
        glGetActiveUniformsiv(kernel->name, 1, &index, GL_UNIFORM_SIZE, &size);
    }
    if (location < 0 || type != GL_UNSIGNED_INT_VEC2 || size != 1)
        return Fail(error, "compute parameter must name an active non-array uvec2 uniform");
    kernel->uint2Uniforms.emplace_back(name, location);
    glProgramUniform2ui(kernel->name, location, values[0], values[1]);
    return true;
}

ComputeReadback ComputeDevice::RequestReadback(ComputeBuffer handle, std::size_t offset,
                                               std::size_t bytes, std::string &error) {
    if (!Current(error)) return {};
    const auto *source = state->Buffer(handle, error);
    if (!source) return {};
    if (!bytes || bytes > MaxReadbackBytes || offset > source->bytes || bytes > source->bytes-offset ||
        bytes > state->budget.bytes-state->bytes-state->readbackBytes) {
        Fail(error, "compute readback exceeds its source, staging or device budget"); return {};
    }
    if (source->pending & unsigned(ComputeBarrier::BufferUpdate)) {
        Fail(error, "compute readback copy requires a BufferUpdate barrier"); return {};
    }
    unsigned slot = 0;
    while (slot < state->readbacks.size() && (state->readbacks[slot].buffer ||
           state->readbacks[slot].generation == std::numeric_limits<unsigned>::max())) ++slot;
    if (slot == state->readbacks.size()) {
        Fail(error, "bounded compute readback queue is full"); return {};
    }
    GLint previousRead = 0, previousWrite = 0;
    glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &previousRead);
    glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &previousWrite);
    unsigned buffer = 0;
    glGenBuffers(1, &buffer);
    if (!buffer) { Fail(error, "cannot create compute readback storage"); return {}; }
    glBindBuffer(GL_COPY_WRITE_BUFFER, buffer);
    glBufferData(GL_COPY_WRITE_BUFFER, GLsizeiptr(bytes), nullptr, GL_STREAM_READ);
    GLint actual = 0;
    glGetBufferParameteriv(GL_COPY_WRITE_BUFFER, GL_BUFFER_SIZE, &actual);
    GLsync fence = nullptr;
    if (buffer && actual == GLint(bytes)) {
        glBindBuffer(GL_COPY_READ_BUFFER, source->name);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, GLintptr(offset), 0,
                           GLsizeiptr(bytes));
        fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }
    glBindBuffer(GL_COPY_READ_BUFFER, GLuint(previousRead));
    glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(previousWrite));
    if (!fence) {
        if (buffer) glDeleteBuffers(1, &buffer);
        Fail(error, "cannot allocate or fence bounded compute readback storage"); return {};
    }
    auto &entry = state->readbacks[slot];
    entry.buffer = buffer; entry.fence = fence; entry.bytes = bytes;
    state->readbackBytes += bytes;
    glFlush(); // Submit without waiting, including headless callers without SwapBuffers.
    return {slot, entry.generation, state->owner};
}

ComputeReadbackStatus ComputeDevice::PollReadback(ComputeReadback handle, void *output,
                                                  std::size_t bytes, std::string &error) {
    if (!Current(error)) return ComputeReadbackStatus::Failed;
    auto *entry = state->Readback(handle, error);
    if (!entry) return ComputeReadbackStatus::Failed;
    if (!output || bytes != entry->bytes) {
        Fail(error, "compute readback output must match the requested bounded size");
        return ComputeReadbackStatus::Failed;
    }
    const auto result = glClientWaitSync(entry->fence, 0, 0);
    if (result == GL_TIMEOUT_EXPIRED) return ComputeReadbackStatus::Pending;
    if (result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED) {
        state->RetireReadback(*entry);
        Fail(error, "compute readback fence failed"); return ComputeReadbackStatus::Failed;
    }
    GLint previous = 0;
    glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &previous);
    glBindBuffer(GL_COPY_READ_BUFFER, entry->buffer);
    glGetBufferSubData(GL_COPY_READ_BUFFER, 0, GLsizeiptr(bytes), output);
    glBindBuffer(GL_COPY_READ_BUFFER, GLuint(previous));
    state->RetireReadback(*entry);
    return ComputeReadbackStatus::Ready;
}
bool ComputeDevice::CancelReadback(ComputeReadback handle, std::string &error) {
    if (!Current(error)) return false;
    auto *entry = state->Readback(handle, error);
    if (!entry) return false;
    state->RetireReadback(*entry);
    return true;
}

bool ComputeDevice::FinishForDiagnostics(std::string& error) {
    if (!Current(error)) return false;
    glFinish();
    return true;
}
bool ComputeDevice::ReadForDiagnostics(ComputeBuffer handle, std::size_t offset, void *output,
                                       std::size_t bytes, std::string &error) {
    if (!Current(error))
        return false;
    auto *entry = state->Buffer(handle, error);
    if (!entry)
        return false;
    if (!output || !bytes || offset > entry->bytes || bytes > entry->bytes - offset)
        return Fail(error, "diagnostic read lies outside its compute allocation");
    if (entry->pending & unsigned(ComputeBarrier::BufferUpdate))
        return Fail(error, "diagnostic read requires a BufferUpdate barrier");
    GLint previous = 0;
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &previous);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, entry->name);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, GLintptr(offset), GLsizeiptr(bytes), output);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, GLuint(previous));
    return true;
}

bool ComputeDevice::Shutdown(std::string &error) {
    error.clear();
    if (!state)
        return true;
    if (!Current(error))
        return false;
    for (auto &entry : state->readbacks)
        if (entry.buffer) state->RetireReadback(entry);
    for (const auto &entry : state->kernels)
        if (entry.name)
            glDeleteProgram(entry.name);
    for (auto &entry : state->buffers)
        if (entry.name) ReleaseBuffer(entry);
    state.reset();
    diagnostic = "compute device is shut down";
    return true;
}
bool ComputeDevice::Supported() const { return state != nullptr; }
std::size_t ComputeDevice::AllocatedBytes() const { return state ? state->bytes + state->readbackBytes : 0; }
unsigned ComputeDevice::LiveBuffers() const { return state ? state->liveBuffers : 0; }
unsigned ComputeDevice::LiveKernels() const { return state ? state->liveKernels : 0; }
#else
struct ComputeDevice::State {};
bool ComputeDevice::Initialize(const GraphicsCapabilities &, ComputeBudget, std::string &error) {
    diagnostic = "compute is unavailable on the GLES3/WebGL2 backend";
    error = diagnostic;
    return false;
}
bool ComputeDevice::Current(std::string &error) const {
    return Fail(error,
                diagnostic.empty() ? "compute is unavailable on this backend" : diagnostic.c_str());
}
ComputeBuffer ComputeDevice::CreateBuffer(std::size_t, ComputeBufferUse, const void *, std::string &e) {
    Current(e);
    return {};
}
ComputeBuffer ComputeDevice::ShareReadOnlyBuffer(ComputeDevice &, ComputeBuffer, std::string &e) {
    Current(e); return {};
}
bool ComputeDevice::DestroyBuffer(ComputeBuffer, std::string &e) { return Current(e); }
bool ComputeDevice::UpdateBuffer(ComputeBuffer, std::size_t, const void *, std::size_t,
                                 std::string &e) {
    return Current(e);
}
bool ComputeDevice::BindStorage(ComputeBuffer, unsigned, ComputeAccess, std::string &e) {
    return Current(e);
}
bool ComputeDevice::BindGraphicsStorage(ComputeBuffer,unsigned,std::string& e){return Current(e);}
bool ComputeDevice::BindVertex(ComputeBuffer, std::string &e) { return Current(e); }
bool ComputeDevice::BindIndirect(ComputeBuffer, std::string &e) { return Current(e); }
ComputeKernel ComputeDevice::CreateKernel(const char *, const char *, std::string &e) {
    Current(e);
    return {};
}
bool ComputeDevice::DestroyKernel(ComputeKernel, std::string &e) { return Current(e); }
bool ComputeDevice::Dispatch(ComputeKernel, std::array<unsigned, 3>, std::string &e) {
    return Current(e);
}
bool ComputeDevice::SetKernelUInt2(ComputeKernel, const char *, std::array<unsigned, 2>, std::string &e) { return Current(e); }
ComputeReadback ComputeDevice::RequestReadback(ComputeBuffer, std::size_t, std::size_t, std::string &e) { Current(e); return {}; }
ComputeReadbackStatus ComputeDevice::PollReadback(ComputeReadback, void *, std::size_t, std::string &e) { Current(e); return ComputeReadbackStatus::Failed; }
bool ComputeDevice::CancelReadback(ComputeReadback, std::string &e) { return Current(e); }
bool ComputeDevice::Barrier(ComputeBarrier, std::string &e) { return Current(e); }
bool ComputeDevice::ReadForDiagnostics(ComputeBuffer, std::size_t, void *, std::size_t,
                                       std::string &e) {
    return Current(e);
}
bool ComputeDevice::FinishForDiagnostics(std::string& e) { return Current(e); }
bool ComputeDevice::Shutdown(std::string &error) {
    error.clear();
    return true;
}
bool ComputeDevice::Supported() const { return false; }
std::size_t ComputeDevice::AllocatedBytes() const { return 0; }
unsigned ComputeDevice::LiveBuffers() const { return 0; }
unsigned ComputeDevice::LiveKernels() const { return 0; }
#endif

ComputeDevice::ComputeDevice() = default;
ComputeDevice::~ComputeDevice() {
    std::string error;
    if (!Shutdown(error))
        std::fprintf(stderr, "Compute device shutdown: %s\n", error.c_str());
}
const std::string &ComputeDevice::Diagnostic() const { return diagnostic; }
} // namespace JudasRender
