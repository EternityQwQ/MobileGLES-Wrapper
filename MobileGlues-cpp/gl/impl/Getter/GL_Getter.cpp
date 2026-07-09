// MobileGlues - gl/impl/Getter/GL_Getter.cpp
// GL Getter implementation - Core Profile → GLES 3.2
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "GL_Getter.h"
#include "../../backend/DirectGLES/DirectGLES.h"
#include "../../state/Core.h"
#include "../../state.h"

using namespace MobileGL::backend::DirectGLES;
using namespace MobileGL::MG_State::GLState;

namespace MobileGL::impl::GLImpl {

void GetDoublev(GLenum pname, GLdouble* params) {
    auto& stateMgr = GLContext::Get().GetStateManager();

    switch (pname) {
        case GL_VIEWPORT: {
            GLint vp[4];
            stateMgr.GetViewport(vp);
            for (int i = 0; i < 4; ++i) params[i] = static_cast<GLdouble>(vp[i]);
            return;
        }
        case GL_DEPTH_RANGE: {
            GLfloat range[2];
            stateMgr.GetDepthRange(range);
            params[0] = static_cast<GLdouble>(range[0]);
            params[1] = static_cast<GLdouble>(range[1]);
            return;
        }
        case GL_COLOR_CLEAR_VALUE: {
            GLfloat color[4];
            stateMgr.GetClearColor(color);
            for (int i = 0; i < 4; ++i) params[i] = static_cast<GLdouble>(color[i]);
            return;
        }
        default:
            CallAndCheckGLES(g_GLESFuncs.glGetFloatv(pname, reinterpret_cast<GLfloat*>(params)));
            return;
    }
}

void GetFloatv(GLenum pname, GLfloat* params) {
    auto& stateMgr = GLContext::Get().GetStateManager();

    switch (pname) {
        case GL_VIEWPORT: {
            GLint vp[4];
            stateMgr.GetViewport(vp);
            for (int i = 0; i < 4; ++i) params[i] = static_cast<GLfloat>(vp[i]);
            return;
        }
        case GL_DEPTH_RANGE:
            stateMgr.GetDepthRange(params);
            return;
        case GL_COLOR_CLEAR_VALUE:
            stateMgr.GetClearColor(params);
            return;
        default:
            CallAndCheckGLES(g_GLESFuncs.glGetFloatv(pname, params));
            return;
    }
}

void GetIntegerv(GLenum pname, GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetIntegerv(pname, params));
}

void GetInteger64v(GLenum pname, GLint64* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetInteger64v(pname, params));
}

void GetBooleanv(GLenum pname, GLboolean* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetBooleanv(pname, params));
}

void GetDoublei_v(GLenum target, GLuint index, GLdouble* data) {
    CallAndCheckGLES(g_GLESFuncs.glGetFloati_v(target, index, reinterpret_cast<GLfloat*>(data)));
}

void GetFloati_v(GLenum target, GLuint index, GLfloat* data) {
    CallAndCheckGLES(g_GLESFuncs.glGetFloati_v(target, index, data));
}

void GetIntegeri_v(GLenum target, GLuint index, GLint* data) {
    CallAndCheckGLES(g_GLESFuncs.glGetIntegeri_v(target, index, data));
}

void GetInteger64i_v(GLenum target, GLuint index, GLint64* data) {
    CallAndCheckGLES(g_GLESFuncs.glGetInteger64i_v(target, index, data));
}

void GetBooleani_v(GLenum target, GLuint index, GLboolean* data) {
    CallAndCheckGLES(g_GLESFuncs.glGetBooleani_v(target, index, data));
}

const GLubyte* GetString(GLenum name) {
    return g_GLESFuncs.glGetString(name);
}

const GLubyte* GetStringi(GLenum name, GLuint index) {
    return g_GLESFuncs.glGetStringi(name, index);
}

void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) {
    CallAndCheckGLES(g_GLESFuncs.glReadPixels(x, y, width, height, format, type, pixels));
}

GLenum GetError() {
    return g_GLESFuncs.glGetError();
}

} // namespace MobileGL::impl::GLImpl