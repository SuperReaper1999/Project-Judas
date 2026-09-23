#include "gl_core33.h"

#include <SDL2/SDL.h>
#include <cstdio>

PFNGLVIEWPORT glViewport = nullptr;
PFNGLCLEARCOLOR glClearColor = nullptr;
PFNGLCLEAR glClear = nullptr;
PFNGLENABLE glEnable = nullptr;
PFNGLDEPTHFUNC glDepthFunc = nullptr;
PFNGLGENBUFFERS glGenBuffers = nullptr;
PFNGLBINDBUFFER glBindBuffer = nullptr;
PFNGLBUFFERDATA glBufferData = nullptr;
PFNGLGENVERTEXARRAYS glGenVertexArrays = nullptr;
PFNGLBINDVERTEXARRAY glBindVertexArray = nullptr;
PFNGLVERTEXATTRIBPOINTER glVertexAttribPointer = nullptr;
PFNGLENABLEVERTEXATTRIBARRAY glEnableVertexAttribArray = nullptr;
PFNGLCREATESHADER glCreateShader = nullptr;
PFNGLSHADERSOURCE glShaderSource = nullptr;
PFNGLCOMPILESHADER glCompileShader = nullptr;
PFNGLGETSHADERIV glGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOG glGetShaderInfoLog = nullptr;
PFNGLCREATEPROGRAM glCreateProgram = nullptr;
PFNGLATTACHSHADER glAttachShader = nullptr;
PFNGLLINKPROGRAM glLinkProgram = nullptr;
PFNGLGETPROGRAMIV glGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOG glGetProgramInfoLog = nullptr;
PFNGLUSEPROGRAM glUseProgram = nullptr;
PFNGLGETUNIFORMLOCATION glGetUniformLocation = nullptr;
PFNGLUNIFORM1F glUniform1f = nullptr;
PFNGLUNIFORM2F glUniform2f = nullptr;
PFNGLUNIFORM4F glUniform4f = nullptr;
PFNGLUNIFORMMATRIX4FV glUniformMatrix4fv = nullptr;
PFNGLDRAWARRAYS glDrawArrays = nullptr;
PFNGLDELETESHADER glDeleteShader = nullptr;
PFNGLDELETEPROGRAM glDeleteProgram = nullptr;
PFNGLDELETEBUFFERS glDeleteBuffers = nullptr;
PFNGLDELETEVERTEXARRAYS glDeleteVertexArrays = nullptr;
PFNGLREADPIXELS glReadPixels = nullptr;

// Milestone 9 additions.
PFNGLDRAWELEMENTS glDrawElements = nullptr;
PFNGLGENTEXTURES glGenTextures = nullptr;
PFNGLBINDTEXTURE glBindTexture = nullptr;
PFNGLTEXIMAGE2D glTexImage2D = nullptr;
PFNGLTEXPARAMETERI glTexParameteri = nullptr;
PFNGLGENERATEMIPMAP glGenerateMipmap = nullptr;
PFNGLDELETETEXTURES glDeleteTextures = nullptr;
PFNGLUNIFORM1I glUniform1i = nullptr;
PFNGLUNIFORM3F glUniform3f = nullptr;
PFNGLUNIFORMMATRIX3FV glUniformMatrix3fv = nullptr;

// Milestone 13 additions.
PFNGLDISABLE glDisable = nullptr;
PFNGLBLENDFUNC glBlendFunc = nullptr;

namespace {

template <typename FnPtr>
bool LoadOne(FnPtr& fnPtr, const char* name) {
    fnPtr = reinterpret_cast<FnPtr>(SDL_GL_GetProcAddress(name));
    if (!fnPtr) {
        std::fprintf(stderr, "Failed to load OpenGL function: %s\n", name);
        return false;
    }
    return true;
}

}  // namespace

bool LoadGLFunctions() {
    bool ok = true;
    ok &= LoadOne(glViewport, "glViewport");
    ok &= LoadOne(glClearColor, "glClearColor");
    ok &= LoadOne(glClear, "glClear");
    ok &= LoadOne(glEnable, "glEnable");
    ok &= LoadOne(glDepthFunc, "glDepthFunc");
    ok &= LoadOne(glGenBuffers, "glGenBuffers");
    ok &= LoadOne(glBindBuffer, "glBindBuffer");
    ok &= LoadOne(glBufferData, "glBufferData");
    ok &= LoadOne(glGenVertexArrays, "glGenVertexArrays");
    ok &= LoadOne(glBindVertexArray, "glBindVertexArray");
    ok &= LoadOne(glVertexAttribPointer, "glVertexAttribPointer");
    ok &= LoadOne(glEnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= LoadOne(glCreateShader, "glCreateShader");
    ok &= LoadOne(glShaderSource, "glShaderSource");
    ok &= LoadOne(glCompileShader, "glCompileShader");
    ok &= LoadOne(glGetShaderiv, "glGetShaderiv");
    ok &= LoadOne(glGetShaderInfoLog, "glGetShaderInfoLog");
    ok &= LoadOne(glCreateProgram, "glCreateProgram");
    ok &= LoadOne(glAttachShader, "glAttachShader");
    ok &= LoadOne(glLinkProgram, "glLinkProgram");
    ok &= LoadOne(glGetProgramiv, "glGetProgramiv");
    ok &= LoadOne(glGetProgramInfoLog, "glGetProgramInfoLog");
    ok &= LoadOne(glUseProgram, "glUseProgram");
    ok &= LoadOne(glGetUniformLocation, "glGetUniformLocation");
    ok &= LoadOne(glUniform1f, "glUniform1f");
    ok &= LoadOne(glUniform2f, "glUniform2f");
    ok &= LoadOne(glUniform4f, "glUniform4f");
    ok &= LoadOne(glUniformMatrix4fv, "glUniformMatrix4fv");
    ok &= LoadOne(glDrawArrays, "glDrawArrays");
    ok &= LoadOne(glDeleteShader, "glDeleteShader");
    ok &= LoadOne(glDeleteProgram, "glDeleteProgram");
    ok &= LoadOne(glDeleteBuffers, "glDeleteBuffers");
    ok &= LoadOne(glDeleteVertexArrays, "glDeleteVertexArrays");
    ok &= LoadOne(glReadPixels, "glReadPixels");
    ok &= LoadOne(glDrawElements, "glDrawElements");
    ok &= LoadOne(glGenTextures, "glGenTextures");
    ok &= LoadOne(glBindTexture, "glBindTexture");
    ok &= LoadOne(glTexImage2D, "glTexImage2D");
    ok &= LoadOne(glTexParameteri, "glTexParameteri");
    ok &= LoadOne(glGenerateMipmap, "glGenerateMipmap");
    ok &= LoadOne(glDeleteTextures, "glDeleteTextures");
    ok &= LoadOne(glUniform1i, "glUniform1i");
    ok &= LoadOne(glUniform3f, "glUniform3f");
    ok &= LoadOne(glUniformMatrix3fv, "glUniformMatrix3fv");
    ok &= LoadOne(glDisable, "glDisable");
    ok &= LoadOne(glBlendFunc, "glBlendFunc");
    return ok;
}
