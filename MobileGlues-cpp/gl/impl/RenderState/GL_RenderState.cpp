// MobileGlues - gl/impl/RenderState/GL_RenderState.cpp
// GL RenderState implementation - Core Profile → GLES 3.2
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "GL_RenderState.h"
#include "../../backend/DirectGLES/DirectGLES.h"
#include "../../state/Core.h"
#include "../../state.h"

using namespace MobileGL::backend::DirectGLES;
using namespace MobileGL::MG_State::GLState;

namespace MobileGL::impl::GLImpl {

void Enable(GLenum cap) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetCapability(cap, true);
    CallAndCheckGLES(g_GLESFuncs.glEnable(cap));
}

void Disable(GLenum cap) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetCapability(cap, false);
    CallAndCheckGLES(g_GLESFuncs.glDisable(cap));
}

GLboolean IsEnabled(GLenum cap) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    return stateMgr.GetCapability(cap) ? GL_TRUE : GL_FALSE;
}

void Enablei(GLenum cap, GLuint index) {
    CallAndCheckGLES(g_GLESFuncs.glEnablei(cap, index));
}

void Disablei(GLenum cap, GLuint index) {
    CallAndCheckGLES(g_GLESFuncs.glDisablei(cap, index));
}

GLboolean IsEnabledi(GLenum cap, GLuint index) {
    return g_GLESFuncs.glIsEnabledi(cap, index);
}

void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetViewport(x, y, width, height);
    CallAndCheckGLES(g_GLESFuncs.glViewport(x, y, width, height));
}

void DepthRange(GLclampd nearVal, GLclampd farVal) {
    DepthRangef(static_cast<GLfloat>(nearVal), static_cast<GLfloat>(farVal));
}

void DepthRangef(GLfloat nearVal, GLfloat farVal) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetDepthRange(nearVal, farVal);
    CallAndCheckGLES(g_GLESFuncs.glDepthRangef(nearVal, farVal));
}

void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetScissor(x, y, width, height);
    CallAndCheckGLES(g_GLESFuncs.glScissor(x, y, width, height));
}

void ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetClearColor(red, green, blue, alpha);
    CallAndCheckGLES(g_GLESFuncs.glClearColor(red, green, blue, alpha));
}

void ClearDepth(GLclampd depth) { ClearDepthf(static_cast<GLfloat>(depth)); }
void ClearDepthf(GLfloat depth) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetClearDepth(depth);
    CallAndCheckGLES(g_GLESFuncs.glClearDepthf(depth));
}

void ClearStencil(GLint s) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetClearStencil(s);
    CallAndCheckGLES(g_GLESFuncs.glClearStencil(s));
}

void ColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetColorMask(red, green, blue, alpha);
    CallAndCheckGLES(g_GLESFuncs.glColorMask(red, green, blue, alpha));
}

void ColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    CallAndCheckGLES(g_GLESFuncs.glColorMaski(index, r, g, b, a));
}

void BlendFunc(GLenum sfactor, GLenum dfactor) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetBlendFunc(sfactor, dfactor);
    CallAndCheckGLES(g_GLESFuncs.glBlendFunc(sfactor, dfactor));
}

void BlendFunci(GLuint buf, GLenum sfactor, GLenum dfactor) {
    CallAndCheckGLES(g_GLESFuncs.glBlendFunci(buf, sfactor, dfactor));
}

void BlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    CallAndCheckGLES(g_GLESFuncs.glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha));
}

void BlendFuncSeparatei(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    CallAndCheckGLES(g_GLESFuncs.glBlendFuncSeparatei(buf, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha));
}

void BlendEquation(GLenum mode) {
    CallAndCheckGLES(g_GLESFuncs.glBlendEquation(mode));
}

void BlendEquationi(GLuint buf, GLenum mode) {
    CallAndCheckGLES(g_GLESFuncs.glBlendEquationi(buf, mode));
}

void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    CallAndCheckGLES(g_GLESFuncs.glBlendEquationSeparate(modeRGB, modeAlpha));
}

void BlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha) {
    CallAndCheckGLES(g_GLESFuncs.glBlendEquationSeparatei(buf, modeRGB, modeAlpha));
}

void BlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    CallAndCheckGLES(g_GLESFuncs.glBlendColor(red, green, blue, alpha));
}

void CullFace(GLenum mode) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetCullFace(mode);
    CallAndCheckGLES(g_GLESFuncs.glCullFace(mode));
}

void FrontFace(GLenum mode) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetFrontFace(mode);
    CallAndCheckGLES(g_GLESFuncs.glFrontFace(mode));
}

void DepthFunc(GLenum func) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetDepthFunc(func);
    CallAndCheckGLES(g_GLESFuncs.glDepthFunc(func));
}

void DepthMask(GLboolean flag) {
    auto& stateMgr = GLContext::Get().GetStateManager();
    stateMgr.SetDepthMask(flag);
    CallAndCheckGLES(g_GLESFuncs.glDepthMask(flag));
}

void StencilFunc(GLenum func, GLint ref, GLuint mask) {
    CallAndCheckGLES(g_GLESFuncs.glStencilFunc(func, ref, mask));
}

void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    CallAndCheckGLES(g_GLESFuncs.glStencilFuncSeparate(face, func, ref, mask));
}

void StencilMask(GLuint mask) {
    CallAndCheckGLES(g_GLESFuncs.glStencilMask(mask));
}

void StencilMaskSeparate(GLenum face, GLuint mask) {
    CallAndCheckGLES(g_GLESFuncs.glStencilMaskSeparate(face, mask));
}

void StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    CallAndCheckGLES(g_GLESFuncs.glStencilOp(fail, zfail, zpass));
}

void StencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    CallAndCheckGLES(g_GLESFuncs.glStencilOpSeparate(face, sfail, dpfail, dppass));
}

void LineWidth(GLfloat width) {
    CallAndCheckGLES(g_GLESFuncs.glLineWidth(width));
}

void PointSize(GLfloat size) {
    CallAndCheckGLES(g_GLESFuncs.glPointSize(size));
}

void PolygonOffset(GLfloat factor, GLfloat units) {
    CallAndCheckGLES(g_GLESFuncs.glPolygonOffset(factor, units));
}

void PolygonMode(GLenum face, GLenum mode) {
    // GLES does not support glPolygonMode - stub
    MG_LOG_WARN("glPolygonMode is not supported in GLES 3.2");
}

void SampleCoverage(GLfloat value, GLboolean invert) {
    CallAndCheckGLES(g_GLESFuncs.glSampleCoverage(value, invert));
}

void SampleMaski(GLuint maskNumber, GLbitfield mask) {
    CallAndCheckGLES(g_GLESFuncs.glSampleMaski(maskNumber, mask));
}

void Hint(GLenum target, GLenum mode) {
    CallAndCheckGLES(g_GLESFuncs.glHint(target, mode));
}

} // namespace MobileGL::impl::GLImpl