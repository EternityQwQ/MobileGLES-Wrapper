// MobileGlues - gl/impl/Framebuffer/GL_Framebuffer.h
// GL Framebuffer implementation - Core Profile → GLES 3.2
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#pragma once

#include "../../../includes.h"

namespace MobileGL::impl::GLImpl {

void BindFramebuffer(GLenum target, GLuint framebuffer);
void GenFramebuffers(GLsizei n, GLuint* framebuffers);
void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
GLboolean IsFramebuffer(GLuint framebuffer);
GLenum CheckFramebufferStatus(GLenum target);
void FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
void FramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer);
void FramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
void FramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
void GetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params);
void BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0,
                     GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
void DrawBuffers(GLsizei n, const GLenum* bufs);
void ReadBuffer(GLenum src);
void BindRenderbuffer(GLenum target, GLuint renderbuffer);
void GenRenderbuffers(GLsizei n, GLuint* renderbuffers);
void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
GLboolean IsRenderbuffer(GLuint renderbuffer);
void RenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
void RenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
void GetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params);

// DSA framebuffer functions
void CreateFramebuffers(GLsizei n, GLuint* framebuffers);
void NamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
void NamedFramebufferTextureLayer(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer);
void NamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
void NamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum* bufs);
void NamedFramebufferReadBuffer(GLuint framebuffer, GLenum src);

} // namespace MobileGL::impl::GLImpl