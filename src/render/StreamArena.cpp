#include "StreamArena.h"
#include "GraphicsApi.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace JudasRender {
namespace {
std::uintptr_t CurrentContext() {
#ifdef __EMSCRIPTEN__
    return std::uintptr_t(emscripten_webgl_get_current_context());
#else
    return reinterpret_cast<std::uintptr_t>(SDL_GL_GetCurrentContext());
#endif
}
} // namespace

struct StreamArena::Storage {
    struct Page {
        unsigned buffer = 0;
        void *mapped = nullptr;
    };
    std::array<Page, 3> pages{};
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    std::array<GLsync, 3> fences{};
#endif
    unsigned current = 0, portable = 0;
    std::size_t cursor = 0, portableBytes = 0, capacity = 0, workingSetHint = 0;
    std::uint64_t waited = 0;
    std::uintptr_t context = 0;
    std::thread::id thread;
    bool initialized = false;

    bool OwnContext(std::string &error) {
        const auto active = CurrentContext();
        if (!active) {
            error = "Stream uploads require a current owning graphics context.";
            return false;
        }
        if (context && (context != active || thread != std::this_thread::get_id())) {
            error = "Stream storage cannot be used from another context or thread.";
            return false;
        }
        if (!context) {
            context = active;
            thread = std::this_thread::get_id();
        }
        return true;
    }
    bool HasResources() const {
        return portable || std::any_of(pages.begin(), pages.end(),
                                       [](const auto &page) { return page.buffer != 0; });
    }
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    static void ReleasePages(std::array<Page, 3> &retired) {
        for (auto &page : retired) {
            if (page.mapped) {
                glBindBuffer(GL_ARRAY_BUFFER, page.buffer);
                glUnmapBuffer(GL_ARRAY_BUFFER);
            }
            if (page.buffer)
                glDeleteBuffers(1, &page.buffer);
            page = {};
        }
    }
    void ReleasePersistent() {
        for (auto &fence : fences) {
            if (fence)
                glDeleteSync(fence);
            fence = nullptr;
        }
        ReleasePages(pages);
        initialized = false;
        current = 0;
        cursor = capacity = 0;
    }
    bool Wait(GLsync fence, const char *reason, std::string &error) {
        const auto begin = std::chrono::steady_clock::now();
        const GLenum result = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
        waited += std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::steady_clock::now() - begin)
                      .count();
        if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED)
            return true;
        error = reason;
        return false;
    }
#endif
};

StreamArena::StreamArena() : storage(std::make_unique<Storage>()) {}
StreamArena::~StreamArena() {
    // Never dispatch GL from a destructor which can run after context teardown.
    if (storage->HasResources())
        std::fprintf(stderr, "[Graphics] StreamArena owner omitted Shutdown before context teardown; "
                             "destruction performs no GL calls.\n");
}

void StreamArena::SetWorkingSetHint(std::size_t bytes) {
    storage->workingSetHint = std::min(bytes, PageBytes);
}

bool StreamArena::Upload(const void *data, std::size_t bytes, bool persistent, StreamSlice &slice,
                         std::string &error) {
    error.clear();
    slice = {};
    if (!data || !bytes || bytes > PageBytes) {
        error = "Stream upload must contain 1 byte to one bounded page.";
        return false;
    }
    auto &s = *storage;
    if (!s.OwnContext(error))
        return false;
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    if (persistent && glBufferStorage && glMapBufferRange && glUnmapBuffer && glFenceSync &&
        glClientWaitSync && glDeleteSync) {
        const auto desiredCapacity = std::max(bytes, s.workingSetHint);
        if (!s.initialized || desiredCapacity > s.capacity) {
            std::size_t capacity = 64 * 1024;
            while (capacity < desiredCapacity)
                capacity *= 2;
            std::array<Storage::Page, 3> replacement{};
            for (auto &page : replacement) {
                glGenBuffers(1, &page.buffer);
                if (!page.buffer) {
                    Storage::ReleasePages(replacement);
                    error = "Cannot create a persistent stream buffer.";
                    return false;
                }
                glBindBuffer(GL_ARRAY_BUFFER, page.buffer);
                glBufferStorage(GL_ARRAY_BUFFER, GLsizeiptr(capacity), nullptr,
                                GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
                page.mapped = glMapBufferRange(GL_ARRAY_BUFFER, 0, GLsizeiptr(capacity),
                                              GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT |
                                                  GL_MAP_COHERENT_BIT);
                if (!page.mapped) {
                    Storage::ReleasePages(replacement);
                    error = "Persistent stream mapping failed.";
                    return false;
                }
            }
            if (s.initialized) {
                const auto last = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
                if (!last) {
                    Storage::ReleasePages(replacement);
                    error = "Cannot fence stream growth.";
                    return false;
                }
                const bool completed =
                    s.Wait(last, "GPU did not complete bounded stream growth.", error);
                glDeleteSync(last);
                if (!completed) {
                    Storage::ReleasePages(replacement);
                    return false;
                }
            }
            // Publish replacement pages only after allocation and completion
            // succeed. Failed growth retains existing slices, fences and size.
            // Cumulative waits, the CPU hint and portable storage remain owned.
            s.ReleasePersistent();
            s.pages = replacement;
            s.capacity = capacity;
            s.initialized = true;
        }
        const auto aligned = (s.cursor + 255) & ~std::size_t(255);
        if (aligned > s.capacity - bytes) {
            const auto next = (s.current + 1) % s.pages.size();
            // Do not advance the active page or replace its fence until the
            // destination is available. A timeout must remain safely retryable.
            if (const auto fence = s.fences[next]) {
                if (!s.Wait(fence, "GPU did not complete bounded stream page reuse.", error))
                    return false;
                glDeleteSync(fence);
                s.fences[next] = nullptr;
            }
            const auto sealed = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            if (!sealed) {
                error = "Cannot fence stream page.";
                return false;
            }
            s.fences[s.current] = sealed;
            s.current = unsigned(next);
            s.cursor = 0;
        } else {
            s.cursor = aligned;
        }
        const auto &page = s.pages[s.current];
        std::memcpy(static_cast<unsigned char *>(page.mapped) + s.cursor, data, bytes);
        slice = {page.buffer, s.cursor};
        s.cursor += bytes;
        // Mapped writes need no binding, but both paths expose one postcondition.
        glBindBuffer(GL_ARRAY_BUFFER, page.buffer);
        return true;
    }
#else
    (void)persistent;
#endif
    const bool created = !s.portable;
    if (created) {
        glGenBuffers(1, &s.portable);
        if (!s.portable) {
            error = "Cannot create a portable stream buffer.";
            return false;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, s.portable);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(bytes), data, GL_STREAM_DRAW);
    GLint allocated = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &allocated);
    if (allocated != GLint(bytes)) {
        if (created) {
            glDeleteBuffers(1, &s.portable);
            s.portable = 0;
        }
        error = "Cannot allocate portable stream storage.";
        return false;
    }
    s.portableBytes = bytes;
    slice = {s.portable, 0};
    return true;
}

void StreamArena::Shutdown() {
    auto &s = *storage;
    std::string error;
    if (s.HasResources() && !s.OwnContext(error)) {
        std::fprintf(stderr, "[Graphics] %s\n", error.c_str());
        return;
    }
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    s.ReleasePersistent();
#endif
    if (s.portable)
        glDeleteBuffers(1, &s.portable);
    s = {};
}

std::size_t StreamArena::AllocatedBytes() const {
    return (storage->initialized ? storage->capacity * storage->pages.size() : 0) +
           storage->portableBytes;
}
std::uint64_t StreamArena::WaitNanoseconds() const { return storage->waited; }
} // namespace JudasRender
