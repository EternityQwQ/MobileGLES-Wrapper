// MobileGlues - gl/impl/Framebuffer/GL_Framebuffer.cpp
// GL Framebuffer implementation - Core Profile → GLES 3.2
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "GL_Framebuffer.h"
#include "../../backend/DirectGLES/DirectGLES.h"
#include "../../backend/DirectGLES/Managers.h"
#include "../../state/Core.h"
#include "../../state.h"

using namespace MobileGL::backend::DirectGLES;
using namespace MobileGL::MG_State::GLState;

namespace MobileGL::impl::GLImpl {

void BindFramebuffer(GLenum target, GLuint framebuffer) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    FramebufferTarget fbTarget = static_cast<FramebufferTarget>(target);
    auto fbo = glCtx.GetFramebuffer(framebuffer);
    stateMgr.GetFramebufferState().BindFramebuffer(fbTarget, fbo);
}

void GenFramebuffers(GLsizei n, GLuint* framebuffers) {
    auto& glCtx = GLContext::Get();
    for (GLsizei i = 0; i < n; ++i) {
        glCtx.CreateFramebuffer(framebuffers[i]);
    }
}

void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
    auto& glCtx = GLContext::Get();
    for (GLsizei i = 0; i < n; ++i) {
        glCtx.DeleteFramebuffer(framebuffers[i]);
    }
}

GLboolean IsFramebuffer(GLuint framebuffer) {
    return GLContext::Get().IsFramebuffer(framebuffer) ? GL_TRUE : GL_FALSE;
}

GLenum CheckFramebufferStatus(GLenum target) {
    return g_GLESFuncs.glCheckFramebufferStatus(target);
}

void FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    auto fbo = stateMgr.GetFramebufferState().GetBoundFramebuffer(static_cast<FramebufferTarget>(target));
    auto tex = glCtx.GetTexture(texture);

    if (fbo) {
        FramebufferAttachmentType attType = static_cast<FramebufferAttachmentType>(attachment);
        fbo->SetAttachment(attType, tex, level);
    }

    CallAndCheckGLES(g_GLESFuncs.glFramebufferTexture2D(target, attachment, textarget, texture, level));
}

void FramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer) {
    CallAndCheckGLES(g_GLESFuncs.glFramebufferTexture3D(target, attachment, textarget, texture, level, layer));
}

void FramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    CallAndCheckGLES(g_GLESFuncs.glFramebufferTextureLayer(target, attachment, texture, level, layer));
}

void FramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    CallAndCheckGLES(g_GLESFuncs.glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer));
}

void GetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetFramebufferAttachmentParameteriv(target, attachment, pname, params));
}

void BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0,
                     GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    CallAndCheckGLES(g_GLESFuncs.glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter));
}

void DrawBuffers(GLsizei n, const GLenum* bufs) {
    CallAndCheckGLES(g_GLESFuncs.glDrawBuffers(n, bufs));
}

void ReadBuffer(GLenum src) {
    CallAndCheckGLES(g_GLESFuncs.glReadBuffer(src));
}

void BindRenderbuffer(GLenum target, GLuint renderbuffer) {
    auto& glCtx = GLContext::Get();
    auto rbo = glCtx.GetRenderbuffer(renderbuffer);
    CallAndCheckGLES(g_GLESFuncs.glBindRenderbuffer(target, renderbuffer));
}

void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
    auto& glCtx = GLContext::Get();
    for (GLsizei i = 0; i < n; ++i) {
        glCtx.CreateRenderbuffer(renderbuffers[i]);
    }
}

void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
    auto& glCtx = GLContext::Get();
    for (GLsizei i = 0; i < n; ++i) {
        glCtx.DeleteRenderbuffer(renderbuffers[i]);
    }
}

GLboolean IsRenderbuffer(GLuint renderbuffer) {
    return GLContext::Get().IsRenderbuffer(renderbuffer) ? GL_TRUE : GL_FALSE;
}

void RenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    CallAndCheckGLES(g_GLESFuncs.glRenderbufferStorage(target, internalformat, width, height));
}

void RenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    CallAndCheckGLES(g_GLESFuncs.glRenderbufferStorageMultisample(target, samples, internalformat, width, height));
}

void GetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetRenderbufferParameteriv(target, pname, params));
}

// DSA framebuffer functions
void CreateFramebuffers(GLsizei n, GLuint* framebuffers) { GenFramebuffers(n, framebuffers); }
void NamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) {
    CallAndCheckGLES(g_GLESFuncs.glNamedFramebufferTexture(framebuffer, attachment, texture, level));
}
void NamedFramebufferTextureLayer(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    CallAndCheckGLES(g_GLESFuncs.glNamedFramebufferTextureLayer(framebuffer, attachment, texture, level, layer));
}
void NamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    CallAndCheckGLES(g_GLESFuncs.glNamedFramebufferRenderbuffer(framebuffer, attachment, renderbuffertarget, renderbuffer));
}
void NamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum* bufs) {
    CallAndCheckGLES(g_GLESFuncs.glNamedFramebufferDrawBuffers(framebuffer, n, bufs));
}
void NamedFramebufferReadBuffer(GLuint framebuffer, GLenum src) {
    CallAndCheckGLES(g_GLESFuncs.glNamedFramebufferReadBuffer(framebuffer, src));
}

} // namespace MobileGL::impl::GLImpl