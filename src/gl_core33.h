#pragma once

// Minimal hand-written OpenGL 3.3 core function loader.
//
// We declare only the ~30 GL entry points Milestone 1 actually calls, and
// resolve them at runtime via SDL_GL_GetProcAddress (see gl_core33.cpp).
// This avoids pulling in a full loader-generator (glad/GLEW) and its build
// or network-fetch step for a surface this small. When later milestones
// need a much larger GL surface (textures, framebuffers, compute, etc.),
// replace this file with a generated glad loader instead of growing it by
// hand indefinitely.

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
using PFNGLUNIFORM2F = void (*)(GLint, GLfloat, GLfloat);
using PFNGLUNIFORM4F = void (*)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
using PFNGLUNIFORMMATRIX4FV = void (*)(GLint, GLsizei, GLboolean, const GLfloat*);
using PFNGLDRAWARRAYS = void (*)(GLenum, GLint, GLsizei);
using PFNGLDELETESHADER = void (*)(GLuint);
using PFNGLDELETEPROGRAM = void (*)(GLuint);
using PFNGLDELETEBUFFERS = void (*)(GLsizei, const GLuint*);
using PFNGLDELETEVERTEXARRAYS = void (*)(GLsizei, const GLuint*);

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
extern PFNGLUNIFORM2F glUniform2f;
extern PFNGLUNIFORM4F glUniform4f;
extern PFNGLUNIFORMMATRIX4FV glUniformMatrix4fv;
extern PFNGLDRAWARRAYS glDrawArrays;
extern PFNGLDELETESHADER glDeleteShader;
extern PFNGLDELETEPROGRAM glDeleteProgram;
extern PFNGLDELETEBUFFERS glDeleteBuffers;
extern PFNGLDELETEVERTEXARRAYS glDeleteVertexArrays;

// Resolves every function pointer above via SDL_GL_GetProcAddress.
// Must be called after an OpenGL context is current. Returns false (and
// logs to stderr) if any required function is missing.
bool LoadGLFunctions();
