// MobileGlues - gl/impl/VertexArray/GL_VertexArray.cpp
// GL VertexArray implementation - Core Profile → GLES 3.2
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "GL_VertexArray.h"
#include "../../backend/DirectGLES/DirectGLES.h"
#include "../../backend/DirectGLES/Managers.h"
#include "../../state/Core.h"
#include "../../state.h"

using namespace MobileGL::backend::DirectGLES;
using namespace MobileGL::MG_State::GLState;

namespace MobileGL::impl::GLImpl {

void BindVertexArray(GLuint array) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    auto vao = glCtx.GetVertexArray(array);
    stateMgr.SetCurrentVertexArray(array);

    CallAndCheckGLES(g_GLESFuncs.glBindVertexArray(array));
}

void GenVertexArrays(GLsizei n, GLuint* arrays) {
    auto& glCtx = GLContext::Get();
    for (GLsizei i = 0; i < n; ++i) {
        glCtx.CreateVertexArray(arrays[i]);
    }
}

void DeleteVertexArrays(GLsizei n, const GLuint* arrays) {
    auto& glCtx = GLContext::Get();
    for (GLsizei i = 0; i < n; ++i) {
        glCtx.DeleteVertexArray(arrays[i]);
    }
}

GLboolean IsVertexArray(GLuint array) {
    return GLContext::Get().IsVertexArray(array) ? GL_TRUE : GL_FALSE;
}

void VertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
    CallAndCheckGLES(g_GLESFuncs.glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset));
}

void VertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    CallAndCheckGLES(g_GLESFuncs.glVertexAttribIFormat(attribindex, size, type, relativeoffset));
}

void VertexAttribLFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    // GLES 3.2 does not support double-precision vertex attributes
    MG_LOG_WARN("glVertexAttribLFormat is not supported in GLES 3.2");
}

void VertexAttribBinding(GLuint attribindex, GLuint bindingindex) {
    CallAndCheckGLES(g_GLESFuncs.glVertexAttribBinding(attribindex, bindingindex));
}

void VertexBindingDivisor(GLuint bindingindex, GLuint divisor) {
    CallAndCheckGLES(g_GLESFuncs.glVertexBindingDivisor(bindingindex, divisor));
}

void BindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizeiptr stride) {
    CallAndCheckGLES(g_GLESFuncs.glBindVertexBuffer(bindingindex, buffer, offset, stride));
}

void VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) {
    CallAndCheckGLES(g_GLESFuncs.glVertexAttribPointer(index, size, type, normalized, stride, pointer));
}

void VertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer) {
    CallAndCheckGLES(g_GLESFuncs.glVertexAttribIPointer(index, size, type, stride, pointer));
}

void VertexAttribLPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer) {
    MG_LOG_WARN("glVertexAttribLPointer is not supported in GLES 3.2");
}

void EnableVertexAttribArray(GLuint index) {
    CallAndCheckGLES(g_GLESFuncs.glEnableVertexAttribArray(index));
}

void DisableVertexAttribArray(GLuint index) {
    CallAndCheckGLES(g_GLESFuncs.glDisableVertexAttribArray(index));
}

void VertexAttrib1f(GLuint index, GLfloat x) { g_GLESFuncs.glVertexAttrib1f(index, x); }
void VertexAttrib1fv(GLuint index, const GLfloat* v) { g_GLESFuncs.glVertexAttrib1fv(index, v); }
void VertexAttrib2f(GLuint index, GLfloat x, GLfloat y) { g_GLESFuncs.glVertexAttrib2f(index, x, y); }
void VertexAttrib2fv(GLuint index, const GLfloat* v) { g_GLESFuncs.glVertexAttrib2fv(index, v); }
void VertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) { g_GLESFuncs.glVertexAttrib3f(index, x, y, z); }
void VertexAttrib3fv(GLuint index, const GLfloat* v) { g_GLESFuncs.glVertexAttrib3fv(index, v); }
void VertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { g_GLESFuncs.glVertexAttrib4f(index, x, y, z, w); }
void VertexAttrib4fv(GLuint index, const GLfloat* v) { g_GLESFuncs.glVertexAttrib4fv(index, v); }
void VertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) { g_GLESFuncs.glVertexAttribI4i(index, x, y, z, w); }
void VertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) { g_GLESFuncs.glVertexAttribI4ui(index, x, y, z, w); }
void VertexAttribI4iv(GLuint index, const GLint* v) { g_GLESFuncs.glVertexAttribI4iv(index, v); }
void VertexAttribI4uiv(GLuint index, const GLuint* v) { g_GLESFuncs.glVertexAttribI4uiv(index, v); }
void VertexAttribL1d(GLuint index, GLdouble x) { g_GLESFuncs.glVertexAttrib1f(index, static_cast<GLfloat>(x)); }
void VertexAttribL2d(GLuint index, GLdouble x, GLdouble y) { g_GLESFuncs.glVertexAttrib2f(index, static_cast<GLfloat>(x), static_cast<GLfloat>(y)); }
void VertexAttribL3d(GLuint index, GLdouble x, GLdouble y, GLdouble z) { g_GLESFuncs.glVertexAttrib3f(index, static_cast<GLfloat>(x), static_cast<GLfloat>(y), static_cast<GLfloat>(z)); }
void VertexAttribL4d(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) { g_GLESFuncs.glVertexAttrib4f(index, static_cast<GLfloat>(x), static_cast<GLfloat>(y), static_cast<GLfloat>(z), static_cast<GLfloat>(w)); }

void GetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetVertexAttribfv(index, pname, params));
}
void GetVertexAttribiv(GLuint index, GLenum pname, GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetVertexAttribiv(index, pname, params));
}
void GetVertexAttribIiv(GLuint index, GLenum pname, GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetVertexAttribIiv(index, pname, params));
}
void GetVertexAttribIuiv(GLuint index, GLenum pname, GLuint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetVertexAttribIuiv(index, pname, params));
}
void GetVertexAttribLdv(GLuint index, GLenum pname, GLdouble* params) {
    // GLES doesn't support double vertex attribs; convert from float
    GLfloat floatParams[4] = {};
    CallAndCheckGLES(g_GLESFuncs.glGetVertexAttribfv(index, pname, floatParams));
    params[0] = static_cast<GLdouble>(floatParams[0]);
    if (pname == GL_CURRENT_VERTEX_ATTRIB) {
        params[1] = static_cast<GLdouble>(floatParams[1]);
        params[2] = static_cast<GLdouble>(floatParams[2]);
        params[3] = static_cast<GLdouble>(floatParams[3]);
    }
}
void GetVertexAttribPointerv(GLuint index, GLenum pname, void** pointer) {
    CallAndCheckGLES(g_GLESFuncs.glGetVertexAttribPointerv(index, pname, pointer));
}

// DSA vertex array
void CreateVertexArrays(GLsizei n, GLuint* arrays) { GenVertexArrays(n, arrays); }
void EnableVertexArrayAttrib(GLuint vaobj, GLuint index) {
    CallAndCheckGLES(g_GLESFuncs.glEnableVertexArrayAttrib(vaobj, index));
}
void DisableVertexArrayAttrib(GLuint vaobj, GLuint index) {
    CallAndCheckGLES(g_GLESFuncs.glDisableVertexArrayAttrib(vaobj, index));
}
void VertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
    CallAndCheckGLES(g_GLESFuncs.glVertexArrayAttribFormat(vaobj, attribindex, size, type, normalized, relativeoffset));
}
void VertexArrayAttribIFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    CallAndCheckGLES(g_GLESFuncs.glVertexArrayAttribIFormat(vaobj, attribindex, size, type, relativeoffset));
}
void VertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex) {
    CallAndCheckGLES(g_GLESFuncs.glVertexArrayAttribBinding(vaobj, attribindex, bindingindex));
}
void VertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizeiptr stride) {
    CallAndCheckGLES(g_GLESFuncs.glVertexArrayVertexBuffer(vaobj, bindingindex, buffer, offset, stride));
}
void VertexArrayElementBuffer(GLuint vaobj, GLuint buffer) {
    CallAndCheckGLES(g_GLESFuncs.glVertexArrayElementBuffer(vaobj, buffer));
}

} // namespace MobileGL::impl::GLImpl