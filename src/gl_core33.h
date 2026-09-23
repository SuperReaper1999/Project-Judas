#pragma once

// Minimal hand-written OpenGL 3.3 core function loader.
//
// We declare only the GL entry points the engine actually calls, and
// resolve them at runtime via SDL_GL_GetProcAddress (see gl_core33.cpp).
// This avoids pulling in a full loader-generator (glad/GLEW) and its build
// or network-fetch step for a surface this small. This decision is
// re-evaluated each time new GL surface is needed (see docs/ARCHITECTURE.md)
// and kept deliberately each time so far; when a future milestone needs a
// much larger GL surface (framebuffers, compute, etc.), replace this file
// with a generated glad loader instead of growing it by hand indefinitely.
//
// Milestone 9 added 10 functions (textures, element-buffer drawing, a
// couple more uniform setters) on top of the ~31 already here — reconsidered
// against the same "still small and clear?" question, and kept: it's still
// a flat, readable list of entry points this engine actually calls, not
// meaningfully harder to maintain than before. See docs/ARCHITECTURE.md,
// "Milestone 9," for the explicit re-evaluation.

#include <cstddef>

using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLint = int;
using GLsizei = int;
using GLuint = unsigned int;
using GLfloat = float;
using GLbitfield = unsigned int;
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;

constexpr GLboolean GL_FALSE = 0;
constexpr GLboolean GL_TRUE = 1;

constexpr GLbitfield GL_COLOR_BUFFER_BIT = 0x00004000;
constexpr GLbitfield GL_DEPTH_BUFFER_BIT = 0x00000100;
constexpr GLenum GL_DEPTH_TEST = 0x0B71;
constexpr GLenum GL_LESS = 0x0201;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_STATIC_DRAW = 0x88E4;
constexpr GLenum GL_FLOAT = 0x1406;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
constexpr GLenum GL_RGB = 0x1907;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;

// Milestone 9: textures + element-buffer (indexed) drawing.
constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
constexpr GLenum GL_UNSIGNED_INT = 0x1405;
constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLenum GL_REPEAT = 0x2901;
constexpr GLenum GL_LINEAR = 0x2601;
constexpr GLenum GL_LINEAR_MIPMAP_LINEAR = 0x2703;
constexpr GLenum GL_RGBA = 0x1908;

using PFNGLVIEWPORT = void (*)(GLint, GLint, GLsizei, GLsizei);
using PFNGLCLEARCOLOR = void (*)(GLfloat, GLfloat, GLfloat, GLfloat);
using PFNGLCLEAR = void (*)(GLbitfield);
using PFNGLENABLE = void (*)(GLenum);
using PFNGLDEPTHFUNC = void (*)(GLenum);
using PFNGLGENBUFFERS = void (*)(GLsizei, GLuint*);
using PFNGLBINDBUFFER = void (*)(GLenum, GLuint);
using PFNGLBUFFERDATA = void (*)(GLenum, GLsizeiptr, const void*, GLenum);
using PFNGLGENVERTEXARRAYS = void (*)(GLsizei, GLuint*);
using PFNGLBINDVERTEXARRAY = void (*)(GLuint);
using PFNGLVERTEXATTRIBPOINTER = void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using PFNGLENABLEVERTEXATTRIBARRAY = void (*)(GLuint);
using PFNGLCREATESHADER = GLuint (*)(GLenum);
using PFNGLSHADERSOURCE = void (*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
using PFNGLCOMPILESHADER = void (*)(GLuint);
using PFNGLGETSHADERIV = void (*)(GLuint, GLenum, GLint*);
using PFNGLGETSHADERINFOLOG = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
using PFNGLCREATEPROGRAM = GLuint (*)();
using PFNGLATTACHSHADER = void (*)(GLuint, GLuint);
using PFNGLLINKPROGRAM = void (*)(GLuint);
using PFNGLGETPROGRAMIV = void (*)(GLuint, GLenum, GLint*);
using PFNGLGETPROGRAMINFOLOG = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
using PFNGLUSEPROGRAM = void (*)(GLuint);
using PFNGLGETUNIFORMLOCATION = GLint (*)(GLuint, const GLchar*);
using PFNGLUNIFORM1F = void (*)(GLint, GLfloat);
using PFNGLUNIFORM2F = void (*)(GLint, GLfloat, GLfloat);
using PFNGLUNIFORM4F = void (*)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
using PFNGLUNIFORMMATRIX4FV = void (*)(GLint, GLsizei, GLboolean, const GLfloat*);
using PFNGLDRAWARRAYS = void (*)(GLenum, GLint, GLsizei);
using PFNGLDELETESHADER = void (*)(GLuint);
using PFNGLDELETEPROGRAM = void (*)(GLuint);
using PFNGLDELETEBUFFERS = void (*)(GLsizei, const GLuint*);
using PFNGLDELETEVERTEXARRAYS = void (*)(GLsizei, const GLuint*);
using PFNGLREADPIXELS = void (*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);

// Milestone 9 additions.
using PFNGLDRAWELEMENTS = void (*)(GLenum, GLsizei, GLenum, const void*);
using PFNGLGENTEXTURES = void (*)(GLsizei, GLuint*);
using PFNGLBINDTEXTURE = void (*)(GLenum, GLuint);
using PFNGLTEXIMAGE2D = void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                                  const void*);
using PFNGLTEXPARAMETERI = void (*)(GLenum, GLenum, GLint);
using PFNGLGENERATEMIPMAP = void (*)(GLenum);
using PFNGLDELETETEXTURES = void (*)(GLsizei, const GLuint*);
using PFNGLUNIFORM1I = void (*)(GLint, GLint);
using PFNGLUNIFORM3F = void (*)(GLint, GLfloat, GLfloat, GLfloat);
using PFNGLUNIFORMMATRIX3FV = void (*)(GLint, GLsizei, GLboolean, const GLfloat*);

// Milestone 13: alpha-blended, depth-test-disabled UI overlay rendering
// (see Renderer::BeginUIFrame/EndUIFrame) — the first thing in this engine
// that needs to turn a GL capability OFF at runtime (glDisable) or blend
// translucent fragments (glBlendFunc) rather than just turning depth
// testing on once at Init and leaving it there.
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
using PFNGLDISABLE = void (*)(GLenum);
using PFNGLBLENDFUNC = void (*)(GLenum, GLenum);

// Milestone 15: shadow mapping — depth-only framebuffer objects (one per
// shadow-casting light) and multi-texture binding (the main lit shader now
// samples up to 3 shadow-map textures in addition to the existing diffuse
// texture, all bound simultaneously — see Renderer::DrawMesh). Every
// constant/function here exists for exactly this purpose; nothing here is
// speculative GL surface.
constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
constexpr GLenum GL_DEPTH_ATTACHMENT = 0x8D00;
constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr GLenum GL_DEPTH_COMPONENT = 0x1902;
constexpr GLenum GL_NEAREST = 0x2600;
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_NONE = 0;
constexpr GLenum GL_TEXTURE0 = 0x84C0;  // GL_TEXTUREi = GL_TEXTURE0 + i, per the GL spec
using PFNGLGENFRAMEBUFFERS = void (*)(GLsizei, GLuint*);
using PFNGLBINDFRAMEBUFFER = void (*)(GLenum, GLuint);
using PFNGLFRAMEBUFFERTEXTURE2D = void (*)(GLenum, GLenum, GLenum, GLuint, GLint);
using PFNGLCHECKFRAMEBUFFERSTATUS = GLenum (*)(GLenum);
using PFNGLDELETEFRAMEBUFFERS = void (*)(GLsizei, const GLuint*);
using PFNGLDRAWBUFFER = void (*)(GLenum);
using PFNGLREADBUFFER = void (*)(GLenum);
using PFNGLACTIVETEXTURE = void (*)(GLenum);

extern PFNGLVIEWPORT glViewport;
extern PFNGLCLEARCOLOR glClearColor;
extern PFNGLCLEAR glClear;
extern PFNGLENABLE glEnable;
extern PFNGLDEPTHFUNC glDepthFunc;
extern PFNGLGENBUFFERS glGenBuffers;
extern PFNGLBINDBUFFER glBindBuffer;
extern PFNGLBUFFERDATA glBufferData;
extern PFNGLGENVERTEXARRAYS glGenVertexArrays;
extern PFNGLBINDVERTEXARRAY glBindVertexArray;
extern PFNGLVERTEXATTRIBPOINTER glVertexAttribPointer;
extern PFNGLENABLEVERTEXATTRIBARRAY glEnableVertexAttribArray;
extern PFNGLCREATESHADER glCreateShader;
extern PFNGLSHADERSOURCE glShaderSource;
extern PFNGLCOMPILESHADER glCompileShader;
extern PFNGLGETSHADERIV glGetShaderiv;
extern PFNGLGETSHADERINFOLOG glGetShaderInfoLog;
extern PFNGLCREATEPROGRAM glCreateProgram;
extern PFNGLATTACHSHADER glAttachShader;
extern PFNGLLINKPROGRAM glLinkProgram;
extern PFNGLGETPROGRAMIV glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOG glGetProgramInfoLog;
extern PFNGLUSEPROGRAM glUseProgram;
extern PFNGLGETUNIFORMLOCATION glGetUniformLocation;
extern PFNGLUNIFORM1F glUniform1f;
extern PFNGLUNIFORM2F glUniform2f;
extern PFNGLUNIFORM4F glUniform4f;
extern PFNGLUNIFORMMATRIX4FV glUniformMatrix4fv;
extern PFNGLDRAWARRAYS glDrawArrays;
extern PFNGLDELETESHADER glDeleteShader;
extern PFNGLDELETEPROGRAM glDeleteProgram;
extern PFNGLDELETEBUFFERS glDeleteBuffers;
extern PFNGLDELETEVERTEXARRAYS glDeleteVertexArrays;
extern PFNGLREADPIXELS glReadPixels;

// Milestone 9 additions.
extern PFNGLDRAWELEMENTS glDrawElements;
extern PFNGLGENTEXTURES glGenTextures;
extern PFNGLBINDTEXTURE glBindTexture;
extern PFNGLTEXIMAGE2D glTexImage2D;
extern PFNGLTEXPARAMETERI glTexParameteri;
extern PFNGLGENERATEMIPMAP glGenerateMipmap;
extern PFNGLDELETETEXTURES glDeleteTextures;
extern PFNGLUNIFORM1I glUniform1i;
extern PFNGLUNIFORM3F glUniform3f;
extern PFNGLUNIFORMMATRIX3FV glUniformMatrix3fv;

// Milestone 13 additions.
extern PFNGLDISABLE glDisable;
extern PFNGLBLENDFUNC glBlendFunc;

// Milestone 15 additions.
extern PFNGLGENFRAMEBUFFERS glGenFramebuffers;
extern PFNGLBINDFRAMEBUFFER glBindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2D glFramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUS glCheckFramebufferStatus;
extern PFNGLDELETEFRAMEBUFFERS glDeleteFramebuffers;
extern PFNGLDRAWBUFFER glDrawBuffer;
extern PFNGLREADBUFFER glReadBuffer;
extern PFNGLACTIVETEXTURE glActiveTexture;

// Resolves every function pointer above via SDL_GL_GetProcAddress.
// Must be called after an OpenGL context is current. Returns false (and
// logs to stderr) if any required function is missing.
bool LoadGLFunctions();
