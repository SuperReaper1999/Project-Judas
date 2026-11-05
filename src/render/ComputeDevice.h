#pragma once

#include "GraphicsCapabilities.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace JudasRender {
struct ComputeBuffer {
    unsigned slot = ~0u, generation = 0;
    std::uint64_t owner = 0;
    bool IsValid() const { return slot != ~0u; }
};
struct ComputeKernel {
    unsigned slot = ~0u, generation = 0;
    std::uint64_t owner = 0;
    bool IsValid() const { return slot != ~0u; }
};
struct ComputeReadback {
    unsigned slot = ~0u, generation = 0;
    std::uint64_t owner = 0;
    bool IsValid() const { return slot != ~0u; }
};
enum class ComputeReadbackStatus { Pending, Ready, Failed };
enum class ComputeBufferUse : unsigned { Storage = 1, Vertex = 2, Indirect = 4 };
enum class ComputeAccess { ReadOnly, WriteOnly, ReadWrite };
enum class ComputeBarrier : unsigned {
    Storage = 1,
    Vertex = 2,
    Indirect = 4,
    Image = 8,
    Texture = 16,
    BufferUpdate = 32 // CPU update or diagnostic readback after a shader write.
};
constexpr ComputeBufferUse operator|(ComputeBufferUse a, ComputeBufferUse b) {
    return ComputeBufferUse(unsigned(a) | unsigned(b));
}
constexpr ComputeBarrier operator|(ComputeBarrier a, ComputeBarrier b) {
    return ComputeBarrier(unsigned(a) | unsigned(b));
}

struct ComputeBudget {
    std::size_t bytes = 64 * 1024 * 1024;
    unsigned buffers = 256, kernels = 64;
};

// Optional context/thread-owned compute resources. No allocation or GPU work
// occurs before Initialize. Portable desktop, GLES and WebGL report unsupported;
// they never execute or even reference the modern GL entry points below.
//
// Storage access is an explicit caller contract. Dispatch records writes to
// WriteOnly/ReadWrite bindings. The caller must issue the relevant Barrier before
// storage, vertex, indirect or CPU consumers. Image/texture barriers are explicit
// publication hooks; image allocation/binding is not owned by this buffer API.
// Dispatch restores the current program but owns its declared SSBO binding
// points; the next rendering pass must bind its own storage inputs. BindVertex
// and BindIndirect intentionally leave their consumer target bound.
class ComputeDevice {
  public:
    ComputeDevice();
    ~ComputeDevice();
    ComputeDevice(const ComputeDevice &) = delete;
    ComputeDevice &operator=(const ComputeDevice &) = delete;

    bool Initialize(const GraphicsCapabilities &, ComputeBudget, std::string &error);
    bool Supported() const;
    bool ValidateOwner(std::string& error) const { return Current(error); }
    const std::string &Diagnostic() const;
    bool Shutdown(std::string &error);

    ComputeBuffer CreateBuffer(std::size_t bytes, ComputeBufferUse, const void *initial,
                               std::string &error);
    bool DestroyBuffer(ComputeBuffer, std::string &error);
    bool UpdateBuffer(ComputeBuffer, std::size_t offset, const void *data, std::size_t bytes,
                      std::string &error);
    bool BindStorage(ComputeBuffer, unsigned binding, ComputeAccess, std::string &error);
    bool BindVertex(ComputeBuffer, std::string &error);
    bool BindIndirect(ComputeBuffer, std::string &error);

    ComputeKernel CreateKernel(const char *source, const char *identity, std::string &error);
    bool DestroyKernel(ComputeKernel, std::string &error);
    bool Dispatch(ComputeKernel, std::array<unsigned, 3> groups, std::string &error);
    // Small per-dispatch parameters without overwriting an in-flight SSBO.
    // The named default-block uniform must be an active uvec2.
    bool SetKernelUInt2(ComputeKernel, const char *name, std::array<unsigned, 2>, std::string &error);
    bool Barrier(ComputeBarrier, std::string &error);

    // GPU copy into at most three bounded staging slots. Poll never waits:
    // pending leaves output untouched; Ready consumes the request. Gameplay
    // must account for delayed delivery and explicit queue-full rejection.
    static constexpr unsigned MaxReadbacks = 3;
    static constexpr std::size_t MaxReadbackBytes = 256 * 1024;
    ComputeReadback RequestReadback(ComputeBuffer, std::size_t offset, std::size_t bytes,
                                    std::string &error);
    ComputeReadbackStatus PollReadback(ComputeReadback, void *output, std::size_t bytes,
                                       std::string &error);
    bool CancelReadback(ComputeReadback, std::string &error);

    // Intentionally blocking diagnostics. This is not a frame-loop transfer API.
    bool FinishForDiagnostics(std::string& error);
    bool ReadForDiagnostics(ComputeBuffer, std::size_t offset, void *output, std::size_t bytes,
                            std::string &error);
    std::size_t AllocatedBytes() const;
    unsigned LiveBuffers() const;
    unsigned LiveKernels() const;

  private:
    struct State;
    std::unique_ptr<State> state;
    std::string diagnostic;
    bool Current(std::string &error) const;
};
} // namespace JudasRender
