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
inline bool GraphicsContextCurrent() { return SDL_GL_GetCurrentContext()!=nullptr; }
#endif
