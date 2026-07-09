// MobileGlues - gl/impl/VertexArray/GL_VertexArray.h
// GL VertexArray implementation - Core Profile → GLES 3.2
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

void BindVertexArray(GLuint array);
void GenVertexArrays(GLsizei n, GLuint* arrays);
void DeleteVertexArrays(GLsizei n, const GLuint* arrays);
GLboolean IsVertexArray(GLuint array);

void VertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
void VertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
void VertexAttribLFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
void VertexAttribBinding(GLuint attribindex, GLuint bindingindex);
void VertexBindingDivisor(GLuint bindingindex, GLuint divisor);
void BindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizeiptr stride);

void VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
void VertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);
void VertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);
void EnableVertexAttribArray(GLuint index);
void DisableVertexAttribArray(GLuint index);

void VertexAttrib1f(GLuint index, GLfloat x);
void VertexAttrib1fv(GLuint index, const GLfloat* v);
void VertexAttrib2f(GLuint index, GLfloat x, GLfloat y);
void VertexAttrib2fv(GLuint index, const GLfloat* v);
void VertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z);
void VertexAttrib3fv(GLuint index, const GLfloat* v);
void VertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void VertexAttrib4fv(GLuint index, const GLfloat* v);
void VertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w);
void VertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
void VertexAttribI4iv(GLuint index, const GLint* v);
void VertexAttribI4uiv(GLuint index, const GLuint* v);
void VertexAttribL1d(GLuint index, GLdouble x);
void VertexAttribL2d(GLuint index, GLdouble x, GLdouble y);
void VertexAttribL3d(GLuint index, GLdouble x, GLdouble y, GLdouble z);
void VertexAttribL4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);

void GetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params);
void GetVertexAttribiv(GLuint index, GLenum pname, GLint* params);
void GetVertexAttribIiv(GLuint index, GLenum pname, GLint* params);
void GetVertexAttribIuiv(GLuint index, GLenum pname, GLuint* params);
void GetVertexAttribLdv(GLuint index, GLenum pname, GLdouble* params);
void GetVertexAttribPointerv(GLuint index, GLenum pname, void** pointer);

// DSA vertex array functions
void CreateVertexArrays(GLsizei n, GLuint* arrays);
void EnableVertexArrayAttrib(GLuint vaobj, GLuint index);
void DisableVertexArrayAttrib(GLuint vaobj, GLuint index);
void VertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
void VertexArrayAttribIFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
void VertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex);
void VertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizeiptr stride);
void VertexArrayElementBuffer(GLuint vaobj, GLuint buffer);

} // namespace MobileGL::impl::GLImpl