// MobileGlues - gl/state/Core.h
// GL State Core - Central state management for GL Core Profile → GLES 3.2
//
// Architecture: "CPU-GPU Symbiotic Context"
//   GLContext is the unified context that completely binds CPU-side state
//   tracking (GLStateManager) with GPU-side backend resources (BackendObject,
//   EGLDisplay, EGLContext, EGLSurface). They are inseparable — created
//   together, live together, and destroyed together. No more separate
//   singletons.
//
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#pragma once

#include "../../includes.h"
#include "../state.h"
#include <EGL/egl.h>

// state.h 定义了 #define GLState GLStateManager::Instance() 宏，
// 这会污染 namespace MobileGL::MG_State::GLState 的声明。
// 在此处暂存并取消宏定义，命名空间声明结束后恢复。
#pragma push_macro("GLState")
#undef GLState

namespace MobileGL::MG_Backend {
class BackendObject;
}

namespace MobileGL::MG_State::GLState {

// Forward declarations
class GLContext;
class BufferObject;
class VertexArrayObject;
class ITextureObject;
class FramebufferObject;
class ProgramObject;
class SamplerObject;
class RenderbufferObject;

// Version tracking for dirty-checking
using VersionType = Uint16;

// =============================================================================
// CPU-GPU Symbiotic Context — GPU主导，CPU绑定
//
// GLContext将CPU状态跟踪和GPU后端资源绑定为一体：
//   - GPU优先：GPU后端先创建，CPU状态随后绑定，二者共生
//   - 通过SetBackend()将GPU后端注入Context，CPU状态随之初始化
//   - 共同存活，共同销毁
// =============================================================================

class GLContext {
public:
    GLContext();
    ~GLContext();

    // -------------------------------------------------------------------------
    // Lifecycle: GPU主导，CPU跟随绑定
    // -------------------------------------------------------------------------

    /// 初始化CPU状态。GPU后端通过SetBackend()单独注入。
    void Initialize();

    /// 关闭CPU状态和GPU后端——两者同时释放。
    void Shutdown();

    /// 返回true当CPU和GPU都已初始化。
    Bool IsInitialized() const { return m_initialized; }

    // -------------------------------------------------------------------------
    // GPU Backend（GPU优先：通过SetBackend注入，Context接管所有权）
    // -------------------------------------------------------------------------

    MG_Backend::BackendObject* GetBackend() { return m_backend.get(); }
    const MG_Backend::BackendObject* GetBackend() const { return m_backend.get(); }

    /// 将GPU后端绑定到Context——Context接管所有权。
    void SetBackend(UniquePtr<MG_Backend::BackendObject> backend);

    // -------------------------------------------------------------------------
    // EGL Resource Access
    // -------------------------------------------------------------------------

    EGLDisplay GetEGLDisplay() const { return m_eglDisplay; }
    EGLContext GetEGLContext() const { return m_eglContext; }
    EGLSurface GetEGLSurface() const { return m_eglSurface; }

    void SetEGLDisplay(EGLDisplay dpy) { m_eglDisplay = dpy; }
    void SetEGLContext(EGLContext ctx) { m_eglContext = ctx; }
    void SetEGLSurface(EGLSurface surf) { m_eglSurface = surf; }

    // -------------------------------------------------------------------------
    // Active Context (thread-local or global fallback)
    // -------------------------------------------------------------------------

    /// Get the currently active GLContext (for backward compatibility).
    static GLContext& Get();

    /// Set the currently active GLContext.
    static void SetActive(GLContext* ctx);

    // -------------------------------------------------------------------------
    // Object management
    // -------------------------------------------------------------------------

    void CreateBuffer(GLuint id);
    void CreateVertexArray(GLuint id);
    void CreateTexture(GLuint id);
    void CreateFramebuffer(GLuint id);
    void CreateProgram(GLuint id);
    void CreateShader(GLuint id);
    void CreateSampler(GLuint id);
    void CreateRenderbuffer(GLuint id);

    void DeleteBuffer(GLuint id);
    void DeleteVertexArray(GLuint id);
    void DeleteTexture(GLuint id);
    void DeleteFramebuffer(GLuint id);
    void DeleteProgram(GLuint id);
    void DeleteShader(GLuint id);
    void DeleteSampler(GLuint id);
    void DeleteRenderbuffer(GLuint id);

    // Object lookup
    SharedPtr<BufferObject> GetBuffer(GLuint id);
    SharedPtr<VertexArrayObject> GetVertexArray(GLuint id);
    SharedPtr<ITextureObject> GetTexture(GLuint id);
    SharedPtr<FramebufferObject> GetFramebuffer(GLuint id);
    SharedPtr<ProgramObject> GetProgram(GLuint id);
    SharedPtr<SamplerObject> GetSampler(GLuint id);
    SharedPtr<RenderbufferObject> GetRenderbuffer(GLuint id);

    Bool IsBuffer(GLuint id) const;
    Bool IsVertexArray(GLuint id) const;
    Bool IsTexture(GLuint id) const;
    Bool IsFramebuffer(GLuint id) const;
    Bool IsProgram(GLuint id) const;
    Bool IsShader(GLuint id) const;
    Bool IsSampler(GLuint id) const;
    Bool IsRenderbuffer(GLuint id) const;

    // -------------------------------------------------------------------------
    // CPU State Management
    // -------------------------------------------------------------------------

    GLStateManager& GetStateManager() { return m_stateManager; }
    const GLStateManager& GetStateManager() const { return m_stateManager; }

    // Version tracking for dirty-checking
    VersionType IncrementVersion();
    VersionType GetVersion() const;

private:
    // ---- CPU-side state (symbiotic partner 1) ----
    GLStateManager m_stateManager;
    VersionType m_version = 0;

    // ---- GPU-side resources (symbiotic partner 2) ----
    UniquePtr<MG_Backend::BackendObject> m_backend;
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    EGLContext m_eglContext = EGL_NO_CONTEXT;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    Bool m_initialized = false;

    // ---- Object registries (CPU-side object tracking) ----
    UnorderedMap<GLuint, SharedPtr<BufferObject>> m_buffers;
    UnorderedMap<GLuint, SharedPtr<VertexArrayObject>> m_vertexArrays;
    UnorderedMap<GLuint, SharedPtr<ITextureObject>> m_textures;
    UnorderedMap<GLuint, SharedPtr<FramebufferObject>> m_framebuffers;
    UnorderedMap<GLuint, SharedPtr<ProgramObject>> m_programs;
    UnorderedMap<GLuint, SharedPtr<SamplerObject>> m_samplers;
    UnorderedMap<GLuint, SharedPtr<RenderbufferObject>> m_renderbuffers;
    UnorderedMap<GLuint, GLenum> m_shaders;
};

// =============================================================================
// Buffer Object
// =============================================================================

class BufferObject {
public:
    BufferObject(GLuint id = 0);
    ~BufferObject();

    GLuint GetId() const { return m_id; }

    void SetData(GLsizeiptr size, const void* data, GLenum usage);
    void SetSubData(GLintptr offset, GLsizeiptr size, const void* data);
    void SetStorage(GLsizeiptr size, const void* data, GLbitfield flags);

    void* Map(GLenum access);
    void* MapRange(GLintptr offset, GLsizeiptr length, GLbitfield access);
    Bool Unmap();
    void FlushMappedRange(GLintptr offset, GLsizeiptr length);

    GLsizeiptr GetSize() const { return m_size; }
    GLenum GetUsage() const { return m_usage; }
    GLbitfield GetStorageFlags() const { return m_storageFlags; }
    Bool IsMapped() const { return m_mapped; }
    void* GetMappedPointer() const { return m_mappedPtr; }
    Bool IsImmutable() const { return m_immutable; }

    VersionType GetVersion() const { return m_version; }
    void IncrementVersion() { m_version++; }

private:
    GLuint m_id = 0;
    GLsizeiptr m_size = 0;
    GLenum m_usage = GL_STATIC_DRAW;
    GLbitfield m_storageFlags = 0;
    Bool m_immutable = false;
    Bool m_mapped = false;
    void* m_mappedPtr = nullptr;
    GLsizeiptr m_mappedOffset = 0;
    GLsizeiptr m_mappedLength = 0;
    GLbitfield m_mappedAccess = 0;
    VersionType m_version = 0;
};

// =============================================================================
// Vertex Array Object
// =============================================================================

class VertexArrayObject {
public:
    static constexpr SizeT MAX_VERTEX_ATTRIBS = 16;
    static constexpr SizeT MAX_VERTEX_BINDINGS = 16;

    struct VertexAttribute {
        Bool enabled = false;
        GLint size = 4;
        GLenum type = GL_FLOAT;
        Bool normalized = false;
        Bool isInteger = false;
        Bool isLong = false;
        GLuint relativeOffset = 0;
        GLuint bindingIndex = 0;
        GLuint divisor = 0;
        VersionType version = 0;
    };

    struct VertexBinding {
        GLuint buffer = 0;
        GLintptr offset = 0;
        GLsizeiptr stride = 0;
        VersionType version = 0;
    };

    VertexArrayObject(GLuint id = 0);
    ~VertexArrayObject();

    GLuint GetId() const { return m_id; }

    void SetElementBuffer(GLuint buffer);
    GLuint GetElementBuffer() const { return m_elementBuffer; }

    VertexAttribute& GetAttribute(GLuint index);
    VertexBinding& GetBinding(GLuint index);
    const VertexAttribute& GetAttribute(GLuint index) const;
    const VertexBinding& GetBinding(GLuint index) const;

    VersionType GetVersion() const { return m_version; }
    void IncrementVersion() { m_version++; }

private:
    GLuint m_id = 0;
    GLuint m_elementBuffer = 0;
    Array<VertexAttribute, MAX_VERTEX_ATTRIBS> m_attributes;
    Array<VertexBinding, MAX_VERTEX_BINDINGS> m_bindings;
    VersionType m_version = 0;
};

using VertexAttributeVersion = VersionType;

} // namespace MobileGL::MG_State::GLState

#pragma pop_macro("GLState")