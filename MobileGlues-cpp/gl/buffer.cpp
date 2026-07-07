// MobileGlues - gl/buffer.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

// ============================================================================
// ES 3.2 native → native, ES 3.2 not native → CPU simulation
// ============================================================================

#include "buffer.h"
#include "ankerl/unordered_dense.h"
#include "texture.h"

#define DEBUG 0

// ============================================================================
// Internal State: Buffer Map Management
// ============================================================================

GLuint bound_array;
static GLint maxBufferId = 0;
static GLint maxArrayId = 0;

static std::vector<GLuint> g_gen_buffers;
static std::vector<char> g_gen_buffer_exists;
static std::vector<GLuint> g_free_buffer_ids;

static std::vector<GLuint> g_gen_arrays;
static std::vector<char> g_gen_array_exists;
static std::vector<GLuint> g_free_array_ids;

static std::vector<size_t> g_buffer_datasize;

static std::vector<GLuint> g_element_array_buffer_per_vao;

// ============================================================================
// Buffer Binding Index Tracking
// ============================================================================

enum BindingIndex : int {
    BI_ARRAY_BUFFER = 0,
    BI_ATOMIC_COUNTER,
    BI_COPY_READ,
    BI_COPY_WRITE,
    BI_DRAW_INDIRECT,
    BI_DISPATCH_INDIRECT,
    BI_ELEMENT_ARRAY,
    BI_PIXEL_PACK,
    BI_PIXEL_UNPACK,
    BI_SHADER_STORAGE,
    BI_TRANSFORM_FEEDBACK,
    BI_UNIFORM_BUFFER,
    BINDING_COUNT
};
static std::array<GLuint, BINDING_COUNT> g_bound_buffers_arr = {0};

// ============================================================================
// Buffer Map Helpers: Capacity & Lifecycle
// ============================================================================

static inline int ensure_buffer_capacity(GLuint id) {
    if (id < (GLuint)g_gen_buffers.size()) [[likely]] return 0;
    g_gen_buffers.resize(id + 1, 0);
    g_gen_buffer_exists.resize(id + 1, 0);
    if (g_buffer_datasize.size() <= (size_t)id) g_buffer_datasize.resize(id + 1, 0);
    return 0;
}

static inline int ensure_array_capacity(GLuint id) {
    if (id < (GLuint)g_gen_arrays.size()) [[likely]] return 0;
    g_gen_arrays.resize(id + 1, 0);
    g_gen_array_exists.resize(id + 1, 0);
    if (g_element_array_buffer_per_vao.size() <= (size_t)id) g_element_array_buffer_per_vao.resize(id + 1, 0);
    return 0;
}

GLuint gen_buffer() {
    if (!g_free_buffer_ids.empty()) {
        GLuint id = g_free_buffer_ids.back();
        g_free_buffer_ids.pop_back();
        ensure_buffer_capacity(id);
        g_gen_buffers[id] = 0;
        g_gen_buffer_exists[id] = 1;
        g_buffer_datasize[id] = 0;
        if (id > (GLuint)maxBufferId) maxBufferId = id;
        return id;
    }
    maxBufferId++;
    ensure_buffer_capacity((GLuint)maxBufferId);
    g_gen_buffers[maxBufferId] = 0;
    g_gen_buffer_exists[maxBufferId] = 1;
    g_buffer_datasize[maxBufferId] = 0;
    return (GLuint)maxBufferId;
}

GLboolean has_buffer(GLuint key) {
    return key < g_gen_buffer_exists.size() ? (g_gen_buffer_exists[key] != 0) : 0;
}

void modify_buffer(GLuint key, GLuint value) {
    if (key >= g_gen_buffers.size()) [[unlikely]] ensure_buffer_capacity(key);
    g_gen_buffers[key] = value;
    g_gen_buffer_exists[key] = 1;
}

void remove_buffer(GLuint key) {
    if (key < g_gen_buffer_exists.size() && g_gen_buffer_exists[key]) {
        g_gen_buffer_exists[key] = 0;
        g_gen_buffers[key] = 0;
        if (key < g_buffer_datasize.size()) g_buffer_datasize[key] = 0;
        g_free_buffer_ids.push_back(key);
    }
}

GLuint find_real_buffer(GLuint key) {
    if (key < g_gen_buffers.size() && g_gen_buffer_exists[key]) return g_gen_buffers[key];
    return 0;
}

// Combined lookup: returns {real_buffer, exists} in a single bounds check.
// g_gen_buffers and g_gen_buffer_exists are always resized together,
// so checking one is sufficient for both.
static inline std::pair<GLuint, bool> find_real_buffer_with_exists(GLuint key) {
    if (key < g_gen_buffers.size() && g_gen_buffer_exists[key]) [[likely]]
        return {g_gen_buffers[key], true};
    return {0, false};
}

// Fast-path: direct assignment without capacity checks, for use when the caller
// has already verified the buffer exists (e.g. after find_real_buffer_with_exists).
// Avoids redundant ensure_buffer_capacity in hot paths like glBindBuffer,
// glBindBufferRange, glBindBufferBase, glBindVertexBuffer, glTexBuffer.
static inline void modify_buffer_direct(GLuint key, GLuint value) {
    g_gen_buffers[key] = value;
}

GLuint get_ibo_by_vao(GLuint vao) {
    if (vao < g_element_array_buffer_per_vao.size()) return g_element_array_buffer_per_vao[vao];
    return 0;
}

GLuint find_bound_array() {
    return bound_array;
}

void update_vao_ibo_binding(GLuint vao, GLuint ibo) {
    ensure_array_capacity(vao);
    g_element_array_buffer_per_vao[vao] = ibo;
}

void set_buffer_data_size(GLuint buffer, size_t size) {
    ensure_buffer_capacity(buffer);
    g_buffer_datasize[buffer] = size;
}

size_t get_buffer_data_size(GLuint buffer) {
    if (buffer < g_buffer_datasize.size()) return g_buffer_datasize[buffer];
    return 0;
}

// ============================================================================
// Buffer Binding Helpers
// ============================================================================

static inline int binding_target_to_index(GLenum target) {
    switch (target) {
    case GL_ARRAY_BUFFER:             return BI_ARRAY_BUFFER;
    case GL_ATOMIC_COUNTER_BUFFER:    return BI_ATOMIC_COUNTER;
    case GL_COPY_READ_BUFFER:         return BI_COPY_READ;
    case GL_COPY_WRITE_BUFFER:        return BI_COPY_WRITE;
    case GL_DRAW_INDIRECT_BUFFER:     return BI_DRAW_INDIRECT;
    case GL_DISPATCH_INDIRECT_BUFFER: return BI_DISPATCH_INDIRECT;
    case GL_ELEMENT_ARRAY_BUFFER:     return BI_ELEMENT_ARRAY;
    case GL_PIXEL_PACK_BUFFER:        return BI_PIXEL_PACK;
    case GL_PIXEL_UNPACK_BUFFER:      return BI_PIXEL_UNPACK;
    case GL_SHADER_STORAGE_BUFFER:    return BI_SHADER_STORAGE;
    case GL_TRANSFORM_FEEDBACK_BUFFER:return BI_TRANSFORM_FEEDBACK;
    case GL_UNIFORM_BUFFER:           return BI_UNIFORM_BUFFER;
    default:                          return -1;
    }
}

void set_bound_buffer_by_target(GLenum target, GLuint buffer) {
    int idx = binding_target_to_index(target);
    if (idx >= 0) g_bound_buffers_arr[idx] = buffer;
}

GLuint find_bound_buffer(GLenum key) {
    // Special case: ELEMENT_ARRAY_BUFFER uses VAO tracking
    if (key == GL_ELEMENT_ARRAY_BUFFER_BINDING) {
        return get_ibo_by_vao(find_bound_array());
    }
    switch (key) {
    case GL_ARRAY_BUFFER_BINDING:              return g_bound_buffers_arr[BI_ARRAY_BUFFER];
    case GL_ATOMIC_COUNTER_BUFFER_BINDING:     return g_bound_buffers_arr[BI_ATOMIC_COUNTER];
    case GL_COPY_READ_BUFFER_BINDING:          return g_bound_buffers_arr[BI_COPY_READ];
    case GL_COPY_WRITE_BUFFER_BINDING:         return g_bound_buffers_arr[BI_COPY_WRITE];
    case GL_DRAW_INDIRECT_BUFFER_BINDING:      return g_bound_buffers_arr[BI_DRAW_INDIRECT];
    case GL_DISPATCH_INDIRECT_BUFFER_BINDING:  return g_bound_buffers_arr[BI_DISPATCH_INDIRECT];
    case GL_PIXEL_PACK_BUFFER_BINDING:         return g_bound_buffers_arr[BI_PIXEL_PACK];
    case GL_PIXEL_UNPACK_BUFFER_BINDING:       return g_bound_buffers_arr[BI_PIXEL_UNPACK];
    case GL_SHADER_STORAGE_BUFFER_BINDING:     return g_bound_buffers_arr[BI_SHADER_STORAGE];
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING: return g_bound_buffers_arr[BI_TRANSFORM_FEEDBACK];
    case GL_UNIFORM_BUFFER_BINDING:            return g_bound_buffers_arr[BI_UNIFORM_BUFFER];
    default:                                   return 0;
    }
}

static GLenum get_binding_query(GLenum target) {
    switch (target) {
    case GL_ARRAY_BUFFER:              return GL_ARRAY_BUFFER_BINDING;
    case GL_ELEMENT_ARRAY_BUFFER:      return GL_ELEMENT_ARRAY_BUFFER_BINDING;
    case GL_PIXEL_PACK_BUFFER:         return GL_PIXEL_PACK_BUFFER_BINDING;
    case GL_PIXEL_UNPACK_BUFFER:       return GL_PIXEL_UNPACK_BUFFER_BINDING;
    case GL_UNIFORM_BUFFER:            return GL_UNIFORM_BUFFER_BINDING;
    case GL_TRANSFORM_FEEDBACK_BUFFER: return GL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
    case GL_COPY_READ_BUFFER:          return GL_COPY_READ_BUFFER_BINDING;
    case GL_COPY_WRITE_BUFFER:         return GL_COPY_WRITE_BUFFER_BINDING;
    case GL_DRAW_INDIRECT_BUFFER:      return GL_DRAW_INDIRECT_BUFFER_BINDING;
    case GL_SHADER_STORAGE_BUFFER:     return GL_SHADER_STORAGE_BUFFER_BINDING;
    case GL_DISPATCH_INDIRECT_BUFFER:  return GL_DISPATCH_INDIRECT_BUFFER_BINDING;
    case GL_ATOMIC_COUNTER_BUFFER:     return GL_ATOMIC_COUNTER_BUFFER_BINDING;
    default:                           return 0;
    }
}

// ============================================================================
// Vertex Array Map Helpers
// ============================================================================

GLuint gen_array() {
    if (!g_free_array_ids.empty()) {
        GLuint id = g_free_array_ids.back();
        g_free_array_ids.pop_back();
        ensure_array_capacity(id);
        g_gen_arrays[id] = 0;
        g_gen_array_exists[id] = 1;
        g_element_array_buffer_per_vao[id] = 0;
        if (id > (GLuint)maxArrayId) maxArrayId = id;
        return id;
    }
    maxArrayId++;
    ensure_array_capacity((GLuint)maxArrayId);
    g_gen_arrays[maxArrayId] = 0;
    g_gen_array_exists[maxArrayId] = 1;
    g_element_array_buffer_per_vao[maxArrayId] = 0;
    return (GLuint)maxArrayId;
}

GLboolean has_array(GLuint key) {
    return key < g_gen_array_exists.size() ? (g_gen_array_exists[key] != 0) : 0;
}

void modify_array(GLuint key, GLuint value) {
    if (key >= g_gen_arrays.size()) ensure_array_capacity(key);
    g_gen_arrays[key] = value;
    g_gen_array_exists[key] = 1;
}

void remove_array(GLuint key) {
    if (key < g_gen_array_exists.size() && g_gen_array_exists[key]) {
        g_gen_array_exists[key] = 0;
        g_gen_arrays[key] = 0;
        if (key < g_element_array_buffer_per_vao.size()) g_element_array_buffer_per_vao[key] = 0;
        g_free_array_ids.push_back(key);
    }
}

GLuint find_real_array(GLuint key) {
    if (key < g_gen_arrays.size() && g_gen_array_exists[key]) return g_gen_arrays[key];
    return 0;
}

// ============================================================================
// Map Initialization
// ============================================================================

void InitBufferMap(size_t expectedSize) {
    g_gen_buffers.reserve(expectedSize + 2);
    g_gen_buffer_exists.reserve(expectedSize + 2);
    g_buffer_datasize.reserve(expectedSize + 2);
    g_gen_buffers.resize(1, 0);
    g_gen_buffer_exists.resize(1, 0);
    g_buffer_datasize.resize(1, 0);
}

void InitVertexArrayMap(size_t expectedSize) {
    g_gen_arrays.reserve(expectedSize + 2);
    g_gen_array_exists.reserve(expectedSize + 2);
    g_element_array_buffer_per_vao.reserve(expectedSize + 2);
    g_gen_arrays.resize(1, 0);
    g_gen_array_exists.resize(1, 0);
    g_element_array_buffer_per_vao.resize(1, 0);
}

// ============================================================================
// Buffer Object Lifecycle (ES 3.2 native)
// ============================================================================

void glGenBuffers(GLsizei n, GLuint* buffers) {
    LOG()
    LOG_D("glGenBuffers(%i, %p)", n, buffers)
    for (int i = 0; i < n; ++i) {
        buffers[i] = gen_buffer();
    }
}

void glDeleteBuffers(GLsizei n, const GLuint* buffers) {
    LOG()
    LOG_D("glDeleteBuffers(%i, %p)", n, buffers)
    for (int i = 0; i < n; ++i) {
        auto [real_buff, exists] = find_real_buffer_with_exists(buffers[i]);
        if (exists) {
            GLES.glDeleteBuffers(1, &real_buff);
            CHECK_GL_ERROR
        }
        remove_buffer(buffers[i]);
    }
}

GLboolean glIsBuffer(GLuint buffer) {
    LOG()
    LOG_D("glIsBuffer, buffer = %d", buffer)
    return has_buffer(buffer);
}

// ============================================================================
// Buffer Binding (ES 3.2 native, with buffer map)
// ============================================================================

void glBindBuffer(GLenum target, GLuint buffer) {
    LOG()
    LOG_D("glBindBuffer, target = %s, buffer = %d", glEnumToString(target), buffer)
    set_bound_buffer_by_target(target, buffer);
    // save ibo binding to vao
    if (target == GL_ELEMENT_ARRAY_BUFFER) {
        update_vao_ibo_binding(find_bound_array(), buffer);
    }

    if (buffer == 0) [[unlikely]] {
        GLES.glBindBuffer(target, buffer);
        CHECK_GL_ERROR
        return;
    }
    auto [real_buffer, exists] = find_real_buffer_with_exists(buffer);
    if (!exists) [[unlikely]] {
        GLES.glBindBuffer(target, buffer);
        CHECK_GL_ERROR
        return;
    }
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer_direct(buffer, real_buffer);
        CHECK_GL_ERROR
    }
    LOG_D("glBindBuffer: %d -> %d", buffer, real_buffer)
    GLES.glBindBuffer(target, real_buffer);
    CHECK_GL_ERROR
}

// ============================================================================
// Buffer Data Upload (ES 3.2 native, with buffer map)
// ============================================================================

void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    LOG()
    LOG_D("glBufferData, target = %s, size = %d, data = 0x%x, usage = %s", glEnumToString(target), size, data,
          glEnumToString(usage))
    GLES.glBufferData(target, size, data, usage);
    int idx = binding_target_to_index(target);
    if (idx >= 0) set_buffer_data_size(g_bound_buffers_arr[idx], size);
    CHECK_GL_ERROR
}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    LOG()
    LOG_D("glBufferSubData, target = %s, offset = %d, size = %d, data = %p", glEnumToString(target), offset, size, data)
    GLES.glBufferSubData(target, offset, size, data);
    CHECK_GL_ERROR
}

// ============================================================================
// Buffer Mapping (ES 3.2 native)
// ============================================================================

void* glMapBuffer(GLenum target, GLenum access) {
    LOG()
    LOG_D("glMapBuffer, target = %s, access = %s", glEnumToString(target), glEnumToString(access))
    if (g_gles_caps.GL_OES_mapbuffer) {
        return GLES.glMapBufferOES(target, access);
    }
    GLint buffer_size;
    GLES.glGetBufferParameteriv(target, GL_BUFFER_SIZE, &buffer_size);
    if (buffer_size <= 0) {
        return nullptr;
    }
    GLbitfield flags = 0;
    switch (access) {
    case GL_READ_ONLY:  flags = GL_MAP_READ_BIT;                              break;
    case GL_WRITE_ONLY: flags = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT; break;
    case GL_READ_WRITE: flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;            break;
    default:            return nullptr;
    }
    void* ptr = glMapBufferRange(target, 0, buffer_size, flags);
    return ptr;
}

void* glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    LOG()
    if (global_settings.buffer_coherent_as_flush) access &= ~GL_MAP_FLUSH_EXPLICIT_BIT;
    return GLES.glMapBufferRange(target, offset, length, access);
}

GLboolean glUnmapBuffer(GLenum target) {
    LOG()
    LOG_D("%s(%s)", __func__, glEnumToString(target));
    if (g_gles_caps.GL_OES_mapbuffer) return GLES.glUnmapBuffer(target);

    GLboolean result = GLES.glUnmapBuffer(target);
    CHECK_GL_ERROR
    return result;
}

void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    LOG()
    if (!global_settings.buffer_coherent_as_flush) GLES.glFlushMappedBufferRange(target, offset, length);
}

// ============================================================================
// Buffer Range / Base Binding (ES 3.2 native, with buffer map)
// ============================================================================

struct atomic_buffer {
    GLuint id;
    GLsizeiptr size;
    GLintptr offset;
};

static std::vector<atomic_buffer> g_buffer_map_atomic_buffer_info;
static std::vector<GLuint> g_buffer_map_ssbo_id;

void bindAllAtomicCounterAsSSBO() {
    const size_t count = g_buffer_map_atomic_buffer_info.size();
    for (size_t i = 0; i < count; ++i) {
        atomic_buffer buf = g_buffer_map_atomic_buffer_info[i];
        if (buf.id != 0) {
            GLuint realID = find_real_buffer(buf.id);
            GLES.glBindBufferRange(GL_SHADER_STORAGE_BUFFER, i, realID, buf.offset, buf.size);
            LOG_D("Bound atomic counter buffer %u(real: %u) as SSBO at index %zu", buf, realID, i);
        }
    }
}

void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    LOG()
    LOG_D("glBindBufferRange, target = %s, index = %d, buffer = %d, offset = %p, size = %zi", glEnumToString(target),
          index, buffer, (void*)offset, size)

    if (buffer == 0) {
        GLES.glBindBufferRange(target, index, buffer, offset, size);
        CHECK_GL_ERROR
        return;
    }
    auto [real_buffer, exists] = find_real_buffer_with_exists(buffer);
    if (!exists) {
        GLES.glBindBufferRange(target, index, buffer, offset, size);
        CHECK_GL_ERROR
        return;
    }
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer_direct(buffer, real_buffer);
        CHECK_GL_ERROR
    }
    GLES.glBindBufferRange(target, index, real_buffer, offset, size);
    if (target == GL_ATOMIC_COUNTER_BUFFER) {
        if (g_buffer_map_atomic_buffer_info.empty()) {
            g_buffer_map_atomic_buffer_info.resize(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, {});
        }
        g_buffer_map_atomic_buffer_info[index] = {buffer, size, offset};
    }
    CHECK_GL_ERROR
}

void glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    LOG()
    LOG_D("glBindBufferBase, target = %s, index = %d, buffer = %d", glEnumToString(target), index, buffer)

    if (buffer == 0) {
        GLES.glBindBufferBase(target, index, buffer);
        CHECK_GL_ERROR
        return;
    }
    auto [real_buffer, exists] = find_real_buffer_with_exists(buffer);
    if (!exists) {
        GLES.glBindBufferBase(target, index, buffer);
        CHECK_GL_ERROR
        return;
    }
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer_direct(buffer, real_buffer);
        CHECK_GL_ERROR
    }
    GLES.glBindBufferBase(target, index, real_buffer);
    if (target == GL_SHADER_STORAGE_BUFFER) {
        if (g_buffer_map_ssbo_id.empty()) {
            g_buffer_map_ssbo_id.resize(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, 0);
        }
        g_buffer_map_ssbo_id[index] = buffer;
    }
    CHECK_GL_ERROR
}

// ============================================================================
// Vertex Buffer Binding (ES 3.2 native, with buffer map)
// ============================================================================

void glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    LOG()
    LOG_D("glBindVertexBuffer, bindingindex = %d, buffer = %d, offset = %p, stride = %i", bindingindex, buffer, offset,
          stride)
    if (buffer == 0) [[unlikely]] {
        GLES.glBindVertexBuffer(bindingindex, buffer, offset, stride);
        CHECK_GL_ERROR
        return;
    }
    auto [real_buffer, exists] = find_real_buffer_with_exists(buffer);
    if (!exists) [[unlikely]] {
        GLES.glBindVertexBuffer(bindingindex, buffer, offset, stride);
        CHECK_GL_ERROR
        return;
    }
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer_direct(buffer, real_buffer);
        CHECK_GL_ERROR
    }
    GLES.glBindVertexBuffer(bindingindex, real_buffer, offset, stride);
    CHECK_GL_ERROR
}

// ============================================================================
// Buffer Copy (ES 3.2 native)
// ============================================================================

void glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    LOG()
    LOG_D("glCopyBufferSubData, readTarget = %s, writeTarget = %s, readOffset = %d, writeOffset = %d, size = %d",
          glEnumToString(readTarget), glEnumToString(writeTarget), readOffset, writeOffset, size)
    GLES.glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
    CHECK_GL_ERROR
}

// ============================================================================
// Buffer Parameter Queries (ES 3.2 native)
// ============================================================================

void glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    LOG()
    LOG_D("glGetBufferParameteriv, target = %s, pname = %s", glEnumToString(target), glEnumToString(pname))
    GLES.glGetBufferParameteriv(target, pname, params);
    CHECK_GL_ERROR
}

void glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64* params) {
    LOG()
    LOG_D("glGetBufferParameteri64v, target = %s, pname = %s", glEnumToString(target), glEnumToString(pname))
    GLES.glGetBufferParameteri64v(target, pname, params);
    CHECK_GL_ERROR
}

void glGetBufferPointerv(GLenum target, GLenum pname, void** params) {
    LOG()
    LOG_D("glGetBufferPointerv, target = %s, pname = %s", glEnumToString(target), glEnumToString(pname))
    GLES.glGetBufferPointerv(target, pname, params);
    CHECK_GL_ERROR
}

// ============================================================================
// Buffer Storage (ES 3.2 native via EXT)
// ============================================================================

void glBufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags) {
    LOG()
    if (GLES.glBufferStorageEXT) {
        if (global_settings.buffer_coherent_as_flush &&
            ((flags & GL_MAP_PERSISTENT_BIT) != 0 || (flags & GL_DYNAMIC_STORAGE_BIT) != 0))
            flags |= (GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT);
        GLES.glBufferStorageEXT(target, size, data, flags);
    }
    CHECK_GL_ERROR
}

// ============================================================================
// Texture Buffer Objects (ES 3.2 native)
// ============================================================================
//
// TBO attaches a buffer object's data store to a texture buffer target.
// Virtual buffer IDs are resolved to real GPU buffer IDs via the buffer map.
// On ES 3.2, glTexBuffer/glTexBufferRange are natively supported.
//
// Safety: runtime function-pointer check — if the driver does not expose
// glTexBuffer (e.g. incomplete ES 3.2 implementation), we log an error
// and return gracefully rather than crashing.
// ============================================================================

static bool tbo_available() {
    static bool checked = false;
    static bool avail = false;
    if (!checked) {
        checked = true;
        if (GLES.glTexBuffer == nullptr) {
            LOG_E("glTexBuffer is not available on this driver — TBO unsupported");
        } else {
            avail = true;
        }
    }
    return avail;
}

void glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer) {
    LOG()
    LOG_D("glTexBuffer, target = %s, internalformat = %s, buffer = %d", glEnumToString(target),
          glEnumToString(internalformat), buffer)
    if (target != GL_TEXTURE_BUFFER) return;
    if (!tbo_available()) return;

    GLuint real_buffer = buffer;
    if (has_buffer(buffer) && buffer != 0) {
        real_buffer = find_real_buffer(buffer);
        if (!real_buffer) {
            GLES.glGenBuffers(1, &real_buffer);
            modify_buffer_direct(buffer, real_buffer);
            CHECK_GL_ERROR
        }
    }
    GLES.glTexBuffer(target, internalformat, real_buffer);
    CHECK_GL_ERROR
}

void glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    LOG()
    LOG_D("glTexBufferRange, target = %s, internalformat = %s, buffer = %d, offset = %p, size = %zi",
          glEnumToString(target), glEnumToString(internalformat), buffer, (void*)offset, size)
    if (!tbo_available()) return;

    GLuint real_buffer = buffer;
    if (has_buffer(buffer) && buffer != 0) {
        real_buffer = find_real_buffer(buffer);
        if (!real_buffer) {
            GLES.glGenBuffers(1, &real_buffer);
            modify_buffer_direct(buffer, real_buffer);
            CHECK_GL_ERROR
        }
    }
    GLES.glTexBufferRange(target, internalformat, real_buffer, offset, size);
    CHECK_GL_ERROR
}

// ============================================================================
// Vertex Array Object Lifecycle (ES 3.2 native, with VAO map)
// ============================================================================

void glGenVertexArrays(GLsizei n, GLuint* arrays) {
    LOG()
    LOG_D("glGenVertexArrays(%i, %p)", n, arrays)
    for (int i = 0; i < n; ++i) {
        arrays[i] = gen_array();
    }
}

void glDeleteVertexArrays(GLsizei n, const GLuint* arrays) {
    LOG()
    LOG_D("glDeleteVertexArrays(%i, %p)", n, arrays)
    for (int i = 0; i < n; ++i) {
        if (has_array(arrays[i]) && arrays[i] != 0) {
            GLuint real_array = find_real_array(arrays[i]);
            if (real_array) {
                GLES.glDeleteVertexArrays(1, &real_array);
                CHECK_GL_ERROR
            }
        }
        remove_array(arrays[i]);
    }
}

GLboolean glIsVertexArray(GLuint array) {
    LOG()
    LOG_D("glIsVertexArray(%d)", array)
    return has_array(array);
}

void glBindVertexArray(GLuint array) {
    LOG()
    LOG_D("glBindVertexArray(%d)", array)

    bound_array = array;

    // update bound ibo
    set_bound_buffer_by_target(GL_ELEMENT_ARRAY_BUFFER, get_ibo_by_vao(array));

    if (!has_array(array) || array == 0) [[unlikely]] {
        LOG_D("Does not have va=%d found!", array)
        GLES.glBindVertexArray(array);
        CHECK_GL_ERROR
        return;
    }

    GLuint real_array = find_real_array(array);
    if (!real_array) {
        LOG_D("va=%d not initialized, initializing...", array)
        GLES.glGenVertexArrays(1, &real_array);
        modify_array(array, real_array);
        CHECK_GL_ERROR
    }
    LOG_D("glBindVertexArray: %d -> %d", array, real_array)
    GLES.glBindVertexArray(real_array);
    CHECK_GL_ERROR
}

// ============================================================================
// ARB Aliases
// ============================================================================

#if GLOBAL_DEBUG || DEBUG
#include <fstream>
#define BIN_FILE_PREFIX "/sdcard/MG/buf/"
#endif

extern "C"
{
    GLAPI GLAPIENTRY void* glMapBufferARB(GLenum target, GLenum access) __attribute__((alias("glMapBuffer")));
    GLAPI GLAPIENTRY void glBufferDataARB(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
        __attribute__((alias("glBufferData")));
    GLAPI GLAPIENTRY GLboolean glUnmapBufferARB(GLenum target) __attribute__((alias("glUnmapBuffer")));
    GLAPI GLAPIENTRY void glBufferStorageARB(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags)
        __attribute__((alias("glBufferStorage")));
    GLAPI GLAPIENTRY void glBindBufferARB(GLenum target, GLuint buffer) __attribute__((alias("glBindBuffer")));
}