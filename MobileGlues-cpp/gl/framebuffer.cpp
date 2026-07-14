// MobileGlues - gl/framebuffer.cpp
// Framebuffer state management: FBO ID mapping, attachment tracking,
// draw buffer remapping, and temp FBO pool for format conversion.
//
// Architecture principle: "ES 3.2 native → native, ES 3.2 not native → CPU simulation"
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
#include "texture.h"
#include <GLES3/gl32.h>
#include <cstring>
#include <vector>

#define DEBUG 0

// ---------------------------------------------------------------------------
// Float color-format detection helper.
//
// GLES 3.2 core only guarantees color-renderability for a small set of
// formats (RGBA8, RGB10_A2, SRGB8_ALPHA8, the RGBA8/16/32 *UI/I variants,
// etc.). Float and half-float internalformats require the
// GL_EXT_color_buffer_float / GL_EXT_color_buffer_half_float extensions to
// be usable as FBO color attachments; without them glCheckFramebufferStatus
// returns GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT even though desktop GL (the
// API MobileGlues translates *from*) accepts these configurations as core
// since GL 3.0.
//
// This helper returns true when `internalFormat` is a float format whose
// required color-buffer extension is NOT present on the underlying GLES
// driver. glCheckFramebufferStatus() uses it to decide whether to promote
// INCOMPLETE_ATTACHMENT -> COMPLETE so that Iris shader packs (which
// routinely attach RGBA16F / RGBA32F / R11F_G11F_B10F color textures)
// keep rendering instead of disabling shaders.
// ---------------------------------------------------------------------------
static bool float_color_format_needs_missing_ext(GLenum internalFormat) {
    switch (internalFormat) {
    // Half-float color attachments — require GL_EXT_color_buffer_half_float.
    case GL_RGBA16F:
    case GL_RG16F:
    case GL_R16F:
    case GL_RGB16F:
        return !g_gles_caps.GL_EXT_color_buffer_half_float;
    // Full-float color attachments — require GL_EXT_color_buffer_float.
    case GL_RGBA32F:
    case GL_RGB32F:
    case GL_RG32F:
    case GL_R32F:
    case GL_R11F_G11F_B10F:
    case GL_RGB9_E5:
        return !g_gles_caps.GL_EXT_color_buffer_float;
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// Temporary FBO pool for hot-path texture operations
// (acquireTempFBO / releaseTempFBO — used by texture.cpp)
// ---------------------------------------------------------------------------

static std::vector<GLuint> g_tempFboPool;
static size_t g_tempFboPoolIndex = 0;

// ============================================================================
// FBO ID management
// ============================================================================

extern "C" GLAPI GLAPIENTRY void glGenFramebuffers(GLsizei n, GLuint *framebuffers) {
    LOG()
    GLES.glGenFramebuffers(n, framebuffers);

    auto &fb = GLState.framebuffer;
    for (GLsizei i = 0; i < n; i++) {
        fb.fboMap[framebuffers[i]] = framebuffers[i];
        fb.fboMapReverse[framebuffers[i]] = framebuffers[i];
    }
}

extern "C" GLAPI GLAPIENTRY void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) {
    LOG()
    GLES.glDeleteFramebuffers(n, framebuffers);

    auto &fb = GLState.framebuffer;
    for (GLsizei i = 0; i < n; i++) {
        GLuint id = framebuffers[i];
        if (id == 0) continue;

        if (fb.drawFBO == id) fb.drawFBO = 0;
        if (fb.readFBO == id) fb.readFBO = 0;

        fb.fboMap.erase(id);
        fb.fboMapReverse.erase(id);
        fb.attachments.erase(id);
    }
}

// ============================================================================
// glBindFramebuffer
// ============================================================================

extern "C" GLAPI GLAPIENTRY void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    LOG()
    GLES.glBindFramebuffer(target, framebuffer);

    // Hot path: most callers pass GL_FRAMEBUFFER (0x8D40), which binds to
    // both draw and read targets. Restructured so the common case is a
    // single branch instead of two OR-checks that each re-test the same
    // value. The DRAW/READ-only splits are cold by comparison.
    auto &fb = GLState.framebuffer;
    if (target == GL_FRAMEBUFFER) [[likely]] {
        fb.drawFBO = framebuffer;
        fb.readFBO = framebuffer;
        GLState.currentDrawFBO = framebuffer;
    } else if (target == GL_DRAW_FRAMEBUFFER) {
        fb.drawFBO = framebuffer;
        GLState.currentDrawFBO = framebuffer;
    } else if (target == GL_READ_FRAMEBUFFER) {
        fb.readFBO = framebuffer;
    }

    STATE_LOG("glBindFramebuffer: target=0x%X, fbo=%u", target, framebuffer);
}

// ============================================================================
// glFramebufferTexture2D / glFramebufferTexture - attachment tracking
// ============================================================================

extern "C" GLAPI GLAPIENTRY void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    LOG()
    GLenum esTexTarget = GLStateManager::ConvertTextureTarget(textarget);
    GLES.glFramebufferTexture2D(target, attachment, esTexTarget, texture, level);

    // Track attachment
    GLuint currentFBO = (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
                        ? GLState.framebuffer.drawFBO
                        : GLState.framebuffer.readFBO;

    if (currentFBO) {
        auto &att = GLState.framebuffer.attachments[currentFBO][attachment];
        att.fbo = currentFBO;
        att.attachment = attachment;
        att.texture = texture;
    }
}

extern "C" GLAPI GLAPIENTRY void glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    LOG()
    GLES.glFramebufferTexture(target, attachment, texture, level);

    GLuint currentFBO = (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
                        ? GLState.framebuffer.drawFBO
                        : GLState.framebuffer.readFBO;

    if (currentFBO) {
        auto &att = GLState.framebuffer.attachments[currentFBO][attachment];
        att.fbo = currentFBO;
        att.attachment = attachment;
        att.texture = texture;
    }
}

// ============================================================================
// glFramebufferTextureLayer - native passthrough
// ============================================================================

extern "C" GLAPI GLAPIENTRY void glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    GLES.glFramebufferTextureLayer(target, attachment, texture, level, layer);

    // Track attachment so glCheckFramebufferStatus can inspect the texture's
    // internalformat when GLES reports INCOMPLETE_ATTACHMENT.
    GLuint currentFBO = (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
                        ? GLState.framebuffer.drawFBO
                        : GLState.framebuffer.readFBO;

    if (currentFBO) {
        auto &att = GLState.framebuffer.attachments[currentFBO][attachment];
        att.fbo = currentFBO;
        att.attachment = attachment;
        att.texture = texture;
    }
}

// ============================================================================
// glFramebufferRenderbuffer - native passthrough
// ============================================================================

extern "C" GLAPI GLAPIENTRY void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    GLES.glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

// ============================================================================
// glCheckFramebufferStatus
// ============================================================================

extern "C" GLAPI GLAPIENTRY GLenum glCheckFramebufferStatus(GLenum target) {
    LOG()
    GLenum status = GLES.glCheckFramebufferStatus(target);
    // GLES drivers occasionally report GL_FRAMEBUFFER_UNSUPPORTED for FBO
    // configurations that are legitimate on desktop GL (e.g. depth-stencil
    // texture attachments with specific sized internalformats). Desktop GL
    // shaders (like ComplementaryUnbound) rely on these configurations, and
    // the desktop driver typically allows them in practice. Promote
    // UNSUPPORTED → COMPLETE so callers continue rendering instead of
    // aborting on a configuration that the desktop driver would have accepted.
    //
    // GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT is returned by GLES drivers when
    // a color attachment uses a float/half-float internalformat (RGBA16F,
    // RGBA32F, R11F_G11F_B10F, ...) and the device lacks the
    // GL_EXT_color_buffer_float / GL_EXT_color_buffer_half_float extension
    // that makes those formats color-renderable. Desktop GL accepts these
    // configurations as core since GL 3.0, and Iris shader packs rely on
    // them for HDR render targets. Unlike INCOMPLETE_MISSING_ATTACHMENT
    // (which means there is NO attachment at all and is a real bug),
    // INCOMPLETE_ATTACHMENT means there IS an attachment but GLES considers
    // it non-renderable — which on desktop GL would have been fine. We
    // therefore promote INCOMPLETE_ATTACHMENT → COMPLETE to match desktop
    // GL behaviour, so Iris keeps its shader pipeline instead of disabling
    // shaders and falling back to vanilla rendering.
    //
    // Other incomplete statuses (INCOMPLETE_MISSING_ATTACHMENT,
    // INCOMPLETE_DIMENSIONS, INCOMPLETE_MULTISAMPLE, UNDEFINED) indicate
    // *real* configuration bugs (missing attachment, size mismatch, etc.)
    // that would cause rendering into the FBO to silently fail in GLES.
    // Returning the real status lets applications see the problem and fall
    // back to a working path instead of rendering into a broken FBO (which
    // manifests as a black screen, e.g. when Xaero's World Map allocates its
    // region FBO with a configuration GLES actually rejects).
    switch (status) {
        case GL_FRAMEBUFFER_COMPLETE:
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            LOG_D("glCheckFramebufferStatus: promoting UNSUPPORTED -> COMPLETE (target=0x%X)", target);
            status = GL_FRAMEBUFFER_COMPLETE;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: {
            // Try to identify the cause via two paths:
            //   (a) our own attachment tracking (texture ID → internal_format)
            //   (b) direct GLES query of GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE
            // If either path confirms a float attachment, log it for diagnostics.
            // Either way, promote to COMPLETE — on desktop GL the configuration
            // would have been valid, and the alternative (Iris disabling
            // shaders) is worse than rendering into a possibly-degraded FBO.
            GLuint currentFBO = (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
                                ? GLState.framebuffer.drawFBO
                                : GLState.framebuffer.readFBO;
            bool foundFloatAttachment = false;

            // Path (a): consult our attachment tracking.
            if (currentFBO) {
                auto fboIt = GLState.framebuffer.attachments.find(currentFBO);
                if (fboIt != GLState.framebuffer.attachments.end()) {
                    for (const auto& [attPoint, info] : fboIt->second) {
                        if (info.texture == 0) continue;
                        TextureObject* tex = mgGetTexObjectByID(info.texture);
                        if (tex && float_color_format_needs_missing_ext(tex->internal_format)) {
                            foundFloatAttachment = true;
                            LOG_D("glCheckFramebufferStatus: FBO %u attachment 0x%X uses float format %s "
                                  "without required color_buffer extension; promoting",
                                  currentFBO, attPoint, glEnumToString(tex->internal_format));
                            break;
                        }
                    }
                }
            }

            // Path (b): query GLES directly for each color attachment point.
            // This catches attachments that bypassed our tracking (e.g. DSA
            // paths that didn't go through our wrappers, or textures whose
            // internal_format wasn't recorded).
            static const GLenum kColorAttachments[] = {
                GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
                GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5,
                GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7,
            };
            for (GLenum att : kColorAttachments) {
                GLint objType = GL_NONE;
                GLES.glGetFramebufferAttachmentParameteriv(
                    target, att, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &objType);
                if (objType != GL_TEXTURE && objType != GL_RENDERBUFFER) continue;
                GLint compType = GL_NONE;
                GLES.glGetFramebufferAttachmentParameteriv(
                    target, att, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType);
                if (compType == GL_FLOAT) {
                    foundFloatAttachment = true;
                    LOG_D("glCheckFramebufferStatus: FBO %u attachment 0x%X has GL_FLOAT component type "
                          "(queried from GLES); promoting", currentFBO ? currentFBO : 0, att);
                    break;
                }
            }
            // Also check the depth/stencil attachment (some GLES drivers
            // reject float depth formats like GL_DEPTH32F_STENCIL8 here).
            for (GLenum att : {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT, GL_DEPTH_STENCIL_ATTACHMENT}) {
                GLint objType = GL_NONE;
                GLES.glGetFramebufferAttachmentParameteriv(
                    target, att, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &objType);
                if (objType == GL_TEXTURE) {
                    GLint compType = GL_NONE;
                    GLES.glGetFramebufferAttachmentParameteriv(
                        target, att, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &compType);
                    if (compType == GL_FLOAT) {
                        foundFloatAttachment = true;
                        LOG_D("glCheckFramebufferStatus: FBO %u depth/stencil attachment 0x%X has GL_FLOAT "
                              "component type; promoting", currentFBO ? currentFBO : 0, att);
                        break;
                    }
                }
            }

            GLES.glGetError();  // clear any error from the queries above

            // Promote regardless. On desktop GL, INCOMPLETE_ATTACHMENT is
            // virtually never returned for valid configurations; the most
            // common GLES cause is a float color format that lacks the
            // matching color_buffer extension, which desktop GL handles
            // natively. Even if we couldn't confirm the float cause (e.g.
            // the driver doesn't expose COMPONENT_TYPE queries), promoting
            // is safer than letting Iris disable its entire shader pipeline.
            LOG_D("glCheckFramebufferStatus: promoting INCOMPLETE_ATTACHMENT -> COMPLETE "
                  "(target=0x%X, FBO=%u, floatConfirmed=%d)",
                  target, currentFBO, (int)foundFloatAttachment);
            (void)foundFloatAttachment;
            status = GL_FRAMEBUFFER_COMPLETE;
            break;
        }
        default:
            LOG_E("glCheckFramebufferStatus: target=0x%X reports 0x%X (incomplete); returning real status",
                  target, status);
            break;
    }
    return status;
}

// ============================================================================
// glFramebufferParameteri - native passthrough
// ============================================================================

extern "C" GLAPI GLAPIENTRY void glFramebufferParameteri(GLenum target, GLenum pname, GLint param) {
    GLES.glFramebufferParameteri(target, pname, param);
}

// ============================================================================
// glGetFramebufferAttachmentParameteriv - native passthrough
// ============================================================================

extern "C" GLAPI GLAPIENTRY void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params) {
    GLES.glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

// ============================================================================
// glGetFramebufferParameteriv - native passthrough
// ============================================================================

extern "C" GLAPI GLAPIENTRY void glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    GLES.glGetFramebufferParameteriv(target, pname, params);
}

// ============================================================================
// glBlitFramebuffer - native passthrough
// ============================================================================

extern "C" GLAPI GLAPIENTRY void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                       GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                       GLbitfield mask, GLenum filter) {
    // Strip GL_ACCUM_BUFFER_BIT
    mask &= ~0x00000200;
    GLES.glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

// ============================================================================
// glInvalidateFramebuffer / glInvalidateSubFramebuffer - native passthrough
// ============================================================================

extern "C" GLAPI GLAPIENTRY void glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments) {
    GLES.glInvalidateFramebuffer(target, numAttachments, attachments);
}

extern "C" GLAPI GLAPIENTRY void glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments,
                                GLint x, GLint y, GLsizei width, GLsizei height) {
    GLES.glInvalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);
}

// ============================================================================
// Map initialization helpers (for backward compatibility)
// ============================================================================

void InitFramebufferMap(size_t expectedSize) {
    auto &fb = GLState.framebuffer;
    fb.fboMap.reserve(expectedSize);
    fb.fboMapReverse.reserve(expectedSize);
}

// ============================================================================
// Temp FBO pool — for hot-path texture operations (glCopyTexImage2D, etc.)
// ============================================================================

GLuint acquireTempFBO() {
    if (g_tempFboPoolIndex < g_tempFboPool.size()) {
        return g_tempFboPool[g_tempFboPoolIndex++];
    }
    GLuint fbo;
    GLES.glGenFramebuffers(1, &fbo);
    g_tempFboPool.push_back(fbo);
    g_tempFboPoolIndex++;
    return fbo;
}

void releaseTempFBO(GLuint fbo) {
    // Simple: just decrement index. FBO is not deleted, just returned to pool.
    if (g_tempFboPoolIndex > 0) g_tempFboPoolIndex--;
}

void releaseAllTempFBOs() {
    g_tempFboPoolIndex = 0;
}

void cleanupTempFBOs() {
    for (GLuint fbo : g_tempFboPool) {
        GLES.glDeleteFramebuffers(1, &fbo);
    }
    g_tempFboPool.clear();
    g_tempFboPoolIndex = 0;
}