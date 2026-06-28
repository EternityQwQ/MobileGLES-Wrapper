// MobileGlues - gl/gl_native.cpp
// Native pass-through for OpenGL ES 3.2 functions.
// These functions exist natively in both OpenGL and OpenGL ES 3.2,
// so they are passed through directly without any CPU emulation.
//
// Functions that require state tracking or format conversion are
// handled in other files (gl.cpp, drawing.cpp, texture.cpp, etc.)
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "../includes.h"
#include <GL/gl.h>
#include "glcorearb.h"
#include "log.h"
#include "../gles/loader.h"
#include "mg.h"
#include <GLES3/gl32.h>

#define DEBUG 0

// ============================================================================
// Buffer Operations
// ============================================================================

// glBindBuffer is handled in buffer.cpp (state tracking)
// glBufferData is handled in buffer.cpp (buffer map management)
// glBufferSubData is handled in buffer.cpp (buffer map management)
// glDeleteBuffers is handled in buffer.cpp (buffer map management)
// glGenBuffers is handled in buffer.cpp (buffer map management)
// glIsBuffer is handled in buffer.cpp (buffer map management)
// glMapBufferRange is handled in buffer.cpp (buffer map management)
// glUnmapBuffer is handled in buffer.cpp (buffer map management)
// glFlushMappedBufferRange is handled in buffer.cpp (buffer map management)
// glBindBufferRange is handled in buffer.cpp (buffer map management)
// glBindBufferBase is handled in buffer.cpp (buffer map management)
// glCopyBufferSubData is handled in buffer.cpp (buffer map management)
// glGetBufferParameteriv is handled in buffer.cpp (buffer map management)
// glGetBufferParameteri64v is handled in buffer.cpp (buffer map management)
// glGetBufferPointerv is handled in buffer.cpp (buffer map management)

// ============================================================================
// Vertex Array Operations
// ============================================================================

// glBindVertexArray is handled in buffer.cpp (VAO map management)
// glDeleteVertexArrays is handled in buffer.cpp (VAO map management)
// glGenVertexArrays is handled in buffer.cpp (VAO map management)
// glIsVertexArray is handled in buffer.cpp (VAO map management)
// glBindVertexBuffer is handled in buffer.cpp (buffer map management)
NATIVE_FUNCTION_HEAD(void, glVertexAttribFormat, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttribFormat, attribindex,size,type,normalized,relativeoffset)
NATIVE_FUNCTION_HEAD(void, glVertexAttribIFormat, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttribIFormat, attribindex,size,type,relativeoffset)
NATIVE_FUNCTION_HEAD(void, glVertexAttribBinding, GLuint attribindex, GLuint bindingindex) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttribBinding, attribindex,bindingindex)
NATIVE_FUNCTION_HEAD(void, glVertexBindingDivisor, GLuint bindingindex, GLuint divisor) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexBindingDivisor, bindingindex,divisor)
NATIVE_FUNCTION_HEAD(void, glVertexAttribDivisor, GLuint index, GLuint divisor) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttribDivisor, index,divisor)
NATIVE_FUNCTION_HEAD(void, glVertexAttribPointer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttribPointer, index,size,type,normalized,stride,pointer)
NATIVE_FUNCTION_HEAD(void, glVertexAttribIPointer, GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttribIPointer, index,size,type,stride,pointer)
NATIVE_FUNCTION_HEAD(void, glEnableVertexAttribArray, GLuint index) NATIVE_FUNCTION_END_NO_RETURN(void, glEnableVertexAttribArray, index)
NATIVE_FUNCTION_HEAD(void, glDisableVertexAttribArray, GLuint index) NATIVE_FUNCTION_END_NO_RETURN(void, glDisableVertexAttribArray, index)
NATIVE_FUNCTION_HEAD(void, glVertexAttrib1f, GLuint index, GLfloat x) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttrib1f, index,x)
NATIVE_FUNCTION_HEAD(void, glVertexAttrib1fv, GLuint index, const GLfloat *v) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttrib1fv, index,v)
NATIVE_FUNCTION_HEAD(void, glVertexAttrib2f, GLuint index, GLfloat x, GLfloat y) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttrib2f, index,x,y)
NATIVE_FUNCTION_HEAD(void, glVertexAttrib2fv, GLuint index, const GLfloat *v) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttrib2fv, index,v)
NATIVE_FUNCTION_HEAD(void, glVertexAttrib3f, GLuint index, GLfloat x, GLfloat y, GLfloat z) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttrib3f, index,x,y,z)
NATIVE_FUNCTION_HEAD(void, glVertexAttrib3fv, GLuint index, const GLfloat *v) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttrib3fv, index,v)
NATIVE_FUNCTION_HEAD(void, glVertexAttrib4f, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttrib4f, index,x,y,z,w)
NATIVE_FUNCTION_HEAD(void, glVertexAttrib4fv, GLuint index, const GLfloat *v) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttrib4fv, index,v)
NATIVE_FUNCTION_HEAD(void, glVertexAttribI4i, GLuint index, GLint x, GLint y, GLint z, GLint w) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttribI4i, index,x,y,z,w)
NATIVE_FUNCTION_HEAD(void, glVertexAttribI4ui, GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttribI4ui, index,x,y,z,w)
NATIVE_FUNCTION_HEAD(void, glVertexAttribI4iv, GLuint index, const GLint *v) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttribI4iv, index,v)
NATIVE_FUNCTION_HEAD(void, glVertexAttribI4uiv, GLuint index, const GLuint *v) NATIVE_FUNCTION_END_NO_RETURN(void, glVertexAttribI4uiv, index,v)
NATIVE_FUNCTION_HEAD(void, glGetVertexAttribfv, GLuint index, GLenum pname, GLfloat *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetVertexAttribfv, index,pname,params)
NATIVE_FUNCTION_HEAD(void, glGetVertexAttribiv, GLuint index, GLenum pname, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetVertexAttribiv, index,pname,params)
NATIVE_FUNCTION_HEAD(void, glGetVertexAttribPointerv, GLuint index, GLenum pname, void **pointer) NATIVE_FUNCTION_END_NO_RETURN(void, glGetVertexAttribPointerv, index,pname,pointer)
NATIVE_FUNCTION_HEAD(void, glGetVertexAttribIiv, GLuint index, GLenum pname, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetVertexAttribIiv, index,pname,params)
NATIVE_FUNCTION_HEAD(void, glGetVertexAttribIuiv, GLuint index, GLenum pname, GLuint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetVertexAttribIuiv, index,pname,params)

// ============================================================================
// Texture Operations
// ============================================================================

// glActiveTexture is handled in texture.cpp (state tracking)
// glBindTexture is handled in texture.cpp (1D texture mapping)
// glTexImage2D is handled in texture.cpp (format conversion)
// glTexImage3D is handled in texture.cpp (format conversion)
// glTexSubImage2D is handled in texture.cpp (format conversion)
// glTexSubImage3D is handled in texture.cpp (format conversion)
// glCopyTexImage2D is handled in texture.cpp (depth blit workaround)
// glCopyTexSubImage2D is handled in texture.cpp (depth blit workaround)
// glCopyTexSubImage3D is handled in texture.cpp (format conversion)
// glCompressedTexImage2D is handled in texture.cpp (format conversion)
// glCompressedTexSubImage2D is handled in texture.cpp (format conversion)
// glCompressedTexImage3D is handled in texture.cpp (format conversion)
// glCompressedTexSubImage3D is handled in texture.cpp (format conversion)
// glTexParameterf is handled in texture.cpp (parameter conversion)
// glTexParameteri is handled in texture.cpp (parameter conversion)
// glTexParameteriv is handled in texture.cpp (parameter conversion)
// glTexParameterfv is handled in texture.cpp (parameter conversion)
NATIVE_FUNCTION_HEAD(void, glTexParameterIiv, GLenum target, GLenum pname, const GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glTexParameterIiv, target,pname,params)
NATIVE_FUNCTION_HEAD(void, glTexParameterIuiv, GLenum target, GLenum pname, const GLuint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glTexParameterIuiv, target,pname,params)
// glGetTexParameterfv is handled in texture.cpp (parameter query)
// glGetTexParameteriv is handled in texture.cpp (parameter query)
NATIVE_FUNCTION_HEAD(void, glGetTexParameterIiv, GLenum target, GLenum pname, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetTexParameterIiv, target,pname,params)
NATIVE_FUNCTION_HEAD(void, glGetTexParameterIuiv, GLenum target, GLenum pname, GLuint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetTexParameterIuiv, target,pname,params)
// glTexStorage2D is handled in texture.cpp (format conversion)
// glTexStorage3D is handled in texture.cpp (format conversion)
NATIVE_FUNCTION_HEAD(void, glTexStorage2DMultisample, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) NATIVE_FUNCTION_END_NO_RETURN(void, glTexStorage2DMultisample, target,samples,internalformat,width,height,fixedsamplelocations)
NATIVE_FUNCTION_HEAD(void, glTexStorage3DMultisample, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) NATIVE_FUNCTION_END_NO_RETURN(void, glTexStorage3DMultisample, target,samples,internalformat,width,height,depth,fixedsamplelocations)
// glTexBuffer is handled in buffer.cpp (texture buffer emulation)
// glTexBufferRange is handled in buffer.cpp (texture buffer emulation)
// glGenerateMipmap is handled in texture.cpp (native pass-through)
NATIVE_FUNCTION_HEAD(void, glGenTextures, GLsizei n, GLuint *textures) NATIVE_FUNCTION_END_NO_RETURN(void, glGenTextures, n,textures)
// glDeleteTextures is handled in texture.cpp (texture map management)
NATIVE_FUNCTION_HEAD(GLboolean, glIsTexture, GLuint texture) NATIVE_FUNCTION_END(GLboolean, glIsTexture, texture)
// glGetTexLevelParameteriv is handled in texture.cpp (proxy texture handling)
// glGetTexLevelParameterfv is handled in texture.cpp (proxy texture handling)
NATIVE_FUNCTION_HEAD(void, glCopyImageSubData, GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth) NATIVE_FUNCTION_END_NO_RETURN(void, glCopyImageSubData, srcName,srcTarget,srcLevel,srcX,srcY,srcZ,dstName,dstTarget,dstLevel,dstX,dstY,dstZ,srcWidth,srcHeight,srcDepth)
NATIVE_FUNCTION_HEAD(void, glGetInternalformativ, GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetInternalformativ, target,internalformat,pname,bufSize,params)

// ============================================================================
// Sampler Operations
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glGenSamplers, GLsizei count, GLuint *samplers) NATIVE_FUNCTION_END_NO_RETURN(void, glGenSamplers, count,samplers)
NATIVE_FUNCTION_HEAD(void, glDeleteSamplers, GLsizei count, const GLuint *samplers) NATIVE_FUNCTION_END_NO_RETURN(void, glDeleteSamplers, count,samplers)
NATIVE_FUNCTION_HEAD(GLboolean, glIsSampler, GLuint sampler) NATIVE_FUNCTION_END(GLboolean, glIsSampler, sampler)
NATIVE_FUNCTION_HEAD(void, glBindSampler, GLuint unit, GLuint sampler) NATIVE_FUNCTION_END_NO_RETURN(void, glBindSampler, unit,sampler)
NATIVE_FUNCTION_HEAD(void, glSamplerParameteri, GLuint sampler, GLenum pname, GLint param) NATIVE_FUNCTION_END_NO_RETURN(void, glSamplerParameteri, sampler,pname,param)
NATIVE_FUNCTION_HEAD(void, glSamplerParameteriv, GLuint sampler, GLenum pname, const GLint *param) NATIVE_FUNCTION_END_NO_RETURN(void, glSamplerParameteriv, sampler,pname,param)
NATIVE_FUNCTION_HEAD(void, glSamplerParameterf, GLuint sampler, GLenum pname, GLfloat param) NATIVE_FUNCTION_END_NO_RETURN(void, glSamplerParameterf, sampler,pname,param)
NATIVE_FUNCTION_HEAD(void, glSamplerParameterfv, GLuint sampler, GLenum pname, const GLfloat *param) NATIVE_FUNCTION_END_NO_RETURN(void, glSamplerParameterfv, sampler,pname,param)
NATIVE_FUNCTION_HEAD(void, glSamplerParameterIiv, GLuint sampler, GLenum pname, const GLint *param) NATIVE_FUNCTION_END_NO_RETURN(void, glSamplerParameterIiv, sampler,pname,param)
NATIVE_FUNCTION_HEAD(void, glSamplerParameterIuiv, GLuint sampler, GLenum pname, const GLuint *param) NATIVE_FUNCTION_END_NO_RETURN(void, glSamplerParameterIuiv, sampler,pname,param)
NATIVE_FUNCTION_HEAD(void, glGetSamplerParameteriv, GLuint sampler, GLenum pname, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetSamplerParameteriv, sampler,pname,params)
NATIVE_FUNCTION_HEAD(void, glGetSamplerParameterfv, GLuint sampler, GLenum pname, GLfloat *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetSamplerParameterfv, sampler,pname,params)
NATIVE_FUNCTION_HEAD(void, glGetSamplerParameterIiv, GLuint sampler, GLenum pname, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetSamplerParameterIiv, sampler,pname,params)
NATIVE_FUNCTION_HEAD(void, glGetSamplerParameterIuiv, GLuint sampler, GLenum pname, GLuint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetSamplerParameterIuiv, sampler,pname,params)

// ============================================================================
// Framebuffer Operations
// ============================================================================

// glBindFramebuffer is handled in framebuffer.cpp (FSR1, state tracking)
// glCheckFramebufferStatus is handled in framebuffer.cpp (ignore error handling)
// glDeleteFramebuffers is handled in framebuffer.cpp (FBO map management)
// glGenFramebuffers is handled in framebuffer.cpp (FBO map management)
NATIVE_FUNCTION_HEAD(GLboolean, glIsFramebuffer, GLuint framebuffer) NATIVE_FUNCTION_END(GLboolean, glIsFramebuffer, framebuffer)
// glFramebufferRenderbuffer is handled in framebuffer.cpp (native)
// glFramebufferTexture2D is handled in framebuffer.cpp (attachment tracking)
// glFramebufferTexture is handled in framebuffer.cpp (attachment tracking)
// glFramebufferTextureLayer is handled in framebuffer.cpp (native)
// glFramebufferParameteri is handled in framebuffer.cpp (native)
// glGetFramebufferAttachmentParameteriv is handled in framebuffer.cpp (native)
// glGetFramebufferParameteriv is handled in framebuffer.cpp (native)
// glBlitFramebuffer is handled in framebuffer.cpp (native)
// glInvalidateFramebuffer is handled in framebuffer.cpp (native)
// glInvalidateSubFramebuffer is handled in framebuffer.cpp (native)

// ============================================================================
// Renderbuffer Operations
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glBindRenderbuffer, GLenum target, GLuint renderbuffer) NATIVE_FUNCTION_END_NO_RETURN(void, glBindRenderbuffer, target,renderbuffer)
// glRenderbufferStorage is handled in texture.cpp (format conversion)
// glRenderbufferStorageMultisample is handled in texture.cpp (format conversion)
NATIVE_FUNCTION_HEAD(void, glDeleteRenderbuffers, GLsizei n, const GLuint *renderbuffers) NATIVE_FUNCTION_END_NO_RETURN(void, glDeleteRenderbuffers, n,renderbuffers)
NATIVE_FUNCTION_HEAD(void, glGenRenderbuffers, GLsizei n, GLuint *renderbuffers) NATIVE_FUNCTION_END_NO_RETURN(void, glGenRenderbuffers, n,renderbuffers)
NATIVE_FUNCTION_HEAD(GLboolean, glIsRenderbuffer, GLuint renderbuffer) NATIVE_FUNCTION_END(GLboolean, glIsRenderbuffer, renderbuffer)
NATIVE_FUNCTION_HEAD(void, glGetRenderbufferParameteriv, GLenum target, GLenum pname, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetRenderbufferParameteriv, target,pname,params)

// ============================================================================
// Shader Operations
// ============================================================================

// glCreateShader is handled in both shader.cpp and program.cpp (GLSL conversion, state tracking)
// glShaderSource is handled in shader.cpp (GLSL-to-ESSL conversion)
// glCompileShader is handled in both shader.cpp and program.cpp (GLSL conversion, error logging)
// glDeleteShader is handled in program.cpp (state tracking)
NATIVE_FUNCTION_HEAD(GLboolean, glIsShader, GLuint shader) NATIVE_FUNCTION_END(GLboolean, glIsShader, shader)
// glGetShaderiv is handled in program.cpp (state tracking)
// glGetShaderInfoLog is handled in program.cpp (state tracking)
NATIVE_FUNCTION_HEAD(void, glGetShaderSource, GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source) NATIVE_FUNCTION_END_NO_RETURN(void, glGetShaderSource, shader,bufSize,length,source)
NATIVE_FUNCTION_HEAD(void, glGetShaderPrecisionFormat, GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision) NATIVE_FUNCTION_END_NO_RETURN(void, glGetShaderPrecisionFormat, shadertype,precisiontype,range,precision)
NATIVE_FUNCTION_HEAD(void, glReleaseShaderCompiler) NATIVE_FUNCTION_END_NO_RETURN(void, glReleaseShaderCompiler)
NATIVE_FUNCTION_HEAD(void, glShaderBinary, GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length) NATIVE_FUNCTION_END_NO_RETURN(void, glShaderBinary, count,shaders,binaryformat,binary,length)

// ============================================================================
// Program Operations
// ============================================================================

// glCreateProgram is handled in program.cpp (state tracking)
// glAttachShader is handled in program.cpp (state tracking)
// glDetachShader is handled in program.cpp (state tracking)
// glBindAttribLocation is handled in program.cpp (state tracking)
// glLinkProgram is handled in program.cpp (error logging)
// glUseProgram is handled in program.cpp (state tracking)
// glValidateProgram is handled in program.cpp (error logging)
// glDeleteProgram is handled in program.cpp (state tracking)
NATIVE_FUNCTION_HEAD(GLboolean, glIsProgram, GLuint program) NATIVE_FUNCTION_END(GLboolean, glIsProgram, program)
// glGetProgramiv is handled in program.cpp (state tracking)
// glGetProgramInfoLog is handled in program.cpp (error logging)
// glGetAttachedShaders is handled in program.cpp (state tracking)
// glGetActiveAttrib is handled in program.cpp (state tracking)
// glGetActiveUniform is handled in program.cpp (state tracking)
// glGetAttribLocation is handled in program.cpp (state tracking)
// glGetUniformLocation is handled in program.cpp (state tracking)
// glUniform1i is a native ES 3.2 pass-through
NATIVE_FUNCTION_HEAD(void, glUniform1i, GLint location, GLint v0) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform1i, location,v0)
NATIVE_FUNCTION_HEAD(void, glUniform1f, GLint location, GLfloat v0) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform1f, location,v0)
NATIVE_FUNCTION_HEAD(void, glUniform1fv, GLint location, GLsizei count, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform1fv, location,count,value)
NATIVE_FUNCTION_HEAD(void, glUniform1iv, GLint location, GLsizei count, const GLint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform1iv, location,count,value)
NATIVE_FUNCTION_HEAD(void, glUniform2f, GLint location, GLfloat v0, GLfloat v1) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform2f, location,v0,v1)
NATIVE_FUNCTION_HEAD(void, glUniform2fv, GLint location, GLsizei count, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform2fv, location,count,value)
NATIVE_FUNCTION_HEAD(void, glUniform2i, GLint location, GLint v0, GLint v1) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform2i, location,v0,v1)
NATIVE_FUNCTION_HEAD(void, glUniform2iv, GLint location, GLsizei count, const GLint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform2iv, location,count,value)
NATIVE_FUNCTION_HEAD(void, glUniform3f, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform3f, location,v0,v1,v2)
NATIVE_FUNCTION_HEAD(void, glUniform3fv, GLint location, GLsizei count, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform3fv, location,count,value)
NATIVE_FUNCTION_HEAD(void, glUniform3i, GLint location, GLint v0, GLint v1, GLint v2) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform3i, location,v0,v1,v2)
NATIVE_FUNCTION_HEAD(void, glUniform3iv, GLint location, GLsizei count, const GLint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform3iv, location,count,value)
NATIVE_FUNCTION_HEAD(void, glUniform4f, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform4f, location,v0,v1,v2,v3)
NATIVE_FUNCTION_HEAD(void, glUniform4fv, GLint location, GLsizei count, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform4fv, location,count,value)
NATIVE_FUNCTION_HEAD(void, glUniform4i, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform4i, location,v0,v1,v2,v3)
NATIVE_FUNCTION_HEAD(void, glUniform4iv, GLint location, GLsizei count, const GLint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform4iv, location,count,value)
NATIVE_FUNCTION_HEAD(void, glUniform1ui, GLint location, GLuint v0) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform1ui, location,v0)
NATIVE_FUNCTION_HEAD(void, glUniform2ui, GLint location, GLuint v0, GLuint v1) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform2ui, location,v0,v1)
NATIVE_FUNCTION_HEAD(void, glUniform3ui, GLint location, GLuint v0, GLuint v1, GLuint v2) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform3ui, location,v0,v1,v2)
NATIVE_FUNCTION_HEAD(void, glUniform4ui, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform4ui, location,v0,v1,v2,v3)
NATIVE_FUNCTION_HEAD(void, glUniform1uiv, GLint location, GLsizei count, const GLuint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform1uiv, location,count,value)
NATIVE_FUNCTION_HEAD(void, glUniform2uiv, GLint location, GLsizei count, const GLuint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform2uiv, location,count,value)
NATIVE_FUNCTION_HEAD(void, glUniform3uiv, GLint location, GLsizei count, const GLuint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform3uiv, location,count,value)
NATIVE_FUNCTION_HEAD(void, glUniform4uiv, GLint location, GLsizei count, const GLuint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniform4uiv, location,count,value)
NATIVE_FUNCTION_HEAD(void, glUniformMatrix2fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniformMatrix2fv, location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glUniformMatrix3fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniformMatrix3fv, location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glUniformMatrix4fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniformMatrix4fv, location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glUniformMatrix2x3fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniformMatrix2x3fv, location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glUniformMatrix3x2fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniformMatrix3x2fv, location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glUniformMatrix2x4fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniformMatrix2x4fv, location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glUniformMatrix4x2fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniformMatrix4x2fv, location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glUniformMatrix3x4fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniformMatrix3x4fv, location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glUniformMatrix4x3fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glUniformMatrix4x3fv, location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glGetUniformfv, GLuint program, GLint location, GLfloat *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetUniformfv, program,location,params)
NATIVE_FUNCTION_HEAD(void, glGetUniformiv, GLuint program, GLint location, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetUniformiv, program,location,params)
NATIVE_FUNCTION_HEAD(void, glGetUniformuiv, GLuint program, GLint location, GLuint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetUniformuiv, program,location,params)
NATIVE_FUNCTION_HEAD(void, glGetUniformIndices, GLuint program, GLsizei uniformCount, const GLchar *const*uniformNames, GLuint *uniformIndices) NATIVE_FUNCTION_END_NO_RETURN(void, glGetUniformIndices, program,uniformCount,uniformNames,uniformIndices)
NATIVE_FUNCTION_HEAD(void, glGetActiveUniformsiv, GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetActiveUniformsiv, program,uniformCount,uniformIndices,pname,params)
NATIVE_FUNCTION_HEAD(GLuint, glGetUniformBlockIndex, GLuint program, const GLchar *uniformBlockName) NATIVE_FUNCTION_END(GLuint, glGetUniformBlockIndex, program,uniformBlockName)
NATIVE_FUNCTION_HEAD(void, glGetActiveUniformBlockiv, GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetActiveUniformBlockiv, program,uniformBlockIndex,pname,params)
NATIVE_FUNCTION_HEAD(void, glGetActiveUniformBlockName, GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName) NATIVE_FUNCTION_END_NO_RETURN(void, glGetActiveUniformBlockName, program,uniformBlockIndex,bufSize,length,uniformBlockName)
NATIVE_FUNCTION_HEAD(void, glUniformBlockBinding, GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) NATIVE_FUNCTION_END_NO_RETURN(void, glUniformBlockBinding, program,uniformBlockIndex,uniformBlockBinding)
NATIVE_FUNCTION_HEAD(GLint, glGetFragDataLocation, GLuint program, const GLchar *name) NATIVE_FUNCTION_END(GLint, glGetFragDataLocation, program,name)
NATIVE_FUNCTION_HEAD(void, glGetProgramBinary, GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary) NATIVE_FUNCTION_END_NO_RETURN(void, glGetProgramBinary, program,bufSize,length,binaryFormat,binary)
NATIVE_FUNCTION_HEAD(void, glProgramBinary, GLuint program, GLenum binaryFormat, const void *binary, GLsizei length) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramBinary, program,binaryFormat,binary,length)
NATIVE_FUNCTION_HEAD(void, glProgramParameteri, GLuint program, GLenum pname, GLint value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramParameteri, program,pname,value)
NATIVE_FUNCTION_HEAD(void, glGetProgramInterfaceiv, GLuint program, GLenum programInterface, GLenum pname, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetProgramInterfaceiv, program,programInterface,pname,params)
NATIVE_FUNCTION_HEAD(GLuint, glGetProgramResourceIndex, GLuint program, GLenum programInterface, const GLchar *name) NATIVE_FUNCTION_END(GLuint, glGetProgramResourceIndex, program,programInterface,name)
NATIVE_FUNCTION_HEAD(void, glGetProgramResourceName, GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name) NATIVE_FUNCTION_END_NO_RETURN(void, glGetProgramResourceName, program,programInterface,index,bufSize,length,name)
NATIVE_FUNCTION_HEAD(void, glGetProgramResourceiv, GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetProgramResourceiv, program,programInterface,index,propCount,props,bufSize,length,params)
NATIVE_FUNCTION_HEAD(GLint, glGetProgramResourceLocation, GLuint program, GLenum programInterface, const GLchar *name) NATIVE_FUNCTION_END(GLint, glGetProgramResourceLocation, program,programInterface,name)

// ============================================================================
// Program Pipeline Operations
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glUseProgramStages, GLuint pipeline, GLbitfield stages, GLuint program) NATIVE_FUNCTION_END_NO_RETURN(void, glUseProgramStages, pipeline,stages,program)
NATIVE_FUNCTION_HEAD(void, glActiveShaderProgram, GLuint pipeline, GLuint program) NATIVE_FUNCTION_END_NO_RETURN(void, glActiveShaderProgram, pipeline,program)
NATIVE_FUNCTION_HEAD(GLuint, glCreateShaderProgramv, GLenum type, GLsizei count, const GLchar *const*strings) NATIVE_FUNCTION_END(GLuint, glCreateShaderProgramv, type,count,strings)
NATIVE_FUNCTION_HEAD(void, glBindProgramPipeline, GLuint pipeline) NATIVE_FUNCTION_END_NO_RETURN(void, glBindProgramPipeline, pipeline)
NATIVE_FUNCTION_HEAD(void, glDeleteProgramPipelines, GLsizei n, const GLuint *pipelines) NATIVE_FUNCTION_END_NO_RETURN(void, glDeleteProgramPipelines, n,pipelines)
NATIVE_FUNCTION_HEAD(void, glGenProgramPipelines, GLsizei n, GLuint *pipelines) NATIVE_FUNCTION_END_NO_RETURN(void, glGenProgramPipelines, n,pipelines)
NATIVE_FUNCTION_HEAD(GLboolean, glIsProgramPipeline, GLuint pipeline) NATIVE_FUNCTION_END(GLboolean, glIsProgramPipeline, pipeline)
NATIVE_FUNCTION_HEAD(void, glGetProgramPipelineiv, GLuint pipeline, GLenum pname, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetProgramPipelineiv, pipeline,pname,params)
NATIVE_FUNCTION_HEAD(void, glValidateProgramPipeline, GLuint pipeline) NATIVE_FUNCTION_END_NO_RETURN(void, glValidateProgramPipeline, pipeline)
NATIVE_FUNCTION_HEAD(void, glGetProgramPipelineInfoLog, GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog) NATIVE_FUNCTION_END_NO_RETURN(void, glGetProgramPipelineInfoLog, pipeline,bufSize,length,infoLog)
NATIVE_FUNCTION_HEAD(void, glProgramUniform1i, GLuint program, GLint location, GLint v0) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform1i, program,location,v0)
NATIVE_FUNCTION_HEAD(void, glProgramUniform2i, GLuint program, GLint location, GLint v0, GLint v1) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform2i, program,location,v0,v1)
NATIVE_FUNCTION_HEAD(void, glProgramUniform3i, GLuint program, GLint location, GLint v0, GLint v1, GLint v2) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform3i, program,location,v0,v1,v2)
NATIVE_FUNCTION_HEAD(void, glProgramUniform4i, GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform4i, program,location,v0,v1,v2,v3)
NATIVE_FUNCTION_HEAD(void, glProgramUniform1ui, GLuint program, GLint location, GLuint v0) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform1ui, program,location,v0)
NATIVE_FUNCTION_HEAD(void, glProgramUniform2ui, GLuint program, GLint location, GLuint v0, GLuint v1) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform2ui, program,location,v0,v1)
NATIVE_FUNCTION_HEAD(void, glProgramUniform3ui, GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform3ui, program,location,v0,v1,v2)
NATIVE_FUNCTION_HEAD(void, glProgramUniform4ui, GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform4ui, program,location,v0,v1,v2,v3)
NATIVE_FUNCTION_HEAD(void, glProgramUniform1f, GLuint program, GLint location, GLfloat v0) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform1f, program,location,v0)
NATIVE_FUNCTION_HEAD(void, glProgramUniform2f, GLuint program, GLint location, GLfloat v0, GLfloat v1) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform2f, program,location,v0,v1)
NATIVE_FUNCTION_HEAD(void, glProgramUniform3f, GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform3f, program,location,v0,v1,v2)
NATIVE_FUNCTION_HEAD(void, glProgramUniform4f, GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform4f, program,location,v0,v1,v2,v3)
NATIVE_FUNCTION_HEAD(void, glProgramUniform1iv, GLuint program, GLint location, GLsizei count, const GLint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform1iv, program,location,count,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniform2iv, GLuint program, GLint location, GLsizei count, const GLint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform2iv, program,location,count,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniform3iv, GLuint program, GLint location, GLsizei count, const GLint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform3iv, program,location,count,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniform4iv, GLuint program, GLint location, GLsizei count, const GLint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform4iv, program,location,count,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniform1uiv, GLuint program, GLint location, GLsizei count, const GLuint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform1uiv, program,location,count,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniform2uiv, GLuint program, GLint location, GLsizei count, const GLuint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform2uiv, program,location,count,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniform3uiv, GLuint program, GLint location, GLsizei count, const GLuint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform3uiv, program,location,count,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniform4uiv, GLuint program, GLint location, GLsizei count, const GLuint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform4uiv, program,location,count,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniform1fv, GLuint program, GLint location, GLsizei count, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform1fv, program,location,count,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniform2fv, GLuint program, GLint location, GLsizei count, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform2fv, program,location,count,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniform3fv, GLuint program, GLint location, GLsizei count, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform3fv, program,location,count,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniform4fv, GLuint program, GLint location, GLsizei count, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniform4fv, program,location,count,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniformMatrix2fv, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniformMatrix2fv, program,location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniformMatrix3fv, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniformMatrix3fv, program,location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniformMatrix4fv, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniformMatrix4fv, program,location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniformMatrix2x3fv, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniformMatrix2x3fv, program,location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniformMatrix3x2fv, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniformMatrix3x2fv, program,location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniformMatrix2x4fv, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniformMatrix2x4fv, program,location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniformMatrix4x2fv, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniformMatrix4x2fv, program,location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniformMatrix3x4fv, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniformMatrix3x4fv, program,location,count,transpose,value)
NATIVE_FUNCTION_HEAD(void, glProgramUniformMatrix4x3fv, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glProgramUniformMatrix4x3fv, program,location,count,transpose,value)

// ============================================================================
// State Management
// ============================================================================

// glEnable/glDisable: track state CPU-side for fast glIsEnabled, then forward to GLES
extern "C" void glEnable(GLenum cap) {
    gl_state_track_enable(cap);
    GLES.glEnable(cap);
}
extern "C" void glDisable(GLenum cap) {
    gl_state_track_disable(cap);
    GLES.glDisable(cap);
}
// glIsEnabled: check CPU-side cache first, avoid GPU round-trip
extern "C" GLboolean glIsEnabled(GLenum cap) {
    return gl_state_is_enabled(cap);
}
// glEnablei/glDisablei: indexed state — pass through to GLES (less common, not worth tracking)
extern "C" void glEnablei(GLenum target, GLuint index) {
    GLES.glEnablei(target, index);
}
extern "C" void glDisablei(GLenum target, GLuint index) {
    GLES.glDisablei(target, index);
}
extern "C" GLboolean glIsEnabledi(GLenum target, GLuint index) {
    return GLES.glIsEnabledi(target, index);
}
// glGetError is handled in getter.cpp (always returns GL_NO_ERROR)
// glGetIntegerv is handled in getter.cpp (custom getter)
NATIVE_FUNCTION_HEAD(void, glGetBooleanv, GLenum pname, GLboolean *data) NATIVE_FUNCTION_END_NO_RETURN(void, glGetBooleanv, pname,data)
NATIVE_FUNCTION_HEAD(void, glGetFloatv, GLenum pname, GLfloat *data) NATIVE_FUNCTION_END_NO_RETURN(void, glGetFloatv, pname,data)
NATIVE_FUNCTION_HEAD(void, glGetInteger64v, GLenum pname, GLint64 *data) NATIVE_FUNCTION_END_NO_RETURN(void, glGetInteger64v, pname,data)
NATIVE_FUNCTION_HEAD(void, glGetIntegeri_v, GLenum target, GLuint index, GLint *data) NATIVE_FUNCTION_END_NO_RETURN(void, glGetIntegeri_v, target,index,data)
NATIVE_FUNCTION_HEAD(void, glGetInteger64i_v, GLenum target, GLuint index, GLint64 *data) NATIVE_FUNCTION_END_NO_RETURN(void, glGetInteger64i_v, target,index,data)
NATIVE_FUNCTION_HEAD(void, glGetBooleani_v, GLenum target, GLuint index, GLboolean *data) NATIVE_FUNCTION_END_NO_RETURN(void, glGetBooleani_v, target,index,data)
NATIVE_FUNCTION_HEAD(void, glGetPointerv, GLenum pname, void **params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetPointerv, pname,params)
NATIVE_FUNCTION_HEAD(GLenum, glGetGraphicsResetStatus) NATIVE_FUNCTION_END(GLenum, glGetGraphicsResetStatus)

// ============================================================================
// Drawing Operations
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glClearColor, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) NATIVE_FUNCTION_END_NO_RETURN(void, glClearColor, red,green,blue,alpha)
NATIVE_FUNCTION_HEAD(void, glClearDepthf, GLfloat d) NATIVE_FUNCTION_END_NO_RETURN(void, glClearDepthf, d)
NATIVE_FUNCTION_HEAD(void, glClearStencil, GLint s) NATIVE_FUNCTION_END_NO_RETURN(void, glClearStencil, s)
NATIVE_FUNCTION_HEAD(void, glClearBufferiv, GLenum buffer, GLint drawbuffer, const GLint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glClearBufferiv, buffer,drawbuffer,value)
NATIVE_FUNCTION_HEAD(void, glClearBufferuiv, GLenum buffer, GLint drawbuffer, const GLuint *value) NATIVE_FUNCTION_END_NO_RETURN(void, glClearBufferuiv, buffer,drawbuffer,value)
NATIVE_FUNCTION_HEAD(void, glClearBufferfv, GLenum buffer, GLint drawbuffer, const GLfloat *value) NATIVE_FUNCTION_END_NO_RETURN(void, glClearBufferfv, buffer,drawbuffer,value)
NATIVE_FUNCTION_HEAD(void, glClearBufferfi, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) NATIVE_FUNCTION_END_NO_RETURN(void, glClearBufferfi, buffer,drawbuffer,depth,stencil)
// glDrawArrays is handled in drawing.cpp (state tracking)
// glDrawElements is handled in drawing.cpp (state tracking)
// glDrawElementsInstanced is handled in drawing.cpp (state tracking)
// glDrawArraysInstanced is handled in drawing.cpp (state tracking)
// glDrawRangeElements is handled in drawing.cpp (state tracking)
// glDrawElementsBaseVertex is handled in drawing.cpp (state tracking)
NATIVE_FUNCTION_HEAD(void, glDrawRangeElementsBaseVertex, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex) NATIVE_FUNCTION_END_NO_RETURN(void, glDrawRangeElementsBaseVertex, mode,start,end,count,type,indices,basevertex)
NATIVE_FUNCTION_HEAD(void, glDrawElementsInstancedBaseVertex, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex) NATIVE_FUNCTION_END_NO_RETURN(void, glDrawElementsInstancedBaseVertex, mode,count,type,indices,instancecount,basevertex)
// glDrawArraysIndirect is handled in drawing.cpp (state tracking)
// glDrawElementsIndirect is handled in drawing.cpp (state tracking)
// glDispatchCompute is handled in drawing.cpp (atomic counter emulation)
NATIVE_FUNCTION_HEAD(void, glDispatchComputeIndirect, GLintptr indirect) NATIVE_FUNCTION_END_NO_RETURN(void, glDispatchComputeIndirect, indirect)
// glDrawBuffers is handled in framebuffer.cpp (attachment remapping)

// ============================================================================
// Blending Operations
// ============================================================================

// --- Blend state (redundant-call prevention) ---
extern "C" void glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    if (gl_state->blend_color[0] == red && gl_state->blend_color[1] == green &&
        gl_state->blend_color[2] == blue && gl_state->blend_color[3] == alpha) [[likely]] { return; }
    set_gl_state_blend_color(red, green, blue, alpha);
    GLES.glBlendColor(red, green, blue, alpha);
}
extern "C" void glBlendEquation(GLenum mode) {
    if (gl_state->blend_equation_rgb == mode) [[likely]] { return; }
    set_gl_state_blend_equation_rgb(mode);
    set_gl_state_blend_equation_alpha(mode);
    GLES.glBlendEquation(mode);
}
extern "C" void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    if (gl_state->blend_equation_rgb == modeRGB && gl_state->blend_equation_alpha == modeAlpha) [[likely]] { return; }
    set_gl_state_blend_equation_rgb(modeRGB);
    set_gl_state_blend_equation_alpha(modeAlpha);
    GLES.glBlendEquationSeparate(modeRGB, modeAlpha);
}
extern "C" void glBlendFunc(GLenum sfactor, GLenum dfactor) {
    if (gl_state->blend_src_rgb == sfactor && gl_state->blend_dst_rgb == dfactor) [[likely]] { return; }
    set_gl_state_blend_src_rgb(sfactor);
    set_gl_state_blend_dst_rgb(dfactor);
    set_gl_state_blend_src_alpha(sfactor);
    set_gl_state_blend_dst_alpha(dfactor);
    GLES.glBlendFunc(sfactor, dfactor);
}
extern "C" void glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    if (gl_state->blend_src_rgb == sfactorRGB && gl_state->blend_dst_rgb == dfactorRGB &&
        gl_state->blend_src_alpha == sfactorAlpha && gl_state->blend_dst_alpha == dfactorAlpha) [[likely]] { return; }
    set_gl_state_blend_src_rgb(sfactorRGB);
    set_gl_state_blend_dst_rgb(dfactorRGB);
    set_gl_state_blend_src_alpha(sfactorAlpha);
    set_gl_state_blend_dst_alpha(dfactorAlpha);
    GLES.glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}
// Per-buffer blend: less common, pass through
extern "C" void glBlendEquationi(GLuint buf, GLenum mode) { GLES.glBlendEquationi(buf, mode); }
extern "C" void glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha) { GLES.glBlendEquationSeparatei(buf, modeRGB, modeAlpha); }
extern "C" void glBlendFunci(GLuint buf, GLenum src, GLenum dst) { GLES.glBlendFunci(buf, src, dst); }
extern "C" void glBlendFuncSeparatei(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) { GLES.glBlendFuncSeparatei(buf, srcRGB, dstRGB, srcAlpha, dstAlpha); }
extern "C" void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) { GLES.glColorMask(red, green, blue, alpha); }
extern "C" void glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) { GLES.glColorMaski(index, r, g, b, a); }
extern "C" void glBlendBarrier() { GLES.glBlendBarrier(); }

// ============================================================================
// Read-back Operations
// ============================================================================

// glReadPixels is handled in texture.cpp (BGRA format conversion)
NATIVE_FUNCTION_HEAD(void, glReadnPixels, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data) NATIVE_FUNCTION_END_NO_RETURN(void, glReadnPixels, x,y,width,height,format,type,bufSize,data)
// glReadBuffer is handled in framebuffer.cpp (attachment remapping)

// ============================================================================
// Viewport, Scissor, and Miscellaneous
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glScissor, GLint x, GLint y, GLsizei width, GLsizei height) NATIVE_FUNCTION_END_NO_RETURN(void, glScissor, x,y,width,height)
NATIVE_FUNCTION_HEAD(void, glLineWidth, GLfloat width) NATIVE_FUNCTION_END_NO_RETURN(void, glLineWidth, width)
NATIVE_FUNCTION_HEAD(void, glPolygonOffset, GLfloat factor, GLfloat units) NATIVE_FUNCTION_END_NO_RETURN(void, glPolygonOffset, factor,units)
NATIVE_FUNCTION_HEAD(void, glSampleCoverage, GLfloat value, GLboolean invert) NATIVE_FUNCTION_END_NO_RETURN(void, glSampleCoverage, value,invert)
NATIVE_FUNCTION_HEAD(void, glSampleMaski, GLuint maskNumber, GLbitfield mask) NATIVE_FUNCTION_END_NO_RETURN(void, glSampleMaski, maskNumber,mask)
NATIVE_FUNCTION_HEAD(void, glMinSampleShading, GLfloat value) NATIVE_FUNCTION_END_NO_RETURN(void, glMinSampleShading, value)
NATIVE_FUNCTION_HEAD(void, glGetMultisamplefv, GLenum pname, GLuint index, GLfloat *val) NATIVE_FUNCTION_END_NO_RETURN(void, glGetMultisamplefv, pname,index,val)
NATIVE_FUNCTION_HEAD(void, glPrimitiveBoundingBox, GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW) NATIVE_FUNCTION_END_NO_RETURN(void, glPrimitiveBoundingBox, minX,minY,minZ,minW,maxX,maxY,maxZ,maxW)
NATIVE_FUNCTION_HEAD(void, glPatchParameteri, GLenum pname, GLint value) NATIVE_FUNCTION_END_NO_RETURN(void, glPatchParameteri, pname,value)

// ============================================================================
// Stencil and Depth Operations
// ============================================================================

// --- Depth state (redundant-call prevention) ---
extern "C" void glDepthFunc(GLenum func) {
    if (gl_state->depth_func == func) [[likely]] { return; }
    set_gl_state_depth_func(func);
    GLES.glDepthFunc(func);
}
extern "C" void glDepthMask(GLboolean flag) {
    if (gl_state->depth_mask == flag) [[likely]] { return; }
    set_gl_state_depth_mask(flag);
    GLES.glDepthMask(flag);
}
extern "C" void glDepthRangef(GLfloat n, GLfloat f) { GLES.glDepthRangef(n, f); }

// --- Stencil state (redundant-call prevention) ---
extern "C" void glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    if (gl_state->stencil_func_front == func && gl_state->stencil_func_back == func) [[likely]] { return; }
    set_gl_state_stencil_func_front(func);
    set_gl_state_stencil_func_back(func);
    GLES.glStencilFunc(func, ref, mask);
}
extern "C" void glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    bool needs_call = false;
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
        if (gl_state->stencil_func_front != func) { set_gl_state_stencil_func_front(func); needs_call = true; }
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
        if (gl_state->stencil_func_back != func) { set_gl_state_stencil_func_back(func); needs_call = true; }
    }
    if (!needs_call) [[likely]] return;
    GLES.glStencilFuncSeparate(face, func, ref, mask);
}
extern "C" void glStencilMask(GLuint mask) { GLES.glStencilMask(mask); }
extern "C" void glStencilMaskSeparate(GLenum face, GLuint mask) { GLES.glStencilMaskSeparate(face, mask); }
extern "C" void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    if (gl_state->stencil_fail_front == fail && gl_state->stencil_zfail_front == zfail &&
        gl_state->stencil_zpass_front == zpass && gl_state->stencil_fail_back == fail &&
        gl_state->stencil_zfail_back == zfail && gl_state->stencil_zpass_back == zpass) [[likely]] { return; }
    set_gl_state_stencil_fail_front(fail);
    set_gl_state_stencil_zfail_front(zfail);
    set_gl_state_stencil_zpass_front(zpass);
    set_gl_state_stencil_fail_back(fail);
    set_gl_state_stencil_zfail_back(zfail);
    set_gl_state_stencil_zpass_back(zpass);
    GLES.glStencilOp(fail, zfail, zpass);
}
extern "C" void glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    bool needs_call = false;
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
        if (gl_state->stencil_fail_front != sfail || gl_state->stencil_zfail_front != dpfail ||
            gl_state->stencil_zpass_front != dppass) {
            set_gl_state_stencil_fail_front(sfail);
            set_gl_state_stencil_zfail_front(dpfail);
            set_gl_state_stencil_zpass_front(dppass);
            needs_call = true;
        }
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
        if (gl_state->stencil_fail_back != sfail || gl_state->stencil_zfail_back != dpfail ||
            gl_state->stencil_zpass_back != dppass) {
            set_gl_state_stencil_fail_back(sfail);
            set_gl_state_stencil_zfail_back(dpfail);
            set_gl_state_stencil_zpass_back(dppass);
            needs_call = true;
        }
    }
    if (!needs_call) [[likely]] return;
    GLES.glStencilOpSeparate(face, sfail, dpfail, dppass);
}

// ============================================================================
// Culling and Face Operations
// ============================================================================

// --- Cull face state (redundant-call prevention) ---
extern "C" void glCullFace(GLenum mode) {
    if (gl_state->cull_face_mode == mode) [[likely]] { return; }
    set_gl_state_cull_face_mode(mode);
    GLES.glCullFace(mode);
}
extern "C" void glFrontFace(GLenum mode) {
    if (gl_state->front_face == mode) [[likely]] { return; }
    set_gl_state_front_face(mode);
    GLES.glFrontFace(mode);
}

// ============================================================================
// Finish and Flush
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glFinish) NATIVE_FUNCTION_END_NO_RETURN(void, glFinish)
NATIVE_FUNCTION_HEAD(void, glFlush) NATIVE_FUNCTION_END_NO_RETURN(void, glFlush)

// ============================================================================
// Pixel Storage
// ============================================================================

// glPixelStorei is handled in texture.cpp

// ============================================================================
// Synchronization
// ============================================================================

NATIVE_FUNCTION_HEAD(GLsync, glFenceSync, GLenum condition, GLbitfield flags) NATIVE_FUNCTION_END(GLsync, glFenceSync, condition,flags)
NATIVE_FUNCTION_HEAD(GLboolean, glIsSync, GLsync sync) NATIVE_FUNCTION_END(GLboolean, glIsSync, sync)
NATIVE_FUNCTION_HEAD(void, glDeleteSync, GLsync sync) NATIVE_FUNCTION_END_NO_RETURN(void, glDeleteSync, sync)
NATIVE_FUNCTION_HEAD(GLenum, glClientWaitSync, GLsync sync, GLbitfield flags, GLuint64 timeout) NATIVE_FUNCTION_END(GLenum, glClientWaitSync, sync,flags,timeout)
NATIVE_FUNCTION_HEAD(void, glWaitSync, GLsync sync, GLbitfield flags, GLuint64 timeout) NATIVE_FUNCTION_END_NO_RETURN(void, glWaitSync, sync,flags,timeout)
NATIVE_FUNCTION_HEAD(void, glGetSynciv, GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values) NATIVE_FUNCTION_END_NO_RETURN(void, glGetSynciv, sync,pname,bufSize,length,values)

// ============================================================================
// Query Operations
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glGenQueries, GLsizei n, GLuint *ids) NATIVE_FUNCTION_END_NO_RETURN(void, glGenQueries, n,ids)
NATIVE_FUNCTION_HEAD(void, glDeleteQueries, GLsizei n, const GLuint *ids) NATIVE_FUNCTION_END_NO_RETURN(void, glDeleteQueries, n,ids)
NATIVE_FUNCTION_HEAD(GLboolean, glIsQuery, GLuint id) NATIVE_FUNCTION_END(GLboolean, glIsQuery, id)
NATIVE_FUNCTION_HEAD(void, glBeginQuery, GLenum target, GLuint id) NATIVE_FUNCTION_END_NO_RETURN(void, glBeginQuery, target,id)
NATIVE_FUNCTION_HEAD(void, glEndQuery, GLenum target) NATIVE_FUNCTION_END_NO_RETURN(void, glEndQuery, target)
NATIVE_FUNCTION_HEAD(void, glGetQueryiv, GLenum target, GLenum pname, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetQueryiv, target,pname,params)
NATIVE_FUNCTION_HEAD(void, glGetQueryObjectuiv, GLuint id, GLenum pname, GLuint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetQueryObjectuiv, id,pname,params)

// ============================================================================
// Transform Feedback
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glBindTransformFeedback, GLenum target, GLuint id) NATIVE_FUNCTION_END_NO_RETURN(void, glBindTransformFeedback, target,id)
NATIVE_FUNCTION_HEAD(void, glDeleteTransformFeedbacks, GLsizei n, const GLuint *ids) NATIVE_FUNCTION_END_NO_RETURN(void, glDeleteTransformFeedbacks, n,ids)
NATIVE_FUNCTION_HEAD(void, glGenTransformFeedbacks, GLsizei n, GLuint *ids) NATIVE_FUNCTION_END_NO_RETURN(void, glGenTransformFeedbacks, n,ids)
NATIVE_FUNCTION_HEAD(GLboolean, glIsTransformFeedback, GLuint id) NATIVE_FUNCTION_END(GLboolean, glIsTransformFeedback, id)
NATIVE_FUNCTION_HEAD(void, glBeginTransformFeedback, GLenum primitiveMode) NATIVE_FUNCTION_END_NO_RETURN(void, glBeginTransformFeedback, primitiveMode)
NATIVE_FUNCTION_HEAD(void, glEndTransformFeedback) NATIVE_FUNCTION_END_NO_RETURN(void, glEndTransformFeedback)
NATIVE_FUNCTION_HEAD(void, glPauseTransformFeedback) NATIVE_FUNCTION_END_NO_RETURN(void, glPauseTransformFeedback)
NATIVE_FUNCTION_HEAD(void, glResumeTransformFeedback) NATIVE_FUNCTION_END_NO_RETURN(void, glResumeTransformFeedback)
NATIVE_FUNCTION_HEAD(void, glTransformFeedbackVaryings, GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode) NATIVE_FUNCTION_END_NO_RETURN(void, glTransformFeedbackVaryings, program,count,varyings,bufferMode)
NATIVE_FUNCTION_HEAD(void, glGetTransformFeedbackVarying, GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name) NATIVE_FUNCTION_END_NO_RETURN(void, glGetTransformFeedbackVarying, program,index,bufSize,length,size,type,name)

// ============================================================================
// Compute and Image Operations
// ============================================================================

// glBindImageTexture is handled in drawing.cpp (state tracking)
// glMemoryBarrier is handled in drawing.cpp (atomic counter handling)
NATIVE_FUNCTION_HEAD(void, glMemoryBarrierByRegion, GLbitfield barriers) NATIVE_FUNCTION_END_NO_RETURN(void, glMemoryBarrierByRegion, barriers)

// ============================================================================
// Debug Operations
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glDebugMessageControl, GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled) NATIVE_FUNCTION_END_NO_RETURN(void, glDebugMessageControl, source,type,severity,count,ids,enabled)
NATIVE_FUNCTION_HEAD(void, glDebugMessageInsert, GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf) NATIVE_FUNCTION_END_NO_RETURN(void, glDebugMessageInsert, source,type,id,severity,length,buf)
NATIVE_FUNCTION_HEAD(void, glDebugMessageCallback, GLDEBUGPROC callback, const void *userParam) NATIVE_FUNCTION_END_NO_RETURN(void, glDebugMessageCallback, callback,userParam)
NATIVE_FUNCTION_HEAD(GLuint, glGetDebugMessageLog, GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog) NATIVE_FUNCTION_END(GLuint, glGetDebugMessageLog, count,bufSize,sources,types,ids,severities,lengths,messageLog)
NATIVE_FUNCTION_HEAD(void, glPushDebugGroup, GLenum source, GLuint id, GLsizei length, const GLchar *message) NATIVE_FUNCTION_END_NO_RETURN(void, glPushDebugGroup, source,id,length,message)
NATIVE_FUNCTION_HEAD(void, glPopDebugGroup) NATIVE_FUNCTION_END_NO_RETURN(void, glPopDebugGroup)
NATIVE_FUNCTION_HEAD(void, glObjectLabel, GLenum identifier, GLuint name, GLsizei length, const GLchar *label) NATIVE_FUNCTION_END_NO_RETURN(void, glObjectLabel, identifier,name,length,label)
NATIVE_FUNCTION_HEAD(void, glGetObjectLabel, GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label) NATIVE_FUNCTION_END_NO_RETURN(void, glGetObjectLabel, identifier,name,bufSize,length,label)
NATIVE_FUNCTION_HEAD(void, glObjectPtrLabel, const void *ptr, GLsizei length, const GLchar *label) NATIVE_FUNCTION_END_NO_RETURN(void, glObjectPtrLabel, ptr,length,label)
NATIVE_FUNCTION_HEAD(void, glGetObjectPtrLabel, const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label) NATIVE_FUNCTION_END_NO_RETURN(void, glGetObjectPtrLabel, ptr,bufSize,length,label)

// ============================================================================
// Robust Buffer Access
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glGetnUniformfv, GLuint program, GLint location, GLsizei bufSize, GLfloat *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetnUniformfv, program,location,bufSize,params)
NATIVE_FUNCTION_HEAD(void, glGetnUniformiv, GLuint program, GLint location, GLsizei bufSize, GLint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetnUniformiv, program,location,bufSize,params)
NATIVE_FUNCTION_HEAD(void, glGetnUniformuiv, GLuint program, GLint location, GLsizei bufSize, GLuint *params) NATIVE_FUNCTION_END_NO_RETURN(void, glGetnUniformuiv, program,location,bufSize,params)