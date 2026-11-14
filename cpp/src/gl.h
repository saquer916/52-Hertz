#pragma once
#include <GL/glcorearb.h>

#define GL_FUNCTIONS(X)                                                        \
  X(PFNGLCLEARPROC, glClear)                                                   \
  X(PFNGLCLEARCOLORPROC, glClearColor)                                         \
  X(PFNGLCLEARDEPTHPROC, glClearDepth)                                         \
  X(PFNGLENABLEPROC, glEnable)                                                 \
  X(PFNGLDISABLEPROC, glDisable)                                               \
  X(PFNGLBLENDFUNCPROC, glBlendFunc)                                           \
  X(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate)                           \
  X(PFNGLDEPTHFUNCPROC, glDepthFunc)                                           \
  X(PFNGLDEPTHMASKPROC, glDepthMask)                                           \
  X(PFNGLCOLORMASKPROC, glColorMask)                                           \
  X(PFNGLCULLFACEPROC, glCullFace)                                             \
  X(PFNGLFRONTFACEPROC, glFrontFace)                                           \
  X(PFNGLPOLYGONOFFSETPROC, glPolygonOffset)                                   \
  X(PFNGLVIEWPORTPROC, glViewport)                                             \
  X(PFNGLGETERRORPROC, glGetError)                                             \
  X(PFNGLGETINTEGERVPROC, glGetIntegerv)                                       \
  X(PFNGLGETSTRINGPROC, glGetString)                                           \
  X(PFNGLPIXELSTOREIPROC, glPixelStorei)                                       \
  X(PFNGLREADPIXELSPROC, glReadPixels)                                         \
  X(PFNGLFINISHPROC, glFinish)                                                 \
  X(PFNGLDRAWARRAYSPROC, glDrawArrays)                                         \
  X(PFNGLDRAWELEMENTSPROC, glDrawElements)                                     \
  X(PFNGLDRAWARRAYSINSTANCEDPROC, glDrawArraysInstanced)                       \
  X(PFNGLDRAWELEMENTSINSTANCEDPROC, glDrawElementsInstanced)                   \
  X(PFNGLCLIPCONTROLPROC, glClipControl)                                       \
  X(PFNGLGENBUFFERSPROC, glGenBuffers)                                         \
  X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)                                   \
  X(PFNGLBINDBUFFERPROC, glBindBuffer)                                         \
  X(PFNGLBINDBUFFERBASEPROC, glBindBufferBase)                                 \
  X(PFNGLBUFFERDATAPROC, glBufferData)                                         \
  X(PFNGLBUFFERSUBDATAPROC, glBufferSubData)                                   \
  X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)                               \
  X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)                         \
  X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                               \
  X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)               \
  X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)                       \
  X(PFNGLVERTEXATTRIBIPOINTERPROC, glVertexAttribIPointer)                     \
  X(PFNGLVERTEXATTRIBDIVISORPROC, glVertexAttribDivisor)                       \
  X(PFNGLCREATESHADERPROC, glCreateShader)                                     \
  X(PFNGLSHADERSOURCEPROC, glShaderSource)                                     \
  X(PFNGLCOMPILESHADERPROC, glCompileShader)                                   \
  X(PFNGLGETSHADERIVPROC, glGetShaderiv)                                       \
  X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)                             \
  X(PFNGLDELETESHADERPROC, glDeleteShader)                                     \
  X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                                   \
  X(PFNGLATTACHSHADERPROC, glAttachShader)                                     \
  X(PFNGLLINKPROGRAMPROC, glLinkProgram)                                       \
  X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                                     \
  X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)                           \
  X(PFNGLUSEPROGRAMPROC, glUseProgram)                                         \
  X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)                         \
  X(PFNGLGETUNIFORMBLOCKINDEXPROC, glGetUniformBlockIndex)                     \
  X(PFNGLUNIFORMBLOCKBINDINGPROC, glUniformBlockBinding)                       \
  X(PFNGLUNIFORM1IPROC, glUniform1i)                                           \
  X(PFNGLUNIFORM1FPROC, glUniform1f)                                           \
  X(PFNGLUNIFORM2FPROC, glUniform2f)                                           \
  X(PFNGLUNIFORM3FPROC, glUniform3f)                                           \
  X(PFNGLUNIFORM4FPROC, glUniform4f)                                           \
  X(PFNGLUNIFORM3FVPROC, glUniform3fv)                                         \
  X(PFNGLUNIFORM4FVPROC, glUniform4fv)                                         \
  X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)                             \
  X(PFNGLGENTEXTURESPROC, glGenTextures)                                       \
  X(PFNGLDELETETEXTURESPROC, glDeleteTextures)                                 \
  X(PFNGLBINDTEXTUREPROC, glBindTexture)                                       \
  X(PFNGLACTIVETEXTUREPROC, glActiveTexture)                                   \
  X(PFNGLTEXIMAGE2DPROC, glTexImage2D)                                         \
  X(PFNGLTEXPARAMETERIPROC, glTexParameteri)                                   \
  X(PFNGLTEXPARAMETERFPROC, glTexParameterf)                                   \
  X(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap)                                 \
  X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)                               \
  X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)                         \
  X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)                               \
  X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)                     \
  X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus)                 \
  X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer)                               \
  X(PFNGLREADBUFFERPROC, glReadBuffer)                                         \
  X(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback)

#define GL_DECLARE(type, name) extern type name;
GL_FUNCTIONS(GL_DECLARE)
#undef GL_DECLARE

const char* loadGL();
