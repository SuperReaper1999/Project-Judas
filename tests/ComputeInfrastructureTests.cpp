#include "EngineHost.h"
#include "GraphicsApi.h"
#include "render/ComputeDevice.h"
#include "render/ShaderLibrary.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool okay, const char *message) {
    ++checks;
    failures += !okay;
    std::printf("%s %s\n", okay ? "PASS" : "FAIL", message);
    std::fflush(stdout);
}
void Portable() {
#ifdef _WIN32
    _putenv_s("JUDAS_GL_PROFILE", "portable");
#else
    setenv("JUDAS_GL_PROFILE", "portable", 1);
#endif
}
constexpr const char *arithmetic = R"GLSL(#version 450 core
layout(local_size_x=8)in;
layout(std430,binding=0)buffer Numbers{uint values[];};
void main(){uint i=gl_GlobalInvocationID.x;if(i<64u)values[i]=i*3u+7u;}
)GLSL";
} // namespace

int main(int argc, char **argv) {
    using namespace JudasRender;
    const bool portable = argc > 1 && std::strcmp(argv[1], "--portable") == 0;
    if (portable)
        Portable();
    EngineHost host;
    std::string error;
    if (!host.Init("Compute infrastructure qualification", 128, 128, false, error)) {
        std::fprintf(stderr, "Graphics host unavailable: %s\n", error.c_str());
        return 1;
    }
    const auto caps = host.GraphicsCaps();
    ComputeDevice unavailable;
    auto disabled = caps;
    disabled.compute = false;
    Check(!unavailable.Initialize(disabled, {}, error) && !unavailable.Supported() && !error.empty(),
          "disabled compute capability rejects opt-in with an actionable diagnostic");
    Check(!unavailable.CreateBuffer(32, ComputeBufferUse::Storage, nullptr, error).IsValid() &&
              unavailable.AllocatedBytes() == 0 && unavailable.LiveBuffers() == 0,
          "unsupported compute does not allocate hidden GPU buffers");
    Check(!unavailable.CreateKernel(arithmetic, "disabled-kernel", error).IsValid() &&
              !unavailable.Dispatch({}, {{1, 1, 1}}, error),
          "unsupported kernel/dispatch operations remain unavailable");
    Check(unavailable.Shutdown(error), "unsupported device shuts down without GPU work");
    if (portable) {
        Check(caps.requestedProfile == GraphicsProfile::Portable && !caps.modernTier,
              "native EngineHost respects the forced-portable graphics policy");
        ComputeDevice forced;
        Check(!forced.Initialize(caps, {}, error) && !forced.Diagnostic().empty() &&
                  forced.AllocatedBytes() == 0,
              "forced portable cannot enable compute even when the driver exposes newer entry points");
        std::printf("Compute portable: %u checks, %u failures\n", checks, failures);
        return failures ? 1 : 0;
    }
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    std::printf("Compute backend unavailable: GLES3/WebGL2 remains a supported portable renderer\n");
    return failures ? 1 : 77;
#else
    if (!caps.modernTier || !caps.compute || !caps.storageBuffers) {
        std::printf("Compute backend unavailable: %s\n", GraphicsCapabilitiesSummary(caps).c_str());
        return failures ? 1 : 77;
    }
    {
        ComputeDevice source, receiver, small;
        Check(source.Initialize(caps, {}, error) && receiver.Initialize(caps, {}, error) &&
                  small.Initialize(caps, {16, 1, 1}, error), "immutable alias owners initialize");
        std::array<unsigned,64> original{}; original.fill(123);
        const auto buffer = source.CreateBuffer(sizeof(original), ComputeBufferUse::Storage, original.data(), error);
        Check(source.BindStorage(buffer, 0, ComputeAccess::ReadWrite, error),
              "source writable binding predates immutable sharing");
        Check(!small.ShareReadOnlyBuffer(source, buffer, error).IsValid() && small.LiveBuffers() == 0 &&
                  !source.ShareReadOnlyBuffer(source, buffer, error).IsValid(),
              "alias budget and self-sharing reject without allocations");
        const auto alias = receiver.ShareReadOnlyBuffer(source, buffer, error);
        Check(alias.IsValid() && receiver.AllocatedBytes() == sizeof(original),
              "read-only alias reserves bounded storage without uploading");
        const auto writer = source.CreateKernel(arithmetic, "immutable-prebound-writer", error);
        Check(!source.UpdateBuffer(buffer, 0, original.data(), sizeof(original), error) &&
                  !receiver.UpdateBuffer(alias, 0, original.data(), sizeof(original), error) &&
                  !receiver.BindStorage(alias, 0, ComputeAccess::ReadWrite, error) &&
                  !source.Dispatch(writer, {{8,1,1}}, error),
              "sharing seals both handles and rejects an already-bound writer at dispatch");
        Check(source.Shutdown(error), "original owner can retire before its aliases");
        std::array<unsigned,64> read{};
        Check(receiver.ReadForDiagnostics(alias, 0, read.data(), sizeof(read), error) && read == original,
              "alias retains actual GPU payload after original device teardown");
        Check(receiver.DestroyBuffer(alias, error) && receiver.LiveBuffers() == 0 &&
                  !receiver.ReadForDiagnostics(alias, 0, read.data(), sizeof(read), error),
              "last alias retires storage and stale handle rejects");
        Check(receiver.Shutdown(error) && small.Shutdown(error), "alias owners release cleanly");
    }
    {
        ComputeDevice queried;
        auto incompleteSnapshot = caps;
        incompleteSnapshot.limits = {};
        Check(queried.Initialize(incompleteSnapshot, {4096, 8, 4}, error),
              "compute queries numeric limits from its actual context rather than a caller snapshot");
        Check(queried.Shutdown(error), "queried-limit control releases its optional owner");
        auto oversizedSnapshot = caps;
        oversizedSnapshot.limits.storageBufferBindings = caps.limits.storageBufferBindings + 1;
        Check(queried.Initialize(oversizedSnapshot, {4096, 8, 4}, error),
              "caller capability metadata cannot enlarge actual context limits");
        const auto buffer =
            queried.CreateBuffer(32, ComputeBufferUse::Storage, nullptr, error);
        Check(buffer.IsValid() &&
                  !queried.BindStorage(buffer, unsigned(caps.limits.storageBufferBindings),
                                       ComputeAccess::ReadOnly, error),
              "forged extra storage binding is rejected at the actual driver boundary");
        Check(queried.Shutdown(error), "forged-limit control releases all GPU allocations");
    }
    ComputeDevice device;
    Check(device.Initialize(caps, {4096, 8, 4}, error) && device.Supported(),
          "real native modern context initializes the optional compute owner");
    if (!device.Supported())
        return 1;
    Check(!device.Initialize(caps, {4096, 8, 4}, error) && device.AllocatedBytes() == 0,
          "double initialization is rejected without replacing the live owner");
    Check(!device.CreateBuffer(0, ComputeBufferUse::Storage, nullptr, error).IsValid() &&
              !device.CreateBuffer(4097, ComputeBufferUse::Storage, nullptr, error).IsValid() &&
              device.AllocatedBytes() == 0,
          "zero-size and over-budget buffers are rejected before publication");
    Check(!device.CreateBuffer(16, static_cast<ComputeBufferUse>(128), nullptr, error).IsValid(),
          "unknown buffer-use bits are rejected");
    std::array<unsigned, 64> values{};
    values.fill(0xdeadbeef);
    const auto numbers =
        device.CreateBuffer(sizeof(values), ComputeBufferUse::Storage, values.data(), error);
    Check(numbers.IsValid() && device.AllocatedBytes() == sizeof(values) && device.LiveBuffers() == 1,
          "bounded compute buffer contains the declared native allocation");
    const auto kernel = device.CreateKernel(arithmetic, "qualification/arithmetic.comp", error);
    Check(kernel.IsValid() && device.LiveKernels() == 1,
          "native compute shader compiles, links and reflects its local/storage contract");
    if (!numbers.IsValid() || !kernel.IsValid())
        return 1;
    Check(!device.Dispatch(kernel, {{8, 1, 1}}, error),
          "missing required storage binding rejects dispatch");
    Check(!device.BindStorage(numbers, unsigned(caps.limits.storageBufferBindings),
                              ComputeAccess::ReadWrite, error),
          "storage binding cannot exceed the queried device limit");
    Check(device.BindStorage(numbers, 0, ComputeAccess::WriteOnly, error),
          "compute buffer is assigned explicitly to its linked storage binding");
    Check(!device.Dispatch(kernel, {{0, 1, 1}}, error) &&
              !device.Dispatch(kernel, {{unsigned(caps.limits.computeGroupCount[0]) + 1, 1, 1}}, error),
          "zero and excessive dispatch group counts are rejected");
    Check(device.ReadForDiagnostics(numbers, 0, values.data(), sizeof(values), error) &&
              values[0] == 0xdeadbeef,
          "rejected dispatches preserve the original GPU payload");
    Check(device.Dispatch(kernel, {{8, 1, 1}}, error),
          "known-answer arithmetic kernel executes on the GPU");
    Check(!device.ReadForDiagnostics(numbers, 0, values.data(), sizeof(values), error),
          "CPU diagnostic consumer cannot bypass the required publication barrier");
    Check(!device.Dispatch(kernel, {{8, 1, 1}}, error),
          "a second storage writer cannot bypass a write-after-write barrier");
    Check(!device.Barrier(static_cast<ComputeBarrier>(0), error) &&
              !device.Barrier(static_cast<ComputeBarrier>(128), error),
          "empty and unknown barrier masks are rejected");
    Check(device.Barrier(ComputeBarrier::Storage | ComputeBarrier::BufferUpdate, error) &&
              device.ReadForDiagnostics(numbers, 0, values.data(), sizeof(values), error),
          "explicit storage and buffer-update barriers publish diagnostic results");
    bool correct = true;
    for (unsigned i = 0; i < values.size(); ++i)
        correct &= values[i] == i * 3u + 7u;
    Check(correct, "all 64 GPU outputs match the independent integer arithmetic oracle");
    unsigned replacement = 91;
    Check(!device.UpdateBuffer(numbers, sizeof(values) - 1, &replacement, sizeof(replacement), error) &&
              !device.ReadForDiagnostics(numbers, sizeof(values), &replacement, sizeof(replacement),
                                         error),
          "overflowing update/read ranges are rejected without touching adjacent memory");
    Check(device.UpdateBuffer(numbers, 0, &replacement, sizeof(replacement), error) &&
              device.ReadForDiagnostics(numbers, 0, values.data(), sizeof(values), error) &&
              values[0] == 91,
          "in-bounds CPU updates use the same owned allocation");
    const auto programsBefore = device.LiveKernels();
    Check(!device.CreateKernel("#version 450 core\nthis is not GLSL;", "qualification/malformed.comp",
                               error)
                  .IsValid() &&
              device.LiveKernels() == programsBefore,
          "malformed shader cannot publish or leak a partial compute kernel");
    const auto excessiveLocal = std::string("#version 450 core\nlayout(local_size_x=") +
                                std::to_string(unsigned(caps.limits.computeGroupSize[0]) + 1u) +
                                ")in;void main(){}";
    Check(!device.CreateKernel(excessiveLocal.c_str(), "qualification/local-limit.comp", error)
                  .IsValid() &&
              device.LiveKernels() == programsBefore,
          "invalid linked local workgroup limits are rejected without replacing existing kernels");
    bool wrongThreadAccepted = true;
    std::thread worker([&] {
        std::string e;
        wrongThreadAccepted = device.BindStorage(numbers, 0, ComputeAccess::ReadOnly, e);
    });
    worker.join();
    Check(!wrongThreadAccepted, "compute resources cannot be manipulated from a foreign thread");
    Check(SDL_GL_MakeCurrent(nullptr, nullptr) == 0,
          "test temporarily releases its actual native context");
    Check(!device.BindStorage(numbers, 0, ComputeAccess::ReadOnly, error),
          "context-free operations cannot touch the device's GPU state");
    Check(SDL_GL_MakeCurrent(host.GetWindow().NativeWindow(), host.GetWindow().NativeGLContext()) == 0,
          "test restores the owning native graphics context");

    constexpr const char *triangle = R"GLSL(#version 450 core
layout(local_size_x=1)in;
layout(std430,binding=0)buffer Vertices{vec2 positions[];};
layout(std430,binding=1)buffer Commands{uint commands[4];};
void main(){positions[0]=vec2(-0.8,-0.8);positions[1]=vec2(0.8,-0.8);positions[2]=vec2(0,0.8);
commands[0]=3u;commands[1]=1u;commands[2]=0u;commands[3]=0u;}
)GLSL";
    const auto vertices = device.CreateBuffer(
        6 * sizeof(float), ComputeBufferUse::Storage | ComputeBufferUse::Vertex, nullptr, error);
    const auto commands = device.CreateBuffer(
        4 * sizeof(unsigned), ComputeBufferUse::Storage | ComputeBufferUse::Indirect, nullptr, error);
    const auto drawKernel = device.CreateKernel(triangle, "qualification/generated-draw.comp", error);
    Check(vertices.IsValid() && commands.IsValid() && drawKernel.IsValid(),
          "compute owns bounded vertex and indirect-command allocations");
    Check(device.BindStorage(vertices, 0, ComputeAccess::WriteOnly, error) &&
              device.BindStorage(commands, 1, ComputeAccess::WriteOnly, error) &&
              device.Dispatch(drawKernel, {{1, 1, 1}}, error),
          "GPU kernel writes actual triangle vertices and an indirect draw command");
    Check(!device.BindVertex(vertices, error) && !device.BindIndirect(commands, error),
          "vertex and indirect consumers both reject unpublished shader writes");
    Check(device.Barrier(ComputeBarrier::Vertex | ComputeBarrier::Indirect, error) &&
              device.BindVertex(vertices, error) && device.BindIndirect(commands, error),
          "explicit vertex/command barriers enable the actual draw consumers");
    const char *vs = "#version 450 core\nlayout(location=0)in vec2 position;void "
                     "main(){gl_Position=vec4(position,0,1);}";
    const char *fs = "#version 450 core\nout vec4 color;void main(){color=vec4(0,1,0,1);}";
    GLuint vertex = 0, fragment = 0, program = 0;
    const bool built =
        Shaders::CompileShader(GL_VERTEX_SHADER, vs, vertex, Shaders::Profile::Desktop450,
                               "qualification/generated.vert") &&
        Shaders::CompileShader(GL_FRAGMENT_SHADER, fs, fragment, Shaders::Profile::Desktop450,
                               "qualification/generated.frag") &&
        Shaders::LinkProgram(vertex, fragment, program, "qualification/generated-draw");
    if (vertex)
        glDeleteShader(vertex);
    if (fragment)
        glDeleteShader(fragment);
    Check(built, "ordinary graphics stages link for the generated GPU draw");
    auto &renderer = host.GetRenderer();
    auto target = renderer.CreateRenderTarget(32, 32, error);
    Check(target.IsValid() && renderer.BeginRenderTarget(target),
          "known-answer framebuffer is an ordinary owned render target");
    if (built && target.IsValid()) {
        glViewport(0, 0, 32, 32);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        std::array<unsigned char, 4> before{}, after{};
        glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, before.data());
        Check(before[0] == 0 && before[1] == 0 && before[2] == 0,
              "negative framebuffer control is black before the GPU-generated draw");
        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        device.BindVertex(vertices, error);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        device.BindIndirect(commands, error);
        glUseProgram(program);
        glDrawArraysIndirect(GL_TRIANGLES, nullptr);
        glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, after.data());
        Check(after[0] == 0 && after[1] == 255 && after[2] == 0 && after[3] == 255,
              "compute-generated vertices and indirect command produce the independently expected "
              "green pixel");
        glBindVertexArray(0);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        glUseProgram(0);
        glDeleteVertexArrays(1, &vao);
    }
    renderer.EndRenderTarget();
    renderer.DestroyRenderTarget(target);
    if (program)
        glDeleteProgram(program);
    Check(device.Barrier(ComputeBarrier::Image | ComputeBarrier::Texture, error),
          "image and texture publication barrier categories are explicit supported hooks");
    const auto oldNumbers = numbers;
    Check(device.DestroyBuffer(numbers, error), "destroy releases the known allocation");
    const auto recycled = device.CreateBuffer(16, ComputeBufferUse::Storage, nullptr, error);
    Check(recycled.IsValid() && recycled.slot == oldNumbers.slot &&
              recycled.generation != oldNumbers.generation &&
              !device.BindStorage(oldNumbers, 0, ComputeAccess::ReadOnly, error),
          "recycled buffer slots cannot be accessed through stale generations");
    const auto oldKernel = kernel;
    Check(device.DestroyKernel(kernel, error), "destroy releases the original compute kernel");
    const auto recycledKernel = device.CreateKernel(arithmetic, "qualification/recycled.comp", error);
    Check(recycledKernel.IsValid() && recycledKernel.slot == oldKernel.slot &&
              recycledKernel.generation != oldKernel.generation &&
              !device.Dispatch(oldKernel, {{1, 1, 1}}, error),
          "recycled kernel slots reject stale program generations");
    ComputeDevice other;
    Check(other.Initialize(caps, {1024, 2, 1}, error) &&
              !other.BindStorage(recycled, 0, ComputeAccess::ReadOnly, error),
          "an otherwise matching slot cannot cross compute-device ownership");
    Check(other.Shutdown(error) && device.Shutdown(error) && device.AllocatedBytes() == 0 &&
              device.LiveBuffers() == 0 && device.LiveKernels() == 0,
          "owning-context shutdown retires all compute allocations and kernels");
    Check(device.Initialize(caps, {1024, 2, 1}, error) &&
              !device.BindStorage(recycled, 0, ComputeAccess::ReadOnly, error),
          "old handles remain invalid after an entire device lifecycle restarts");
    device.Shutdown(error);
    std::printf("Compute infrastructure: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
#endif
}
