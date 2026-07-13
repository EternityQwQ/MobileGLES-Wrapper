// MobileGlues - gl/buffer.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#ifndef MOBILEGLUES_BUFFER_H
#define GL_GLEXT_PROTOTYPES
#include "../config/settings.h"
#include "../gles/loader.h"
#include "../includes.h"
#include "glcorearb.h"
#include "log.h"
#include "mg.h"
#include <GL/gl.h>
#include <cstddef>
#include <vector>

#ifdef __cplusplus
extern "C"
{
#endif

    GLuint gen_buffer();

    GLboolean has_buffer(GLuint key);

    void modify_buffer(GLuint key, GLuint value);

    void remove_buffer(GLuint key);

    GLuint find_real_buffer(GLuint key);

    GLuint find_bound_buffer(GLenum key);

    void set_bound_buffer_by_target(GLenum target, GLuint buffer);

    // --- PBO CPU shadow data (for BGRA swizzle without glMapBufferRange read) ---
    // MobileGL-DirectGLES keeps a CPU shadow copy of every PBO; we adopt the
    // same design but only for GL_PIXEL_UNPACK_BUFFER. This lets us do CPU-side
    // BGRA->RGBA swizzle in glTexSubImage2D/glTexImage2D without mapping the
    // source PBO for reading (which fails on many GLES drivers).
    void pbo_shadow_alloc(GLuint pbo, GLsizeiptr size, const void* data);
    void pbo_shadow_subdata(GLuint pbo, GLintptr offset, GLsizeiptr size, const void* data);
    void pbo_shadow_delete(GLuint pbo);
    // Returns a pointer to the PBO's CPU shadow data, or nullptr if none.
    // The pointer is valid until the next PBO operation on this buffer.
    const unsigned char* pbo_shadow_get(GLuint pbo);
    // Returns the size of the PBO's CPU shadow data, or 0 if none.
    GLsizeiptr pbo_shadow_size(GLuint pbo);
    // For glMapBufferRange(GL_MAP_WRITE_BIT): returns a writable CPU pointer
    // into the shadow buffer at `offset`. Caller must call pbo_shadow_unmap
    // to flush the shadow back to GLES.
    void* pbo_shadow_map_write(GLuint pbo, GLintptr offset, GLsizeiptr length);
    void pbo_shadow_unmap(GLuint pbo);

    GLuint find_bound_ssbo_indexed(GLuint index);

    GLuint gen_array();

    GLboolean has_array(GLuint key);

    void modify_array(GLuint key, GLuint value);

    void remove_array(GLuint key);

    GLuint find_real_array(GLuint key);

    GLuint find_bound_array();

    static GLenum get_binding_query(GLenum target);

    void InitBufferMap(size_t expectedSize);

    void InitVertexArrayMap(size_t expectedSize);

    GLAPI GLAPIENTRY void glGenBuffers(GLsizei n, GLuint* buffers);

    GLAPI GLAPIENTRY void glDeleteBuffers(GLsizei n, const GLuint* buffers);

    GLAPI GLAPIENTRY GLboolean glIsBuffer(GLuint buffer);

    GLAPI GLAPIENTRY void glBindBuffer(GLenum target, GLuint buffer);

    GLAPI GLAPIENTRY void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                                            GLsizeiptr size);

    GLAPI GLAPIENTRY void glBindBufferBase(GLenum target, GLuint index, GLuint buffer);

    GLAPI GLAPIENTRY void glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);

    GLAPI GLAPIENTRY void glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer);

    GLAPI GLAPIENTRY void glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset,
                                           GLsizeiptr size);

    GLAPI GLAPIENTRY GLboolean glUnmapBuffer(GLenum target);

    GLAPI GLAPIENTRY void* glMapBuffer(GLenum target, GLenum access);

    GLAPI GLAPIENTRY void* glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);

    GLAPI GLAPIENTRY void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage);

    GLAPI GLAPIENTRY void glBufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags);

    GLAPI GLAPIENTRY void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length);

    GLAPI GLAPIENTRY void glGenVertexArrays(GLsizei n, GLuint* arrays);

    GLAPI GLAPIENTRY void glDeleteVertexArrays(GLsizei n, const GLuint* arrays);

    GLAPI GLAPIENTRY GLboolean glIsVertexArray(GLuint array);

    GLAPI GLAPIENTRY void glBindVertexArray(GLuint array);

#ifdef __cplusplus
}
#endif

#define MOBILEGLUES_BUFFER_H

#endif // MOBILEGLUES_BUFFER_H
