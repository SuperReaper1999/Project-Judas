#pragma once
// Only Renderer and host initialization use the graphics API. ES is an optional
// compile-time backend; no Android SDK is needed by desktop builds.
#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#include <glad/gl.h>
#endif
