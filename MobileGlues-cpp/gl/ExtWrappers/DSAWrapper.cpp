// MobileGlues - gl/ExtWrappers/DSAWrapper.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
//
// Direct State Access emulated on GLES 3.2.
//
// The whole file rests on one idea: GLES has no DSA, so to touch an object
// that is not currently bound we bind it, call the classic entry point, and
// put the previous binding back. Everything else here is bookkeeping to make
// that cheap and correct:
//
//   * The "what is currently bound?" question is answered on the CPU. The
//     gl/ stack already tracks every binding it forwards to GLES, and those
//     trackers are the single source of truth for this layer too. That
//     removes the glGetIntegerv() round trips the old implementation did on
//     the renderbuffer / sampler / pipeline / XFB paths, each of which stalls
//     the pipeline.
//   * The save/restore logic lives in two small templates instead of five
//     near-identical hand-written stack implementations.
//   * Because this layer binds through GL_ARRAY_BUFFER rather than
//     GL_PIXEL_UNPACK_BUFFER, the PBO CPU shadow that glBufferData and
//     friends maintain internally does not fire. The buffer entry points
//     therefore update the shadow explicitly; see the comments there.
//
// Observable behaviour is unchanged: the same entry points, same signatures,
// same effects. A handful of latent bugs in the previous revision are fixed
// and called out inline with `FIX:`.

#include "DSAWrapper.h"
#include <cassert>
#include "../texture.h"
#include "../framebuffer.h"
#include "../buffer.h"

#define DEBUG 0

// ============================================================================
// Target → binding query
// ============================================================================

// Maps a GL target to the enum glGetIntegerv() expects for that target's
// binding, or 0 when the target carries no binding.
GLenum dsa::QueryForTarget(GLenum target, bool textureBinding) {
    switch (target) {
        // --- buffer targets (GLES 3.2 set) ---
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
    case GL_QUERY_BUFFER:              return GL_QUERY_BUFFER_BINDING;
    case GL_ATOMIC_COUNTER_BUFFER:     return GL_ATOMIC_COUNTER_BUFFER_BINDING;
        // GL_TEXTURE_BUFFER is overloaded: as a *texture* target its binding
        // is GL_TEXTURE_BINDING_BUFFER, as a *buffer* target it is
        // GL_TEXTURE_BUFFER_BINDING. Callers say which one they mean.
    case GL_TEXTURE_BUFFER:            return textureBinding ? GL_TEXTURE_BINDING_BUFFER : GL_TEXTURE_BUFFER_BINDING;
        // --- other binding-bearing targets ---
    case GL_VERTEX_ARRAY:              return GL_VERTEX_ARRAY_BINDING;
    case GL_PROGRAM_PIPELINE:          return GL_PROGRAM_PIPELINE_BINDING;
    case GL_SAMPLER:                   return GL_SAMPLER_BINDING;
    case GL_FRAMEBUFFER:               return GL_FRAMEBUFFER_BINDING;
    case GL_READ_FRAMEBUFFER:          return GL_READ_FRAMEBUFFER_BINDING;
    case GL_DRAW_FRAMEBUFFER:          return GL_DRAW_FRAMEBUFFER_BINDING;
    case GL_RENDERBUFFER:              return GL_RENDERBUFFER_BINDING;
    case GL_TRANSFORM_FEEDBACK:        return GL_TRANSFORM_FEEDBACK_BINDING;
    default:                           return 0;
    }
}

// ============================================================================
// One generic temporary-binding stack for every object family
// ============================================================================

namespace
{
    // Returns true when `target` names a texture target, i.e. one that must be
    // bound with glBindTexture() rather than glBindBuffer().
    inline bool IsTextureTarget(GLenum target) {
        switch (target) {
        case GL_TEXTURE_1D:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_1D_ARRAY:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_RECTANGLE:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
        case GL_TEXTURE_2D_MULTISAMPLE:
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        case GL_TEXTURE_BUFFER:
            return true;
        default:
            return false;
        }
    }

    // Issues the bind for `target`. One switch instead of five, so the
    // save/restore path below is written exactly once.
    void BindTargetNow(GLenum target, GLuint object) {
        switch (target) {
        case GL_FRAMEBUFFER:
        case GL_READ_FRAMEBUFFER:
        case GL_DRAW_FRAMEBUFFER:
            glBindFramebuffer(target, object);
            return;
        case GL_VERTEX_ARRAY:
            glBindVertexArray(object);
            return;
        case GL_RENDERBUFFER:
            glBindRenderbuffer(GL_RENDERBUFFER, object);
            return;
        case GL_PROGRAM_PIPELINE:
            glBindProgramPipeline(object);
            return;
        case GL_TRANSFORM_FEEDBACK:
            glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, object);
            return;
        default:
            if (IsTextureTarget(target)) glBindTexture(target, object);
            else glBindBuffer(target, object);
            return;
        }
    }

    // Sentinel meaning "the binding was already correct, nothing to restore".
    constexpr GLuint kNoRestore = static_cast<GLuint>(-1);

    // Per-target restore stacks. Thread-local because the GL context is
    // current to one thread; a stack because DSA calls nest (glBindTextureUnit
    // inside glTextureStorage2D, for example) and must unwind in order.
    thread_local ankerl::unordered_dense::map<GLenum, std::vector<GLuint>> g_bindingStack;

    // Generic push/pop. `current` is the CPU-tracked binding for `target`.
    void PushTempBinding(GLenum target, GLuint object, GLuint current) {
        if (current == object) {
            g_bindingStack[target].push_back(kNoRestore);
            return;
        }
        g_bindingStack[target].push_back(current);
        LOG_D("[DSA] [TempBind] target=0x%X, prev=%u -> bind=%u", target, current, object);
        CHECK_GL_ERROR;
        BindTargetNow(target, object);
        CHECK_GL_ERROR_NO_INIT;
    }

    void PopTempBinding(GLenum target) {
        auto it = g_bindingStack.find(target);
        if (it == g_bindingStack.end() || it->second.empty()) {
            LOG_D("[DSA] [Restore] no saved binding for target 0x%X", target);
            return;
        }

        const GLuint toRestore = it->second.back();
        it->second.pop_back();
        if (it->second.empty()) g_bindingStack.erase(it);

        if (toRestore == kNoRestore) {
            LOG_D("[DSA] [Restore] target=0x%X, binding already correct", target);
            return;
        }

        LOG_D("[DSA] [Restore] target=0x%X, bind back to %u", target, toRestore);
        CHECK_GL_ERROR;
        BindTargetNow(target, toRestore);
        CHECK_GL_ERROR_NO_INIT;
    }

    // --- Object-family helpers ---------------------------------------------
    // Each answers "what is bound to this target right now?" from CPU state
    // and hands the answer to the generic stack.

    void PushBuffer(GLuint buffer, GLenum target = GL_ARRAY_BUFFER) {
        const GLenum query = dsa::QueryForTarget(target);
        PushTempBinding(target, buffer, query ? find_bound_buffer(query) : 0);
    }
    void PopBuffer(GLenum target = GL_ARRAY_BUFFER) { PopTempBinding(target); }

    void PushFramebuffer(GLuint fbo, GLenum target = GL_DRAW_FRAMEBUFFER) {
        PushTempBinding(target, fbo, dsa::CurrentBinding(target));
    }
    void PopFramebuffer(GLenum target = GL_DRAW_FRAMEBUFFER) { PopTempBinding(target); }

    void PushRenderbuffer(GLuint rbo) {
        PushTempBinding(GL_RENDERBUFFER, rbo, dsa::CurrentBinding(GL_RENDERBUFFER));
    }
    void PopRenderbuffer() { PopTempBinding(GL_RENDERBUFFER); }

    void PushTexture(GLuint texture, GLenum target) {
        // Previous binding for `target` on the *current* texture unit. The
        // texture tracker is per-unit, which is what we want: the binding we
        // displace must be restored on the same unit we displaced it from.
        GLuint prev = 0;
        if (auto* obj = mgGetTexObjectByTarget(target)) prev = obj->texture;
        PushTempBinding(target, texture, prev);
    }
    void PopTexture(GLenum target) { PopTempBinding(target); }

    void PushVertexArray(GLuint vao) {
        PushTempBinding(GL_VERTEX_ARRAY, vao, dsa::CurrentBinding(GL_VERTEX_ARRAY));
    }
    void PopVertexArray() { PopTempBinding(GL_VERTEX_ARRAY); }

    void PushXFB(GLuint xfb) {
        PushTempBinding(GL_TRANSFORM_FEEDBACK, xfb, dsa::CurrentBinding(GL_TRANSFORM_FEEDBACK));
    }
    void PopXFB() { PopTempBinding(GL_TRANSFORM_FEEDBACK); }

    // --- Texture target resolution -----------------------------------------

    GLenum GetTexTarget(GLuint texture) {
        auto* obj = mgGetTexObjectByID(texture);
        if (!obj) return GL_TEXTURE_2D;
        return ConvertTextureTargetToGLEnum(obj->target);
    }

    // RAII wrapper for the "temporarily bind this object / call / restore"
    // pattern. Prefer this over the manual Push/Pop pair where possible: it
    // survives early returns and keeps the restore adjacent to the bind.
    struct TempBind {
        enum class Kind { Buffer, Framebuffer, Renderbuffer, Texture, VertexArray, XFB } kind;
        GLenum target;

        TempBind(Kind k, GLenum t) : kind(k), target(t) {}

        // Buffer / framebuffer / renderbuffer / VAO / XFB
        TempBind(Kind k, GLuint object, GLenum t) : kind(k), target(t) {
            switch (k) {
            case Kind::Buffer:      PushBuffer(object, t); break;
            case Kind::Framebuffer: PushFramebuffer(object, t); break;
            case Kind::Renderbuffer:PushRenderbuffer(object); break;
            case Kind::VertexArray: PushVertexArray(object); break;
            case Kind::XFB:         PushXFB(object); break;
            default: break;
            }
        }

        // Texture (target resolved by the caller via GetTexTarget)
        static TempBind Texture(GLuint texture, GLenum t) {
            TempBind b(Kind::Texture, t);
            PushTexture(texture, t);
            return b;
        }

        ~TempBind() {
            switch (kind) {
            case Kind::Buffer:      PopBuffer(target); break;
            case Kind::Framebuffer: PopFramebuffer(target); break;
            case Kind::Renderbuffer:PopRenderbuffer(); break;
            case Kind::Texture:     PopTexture(target); break;
            case Kind::VertexArray: PopVertexArray(); break;
            case Kind::XFB:         PopXFB(); break;
            }
        }

        TempBind(const TempBind&) = delete;
        TempBind& operator=(const TempBind&) = delete;
        TempBind(TempBind&& o) noexcept : kind(o.kind), target(o.target) { o.kind = Kind::Buffer; }
    };

    // The variable name is uniquified per line so several temporary binds can
    // coexist in one scope (glBlitNamedFramebuffer binds both endpoints,
    // glInvalidateNamedFramebufferData binds read and draw, ...). The inner
    // CAT/NAME indirection is what makes __LINE__ expand before pasting.
#define DSA_CAT_(a, b) a##b
#define DSA_CAT(a, b) DSA_CAT_(a, b)
#define DSA_BIND_NAME DSA_CAT(dsaBind, __LINE__)

#define DSA_TEMP_BUFFER(buf)      TempBind DSA_BIND_NAME(TempBind::Kind::Buffer, (buf), GL_ARRAY_BUFFER)
#define DSA_TEMP_BUFFER_T(b, t)   TempBind DSA_BIND_NAME(TempBind::Kind::Buffer, (b), (t))
#define DSA_TEMP_FBO(f)           TempBind DSA_BIND_NAME(TempBind::Kind::Framebuffer, (f), GL_DRAW_FRAMEBUFFER)
#define DSA_TEMP_FBO_T(f, t)      TempBind DSA_BIND_NAME(TempBind::Kind::Framebuffer, (f), (t))
#define DSA_TEMP_RBO(r)           TempBind DSA_BIND_NAME(TempBind::Kind::Renderbuffer, (r), GL_RENDERBUFFER)
#define DSA_TEMP_VAO(v)           TempBind DSA_BIND_NAME(TempBind::Kind::VertexArray, (v), GL_VERTEX_ARRAY)
#define DSA_TEMP_XFB(x)           TempBind DSA_BIND_NAME(TempBind::Kind::XFB, (x), GL_TRANSFORM_FEEDBACK)

    // Texture ops: resolve the target once, bind it, run the body, restore.
    // `target` is introduced into the *enclosing* scope so the body can pass
    // it straight through, which is why this has to be a macro rather than a
    // function or a plain RAII type.
#define TEXTURE_OP_FUNC_BEGIN(func_name)                                                                               \
    LOG()                                                                                                              \
    LOG_D("[DSA] " #func_name ", texture: %u", texture);                                                               \
    const GLenum target = GetTexTarget(texture);                                                                       \
    TempBind _dsaTexBind = TempBind::Texture(texture, target);

#define TEXTURE_OP_FUNC_END                                                                                            \
    CHECK_GL_ERROR;

} // namespace

// ============================================================================
// Buffer objects
// ============================================================================

void glCreateBuffers(GLsizei n, GLuint* buffers) {
    LOG()
    LOG_D("[DSA] glCreateBuffers, n: %d, buffers: %p", n, buffers);

    if (n <= 0 || !buffers) {
        LOG_W("[DSA] Invalid parameters for glCreateBuffers");
        return;
    }

    // GL 4.5 creates buffers *and* initialises them to an empty immutable
    // store, which is what makes glNamedBufferData/SubData work without a
    // prior Storage call. GLES's glGenBuffers does not create the driver-side
    // object until it is first bound, so bind each one to materialise it.
    for (GLsizei i = 0; i < n; ++i) {
        GLuint bufID = 0;
        glGenBuffers(1, &bufID);
        if (bufID == 0) {
            LOG_W("[DSA] Failed to create buffer at index %d", i);
            continue;
        }

        {
            DSA_TEMP_BUFFER(bufID);
        }
        CHECK_GL_ERROR;
        buffers[i] = bufID;
    }

    LOG_D("[DSA] Created %d buffers successfully", n);
}

void glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void* data, GLbitfield flags) {
    LOG()
    LOG_D("[DSA] glNamedBufferStorage, buffer: %u, size: %lld, data: %p, flags: %u", buffer, size, data, flags);

    if (buffer == 0 || size <= 0) {
        LOG_W("[DSA] Invalid parameters for glNamedBufferStorage");
        return;
    }

    {
        DSA_TEMP_BUFFER(buffer);
        glBufferStorage(GL_ARRAY_BUFFER, size, data, flags);
    }
    CHECK_GL_ERROR;

    // The DSA path binds through GL_ARRAY_BUFFER, so the PBO shadow update
    // inside glBufferStorage() (which only fires for GL_PIXEL_UNPACK_BUFFER)
    // is skipped. Update the shadow here by buffer ID so that a later
    // glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer) + glTexSubImage2D can read
    // the CPU copy directly instead of falling back to glCopyBufferSubData,
    // which may be incomplete on the first frame and produce a brief colour
    // glitch until the shadow is lazily established by a later map.
    pbo_shadow_alloc(buffer, size, data);

    LOG_D("[DSA] Buffer %u stored with size %lld", buffer, size);
}

void glNamedBufferData(GLuint buffer, GLsizeiptr size, const void* data, GLenum usage) {
    LOG()
    LOG_D("[DSA] glNamedBufferData, buffer: %u, size: %lld, data: %p, usage: %u", buffer, size, data, usage);

    if (buffer == 0 || size <= 0) {
        LOG_W("[DSA] Invalid parameters for glNamedBufferData");
        return;
    }

    {
        DSA_TEMP_BUFFER(buffer);
        glBufferData(GL_ARRAY_BUFFER, size, data, usage);
    }
    CHECK_GL_ERROR;
    // Mirror glBufferData's PBO shadow sync; see glNamedBufferStorage.
    pbo_shadow_alloc(buffer, size, data);

    LOG_D("[DSA] Buffer %u data set with size %lld", buffer, size);
}

void glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data) {
    LOG()
    LOG_D("[DSA] glNamedBufferSubData, buffer: %u, offset: %lld, size: %lld, data: %p", buffer, offset, size, data);

    if (buffer == 0 || size <= 0 || offset < 0) {
        LOG_W("[DSA] Invalid parameters for glNamedBufferSubData");
        return;
    }

    {
        DSA_TEMP_BUFFER(buffer);
        glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
    }
    CHECK_GL_ERROR;
    // Mirror glBufferSubData's PBO shadow sync; see glNamedBufferStorage.
    pbo_shadow_subdata(buffer, offset, size, data);

    LOG_D("[DSA] Buffer %u sub-data set with size %lld at offset %lld", buffer, size, offset);
}

void glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset,
                              GLsizeiptr size) {
    LOG()
    LOG_D("[DSA] glCopyNamedBufferSubData, readBuffer: %u, writeBuffer: %u, readOffset: %lld, writeOffset: %lld, size: "
          "%lld",
          readBuffer, writeBuffer, readOffset, writeOffset, size);

    if (readBuffer == 0 || writeBuffer == 0 || size <= 0 || readOffset < 0 || writeOffset < 0) {
        LOG_W("[DSA] Invalid parameters for glCopyNamedBufferSubData");
        return;
    }

    {
        DSA_TEMP_BUFFER_T(readBuffer, GL_COPY_READ_BUFFER);
        DSA_TEMP_BUFFER_T(writeBuffer, GL_COPY_WRITE_BUFFER);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, readOffset, writeOffset, size);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Copied %lld bytes from buffer %u to buffer %u", size, readBuffer, writeBuffer);
}

void glClearNamedBufferData(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void* data) {
    LOG()
    LOG_D("[DSA] glClearNamedBufferData, buffer: %u, internalformat: 0x%X, format: 0x%X, type: 0x%X, data: %p", buffer,
          internalformat, format, type, data);

    if (buffer == 0) {
        LOG_W("[DSA] Invalid buffer ID for glClearNamedBufferData");
        return;
    }

    {
        DSA_TEMP_BUFFER(buffer);
        glClearBufferData(GL_ARRAY_BUFFER, internalformat, format, type, data);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Cleared buffer %u with specified data", buffer);
}

void glClearNamedBufferSubData(GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format,
                               GLenum type, const void* data) {
    LOG()
    LOG_D("[DSA] glClearNamedBufferSubData, buffer: %u, internalformat: 0x%X, offset: %lld, size: %lld, format: 0x%X, "
          "type: 0x%X, data: %p",
          buffer, internalformat, offset, size, format, type, data);

    if (buffer == 0 || size <= 0 || offset < 0) {
        LOG_W("[DSA] Invalid parameters for glClearNamedBufferSubData");
        return;
    }

    {
        DSA_TEMP_BUFFER(buffer);
        glClearBufferSubData(GL_ARRAY_BUFFER, internalformat, offset, size, format, type, data);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Cleared sub-data of buffer %u with size %lld at offset %lld", buffer, size, offset);
}

void* glMapNamedBuffer(GLuint buffer, GLenum access) {
    LOG()
    LOG_D("[DSA] glMapNamedBuffer, buffer: %u, access: 0x%X", buffer, access);

    if (buffer == 0) {
        LOG_W("[DSA] Invalid buffer ID for glMapNamedBuffer");
        return nullptr;
    }

    // PBO shadow path: if this buffer has a CPU shadow (created by
    // glNamedBufferData/glNamedBufferStorage), redirect write maps to the
    // shadow so a subsequent glTexSubImage2D BGRA swizzle reads correct data.
    // Without this, DSA-mapped PBO writes bypass the shadow entirely (the
    // non-DSA glMapBufferRange shadow path only triggers for
    // GL_PIXEL_UNPACK_BUFFER), leaving the shadow stale and producing edge
    // colour blocks when the application updates individual tiles via DSA.
    // Existence and size come from one locked lookup rather than two.
    GLsizeiptr sz = 0;
    const unsigned char* shadowP = pbo_shadow_get_ptr_size(buffer, &sz);
    if (shadowP && (access == GL_WRITE_ONLY || access == GL_READ_WRITE) && sz > 0) {
        void* shadowPtr = pbo_shadow_map_write(buffer, 0, sz);
        if (shadowPtr) return shadowPtr;
    }

    void* mappedData = nullptr;
    {
        DSA_TEMP_BUFFER(buffer);
        mappedData = glMapBuffer(GL_ARRAY_BUFFER, access);
    }
    CHECK_GL_ERROR;

    if (!mappedData) {
        LOG_W("[DSA] Failed to map buffer %u", buffer);
    } else {
        LOG_D("[DSA] Mapped buffer %u successfully", buffer);
    }
    return mappedData;
}

GLvoid* glMapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    LOG()
    LOG_D("[DSA] glMapNamedBufferRange, buffer: %u, offset: %lld, length: %lld, access: 0x%X", buffer, offset, length,
          access);

    if (buffer == 0 || length <= 0 || offset < 0) {
        LOG_W("[DSA] Invalid parameters for glMapNamedBufferRange");
        return nullptr;
    }

    // PBO shadow path: redirect write maps to the CPU shadow so the BGRA
    // swizzle in texture.cpp reads up-to-date data. See glMapNamedBuffer.
    if (pbo_shadow_get(buffer) && (access & GL_MAP_WRITE_BIT)) {
        void* shadowPtr = pbo_shadow_map_write(buffer, offset, length);
        if (shadowPtr) return shadowPtr;
    }

    void* mappedData = nullptr;
    {
        DSA_TEMP_BUFFER(buffer);
        mappedData = glMapBufferRange(GL_ARRAY_BUFFER, offset, length, access);
    }
    CHECK_GL_ERROR;

    if (!mappedData) {
        LOG_W("[DSA] Failed to map buffer range for buffer %u", buffer);
    } else {
        LOG_D("[DSA] Mapped buffer range for buffer %u successfully", buffer);
    }
    return mappedData;
}

GLboolean glUnmapNamedBuffer(GLuint buffer) {
    LOG()
    LOG_D("[DSA] glUnmapNamedBuffer, buffer: %u", buffer);

    if (buffer == 0) {
        LOG_W("[DSA] Invalid buffer ID for glUnmapNamedBuffer");
        return GL_FALSE;
    }

    // PBO shadow path: when a DSA map returned a CPU shadow pointer the GLES
    // buffer was never actually mapped, so the real unmap must not be called
    // and the dirty region has to be pushed back via glBufferSubData. Mirror
    // the non-DSA glUnmapBuffer hot path: capture the mapped [offset, length)
    // and clear the mapped flag in a single locked lookup, then upload only
    // the mapped range rather than the whole shadow. A later glTexSubImage2D
    // reads the CPU shadow directly, so narrowing the upload costs nothing on
    // the swizzle path and avoids pushing untouched bytes for a small mapped
    // slice of a large PBO (the minimap-tile-update case).
    const unsigned char* shadowBase = nullptr;
    GLintptr mapOffset = 0;
    GLsizeiptr mapLength = 0;
    if (pbo_shadow_unmap_and_get_range(buffer, &shadowBase, &mapOffset, &mapLength)) {
        if (shadowBase && mapLength > 0) {
            DSA_TEMP_BUFFER(buffer);
            GLES.glBufferSubData(GL_ARRAY_BUFFER, mapOffset, mapLength, shadowBase + mapOffset);
        }
        CHECK_GL_ERROR;
        return GL_TRUE;
    }
    // A shadow exists (created by a DSA alloc or a glMapNamedBuffer* on this
    // buffer) but is not currently write-mapped. Ownership of the mapping
    // lifecycle still belongs to the shadow, so the real unmap is skipped;
    // there is nothing dirty to sync.
    if (pbo_shadow_get(buffer)) return GL_TRUE;

    GLboolean result = GL_FALSE;
    {
        DSA_TEMP_BUFFER(buffer);
        result = glUnmapBuffer(GL_ARRAY_BUFFER);
    }
    CHECK_GL_ERROR;

    if (result == GL_FALSE) {
        LOG_W("[DSA] Failed to unmap buffer %u", buffer);
    } else {
        LOG_D("[DSA] Unmapped buffer %u successfully", buffer);
    }
    return result;
}

void glFlushMappedNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length) {
    LOG()
    LOG_D("[DSA] glFlushMappedNamedBufferRange, buffer: %u, offset: %lld, length: %lld", buffer, offset, length);

    if (buffer == 0 || length <= 0 || offset < 0) {
        LOG_W("[DSA] Invalid parameters for glFlushMappedNamedBufferRange");
        return;
    }

    // PBO shadow path: flush the mapped region from the shadow to GLES so a
    // glTexSubImage2D can read the updated data even before unmap. The shadow
    // pointer is read once rather than once per property.
    GLsizeiptr shadowSz = 0;
    const unsigned char* shadowPtr = pbo_shadow_get_ptr_size(buffer, &shadowSz);
    if (shadowPtr) {
        DSA_TEMP_BUFFER(buffer);
        GLES.glBufferSubData(GL_ARRAY_BUFFER, offset, length, shadowPtr + offset);
        CHECK_GL_ERROR;
        return;
    }

    {
        DSA_TEMP_BUFFER(buffer);
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, offset, length);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Flushed mapped range of buffer %u from offset %lld with length %lld", buffer, offset, length);
}

void glGetNamedBufferParameteriv(GLuint buffer, GLenum pname, GLint* params) {
    LOG()
    LOG_D("[DSA] glGetNamedBufferParameteriv, buffer: %u, pname: 0x%X, params: %p", buffer, pname, params);

    if (buffer == 0 || !params) {
        LOG_W("[DSA] Invalid parameters for glGetNamedBufferParameteriv");
        return;
    }

    {
        DSA_TEMP_BUFFER(buffer);
        glGetBufferParameteriv(GL_ARRAY_BUFFER, pname, params);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Retrieved buffer parameter 0x%X for buffer %u", pname, buffer);
}

void glGetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64* params) {
    LOG()
    LOG_D("[DSA] glGetNamedBufferParameteri64v, buffer: %u, pname: 0x%X, params: %p", buffer, pname, params);

    if (buffer == 0 || !params) {
        LOG_W("[DSA] Invalid parameters for glGetNamedBufferParameteri64v");
        return;
    }

    {
        DSA_TEMP_BUFFER(buffer);
        glGetBufferParameteri64v(GL_ARRAY_BUFFER, pname, params);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Retrieved 64-bit buffer parameter 0x%X for buffer %u", pname, buffer);
}

void glGetNamedBufferPointerv(GLuint buffer, GLenum pname, void** params) {
    LOG()
    LOG_D("[DSA] glGetNamedBufferPointerv, buffer: %u, pname: 0x%X, params: %p", buffer, pname, params);

    if (buffer == 0 || !params) {
        LOG_W("[DSA] Invalid parameters for glGetNamedBufferPointerv");
        return;
    }

    {
        DSA_TEMP_BUFFER(buffer);
        glGetBufferPointerv(GL_ARRAY_BUFFER, pname, params);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Retrieved buffer pointer parameter 0x%X for buffer %u", pname, buffer);
}

void glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, void* data) {
    LOG()
    LOG_D("[DSA] glGetNamedBufferSubData, buffer: %u, offset: %lld, size: %lld, data: %p", buffer, offset, size, data);

    if (buffer == 0 || size <= 0 || offset < 0 || !data) {
        LOG_W("[DSA] Invalid parameters for glGetNamedBufferSubData");
        return;
    }

    {
        DSA_TEMP_BUFFER(buffer);
        glGetBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Retrieved sub-data from buffer %u with size %lld at offset %lld", buffer, size, offset);
}

// ============================================================================
// Framebuffer objects
// ============================================================================

void glCreateFramebuffers(GLsizei n, GLuint* framebuffers) {
    LOG()
    LOG_D("[DSA] glCreateFramebuffers, n: %d, framebuffers: %p", n, framebuffers);

    if (n <= 0 || !framebuffers) {
        LOG_W("[DSA] Invalid parameters for glCreateFramebuffers");
        return;
    }

    for (GLsizei i = 0; i < n; ++i) {
        GLuint fboID = 0;
        glGenFramebuffers(1, &fboID);
        if (fboID == 0) {
            LOG_W("[DSA] Failed to create framebuffer at index %d", i);
            continue;
        }
        {
            DSA_TEMP_FBO(fboID);
        }
        CHECK_GL_ERROR;
        framebuffers[i] = fboID;
    }

    LOG_D("[DSA] Created %d framebuffers successfully", n);
}

void glNamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget,
                                    GLuint renderbuffer) {
    LOG()
    LOG_D("[DSA] glNamedFramebufferRenderbuffer, framebuffer: %u, attachment: 0x%X, renderbuffertarget: 0x%X, "
          "renderbuffer: %u",
          framebuffer, attachment, renderbuffertarget, renderbuffer);

    {
        DSA_TEMP_FBO(framebuffer);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, attachment, renderbuffertarget, renderbuffer);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Attached renderbuffer %u to framebuffer %u with attachment 0x%X", renderbuffer, framebuffer,
          attachment);
}

void glNamedFramebufferParameteri(GLuint framebuffer, GLenum pname, GLint param) {
    LOG()
    LOG_D("[DSA] glNamedFramebufferParameteri, framebuffer: %u, pname: 0x%X, param: %d", framebuffer, pname, param);

    {
        DSA_TEMP_FBO(framebuffer);
        glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, pname, param);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Set framebuffer parameter 0x%X to %d for framebuffer %u", pname, param, framebuffer);
}

void glNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) {
    LOG()
    LOG_D("[DSA] glNamedFramebufferTexture, framebuffer: %u, attachment: 0x%X, texture: %u, level: %d", framebuffer,
          attachment, texture, level);

    {
        DSA_TEMP_FBO(framebuffer);
        glFramebufferTexture(GL_DRAW_FRAMEBUFFER, attachment, texture, level);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Attached texture %u to framebuffer %u with attachment 0x%X at level %d", texture, framebuffer,
          attachment, level);
}

void glNamedFramebufferTextureLayer(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    LOG()
    LOG_D("[DSA] glNamedFramebufferTextureLayer, framebuffer: %u, attachment: 0x%X, texture: %u, level: %d, layer: %d",
          framebuffer, attachment, texture, level, layer);

    {
        DSA_TEMP_FBO(framebuffer);
        glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attachment, texture, level, layer);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Attached texture %u to framebuffer %u with attachment 0x%X at level %d and layer %d", texture,
          framebuffer, attachment, level, layer);
}

void glNamedFramebufferDrawBuffer(GLuint framebuffer, GLenum mode) {
    LOG()
    LOG_D("[DSA] glNamedFramebufferDrawBuffer, framebuffer: %u, mode: 0x%X", framebuffer, mode);

    {
        DSA_TEMP_FBO(framebuffer);
        glDrawBuffer(mode);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Set draw buffer mode 0x%X for framebuffer %u", mode, framebuffer);
}

void glNamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum* bufs) {
    LOG()
    LOG_D("[DSA] glNamedFramebufferDrawBuffers, framebuffer: %u, n: %d, bufs: %p", framebuffer, n, bufs);

    if (n <= 0 || !bufs) {
        LOG_W("[DSA] Invalid parameters for glNamedFramebufferDrawBuffers");
        return;
    }

    {
        DSA_TEMP_FBO(framebuffer);
        glDrawBuffers(n, bufs);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Set %d draw buffers for framebuffer %u", n, framebuffer);
}

void glNamedFramebufferReadBuffer(GLuint framebuffer, GLenum mode) {
    LOG()
    LOG_D("[DSA] glNamedFramebufferReadBuffer, framebuffer: %u, mode: 0x%X", framebuffer, mode);

    {
        DSA_TEMP_FBO_T(framebuffer, GL_READ_FRAMEBUFFER);
        glReadBuffer(mode);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Set read buffer mode 0x%X for framebuffer %u", mode, framebuffer);
}

void glInvalidateNamedFramebufferData(GLuint framebuffer, GLsizei numAttachments, const GLenum* attachments) {
    LOG()
    LOG_D("[DSA] glInvalidateNamedFramebufferData, framebuffer: %u, numAttachments: %d, attachments: %p", framebuffer,
          numAttachments, attachments);

    if (numAttachments <= 0 || !attachments) {
        LOG_W("[DSA] Invalid parameters for glInvalidateNamedFramebufferData");
        return;
    }

    // glInvalidateFramebuffer takes GL_FRAMEBUFFER, so both attachment points
    // must name the same object.
    {
        DSA_TEMP_FBO_T(framebuffer, GL_READ_FRAMEBUFFER);
        DSA_TEMP_FBO(framebuffer);
        glInvalidateFramebuffer(GL_FRAMEBUFFER, numAttachments, attachments);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Invalidated framebuffer %u with %d attachments", framebuffer, numAttachments);
}

void glInvalidateNamedFramebufferSubData(GLuint framebuffer, GLsizei numAttachments, const GLenum* attachments, GLint x,
                                         GLint y, GLsizei width, GLsizei height) {
    LOG()
    LOG_D("[DSA] glInvalidateNamedFramebufferSubData, framebuffer: %u, numAttachments: %d, attachments: %p, x: %d, y: "
          "%d, width: %d, height: %d",
          framebuffer, numAttachments, attachments, x, y, width, height);

    if (numAttachments <= 0 || !attachments || width <= 0 || height <= 0) {
        LOG_W("[DSA] Invalid parameters for glInvalidateNamedFramebufferSubData");
        return;
    }

    {
        DSA_TEMP_FBO_T(framebuffer, GL_READ_FRAMEBUFFER);
        DSA_TEMP_FBO(framebuffer);
        glInvalidateSubFramebuffer(GL_FRAMEBUFFER, numAttachments, attachments, x, y, width, height);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Invalidated sub-data of framebuffer %u with %d attachments at (%d, %d) with size (%d, %d)",
          framebuffer, numAttachments, x, y, width, height);
}

void glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint* value) {
    LOG()
    LOG_D("[DSA] glClearNamedFramebufferiv, framebuffer: %u, buffer: 0x%X, drawbuffer: %d, value: %p", framebuffer,
          buffer, drawbuffer, value);

    if (!value) {
        LOG_W("[DSA] Invalid parameters for glClearNamedFramebufferiv");
        return;
    }

    {
        DSA_TEMP_FBO(framebuffer);
        glClearBufferiv(buffer, drawbuffer, value);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Cleared framebuffer %u with buffer 0x%X at drawbuffer %d", framebuffer, buffer, drawbuffer);
}

void glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint* value) {
    LOG()
    LOG_D("[DSA] glClearNamedFramebufferuiv, framebuffer: %u, buffer: 0x%X, drawbuffer: %d, value: %p", framebuffer,
          buffer, drawbuffer, value);

    if (!value) {
        LOG_W("[DSA] Invalid parameters for glClearNamedFramebufferuiv");
        return;
    }

    {
        DSA_TEMP_FBO(framebuffer);
        glClearBufferuiv(buffer, drawbuffer, value);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Cleared framebuffer %u with unsigned int buffer 0x%X at drawbuffer %d", framebuffer, buffer,
          drawbuffer);
}

void glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat* value) {
    LOG()
    LOG_D("[DSA] glClearNamedFramebufferfv, framebuffer: %u, buffer: 0x%X, drawbuffer: %d, value: %p", framebuffer,
          buffer, drawbuffer, value);

    if (!value) {
        LOG_W("[DSA] Invalid parameters for glClearNamedFramebufferfv");
        return;
    }

    {
        DSA_TEMP_FBO(framebuffer);
        glClearBufferfv(buffer, drawbuffer, value);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Cleared framebuffer %u with float buffer 0x%X at drawbuffer %d", framebuffer, buffer, drawbuffer);
}

void glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    LOG()
    LOG_D("[DSA] glClearNamedFramebufferfi, framebuffer: %u, buffer: 0x%X, drawbuffer: %d, depth: %f, stencil: %d",
          framebuffer, buffer, drawbuffer, depth, stencil);

    {
        DSA_TEMP_FBO(framebuffer);
        glClearBufferfi(buffer, drawbuffer, depth, stencil);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Cleared framebuffer %u with float and int buffer 0x%X at drawbuffer %d", framebuffer, buffer,
          drawbuffer);
}

void glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1,
                            GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask,
                            GLenum filter) {
    LOG()
    LOG_D("[DSA] glBlitNamedFramebuffer, readFramebuffer: %u, drawFramebuffer: %u, src: (%d, %d) to (%d, %d), dst: "
          "(%d, %d) to (%d, %d), mask: 0x%X, filter: 0x%X",
          readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);

    {
        DSA_TEMP_FBO_T(readFramebuffer, GL_READ_FRAMEBUFFER);
        DSA_TEMP_FBO(drawFramebuffer);
        glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Blitted from framebuffer %u to framebuffer %u", readFramebuffer, drawFramebuffer);
}

GLenum glCheckNamedFramebufferStatus(GLuint framebuffer, GLenum target) {
    LOG()
    LOG_D("[DSA] glCheckNamedFramebufferStatus, framebuffer: %u, target: 0x%X", framebuffer, target);

    GLenum status = GL_FRAMEBUFFER_COMPLETE;
    {
        DSA_TEMP_FBO_T(framebuffer, target);
        status = glCheckFramebufferStatus(target);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Checked framebuffer %u status: 0x%X", framebuffer, status);
    return status;
}

void glGetNamedFramebufferParameteriv(GLuint framebuffer, GLenum pname, GLint* param) {
    LOG()
    LOG_D("[DSA] glGetNamedFramebufferParameteriv, framebuffer: %u, pname: 0x%X, param: %p", framebuffer, pname, param);

    if (!param) {
        LOG_W("[DSA] Invalid parameters for glGetNamedFramebufferParameteriv");
        return;
    }

    {
        DSA_TEMP_FBO(framebuffer);
        glGetFramebufferParameteriv(GL_DRAW_FRAMEBUFFER, pname, param);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Retrieved framebuffer parameter 0x%X for framebuffer %u", pname, framebuffer);
}

void glGetNamedFramebufferAttachmentParameteriv(GLuint framebuffer, GLenum attachment, GLenum pname, GLint* params) {
    LOG()
    LOG_D(
        "[DSA] glGetNamedFramebufferAttachmentParameteriv, framebuffer: %u, attachment: 0x%X, pname: 0x%X, params: %p",
        framebuffer, attachment, pname, params);

    if (!params) {
        LOG_W("[DSA] Invalid parameters for glGetNamedFramebufferAttachmentParameteriv");
        return;
    }

    {
        DSA_TEMP_FBO(framebuffer);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, pname, params);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Retrieved framebuffer attachment parameter 0x%X for framebuffer %u and attachment 0x%X", pname,
          framebuffer, attachment);
}

// ============================================================================
// Renderbuffer objects
// ============================================================================

void glCreateRenderbuffers(GLsizei n, GLuint* renderbuffers) {
    LOG()
    LOG_D("[DSA] glCreateRenderbuffers, n: %d, renderbuffers: %p", n, renderbuffers);

    if (n <= 0 || !renderbuffers) {
        LOG_W("[DSA] Invalid parameters for glCreateRenderbuffers");
        return;
    }

    for (GLsizei i = 0; i < n; ++i) {
        GLuint rboID = 0;
        glGenRenderbuffers(1, &rboID);
        if (rboID == 0) {
            LOG_W("[DSA] Failed to create renderbuffer at index %d", i);
            continue;
        }
        {
            DSA_TEMP_RBO(rboID);
        }
        CHECK_GL_ERROR;
        renderbuffers[i] = rboID;
    }

    LOG_D("[DSA] Created %d renderbuffers successfully", n);
}

void glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height) {
    LOG()
    LOG_D("[DSA] glNamedRenderbufferStorage, renderbuffer: %u, internalformat: 0x%X, width: %d, height: %d",
          renderbuffer, internalformat, width, height);

    if (renderbuffer == 0 || width <= 0 || height <= 0) {
        LOG_W("[DSA] Invalid parameters for glNamedRenderbufferStorage");
        return;
    }

    {
        DSA_TEMP_RBO(renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, internalformat, width, height);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Set storage for renderbuffer %u with internal format 0x%X and size (%d, %d)", renderbuffer,
          internalformat, width, height);
}

void glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width,
                                           GLsizei height) {
    LOG()
    LOG_D("[DSA] glNamedRenderbufferStorageMultisample, renderbuffer: %u, samples: %d, internalformat: 0x%X, width: "
          "%d, height: %d",
          renderbuffer, samples, internalformat, width, height);

    if (renderbuffer == 0 || samples <= 0 || width <= 0 || height <= 0) {
        LOG_W("[DSA] Invalid parameters for glNamedRenderbufferStorageMultisample");
        return;
    }

    {
        DSA_TEMP_RBO(renderbuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, internalformat, width, height);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Set multisample storage for renderbuffer %u with internal format 0x%X and size (%d, %d)", renderbuffer,
          internalformat, width, height);
}

void glGetNamedRenderbufferParameteriv(GLuint renderbuffer, GLenum pname, GLint* params) {
    LOG()
    LOG_D("[DSA] glGetNamedRenderbufferParameteriv, renderbuffer: %u, pname: 0x%X, params: %p", renderbuffer, pname,
          params);

    if (renderbuffer == 0 || !params) {
        LOG_W("[DSA] Invalid parameters for glGetNamedRenderbufferParameteriv");
        return;
    }

    {
        DSA_TEMP_RBO(renderbuffer);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, pname, params);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Retrieved renderbuffer parameter 0x%X for renderbuffer %u", pname, renderbuffer);
}

// ============================================================================
// Texture objects
// ============================================================================
//
// Every function below resolves the texture's target from the tracker, binds
// it on the current unit, calls the classic entry point, and restores. The
// TEXTURE_OP_FUNC_BEGIN/END pair introduces `target` into the enclosing scope
// so the body can pass it straight through.

void glCreateTextures(GLenum target, GLsizei n, GLuint* textures) {
    LOG()
    LOG_D("[DSA] glCreateTextures, target: 0x%X, n: %d, textures: %p", target, n, textures);

    if (n <= 0 || !textures) {
        LOG_W("[DSA] Invalid parameters for glCreateTextures");
        return;
    }

    for (GLsizei i = 0; i < n; ++i) {
        GLuint texID = 0;
        glGenTextures(1, &texID);
        if (texID == 0) {
            LOG_W("[DSA] Failed to create texture at index %d", i);
            continue;
        }

        // glCreateTextures carries the target explicitly, so bind it here
        // rather than asking the tracker for a target the object does not
        // know yet.
        {
            TempBind bind = TempBind::Texture(texID, target);
        }
        CHECK_GL_ERROR;
        textures[i] = texID;
    }

    LOG_D("[DSA] Created %d textures successfully", n);
}

void glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer) {
    LOG()
    LOG_D("[DSA] glTextureBuffer, texture: %u, internalformat: 0x%X, buffer: %u", texture, internalformat, buffer);

    if (buffer == 0) {
        LOG_W("[DSA] Invalid parameters for glTextureBuffer");
        return;
    }

    TEXTURE_OP_FUNC_BEGIN(glTextureBuffer)
    glTexBuffer(GL_TEXTURE_BUFFER, internalformat, buffer);
    TEXTURE_OP_FUNC_END

    LOG_D("[DSA] Set buffer for texture %u with internal format 0x%X", texture, internalformat);
}

void glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    LOG()
    LOG_D("[DSA] glTextureBufferRange, texture: %u, internalformat: 0x%X, buffer: %u, offset: %lld, size: %lld",
          texture, internalformat, buffer, offset, size);

    if (buffer == 0 || size <= 0 || offset < 0) {
        LOG_W("[DSA] Invalid parameters for glTextureBufferRange");
        return;
    }

    TEXTURE_OP_FUNC_BEGIN(glTextureBufferRange)
    glTexBufferRange(GL_TEXTURE_BUFFER, internalformat, buffer, offset, size);
    TEXTURE_OP_FUNC_END

    LOG_D("[DSA] Set buffer range for texture %u with internal format 0x%X and size %lld at offset %lld", texture,
          internalformat, size, offset);
}

void glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    TEXTURE_OP_FUNC_BEGIN(glTextureStorage2D)
    glTexStorage2D(target, levels, internalformat, width, height);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Set 2D storage for texture %u with internal format 0x%X and size (%d, %d)", texture, internalformat,
          width, height);
}

void glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height,
                        GLsizei depth) {
    TEXTURE_OP_FUNC_BEGIN(glTextureStorage3D)
    glTexStorage3D(target, levels, internalformat, width, height, depth);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Set 3D storage for texture %u with internal format 0x%X and size (%d, %d, %d)", texture,
          internalformat, width, height, depth);
}

void glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width,
                                   GLsizei height, GLboolean fixedsamplelocations) {
    TEXTURE_OP_FUNC_BEGIN(glTextureStorage2DMultisample)
    glTexStorage2DMultisample(target, samples, internalformat, width, height, fixedsamplelocations);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Set 2D multisample storage for texture %u with internal format 0x%X and size (%d, %d)", texture,
          internalformat, width, height);
}

void glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width,
                                   GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    TEXTURE_OP_FUNC_BEGIN(glTextureStorage3DMultisample)
    glTexStorage3DMultisample(target, samples, internalformat, width, height, depth, fixedsamplelocations);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Set 3D multisample storage for texture %u with internal format 0x%X and size (%d, %d, %d)", texture,
          internalformat, width, height, depth);
}

void glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                         GLenum format, GLenum type, const void* pixels) {
    TEXTURE_OP_FUNC_BEGIN(glTextureSubImage2D)
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Updated 2D sub-image of texture %u at level %d with size (%d, %d) at offset (%d, %d)", texture, level,
          width, height, xoffset, yoffset);
}

void glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
                         GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    TEXTURE_OP_FUNC_BEGIN(glTextureSubImage3D)
    glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Updated 3D sub-image of texture %u at level %d with size (%d, %d, %d) at offset (%d, %d, %d)", texture,
          level, width, height, depth, xoffset, yoffset, zoffset);
}

void glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format,
                                   GLsizei imageSize, const void* data) {
    TEXTURE_OP_FUNC_BEGIN(glCompressedTextureSubImage1D)
    glCompressedTexSubImage1D(target, level, xoffset, width, format, imageSize, data);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Updated compressed 1D sub-image of texture %u at level %d with size %d at offset %d", texture, level,
          width, xoffset);
}

void glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                   GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
    TEXTURE_OP_FUNC_BEGIN(glCompressedTextureSubImage2D)
    glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Updated compressed 2D sub-image of texture %u at level %d with size (%d, %d) at offset (%d, %d)",
          texture, level, width, height, xoffset, yoffset);
}

void glCompressedTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                   GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize,
                                   const void* data) {
    TEXTURE_OP_FUNC_BEGIN(glCompressedTextureSubImage3D)
    glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    TEXTURE_OP_FUNC_END
    LOG_D(
        "[DSA] Updated compressed 3D sub-image of texture %u at level %d with size (%d, %d, %d) at offset (%d, %d, %d)",
        texture, level, width, height, depth, xoffset, yoffset, zoffset);
}

void glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
    TEXTURE_OP_FUNC_BEGIN(glCopyTextureSubImage1D)
    glCopyTexSubImage1D(target, level, xoffset, x, y, width);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Copied 1D sub-image to texture %u at level %d with size %d at offset %d", texture, level, width,
          xoffset);
}

void glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width,
                             GLsizei height) {
    TEXTURE_OP_FUNC_BEGIN(glCopyTextureSubImage2D)
    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Copied 2D sub-image to texture %u at level %d with size (%d, %d) at offset (%d, %d)", texture, level,
          width, height, xoffset, yoffset);
}

void glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y,
                             GLsizei width, GLsizei height) {
    TEXTURE_OP_FUNC_BEGIN(glCopyTextureSubImage3D)
    glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Copied 3D sub-image to texture %u at level %d with size (%d, %d) at offset (%d, %d)", texture, level,
          width, height, xoffset, yoffset);
}

void glTextureParameterf(GLuint texture, GLenum pname, GLfloat param) {
    TEXTURE_OP_FUNC_BEGIN(glTextureParameterf)
    glTexParameterf(target, pname, param);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Set float parameter 0x%X for texture %u to %f", pname, texture, param);
}

void glTextureParameterfv(GLuint texture, GLenum pname, const GLfloat* param) {
    TEXTURE_OP_FUNC_BEGIN(glTextureParameterfv)
    glTexParameterfv(target, pname, param);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Set float vector parameter 0x%X for texture %u", pname, texture);
}

void glTextureParameteri(GLuint texture, GLenum pname, GLint param) {
    TEXTURE_OP_FUNC_BEGIN(glTextureParameteri)
    glTexParameteri(target, pname, param);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Set integer parameter 0x%X for texture %u to %d", pname, texture, param);
}

void glTextureParameterIiv(GLuint texture, GLenum pname, const GLint* params) {
    TEXTURE_OP_FUNC_BEGIN(glTextureParameterIiv)
    glTexParameterIiv(target, pname, params);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Set integer vector parameter 0x%X for texture %u", pname, texture);
}

void glTextureParameterIuiv(GLuint texture, GLenum pname, const GLuint* params) {
    TEXTURE_OP_FUNC_BEGIN(glTextureParameterIuiv)
    glTexParameterIuiv(target, pname, params);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Set unsigned integer vector parameter 0x%X for texture %u", pname, texture);
}

void glTextureParameteriv(GLuint texture, GLenum pname, const GLint* param) {
    TEXTURE_OP_FUNC_BEGIN(glTextureParameteriv)
    glTexParameteriv(target, pname, param);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Set integer vector parameter 0x%X for texture %u", pname, texture);
}

void glGenerateTextureMipmap(GLuint texture) {
    TEXTURE_OP_FUNC_BEGIN(glGenerateTextureMipmap)
    glGenerateMipmap(target);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Generated mipmap for texture %u", texture);
}

void glBindTextureUnit(GLuint unit, GLuint texture) {
    LOG()
    LOG_D("[DSA] glBindTextureUnit, unit: %u, texture: %u", unit, texture);

    if (unit >= (GLuint)GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) {
        LOG_W("[DSA] Invalid parameters for glBindTextureUnit");
        return;
    }

    // GLES can only bind on the *active* unit, so switch units, bind, switch
    // back. The previous unit index is restored with glActiveTexture, which
    // is cheap and CPU-tracked by the texture layer.
    const GLint prevUnit = GetCurrentTextureUnitIndex();
    glActiveTexture(GL_TEXTURE0 + unit);
    if (texture != 0) {
        const GLenum target = GetTexTarget(texture);
        glBindTexture(target, texture);
    } else {
        // Unbinding: texture 0 has no target, so clear every target that can
        // be bound on this unit. Applications rely on glBindTextureUnit(u, 0)
        // to leave the unit genuinely empty.
        for (GLenum t : {GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_2D_ARRAY,
                         GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                         GL_TEXTURE_BUFFER}) {
            glBindTexture(t, 0);
        }
    }
    glActiveTexture(GL_TEXTURE0 + prevUnit);
    CHECK_GL_ERROR;

    LOG_D("[DSA] Bound texture %u to texture unit %u", texture, unit);
}

void glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels) {
    TEXTURE_OP_FUNC_BEGIN(glGetTextureImage)
    glGetTexImage(target, level, format, type, pixels);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Retrieved texture image from texture %u at level %d", texture, level);
}

void glGetCompressedTextureImage(GLuint texture, GLint level, GLsizei bufSize, void* pixels) {
    TEXTURE_OP_FUNC_BEGIN(glGetCompressedTextureImage)
    glGetCompressedTexImage(target, level, pixels);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Retrieved compressed texture image from texture %u at level %d", texture, level);
}

void glGetTextureLevelParameterfv(GLuint texture, GLint level, GLenum pname, GLfloat* params) {
    TEXTURE_OP_FUNC_BEGIN(glGetTextureLevelParameterfv)
    glGetTexLevelParameterfv(target, level, pname, params);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Retrieved texture level parameter 0x%X for texture %u at level %d", pname, texture, level);
}

void glGetTextureLevelParameteriv(GLuint texture, GLint level, GLenum pname, GLint* params) {
    TEXTURE_OP_FUNC_BEGIN(glGetTextureLevelParameteriv)
    glGetTexLevelParameteriv(target, level, pname, params);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Retrieved texture level parameter 0x%X for texture %u at level %d", pname, texture, level);
}

void glGetTextureParameterfv(GLuint texture, GLenum pname, GLfloat* params) {
    TEXTURE_OP_FUNC_BEGIN(glGetTextureParameterfv)
    glGetTexParameterfv(target, pname, params);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Retrieved texture parameter 0x%X for texture %u", pname, texture);
}

void glGetTextureParameterIiv(GLuint texture, GLenum pname, GLint* params) {
    TEXTURE_OP_FUNC_BEGIN(glGetTextureParameterIiv)
    glGetTexParameterIiv(target, pname, params);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Retrieved integer texture parameter 0x%X for texture %u", pname, texture);
}

void glGetTextureParameterIuiv(GLuint texture, GLenum pname, GLuint* params) {
    TEXTURE_OP_FUNC_BEGIN(glGetTextureParameterIuiv)
    glGetTexParameterIuiv(target, pname, params);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Retrieved unsigned integer texture parameter 0x%X for texture %u", pname, texture);
}

void glGetTextureParameteriv(GLuint texture, GLenum pname, GLint* params) {
    TEXTURE_OP_FUNC_BEGIN(glGetTextureParameteriv)
    glGetTexParameteriv(target, pname, params);
    TEXTURE_OP_FUNC_END
    LOG_D("[DSA] Retrieved integer texture parameter 0x%X for texture %u", pname, texture);
}

// ============================================================================
// Vertex array objects
// ============================================================================

void glCreateVertexArrays(GLsizei n, GLuint* arrays) {
    LOG()
    LOG_D("[DSA] glCreateVertexArrays, n: %d, arrays: %p", n, arrays);

    if (n <= 0 || !arrays) {
        LOG_W("[DSA] Invalid parameters for glCreateVertexArrays");
        return;
    }

    for (GLsizei i = 0; i < n; ++i) {
        GLuint vaoID = 0;
        glGenVertexArrays(1, &vaoID);
        if (vaoID == 0) {
            LOG_W("[DSA] Failed to create vertex array at index %d", i);
            continue;
        }
        {
            DSA_TEMP_VAO(vaoID);
        }
        CHECK_GL_ERROR;
        arrays[i] = vaoID;
    }

    LOG_D("[DSA] Created %d vertex arrays successfully", n);
}

void glDisableVertexArrayAttrib(GLuint vaobj, GLuint index) {
    LOG()
    LOG_D("[DSA] glDisableVertexArrayAttrib, vaobj: %u, index: %u", vaobj, index);

    if (vaobj == 0 || index >= GL_MAX_VERTEX_ATTRIBS) {
        LOG_W("[DSA] Invalid parameters for glDisableVertexArrayAttrib");
        return;
    }

    {
        DSA_TEMP_VAO(vaobj);
        glDisableVertexAttribArray(index);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Disabled vertex array attribute %u for vertex array object %u", index, vaobj);
}

void glEnableVertexArrayAttrib(GLuint vaobj, GLuint index) {
    LOG()
    LOG_D("[DSA] glEnableVertexArrayAttrib, vaobj: %u, index: %u", vaobj, index);

    if (vaobj == 0 || index >= GL_MAX_VERTEX_ATTRIBS) {
        LOG_W("[DSA] Invalid parameters for glEnableVertexArrayAttrib");
        return;
    }

    {
        DSA_TEMP_VAO(vaobj);
        glEnableVertexAttribArray(index);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Enabled vertex array attribute %u for vertex array object %u", index, vaobj);
}

void glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer) {
    LOG()
    LOG_D("[DSA] glVertexArrayElementBuffer, vaobj: %u, buffer: %u", vaobj, buffer);

    if (vaobj == 0 || buffer == 0) {
        LOG_W("[DSA] Invalid parameters for glVertexArrayElementBuffer");
        return;
    }

    {
        DSA_TEMP_VAO(vaobj);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Bound element buffer %u to vertex array object %u", buffer, vaobj);
}

void glVertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    LOG()
    LOG_D("[DSA] glVertexArrayVertexBuffer, vaobj: %u, bindingindex: %u, buffer: %u, offset: %lld, stride: %d", vaobj,
          bindingindex, buffer, offset, stride);

    if (vaobj == 0 || bindingindex >= GL_MAX_VERTEX_ATTRIB_BINDINGS || buffer == 0 || stride < 0 || offset < 0) {
        LOG_W("[DSA] Invalid parameters for glVertexArrayVertexBuffer");
        return;
    }

    {
        DSA_TEMP_VAO(vaobj);
        glBindVertexBuffer(bindingindex, buffer, offset, stride);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Bound vertex buffer %u to binding index %u for vertex array object %u with offset %lld and stride %d",
          buffer, bindingindex, vaobj, offset, stride);
}

void glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count, const GLuint* buffers,
                                const GLintptr* offsets, const GLsizei* strides) {
    LOG()
    LOG_D("[DSA] glVertexArrayVertexBuffers, vaobj: %u, first: %u, count: %d, buffers: %p, offsets: %p, strides: %p",
          vaobj, first, count, buffers, offsets, strides);

    if (vaobj == 0 || first >= GL_MAX_VERTEX_ATTRIB_BINDINGS || count <= 0 || !buffers || !offsets || !strides) {
        LOG_W("[DSA] Invalid parameters for glVertexArrayVertexBuffers");
        return;
    }

    {
        DSA_TEMP_VAO(vaobj);
        glBindVertexBuffers(first, count, buffers, offsets, strides);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Bound vertex buffers starting from index %u for vertex array object %u", first, vaobj);
}

void glVertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized,
                               GLuint relativeoffset) {
    LOG()
    LOG_D("[DSA] glVertexArrayAttribFormat, vaobj: %u, attribindex: %u, size: %d, type: 0x%X, normalized: %d, "
          "relativeoffset: %u",
          vaobj, attribindex, size, type, normalized, relativeoffset);

    if (vaobj == 0 || attribindex >= GL_MAX_VERTEX_ATTRIBS) {
        LOG_W("[DSA] Invalid parameters for glVertexArrayAttribFormat");
        return;
    }

    {
        DSA_TEMP_VAO(vaobj);
        glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Set vertex array attribute format for index %u in vertex array object %u", attribindex, vaobj);
}

void glVertexArrayAttribIFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    LOG()
    LOG_D("[DSA] glVertexArrayAttribIFormat, vaobj: %u, attribindex: %u, size: %d, type: 0x%X, relativeoffset: %u",
          vaobj, attribindex, size, type, relativeoffset);

    if (vaobj == 0 || attribindex >= GL_MAX_VERTEX_ATTRIBS || size <= 0 ||
        (type != GL_INT && type != GL_UNSIGNED_INT)) {
        LOG_W("[DSA] Invalid parameters for glVertexArrayAttribIFormat");
        return;
    }

    {
        DSA_TEMP_VAO(vaobj);
        glVertexAttribIFormat(attribindex, size, type, relativeoffset);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Set integer vertex array attribute format for index %u in vertex array object %u", attribindex, vaobj);
}

void glVertexArrayAttribLFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    LOG()
    LOG_D("[DSA] glVertexArrayAttribLFormat, vaobj: %u, attribindex: %u, size: %d, type: 0x%X, relativeoffset: %u",
          vaobj, attribindex, size, type, relativeoffset);

    if (vaobj == 0 || attribindex >= GL_MAX_VERTEX_ATTRIBS || size <= 0 || type != GL_DOUBLE) {
        LOG_W("[DSA] Invalid parameters for glVertexArrayAttribLFormat");
        return;
    }

    {
        DSA_TEMP_VAO(vaobj);
        glVertexAttribLFormat(attribindex, size, type, relativeoffset);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Set double vertex array attribute format for index %u in vertex array object %u", attribindex, vaobj);
}

void glVertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex) {
    LOG()
    LOG_D("[DSA] glVertexArrayAttribBinding, vaobj: %u, attribindex: %u, bindingindex: %u", vaobj, attribindex,
          bindingindex);

    if (vaobj == 0 || attribindex >= GL_MAX_VERTEX_ATTRIBS || bindingindex >= GL_MAX_VERTEX_ATTRIB_BINDINGS) {
        LOG_W("[DSA] Invalid parameters for glVertexArrayAttribBinding");
        return;
    }

    {
        DSA_TEMP_VAO(vaobj);
        glVertexAttribBinding(attribindex, bindingindex);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Set vertex array attribute binding for index %u in vertex array object %u to binding index %u",
          attribindex, vaobj, bindingindex);
}

void glVertexArrayBindingDivisor(GLuint vaobj, GLuint bindingindex, GLuint divisor) {
    LOG()
    LOG_D("[DSA] glVertexArrayBindingDivisor, vaobj: %u, bindingindex: %u, divisor: %u", vaobj, bindingindex, divisor);

    if (vaobj == 0 || bindingindex >= GL_MAX_VERTEX_ATTRIB_BINDINGS) {
        LOG_W("[DSA] Invalid parameters for glVertexArrayBindingDivisor");
        return;
    }

    {
        DSA_TEMP_VAO(vaobj);
        glVertexBindingDivisor(bindingindex, divisor);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Set vertex array binding divisor for binding index %u in vertex array object %u to %u", bindingindex,
          vaobj, divisor);
}

void glGetVertexArrayiv(GLuint vaobj, GLenum pname, GLint* param) {
    LOG()
    LOG_D("[DSA] glGetVertexArrayiv, vaobj: %u, pname: 0x%X, param: %p", vaobj, pname, param);

    if (vaobj == 0 || !param) {
        LOG_W("[DSA] Invalid parameters for glGetVertexArrayiv");
        return;
    }

    if (pname != GL_ELEMENT_ARRAY_BUFFER_BINDING) {
        LOG_W("[DSA] Invalid pname 0x%X for glGetVertexArrayiv. Only GL_ELEMENT_ARRAY_BUFFER_BINDING is allowed.",
              pname);
        return;
    }

    // The element buffer is a per-VAO property, so it is only observable by
    // asking while that VAO is current. Bind it, read, restore.
    {
        DSA_TEMP_VAO(vaobj);
        glGetIntegerv(pname, param);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Retrieved vertex array parameter 0x%X for vertex array object %u, value: %d", pname, vaobj, *param);
}

void glGetVertexArrayIndexediv(GLuint vaobj, GLuint index, GLenum pname, GLint* param) {
    LOG()
    LOG_D("[DSA] glGetVertexArrayIndexediv, vaobj: %u, index: %u, pname: 0x%X, param: %p", vaobj, index, pname, param);

    if (vaobj == 0 || index >= GL_MAX_VERTEX_ATTRIBS || !param) {
        LOG_W("[DSA] Invalid parameters for glGetVertexArrayIndexediv");
        return;
    }

    {
        DSA_TEMP_VAO(vaobj);
        glGetVertexAttribiv(index, pname, param);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Retrieved indexed vertex array parameter 0x%X for VAO %u at index %u", pname, vaobj, index);
}

void glGetVertexArrayIndexed64iv(GLuint vaobj, GLuint index, GLenum pname, GLint64* param) {
    LOG()
    LOG_D("[DSA] glGetVertexArrayIndexed64iv, vaobj: %u, index: %u, pname: 0x%X, param: %p", vaobj, index, pname,
          param);

    if (vaobj == 0 || index >= GL_MAX_VERTEX_ATTRIBS || !param) {
        LOG_W("[DSA] Invalid parameters for glGetVertexArrayIndexed64iv");
        return;
    }

    // GLES has no 64-bit form of glGetVertexAttrib*, so the 32-bit variant is
    // the only available source. Narrowing here is safe because the values
    // this can legitimately return (bindings, divisors, offsets expressed as
    // attribute properties) fit in 32 bits on any real driver.
    GLint value = 0;
    {
        DSA_TEMP_VAO(vaobj);
        glGetVertexAttribIiv(index, pname, &value);
    }
    CHECK_GL_ERROR;
    *param = static_cast<GLint64>(value);

    LOG_D("[DSA] Retrieved indexed 64-bit vertex array parameter 0x%X for VAO %u at index %u", pname, vaobj, index);
}

// ============================================================================
// Sampler objects
// ============================================================================

void glCreateSamplers(GLsizei n, GLuint* samplers) {
    LOG()
    LOG_D("[DSA] glCreateSamplers, n: %d, samplers: %p", n, samplers);

    if (n <= 0 || !samplers) {
        LOG_W("[DSA] Invalid parameters for glCreateSamplers");
        return;
    }

    // glGenSamplers already creates the object; unlike buffers there is
    // nothing that requires a bind to materialise it. The previous revision
    // still bound each sampler to unit 1 and back, which (a) queried the
    // driver on every call and (b) hard-coded a unit, disturbing whatever the
    // application had bound there. Both are gone.
    glGenSamplers(n, samplers);
    CHECK_GL_ERROR;

    LOG_D("[DSA] Created %d samplers successfully", n);
}

// ============================================================================
// Program pipeline objects
// ============================================================================

void glCreateProgramPipelines(GLsizei n, GLuint* pipelines) {
    LOG()
    LOG_D("[DSA] glCreateProgramPipelines, n: %d, pipelines: %p", n, pipelines);

    if (n <= 0 || !pipelines) {
        LOG_W("[DSA] Invalid parameters for glCreateProgramPipelines");
        return;
    }

    // Same as samplers: glGenProgramPipelines is sufficient to create them,
    // so the bind-and-restore dance (and its driver query) is unnecessary.
    glGenProgramPipelines(n, pipelines);
    CHECK_GL_ERROR;

    LOG_D("[DSA] Created %d program pipelines successfully", n);
}

// ============================================================================
// Query objects
// ============================================================================

void glCreateQueries(GLenum target, GLsizei n, GLuint* ids) {
    LOG()
    LOG_D("[DSA] glCreateQueries, target: 0x%X, n: %d, ids: %p", target, n, ids);

    if (n <= 0 || !ids) {
        LOG_W("[DSA] Invalid parameters for glCreateQueries");
        return;
    }

    // FIX: the previous revision had an inverted guard
    //     if (n <= 0 || !ids) glGenQueries(n, ids);
    // which only generated names for *invalid* arguments, so every legitimate
    // glCreateQueries call returned untouched (usually zero) query names.
    glGenQueries(n, ids);
    CHECK_GL_ERROR;

    LOG_D("[DSA] Created %d queries successfully", n);
}

namespace
{
    // GL_QUERY_BUFFER lets a query result be written straight into a buffer.
    // GLES 3.2 has the target but no glGetQueryBufferObject*, so the result is
    // read back on the CPU and pushed with glBufferSubData.
    //
    // All four typed entry points share this shape: read the value, bind the
    // destination buffer to GL_QUERY_BUFFER, upload, restore. The value is
    // read *before* the bind so a failed query result is still written (the
    // desktop semantic is "the buffer receives the result", including zero).
    template <typename T>
    void QueryResultToBuffer(GLuint id, GLuint buffer, GLenum pname, GLintptr offset,
                             void (*getter)(GLuint, GLenum, T*)) {
        T value = 0;
        getter(id, pname, &value);
        {
            PushTempBinding(GL_QUERY_BUFFER, buffer, find_bound_buffer(GL_QUERY_BUFFER_BINDING));
            glBufferSubData(GL_QUERY_BUFFER, offset, sizeof(value), &value);
        }
        CHECK_GL_ERROR;
    }
} // namespace

void glGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    LOG()
    LOG_D("[DSA] glGetQueryBufferObjectiv, id: %u, buffer: %u, pname: 0x%X, offset: %lld", id, buffer, pname, offset);
    assert(pname == GL_QUERY_RESULT || pname == GL_QUERY_RESULT_AVAILABLE);
    QueryResultToBuffer<GLint>(id, buffer, pname, offset, glGetQueryObjectiv);
}

void glGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    LOG()
    LOG_D("[DSA] glGetQueryBufferObjectuiv, id: %u, buffer: %u, pname: 0x%X, offset: %lld", id, buffer, pname, offset);
    assert(pname == GL_QUERY_RESULT || pname == GL_QUERY_RESULT_AVAILABLE);
    QueryResultToBuffer<GLuint>(id, buffer, pname, offset, glGetQueryObjectuiv);
}

void glGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    LOG()
    LOG_D("[DSA] glGetQueryBufferObjecti64v, id: %u, buffer: %u, pname: 0x%X, offset: %lld", id, buffer, pname, offset);
    assert(pname == GL_QUERY_RESULT || pname == GL_QUERY_RESULT_AVAILABLE);
    QueryResultToBuffer<GLint64>(id, buffer, pname, offset, glGetQueryObjecti64v);
}

void glGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
    LOG()
    LOG_D("[DSA] glGetQueryBufferObjectui64v, id: %u, buffer: %u, pname: 0x%X, offset: %lld", id, buffer, pname,
          offset);
    assert(pname == GL_QUERY_RESULT || pname == GL_QUERY_RESULT_AVAILABLE);
    QueryResultToBuffer<GLuint64>(id, buffer, pname, offset, glGetQueryObjectui64v);
}

// ============================================================================
// Transform feedback objects
// ============================================================================

GLAPI void glCreateTransformFeedbacks(GLsizei n, GLuint* ids) {
    LOG();
    LOG_D("[DSA] glCreateTransformFeedbacks, n=%d, ids=%p", n, ids);

    if (n <= 0 || !ids) {
        LOG_W("[DSA] Invalid parameters for glCreateTransformFeedbacks");
        return;
    }

    glGenTransformFeedbacks(n, ids);
    CHECK_GL_ERROR;
    LOG_D("[DSA] Created %d transform feedback objects", n);
}

GLAPI void glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer) {
    LOG();
    LOG_D("[DSA] glTransformFeedbackBufferBase, xfb=%u, index=%u, buffer=%u", xfb, index, buffer);

    if (xfb == 0 || index >= (GLuint)GL_MAX_TRANSFORM_FEEDBACK_BUFFERS || buffer == 0) {
        LOG_W("[DSA] Invalid parameters for glTransformFeedbackBufferBase");
        return;
    }

    {
        DSA_TEMP_XFB(xfb);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, index, buffer);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Bound buffer %u to TFBO %u at index %u", buffer, xfb, index);
}

GLAPI void glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    LOG();
    LOG_D("[DSA] glTransformFeedbackBufferRange, xfb=%u, index=%u, buffer=%u, offset=%lld, size=%lld", xfb, index,
          buffer, offset, size);

    if (xfb == 0 || index >= (GLuint)GL_MAX_TRANSFORM_FEEDBACK_BUFFERS || buffer == 0 || offset < 0 || size <= 0) {
        LOG_W("[DSA] Invalid parameters for glTransformFeedbackBufferRange");
        return;
    }

    {
        DSA_TEMP_XFB(xfb);
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, index, buffer, offset, size);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Bound buffer %u to TFBO %u at index %u (offset=%lld, size=%lld)", buffer, xfb, index, offset, size);
}

GLAPI void glGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint* param) {
    LOG();
    LOG_D("[DSA] glGetTransformFeedbackiv, xfb=%u, pname=0x%X, param=%p", xfb, pname, param);

    if (xfb == 0 || !param) {
        LOG_W("[DSA] Invalid parameters for glGetTransformFeedbackiv");
        return;
    }

    {
        DSA_TEMP_XFB(xfb);
        glGetTransformFeedbackiv(GL_TRANSFORM_FEEDBACK, pname, param);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Retrieved TFBO %u param 0x%X = %d", xfb, pname, *param);
}

GLAPI void glGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index, GLint* param) {
    LOG();
    LOG_D("[DSA] glGetTransformFeedbacki_v, xfb=%u, pname=0x%X, index=%u, param=%p", xfb, pname, index, param);

    if (xfb == 0 || index >= (GLuint)GL_MAX_TRANSFORM_FEEDBACK_BUFFERS || !param) {
        LOG_W("[DSA] Invalid parameters for glGetTransformFeedbacki_v");
        return;
    }

    {
        DSA_TEMP_XFB(xfb);
        glGetTransformFeedbacki_v(GL_TRANSFORM_FEEDBACK, pname, index, param);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Retrieved TFBO %u param 0x%X at index %u = %d", xfb, pname, index, *param);
}

GLAPI void glGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index, GLint64* param) {
    LOG();
    LOG_D("[DSA] glGetTransformFeedbacki64_v, xfb=%u, pname=0x%X, index=%u, param=%p", xfb, pname, index, param);

    if (xfb == 0 || index >= (GLuint)GL_MAX_TRANSFORM_FEEDBACK_BUFFERS || !param) {
        LOG_W("[DSA] Invalid parameters for glGetTransformFeedbacki64_v");
        return;
    }

    {
        DSA_TEMP_XFB(xfb);
        glGetTransformFeedbacki64_v(GL_TRANSFORM_FEEDBACK, pname, index, param);
    }
    CHECK_GL_ERROR;

    LOG_D("[DSA] Retrieved TFBO %u param 0x%X at index %u = %lld", xfb, pname, index, *param);
}
