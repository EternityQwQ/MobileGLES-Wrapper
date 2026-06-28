// MobileGlues - gl/mg.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#ifndef MOBILEGLUES_MG_H
#define MOBILEGLUES_MG_H

typedef unsigned int uint;

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <malloc.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include <GL/gl.h>
#include "../gles/gles.h"
#include "log.h"
#include "../gles/loader.h"
#include "../includes.h"
#include "glsl/glsl_for_es.h"
#include "../config/config.h"

#ifdef __cplusplus
extern "C"
{
#endif

// ============================================================================
// MobileGlues Architecture for OpenGL ES 3.2
//
// Rule:
//  - OpenGL ES 3.2 natively supports: Direct pass-through (no CPU emulation)
//  - OpenGL ES 3.2 does NOT support: CPU emulation / stub
// ============================================================================

// Display list emulation state
struct display_list_s {
    bool is_compiled;
    void* commands;  // TODO: command buffer
    size_t cmd_size;
};

#define FUNC_GL_STATE_SIZEI(name)                                                                                      \
    void set_gl_state_##name(GLsizei value) {                                                                          \
        gl_state->name = value;                                                                                        \
        LOG_D(" -> gl_state: %s is %d", #name, value);                                                                 \
    }
#define FUNC_GL_STATE_ENUM(name)                                                                                       \
    void set_gl_state_##name(GLenum value) {                                                                           \
        gl_state->name = value;                                                                                        \
        LOG_D(" -> gl_state: %s is %d", #name, value);                                                                 \
    }
#define FUNC_GL_STATE_UINT(name)                                                                                       \
    void set_gl_state_##name(GLuint value) {                                                                           \
        gl_state->name = value;                                                                                        \
        LOG_D(" -> gl_state: %s is %d", #name, value);                                                                 \
    }
#define FUNC_GL_STATE_BOOL(name)                                                                                       \
    void set_gl_state_##name(GLboolean value) {                                                                        \
        gl_state->name = value;                                                                                        \
        LOG_D(" -> gl_state: %s is %d", #name, (int)value);                                                            \
    }
#define FUNC_GL_STATE_FLOAT4(name)                                                                                     \
    void set_gl_state_##name(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {                                            \
        gl_state->name[0] = r; gl_state->name[1] = g; gl_state->name[2] = b; gl_state->name[3] = a;                   \
        LOG_D(" -> gl_state: %s is {%f,%f,%f,%f}", #name, r, g, b, a);                                                \
    }
#define FUNC_GL_STATE_SIZEI_DECLARATION(name) void set_gl_state_##name(GLsizei value);
#define FUNC_GL_STATE_ENUM_DECLARATION(name) void set_gl_state_##name(GLenum value);
#define FUNC_GL_STATE_UINT_DECLARATION(name) void set_gl_state_##name(GLuint value);
#define FUNC_GL_STATE_BOOL_DECLARATION(name) void set_gl_state_##name(GLboolean value);
#define FUNC_GL_STATE_FLOAT4_DECLARATION(name) void set_gl_state_##name(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

    FUNC_GL_STATE_SIZEI_DECLARATION(proxy_width)
    FUNC_GL_STATE_SIZEI_DECLARATION(proxy_height)
    FUNC_GL_STATE_ENUM_DECLARATION(proxy_intformat)
    FUNC_GL_STATE_UINT_DECLARATION(current_program)
    FUNC_GL_STATE_UINT_DECLARATION(current_tex_unit)
    FUNC_GL_STATE_UINT_DECLARATION(current_draw_fbo)

    // ============================================================================
    // State tracking: enable/disable caps
    // ============================================================================
    // Bitmask for fast CPU-side glIsEnabled. Each bit corresponds to a tracked cap.
    // Caps are hashed to 0..63 range via a simple mapping function.
    // We track up to 64 distinct caps in a 64-bit bitmask.
    void gl_state_track_enable(GLenum cap);
    void gl_state_track_disable(GLenum cap);
    GLboolean gl_state_is_enabled(GLenum cap);

    // ============================================================================
    // State tracking: blend state (for redundant-call prevention)
    // ============================================================================
    FUNC_GL_STATE_ENUM_DECLARATION(blend_src_rgb)
    FUNC_GL_STATE_ENUM_DECLARATION(blend_dst_rgb)
    FUNC_GL_STATE_ENUM_DECLARATION(blend_src_alpha)
    FUNC_GL_STATE_ENUM_DECLARATION(blend_dst_alpha)
    FUNC_GL_STATE_ENUM_DECLARATION(blend_equation_rgb)
    FUNC_GL_STATE_ENUM_DECLARATION(blend_equation_alpha)
    FUNC_GL_STATE_FLOAT4_DECLARATION(blend_color)

    // ============================================================================
    // State tracking: depth state
    // ============================================================================
    FUNC_GL_STATE_ENUM_DECLARATION(depth_func)
    FUNC_GL_STATE_BOOL_DECLARATION(depth_mask)

    // ============================================================================
    // State tracking: stencil state
    // ============================================================================
    FUNC_GL_STATE_ENUM_DECLARATION(stencil_func_front)
    FUNC_GL_STATE_ENUM_DECLARATION(stencil_func_back)
    FUNC_GL_STATE_ENUM_DECLARATION(stencil_fail_front)
    FUNC_GL_STATE_ENUM_DECLARATION(stencil_zfail_front)
    FUNC_GL_STATE_ENUM_DECLARATION(stencil_zpass_front)
    FUNC_GL_STATE_ENUM_DECLARATION(stencil_fail_back)
    FUNC_GL_STATE_ENUM_DECLARATION(stencil_zfail_back)
    FUNC_GL_STATE_ENUM_DECLARATION(stencil_zpass_back)

    // ============================================================================
    // State tracking: cull face
    // ============================================================================
    FUNC_GL_STATE_ENUM_DECLARATION(cull_face_mode)
    FUNC_GL_STATE_ENUM_DECLARATION(front_face)

    struct hardware_s {
        unsigned int es_version;      // ES version in hundreds (e.g. 320 = 3.2)
        bool emulate_texture_buffer;  // Always false on ES 3.2 (native support)
    };
    typedef struct hardware_s* hardware_t;
    extern hardware_t hardware;

    struct gl_state_s {
        GLsizei proxy_width;
        GLsizei proxy_height;
        GLenum proxy_intformat;

        GLuint current_program;
        GLuint current_tex_unit;
        GLuint current_draw_fbo;

        // Enable/disable caps bitmask (64 bits, tracks up to 64 distinct caps)
        uint64_t enable_caps_mask;

        // Blend state
        GLenum blend_src_rgb;
        GLenum blend_dst_rgb;
        GLenum blend_src_alpha;
        GLenum blend_dst_alpha;
        GLenum blend_equation_rgb;
        GLenum blend_equation_alpha;
        GLfloat blend_color[4];

        // Depth state
        GLenum depth_func;
        GLboolean depth_mask;

        // Stencil state
        GLenum stencil_func_front;
        GLenum stencil_func_back;
        GLenum stencil_fail_front;
        GLenum stencil_zfail_front;
        GLenum stencil_zpass_front;
        GLenum stencil_fail_back;
        GLenum stencil_zfail_back;
        GLenum stencil_zpass_back;

        // Cull face state
        GLenum cull_face_mode;
        GLenum front_face;
    };
    typedef struct gl_state_s* gl_state_t;
    extern gl_state_t gl_state;

    GLenum pname_convert(GLenum pname);
    GLenum map_tex_target(GLenum target);
    void start_log();
    void write_log(const char* format, ...);
    void write_log_n(const char* format, ...);
    void clear_log();

#ifdef __cplusplus
}
#endif

void prepareForDrawImpl();

// Inline check avoids function call overhead on the hot draw path
// when emulate_texture_buffer is false (the default ES 3.2 case).
#define PREPARE_FOR_DRAW() do { if (hardware->emulate_texture_buffer) [[unlikely]] prepareForDrawImpl(); } while(0)

#endif // MOBILEGLUES_MG_H
