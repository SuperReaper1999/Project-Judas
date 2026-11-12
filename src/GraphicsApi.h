#pragma once
// Only Renderer and host initialization use the graphics API. ES is an optional
// compile-time backend; no Android SDK is needed by desktop builds.
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <glad/gl.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/html5_webgl.h>
inline bool GraphicsContextCurrent() { return emscripten_webgl_get_current_context()!=0; }
#else
#include <SDL2/SDL.h>
#include "vulkan/VulkanBackend.h"
// The Vulkan backend has no OpenGL context; its device is the "context".
inline bool GraphicsContextCurrent() { return JudasVulkan::Active() || SDL_GL_GetCurrentContext()!=nullptr; }
#endif

// Identity of the graphics context/device owning GPU objects on this thread.
// Renderer owners compare it to detect use from another context; zero = none.
inline std::uintptr_t GraphicsContextIdentity() {
#ifdef __EMSCRIPTEN__
    return static_cast<std::uintptr_t>(emscripten_webgl_get_current_context());
#else
    if (const auto vulkan = JudasVulkan::ContextIdentity()) return vulkan;
    return reinterpret_cast<std::uintptr_t>(SDL_GL_GetCurrentContext());
#endif
}
// Presentation interval of the active backend (0 = immediate, 1 = vsync).
inline void GraphicsSetSwapInterval(int interval) {
#ifndef __EMSCRIPTEN__
    if (JudasVulkan::Active()) JudasVulkan::SetSwapInterval(interval);
    else SDL_GL_SetSwapInterval(interval);
#else
    (void)interval;
#endif
}
// Interval actually in effect on the active backend (SDL_GL_GetSwapInterval semantics).
inline int GraphicsGetSwapInterval() {
#ifndef __EMSCRIPTEN__
    if (JudasVulkan::Active()) return JudasVulkan::SwapInterval();
    return SDL_GL_GetSwapInterval();
#else
    return 0;
#endif
}
