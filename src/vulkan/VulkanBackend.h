#pragma once
// Experimental Vulkan graphics backend (branch Vulkan-Exp).
//
// The renderer's graphics calls are resolved through GLAD's function-pointer
// table. The OpenGL backend fills that table from the driver's context. This
// backend fills the same table with Judas-owned implementations which record
// native Vulkan commands: no OpenGL context, driver or translation library is
// involved. Renderer, pass and shader sources are therefore shared verbatim by
// both backends, which is what makes their output directly comparable.
//
// Selection is a run-time decision so one binary can be compared against
// itself: JUDAS_GRAPHICS=vulkan selects this backend, anything else (or a
// build without JUDAS_VULKAN) keeps OpenGL. See docs/VULKAN_BACKEND.md.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

struct SDL_Window;

namespace JudasVulkan {

// True when the process asked for Vulkan (JUDAS_GRAPHICS=vulkan), whether or
// not this build can honor it; hosts report an error rather than silently
// rendering with OpenGL.
inline bool Requested() {
    const char *value = std::getenv("JUDAS_GRAPHICS");
    return value && (std::strcmp(value, "vulkan") == 0 || std::strcmp(value, "Vulkan") == 0 ||
                     std::strcmp(value, "VULKAN") == 0 || std::strcmp(value, "vk") == 0);
}

#if defined(JUDAS_VULKAN)
// True when this build contains the backend.
bool Compiled();
// SDL window flag needed for a Vulkan surface (0 when not compiled).
std::uint32_t WindowFlag();

// Creates the instance, device, swapchain and default framebuffer for
// `window`. `glMajor.glMinor` is the feature level reported through the
// renderer's capability queries (3.3 = portable tier, 4.6 = modern tier).
bool Init(SDL_Window *window, int glMajor, int glMinor, std::string &error);
void Shutdown();
// True between a successful Init and Shutdown.
bool Active();

// Loader callback compatible with gladLoadGL. Unimplemented entry points
// return null so capability discovery reports them as unavailable.
using Proc = void (*)(void);
Proc ProcAddress(const char *name);

// Submits the frame and presents the default framebuffer (bottom-left origin,
// like the OpenGL default framebuffer) to the window.
void Present();
void SetSwapInterval(int interval);
// Drawable size of the default framebuffer in pixels.
void DrawableSize(int &width, int &height);

struct Statistics {
    std::uint64_t frames = 0, submissions = 0, renderPasses = 0, draws = 0;
    std::uint64_t pipelines = 0, descriptorSets = 0, validationMessages = 0;
    std::uint64_t bufferBytes = 0, imageBytes = 0;
};
Statistics Stats();
// Device description for logs and evidence ("Vulkan 1.4.x / llvmpipe ...").
std::string DeviceSummary();
// Opaque identity of the device for context-ownership checks (0 when inactive).
std::uintptr_t ContextIdentity();

#else
inline bool Compiled() { return false; }
inline std::uint32_t WindowFlag() { return 0; }
inline bool Init(SDL_Window *, int, int, std::string &error) {
    error = "This build does not contain the Vulkan backend (configure with JUDAS_VULKAN=ON).";
    return false;
}
inline void Shutdown() {}
inline bool Active() { return false; }
using Proc = void (*)(void);
inline Proc ProcAddress(const char *) { return nullptr; }
inline void Present() {}
inline void SetSwapInterval(int) {}
inline void DrawableSize(int &width, int &height) { width = height = 0; }
struct Statistics {
    std::uint64_t frames = 0, submissions = 0, renderPasses = 0, draws = 0;
    std::uint64_t pipelines = 0, descriptorSets = 0, validationMessages = 0;
    std::uint64_t bufferBytes = 0, imageBytes = 0;
};
inline Statistics Stats() { return {}; }
inline std::string DeviceSummary() { return "Vulkan backend not built"; }
// Opaque identity of the device for context-ownership checks (0 when inactive).
inline std::uintptr_t ContextIdentity() { return 0; }

#endif

} // namespace JudasVulkan
