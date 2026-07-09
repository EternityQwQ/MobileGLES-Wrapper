// MobileGlues - gl/impl/Getter/GL_Getter.h
// GL Getter implementation - Core Profile → GLES 3.2
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

void GetDoublev(GLenum pname, GLdouble* params);
void GetFloatv(GLenum pname, GLfloat* params);
void GetIntegerv(GLenum pname, GLint* params);
void GetInteger64v(GLenum pname, GLint64* params);
void GetBooleanv(GLenum pname, GLboolean* params);

void GetDoublei_v(GLenum target, GLuint index, GLdouble* data);
void GetFloati_v(GLenum target, GLuint index, GLfloat* data);
void GetIntegeri_v(GLenum target, GLuint index, GLint* data);
void GetInteger64i_v(GLenum target, GLuint index, GLint64* data);
void GetBooleani_v(GLenum target, GLuint index, GLboolean* data);

const GLubyte* GetString(GLenum name);
const GLubyte* GetStringi(GLenum name, GLuint index);

void GetTexParameteriv(GLenum target, GLenum pname, GLint* params);
void GetTexParameterfv(GLenum target, GLenum pname, GLfloat* params);
void GetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params);
void GetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat* params);

void GetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params);
void GetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params);

void GetShaderiv(GLuint shader, GLenum pname, GLint* params);
void GetProgramiv(GLuint program, GLenum pname, GLint* params);

void GetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
void GetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);

void GetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size,
                      GLenum* type, GLchar* name);
void GetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size,
                     GLenum* type, GLchar* name);
GLint GetAttribLocation(GLuint program, const GLchar* name);
GLint GetUniformLocation(GLuint program, const GLchar* name);

void GetError();

void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);

} // namespace MobileGL::impl::GLImpl