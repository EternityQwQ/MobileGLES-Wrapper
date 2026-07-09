// MobileGlues - gl/impl/Exporting.cpp
// GL Core Profile → GLES 3.2 API export dispatcher
// Routes all GL API calls to the appropriate Implementation layer
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "Drawing/GL_Drawing.h"
#include "Buffer/GL_Buffer.h"
#include "Texture/GL_Texture.h"
#include "Framebuffer/GL_Framebuffer.h"
#include "Program/GL_Program.h"
#include "RenderState/GL_RenderState.h"
#include "VertexArray/GL_VertexArray.h"
#include "Getter/GL_Getter.h"

using namespace MobileGL::impl::GLImpl;

// =============================================================================
// Drawing Commands
// =============================================================================

extern "C" void glClear(GLbitfield mask) { Clear(mask); }
extern "C" void glDrawArrays(GLenum mode, GLint first, GLsizei count) { DrawArrays(mode, first, count); }
extern "C" void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    DrawElements(mode, count, type, indices);
}
extern "C" void glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices,
                                         GLint basevertex) {
    DrawElementsBaseVertex(mode, count, type, indices, basevertex);
}
extern "C" void glMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const GLvoid* const* indices,
                                    GLsizei drawcount) {
    MultiDrawElements(mode, count, type, indices, drawcount);
}
extern "C" void glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei* count, GLenum type,
                                              const GLvoid* const* indices, GLsizei drawcount,
                                              const GLint* basevertex) {
    MultiDrawElementsBaseVertex(mode, count, type, indices, drawcount, basevertex);
}
extern "C" void glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount,
                                            GLsizei stride) {
    MultiDrawElementsIndirect(mode, type, indirect, drawcount, stride);
}
extern "C" void glMultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride) {
    MultiDrawArraysIndirect(mode, indirect, drawcount, stride);
}
extern "C" void glMultiDrawElementsIndirectCount(GLenum mode, GLenum type, const void* indirect,
                                                 GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride) {
    MultiDrawElementsIndirectCount(mode, type, indirect, drawcount, maxdrawcount, stride);
}
extern "C" void glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type,
                                              const void* indices, GLint basevertex) {
    DrawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex);
}
extern "C" void glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type,
                                    const void* indices) {
    DrawRangeElements(mode, start, end, count, type, indices);
}
extern "C" void glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type,
                                                              const void* indices, GLsizei instancecount,
                                                              GLint basevertex, GLuint baseinstance) {
    DrawElementsInstancedBaseVertexBaseInstance(mode, count, type, indices, instancecount, basevertex, baseinstance);
}
extern "C" void glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices,
                                                  GLsizei instancecount, GLint basevertex) {
    DrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);
}
extern "C" void glDrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices,
                                                    GLsizei instancecount, GLuint baseinstance) {
    DrawElementsInstancedBaseInstance(mode, count, type, indices, instancecount, baseinstance);
}
extern "C" void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void* indices,
                                        GLsizei instancecount) {
    DrawElementsInstanced(mode, count, type, indices, instancecount);
}
extern "C" void glDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect) {
    DrawElementsIndirect(mode, type, indirect);
}
extern "C" void glDrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount,
                                                  GLuint baseinstance) {
    DrawArraysInstancedBaseInstance(mode, first, count, instancecount, baseinstance);
}
extern "C" void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) {
    DrawArraysInstanced(mode, first, count, instancecount);
}
extern "C" void glDrawArraysIndirect(GLenum mode, const void* indirect) {
    DrawArraysIndirect(mode, indirect);
}
extern "C" void glDispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ) {
    DispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
}
extern "C" void glDispatchComputeIndirect(GLintptr indirect) {
    DispatchComputeIndirect(indirect);
}
extern "C" void glMemoryBarrier(GLbitfield barriers) { MemoryBarrier(barriers); }
extern "C" void glMemoryBarrierByRegion(GLbitfield barriers) { MemoryBarrierByRegion(barriers); }

// =============================================================================
// Buffer Commands
// =============================================================================

extern "C" void glGenBuffers(GLsizei n, GLuint* buffers) { GenBuffers(n, buffers); }
extern "C" void glDeleteBuffers(GLsizei n, const GLuint* buffers) { DeleteBuffers(n, buffers); }
extern "C" GLboolean glIsBuffer(GLuint buffer) { return IsBuffer(buffer); }
extern "C" void glBindBuffer(GLenum target, GLuint buffer) { BindBuffer(target, buffer); }
extern "C" void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    BufferData(target, size, data, usage);
}
extern "C" void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    BufferSubData(target, offset, size, data);
}
extern "C" void glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset,
                                    GLsizeiptr size) {
    CopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}
extern "C" void glBufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags) {
    BufferStorage(target, size, data, flags);
}
extern "C" void* glMapBuffer(GLenum target, GLenum access) { return MapBuffer(target, access); }
extern "C" void* glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    return MapBufferRange(target, offset, length, access);
}
extern "C" GLboolean glUnmapBuffer(GLenum target) { return UnmapBuffer(target); }
extern "C" void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    FlushMappedBufferRange(target, offset, length);
}
extern "C" void glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    BindBufferBase(target, index, buffer);
}
extern "C" void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    BindBufferRange(target, index, buffer, offset, size);
}
extern "C" void glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    GetBufferParameteriv(target, pname, params);
}
extern "C" void glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64* params) {
    GetBufferParameteri64v(target, pname, params);
}
extern "C" void glGetBufferPointerv(GLenum target, GLenum pname, void** params) {
    GetBufferPointerv(target, pname, params);
}

// DSA buffer functions
extern "C" void glCreateBuffers(GLsizei n, GLuint* buffers) { CreateBuffers(n, buffers); }
extern "C" void glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void* data, GLbitfield flags) {
    NamedBufferStorage(buffer, size, data, flags);
}
extern "C" void glNamedBufferData(GLuint buffer, GLsizeiptr size, const void* data, GLenum usage) {
    NamedBufferData(buffer, size, data, usage);
}
extern "C" void glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data) {
    NamedBufferSubData(buffer, offset, size, data);
}
extern "C" void glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset,
                                         GLintptr writeOffset, GLsizeiptr size) {
    CopyNamedBufferSubData(readBuffer, writeBuffer, readOffset, writeOffset, size);
}
extern "C" void glClearNamedBufferData(GLuint buffer, GLenum internalformat, GLenum format, GLenum type,
                                       const void* data) {
    ClearNamedBufferData(buffer, internalformat, format, type, data);
}
extern "C" void glClearNamedBufferSubData(GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size,
                                          GLenum format, GLenum type, const void* data) {
    ClearNamedBufferSubData(buffer, internalformat, offset, size, format, type, data);
}
extern "C" void* glMapNamedBuffer(GLuint buffer, GLenum access) { return MapNamedBuffer(buffer, access); }
extern "C" void* glMapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    return MapNamedBufferRange(buffer, offset, length, access);
}
extern "C" GLboolean glUnmapNamedBuffer(GLuint buffer) { return UnmapNamedBuffer(buffer); }
extern "C" void glFlushMappedNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length) {
    FlushMappedNamedBufferRange(buffer, offset, length);
}
extern "C" void glGetNamedBufferParameteriv(GLuint buffer, GLenum pname, GLint* params) {
    GetNamedBufferParameteriv(buffer, pname, params);
}
extern "C" void glGetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64* params) {
    GetNamedBufferParameteri64v(buffer, pname, params);
}
extern "C" void glGetNamedBufferPointerv(GLuint buffer, GLenum pname, void** params) {
    GetNamedBufferPointerv(buffer, pname, params);
}

// =============================================================================
// Texture Commands
// =============================================================================

extern "C" void glActiveTexture(GLenum texture) { ActiveTexture(texture); }
extern "C" void glBindTexture(GLenum target, GLuint texture) { BindTexture(target, texture); }
extern "C" void glGenTextures(GLsizei n, GLuint* textures) { GenTextures(n, textures); }
extern "C" void glDeleteTextures(GLsizei n, const GLuint* textures) { DeleteTextures(n, textures); }
extern "C" GLboolean glIsTexture(GLuint texture) { return IsTexture(texture); }
extern "C" void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                             GLint border, GLenum format, GLenum type, const void* pixels) {
    TexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}
extern "C" void glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                             GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels) {
    TexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
}
extern "C" void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                GLsizei height, GLenum format, GLenum type, const void* pixels) {
    TexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}
extern "C" void glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                const void* pixels) {
    TexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}
extern "C" void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                                       GLsizei height, GLint border, GLsizei imageSize, const void* data) {
    CompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}
extern "C" void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                          GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
                                          const void* data) {
    CompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
extern "C" void glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                                       GLsizei height, GLsizei depth, GLint border, GLsizei imageSize,
                                       const void* data) {
    CompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data);
}
extern "C" void glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                          GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                                          GLsizei imageSize, const void* data) {
    CompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}
extern "C" void glGetCompressedTexImage(GLenum target, GLint level, void* img) {
    GetCompressedTexImage(target, level, img);
}
extern "C" void glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    TexParameterf(target, pname, param);
}
extern "C" void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    TexParameteri(target, pname, param);
}
extern "C" void glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
    TexParameterfv(target, pname, params);
}
extern "C" void glTexParameteriv(GLenum target, GLenum pname, const GLint* params) {
    TexParameteriv(target, pname, params);
}
extern "C" void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
    GetTexParameterfv(target, pname, params);
}
extern "C" void glGetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
    GetTexParameteriv(target, pname, params);
}
extern "C" void glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat* params) {
    GetTexLevelParameterfv(target, level, pname, params);
}
extern "C" void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params) {
    GetTexLevelParameteriv(target, level, pname, params);
}
extern "C" void glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
    GetTexImage(target, level, format, type, pixels);
}
extern "C" void glGenerateMipmap(GLenum target) { GenerateMipmap(target); }
extern "C" void glPixelStorei(GLenum pname, GLint param) { PixelStorei(pname, param); }
extern "C" void glPixelStoref(GLenum pname, GLfloat param) { PixelStoref(pname, param); }

// Texture DSA
extern "C" void glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    TextureStorage2D(texture, levels, internalformat, width, height);
}
extern "C" void glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height,
                                   GLsizei depth) {
    TextureStorage3D(texture, levels, internalformat, width, height, depth);
}
extern "C" void glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                    GLsizei height, GLenum format, GLenum type, const void* pixels) {
    TextureSubImage2D(texture, level, xoffset, yoffset, width, height, format, type, pixels);
}
extern "C" void glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                    GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                    const void* pixels) {
    TextureSubImage3D(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}
extern "C" void glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                        GLint x, GLint y, GLsizei width, GLsizei height) {
    CopyTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, x, y, width, height);
}

// =============================================================================
// Framebuffer Commands
// =============================================================================

extern "C" void glBindFramebuffer(GLenum target, GLuint framebuffer) { BindFramebuffer(target, framebuffer); }
extern "C" void glGenFramebuffers(GLsizei n, GLuint* framebuffers) { GenFramebuffers(n, framebuffers); }
extern "C" void glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) { DeleteFramebuffers(n, framebuffers); }
extern "C" GLboolean glIsFramebuffer(GLuint framebuffer) { return IsFramebuffer(framebuffer); }
extern "C" GLenum glCheckFramebufferStatus(GLenum target) { return CheckFramebufferStatus(target); }
extern "C" void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    FramebufferTexture2D(target, attachment, textarget, texture, level);
}
extern "C" void glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level,
                                       GLint layer) {
    FramebufferTexture3D(target, attachment, textarget, texture, level, layer);
}
extern "C" void glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    FramebufferTextureLayer(target, attachment, texture, level, layer);
}
extern "C" void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    FramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}
extern "C" void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params) {
    GetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}
extern "C" void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0,
                                  GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}
extern "C" void glDrawBuffers(GLsizei n, const GLenum* bufs) { DrawBuffers(n, bufs); }
extern "C" void glReadBuffer(GLenum src) { ReadBuffer(src); }
extern "C" void glBindRenderbuffer(GLenum target, GLuint renderbuffer) { BindRenderbuffer(target, renderbuffer); }
extern "C" void glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) { GenRenderbuffers(n, renderbuffers); }
extern "C" void glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) { DeleteRenderbuffers(n, renderbuffers); }
extern "C" GLboolean glIsRenderbuffer(GLuint renderbuffer) { return IsRenderbuffer(renderbuffer); }
extern "C" void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    RenderbufferStorage(target, internalformat, width, height);
}
extern "C" void glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                                 GLsizei width, GLsizei height) {
    RenderbufferStorageMultisample(target, samples, internalformat, width, height);
}
extern "C" void glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    GetRenderbufferParameteriv(target, pname, params);
}

// DSA framebuffer functions
extern "C" void glCreateFramebuffers(GLsizei n, GLuint* framebuffers) { CreateFramebuffers(n, framebuffers); }
extern "C" void glNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) {
    NamedFramebufferTexture(framebuffer, attachment, texture, level);
}
extern "C" void glNamedFramebufferTextureLayer(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level,
                                               GLint layer) {
    NamedFramebufferTextureLayer(framebuffer, attachment, texture, level, layer);
}
extern "C" void glNamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget,
                                               GLuint renderbuffer) {
    NamedFramebufferRenderbuffer(framebuffer, attachment, renderbuffertarget, renderbuffer);
}
extern "C" void glNamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum* bufs) {
    NamedFramebufferDrawBuffers(framebuffer, n, bufs);
}
extern "C" void glNamedFramebufferReadBuffer(GLuint framebuffer, GLenum src) {
    NamedFramebufferReadBuffer(framebuffer, src);
}

// =============================================================================
// Program & Shader Commands
// =============================================================================

extern "C" GLuint glCreateShader(GLenum type) { return CreateShader(type); }
extern "C" void glDeleteShader(GLuint shader) { DeleteShader(shader); }
extern "C" GLboolean glIsShader(GLuint shader) { return IsShader(shader); }
extern "C" void glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {
    ShaderSource(shader, count, string, length);
}
extern "C" void glCompileShader(GLuint shader) { CompileShader(shader); }
extern "C" void glGetShaderiv(GLuint shader, GLenum pname, GLint* params) { GetShaderiv(shader, pname, params); }
extern "C" void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
    GetShaderInfoLog(shader, bufSize, length, infoLog);
}
extern "C" GLuint glCreateProgram() { return CreateProgram(); }
extern "C" void glDeleteProgram(GLuint program) { DeleteProgram(program); }
extern "C" GLboolean glIsProgram(GLuint program) { return IsProgram(program); }
extern "C" void glAttachShader(GLuint program, GLuint shader) { AttachShader(program, shader); }
extern "C" void glDetachShader(GLuint program, GLuint shader) { DetachShader(program, shader); }
extern "C" void glLinkProgram(GLuint program) { LinkProgram(program); }
extern "C" void glUseProgram(GLuint program) { UseProgram(program); }
extern "C" void glGetProgramiv(GLuint program, GLenum pname, GLint* params) { GetProgramiv(program, pname, params); }
extern "C" void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
    GetProgramInfoLog(program, bufSize, length, infoLog);
}
extern "C" void glValidateProgram(GLuint program) { ValidateProgram(program); }
extern "C" void glBindAttribLocation(GLuint program, GLuint index, const GLchar* name) {
    BindAttribLocation(program, index, name);
}
extern "C" void glBindFragDataLocation(GLuint program, GLuint color, const GLchar* name) {
    BindFragDataLocation(program, color, name);
}
extern "C" GLint glGetAttribLocation(GLuint program, const GLchar* name) { return GetAttribLocation(program, name); }
extern "C" GLint glGetFragDataLocation(GLuint program, const GLchar* name) { return GetFragDataLocation(program, name); }
extern "C" GLint glGetUniformLocation(GLuint program, const GLchar* name) { return GetUniformLocation(program, name); }
extern "C" void glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size,
                                   GLenum* type, GLchar* name) {
    GetActiveUniform(program, index, bufSize, length, size, type, name);
}
extern "C" void glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size,
                                  GLenum* type, GLchar* name) {
    GetActiveAttrib(program, index, bufSize, length, size, type, name);
}

// Uniform setters
extern "C" void glUniform1f(GLint location, GLfloat v0) { Uniform1f(location, v0); }
extern "C" void glUniform2f(GLint location, GLfloat v0, GLfloat v1) { Uniform2f(location, v0, v1); }
extern "C" void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) { Uniform3f(location, v0, v1, v2); }
extern "C" void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    Uniform4f(location, v0, v1, v2, v3);
}
extern "C" void glUniform1i(GLint location, GLint v0) { Uniform1i(location, v0); }
extern "C" void glUniform2i(GLint location, GLint v0, GLint v1) { Uniform2i(location, v0, v1); }
extern "C" void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) { Uniform3i(location, v0, v1, v2); }
extern "C" void glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    Uniform4i(location, v0, v1, v2, v3);
}
extern "C" void glUniform1ui(GLint location, GLuint v0) { Uniform1ui(location, v0); }
extern "C" void glUniform2ui(GLint location, GLuint v0, GLuint v1) { Uniform2ui(location, v0, v1); }
extern "C" void glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) { Uniform3ui(location, v0, v1, v2); }
extern "C" void glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    Uniform4ui(location, v0, v1, v2, v3);
}
extern "C" void glUniform1fv(GLint location, GLsizei count, const GLfloat* value) {
    Uniform1fv(location, count, value);
}
extern "C" void glUniform2fv(GLint location, GLsizei count, const GLfloat* value) {
    Uniform2fv(location, count, value);
}
extern "C" void glUniform3fv(GLint location, GLsizei count, const GLfloat* value) {
    Uniform3fv(location, count, value);
}
extern "C" void glUniform4fv(GLint location, GLsizei count, const GLfloat* value) {
    Uniform4fv(location, count, value);
}
extern "C" void glUniform1iv(GLint location, GLsizei count, const GLint* value) { Uniform1iv(location, count, value); }
extern "C" void glUniform2iv(GLint location, GLsizei count, const GLint* value) { Uniform2iv(location, count, value); }
extern "C" void glUniform3iv(GLint location, GLsizei count, const GLint* value) { Uniform3iv(location, count, value); }
extern "C" void glUniform4iv(GLint location, GLsizei count, const GLint* value) { Uniform4iv(location, count, value); }
extern "C" void glUniform1uiv(GLint location, GLsizei count, const GLuint* value) {
    Uniform1uiv(location, count, value);
}
extern "C" void glUniform2uiv(GLint location, GLsizei count, const GLuint* value) {
    Uniform2uiv(location, count, value);
}
extern "C" void glUniform3uiv(GLint location, GLsizei count, const GLuint* value) {
    Uniform3uiv(location, count, value);
}
extern "C" void glUniform4uiv(GLint location, GLsizei count, const GLuint* value) {
    Uniform4uiv(location, count, value);
}
extern "C" void glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    UniformMatrix2fv(location, count, transpose, value);
}
extern "C" void glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    UniformMatrix3fv(location, count, transpose, value);
}
extern "C" void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    UniformMatrix4fv(location, count, transpose, value);
}
extern "C" void glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    UniformMatrix2x3fv(location, count, transpose, value);
}
extern "C" void glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    UniformMatrix3x2fv(location, count, transpose, value);
}
extern "C" void glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    UniformMatrix2x4fv(location, count, transpose, value);
}
extern "C" void glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    UniformMatrix4x2fv(location, count, transpose, value);
}
extern "C" void glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    UniformMatrix3x4fv(location, count, transpose, value);
}
extern "C" void glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    UniformMatrix4x3fv(location, count, transpose, value);
}

// Uniform block
extern "C" GLuint glGetUniformBlockIndex(GLuint program, const GLchar* uniformBlockName) {
    return GetUniformBlockIndex(program, uniformBlockName);
}
extern "C" void glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) {
    UniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}
extern "C" void glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params) {
    GetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}
extern "C" void glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize,
                                            GLsizei* length, GLchar* uniformBlockName) {
    GetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

// Program pipeline
extern "C" void glBindProgramPipeline(GLuint pipeline) { BindProgramPipeline(pipeline); }
extern "C" void glGenProgramPipelines(GLsizei n, GLuint* pipelines) { GenProgramPipelines(n, pipelines); }
extern "C" void glDeleteProgramPipelines(GLsizei n, const GLuint* pipelines) { DeleteProgramPipelines(n, pipelines); }
extern "C" void glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program) {
    UseProgramStages(pipeline, stages, program);
}

// =============================================================================
// RenderState Commands
// =============================================================================

extern "C" void glEnable(GLenum cap) { Enable(cap); }
extern "C" void glDisable(GLenum cap) { Disable(cap); }
extern "C" GLboolean glIsEnabled(GLenum cap) { return IsEnabled(cap); }
extern "C" void glEnablei(GLenum cap, GLuint index) { Enablei(cap, index); }
extern "C" void glDisablei(GLenum cap, GLuint index) { Disablei(cap, index); }
extern "C" GLboolean glIsEnabledi(GLenum cap, GLuint index) { return IsEnabledi(cap, index); }
extern "C" void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) { Viewport(x, y, width, height); }
extern "C" void glDepthRange(GLclampd nearVal, GLclampd farVal) { DepthRange(nearVal, farVal); }
extern "C" void glDepthRangef(GLfloat nearVal, GLfloat farVal) { DepthRangef(nearVal, farVal); }
extern "C" void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) { Scissor(x, y, width, height); }
extern "C" void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    ClearColor(red, green, blue, alpha);
}
extern "C" void glClearDepth(GLclampd depth) { ClearDepth(depth); }
extern "C" void glClearDepthf(GLfloat depth) { ClearDepthf(depth); }
extern "C" void glClearStencil(GLint s) { ClearStencil(s); }
extern "C" void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    ColorMask(red, green, blue, alpha);
}
extern "C" void glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    ColorMaski(index, r, g, b, a);
}
extern "C" void glBlendFunc(GLenum sfactor, GLenum dfactor) { BlendFunc(sfactor, dfactor); }
extern "C" void glBlendFunci(GLuint buf, GLenum sfactor, GLenum dfactor) { BlendFunci(buf, sfactor, dfactor); }
extern "C" void glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    BlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}
extern "C" void glBlendFuncSeparatei(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha,
                                     GLenum dfactorAlpha) {
    BlendFuncSeparatei(buf, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}
extern "C" void glBlendEquation(GLenum mode) { BlendEquation(mode); }
extern "C" void glBlendEquationi(GLuint buf, GLenum mode) { BlendEquationi(buf, mode); }
extern "C" void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    BlendEquationSeparate(modeRGB, modeAlpha);
}
extern "C" void glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha) {
    BlendEquationSeparatei(buf, modeRGB, modeAlpha);
}
extern "C" void glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    BlendColor(red, green, blue, alpha);
}
extern "C" void glCullFace(GLenum mode) { CullFace(mode); }
extern "C" void glFrontFace(GLenum mode) { FrontFace(mode); }
extern "C" void glDepthFunc(GLenum func) { DepthFunc(func); }
extern "C" void glDepthMask(GLboolean flag) { DepthMask(flag); }
extern "C" void glStencilFunc(GLenum func, GLint ref, GLuint mask) { StencilFunc(func, ref, mask); }
extern "C" void glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    StencilFuncSeparate(face, func, ref, mask);
}
extern "C" void glStencilMask(GLuint mask) { StencilMask(mask); }
extern "C" void glStencilMaskSeparate(GLenum face, GLuint mask) { StencilMaskSeparate(face, mask); }
extern "C" void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) { StencilOp(fail, zfail, zpass); }
extern "C" void glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    StencilOpSeparate(face, sfail, dpfail, dppass);
}
extern "C" void glLineWidth(GLfloat width) { LineWidth(width); }
extern "C" void glPointSize(GLfloat size) { PointSize(size); }
extern "C" void glPolygonOffset(GLfloat factor, GLfloat units) { PolygonOffset(factor, units); }
extern "C" void glPolygonMode(GLenum face, GLenum mode) { PolygonMode(face, mode); }
extern "C" void glSampleCoverage(GLfloat value, GLboolean invert) { SampleCoverage(value, invert); }
extern "C" void glSampleMaski(GLuint maskNumber, GLbitfield mask) { SampleMaski(maskNumber, mask); }
extern "C" void glHint(GLenum target, GLenum mode) { Hint(target, mode); }

// =============================================================================
// VertexArray Commands
// =============================================================================

extern "C" void glBindVertexArray(GLuint array) { BindVertexArray(array); }
extern "C" void glGenVertexArrays(GLsizei n, GLuint* arrays) { GenVertexArrays(n, arrays); }
extern "C" void glDeleteVertexArrays(GLsizei n, const GLuint* arrays) { DeleteVertexArrays(n, arrays); }
extern "C" GLboolean glIsVertexArray(GLuint array) { return IsVertexArray(array); }
extern "C" void glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized,
                                     GLuint relativeoffset) {
    VertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
}
extern "C" void glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    VertexAttribIFormat(attribindex, size, type, relativeoffset);
}
extern "C" void glVertexAttribLFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    VertexAttribLFormat(attribindex, size, type, relativeoffset);
}
extern "C" void glVertexAttribBinding(GLuint attribindex, GLuint bindingindex) {
    VertexAttribBinding(attribindex, bindingindex);
}
extern "C" void glVertexBindingDivisor(GLuint bindingindex, GLuint divisor) {
    VertexBindingDivisor(bindingindex, divisor);
}
extern "C" void glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizeiptr stride) {
    BindVertexBuffer(bindingindex, buffer, offset, stride);
}
extern "C" void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
                                      const void* pointer) {
    VertexAttribPointer(index, size, type, normalized, stride, pointer);
}
extern "C" void glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer) {
    VertexAttribIPointer(index, size, type, stride, pointer);
}
extern "C" void glVertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer) {
    VertexAttribLPointer(index, size, type, stride, pointer);
}
extern "C" void glEnableVertexAttribArray(GLuint index) { EnableVertexAttribArray(index); }
extern "C" void glDisableVertexAttribArray(GLuint index) { DisableVertexAttribArray(index); }
extern "C" void glVertexAttrib1f(GLuint index, GLfloat x) { VertexAttrib1f(index, x); }
extern "C" void glVertexAttrib1fv(GLuint index, const GLfloat* v) { VertexAttrib1fv(index, v); }
extern "C" void glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) { VertexAttrib2f(index, x, y); }
extern "C" void glVertexAttrib2fv(GLuint index, const GLfloat* v) { VertexAttrib2fv(index, v); }
extern "C" void glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) { VertexAttrib3f(index, x, y, z); }
extern "C" void glVertexAttrib3fv(GLuint index, const GLfloat* v) { VertexAttrib3fv(index, v); }
extern "C" void glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    VertexAttrib4f(index, x, y, z, w);
}
extern "C" void glVertexAttrib4fv(GLuint index, const GLfloat* v) { VertexAttrib4fv(index, v); }
extern "C" void glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    VertexAttribI4i(index, x, y, z, w);
}
extern "C" void glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    VertexAttribI4ui(index, x, y, z, w);
}
extern "C" void glVertexAttribI4iv(GLuint index, const GLint* v) { VertexAttribI4iv(index, v); }
extern "C" void glVertexAttribI4uiv(GLuint index, const GLuint* v) { VertexAttribI4uiv(index, v); }
extern "C" void glVertexAttribL1d(GLuint index, GLdouble x) { VertexAttribL1d(index, x); }
extern "C" void glVertexAttribL2d(GLuint index, GLdouble x, GLdouble y) { VertexAttribL2d(index, x, y); }
extern "C" void glVertexAttribL3d(GLuint index, GLdouble x, GLdouble y, GLdouble z) {
    VertexAttribL3d(index, x, y, z);
}
extern "C" void glVertexAttribL4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
    VertexAttribL4d(index, x, y, z, w);
}
extern "C" void glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) {
    GetVertexAttribfv(index, pname, params);
}
extern "C" void glGetVertexAttribiv(GLuint index, GLenum pname, GLint* params) {
    GetVertexAttribiv(index, pname, params);
}
extern "C" void glGetVertexAttribIiv(GLuint index, GLenum pname, GLint* params) {
    GetVertexAttribIiv(index, pname, params);
}
extern "C" void glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint* params) {
    GetVertexAttribIuiv(index, pname, params);
}
extern "C" void glGetVertexAttribLdv(GLuint index, GLenum pname, GLdouble* params) {
    GetVertexAttribLdv(index, pname, params);
}
extern "C" void glGetVertexAttribPointerv(GLuint index, GLenum pname, void** pointer) {
    GetVertexAttribPointerv(index, pname, pointer);
}

// DSA vertex array
extern "C" void glCreateVertexArrays(GLsizei n, GLuint* arrays) { CreateVertexArrays(n, arrays); }
extern "C" void glEnableVertexArrayAttrib(GLuint vaobj, GLuint index) { EnableVertexArrayAttrib(vaobj, index); }
extern "C" void glDisableVertexArrayAttrib(GLuint vaobj, GLuint index) { DisableVertexArrayAttrib(vaobj, index); }
extern "C" void glVertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type,
                                          GLboolean normalized, GLuint relativeoffset) {
    VertexArrayAttribFormat(vaobj, attribindex, size, type, normalized, relativeoffset);
}
extern "C" void glVertexArrayAttribIFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type,
                                           GLuint relativeoffset) {
    VertexArrayAttribIFormat(vaobj, attribindex, size, type, relativeoffset);
}
extern "C" void glVertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex) {
    VertexArrayAttribBinding(vaobj, attribindex, bindingindex);
}
extern "C" void glVertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset,
                                          GLsizeiptr stride) {
    VertexArrayVertexBuffer(vaobj, bindingindex, buffer, offset, stride);
}
extern "C" void glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer) {
    VertexArrayElementBuffer(vaobj, buffer);
}

// =============================================================================
// Getter Commands
// =============================================================================

extern "C" void glGetDoublev(GLenum pname, GLdouble* params) { GetDoublev(pname, params); }
extern "C" void glGetFloatv(GLenum pname, GLfloat* params) { GetFloatv(pname, params); }
extern "C" void glGetIntegerv(GLenum pname, GLint* params) { GetIntegerv(pname, params); }
extern "C" void glGetInteger64v(GLenum pname, GLint64* params) { GetInteger64v(pname, params); }
extern "C" void glGetBooleanv(GLenum pname, GLboolean* params) { GetBooleanv(pname, params); }
extern "C" void glGetDoublei_v(GLenum target, GLuint index, GLdouble* data) { GetDoublei_v(target, index, data); }
extern "C" void glGetFloati_v(GLenum target, GLuint index, GLfloat* data) { GetFloati_v(target, index, data); }
extern "C" void glGetIntegeri_v(GLenum target, GLuint index, GLint* data) { GetIntegeri_v(target, index, data); }
extern "C" void glGetInteger64i_v(GLenum target, GLuint index, GLint64* data) { GetInteger64i_v(target, index, data); }
extern "C" void glGetBooleani_v(GLenum target, GLuint index, GLboolean* data) { GetBooleani_v(target, index, data); }
extern "C" const GLubyte* glGetString(GLenum name) { return GetString(name); }
extern "C" const GLubyte* glGetStringi(GLenum name, GLuint index) { return GetStringi(name, index); }
extern "C" void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) {
    ReadPixels(x, y, width, height, format, type, pixels);
}
extern "C" GLenum glGetError() { return GetError(); }