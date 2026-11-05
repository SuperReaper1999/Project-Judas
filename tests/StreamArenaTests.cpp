#include "EngineHost.h"
#include "GraphicsApi.h"
#include "render/StreamArena.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char *message) {
    ++checks;
    failures += !value;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", message);
    std::fflush(stdout);
}
using JudasRender::StreamArena;
using JudasRender::StreamSlice;
static_assert(sizeof(unsigned) == 4, "GL_UNSIGNED_INT fixtures use 32-bit records");

void PureControls() {
    StreamArena arena;
    StreamSlice slice{999, 999};
    std::string error = "stale diagnostic";
    const unsigned char byte = 42;
    Check(arena.AllocatedBytes() == 0 && arena.WaitNanoseconds() == 0,
          "an unused optional stream arena has no graphics storage or waits");
    arena.SetWorkingSetHint(std::numeric_limits<std::size_t>::max());
    Check(arena.AllocatedBytes() == 0 && arena.WaitNanoseconds() == 0,
          "even an oversized working-set hint is CPU-only and performs no graphics allocation");
    arena.SetWorkingSetHint(0);
    for (bool persistent : {false, true}) {
        Check(!arena.Upload(nullptr, 1, persistent, slice, error) && !error.empty() &&
                  slice.buffer == 0 && slice.offset == 0,
              "null data rejects with an empty unpublished slice");
        Check(!arena.Upload(&byte, 0, persistent, slice, error) && !error.empty(),
              "zero-byte uploads reject without allocating a placeholder buffer");
        Check(!arena.Upload(&byte, StreamArena::PageBytes + 1, persistent, slice, error) &&
                  !error.empty() && arena.AllocatedBytes() == 0,
              "oversized upload rejects before dereferencing input or allocating GPU storage");
        Check(!arena.Upload(&byte, 1, persistent, slice, error) && !error.empty() &&
                  slice.buffer == 0 && arena.AllocatedBytes() == 0,
              "valid input without a current context rejects before any GL dispatch");
    }
    arena.Shutdown();
    arena.Shutdown();
    Check(arena.AllocatedBytes() == 0, "empty shutdown is idempotent and needs no graphics context");
}

#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
std::vector<unsigned char> Pattern(std::size_t bytes, unsigned salt) {
    std::vector<unsigned char> result(bytes);
    for (std::size_t index = 0; index < bytes; ++index)
        result[index] = static_cast<unsigned char>((index * 71 + (index >> 8) * 13 + salt * 37) & 255);
    return result;
}
bool ExactBytes(StreamSlice slice, const std::vector<unsigned char> &expected) {
    if (!slice.buffer)
        return false;
    glBindBuffer(GL_COPY_READ_BUFFER, slice.buffer);
    GLint bytes = 0;
    glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &bytes);
    if (slice.offset + expected.size() > std::size_t(bytes))
        return false;
    std::vector<unsigned char> actual(expected.size());
    glGetBufferSubData(GL_COPY_READ_BUFFER, GLintptr(slice.offset), GLsizeiptr(actual.size()),
                       actual.data());
    return actual == expected;
}

// Independent GPU consumer: queue transform-feedback copies before later uploads
// orphan or reuse pages, then inspect all results together. CPU buffer readback
// alone would not exercise the lifetime of draw commands consuming old slices.
struct Consumer {
    unsigned program = 0, vao = 0, output = 0;
    static constexpr unsigned Samples = 64;
    bool Init(unsigned rounds) {
        constexpr char source[] = R"GLSL(#version 330 core
layout(location=0) in uint value;
flat out uint copied;
void main(){copied=value;gl_Position=vec4(0,0,0,1);}
)GLSL";
        const auto shader = glCreateShader(GL_VERTEX_SHADER);
        const char *pointer = source;
        glShaderSource(shader, 1, &pointer, nullptr);
        glCompileShader(shader);
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glDeleteShader(shader);
            return false;
        }
        program = glCreateProgram();
        glAttachShader(program, shader);
        const char *varying = "copied";
        glTransformFeedbackVaryings(program, 1, &varying, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program);
        glDeleteShader(shader);
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success)
            return false;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &output);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, output);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, GLsizeiptr(rounds * Samples * sizeof(unsigned)),
                     nullptr, GL_STREAM_READ);
        return vao && output;
    }
    void Consume(StreamSlice slice, unsigned round) const {
        glUseProgram(program);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, slice.buffer);
        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(unsigned),
                               reinterpret_cast<void *>(slice.offset));
        glEnableVertexAttribArray(0);
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, output,
                          GLintptr(round * Samples * sizeof(unsigned)), Samples * sizeof(unsigned));
        glEnable(GL_RASTERIZER_DISCARD);
        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, Samples);
        glEndTransformFeedback();
        glDisable(GL_RASTERIZER_DISCARD);
    }
    bool Matches(const std::vector<std::vector<unsigned char>> &payloads) const {
        std::vector<unsigned> actual(payloads.size() * Samples);
        glBindBuffer(GL_COPY_READ_BUFFER, output);
        glGetBufferSubData(GL_COPY_READ_BUFFER, 0, GLsizeiptr(actual.size() * sizeof(unsigned)),
                           actual.data());
        for (std::size_t round = 0; round < payloads.size(); ++round)
            if (std::memcmp(actual.data() + round * Samples, payloads[round].data(),
                            Samples * sizeof(unsigned)) != 0)
                return false;
        return true;
    }
    ~Consumer() {
        glBindVertexArray(0);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
        if (output)
            glDeleteBuffers(1, &output);
        if (vao)
            glDeleteVertexArrays(1, &vao);
        if (program)
            glDeleteProgram(program);
        glUseProgram(0);
    }
};

GLsync timedOutFence = nullptr;
unsigned timeoutCalls = 0;
bool sameTimeoutFence = true;
GLenum GLAD_API_PTR ForcedTimeout(GLsync fence, GLbitfield, GLuint64) {
    if (timeoutCalls && fence != timedOutFence)
        sameTimeoutFence = false;
    timedOutFence = fence;
    ++timeoutCalls;
    return GL_TIMEOUT_EXPIRED;
}
void GLAD_API_PTR MissingBuffers(GLsizei count, GLuint *buffers) {
    std::fill(buffers, buffers + count, 0);
}
void GLAD_API_PTR MissingStorage(GLenum, GLsizeiptr, const void *, GLenum) {}
void *GLAD_API_PTR MissingMapping(GLenum, GLintptr, GLsizeiptr, GLbitfield) { return nullptr; }

void PortableControls() {
    StreamArena arena;
    arena.SetWorkingSetHint(std::numeric_limits<std::size_t>::max());
    std::string error;
    StreamSlice slice;
    unsigned owned = 0;
    for (const auto bytes : {std::size_t(1), std::size_t(257), StreamArena::PageBytes, std::size_t(19)}) {
        const auto data = Pattern(bytes, unsigned(bytes));
        Check(arena.Upload(data.data(), bytes, false, slice, error) && error.empty() &&
                  slice.buffer && slice.offset == 0 && arena.AllocatedBytes() == bytes &&
                  ExactBytes(slice, data),
              "portable orphaning preserves exact odd, full-page and shrinking payloads");
        if (!owned)
            owned = slice.buffer;
        Check(slice.buffer == owned, "portable orphaning reuses its buffer object name");
    }
    const auto savedBytes = arena.AllocatedBytes();
    const auto data = Pattern(256, 7);
    Consumer consumer;
    const bool ready = consumer.Init(12);
    Check(ready, "independent transform-feedback lifetime oracle links successfully");
    if (ready) {
        std::vector<std::vector<unsigned char>> queued;
        bool uploaded = true;
        for (unsigned round = 0; round < 12; ++round) {
            queued.push_back(Pattern(256 + round * 4, round));
            uploaded &= arena.Upload(queued.back().data(), queued.back().size(), false, slice, error);
            if (!uploaded)
                break;
            consumer.Consume(slice, round);
        }
        Check(uploaded && consumer.Matches(queued),
              "queued GPU consumers retain every older portable payload after repeated orphaning");
    }
    bool rejected = false;
    const auto beforeThread = arena.AllocatedBytes();
    std::thread worker([&] {
        StreamSlice rejectedSlice{999, 999};
        std::string reason;
        rejected = !arena.Upload(data.data(), data.size(), false, rejectedSlice, reason) &&
                   !reason.empty() && !rejectedSlice.buffer;
        arena.Shutdown(); // A foreign owner must not destroy the original context's storage.
    });
    worker.join();
    Check(rejected && arena.AllocatedBytes() == beforeThread && glIsBuffer(owned),
          "foreign-thread upload and shutdown preserve the original owner's live buffer");
    Check(savedBytes != 0, "portable allocation remains live until its explicit owning shutdown");
    arena.Shutdown();
    Check(arena.AllocatedBytes() == 0 && !glIsBuffer(owned) && arena.WaitNanoseconds() == 0,
          "portable owning shutdown actually deletes its GL object and resets storage accounting");
    arena.Shutdown();
}

void AllocationFailureControls(bool persistent) {
    StreamArena arena;
    StreamSlice slice{999, 999};
    std::string error;
    const auto data = Pattern(256, 0);
    const auto savedGenerator = glad_glGenBuffers;
    glad_glGenBuffers = MissingBuffers;
    const bool created = arena.Upload(data.data(), data.size(), persistent, slice, error);
    glad_glGenBuffers = savedGenerator;
    Check(!created && !error.empty() && !slice.buffer && arena.AllocatedBytes() == 0,
          "failed buffer-name allocation cannot publish a successful stream slice");
    if (!persistent) {
        const auto savedStorage = glad_glBufferData;
        glad_glBufferData = MissingStorage;
        const bool allocated = arena.Upload(data.data(), data.size(), false, slice, error);
        glad_glBufferData = savedStorage;
        Check(!allocated && !error.empty() && !slice.buffer && arena.AllocatedBytes() == 0,
              "unallocated portable buffer storage rejects and retires the failed object");
    } else {
        const auto savedMapping = glad_glMapBufferRange;
        glad_glMapBufferRange = MissingMapping;
        const bool mapped = arena.Upload(data.data(), data.size(), true, slice, error);
        glad_glMapBufferRange = savedMapping;
        Check(!mapped && !error.empty() && !slice.buffer && arena.AllocatedBytes() == 0,
              "failed persistent mapping releases partial allocations without publishing a slice");
    }
    Check(arena.Upload(data.data(), data.size(), persistent, slice, error) && ExactBytes(slice, data),
          "allocation failures leave the arena usable for a successful retry");
    if (persistent) {
        const auto retained = slice;
        const auto retainedBytes = arena.AllocatedBytes();
        arena.SetWorkingSetHint(70000);
        const auto savedMapping = glad_glMapBufferRange;
        glad_glMapBufferRange = MissingMapping;
        const bool grown = arena.Upload(data.data(), data.size(), true, slice, error);
        glad_glMapBufferRange = savedMapping;
        Check(!grown && !error.empty() && !slice.buffer &&
                  arena.AllocatedBytes() == retainedBytes && ExactBytes(retained, data),
              "failed hint-driven growth retains the old allocation and exact published payload");
        Check(arena.Upload(data.data(), data.size(), true, slice, error) &&
                  arena.AllocatedBytes() > retainedBytes && ExactBytes(slice, data),
              "hint-driven growth can retry successfully after a mapping failure");
    }
    arena.Shutdown();
}

void PersistentControls() {
    StreamArena arena;
    StreamSlice slice;
    std::string error;
    std::vector<unsigned> buffers;
    const auto tiny = Pattern(17, 11);
    Check(arena.Upload(tiny.data(), tiny.size(), true, slice, error) && ExactBytes(slice, tiny),
          "coherent persistent storage publishes the exact initial odd-length payload");
    buffers.push_back(slice.buffer);
    const auto second = Pattern(257, 12);
    Check(arena.Upload(second.data(), second.size(), true, slice, error) && slice.offset == 256 &&
              slice.buffer == buffers.front() && ExactBytes(slice, second),
          "successive persistent slices are aligned and do not overwrite earlier payloads");
    const auto firstCapacity = arena.AllocatedBytes();
    const auto grown = Pattern(70001, 13);
    const auto beforeGrowth = arena.WaitNanoseconds();
    Check(arena.Upload(grown.data(), grown.size(), true, slice, error) && slice.offset == 0 &&
              arena.AllocatedBytes() > firstCapacity &&
              arena.AllocatedBytes() <= 3 * StreamArena::PageBytes && ExactBytes(slice, grown) &&
              arena.WaitNanoseconds() >= beforeGrowth,
          "persistent growth retains exact data with bounded storage and cumulative wait accounting");
    buffers.push_back(slice.buffer);
    arena.Shutdown();
    buffers.clear(); // Deleted GL names may legitimately be reused by the oracle below.

    Consumer consumer;
    const bool ready = consumer.Init(12);
    Check(ready, "persistent draw-lifetime oracle initializes");
    if (ready) {
        std::vector<std::vector<unsigned char>> queued;
        std::array<unsigned, 3> pages{};
        bool correct = true;
        for (unsigned round = 0; round < 12; ++round) {
            queued.push_back(Pattern(64 * 1024, round + 21));
            if (round == 3) {
                const auto savedWait = glad_glClientWaitSync;
                const auto before = arena.AllocatedBytes();
                timedOutFence = nullptr;
                timeoutCalls = 0;
                sameTimeoutFence = true;
                glad_glClientWaitSync = ForcedTimeout;
                for (unsigned attempt = 0; attempt < 2; ++attempt)
                    correct &= !arena.Upload(queued.back().data(), queued.back().size(), true, slice,
                                              error) &&
                               !error.empty() && !slice.buffer && arena.AllocatedBytes() == before;
                glad_glClientWaitSync = savedWait;
                Check(correct && timeoutCalls == 2 && sameTimeoutFence,
                      "repeated page-wait timeouts retry the same dependency without advancing pages");
            }
            correct &= arena.Upload(queued.back().data(), queued.back().size(), true, slice, error);
            if (!correct)
                break;
            if (round < pages.size())
                pages[round] = slice.buffer;
            else
                correct &= slice.buffer == pages[round % pages.size()];
            correct &= slice.offset == 0 && arena.AllocatedBytes() == 3 * 64 * 1024;
            consumer.Consume(slice, round);
        }
        Check(correct && pages[0] != pages[1] && pages[1] != pages[2] && pages[0] != pages[2] &&
                  consumer.Matches(queued),
              "three fenced persistent pages wrap repeatedly without changing queued GPU payloads");
        buffers.insert(buffers.end(), pages.begin(), pages.end());
    }
    const auto waitBeforeGrowth = arena.WaitNanoseconds();
    const auto maximum = Pattern(StreamArena::PageBytes, 99);
    Check(arena.Upload(maximum.data(), maximum.size(), true, slice, error) &&
              arena.AllocatedBytes() == 3 * StreamArena::PageBytes && ExactBytes(slice, maximum) &&
              arena.WaitNanoseconds() >= waitBeforeGrowth,
          "full-page persistent growth preserves payload and previously accumulated wait time");
    buffers.push_back(slice.buffer);
    const auto portable = Pattern(513, 100);
    Check(arena.Upload(portable.data(), portable.size(), false, slice, error) &&
              arena.AllocatedBytes() == 3 * StreamArena::PageBytes + portable.size() &&
              ExactBytes(slice, portable),
          "switching to portable storage preserves bounded live persistent allocations");
    buffers.push_back(slice.buffer);
    arena.Shutdown();
    Check(arena.AllocatedBytes() == 0 && arena.WaitNanoseconds() == 0 &&
              std::none_of(buffers.begin(), buffers.end(), [](auto buffer) { return glIsBuffer(buffer); }),
          "owning shutdown deletes every retained page and portable allocation");
}

void WorkingSetControls() {
    constexpr unsigned Rounds = 20;
    constexpr std::size_t Bytes = 4097, Aligned = (Bytes + 255) & ~std::size_t(255);
    constexpr std::size_t WorkingSet = Rounds * Aligned;
    const auto data = Pattern(Bytes, 7);
    std::string error;
    StreamSlice slice;
    StreamArena baseline;
    bool baselineUploaded = true, baselineWrapped = false;
    unsigned firstBuffer = 0;
    for (unsigned round = 0; round < Rounds; ++round) {
        baselineUploaded &= baseline.Upload(data.data(), data.size(), true, slice, error);
        if (!baselineUploaded)
            break;
        if (!round)
            firstBuffer = slice.buffer;
        baselineWrapped |= slice.buffer != firstBuffer;
    }
    Check(baselineUploaded && baselineWrapped,
          "individually small uploads reproduce a page boundary within the complete pass demand");
    baseline.Shutdown();

    StreamArena hinted;
    hinted.SetWorkingSetHint(WorkingSet);
    Check(hinted.AllocatedBytes() == 0,
          "a realistic pass working-set hint leaves optional GPU storage unallocated until upload");
    Consumer consumer;
    const bool ready = consumer.Init(Rounds);
    Check(ready, "working-set draw-lifetime oracle initializes");
    if (ready) {
        std::vector<std::vector<unsigned char>> queued;
        bool correct = true;
        unsigned page = 0;
        for (unsigned round = 0; round < Rounds; ++round) {
            queued.push_back(Pattern(Bytes, round + 81));
            correct &= hinted.Upload(queued.back().data(), queued.back().size(), true, slice, error);
            if (!correct)
                break;
            if (!round)
                page = slice.buffer;
            correct &= slice.buffer == page && slice.offset == round * Aligned;
            consumer.Consume(slice, round);
        }
        Check(correct && hinted.AllocatedBytes() >= 3 * WorkingSet &&
                  hinted.AllocatedBytes() <= 3 * StreamArena::PageBytes &&
                  hinted.WaitNanoseconds() == 0 && consumer.Matches(queued),
              "aligned pass demand stays on one persistent page with exact GPU payloads and no reuse waits");
    }
    const auto retainedBytes = hinted.AllocatedBytes();
    hinted.SetWorkingSetHint(0);
    Check(hinted.AllocatedBytes() == retainedBytes,
          "clearing a hint does not synchronously shrink or retire live graphics storage");
    hinted.Shutdown();

    hinted.SetWorkingSetHint(std::numeric_limits<std::size_t>::max());
    Check(hinted.Upload(data.data(), data.size(), true, slice, error) &&
              hinted.AllocatedBytes() == 3 * StreamArena::PageBytes && ExactBytes(slice, data),
          "an absurd hint is capped at the bounded page maximum and preserves upload contents");
    hinted.Shutdown();
    hinted.SetWorkingSetHint(std::numeric_limits<std::size_t>::max());
    hinted.SetWorkingSetHint(0);
    Check(hinted.Upload(data.data(), data.size(), true, slice, error) &&
              hinted.AllocatedBytes() == 3 * 64 * 1024 && ExactBytes(slice, data),
          "zero resets the pending hint and restores on-demand minimum-page allocation");
    hinted.Shutdown();
}

void NativeControls() {
    EngineHost host;
    std::string error;
    const bool initialized = host.Init("Stream arena qualification", 64, 64, false, error);
    Check(initialized, "native engine context starts for stream-storage qualification");
    if (!initialized) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return;
    }
    Check(glGetError() == GL_NO_ERROR, "engine startup leaves a clean graphics-error baseline");
    {
        StreamArena arena;
        StreamSlice slice;
        const auto data = Pattern(257, 123);
        unsigned vao = 0, unrelated = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &unrelated);
        glBindBuffer(GL_COPY_READ_BUFFER, unrelated);
        glActiveTexture(GL_TEXTURE3);
        const bool uploaded = arena.Upload(data.data(), data.size(), false, slice, error);
        GLint actualVao = 0, actualRead = 0, active = 0, array = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &actualVao);
        glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &actualRead);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array);
        Check(uploaded && actualVao == GLint(vao) && actualRead == GLint(unrelated) &&
                  active == GL_TEXTURE3 && array == GLint(slice.buffer),
              "uploads preserve unrelated VAO/read-buffer/texture state and expose their ARRAY_BUFFER binding");
        arena.Shutdown();
        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &unrelated);
        glActiveTexture(GL_TEXTURE0);
    }
    PortableControls();
    AllocationFailureControls(false);
    if (host.GraphicsCaps().modernTier && host.GraphicsCaps().bufferStorage) {
        PersistentControls();
        WorkingSetControls();
        AllocationFailureControls(true);
    } else {
        std::printf("SKIP persistent pages: selected context has no qualified modern buffer storage\n");
    }
    Check(glGetError() == GL_NO_ERROR, "stream payload, failure and lifetime controls leave no GL errors");
    host.Shutdown();
    StreamArena stale;
    const unsigned byte = 12;
    StreamSlice slice;
    Check(!stale.Upload(&byte, sizeof(byte), false, slice, error) && !error.empty() &&
              stale.AllocatedBytes() == 0,
          "uploads after context teardown fail without creating or leaking graphics objects");
    stale.Shutdown();
}
#else
void NativeControls() { std::printf("SKIP native stream readback requires the desktop GL test host\n"); }
#endif
} // namespace

int main(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && std::string(argv[1]) != "--native")) {
        std::fprintf(stderr, "usage: StreamArenaTests [--native]\n");
        return 2;
    }
    PureControls();
    if (argc == 2)
        NativeControls();
    std::printf("Stream arena: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
