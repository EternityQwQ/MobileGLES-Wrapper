// MobileGlues - gl/mg.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include <cstdarg>
#include <unistd.h>
#include "mg.h"

#define DEBUG 0

hardware_t hardware;
gl_state_t gl_state;

FUNC_GL_STATE_SIZEI(proxy_width)
FUNC_GL_STATE_SIZEI(proxy_height)
FUNC_GL_STATE_ENUM(proxy_intformat)
FUNC_GL_STATE_UINT(current_program)
FUNC_GL_STATE_UINT(current_tex_unit)
FUNC_GL_STATE_UINT(current_draw_fbo)

// Blend state
FUNC_GL_STATE_ENUM(blend_src_rgb)
FUNC_GL_STATE_ENUM(blend_dst_rgb)
FUNC_GL_STATE_ENUM(blend_src_alpha)
FUNC_GL_STATE_ENUM(blend_dst_alpha)
FUNC_GL_STATE_ENUM(blend_equation_rgb)
FUNC_GL_STATE_ENUM(blend_equation_alpha)
FUNC_GL_STATE_FLOAT4(blend_color)

// Depth state
FUNC_GL_STATE_ENUM(depth_func)
FUNC_GL_STATE_BOOL(depth_mask)

// Stencil state
FUNC_GL_STATE_ENUM(stencil_func_front)
FUNC_GL_STATE_ENUM(stencil_func_back)
FUNC_GL_STATE_ENUM(stencil_fail_front)
FUNC_GL_STATE_ENUM(stencil_zfail_front)
FUNC_GL_STATE_ENUM(stencil_zpass_front)
FUNC_GL_STATE_ENUM(stencil_fail_back)
FUNC_GL_STATE_ENUM(stencil_zfail_back)
FUNC_GL_STATE_ENUM(stencil_zpass_back)

// Cull face
FUNC_GL_STATE_ENUM(cull_face_mode)
FUNC_GL_STATE_ENUM(front_face)

// ============================================================================
// Enable/Disable Cap Tracking
// ============================================================================
// Hash function: maps GLenum cap → bit index (0..63) in enable_caps_mask.
// Most common caps are mapped directly; others use a small hash.
// Returns -1 if the cap is not tracked (should fall through to GLES).
// ============================================================================

static int cap_to_bit_index(GLenum cap) {
    // Direct mapping for the most common caps (fast path)
    switch (cap) {
    case GL_BLEND:              return 0;
    case GL_DEPTH_TEST:         return 1;
    case GL_STENCIL_TEST:       return 2;
    case GL_CULL_FACE:          return 3;
    case GL_SCISSOR_TEST:       return 4;
    case GL_DITHER:             return 5;
    case GL_POLYGON_OFFSET_FILL:return 6;
    case GL_SAMPLE_ALPHA_TO_COVERAGE: return 7;
    case GL_SAMPLE_COVERAGE:    return 8;
    case GL_PRIMITIVE_RESTART_FIXED_INDEX: return 9;
    case GL_RASTERIZER_DISCARD: return 10;
    case GL_FRAMEBUFFER_SRGB:   return 11;
    default:                    return -1; // Not tracked — fall through to GLES
    }
}

void gl_state_track_enable(GLenum cap) {
    int idx = cap_to_bit_index(cap);
    if (idx >= 0) [[likely]] {
        gl_state->enable_caps_mask |= (uint64_t(1) << idx);
    }
}

void gl_state_track_disable(GLenum cap) {
    int idx = cap_to_bit_index(cap);
    if (idx >= 0) [[likely]] {
        gl_state->enable_caps_mask &= ~(uint64_t(1) << idx);
    }
}

GLboolean gl_state_is_enabled(GLenum cap) {
    int idx = cap_to_bit_index(cap);
    if (idx >= 0) [[likely]] {
        return (gl_state->enable_caps_mask & (uint64_t(1) << idx)) ? GL_TRUE : GL_FALSE;
    }
    // Fallback: query GLES for untracked caps
    return GLES.glIsEnabled(cap);
}

UnorderedMap<GLuint, bool> program_map_is_sampler_buffer_emulated;
UnorderedMap<GLuint, bool> program_map_is_atomic_counter_emulated;

FILE* file;

void start_log() {
    file = fopen(log_file_path, "a");
}

void write_log(const char* format, ...) {
    if (file == nullptr) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fprintf(file, "\n");
    fflush(file);
#if FORCE_SYNC_WITH_LOG_FILE == 1
    int fd = fileno(file);
    fsync(fd);
#endif
}

void write_log_n(const char* format, ...) {
    if (file == NULL) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fflush(file);
}

void clear_log() {
    file = fopen(log_file_path, "w");
    if (file == nullptr) {
        return;
    }
    fclose(file);
}

GLenum pname_convert(GLenum pname) {
    switch (pname) {
    // TODO: Realize GL_TEXTURE_LOD_BIAS for other devices.
    case GL_TEXTURE_LOD_BIAS:
        return GL_TEXTURE_LOD_BIAS_QCOM;
    }
    return pname;
}

GLenum map_tex_target(GLenum target) {
    switch (target) {
    case GL_TEXTURE_1D:
    case GL_TEXTURE_RECTANGLE_ARB:
        return GL_TEXTURE_2D;

    case GL_PROXY_TEXTURE_1D:
    case GL_PROXY_TEXTURE_RECTANGLE_ARB:
        return GL_PROXY_TEXTURE_2D;

    default:
        return target;
    }
}