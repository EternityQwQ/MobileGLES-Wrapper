// MobileGlues - gl/mg.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

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

// ============================================================================
// CPU Emulation for features not natively supported by OpenGL ES 3.2
// ============================================================================

// Identity matrix
static const float identity_matrix[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

static void matrix_stack_init(matrix_stack_s* s, int max_depth) {
    s->max_depth = max_depth;
    s->stack = (float*)malloc(max_depth * 16 * sizeof(float));
    s->depth = 0;
    memcpy(s->stack, identity_matrix, 16 * sizeof(float));
}

static void matrix_stack_free(matrix_stack_s* s) {
    if (s->stack) {
        free(s->stack);
        s->stack = nullptr;
    }
}

static void matrix_stack_push(matrix_stack_s* s) {
    if (s->depth < s->max_depth - 1) {
        s->depth++;
        memcpy(s->stack + s->depth * 16, s->stack + (s->depth - 1) * 16, 16 * sizeof(float));
    }
}

static void matrix_stack_pop(matrix_stack_s* s) {
    if (s->depth > 0) s->depth--;
}

static float* matrix_stack_top(matrix_stack_s* s) {
    return s->stack + s->depth * 16;
}

static void matrix_stack_multiply(matrix_stack_s* s, const float* m) {
    float* top = matrix_stack_top(s);
    float result[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result[i * 4 + j] = top[0 * 4 + j] * m[i * 4 + 0] +
                                top[1 * 4 + j] * m[i * 4 + 1] +
                                top[2 * 4 + j] * m[i * 4 + 2] +
                                top[3 * 4 + j] * m[i * 4 + 3];
        }
    }
    memcpy(top, result, 16 * sizeof(float));
}

void init_emulation_state() {
    memset(&gl_state->immediate, 0, sizeof(immediate_state_s));
    gl_state->immediate.in_begin = false;

    memset(&gl_state->matrix, 0, sizeof(matrix_mode_state_s));
    gl_state->matrix.current_mode = GL_MODELVIEW;
    matrix_stack_init(&gl_state->matrix.modelview, MAX_MATRIX_STACK);
    matrix_stack_init(&gl_state->matrix.projection, MAX_MATRIX_STACK);
    matrix_stack_init(&gl_state->matrix.texture, MAX_MATRIX_STACK);
    memcpy(gl_state->matrix.current_matrix, identity_matrix, 16 * sizeof(float));
}

void flush_immediate() {
    if (!gl_state->immediate.in_begin || gl_state->immediate.vertex_count == 0) return;

    // Immediate mode is not supported by GLES 3.2 - use client-side arrays
    GLuint vbo = 0, cbo = 0, tbo = 0, nbo = 0;
    GLES.glGenBuffers(1, &vbo);
    GLES.glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLES.glBufferData(GL_ARRAY_BUFFER, gl_state->immediate.vertex_count * 4 * sizeof(float),
                      gl_state->immediate.vertices, GL_STREAM_DRAW);
    GLES.glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    GLES.glEnableVertexAttribArray(0);

    bool has_color = false;
    for (int i = 0; i < gl_state->immediate.vertex_count * 4; i++) {
        if (gl_state->immediate.colors[i] != 0.0f) { has_color = true; break; }
    }
    if (has_color) {
        GLES.glGenBuffers(1, &cbo);
        GLES.glBindBuffer(GL_ARRAY_BUFFER, cbo);
        GLES.glBufferData(GL_ARRAY_BUFFER, gl_state->immediate.vertex_count * 4 * sizeof(float),
                          gl_state->immediate.colors, GL_STREAM_DRAW);
        GLES.glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
        GLES.glEnableVertexAttribArray(1);
    }

    GLES.glDrawArrays(gl_state->immediate.begin_mode, 0, gl_state->immediate.vertex_count);

    GLES.glDisableVertexAttribArray(0);
    if (has_color) GLES.glDisableVertexAttribArray(1);
    GLES.glDeleteBuffers(1, &vbo);
    if (has_color) GLES.glDeleteBuffers(1, &cbo);

    gl_state->immediate.vertex_count = 0;
}