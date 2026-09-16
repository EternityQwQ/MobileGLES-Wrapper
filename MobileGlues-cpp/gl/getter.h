// MobileGlues - gl/getter.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "../includes.h"
#include <GL/gl.h>
#include "glcorearb.h"
#include "log.h"
#include "../config/settings.h"
#include "../gles/loader.h"
#include "mg.h"
#include "../version.h"

#ifndef MOBILEGLUES_GETTER_H
#define MOBILEGLUES_GETTER_H

#ifdef __cplusplus
extern "C"
{
#endif

    GLAPI GLAPIENTRY const GLubyte* glGetString(GLenum name);
    GLAPI GLAPIENTRY const GLubyte* glGetStringi(GLenum name, GLuint index);
    GLAPI GLAPIENTRY GLenum glGetError();
    GLAPI GLAPIENTRY void glGetIntegerv(GLenum pname, GLint* params);
    GLAPI GLAPIENTRY void glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params);
    GLAPI GLAPIENTRY void glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64* params);

    void AppendExtension(const char* ext);
    void InitGLESBaseExtensions();
    void set_es_version();

    // Builds the synthetic-string caches and glGetStringi's token tables ahead
    // of the first query, so the work does not land on the first frame. Called
    // once from init_target_gles(). Changing the moment does not change any
    // answer; see the definition in getter.cpp.
    void WarmStringCaches();

    // Repair a device-limit query the host GLES driver did not answer.
    // See "Host Limit Query Fallbacks" in getter.cpp for why this exists.
    void mg_guard_host_limit_i(GLenum pname, GLint* params);
    void mg_guard_host_limit_f(GLenum pname, GLfloat* params);
    void mg_guard_host_limit_i64(GLenum pname, GLint64* params);

    // The boolean form, for glGetBooleanv. Returns true when it repaired the
    // value. Unlike the three above it cannot read its own output to tell an
    // unanswered query from a real answer, so the caller writes `sentinel` into
    // *params before calling the driver and this looks for it still being there.
    // 0xFF is the only value that can serve as a sentinel: GLboolean admits
    // exactly GL_FALSE and GL_TRUE, so nothing a driver may write collides.
    bool mg_guard_host_limit_b(GLenum pname, GLboolean* params, GLboolean sentinel);

#ifdef __cplusplus
}
#endif

extern Version GLVersion;

#endif // MOBILEGLUES_GETTER_H
