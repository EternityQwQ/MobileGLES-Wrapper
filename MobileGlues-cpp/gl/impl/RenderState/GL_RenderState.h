// MobileGlues - gl/impl/RenderState/GL_RenderState.h
// GL RenderState implementation - Core Profile → GLES 3.2
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

void Enable(GLenum cap);
void Disable(GLenum cap);
GLboolean IsEnabled(GLenum cap);
void Enablei(GLenum cap, GLuint index);
void Disablei(GLenum cap, GLuint index);
GLboolean IsEnabledi(GLenum cap, GLuint index);

void Viewport(GLint x, GLint y, GLsizei width, GLsizei height);
void DepthRange(GLclampd nearVal, GLclampd farVal);
void DepthRangef(GLfloat nearVal, GLfloat farVal);
void Scissor(GLint x, GLint y, GLsizei width, GLsizei height);

void ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void ClearDepth(GLclampd depth);
void ClearDepthf(GLfloat depth);
void ClearStencil(GLint s);
void ColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void ColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a);

void BlendFunc(GLenum sfactor, GLenum dfactor);
void BlendFunci(GLuint buf, GLenum sfactor, GLenum dfactor);
void BlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
void BlendFuncSeparatei(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
void BlendEquation(GLenum mode);
void BlendEquationi(GLuint buf, GLenum mode);
void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
void BlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha);
void BlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);

void CullFace(GLenum mode);
void FrontFace(GLenum mode);
void DepthFunc(GLenum func);
void DepthMask(GLboolean flag);
void StencilFunc(GLenum func, GLint ref, GLuint mask);
void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
void StencilMask(GLuint mask);
void StencilMaskSeparate(GLenum face, GLuint mask);
void StencilOp(GLenum fail, GLenum zfail, GLenum zpass);
void StencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);

void LineWidth(GLfloat width);
void PointSize(GLfloat size);
void PolygonOffset(GLfloat factor, GLfloat units);
void PolygonMode(GLenum face, GLenum mode);
void SampleCoverage(GLfloat value, GLboolean invert);
void SampleMaski(GLuint maskNumber, GLbitfield mask);

void Hint(GLenum target, GLenum mode);

} // namespace MobileGL::impl::GLImpl