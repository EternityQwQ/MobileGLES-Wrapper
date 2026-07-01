// MobileGlues - gl/gl.cpp
// Core GL state wrappers: glClear, glClearDepth, glHint, glViewport, etc.
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "../includes.h"
#include <GL/gl.h>
#include "glcorearb.h"
#include "log.h"
#include "../gles/loader.h"
#include "mg.h"
#include <GLES3/gl32.h>

#define DEBUG 0

// ============================================================================
// glClearDepth - desktop uses double, ES uses float
// ============================================================================

void glClearDepth(GLclampd depth) {
    LOG()
    GLES.glClearDepthf((float)depth);
    GLState.legacy.clearDepth = (GLfloat)depth;
    CHECK_GL_ERROR
}

// ============================================================================
// glDepthRange - desktop uses double, ES uses float
// ============================================================================

void glDepthRange(GLclampd near_val, GLclampd far_val) {
    LOG()
    GLES.glDepthRangef((GLfloat)near_val, (GLfloat)far_val);
    CHECK_GL_ERROR
}

// ============================================================================
// glClear - handles legacy clear mask conversion
// ============================================================================

void glClear(GLbitfield mask) {
    LOG()
    // GL_ACCUM_BUFFER_BIT is not supported in ES, strip it
    mask &= ~0x00000200; // GL_ACCUM_BUFFER_BIT
    GLES.glClear(mask);
    CHECK_GL_ERROR
}

// ============================================================================
// glViewport - track viewport state
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glViewport, GLint x, GLint y, GLsizei width, GLsizei height)
{
    GLState.legacy.viewport[0] = x;
    GLState.legacy.viewport[1] = y;
    GLState.legacy.viewport[2] = width;
    GLState.legacy.viewport[3] = height;
}
NATIVE_FUNCTION_END_NO_RETURN(void, glViewport, x, y, width, height)

// ============================================================================
// glHint - pass through (ES supports basic hints)
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glHint, GLenum target, GLenum mode)
{
}
NATIVE_FUNCTION_END_NO_RETURN(void, glHint, target, mode)

// ============================================================================
// glPolygonMode - not supported in ES core, track CPU-side only
// ============================================================================

void glPolygonMode(GLenum face, GLenum mode) {
    LOG()
    // ES 3.2 doesn't support glPolygonMode natively
    // Track it CPU-side for queries
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
        GLState.legacy.polygonMode[0] = mode;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
        GLState.legacy.polygonMode[1] = mode;
    }
    // No actual GL call - ES doesn't support polygon mode
}

// ============================================================================
// glDrawBuffer - maps to glDrawBuffers in ES
// ============================================================================

void glDrawBuffer(GLenum mode) {
    LOG()
    if (mode == GL_NONE) {
        GLenum none = GL_NONE;
        GLES.glDrawBuffers(1, &none);
        GLState.framebuffer.drawBuffers[0] = GL_NONE;
        GLState.framebuffer.drawBufferCount = 1;
    } else if (mode >= GL_COLOR_ATTACHMENT0 && mode <= GL_COLOR_ATTACHMENT0 + 31) {
        GLES.glDrawBuffers(1, &mode);
        GLState.framebuffer.drawBuffers[0] = mode;
        GLState.framebuffer.drawBufferCount = 1;
    } else if (mode == GL_BACK || mode == GL_FRONT) {
        GLenum back = GL_BACK;
        GLES.glDrawBuffers(1, &back);
        GLState.framebuffer.drawBuffers[0] = GL_BACK;
        GLState.framebuffer.drawBufferCount = 1;
    }
    CHECK_GL_ERROR
}

// ============================================================================
// glReadBuffer - track read buffer state
// ============================================================================

NATIVE_FUNCTION_HEAD(void, glReadBuffer, GLenum mode)
{
    GLState.framebuffer.readBuffer = mode;
}
NATIVE_FUNCTION_END_NO_RETURN(void, glReadBuffer, mode)

// ============================================================================
// glPointSize - not in ES 3.2 core, track CPU-side only
// ============================================================================

void glPointSize(GLfloat size) {
    LOG()
    // ES 3.2 doesn't support glPointSize; use gl_PointSize in shader instead
    // Track CPU-side for queries
    GLState.legacy.lineWidth = size; // reuse lineWidth as point size placeholder
}