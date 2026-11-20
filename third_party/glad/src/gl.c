/**
 * SPDX-License-Identifier: (WTFPL OR CC0-1.0) AND Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glad/gl.h>

#ifndef GLAD_IMPL_UTIL_C_
#define GLAD_IMPL_UTIL_C_

#ifdef _MSC_VER
#define GLAD_IMPL_UTIL_SSCANF sscanf_s
#else
#define GLAD_IMPL_UTIL_SSCANF sscanf
#endif

#endif /* GLAD_IMPL_UTIL_C_ */

#ifdef __cplusplus
extern "C" {
#endif



#define JUDAS_GL_FLAG(name) int name = 0;
#define JUDAS_GL_PROC(type, name) type name = NULL;
#include "dispatch.inc"
#undef JUDAS_GL_PROC
#undef JUDAS_GL_FLAG

void gladResetGL(void) {
#define JUDAS_GL_FLAG(name) name = 0;
#define JUDAS_GL_PROC(type, name) name = NULL;
#include "dispatch.inc"
#undef JUDAS_GL_PROC
#undef JUDAS_GL_FLAG
}

static void glad_gl_load_GL_VERSION_1_0( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_1_0) return;
    glad_glBlendFunc = (PFNGLBLENDFUNCPROC) load(userptr, "glBlendFunc");
    glad_glClear = (PFNGLCLEARPROC) load(userptr, "glClear");
    glad_glClearColor = (PFNGLCLEARCOLORPROC) load(userptr, "glClearColor");
    glad_glClearDepth = (PFNGLCLEARDEPTHPROC) load(userptr, "glClearDepth");
    glad_glClearStencil = (PFNGLCLEARSTENCILPROC) load(userptr, "glClearStencil");
    glad_glColorMask = (PFNGLCOLORMASKPROC) load(userptr, "glColorMask");
    glad_glCullFace = (PFNGLCULLFACEPROC) load(userptr, "glCullFace");
    glad_glDepthFunc = (PFNGLDEPTHFUNCPROC) load(userptr, "glDepthFunc");
    glad_glDepthMask = (PFNGLDEPTHMASKPROC) load(userptr, "glDepthMask");
    glad_glDepthRange = (PFNGLDEPTHRANGEPROC) load(userptr, "glDepthRange");
    glad_glDisable = (PFNGLDISABLEPROC) load(userptr, "glDisable");
    glad_glDrawBuffer = (PFNGLDRAWBUFFERPROC) load(userptr, "glDrawBuffer");
    glad_glEnable = (PFNGLENABLEPROC) load(userptr, "glEnable");
    glad_glFinish = (PFNGLFINISHPROC) load(userptr, "glFinish");
    glad_glFlush = (PFNGLFLUSHPROC) load(userptr, "glFlush");
    glad_glFrontFace = (PFNGLFRONTFACEPROC) load(userptr, "glFrontFace");
    glad_glGetBooleanv = (PFNGLGETBOOLEANVPROC) load(userptr, "glGetBooleanv");
    glad_glGetDoublev = (PFNGLGETDOUBLEVPROC) load(userptr, "glGetDoublev");
    glad_glGetError = (PFNGLGETERRORPROC) load(userptr, "glGetError");
    glad_glGetFloatv = (PFNGLGETFLOATVPROC) load(userptr, "glGetFloatv");
    glad_glGetIntegerv = (PFNGLGETINTEGERVPROC) load(userptr, "glGetIntegerv");
    glad_glGetString = (PFNGLGETSTRINGPROC) load(userptr, "glGetString");
    glad_glGetTexImage = (PFNGLGETTEXIMAGEPROC) load(userptr, "glGetTexImage");
    glad_glGetTexLevelParameterfv = (PFNGLGETTEXLEVELPARAMETERFVPROC) load(userptr, "glGetTexLevelParameterfv");
    glad_glGetTexLevelParameteriv = (PFNGLGETTEXLEVELPARAMETERIVPROC) load(userptr, "glGetTexLevelParameteriv");
    glad_glGetTexParameterfv = (PFNGLGETTEXPARAMETERFVPROC) load(userptr, "glGetTexParameterfv");
    glad_glGetTexParameteriv = (PFNGLGETTEXPARAMETERIVPROC) load(userptr, "glGetTexParameteriv");
    glad_glHint = (PFNGLHINTPROC) load(userptr, "glHint");
    glad_glIsEnabled = (PFNGLISENABLEDPROC) load(userptr, "glIsEnabled");
    glad_glLineWidth = (PFNGLLINEWIDTHPROC) load(userptr, "glLineWidth");
    glad_glLogicOp = (PFNGLLOGICOPPROC) load(userptr, "glLogicOp");
    glad_glPixelStoref = (PFNGLPIXELSTOREFPROC) load(userptr, "glPixelStoref");
    glad_glPixelStorei = (PFNGLPIXELSTOREIPROC) load(userptr, "glPixelStorei");
    glad_glPointSize = (PFNGLPOINTSIZEPROC) load(userptr, "glPointSize");
    glad_glPolygonMode = (PFNGLPOLYGONMODEPROC) load(userptr, "glPolygonMode");
    glad_glReadBuffer = (PFNGLREADBUFFERPROC) load(userptr, "glReadBuffer");
    glad_glReadPixels = (PFNGLREADPIXELSPROC) load(userptr, "glReadPixels");
    glad_glScissor = (PFNGLSCISSORPROC) load(userptr, "glScissor");
    glad_glStencilFunc = (PFNGLSTENCILFUNCPROC) load(userptr, "glStencilFunc");
    glad_glStencilMask = (PFNGLSTENCILMASKPROC) load(userptr, "glStencilMask");
    glad_glStencilOp = (PFNGLSTENCILOPPROC) load(userptr, "glStencilOp");
    glad_glTexImage1D = (PFNGLTEXIMAGE1DPROC) load(userptr, "glTexImage1D");
    glad_glTexImage2D = (PFNGLTEXIMAGE2DPROC) load(userptr, "glTexImage2D");
    glad_glTexParameterf = (PFNGLTEXPARAMETERFPROC) load(userptr, "glTexParameterf");
    glad_glTexParameterfv = (PFNGLTEXPARAMETERFVPROC) load(userptr, "glTexParameterfv");
    glad_glTexParameteri = (PFNGLTEXPARAMETERIPROC) load(userptr, "glTexParameteri");
    glad_glTexParameteriv = (PFNGLTEXPARAMETERIVPROC) load(userptr, "glTexParameteriv");
    glad_glViewport = (PFNGLVIEWPORTPROC) load(userptr, "glViewport");
}
static void glad_gl_load_GL_VERSION_1_1( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_1_1) return;
    glad_glBindTexture = (PFNGLBINDTEXTUREPROC) load(userptr, "glBindTexture");
    glad_glCopyTexImage1D = (PFNGLCOPYTEXIMAGE1DPROC) load(userptr, "glCopyTexImage1D");
    glad_glCopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC) load(userptr, "glCopyTexImage2D");
    glad_glCopyTexSubImage1D = (PFNGLCOPYTEXSUBIMAGE1DPROC) load(userptr, "glCopyTexSubImage1D");
    glad_glCopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC) load(userptr, "glCopyTexSubImage2D");
    glad_glDeleteTextures = (PFNGLDELETETEXTURESPROC) load(userptr, "glDeleteTextures");
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC) load(userptr, "glDrawArrays");
    glad_glDrawElements = (PFNGLDRAWELEMENTSPROC) load(userptr, "glDrawElements");
    glad_glGenTextures = (PFNGLGENTEXTURESPROC) load(userptr, "glGenTextures");
    glad_glGetPointerv = (PFNGLGETPOINTERVPROC) load(userptr, "glGetPointerv");
    glad_glIsTexture = (PFNGLISTEXTUREPROC) load(userptr, "glIsTexture");
    glad_glPolygonOffset = (PFNGLPOLYGONOFFSETPROC) load(userptr, "glPolygonOffset");
    glad_glTexSubImage1D = (PFNGLTEXSUBIMAGE1DPROC) load(userptr, "glTexSubImage1D");
    glad_glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC) load(userptr, "glTexSubImage2D");
}
static void glad_gl_load_GL_VERSION_1_2( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_1_2) return;
    glad_glCopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC) load(userptr, "glCopyTexSubImage3D");
    glad_glDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC) load(userptr, "glDrawRangeElements");
    glad_glTexImage3D = (PFNGLTEXIMAGE3DPROC) load(userptr, "glTexImage3D");
    glad_glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC) load(userptr, "glTexSubImage3D");
}
static void glad_gl_load_GL_VERSION_1_3( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_1_3) return;
    glad_glActiveTexture = (PFNGLACTIVETEXTUREPROC) load(userptr, "glActiveTexture");
    glad_glCompressedTexImage1D = (PFNGLCOMPRESSEDTEXIMAGE1DPROC) load(userptr, "glCompressedTexImage1D");
    glad_glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC) load(userptr, "glCompressedTexImage2D");
    glad_glCompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC) load(userptr, "glCompressedTexImage3D");
    glad_glCompressedTexSubImage1D = (PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC) load(userptr, "glCompressedTexSubImage1D");
    glad_glCompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC) load(userptr, "glCompressedTexSubImage2D");
    glad_glCompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC) load(userptr, "glCompressedTexSubImage3D");
    glad_glGetCompressedTexImage = (PFNGLGETCOMPRESSEDTEXIMAGEPROC) load(userptr, "glGetCompressedTexImage");
    glad_glSampleCoverage = (PFNGLSAMPLECOVERAGEPROC) load(userptr, "glSampleCoverage");
}
static void glad_gl_load_GL_VERSION_1_4( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_1_4) return;
    glad_glBlendColor = (PFNGLBLENDCOLORPROC) load(userptr, "glBlendColor");
    glad_glBlendEquation = (PFNGLBLENDEQUATIONPROC) load(userptr, "glBlendEquation");
    glad_glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC) load(userptr, "glBlendFuncSeparate");
    glad_glMultiDrawArrays = (PFNGLMULTIDRAWARRAYSPROC) load(userptr, "glMultiDrawArrays");
    glad_glMultiDrawElements = (PFNGLMULTIDRAWELEMENTSPROC) load(userptr, "glMultiDrawElements");
    glad_glPointParameterf = (PFNGLPOINTPARAMETERFPROC) load(userptr, "glPointParameterf");
    glad_glPointParameterfv = (PFNGLPOINTPARAMETERFVPROC) load(userptr, "glPointParameterfv");
    glad_glPointParameteri = (PFNGLPOINTPARAMETERIPROC) load(userptr, "glPointParameteri");
    glad_glPointParameteriv = (PFNGLPOINTPARAMETERIVPROC) load(userptr, "glPointParameteriv");
}
static void glad_gl_load_GL_VERSION_1_5( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_1_5) return;
    glad_glBeginQuery = (PFNGLBEGINQUERYPROC) load(userptr, "glBeginQuery");
    glad_glBindBuffer = (PFNGLBINDBUFFERPROC) load(userptr, "glBindBuffer");
    glad_glBufferData = (PFNGLBUFFERDATAPROC) load(userptr, "glBufferData");
    glad_glBufferSubData = (PFNGLBUFFERSUBDATAPROC) load(userptr, "glBufferSubData");
    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC) load(userptr, "glDeleteBuffers");
    glad_glDeleteQueries = (PFNGLDELETEQUERIESPROC) load(userptr, "glDeleteQueries");
    glad_glEndQuery = (PFNGLENDQUERYPROC) load(userptr, "glEndQuery");
    glad_glGenBuffers = (PFNGLGENBUFFERSPROC) load(userptr, "glGenBuffers");
    glad_glGenQueries = (PFNGLGENQUERIESPROC) load(userptr, "glGenQueries");
    glad_glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC) load(userptr, "glGetBufferParameteriv");
    glad_glGetBufferPointerv = (PFNGLGETBUFFERPOINTERVPROC) load(userptr, "glGetBufferPointerv");
    glad_glGetBufferSubData = (PFNGLGETBUFFERSUBDATAPROC) load(userptr, "glGetBufferSubData");
    glad_glGetQueryObjectiv = (PFNGLGETQUERYOBJECTIVPROC) load(userptr, "glGetQueryObjectiv");
    glad_glGetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVPROC) load(userptr, "glGetQueryObjectuiv");
    glad_glGetQueryiv = (PFNGLGETQUERYIVPROC) load(userptr, "glGetQueryiv");
    glad_glIsBuffer = (PFNGLISBUFFERPROC) load(userptr, "glIsBuffer");
    glad_glIsQuery = (PFNGLISQUERYPROC) load(userptr, "glIsQuery");
    glad_glMapBuffer = (PFNGLMAPBUFFERPROC) load(userptr, "glMapBuffer");
    glad_glUnmapBuffer = (PFNGLUNMAPBUFFERPROC) load(userptr, "glUnmapBuffer");
}
static void glad_gl_load_GL_VERSION_2_0( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_2_0) return;
    glad_glAttachShader = (PFNGLATTACHSHADERPROC) load(userptr, "glAttachShader");
    glad_glBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC) load(userptr, "glBindAttribLocation");
    glad_glBlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC) load(userptr, "glBlendEquationSeparate");
    glad_glCompileShader = (PFNGLCOMPILESHADERPROC) load(userptr, "glCompileShader");
    glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC) load(userptr, "glCreateProgram");
    glad_glCreateShader = (PFNGLCREATESHADERPROC) load(userptr, "glCreateShader");
    glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC) load(userptr, "glDeleteProgram");
    glad_glDeleteShader = (PFNGLDELETESHADERPROC) load(userptr, "glDeleteShader");
    glad_glDetachShader = (PFNGLDETACHSHADERPROC) load(userptr, "glDetachShader");
    glad_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC) load(userptr, "glDisableVertexAttribArray");
    glad_glDrawBuffers = (PFNGLDRAWBUFFERSPROC) load(userptr, "glDrawBuffers");
    glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC) load(userptr, "glEnableVertexAttribArray");
    glad_glGetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC) load(userptr, "glGetActiveAttrib");
    glad_glGetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC) load(userptr, "glGetActiveUniform");
    glad_glGetAttachedShaders = (PFNGLGETATTACHEDSHADERSPROC) load(userptr, "glGetAttachedShaders");
    glad_glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC) load(userptr, "glGetAttribLocation");
    glad_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC) load(userptr, "glGetProgramInfoLog");
    glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC) load(userptr, "glGetProgramiv");
    glad_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) load(userptr, "glGetShaderInfoLog");
    glad_glGetShaderSource = (PFNGLGETSHADERSOURCEPROC) load(userptr, "glGetShaderSource");
    glad_glGetShaderiv = (PFNGLGETSHADERIVPROC) load(userptr, "glGetShaderiv");
    glad_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC) load(userptr, "glGetUniformLocation");
    glad_glGetUniformfv = (PFNGLGETUNIFORMFVPROC) load(userptr, "glGetUniformfv");
    glad_glGetUniformiv = (PFNGLGETUNIFORMIVPROC) load(userptr, "glGetUniformiv");
    glad_glGetVertexAttribPointerv = (PFNGLGETVERTEXATTRIBPOINTERVPROC) load(userptr, "glGetVertexAttribPointerv");
    glad_glGetVertexAttribdv = (PFNGLGETVERTEXATTRIBDVPROC) load(userptr, "glGetVertexAttribdv");
    glad_glGetVertexAttribfv = (PFNGLGETVERTEXATTRIBFVPROC) load(userptr, "glGetVertexAttribfv");
    glad_glGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC) load(userptr, "glGetVertexAttribiv");
    glad_glIsProgram = (PFNGLISPROGRAMPROC) load(userptr, "glIsProgram");
    glad_glIsShader = (PFNGLISSHADERPROC) load(userptr, "glIsShader");
    glad_glLinkProgram = (PFNGLLINKPROGRAMPROC) load(userptr, "glLinkProgram");
    glad_glShaderSource = (PFNGLSHADERSOURCEPROC) load(userptr, "glShaderSource");
    glad_glStencilFuncSeparate = (PFNGLSTENCILFUNCSEPARATEPROC) load(userptr, "glStencilFuncSeparate");
    glad_glStencilMaskSeparate = (PFNGLSTENCILMASKSEPARATEPROC) load(userptr, "glStencilMaskSeparate");
    glad_glStencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC) load(userptr, "glStencilOpSeparate");
    glad_glUniform1f = (PFNGLUNIFORM1FPROC) load(userptr, "glUniform1f");
    glad_glUniform1fv = (PFNGLUNIFORM1FVPROC) load(userptr, "glUniform1fv");
    glad_glUniform1i = (PFNGLUNIFORM1IPROC) load(userptr, "glUniform1i");
    glad_glUniform1iv = (PFNGLUNIFORM1IVPROC) load(userptr, "glUniform1iv");
    glad_glUniform2f = (PFNGLUNIFORM2FPROC) load(userptr, "glUniform2f");
    glad_glUniform2fv = (PFNGLUNIFORM2FVPROC) load(userptr, "glUniform2fv");
    glad_glUniform2i = (PFNGLUNIFORM2IPROC) load(userptr, "glUniform2i");
    glad_glUniform2iv = (PFNGLUNIFORM2IVPROC) load(userptr, "glUniform2iv");
    glad_glUniform3f = (PFNGLUNIFORM3FPROC) load(userptr, "glUniform3f");
    glad_glUniform3fv = (PFNGLUNIFORM3FVPROC) load(userptr, "glUniform3fv");
    glad_glUniform3i = (PFNGLUNIFORM3IPROC) load(userptr, "glUniform3i");
    glad_glUniform3iv = (PFNGLUNIFORM3IVPROC) load(userptr, "glUniform3iv");
    glad_glUniform4f = (PFNGLUNIFORM4FPROC) load(userptr, "glUniform4f");
    glad_glUniform4fv = (PFNGLUNIFORM4FVPROC) load(userptr, "glUniform4fv");
    glad_glUniform4i = (PFNGLUNIFORM4IPROC) load(userptr, "glUniform4i");
    glad_glUniform4iv = (PFNGLUNIFORM4IVPROC) load(userptr, "glUniform4iv");
    glad_glUniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC) load(userptr, "glUniformMatrix2fv");
    glad_glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC) load(userptr, "glUniformMatrix3fv");
    glad_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC) load(userptr, "glUniformMatrix4fv");
    glad_glUseProgram = (PFNGLUSEPROGRAMPROC) load(userptr, "glUseProgram");
    glad_glValidateProgram = (PFNGLVALIDATEPROGRAMPROC) load(userptr, "glValidateProgram");
    glad_glVertexAttrib1d = (PFNGLVERTEXATTRIB1DPROC) load(userptr, "glVertexAttrib1d");
    glad_glVertexAttrib1dv = (PFNGLVERTEXATTRIB1DVPROC) load(userptr, "glVertexAttrib1dv");
    glad_glVertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC) load(userptr, "glVertexAttrib1f");
    glad_glVertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC) load(userptr, "glVertexAttrib1fv");
    glad_glVertexAttrib1s = (PFNGLVERTEXATTRIB1SPROC) load(userptr, "glVertexAttrib1s");
    glad_glVertexAttrib1sv = (PFNGLVERTEXATTRIB1SVPROC) load(userptr, "glVertexAttrib1sv");
    glad_glVertexAttrib2d = (PFNGLVERTEXATTRIB2DPROC) load(userptr, "glVertexAttrib2d");
    glad_glVertexAttrib2dv = (PFNGLVERTEXATTRIB2DVPROC) load(userptr, "glVertexAttrib2dv");
    glad_glVertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC) load(userptr, "glVertexAttrib2f");
    glad_glVertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC) load(userptr, "glVertexAttrib2fv");
    glad_glVertexAttrib2s = (PFNGLVERTEXATTRIB2SPROC) load(userptr, "glVertexAttrib2s");
    glad_glVertexAttrib2sv = (PFNGLVERTEXATTRIB2SVPROC) load(userptr, "glVertexAttrib2sv");
    glad_glVertexAttrib3d = (PFNGLVERTEXATTRIB3DPROC) load(userptr, "glVertexAttrib3d");
    glad_glVertexAttrib3dv = (PFNGLVERTEXATTRIB3DVPROC) load(userptr, "glVertexAttrib3dv");
    glad_glVertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC) load(userptr, "glVertexAttrib3f");
    glad_glVertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC) load(userptr, "glVertexAttrib3fv");
    glad_glVertexAttrib3s = (PFNGLVERTEXATTRIB3SPROC) load(userptr, "glVertexAttrib3s");
    glad_glVertexAttrib3sv = (PFNGLVERTEXATTRIB3SVPROC) load(userptr, "glVertexAttrib3sv");
    glad_glVertexAttrib4Nbv = (PFNGLVERTEXATTRIB4NBVPROC) load(userptr, "glVertexAttrib4Nbv");
    glad_glVertexAttrib4Niv = (PFNGLVERTEXATTRIB4NIVPROC) load(userptr, "glVertexAttrib4Niv");
    glad_glVertexAttrib4Nsv = (PFNGLVERTEXATTRIB4NSVPROC) load(userptr, "glVertexAttrib4Nsv");
    glad_glVertexAttrib4Nub = (PFNGLVERTEXATTRIB4NUBPROC) load(userptr, "glVertexAttrib4Nub");
    glad_glVertexAttrib4Nubv = (PFNGLVERTEXATTRIB4NUBVPROC) load(userptr, "glVertexAttrib4Nubv");
    glad_glVertexAttrib4Nuiv = (PFNGLVERTEXATTRIB4NUIVPROC) load(userptr, "glVertexAttrib4Nuiv");
    glad_glVertexAttrib4Nusv = (PFNGLVERTEXATTRIB4NUSVPROC) load(userptr, "glVertexAttrib4Nusv");
    glad_glVertexAttrib4bv = (PFNGLVERTEXATTRIB4BVPROC) load(userptr, "glVertexAttrib4bv");
    glad_glVertexAttrib4d = (PFNGLVERTEXATTRIB4DPROC) load(userptr, "glVertexAttrib4d");
    glad_glVertexAttrib4dv = (PFNGLVERTEXATTRIB4DVPROC) load(userptr, "glVertexAttrib4dv");
    glad_glVertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC) load(userptr, "glVertexAttrib4f");
    glad_glVertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC) load(userptr, "glVertexAttrib4fv");
    glad_glVertexAttrib4iv = (PFNGLVERTEXATTRIB4IVPROC) load(userptr, "glVertexAttrib4iv");
    glad_glVertexAttrib4s = (PFNGLVERTEXATTRIB4SPROC) load(userptr, "glVertexAttrib4s");
    glad_glVertexAttrib4sv = (PFNGLVERTEXATTRIB4SVPROC) load(userptr, "glVertexAttrib4sv");
    glad_glVertexAttrib4ubv = (PFNGLVERTEXATTRIB4UBVPROC) load(userptr, "glVertexAttrib4ubv");
    glad_glVertexAttrib4uiv = (PFNGLVERTEXATTRIB4UIVPROC) load(userptr, "glVertexAttrib4uiv");
    glad_glVertexAttrib4usv = (PFNGLVERTEXATTRIB4USVPROC) load(userptr, "glVertexAttrib4usv");
    glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC) load(userptr, "glVertexAttribPointer");
}
static void glad_gl_load_GL_VERSION_2_1( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_2_1) return;
    glad_glUniformMatrix2x3fv = (PFNGLUNIFORMMATRIX2X3FVPROC) load(userptr, "glUniformMatrix2x3fv");
    glad_glUniformMatrix2x4fv = (PFNGLUNIFORMMATRIX2X4FVPROC) load(userptr, "glUniformMatrix2x4fv");
    glad_glUniformMatrix3x2fv = (PFNGLUNIFORMMATRIX3X2FVPROC) load(userptr, "glUniformMatrix3x2fv");
    glad_glUniformMatrix3x4fv = (PFNGLUNIFORMMATRIX3X4FVPROC) load(userptr, "glUniformMatrix3x4fv");
    glad_glUniformMatrix4x2fv = (PFNGLUNIFORMMATRIX4X2FVPROC) load(userptr, "glUniformMatrix4x2fv");
    glad_glUniformMatrix4x3fv = (PFNGLUNIFORMMATRIX4X3FVPROC) load(userptr, "glUniformMatrix4x3fv");
}
static void glad_gl_load_GL_VERSION_3_0( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_3_0) return;
    glad_glBeginConditionalRender = (PFNGLBEGINCONDITIONALRENDERPROC) load(userptr, "glBeginConditionalRender");
    glad_glBeginTransformFeedback = (PFNGLBEGINTRANSFORMFEEDBACKPROC) load(userptr, "glBeginTransformFeedback");
    glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC) load(userptr, "glBindBufferBase");
    glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC) load(userptr, "glBindBufferRange");
    glad_glBindFragDataLocation = (PFNGLBINDFRAGDATALOCATIONPROC) load(userptr, "glBindFragDataLocation");
    glad_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) load(userptr, "glBindFramebuffer");
    glad_glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) load(userptr, "glBindRenderbuffer");
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) load(userptr, "glBindVertexArray");
    glad_glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC) load(userptr, "glBlitFramebuffer");
    glad_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) load(userptr, "glCheckFramebufferStatus");
    glad_glClampColor = (PFNGLCLAMPCOLORPROC) load(userptr, "glClampColor");
    glad_glClearBufferfi = (PFNGLCLEARBUFFERFIPROC) load(userptr, "glClearBufferfi");
    glad_glClearBufferfv = (PFNGLCLEARBUFFERFVPROC) load(userptr, "glClearBufferfv");
    glad_glClearBufferiv = (PFNGLCLEARBUFFERIVPROC) load(userptr, "glClearBufferiv");
    glad_glClearBufferuiv = (PFNGLCLEARBUFFERUIVPROC) load(userptr, "glClearBufferuiv");
    glad_glColorMaski = (PFNGLCOLORMASKIPROC) load(userptr, "glColorMaski");
    glad_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) load(userptr, "glDeleteFramebuffers");
    glad_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) load(userptr, "glDeleteRenderbuffers");
    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) load(userptr, "glDeleteVertexArrays");
    glad_glDisablei = (PFNGLDISABLEIPROC) load(userptr, "glDisablei");
    glad_glEnablei = (PFNGLENABLEIPROC) load(userptr, "glEnablei");
    glad_glEndConditionalRender = (PFNGLENDCONDITIONALRENDERPROC) load(userptr, "glEndConditionalRender");
    glad_glEndTransformFeedback = (PFNGLENDTRANSFORMFEEDBACKPROC) load(userptr, "glEndTransformFeedback");
    glad_glFlushMappedBufferRange = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC) load(userptr, "glFlushMappedBufferRange");
    glad_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) load(userptr, "glFramebufferRenderbuffer");
    glad_glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DPROC) load(userptr, "glFramebufferTexture1D");
    glad_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) load(userptr, "glFramebufferTexture2D");
    glad_glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DPROC) load(userptr, "glFramebufferTexture3D");
    glad_glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC) load(userptr, "glFramebufferTextureLayer");
    glad_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) load(userptr, "glGenFramebuffers");
    glad_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) load(userptr, "glGenRenderbuffers");
    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) load(userptr, "glGenVertexArrays");
    glad_glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC) load(userptr, "glGenerateMipmap");
    glad_glGetBooleani_v = (PFNGLGETBOOLEANI_VPROC) load(userptr, "glGetBooleani_v");
    glad_glGetFragDataLocation = (PFNGLGETFRAGDATALOCATIONPROC) load(userptr, "glGetFragDataLocation");
    glad_glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) load(userptr, "glGetFramebufferAttachmentParameteriv");
    glad_glGetIntegeri_v = (PFNGLGETINTEGERI_VPROC) load(userptr, "glGetIntegeri_v");
    glad_glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC) load(userptr, "glGetRenderbufferParameteriv");
    glad_glGetStringi = (PFNGLGETSTRINGIPROC) load(userptr, "glGetStringi");
    glad_glGetTexParameterIiv = (PFNGLGETTEXPARAMETERIIVPROC) load(userptr, "glGetTexParameterIiv");
    glad_glGetTexParameterIuiv = (PFNGLGETTEXPARAMETERIUIVPROC) load(userptr, "glGetTexParameterIuiv");
    glad_glGetTransformFeedbackVarying = (PFNGLGETTRANSFORMFEEDBACKVARYINGPROC) load(userptr, "glGetTransformFeedbackVarying");
    glad_glGetUniformuiv = (PFNGLGETUNIFORMUIVPROC) load(userptr, "glGetUniformuiv");
    glad_glGetVertexAttribIiv = (PFNGLGETVERTEXATTRIBIIVPROC) load(userptr, "glGetVertexAttribIiv");
    glad_glGetVertexAttribIuiv = (PFNGLGETVERTEXATTRIBIUIVPROC) load(userptr, "glGetVertexAttribIuiv");
    glad_glIsEnabledi = (PFNGLISENABLEDIPROC) load(userptr, "glIsEnabledi");
    glad_glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC) load(userptr, "glIsFramebuffer");
    glad_glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC) load(userptr, "glIsRenderbuffer");
    glad_glIsVertexArray = (PFNGLISVERTEXARRAYPROC) load(userptr, "glIsVertexArray");
    glad_glMapBufferRange = (PFNGLMAPBUFFERRANGEPROC) load(userptr, "glMapBufferRange");
    glad_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) load(userptr, "glRenderbufferStorage");
    glad_glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) load(userptr, "glRenderbufferStorageMultisample");
    glad_glTexParameterIiv = (PFNGLTEXPARAMETERIIVPROC) load(userptr, "glTexParameterIiv");
    glad_glTexParameterIuiv = (PFNGLTEXPARAMETERIUIVPROC) load(userptr, "glTexParameterIuiv");
    glad_glTransformFeedbackVaryings = (PFNGLTRANSFORMFEEDBACKVARYINGSPROC) load(userptr, "glTransformFeedbackVaryings");
    glad_glUniform1ui = (PFNGLUNIFORM1UIPROC) load(userptr, "glUniform1ui");
    glad_glUniform1uiv = (PFNGLUNIFORM1UIVPROC) load(userptr, "glUniform1uiv");
    glad_glUniform2ui = (PFNGLUNIFORM2UIPROC) load(userptr, "glUniform2ui");
    glad_glUniform2uiv = (PFNGLUNIFORM2UIVPROC) load(userptr, "glUniform2uiv");
    glad_glUniform3ui = (PFNGLUNIFORM3UIPROC) load(userptr, "glUniform3ui");
    glad_glUniform3uiv = (PFNGLUNIFORM3UIVPROC) load(userptr, "glUniform3uiv");
    glad_glUniform4ui = (PFNGLUNIFORM4UIPROC) load(userptr, "glUniform4ui");
    glad_glUniform4uiv = (PFNGLUNIFORM4UIVPROC) load(userptr, "glUniform4uiv");
    glad_glVertexAttribI1i = (PFNGLVERTEXATTRIBI1IPROC) load(userptr, "glVertexAttribI1i");
    glad_glVertexAttribI1iv = (PFNGLVERTEXATTRIBI1IVPROC) load(userptr, "glVertexAttribI1iv");
    glad_glVertexAttribI1ui = (PFNGLVERTEXATTRIBI1UIPROC) load(userptr, "glVertexAttribI1ui");
    glad_glVertexAttribI1uiv = (PFNGLVERTEXATTRIBI1UIVPROC) load(userptr, "glVertexAttribI1uiv");
    glad_glVertexAttribI2i = (PFNGLVERTEXATTRIBI2IPROC) load(userptr, "glVertexAttribI2i");
    glad_glVertexAttribI2iv = (PFNGLVERTEXATTRIBI2IVPROC) load(userptr, "glVertexAttribI2iv");
    glad_glVertexAttribI2ui = (PFNGLVERTEXATTRIBI2UIPROC) load(userptr, "glVertexAttribI2ui");
    glad_glVertexAttribI2uiv = (PFNGLVERTEXATTRIBI2UIVPROC) load(userptr, "glVertexAttribI2uiv");
    glad_glVertexAttribI3i = (PFNGLVERTEXATTRIBI3IPROC) load(userptr, "glVertexAttribI3i");
    glad_glVertexAttribI3iv = (PFNGLVERTEXATTRIBI3IVPROC) load(userptr, "glVertexAttribI3iv");
    glad_glVertexAttribI3ui = (PFNGLVERTEXATTRIBI3UIPROC) load(userptr, "glVertexAttribI3ui");
    glad_glVertexAttribI3uiv = (PFNGLVERTEXATTRIBI3UIVPROC) load(userptr, "glVertexAttribI3uiv");
    glad_glVertexAttribI4bv = (PFNGLVERTEXATTRIBI4BVPROC) load(userptr, "glVertexAttribI4bv");
    glad_glVertexAttribI4i = (PFNGLVERTEXATTRIBI4IPROC) load(userptr, "glVertexAttribI4i");
    glad_glVertexAttribI4iv = (PFNGLVERTEXATTRIBI4IVPROC) load(userptr, "glVertexAttribI4iv");
    glad_glVertexAttribI4sv = (PFNGLVERTEXATTRIBI4SVPROC) load(userptr, "glVertexAttribI4sv");
    glad_glVertexAttribI4ubv = (PFNGLVERTEXATTRIBI4UBVPROC) load(userptr, "glVertexAttribI4ubv");
    glad_glVertexAttribI4ui = (PFNGLVERTEXATTRIBI4UIPROC) load(userptr, "glVertexAttribI4ui");
    glad_glVertexAttribI4uiv = (PFNGLVERTEXATTRIBI4UIVPROC) load(userptr, "glVertexAttribI4uiv");
    glad_glVertexAttribI4usv = (PFNGLVERTEXATTRIBI4USVPROC) load(userptr, "glVertexAttribI4usv");
    glad_glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC) load(userptr, "glVertexAttribIPointer");
}
static void glad_gl_load_GL_VERSION_3_1( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_3_1) return;
    glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC) load(userptr, "glBindBufferBase");
    glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC) load(userptr, "glBindBufferRange");
    glad_glCopyBufferSubData = (PFNGLCOPYBUFFERSUBDATAPROC) load(userptr, "glCopyBufferSubData");
    glad_glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC) load(userptr, "glDrawArraysInstanced");
    glad_glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC) load(userptr, "glDrawElementsInstanced");
    glad_glGetActiveUniformBlockName = (PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC) load(userptr, "glGetActiveUniformBlockName");
    glad_glGetActiveUniformBlockiv = (PFNGLGETACTIVEUNIFORMBLOCKIVPROC) load(userptr, "glGetActiveUniformBlockiv");
    glad_glGetActiveUniformName = (PFNGLGETACTIVEUNIFORMNAMEPROC) load(userptr, "glGetActiveUniformName");
    glad_glGetActiveUniformsiv = (PFNGLGETACTIVEUNIFORMSIVPROC) load(userptr, "glGetActiveUniformsiv");
    glad_glGetIntegeri_v = (PFNGLGETINTEGERI_VPROC) load(userptr, "glGetIntegeri_v");
    glad_glGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC) load(userptr, "glGetUniformBlockIndex");
    glad_glGetUniformIndices = (PFNGLGETUNIFORMINDICESPROC) load(userptr, "glGetUniformIndices");
    glad_glPrimitiveRestartIndex = (PFNGLPRIMITIVERESTARTINDEXPROC) load(userptr, "glPrimitiveRestartIndex");
    glad_glTexBuffer = (PFNGLTEXBUFFERPROC) load(userptr, "glTexBuffer");
    glad_glUniformBlockBinding = (PFNGLUNIFORMBLOCKBINDINGPROC) load(userptr, "glUniformBlockBinding");
}
static void glad_gl_load_GL_VERSION_3_2( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_3_2) return;
    glad_glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC) load(userptr, "glClientWaitSync");
    glad_glDeleteSync = (PFNGLDELETESYNCPROC) load(userptr, "glDeleteSync");
    glad_glDrawElementsBaseVertex = (PFNGLDRAWELEMENTSBASEVERTEXPROC) load(userptr, "glDrawElementsBaseVertex");
    glad_glDrawElementsInstancedBaseVertex = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC) load(userptr, "glDrawElementsInstancedBaseVertex");
    glad_glDrawRangeElementsBaseVertex = (PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC) load(userptr, "glDrawRangeElementsBaseVertex");
    glad_glFenceSync = (PFNGLFENCESYNCPROC) load(userptr, "glFenceSync");
    glad_glFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC) load(userptr, "glFramebufferTexture");
    glad_glGetBufferParameteri64v = (PFNGLGETBUFFERPARAMETERI64VPROC) load(userptr, "glGetBufferParameteri64v");
    glad_glGetInteger64i_v = (PFNGLGETINTEGER64I_VPROC) load(userptr, "glGetInteger64i_v");
    glad_glGetInteger64v = (PFNGLGETINTEGER64VPROC) load(userptr, "glGetInteger64v");
    glad_glGetMultisamplefv = (PFNGLGETMULTISAMPLEFVPROC) load(userptr, "glGetMultisamplefv");
    glad_glGetSynciv = (PFNGLGETSYNCIVPROC) load(userptr, "glGetSynciv");
    glad_glIsSync = (PFNGLISSYNCPROC) load(userptr, "glIsSync");
    glad_glMultiDrawElementsBaseVertex = (PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC) load(userptr, "glMultiDrawElementsBaseVertex");
    glad_glProvokingVertex = (PFNGLPROVOKINGVERTEXPROC) load(userptr, "glProvokingVertex");
    glad_glSampleMaski = (PFNGLSAMPLEMASKIPROC) load(userptr, "glSampleMaski");
    glad_glTexImage2DMultisample = (PFNGLTEXIMAGE2DMULTISAMPLEPROC) load(userptr, "glTexImage2DMultisample");
    glad_glTexImage3DMultisample = (PFNGLTEXIMAGE3DMULTISAMPLEPROC) load(userptr, "glTexImage3DMultisample");
    glad_glWaitSync = (PFNGLWAITSYNCPROC) load(userptr, "glWaitSync");
}
static void glad_gl_load_GL_VERSION_3_3( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_3_3) return;
    glad_glBindFragDataLocationIndexed = (PFNGLBINDFRAGDATALOCATIONINDEXEDPROC) load(userptr, "glBindFragDataLocationIndexed");
    glad_glBindSampler = (PFNGLBINDSAMPLERPROC) load(userptr, "glBindSampler");
    glad_glDeleteSamplers = (PFNGLDELETESAMPLERSPROC) load(userptr, "glDeleteSamplers");
    glad_glGenSamplers = (PFNGLGENSAMPLERSPROC) load(userptr, "glGenSamplers");
    glad_glGetFragDataIndex = (PFNGLGETFRAGDATAINDEXPROC) load(userptr, "glGetFragDataIndex");
    glad_glGetQueryObjecti64v = (PFNGLGETQUERYOBJECTI64VPROC) load(userptr, "glGetQueryObjecti64v");
    glad_glGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VPROC) load(userptr, "glGetQueryObjectui64v");
    glad_glGetSamplerParameterIiv = (PFNGLGETSAMPLERPARAMETERIIVPROC) load(userptr, "glGetSamplerParameterIiv");
    glad_glGetSamplerParameterIuiv = (PFNGLGETSAMPLERPARAMETERIUIVPROC) load(userptr, "glGetSamplerParameterIuiv");
    glad_glGetSamplerParameterfv = (PFNGLGETSAMPLERPARAMETERFVPROC) load(userptr, "glGetSamplerParameterfv");
    glad_glGetSamplerParameteriv = (PFNGLGETSAMPLERPARAMETERIVPROC) load(userptr, "glGetSamplerParameteriv");
    glad_glIsSampler = (PFNGLISSAMPLERPROC) load(userptr, "glIsSampler");
    glad_glQueryCounter = (PFNGLQUERYCOUNTERPROC) load(userptr, "glQueryCounter");
    glad_glSamplerParameterIiv = (PFNGLSAMPLERPARAMETERIIVPROC) load(userptr, "glSamplerParameterIiv");
    glad_glSamplerParameterIuiv = (PFNGLSAMPLERPARAMETERIUIVPROC) load(userptr, "glSamplerParameterIuiv");
    glad_glSamplerParameterf = (PFNGLSAMPLERPARAMETERFPROC) load(userptr, "glSamplerParameterf");
    glad_glSamplerParameterfv = (PFNGLSAMPLERPARAMETERFVPROC) load(userptr, "glSamplerParameterfv");
    glad_glSamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC) load(userptr, "glSamplerParameteri");
    glad_glSamplerParameteriv = (PFNGLSAMPLERPARAMETERIVPROC) load(userptr, "glSamplerParameteriv");
    glad_glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC) load(userptr, "glVertexAttribDivisor");
    glad_glVertexAttribP1ui = (PFNGLVERTEXATTRIBP1UIPROC) load(userptr, "glVertexAttribP1ui");
    glad_glVertexAttribP1uiv = (PFNGLVERTEXATTRIBP1UIVPROC) load(userptr, "glVertexAttribP1uiv");
    glad_glVertexAttribP2ui = (PFNGLVERTEXATTRIBP2UIPROC) load(userptr, "glVertexAttribP2ui");
    glad_glVertexAttribP2uiv = (PFNGLVERTEXATTRIBP2UIVPROC) load(userptr, "glVertexAttribP2uiv");
    glad_glVertexAttribP3ui = (PFNGLVERTEXATTRIBP3UIPROC) load(userptr, "glVertexAttribP3ui");
    glad_glVertexAttribP3uiv = (PFNGLVERTEXATTRIBP3UIVPROC) load(userptr, "glVertexAttribP3uiv");
    glad_glVertexAttribP4ui = (PFNGLVERTEXATTRIBP4UIPROC) load(userptr, "glVertexAttribP4ui");
    glad_glVertexAttribP4uiv = (PFNGLVERTEXATTRIBP4UIVPROC) load(userptr, "glVertexAttribP4uiv");
}
static void glad_gl_load_GL_VERSION_4_0( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_4_0) return;
    glad_glBeginQueryIndexed = (PFNGLBEGINQUERYINDEXEDPROC) load(userptr, "glBeginQueryIndexed");
    glad_glBindTransformFeedback = (PFNGLBINDTRANSFORMFEEDBACKPROC) load(userptr, "glBindTransformFeedback");
    glad_glBlendEquationSeparatei = (PFNGLBLENDEQUATIONSEPARATEIPROC) load(userptr, "glBlendEquationSeparatei");
    glad_glBlendEquationi = (PFNGLBLENDEQUATIONIPROC) load(userptr, "glBlendEquationi");
    glad_glBlendFuncSeparatei = (PFNGLBLENDFUNCSEPARATEIPROC) load(userptr, "glBlendFuncSeparatei");
    glad_glBlendFunci = (PFNGLBLENDFUNCIPROC) load(userptr, "glBlendFunci");
    glad_glDeleteTransformFeedbacks = (PFNGLDELETETRANSFORMFEEDBACKSPROC) load(userptr, "glDeleteTransformFeedbacks");
    glad_glDrawArraysIndirect = (PFNGLDRAWARRAYSINDIRECTPROC) load(userptr, "glDrawArraysIndirect");
    glad_glDrawElementsIndirect = (PFNGLDRAWELEMENTSINDIRECTPROC) load(userptr, "glDrawElementsIndirect");
    glad_glDrawTransformFeedback = (PFNGLDRAWTRANSFORMFEEDBACKPROC) load(userptr, "glDrawTransformFeedback");
    glad_glDrawTransformFeedbackStream = (PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC) load(userptr, "glDrawTransformFeedbackStream");
    glad_glEndQueryIndexed = (PFNGLENDQUERYINDEXEDPROC) load(userptr, "glEndQueryIndexed");
    glad_glGenTransformFeedbacks = (PFNGLGENTRANSFORMFEEDBACKSPROC) load(userptr, "glGenTransformFeedbacks");
    glad_glGetActiveSubroutineName = (PFNGLGETACTIVESUBROUTINENAMEPROC) load(userptr, "glGetActiveSubroutineName");
    glad_glGetActiveSubroutineUniformName = (PFNGLGETACTIVESUBROUTINEUNIFORMNAMEPROC) load(userptr, "glGetActiveSubroutineUniformName");
    glad_glGetActiveSubroutineUniformiv = (PFNGLGETACTIVESUBROUTINEUNIFORMIVPROC) load(userptr, "glGetActiveSubroutineUniformiv");
    glad_glGetProgramStageiv = (PFNGLGETPROGRAMSTAGEIVPROC) load(userptr, "glGetProgramStageiv");
    glad_glGetQueryIndexediv = (PFNGLGETQUERYINDEXEDIVPROC) load(userptr, "glGetQueryIndexediv");
    glad_glGetSubroutineIndex = (PFNGLGETSUBROUTINEINDEXPROC) load(userptr, "glGetSubroutineIndex");
    glad_glGetSubroutineUniformLocation = (PFNGLGETSUBROUTINEUNIFORMLOCATIONPROC) load(userptr, "glGetSubroutineUniformLocation");
    glad_glGetUniformSubroutineuiv = (PFNGLGETUNIFORMSUBROUTINEUIVPROC) load(userptr, "glGetUniformSubroutineuiv");
    glad_glGetUniformdv = (PFNGLGETUNIFORMDVPROC) load(userptr, "glGetUniformdv");
    glad_glIsTransformFeedback = (PFNGLISTRANSFORMFEEDBACKPROC) load(userptr, "glIsTransformFeedback");
    glad_glMinSampleShading = (PFNGLMINSAMPLESHADINGPROC) load(userptr, "glMinSampleShading");
    glad_glPatchParameterfv = (PFNGLPATCHPARAMETERFVPROC) load(userptr, "glPatchParameterfv");
    glad_glPatchParameteri = (PFNGLPATCHPARAMETERIPROC) load(userptr, "glPatchParameteri");
    glad_glPauseTransformFeedback = (PFNGLPAUSETRANSFORMFEEDBACKPROC) load(userptr, "glPauseTransformFeedback");
    glad_glResumeTransformFeedback = (PFNGLRESUMETRANSFORMFEEDBACKPROC) load(userptr, "glResumeTransformFeedback");
    glad_glUniform1d = (PFNGLUNIFORM1DPROC) load(userptr, "glUniform1d");
    glad_glUniform1dv = (PFNGLUNIFORM1DVPROC) load(userptr, "glUniform1dv");
    glad_glUniform2d = (PFNGLUNIFORM2DPROC) load(userptr, "glUniform2d");
    glad_glUniform2dv = (PFNGLUNIFORM2DVPROC) load(userptr, "glUniform2dv");
    glad_glUniform3d = (PFNGLUNIFORM3DPROC) load(userptr, "glUniform3d");
    glad_glUniform3dv = (PFNGLUNIFORM3DVPROC) load(userptr, "glUniform3dv");
    glad_glUniform4d = (PFNGLUNIFORM4DPROC) load(userptr, "glUniform4d");
    glad_glUniform4dv = (PFNGLUNIFORM4DVPROC) load(userptr, "glUniform4dv");
    glad_glUniformMatrix2dv = (PFNGLUNIFORMMATRIX2DVPROC) load(userptr, "glUniformMatrix2dv");
    glad_glUniformMatrix2x3dv = (PFNGLUNIFORMMATRIX2X3DVPROC) load(userptr, "glUniformMatrix2x3dv");
    glad_glUniformMatrix2x4dv = (PFNGLUNIFORMMATRIX2X4DVPROC) load(userptr, "glUniformMatrix2x4dv");
    glad_glUniformMatrix3dv = (PFNGLUNIFORMMATRIX3DVPROC) load(userptr, "glUniformMatrix3dv");
    glad_glUniformMatrix3x2dv = (PFNGLUNIFORMMATRIX3X2DVPROC) load(userptr, "glUniformMatrix3x2dv");
    glad_glUniformMatrix3x4dv = (PFNGLUNIFORMMATRIX3X4DVPROC) load(userptr, "glUniformMatrix3x4dv");
    glad_glUniformMatrix4dv = (PFNGLUNIFORMMATRIX4DVPROC) load(userptr, "glUniformMatrix4dv");
    glad_glUniformMatrix4x2dv = (PFNGLUNIFORMMATRIX4X2DVPROC) load(userptr, "glUniformMatrix4x2dv");
    glad_glUniformMatrix4x3dv = (PFNGLUNIFORMMATRIX4X3DVPROC) load(userptr, "glUniformMatrix4x3dv");
    glad_glUniformSubroutinesuiv = (PFNGLUNIFORMSUBROUTINESUIVPROC) load(userptr, "glUniformSubroutinesuiv");
}
static void glad_gl_load_GL_VERSION_4_1( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_4_1) return;
    glad_glActiveShaderProgram = (PFNGLACTIVESHADERPROGRAMPROC) load(userptr, "glActiveShaderProgram");
    glad_glBindProgramPipeline = (PFNGLBINDPROGRAMPIPELINEPROC) load(userptr, "glBindProgramPipeline");
    glad_glClearDepthf = (PFNGLCLEARDEPTHFPROC) load(userptr, "glClearDepthf");
    glad_glCreateShaderProgramv = (PFNGLCREATESHADERPROGRAMVPROC) load(userptr, "glCreateShaderProgramv");
    glad_glDeleteProgramPipelines = (PFNGLDELETEPROGRAMPIPELINESPROC) load(userptr, "glDeleteProgramPipelines");
    glad_glDepthRangeArrayv = (PFNGLDEPTHRANGEARRAYVPROC) load(userptr, "glDepthRangeArrayv");
    glad_glDepthRangeIndexed = (PFNGLDEPTHRANGEINDEXEDPROC) load(userptr, "glDepthRangeIndexed");
    glad_glDepthRangef = (PFNGLDEPTHRANGEFPROC) load(userptr, "glDepthRangef");
    glad_glGenProgramPipelines = (PFNGLGENPROGRAMPIPELINESPROC) load(userptr, "glGenProgramPipelines");
    glad_glGetDoublei_v = (PFNGLGETDOUBLEI_VPROC) load(userptr, "glGetDoublei_v");
    glad_glGetFloati_v = (PFNGLGETFLOATI_VPROC) load(userptr, "glGetFloati_v");
    glad_glGetProgramBinary = (PFNGLGETPROGRAMBINARYPROC) load(userptr, "glGetProgramBinary");
    glad_glGetProgramPipelineInfoLog = (PFNGLGETPROGRAMPIPELINEINFOLOGPROC) load(userptr, "glGetProgramPipelineInfoLog");
    glad_glGetProgramPipelineiv = (PFNGLGETPROGRAMPIPELINEIVPROC) load(userptr, "glGetProgramPipelineiv");
    glad_glGetShaderPrecisionFormat = (PFNGLGETSHADERPRECISIONFORMATPROC) load(userptr, "glGetShaderPrecisionFormat");
    glad_glGetVertexAttribLdv = (PFNGLGETVERTEXATTRIBLDVPROC) load(userptr, "glGetVertexAttribLdv");
    glad_glIsProgramPipeline = (PFNGLISPROGRAMPIPELINEPROC) load(userptr, "glIsProgramPipeline");
    glad_glProgramBinary = (PFNGLPROGRAMBINARYPROC) load(userptr, "glProgramBinary");
    glad_glProgramParameteri = (PFNGLPROGRAMPARAMETERIPROC) load(userptr, "glProgramParameteri");
    glad_glProgramUniform1d = (PFNGLPROGRAMUNIFORM1DPROC) load(userptr, "glProgramUniform1d");
    glad_glProgramUniform1dv = (PFNGLPROGRAMUNIFORM1DVPROC) load(userptr, "glProgramUniform1dv");
    glad_glProgramUniform1f = (PFNGLPROGRAMUNIFORM1FPROC) load(userptr, "glProgramUniform1f");
    glad_glProgramUniform1fv = (PFNGLPROGRAMUNIFORM1FVPROC) load(userptr, "glProgramUniform1fv");
    glad_glProgramUniform1i = (PFNGLPROGRAMUNIFORM1IPROC) load(userptr, "glProgramUniform1i");
    glad_glProgramUniform1iv = (PFNGLPROGRAMUNIFORM1IVPROC) load(userptr, "glProgramUniform1iv");
    glad_glProgramUniform1ui = (PFNGLPROGRAMUNIFORM1UIPROC) load(userptr, "glProgramUniform1ui");
    glad_glProgramUniform1uiv = (PFNGLPROGRAMUNIFORM1UIVPROC) load(userptr, "glProgramUniform1uiv");
    glad_glProgramUniform2d = (PFNGLPROGRAMUNIFORM2DPROC) load(userptr, "glProgramUniform2d");
    glad_glProgramUniform2dv = (PFNGLPROGRAMUNIFORM2DVPROC) load(userptr, "glProgramUniform2dv");
    glad_glProgramUniform2f = (PFNGLPROGRAMUNIFORM2FPROC) load(userptr, "glProgramUniform2f");
    glad_glProgramUniform2fv = (PFNGLPROGRAMUNIFORM2FVPROC) load(userptr, "glProgramUniform2fv");
    glad_glProgramUniform2i = (PFNGLPROGRAMUNIFORM2IPROC) load(userptr, "glProgramUniform2i");
    glad_glProgramUniform2iv = (PFNGLPROGRAMUNIFORM2IVPROC) load(userptr, "glProgramUniform2iv");
    glad_glProgramUniform2ui = (PFNGLPROGRAMUNIFORM2UIPROC) load(userptr, "glProgramUniform2ui");
    glad_glProgramUniform2uiv = (PFNGLPROGRAMUNIFORM2UIVPROC) load(userptr, "glProgramUniform2uiv");
    glad_glProgramUniform3d = (PFNGLPROGRAMUNIFORM3DPROC) load(userptr, "glProgramUniform3d");
    glad_glProgramUniform3dv = (PFNGLPROGRAMUNIFORM3DVPROC) load(userptr, "glProgramUniform3dv");
    glad_glProgramUniform3f = (PFNGLPROGRAMUNIFORM3FPROC) load(userptr, "glProgramUniform3f");
    glad_glProgramUniform3fv = (PFNGLPROGRAMUNIFORM3FVPROC) load(userptr, "glProgramUniform3fv");
    glad_glProgramUniform3i = (PFNGLPROGRAMUNIFORM3IPROC) load(userptr, "glProgramUniform3i");
    glad_glProgramUniform3iv = (PFNGLPROGRAMUNIFORM3IVPROC) load(userptr, "glProgramUniform3iv");
    glad_glProgramUniform3ui = (PFNGLPROGRAMUNIFORM3UIPROC) load(userptr, "glProgramUniform3ui");
    glad_glProgramUniform3uiv = (PFNGLPROGRAMUNIFORM3UIVPROC) load(userptr, "glProgramUniform3uiv");
    glad_glProgramUniform4d = (PFNGLPROGRAMUNIFORM4DPROC) load(userptr, "glProgramUniform4d");
    glad_glProgramUniform4dv = (PFNGLPROGRAMUNIFORM4DVPROC) load(userptr, "glProgramUniform4dv");
    glad_glProgramUniform4f = (PFNGLPROGRAMUNIFORM4FPROC) load(userptr, "glProgramUniform4f");
    glad_glProgramUniform4fv = (PFNGLPROGRAMUNIFORM4FVPROC) load(userptr, "glProgramUniform4fv");
    glad_glProgramUniform4i = (PFNGLPROGRAMUNIFORM4IPROC) load(userptr, "glProgramUniform4i");
    glad_glProgramUniform4iv = (PFNGLPROGRAMUNIFORM4IVPROC) load(userptr, "glProgramUniform4iv");
    glad_glProgramUniform4ui = (PFNGLPROGRAMUNIFORM4UIPROC) load(userptr, "glProgramUniform4ui");
    glad_glProgramUniform4uiv = (PFNGLPROGRAMUNIFORM4UIVPROC) load(userptr, "glProgramUniform4uiv");
    glad_glProgramUniformMatrix2dv = (PFNGLPROGRAMUNIFORMMATRIX2DVPROC) load(userptr, "glProgramUniformMatrix2dv");
    glad_glProgramUniformMatrix2fv = (PFNGLPROGRAMUNIFORMMATRIX2FVPROC) load(userptr, "glProgramUniformMatrix2fv");
    glad_glProgramUniformMatrix2x3dv = (PFNGLPROGRAMUNIFORMMATRIX2X3DVPROC) load(userptr, "glProgramUniformMatrix2x3dv");
    glad_glProgramUniformMatrix2x3fv = (PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC) load(userptr, "glProgramUniformMatrix2x3fv");
    glad_glProgramUniformMatrix2x4dv = (PFNGLPROGRAMUNIFORMMATRIX2X4DVPROC) load(userptr, "glProgramUniformMatrix2x4dv");
    glad_glProgramUniformMatrix2x4fv = (PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC) load(userptr, "glProgramUniformMatrix2x4fv");
    glad_glProgramUniformMatrix3dv = (PFNGLPROGRAMUNIFORMMATRIX3DVPROC) load(userptr, "glProgramUniformMatrix3dv");
    glad_glProgramUniformMatrix3fv = (PFNGLPROGRAMUNIFORMMATRIX3FVPROC) load(userptr, "glProgramUniformMatrix3fv");
    glad_glProgramUniformMatrix3x2dv = (PFNGLPROGRAMUNIFORMMATRIX3X2DVPROC) load(userptr, "glProgramUniformMatrix3x2dv");
    glad_glProgramUniformMatrix3x2fv = (PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC) load(userptr, "glProgramUniformMatrix3x2fv");
    glad_glProgramUniformMatrix3x4dv = (PFNGLPROGRAMUNIFORMMATRIX3X4DVPROC) load(userptr, "glProgramUniformMatrix3x4dv");
    glad_glProgramUniformMatrix3x4fv = (PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC) load(userptr, "glProgramUniformMatrix3x4fv");
    glad_glProgramUniformMatrix4dv = (PFNGLPROGRAMUNIFORMMATRIX4DVPROC) load(userptr, "glProgramUniformMatrix4dv");
    glad_glProgramUniformMatrix4fv = (PFNGLPROGRAMUNIFORMMATRIX4FVPROC) load(userptr, "glProgramUniformMatrix4fv");
    glad_glProgramUniformMatrix4x2dv = (PFNGLPROGRAMUNIFORMMATRIX4X2DVPROC) load(userptr, "glProgramUniformMatrix4x2dv");
    glad_glProgramUniformMatrix4x2fv = (PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC) load(userptr, "glProgramUniformMatrix4x2fv");
    glad_glProgramUniformMatrix4x3dv = (PFNGLPROGRAMUNIFORMMATRIX4X3DVPROC) load(userptr, "glProgramUniformMatrix4x3dv");
    glad_glProgramUniformMatrix4x3fv = (PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC) load(userptr, "glProgramUniformMatrix4x3fv");
    glad_glReleaseShaderCompiler = (PFNGLRELEASESHADERCOMPILERPROC) load(userptr, "glReleaseShaderCompiler");
    glad_glScissorArrayv = (PFNGLSCISSORARRAYVPROC) load(userptr, "glScissorArrayv");
    glad_glScissorIndexed = (PFNGLSCISSORINDEXEDPROC) load(userptr, "glScissorIndexed");
    glad_glScissorIndexedv = (PFNGLSCISSORINDEXEDVPROC) load(userptr, "glScissorIndexedv");
    glad_glShaderBinary = (PFNGLSHADERBINARYPROC) load(userptr, "glShaderBinary");
    glad_glUseProgramStages = (PFNGLUSEPROGRAMSTAGESPROC) load(userptr, "glUseProgramStages");
    glad_glValidateProgramPipeline = (PFNGLVALIDATEPROGRAMPIPELINEPROC) load(userptr, "glValidateProgramPipeline");
    glad_glVertexAttribL1d = (PFNGLVERTEXATTRIBL1DPROC) load(userptr, "glVertexAttribL1d");
    glad_glVertexAttribL1dv = (PFNGLVERTEXATTRIBL1DVPROC) load(userptr, "glVertexAttribL1dv");
    glad_glVertexAttribL2d = (PFNGLVERTEXATTRIBL2DPROC) load(userptr, "glVertexAttribL2d");
    glad_glVertexAttribL2dv = (PFNGLVERTEXATTRIBL2DVPROC) load(userptr, "glVertexAttribL2dv");
    glad_glVertexAttribL3d = (PFNGLVERTEXATTRIBL3DPROC) load(userptr, "glVertexAttribL3d");
    glad_glVertexAttribL3dv = (PFNGLVERTEXATTRIBL3DVPROC) load(userptr, "glVertexAttribL3dv");
    glad_glVertexAttribL4d = (PFNGLVERTEXATTRIBL4DPROC) load(userptr, "glVertexAttribL4d");
    glad_glVertexAttribL4dv = (PFNGLVERTEXATTRIBL4DVPROC) load(userptr, "glVertexAttribL4dv");
    glad_glVertexAttribLPointer = (PFNGLVERTEXATTRIBLPOINTERPROC) load(userptr, "glVertexAttribLPointer");
    glad_glViewportArrayv = (PFNGLVIEWPORTARRAYVPROC) load(userptr, "glViewportArrayv");
    glad_glViewportIndexedf = (PFNGLVIEWPORTINDEXEDFPROC) load(userptr, "glViewportIndexedf");
    glad_glViewportIndexedfv = (PFNGLVIEWPORTINDEXEDFVPROC) load(userptr, "glViewportIndexedfv");
}
static void glad_gl_load_GL_VERSION_4_2( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_4_2) return;
    glad_glBindImageTexture = (PFNGLBINDIMAGETEXTUREPROC) load(userptr, "glBindImageTexture");
    glad_glDrawArraysInstancedBaseInstance = (PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC) load(userptr, "glDrawArraysInstancedBaseInstance");
    glad_glDrawElementsInstancedBaseInstance = (PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC) load(userptr, "glDrawElementsInstancedBaseInstance");
    glad_glDrawElementsInstancedBaseVertexBaseInstance = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC) load(userptr, "glDrawElementsInstancedBaseVertexBaseInstance");
    glad_glDrawTransformFeedbackInstanced = (PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC) load(userptr, "glDrawTransformFeedbackInstanced");
    glad_glDrawTransformFeedbackStreamInstanced = (PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC) load(userptr, "glDrawTransformFeedbackStreamInstanced");
    glad_glGetActiveAtomicCounterBufferiv = (PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC) load(userptr, "glGetActiveAtomicCounterBufferiv");
    glad_glGetInternalformativ = (PFNGLGETINTERNALFORMATIVPROC) load(userptr, "glGetInternalformativ");
    glad_glMemoryBarrier = (PFNGLMEMORYBARRIERPROC) load(userptr, "glMemoryBarrier");
    glad_glTexStorage1D = (PFNGLTEXSTORAGE1DPROC) load(userptr, "glTexStorage1D");
    glad_glTexStorage2D = (PFNGLTEXSTORAGE2DPROC) load(userptr, "glTexStorage2D");
    glad_glTexStorage3D = (PFNGLTEXSTORAGE3DPROC) load(userptr, "glTexStorage3D");
}
static void glad_gl_load_GL_VERSION_4_3( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_4_3) return;
    glad_glBindVertexBuffer = (PFNGLBINDVERTEXBUFFERPROC) load(userptr, "glBindVertexBuffer");
    glad_glClearBufferData = (PFNGLCLEARBUFFERDATAPROC) load(userptr, "glClearBufferData");
    glad_glClearBufferSubData = (PFNGLCLEARBUFFERSUBDATAPROC) load(userptr, "glClearBufferSubData");
    glad_glCopyImageSubData = (PFNGLCOPYIMAGESUBDATAPROC) load(userptr, "glCopyImageSubData");
    glad_glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC) load(userptr, "glDebugMessageCallback");
    glad_glDebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC) load(userptr, "glDebugMessageControl");
    glad_glDebugMessageInsert = (PFNGLDEBUGMESSAGEINSERTPROC) load(userptr, "glDebugMessageInsert");
    glad_glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC) load(userptr, "glDispatchCompute");
    glad_glDispatchComputeIndirect = (PFNGLDISPATCHCOMPUTEINDIRECTPROC) load(userptr, "glDispatchComputeIndirect");
    glad_glFramebufferParameteri = (PFNGLFRAMEBUFFERPARAMETERIPROC) load(userptr, "glFramebufferParameteri");
    glad_glGetDebugMessageLog = (PFNGLGETDEBUGMESSAGELOGPROC) load(userptr, "glGetDebugMessageLog");
    glad_glGetFramebufferParameteriv = (PFNGLGETFRAMEBUFFERPARAMETERIVPROC) load(userptr, "glGetFramebufferParameteriv");
    glad_glGetInternalformati64v = (PFNGLGETINTERNALFORMATI64VPROC) load(userptr, "glGetInternalformati64v");
    glad_glGetObjectLabel = (PFNGLGETOBJECTLABELPROC) load(userptr, "glGetObjectLabel");
    glad_glGetObjectPtrLabel = (PFNGLGETOBJECTPTRLABELPROC) load(userptr, "glGetObjectPtrLabel");
    glad_glGetPointerv = (PFNGLGETPOINTERVPROC) load(userptr, "glGetPointerv");
    glad_glGetProgramInterfaceiv = (PFNGLGETPROGRAMINTERFACEIVPROC) load(userptr, "glGetProgramInterfaceiv");
    glad_glGetProgramResourceIndex = (PFNGLGETPROGRAMRESOURCEINDEXPROC) load(userptr, "glGetProgramResourceIndex");
    glad_glGetProgramResourceLocation = (PFNGLGETPROGRAMRESOURCELOCATIONPROC) load(userptr, "glGetProgramResourceLocation");
    glad_glGetProgramResourceLocationIndex = (PFNGLGETPROGRAMRESOURCELOCATIONINDEXPROC) load(userptr, "glGetProgramResourceLocationIndex");
    glad_glGetProgramResourceName = (PFNGLGETPROGRAMRESOURCENAMEPROC) load(userptr, "glGetProgramResourceName");
    glad_glGetProgramResourceiv = (PFNGLGETPROGRAMRESOURCEIVPROC) load(userptr, "glGetProgramResourceiv");
    glad_glInvalidateBufferData = (PFNGLINVALIDATEBUFFERDATAPROC) load(userptr, "glInvalidateBufferData");
    glad_glInvalidateBufferSubData = (PFNGLINVALIDATEBUFFERSUBDATAPROC) load(userptr, "glInvalidateBufferSubData");
    glad_glInvalidateFramebuffer = (PFNGLINVALIDATEFRAMEBUFFERPROC) load(userptr, "glInvalidateFramebuffer");
    glad_glInvalidateSubFramebuffer = (PFNGLINVALIDATESUBFRAMEBUFFERPROC) load(userptr, "glInvalidateSubFramebuffer");
    glad_glInvalidateTexImage = (PFNGLINVALIDATETEXIMAGEPROC) load(userptr, "glInvalidateTexImage");
    glad_glInvalidateTexSubImage = (PFNGLINVALIDATETEXSUBIMAGEPROC) load(userptr, "glInvalidateTexSubImage");
    glad_glMultiDrawArraysIndirect = (PFNGLMULTIDRAWARRAYSINDIRECTPROC) load(userptr, "glMultiDrawArraysIndirect");
    glad_glMultiDrawElementsIndirect = (PFNGLMULTIDRAWELEMENTSINDIRECTPROC) load(userptr, "glMultiDrawElementsIndirect");
    glad_glObjectLabel = (PFNGLOBJECTLABELPROC) load(userptr, "glObjectLabel");
    glad_glObjectPtrLabel = (PFNGLOBJECTPTRLABELPROC) load(userptr, "glObjectPtrLabel");
    glad_glPopDebugGroup = (PFNGLPOPDEBUGGROUPPROC) load(userptr, "glPopDebugGroup");
    glad_glPushDebugGroup = (PFNGLPUSHDEBUGGROUPPROC) load(userptr, "glPushDebugGroup");
    glad_glShaderStorageBlockBinding = (PFNGLSHADERSTORAGEBLOCKBINDINGPROC) load(userptr, "glShaderStorageBlockBinding");
    glad_glTexBufferRange = (PFNGLTEXBUFFERRANGEPROC) load(userptr, "glTexBufferRange");
    glad_glTexStorage2DMultisample = (PFNGLTEXSTORAGE2DMULTISAMPLEPROC) load(userptr, "glTexStorage2DMultisample");
    glad_glTexStorage3DMultisample = (PFNGLTEXSTORAGE3DMULTISAMPLEPROC) load(userptr, "glTexStorage3DMultisample");
    glad_glTextureView = (PFNGLTEXTUREVIEWPROC) load(userptr, "glTextureView");
    glad_glVertexAttribBinding = (PFNGLVERTEXATTRIBBINDINGPROC) load(userptr, "glVertexAttribBinding");
    glad_glVertexAttribFormat = (PFNGLVERTEXATTRIBFORMATPROC) load(userptr, "glVertexAttribFormat");
    glad_glVertexAttribIFormat = (PFNGLVERTEXATTRIBIFORMATPROC) load(userptr, "glVertexAttribIFormat");
    glad_glVertexAttribLFormat = (PFNGLVERTEXATTRIBLFORMATPROC) load(userptr, "glVertexAttribLFormat");
    glad_glVertexBindingDivisor = (PFNGLVERTEXBINDINGDIVISORPROC) load(userptr, "glVertexBindingDivisor");
}
static void glad_gl_load_GL_VERSION_4_4( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_4_4) return;
    glad_glBindBuffersBase = (PFNGLBINDBUFFERSBASEPROC) load(userptr, "glBindBuffersBase");
    glad_glBindBuffersRange = (PFNGLBINDBUFFERSRANGEPROC) load(userptr, "glBindBuffersRange");
    glad_glBindImageTextures = (PFNGLBINDIMAGETEXTURESPROC) load(userptr, "glBindImageTextures");
    glad_glBindSamplers = (PFNGLBINDSAMPLERSPROC) load(userptr, "glBindSamplers");
    glad_glBindTextures = (PFNGLBINDTEXTURESPROC) load(userptr, "glBindTextures");
    glad_glBindVertexBuffers = (PFNGLBINDVERTEXBUFFERSPROC) load(userptr, "glBindVertexBuffers");
    glad_glBufferStorage = (PFNGLBUFFERSTORAGEPROC) load(userptr, "glBufferStorage");
    glad_glClearTexImage = (PFNGLCLEARTEXIMAGEPROC) load(userptr, "glClearTexImage");
    glad_glClearTexSubImage = (PFNGLCLEARTEXSUBIMAGEPROC) load(userptr, "glClearTexSubImage");
}
static void glad_gl_load_GL_VERSION_4_5( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_4_5) return;
    glad_glBindTextureUnit = (PFNGLBINDTEXTUREUNITPROC) load(userptr, "glBindTextureUnit");
    glad_glBlitNamedFramebuffer = (PFNGLBLITNAMEDFRAMEBUFFERPROC) load(userptr, "glBlitNamedFramebuffer");
    glad_glCheckNamedFramebufferStatus = (PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC) load(userptr, "glCheckNamedFramebufferStatus");
    glad_glClearNamedBufferData = (PFNGLCLEARNAMEDBUFFERDATAPROC) load(userptr, "glClearNamedBufferData");
    glad_glClearNamedBufferSubData = (PFNGLCLEARNAMEDBUFFERSUBDATAPROC) load(userptr, "glClearNamedBufferSubData");
    glad_glClearNamedFramebufferfi = (PFNGLCLEARNAMEDFRAMEBUFFERFIPROC) load(userptr, "glClearNamedFramebufferfi");
    glad_glClearNamedFramebufferfv = (PFNGLCLEARNAMEDFRAMEBUFFERFVPROC) load(userptr, "glClearNamedFramebufferfv");
    glad_glClearNamedFramebufferiv = (PFNGLCLEARNAMEDFRAMEBUFFERIVPROC) load(userptr, "glClearNamedFramebufferiv");
    glad_glClearNamedFramebufferuiv = (PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC) load(userptr, "glClearNamedFramebufferuiv");
    glad_glClipControl = (PFNGLCLIPCONTROLPROC) load(userptr, "glClipControl");
    glad_glCompressedTextureSubImage1D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE1DPROC) load(userptr, "glCompressedTextureSubImage1D");
    glad_glCompressedTextureSubImage2D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC) load(userptr, "glCompressedTextureSubImage2D");
    glad_glCompressedTextureSubImage3D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE3DPROC) load(userptr, "glCompressedTextureSubImage3D");
    glad_glCopyNamedBufferSubData = (PFNGLCOPYNAMEDBUFFERSUBDATAPROC) load(userptr, "glCopyNamedBufferSubData");
    glad_glCopyTextureSubImage1D = (PFNGLCOPYTEXTURESUBIMAGE1DPROC) load(userptr, "glCopyTextureSubImage1D");
    glad_glCopyTextureSubImage2D = (PFNGLCOPYTEXTURESUBIMAGE2DPROC) load(userptr, "glCopyTextureSubImage2D");
    glad_glCopyTextureSubImage3D = (PFNGLCOPYTEXTURESUBIMAGE3DPROC) load(userptr, "glCopyTextureSubImage3D");
    glad_glCreateBuffers = (PFNGLCREATEBUFFERSPROC) load(userptr, "glCreateBuffers");
    glad_glCreateFramebuffers = (PFNGLCREATEFRAMEBUFFERSPROC) load(userptr, "glCreateFramebuffers");
    glad_glCreateProgramPipelines = (PFNGLCREATEPROGRAMPIPELINESPROC) load(userptr, "glCreateProgramPipelines");
    glad_glCreateQueries = (PFNGLCREATEQUERIESPROC) load(userptr, "glCreateQueries");
    glad_glCreateRenderbuffers = (PFNGLCREATERENDERBUFFERSPROC) load(userptr, "glCreateRenderbuffers");
    glad_glCreateSamplers = (PFNGLCREATESAMPLERSPROC) load(userptr, "glCreateSamplers");
    glad_glCreateTextures = (PFNGLCREATETEXTURESPROC) load(userptr, "glCreateTextures");
    glad_glCreateTransformFeedbacks = (PFNGLCREATETRANSFORMFEEDBACKSPROC) load(userptr, "glCreateTransformFeedbacks");
    glad_glCreateVertexArrays = (PFNGLCREATEVERTEXARRAYSPROC) load(userptr, "glCreateVertexArrays");
    glad_glDisableVertexArrayAttrib = (PFNGLDISABLEVERTEXARRAYATTRIBPROC) load(userptr, "glDisableVertexArrayAttrib");
    glad_glEnableVertexArrayAttrib = (PFNGLENABLEVERTEXARRAYATTRIBPROC) load(userptr, "glEnableVertexArrayAttrib");
    glad_glFlushMappedNamedBufferRange = (PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC) load(userptr, "glFlushMappedNamedBufferRange");
    glad_glGenerateTextureMipmap = (PFNGLGENERATETEXTUREMIPMAPPROC) load(userptr, "glGenerateTextureMipmap");
    glad_glGetCompressedTextureImage = (PFNGLGETCOMPRESSEDTEXTUREIMAGEPROC) load(userptr, "glGetCompressedTextureImage");
    glad_glGetCompressedTextureSubImage = (PFNGLGETCOMPRESSEDTEXTURESUBIMAGEPROC) load(userptr, "glGetCompressedTextureSubImage");
    glad_glGetGraphicsResetStatus = (PFNGLGETGRAPHICSRESETSTATUSPROC) load(userptr, "glGetGraphicsResetStatus");
    glad_glGetNamedBufferParameteri64v = (PFNGLGETNAMEDBUFFERPARAMETERI64VPROC) load(userptr, "glGetNamedBufferParameteri64v");
    glad_glGetNamedBufferParameteriv = (PFNGLGETNAMEDBUFFERPARAMETERIVPROC) load(userptr, "glGetNamedBufferParameteriv");
    glad_glGetNamedBufferPointerv = (PFNGLGETNAMEDBUFFERPOINTERVPROC) load(userptr, "glGetNamedBufferPointerv");
    glad_glGetNamedBufferSubData = (PFNGLGETNAMEDBUFFERSUBDATAPROC) load(userptr, "glGetNamedBufferSubData");
    glad_glGetNamedFramebufferAttachmentParameteriv = (PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC) load(userptr, "glGetNamedFramebufferAttachmentParameteriv");
    glad_glGetNamedFramebufferParameteriv = (PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC) load(userptr, "glGetNamedFramebufferParameteriv");
    glad_glGetNamedRenderbufferParameteriv = (PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC) load(userptr, "glGetNamedRenderbufferParameteriv");
    glad_glGetQueryBufferObjecti64v = (PFNGLGETQUERYBUFFEROBJECTI64VPROC) load(userptr, "glGetQueryBufferObjecti64v");
    glad_glGetQueryBufferObjectiv = (PFNGLGETQUERYBUFFEROBJECTIVPROC) load(userptr, "glGetQueryBufferObjectiv");
    glad_glGetQueryBufferObjectui64v = (PFNGLGETQUERYBUFFEROBJECTUI64VPROC) load(userptr, "glGetQueryBufferObjectui64v");
    glad_glGetQueryBufferObjectuiv = (PFNGLGETQUERYBUFFEROBJECTUIVPROC) load(userptr, "glGetQueryBufferObjectuiv");
    glad_glGetTextureImage = (PFNGLGETTEXTUREIMAGEPROC) load(userptr, "glGetTextureImage");
    glad_glGetTextureLevelParameterfv = (PFNGLGETTEXTURELEVELPARAMETERFVPROC) load(userptr, "glGetTextureLevelParameterfv");
    glad_glGetTextureLevelParameteriv = (PFNGLGETTEXTURELEVELPARAMETERIVPROC) load(userptr, "glGetTextureLevelParameteriv");
    glad_glGetTextureParameterIiv = (PFNGLGETTEXTUREPARAMETERIIVPROC) load(userptr, "glGetTextureParameterIiv");
    glad_glGetTextureParameterIuiv = (PFNGLGETTEXTUREPARAMETERIUIVPROC) load(userptr, "glGetTextureParameterIuiv");
    glad_glGetTextureParameterfv = (PFNGLGETTEXTUREPARAMETERFVPROC) load(userptr, "glGetTextureParameterfv");
    glad_glGetTextureParameteriv = (PFNGLGETTEXTUREPARAMETERIVPROC) load(userptr, "glGetTextureParameteriv");
    glad_glGetTextureSubImage = (PFNGLGETTEXTURESUBIMAGEPROC) load(userptr, "glGetTextureSubImage");
    glad_glGetTransformFeedbacki64_v = (PFNGLGETTRANSFORMFEEDBACKI64_VPROC) load(userptr, "glGetTransformFeedbacki64_v");
    glad_glGetTransformFeedbacki_v = (PFNGLGETTRANSFORMFEEDBACKI_VPROC) load(userptr, "glGetTransformFeedbacki_v");
    glad_glGetTransformFeedbackiv = (PFNGLGETTRANSFORMFEEDBACKIVPROC) load(userptr, "glGetTransformFeedbackiv");
    glad_glGetVertexArrayIndexed64iv = (PFNGLGETVERTEXARRAYINDEXED64IVPROC) load(userptr, "glGetVertexArrayIndexed64iv");
    glad_glGetVertexArrayIndexediv = (PFNGLGETVERTEXARRAYINDEXEDIVPROC) load(userptr, "glGetVertexArrayIndexediv");
    glad_glGetVertexArrayiv = (PFNGLGETVERTEXARRAYIVPROC) load(userptr, "glGetVertexArrayiv");
    glad_glGetnCompressedTexImage = (PFNGLGETNCOMPRESSEDTEXIMAGEPROC) load(userptr, "glGetnCompressedTexImage");
    glad_glGetnTexImage = (PFNGLGETNTEXIMAGEPROC) load(userptr, "glGetnTexImage");
    glad_glGetnUniformdv = (PFNGLGETNUNIFORMDVPROC) load(userptr, "glGetnUniformdv");
    glad_glGetnUniformfv = (PFNGLGETNUNIFORMFVPROC) load(userptr, "glGetnUniformfv");
    glad_glGetnUniformiv = (PFNGLGETNUNIFORMIVPROC) load(userptr, "glGetnUniformiv");
    glad_glGetnUniformuiv = (PFNGLGETNUNIFORMUIVPROC) load(userptr, "glGetnUniformuiv");
    glad_glInvalidateNamedFramebufferData = (PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC) load(userptr, "glInvalidateNamedFramebufferData");
    glad_glInvalidateNamedFramebufferSubData = (PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC) load(userptr, "glInvalidateNamedFramebufferSubData");
    glad_glMapNamedBuffer = (PFNGLMAPNAMEDBUFFERPROC) load(userptr, "glMapNamedBuffer");
    glad_glMapNamedBufferRange = (PFNGLMAPNAMEDBUFFERRANGEPROC) load(userptr, "glMapNamedBufferRange");
    glad_glMemoryBarrierByRegion = (PFNGLMEMORYBARRIERBYREGIONPROC) load(userptr, "glMemoryBarrierByRegion");
    glad_glNamedBufferData = (PFNGLNAMEDBUFFERDATAPROC) load(userptr, "glNamedBufferData");
    glad_glNamedBufferStorage = (PFNGLNAMEDBUFFERSTORAGEPROC) load(userptr, "glNamedBufferStorage");
    glad_glNamedBufferSubData = (PFNGLNAMEDBUFFERSUBDATAPROC) load(userptr, "glNamedBufferSubData");
    glad_glNamedFramebufferDrawBuffer = (PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC) load(userptr, "glNamedFramebufferDrawBuffer");
    glad_glNamedFramebufferDrawBuffers = (PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC) load(userptr, "glNamedFramebufferDrawBuffers");
    glad_glNamedFramebufferParameteri = (PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC) load(userptr, "glNamedFramebufferParameteri");
    glad_glNamedFramebufferReadBuffer = (PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC) load(userptr, "glNamedFramebufferReadBuffer");
    glad_glNamedFramebufferRenderbuffer = (PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC) load(userptr, "glNamedFramebufferRenderbuffer");
    glad_glNamedFramebufferTexture = (PFNGLNAMEDFRAMEBUFFERTEXTUREPROC) load(userptr, "glNamedFramebufferTexture");
    glad_glNamedFramebufferTextureLayer = (PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC) load(userptr, "glNamedFramebufferTextureLayer");
    glad_glNamedRenderbufferStorage = (PFNGLNAMEDRENDERBUFFERSTORAGEPROC) load(userptr, "glNamedRenderbufferStorage");
    glad_glNamedRenderbufferStorageMultisample = (PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC) load(userptr, "glNamedRenderbufferStorageMultisample");
    glad_glReadnPixels = (PFNGLREADNPIXELSPROC) load(userptr, "glReadnPixels");
    glad_glTextureBarrier = (PFNGLTEXTUREBARRIERPROC) load(userptr, "glTextureBarrier");
    glad_glTextureBuffer = (PFNGLTEXTUREBUFFERPROC) load(userptr, "glTextureBuffer");
    glad_glTextureBufferRange = (PFNGLTEXTUREBUFFERRANGEPROC) load(userptr, "glTextureBufferRange");
    glad_glTextureParameterIiv = (PFNGLTEXTUREPARAMETERIIVPROC) load(userptr, "glTextureParameterIiv");
    glad_glTextureParameterIuiv = (PFNGLTEXTUREPARAMETERIUIVPROC) load(userptr, "glTextureParameterIuiv");
    glad_glTextureParameterf = (PFNGLTEXTUREPARAMETERFPROC) load(userptr, "glTextureParameterf");
    glad_glTextureParameterfv = (PFNGLTEXTUREPARAMETERFVPROC) load(userptr, "glTextureParameterfv");
    glad_glTextureParameteri = (PFNGLTEXTUREPARAMETERIPROC) load(userptr, "glTextureParameteri");
    glad_glTextureParameteriv = (PFNGLTEXTUREPARAMETERIVPROC) load(userptr, "glTextureParameteriv");
    glad_glTextureStorage1D = (PFNGLTEXTURESTORAGE1DPROC) load(userptr, "glTextureStorage1D");
    glad_glTextureStorage2D = (PFNGLTEXTURESTORAGE2DPROC) load(userptr, "glTextureStorage2D");
    glad_glTextureStorage2DMultisample = (PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC) load(userptr, "glTextureStorage2DMultisample");
    glad_glTextureStorage3D = (PFNGLTEXTURESTORAGE3DPROC) load(userptr, "glTextureStorage3D");
    glad_glTextureStorage3DMultisample = (PFNGLTEXTURESTORAGE3DMULTISAMPLEPROC) load(userptr, "glTextureStorage3DMultisample");
    glad_glTextureSubImage1D = (PFNGLTEXTURESUBIMAGE1DPROC) load(userptr, "glTextureSubImage1D");
    glad_glTextureSubImage2D = (PFNGLTEXTURESUBIMAGE2DPROC) load(userptr, "glTextureSubImage2D");
    glad_glTextureSubImage3D = (PFNGLTEXTURESUBIMAGE3DPROC) load(userptr, "glTextureSubImage3D");
    glad_glTransformFeedbackBufferBase = (PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC) load(userptr, "glTransformFeedbackBufferBase");
    glad_glTransformFeedbackBufferRange = (PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC) load(userptr, "glTransformFeedbackBufferRange");
    glad_glUnmapNamedBuffer = (PFNGLUNMAPNAMEDBUFFERPROC) load(userptr, "glUnmapNamedBuffer");
    glad_glVertexArrayAttribBinding = (PFNGLVERTEXARRAYATTRIBBINDINGPROC) load(userptr, "glVertexArrayAttribBinding");
    glad_glVertexArrayAttribFormat = (PFNGLVERTEXARRAYATTRIBFORMATPROC) load(userptr, "glVertexArrayAttribFormat");
    glad_glVertexArrayAttribIFormat = (PFNGLVERTEXARRAYATTRIBIFORMATPROC) load(userptr, "glVertexArrayAttribIFormat");
    glad_glVertexArrayAttribLFormat = (PFNGLVERTEXARRAYATTRIBLFORMATPROC) load(userptr, "glVertexArrayAttribLFormat");
    glad_glVertexArrayBindingDivisor = (PFNGLVERTEXARRAYBINDINGDIVISORPROC) load(userptr, "glVertexArrayBindingDivisor");
    glad_glVertexArrayElementBuffer = (PFNGLVERTEXARRAYELEMENTBUFFERPROC) load(userptr, "glVertexArrayElementBuffer");
    glad_glVertexArrayVertexBuffer = (PFNGLVERTEXARRAYVERTEXBUFFERPROC) load(userptr, "glVertexArrayVertexBuffer");
    glad_glVertexArrayVertexBuffers = (PFNGLVERTEXARRAYVERTEXBUFFERSPROC) load(userptr, "glVertexArrayVertexBuffers");
}
static void glad_gl_load_GL_VERSION_4_6( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_4_6) return;
    glad_glMultiDrawArraysIndirectCount = (PFNGLMULTIDRAWARRAYSINDIRECTCOUNTPROC) load(userptr, "glMultiDrawArraysIndirectCount");
    glad_glMultiDrawElementsIndirectCount = (PFNGLMULTIDRAWELEMENTSINDIRECTCOUNTPROC) load(userptr, "glMultiDrawElementsIndirectCount");
    glad_glPolygonOffsetClamp = (PFNGLPOLYGONOFFSETCLAMPPROC) load(userptr, "glPolygonOffsetClamp");
    glad_glSpecializeShader = (PFNGLSPECIALIZESHADERPROC) load(userptr, "glSpecializeShader");
}
static void glad_gl_load_GL_ARB_buffer_storage( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_buffer_storage) return;
    glad_glBufferStorage = (PFNGLBUFFERSTORAGEPROC) load(userptr, "glBufferStorage");
}
static void glad_gl_load_GL_ARB_compute_shader( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_compute_shader) return;
    glad_glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC) load(userptr, "glDispatchCompute");
    glad_glDispatchComputeIndirect = (PFNGLDISPATCHCOMPUTEINDIRECTPROC) load(userptr, "glDispatchComputeIndirect");
}
static void glad_gl_load_GL_ARB_direct_state_access( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_direct_state_access) return;
    glad_glBindTextureUnit = (PFNGLBINDTEXTUREUNITPROC) load(userptr, "glBindTextureUnit");
    glad_glBlitNamedFramebuffer = (PFNGLBLITNAMEDFRAMEBUFFERPROC) load(userptr, "glBlitNamedFramebuffer");
    glad_glCheckNamedFramebufferStatus = (PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC) load(userptr, "glCheckNamedFramebufferStatus");
    glad_glClearNamedBufferData = (PFNGLCLEARNAMEDBUFFERDATAPROC) load(userptr, "glClearNamedBufferData");
    glad_glClearNamedBufferSubData = (PFNGLCLEARNAMEDBUFFERSUBDATAPROC) load(userptr, "glClearNamedBufferSubData");
    glad_glClearNamedFramebufferfi = (PFNGLCLEARNAMEDFRAMEBUFFERFIPROC) load(userptr, "glClearNamedFramebufferfi");
    glad_glClearNamedFramebufferfv = (PFNGLCLEARNAMEDFRAMEBUFFERFVPROC) load(userptr, "glClearNamedFramebufferfv");
    glad_glClearNamedFramebufferiv = (PFNGLCLEARNAMEDFRAMEBUFFERIVPROC) load(userptr, "glClearNamedFramebufferiv");
    glad_glClearNamedFramebufferuiv = (PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC) load(userptr, "glClearNamedFramebufferuiv");
    glad_glCompressedTextureSubImage1D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE1DPROC) load(userptr, "glCompressedTextureSubImage1D");
    glad_glCompressedTextureSubImage2D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC) load(userptr, "glCompressedTextureSubImage2D");
    glad_glCompressedTextureSubImage3D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE3DPROC) load(userptr, "glCompressedTextureSubImage3D");
    glad_glCopyNamedBufferSubData = (PFNGLCOPYNAMEDBUFFERSUBDATAPROC) load(userptr, "glCopyNamedBufferSubData");
    glad_glCopyTextureSubImage1D = (PFNGLCOPYTEXTURESUBIMAGE1DPROC) load(userptr, "glCopyTextureSubImage1D");
    glad_glCopyTextureSubImage2D = (PFNGLCOPYTEXTURESUBIMAGE2DPROC) load(userptr, "glCopyTextureSubImage2D");
    glad_glCopyTextureSubImage3D = (PFNGLCOPYTEXTURESUBIMAGE3DPROC) load(userptr, "glCopyTextureSubImage3D");
    glad_glCreateBuffers = (PFNGLCREATEBUFFERSPROC) load(userptr, "glCreateBuffers");
    glad_glCreateFramebuffers = (PFNGLCREATEFRAMEBUFFERSPROC) load(userptr, "glCreateFramebuffers");
    glad_glCreateProgramPipelines = (PFNGLCREATEPROGRAMPIPELINESPROC) load(userptr, "glCreateProgramPipelines");
    glad_glCreateQueries = (PFNGLCREATEQUERIESPROC) load(userptr, "glCreateQueries");
    glad_glCreateRenderbuffers = (PFNGLCREATERENDERBUFFERSPROC) load(userptr, "glCreateRenderbuffers");
    glad_glCreateSamplers = (PFNGLCREATESAMPLERSPROC) load(userptr, "glCreateSamplers");
    glad_glCreateTextures = (PFNGLCREATETEXTURESPROC) load(userptr, "glCreateTextures");
    glad_glCreateTransformFeedbacks = (PFNGLCREATETRANSFORMFEEDBACKSPROC) load(userptr, "glCreateTransformFeedbacks");
    glad_glCreateVertexArrays = (PFNGLCREATEVERTEXARRAYSPROC) load(userptr, "glCreateVertexArrays");
    glad_glDisableVertexArrayAttrib = (PFNGLDISABLEVERTEXARRAYATTRIBPROC) load(userptr, "glDisableVertexArrayAttrib");
    glad_glEnableVertexArrayAttrib = (PFNGLENABLEVERTEXARRAYATTRIBPROC) load(userptr, "glEnableVertexArrayAttrib");
    glad_glFlushMappedNamedBufferRange = (PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC) load(userptr, "glFlushMappedNamedBufferRange");
    glad_glGenerateTextureMipmap = (PFNGLGENERATETEXTUREMIPMAPPROC) load(userptr, "glGenerateTextureMipmap");
    glad_glGetCompressedTextureImage = (PFNGLGETCOMPRESSEDTEXTUREIMAGEPROC) load(userptr, "glGetCompressedTextureImage");
    glad_glGetNamedBufferParameteri64v = (PFNGLGETNAMEDBUFFERPARAMETERI64VPROC) load(userptr, "glGetNamedBufferParameteri64v");
    glad_glGetNamedBufferParameteriv = (PFNGLGETNAMEDBUFFERPARAMETERIVPROC) load(userptr, "glGetNamedBufferParameteriv");
    glad_glGetNamedBufferPointerv = (PFNGLGETNAMEDBUFFERPOINTERVPROC) load(userptr, "glGetNamedBufferPointerv");
    glad_glGetNamedBufferSubData = (PFNGLGETNAMEDBUFFERSUBDATAPROC) load(userptr, "glGetNamedBufferSubData");
    glad_glGetNamedFramebufferAttachmentParameteriv = (PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC) load(userptr, "glGetNamedFramebufferAttachmentParameteriv");
    glad_glGetNamedFramebufferParameteriv = (PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC) load(userptr, "glGetNamedFramebufferParameteriv");
    glad_glGetNamedRenderbufferParameteriv = (PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC) load(userptr, "glGetNamedRenderbufferParameteriv");
    glad_glGetQueryBufferObjecti64v = (PFNGLGETQUERYBUFFEROBJECTI64VPROC) load(userptr, "glGetQueryBufferObjecti64v");
    glad_glGetQueryBufferObjectiv = (PFNGLGETQUERYBUFFEROBJECTIVPROC) load(userptr, "glGetQueryBufferObjectiv");
    glad_glGetQueryBufferObjectui64v = (PFNGLGETQUERYBUFFEROBJECTUI64VPROC) load(userptr, "glGetQueryBufferObjectui64v");
    glad_glGetQueryBufferObjectuiv = (PFNGLGETQUERYBUFFEROBJECTUIVPROC) load(userptr, "glGetQueryBufferObjectuiv");
    glad_glGetTextureImage = (PFNGLGETTEXTUREIMAGEPROC) load(userptr, "glGetTextureImage");
    glad_glGetTextureLevelParameterfv = (PFNGLGETTEXTURELEVELPARAMETERFVPROC) load(userptr, "glGetTextureLevelParameterfv");
    glad_glGetTextureLevelParameteriv = (PFNGLGETTEXTURELEVELPARAMETERIVPROC) load(userptr, "glGetTextureLevelParameteriv");
    glad_glGetTextureParameterIiv = (PFNGLGETTEXTUREPARAMETERIIVPROC) load(userptr, "glGetTextureParameterIiv");
    glad_glGetTextureParameterIuiv = (PFNGLGETTEXTUREPARAMETERIUIVPROC) load(userptr, "glGetTextureParameterIuiv");
    glad_glGetTextureParameterfv = (PFNGLGETTEXTUREPARAMETERFVPROC) load(userptr, "glGetTextureParameterfv");
    glad_glGetTextureParameteriv = (PFNGLGETTEXTUREPARAMETERIVPROC) load(userptr, "glGetTextureParameteriv");
    glad_glGetTransformFeedbacki64_v = (PFNGLGETTRANSFORMFEEDBACKI64_VPROC) load(userptr, "glGetTransformFeedbacki64_v");
    glad_glGetTransformFeedbacki_v = (PFNGLGETTRANSFORMFEEDBACKI_VPROC) load(userptr, "glGetTransformFeedbacki_v");
    glad_glGetTransformFeedbackiv = (PFNGLGETTRANSFORMFEEDBACKIVPROC) load(userptr, "glGetTransformFeedbackiv");
    glad_glGetVertexArrayIndexed64iv = (PFNGLGETVERTEXARRAYINDEXED64IVPROC) load(userptr, "glGetVertexArrayIndexed64iv");
    glad_glGetVertexArrayIndexediv = (PFNGLGETVERTEXARRAYINDEXEDIVPROC) load(userptr, "glGetVertexArrayIndexediv");
    glad_glGetVertexArrayiv = (PFNGLGETVERTEXARRAYIVPROC) load(userptr, "glGetVertexArrayiv");
    glad_glInvalidateNamedFramebufferData = (PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC) load(userptr, "glInvalidateNamedFramebufferData");
    glad_glInvalidateNamedFramebufferSubData = (PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC) load(userptr, "glInvalidateNamedFramebufferSubData");
    glad_glMapNamedBuffer = (PFNGLMAPNAMEDBUFFERPROC) load(userptr, "glMapNamedBuffer");
    glad_glMapNamedBufferRange = (PFNGLMAPNAMEDBUFFERRANGEPROC) load(userptr, "glMapNamedBufferRange");
    glad_glNamedBufferData = (PFNGLNAMEDBUFFERDATAPROC) load(userptr, "glNamedBufferData");
    glad_glNamedBufferStorage = (PFNGLNAMEDBUFFERSTORAGEPROC) load(userptr, "glNamedBufferStorage");
    glad_glNamedBufferSubData = (PFNGLNAMEDBUFFERSUBDATAPROC) load(userptr, "glNamedBufferSubData");
    glad_glNamedFramebufferDrawBuffer = (PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC) load(userptr, "glNamedFramebufferDrawBuffer");
    glad_glNamedFramebufferDrawBuffers = (PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC) load(userptr, "glNamedFramebufferDrawBuffers");
    glad_glNamedFramebufferParameteri = (PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC) load(userptr, "glNamedFramebufferParameteri");
    glad_glNamedFramebufferReadBuffer = (PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC) load(userptr, "glNamedFramebufferReadBuffer");
    glad_glNamedFramebufferRenderbuffer = (PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC) load(userptr, "glNamedFramebufferRenderbuffer");
    glad_glNamedFramebufferTexture = (PFNGLNAMEDFRAMEBUFFERTEXTUREPROC) load(userptr, "glNamedFramebufferTexture");
    glad_glNamedFramebufferTextureLayer = (PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC) load(userptr, "glNamedFramebufferTextureLayer");
    glad_glNamedRenderbufferStorage = (PFNGLNAMEDRENDERBUFFERSTORAGEPROC) load(userptr, "glNamedRenderbufferStorage");
    glad_glNamedRenderbufferStorageMultisample = (PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC) load(userptr, "glNamedRenderbufferStorageMultisample");
    glad_glTextureBuffer = (PFNGLTEXTUREBUFFERPROC) load(userptr, "glTextureBuffer");
    glad_glTextureBufferRange = (PFNGLTEXTUREBUFFERRANGEPROC) load(userptr, "glTextureBufferRange");
    glad_glTextureParameterIiv = (PFNGLTEXTUREPARAMETERIIVPROC) load(userptr, "glTextureParameterIiv");
    glad_glTextureParameterIuiv = (PFNGLTEXTUREPARAMETERIUIVPROC) load(userptr, "glTextureParameterIuiv");
    glad_glTextureParameterf = (PFNGLTEXTUREPARAMETERFPROC) load(userptr, "glTextureParameterf");
    glad_glTextureParameterfv = (PFNGLTEXTUREPARAMETERFVPROC) load(userptr, "glTextureParameterfv");
    glad_glTextureParameteri = (PFNGLTEXTUREPARAMETERIPROC) load(userptr, "glTextureParameteri");
    glad_glTextureParameteriv = (PFNGLTEXTUREPARAMETERIVPROC) load(userptr, "glTextureParameteriv");
    glad_glTextureStorage1D = (PFNGLTEXTURESTORAGE1DPROC) load(userptr, "glTextureStorage1D");
    glad_glTextureStorage2D = (PFNGLTEXTURESTORAGE2DPROC) load(userptr, "glTextureStorage2D");
    glad_glTextureStorage2DMultisample = (PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC) load(userptr, "glTextureStorage2DMultisample");
    glad_glTextureStorage3D = (PFNGLTEXTURESTORAGE3DPROC) load(userptr, "glTextureStorage3D");
    glad_glTextureStorage3DMultisample = (PFNGLTEXTURESTORAGE3DMULTISAMPLEPROC) load(userptr, "glTextureStorage3DMultisample");
    glad_glTextureSubImage1D = (PFNGLTEXTURESUBIMAGE1DPROC) load(userptr, "glTextureSubImage1D");
    glad_glTextureSubImage2D = (PFNGLTEXTURESUBIMAGE2DPROC) load(userptr, "glTextureSubImage2D");
    glad_glTextureSubImage3D = (PFNGLTEXTURESUBIMAGE3DPROC) load(userptr, "glTextureSubImage3D");
    glad_glTransformFeedbackBufferBase = (PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC) load(userptr, "glTransformFeedbackBufferBase");
    glad_glTransformFeedbackBufferRange = (PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC) load(userptr, "glTransformFeedbackBufferRange");
    glad_glUnmapNamedBuffer = (PFNGLUNMAPNAMEDBUFFERPROC) load(userptr, "glUnmapNamedBuffer");
    glad_glVertexArrayAttribBinding = (PFNGLVERTEXARRAYATTRIBBINDINGPROC) load(userptr, "glVertexArrayAttribBinding");
    glad_glVertexArrayAttribFormat = (PFNGLVERTEXARRAYATTRIBFORMATPROC) load(userptr, "glVertexArrayAttribFormat");
    glad_glVertexArrayAttribIFormat = (PFNGLVERTEXARRAYATTRIBIFORMATPROC) load(userptr, "glVertexArrayAttribIFormat");
    glad_glVertexArrayAttribLFormat = (PFNGLVERTEXARRAYATTRIBLFORMATPROC) load(userptr, "glVertexArrayAttribLFormat");
    glad_glVertexArrayBindingDivisor = (PFNGLVERTEXARRAYBINDINGDIVISORPROC) load(userptr, "glVertexArrayBindingDivisor");
    glad_glVertexArrayElementBuffer = (PFNGLVERTEXARRAYELEMENTBUFFERPROC) load(userptr, "glVertexArrayElementBuffer");
    glad_glVertexArrayVertexBuffer = (PFNGLVERTEXARRAYVERTEXBUFFERPROC) load(userptr, "glVertexArrayVertexBuffer");
    glad_glVertexArrayVertexBuffers = (PFNGLVERTEXARRAYVERTEXBUFFERSPROC) load(userptr, "glVertexArrayVertexBuffers");
}
static void glad_gl_load_GL_ARB_multi_draw_indirect( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_multi_draw_indirect) return;
    glad_glMultiDrawArraysIndirect = (PFNGLMULTIDRAWARRAYSINDIRECTPROC) load(userptr, "glMultiDrawArraysIndirect");
    glad_glMultiDrawElementsIndirect = (PFNGLMULTIDRAWELEMENTSINDIRECTPROC) load(userptr, "glMultiDrawElementsIndirect");
}
static void glad_gl_load_GL_ARB_robustness( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_robustness) return;
    glad_glGetGraphicsResetStatusARB = (PFNGLGETGRAPHICSRESETSTATUSARBPROC) load(userptr, "glGetGraphicsResetStatusARB");
    glad_glGetnCompressedTexImageARB = (PFNGLGETNCOMPRESSEDTEXIMAGEARBPROC) load(userptr, "glGetnCompressedTexImageARB");
    glad_glGetnTexImageARB = (PFNGLGETNTEXIMAGEARBPROC) load(userptr, "glGetnTexImageARB");
    glad_glGetnUniformdvARB = (PFNGLGETNUNIFORMDVARBPROC) load(userptr, "glGetnUniformdvARB");
    glad_glGetnUniformfvARB = (PFNGLGETNUNIFORMFVARBPROC) load(userptr, "glGetnUniformfvARB");
    glad_glGetnUniformivARB = (PFNGLGETNUNIFORMIVARBPROC) load(userptr, "glGetnUniformivARB");
    glad_glGetnUniformuivARB = (PFNGLGETNUNIFORMUIVARBPROC) load(userptr, "glGetnUniformuivARB");
    glad_glReadnPixelsARB = (PFNGLREADNPIXELSARBPROC) load(userptr, "glReadnPixelsARB");
}
static void glad_gl_load_GL_ARB_shader_storage_buffer_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_shader_storage_buffer_object) return;
    glad_glShaderStorageBlockBinding = (PFNGLSHADERSTORAGEBLOCKBINDINGPROC) load(userptr, "glShaderStorageBlockBinding");
}
static void glad_gl_load_GL_ARB_texture_view( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARB_texture_view) return;
    glad_glTextureView = (PFNGLTEXTUREVIEWPROC) load(userptr, "glTextureView");
}
static void glad_gl_load_GL_KHR_debug( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_KHR_debug) return;
    glad_glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC) load(userptr, "glDebugMessageCallback");
    glad_glDebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC) load(userptr, "glDebugMessageControl");
    glad_glDebugMessageInsert = (PFNGLDEBUGMESSAGEINSERTPROC) load(userptr, "glDebugMessageInsert");
    glad_glGetDebugMessageLog = (PFNGLGETDEBUGMESSAGELOGPROC) load(userptr, "glGetDebugMessageLog");
    glad_glGetObjectLabel = (PFNGLGETOBJECTLABELPROC) load(userptr, "glGetObjectLabel");
    glad_glGetObjectPtrLabel = (PFNGLGETOBJECTPTRLABELPROC) load(userptr, "glGetObjectPtrLabel");
    glad_glGetPointerv = (PFNGLGETPOINTERVPROC) load(userptr, "glGetPointerv");
    glad_glObjectLabel = (PFNGLOBJECTLABELPROC) load(userptr, "glObjectLabel");
    glad_glObjectPtrLabel = (PFNGLOBJECTPTRLABELPROC) load(userptr, "glObjectPtrLabel");
    glad_glPopDebugGroup = (PFNGLPOPDEBUGGROUPPROC) load(userptr, "glPopDebugGroup");
    glad_glPushDebugGroup = (PFNGLPUSHDEBUGGROUPPROC) load(userptr, "glPushDebugGroup");
}



static void glad_gl_free_extensions(char **exts_i) {
    if (exts_i != NULL) {
        unsigned int index;
        for(index = 0; exts_i[index]; index++) {
            free((void *) (exts_i[index]));
        }
        free((void *)exts_i);
        exts_i = NULL;
    }
}
static int glad_gl_get_extensions( const char **out_exts, char ***out_exts_i) {
#if defined(GL_ES_VERSION_3_0) || defined(GL_VERSION_3_0)
    if (glad_glGetStringi != NULL && glad_glGetIntegerv != NULL) {
        unsigned int index = 0;
        unsigned int num_exts_i = 0;
        char **exts_i = NULL;
        glad_glGetIntegerv(GL_NUM_EXTENSIONS, (int*) &num_exts_i);
        exts_i = (char **) malloc((num_exts_i + 1) * (sizeof *exts_i));
        if (exts_i == NULL) {
            return 0;
        }
        for(index = 0; index < num_exts_i; index++) {
            const char *gl_str_tmp = (const char*) glad_glGetStringi(GL_EXTENSIONS, index);
            size_t len = strlen(gl_str_tmp) + 1;

            char *local_str = (char*) malloc(len * sizeof(char));
            if(local_str == NULL) {
                exts_i[index] = NULL;
                glad_gl_free_extensions(exts_i);
                return 0;
            }

            memcpy(local_str, gl_str_tmp, len * sizeof(char));
            exts_i[index] = local_str;
        }
        exts_i[index] = NULL;

        *out_exts_i = exts_i;

        return 1;
    }
#else
    GLAD_UNUSED(out_exts_i);
#endif
    if (glad_glGetString == NULL) {
        return 0;
    }
    *out_exts = (const char *)glad_glGetString(GL_EXTENSIONS);
    return 1;
}
static int glad_gl_has_extension(const char *exts, char **exts_i, const char *ext) {
    if(exts_i) {
        unsigned int index;
        for(index = 0; exts_i[index]; index++) {
            const char *e = exts_i[index];
            if(strcmp(e, ext) == 0) {
                return 1;
            }
        }
    } else {
        const char *extensions;
        const char *loc;
        const char *terminator;
        extensions = exts;
        if(extensions == NULL || ext == NULL) {
            return 0;
        }
        while(1) {
            loc = strstr(extensions, ext);
            if(loc == NULL) {
                return 0;
            }
            terminator = loc + strlen(ext);
            if((loc == extensions || *(loc - 1) == ' ') &&
                (*terminator == ' ' || *terminator == '\0')) {
                return 1;
            }
            extensions = terminator;
        }
    }
    return 0;
}

static GLADapiproc glad_gl_get_proc_from_userptr(void *userptr, const char* name) {
    return (GLAD_GNUC_EXTENSION (GLADapiproc (*)(const char *name)) userptr)(name);
}

static int glad_gl_find_extensions_gl(void) {
    const char *exts = NULL;
    char **exts_i = NULL;
    if (!glad_gl_get_extensions(&exts, &exts_i)) return 0;

    GLAD_GL_ARB_buffer_storage = glad_gl_has_extension(exts, exts_i, "GL_ARB_buffer_storage");
    GLAD_GL_ARB_compute_shader = glad_gl_has_extension(exts, exts_i, "GL_ARB_compute_shader");
    GLAD_GL_ARB_direct_state_access = glad_gl_has_extension(exts, exts_i, "GL_ARB_direct_state_access");
    GLAD_GL_ARB_multi_draw_indirect = glad_gl_has_extension(exts, exts_i, "GL_ARB_multi_draw_indirect");
    GLAD_GL_ARB_robustness = glad_gl_has_extension(exts, exts_i, "GL_ARB_robustness");
    GLAD_GL_ARB_shader_draw_parameters = glad_gl_has_extension(exts, exts_i, "GL_ARB_shader_draw_parameters");
    GLAD_GL_ARB_shader_storage_buffer_object = glad_gl_has_extension(exts, exts_i, "GL_ARB_shader_storage_buffer_object");
    GLAD_GL_ARB_texture_view = glad_gl_has_extension(exts, exts_i, "GL_ARB_texture_view");
    GLAD_GL_KHR_debug = glad_gl_has_extension(exts, exts_i, "GL_KHR_debug");

    glad_gl_free_extensions(exts_i);

    return 1;
}

static int glad_gl_find_core_gl(void) {
    int i;
    const char* version;
    const char* prefixes[] = {
        "OpenGL ES-CM ",
        "OpenGL ES-CL ",
        "OpenGL ES ",
        "OpenGL SC ",
        NULL
    };
    int major = 0;
    int minor = 0;
    version = (const char*) glad_glGetString(GL_VERSION);
    if (!version) return 0;
    for (i = 0;  prefixes[i];  i++) {
        const size_t length = strlen(prefixes[i]);
        if (strncmp(version, prefixes[i], length) == 0) {
            version += length;
            break;
        }
    }

    GLAD_IMPL_UTIL_SSCANF(version, "%d.%d", &major, &minor);

    GLAD_GL_VERSION_1_0 = (major == 1 && minor >= 0) || major > 1;
    GLAD_GL_VERSION_1_1 = (major == 1 && minor >= 1) || major > 1;
    GLAD_GL_VERSION_1_2 = (major == 1 && minor >= 2) || major > 1;
    GLAD_GL_VERSION_1_3 = (major == 1 && minor >= 3) || major > 1;
    GLAD_GL_VERSION_1_4 = (major == 1 && minor >= 4) || major > 1;
    GLAD_GL_VERSION_1_5 = (major == 1 && minor >= 5) || major > 1;
    GLAD_GL_VERSION_2_0 = (major == 2 && minor >= 0) || major > 2;
    GLAD_GL_VERSION_2_1 = (major == 2 && minor >= 1) || major > 2;
    GLAD_GL_VERSION_3_0 = (major == 3 && minor >= 0) || major > 3;
    GLAD_GL_VERSION_3_1 = (major == 3 && minor >= 1) || major > 3;
    GLAD_GL_VERSION_3_2 = (major == 3 && minor >= 2) || major > 3;
    GLAD_GL_VERSION_3_3 = (major == 3 && minor >= 3) || major > 3;
    GLAD_GL_VERSION_4_0 = (major == 4 && minor >= 0) || major > 4;
    GLAD_GL_VERSION_4_1 = (major == 4 && minor >= 1) || major > 4;
    GLAD_GL_VERSION_4_2 = (major == 4 && minor >= 2) || major > 4;
    GLAD_GL_VERSION_4_3 = (major == 4 && minor >= 3) || major > 4;
    GLAD_GL_VERSION_4_4 = (major == 4 && minor >= 4) || major > 4;
    GLAD_GL_VERSION_4_5 = (major == 4 && minor >= 5) || major > 4;
    GLAD_GL_VERSION_4_6 = (major == 4 && minor >= 6) || major > 4;

    return GLAD_MAKE_VERSION(major, minor);
}

int gladLoadGLUserPtr( GLADuserptrloadfunc load, void *userptr) {
    int version;

    gladResetGL();
    glad_glGetString = (PFNGLGETSTRINGPROC) load(userptr, "glGetString");
    if(glad_glGetString == NULL) return 0;
    version = glad_gl_find_core_gl();
    if (!version) {
        gladResetGL();
        return 0;
    }

    glad_gl_load_GL_VERSION_1_0(load, userptr);
    glad_gl_load_GL_VERSION_1_1(load, userptr);
    glad_gl_load_GL_VERSION_1_2(load, userptr);
    glad_gl_load_GL_VERSION_1_3(load, userptr);
    glad_gl_load_GL_VERSION_1_4(load, userptr);
    glad_gl_load_GL_VERSION_1_5(load, userptr);
    glad_gl_load_GL_VERSION_2_0(load, userptr);
    glad_gl_load_GL_VERSION_2_1(load, userptr);
    glad_gl_load_GL_VERSION_3_0(load, userptr);
    glad_gl_load_GL_VERSION_3_1(load, userptr);
    glad_gl_load_GL_VERSION_3_2(load, userptr);
    glad_gl_load_GL_VERSION_3_3(load, userptr);
    glad_gl_load_GL_VERSION_4_0(load, userptr);
    glad_gl_load_GL_VERSION_4_1(load, userptr);
    glad_gl_load_GL_VERSION_4_2(load, userptr);
    glad_gl_load_GL_VERSION_4_3(load, userptr);
    glad_gl_load_GL_VERSION_4_4(load, userptr);
    glad_gl_load_GL_VERSION_4_5(load, userptr);
    glad_gl_load_GL_VERSION_4_6(load, userptr);

    if (!glad_gl_find_extensions_gl()) {
        gladResetGL();
        return 0;
    }
    glad_gl_load_GL_ARB_buffer_storage(load, userptr);
    glad_gl_load_GL_ARB_compute_shader(load, userptr);
    glad_gl_load_GL_ARB_direct_state_access(load, userptr);
    glad_gl_load_GL_ARB_multi_draw_indirect(load, userptr);
    glad_gl_load_GL_ARB_robustness(load, userptr);
    glad_gl_load_GL_ARB_shader_storage_buffer_object(load, userptr);
    glad_gl_load_GL_ARB_texture_view(load, userptr);
    glad_gl_load_GL_KHR_debug(load, userptr);



    return version;
}


int gladLoadGL( GLADloadfunc load) {
    return gladLoadGLUserPtr( glad_gl_get_proc_from_userptr, GLAD_GNUC_EXTENSION (void*) load);
}



 


#ifdef __cplusplus
}
#endif
