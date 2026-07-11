// MobileGlues - gl/state/Core.cpp
// GLContext implementation: CPU-GPU Symbiotic Context
//
// GLContext is the unified context that completely binds CPU-side state
// tracking (GLStateManager) with GPU-side backend resources (BackendObject,
// EGLDisplay, EGLContext, EGLSurface). They are inseparable — created
// together, live together, and destroyed together.
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "Core.h"
#include "../backend/BackendObject.h"

namespace MobileGL::MG_State::GLState {

// =============================================================================
// Active Context (thread-local, with global fallback)
// =============================================================================

static thread_local GLContext* tls_activeContext = nullptr;
static GLContext* g_fallbackContext = nullptr;

GLContext& GLContext::Get()
{
    if (tls_activeContext) {
        return *tls_activeContext;
    }
    if (g_fallbackContext) {
        return *g_fallbackContext;
    }
    // Last resort: create a static fallback (for legacy code compatibility)
    static GLContext s_defaultContext;
    return s_defaultContext;
}

void GLContext::SetActive(GLContext* ctx)
{
    tls_activeContext = ctx;
    if (ctx) {
        g_fallbackContext = ctx;
    }
}

// =============================================================================
// Construction / Destruction
// =============================================================================

GLContext::GLContext()
{
    // CPU state starts uninitialized; GPU backend is null.
    // Call Initialize() to bind both CPU and GPU together.
}

GLContext::~GLContext()
{
    Shutdown();
}

// =============================================================================
// Initialize: CPU状态初始化（GPU后端通过SetBackend单独注入）
// =============================================================================

void GLContext::Initialize()
{
    if (m_initialized) return;
    m_stateManager.Initialize();
    m_initialized = true;
}

// =============================================================================
// Shutdown: CPU状态和GPU后端同时释放
// =============================================================================

void GLContext::Shutdown()
{
    if (!m_initialized) return;

    if (m_backend) {
        m_backend->ReleaseEGLResources();
        m_backend.reset();
    }

    m_eglDisplay = EGL_NO_DISPLAY;
    m_eglContext = EGL_NO_CONTEXT;
    m_eglSurface = EGL_NO_SURFACE;

    m_stateManager.Shutdown();
    m_initialized = false;
}

// =============================================================================
// GPU Backend Binding: GPU优先，注入到Context
// =============================================================================

void GLContext::SetBackend(UniquePtr<MG_Backend::BackendObject> backend)
{
    if (m_backend) {
        m_backend->ReleaseEGLResources();
    }
    m_backend = std::move(backend);
}

// =============================================================================
// Version Tracking
// =============================================================================

VersionType GLContext::IncrementVersion()
{
    return ++m_version;
}

VersionType GLContext::GetVersion() const
{
    return m_version;
}

// =============================================================================
// Object Management: Buffer
// =============================================================================

void GLContext::CreateBuffer(GLuint id)
{
    m_buffers[id] = MakeShared<BufferObject>(id);
}

void GLContext::DeleteBuffer(GLuint id)
{
    m_buffers.erase(id);
}

SharedPtr<BufferObject> GLContext::GetBuffer(GLuint id)
{
    auto it = m_buffers.find(id);
    return (it != m_buffers.end()) ? it->second : nullptr;
}

Bool GLContext::IsBuffer(GLuint id) const
{
    return m_buffers.find(id) != m_buffers.end();
}

// =============================================================================
// Object Management: VertexArray
// =============================================================================

void GLContext::CreateVertexArray(GLuint id)
{
    m_vertexArrays[id] = MakeShared<VertexArrayObject>(id);
}

void GLContext::DeleteVertexArray(GLuint id)
{
    m_vertexArrays.erase(id);
}

SharedPtr<VertexArrayObject> GLContext::GetVertexArray(GLuint id)
{
    auto it = m_vertexArrays.find(id);
    return (it != m_vertexArrays.end()) ? it->second : nullptr;
}

Bool GLContext::IsVertexArray(GLuint id) const
{
    return m_vertexArrays.find(id) != m_vertexArrays.end();
}

// =============================================================================
// Object Management: Texture
// =============================================================================

void GLContext::CreateTexture(GLuint id)
{
    // Texture creation is deferred to the actual ITextureObject implementation
    m_textures[id] = nullptr; // placeholder; actual creation via backend
}

void GLContext::DeleteTexture(GLuint id)
{
    m_textures.erase(id);
}

SharedPtr<ITextureObject> GLContext::GetTexture(GLuint id)
{
    auto it = m_textures.find(id);
    return (it != m_textures.end()) ? it->second : nullptr;
}

Bool GLContext::IsTexture(GLuint id) const
{
    return m_textures.find(id) != m_textures.end();
}

// =============================================================================
// Object Management: Framebuffer
// =============================================================================

void GLContext::CreateFramebuffer(GLuint id)
{
    m_framebuffers[id] = MakeShared<FramebufferObject>();
}

void GLContext::DeleteFramebuffer(GLuint id)
{
    m_framebuffers.erase(id);
}

SharedPtr<FramebufferObject> GLContext::GetFramebuffer(GLuint id)
{
    auto it = m_framebuffers.find(id);
    return (it != m_framebuffers.end()) ? it->second : nullptr;
}

Bool GLContext::IsFramebuffer(GLuint id) const
{
    return m_framebuffers.find(id) != m_framebuffers.end();
}

// =============================================================================
// Object Management: Program
// =============================================================================

void GLContext::CreateProgram(GLuint id)
{
    m_programs[id] = MakeShared<ProgramObject>();
}

void GLContext::DeleteProgram(GLuint id)
{
    m_programs.erase(id);
}

SharedPtr<ProgramObject> GLContext::GetProgram(GLuint id)
{
    auto it = m_programs.find(id);
    return (it != m_programs.end()) ? it->second : nullptr;
}

Bool GLContext::IsProgram(GLuint id) const
{
    return m_programs.find(id) != m_programs.end();
}

// =============================================================================
// Object Management: Shader
// =============================================================================

void GLContext::CreateShader(GLuint id)
{
    m_shaders[id] = 0; // shader type set later
}

void GLContext::DeleteShader(GLuint id)
{
    m_shaders.erase(id);
}

Bool GLContext::IsShader(GLuint id) const
{
    return m_shaders.find(id) != m_shaders.end();
}

// =============================================================================
// Object Management: Sampler
// =============================================================================

void GLContext::CreateSampler(GLuint id)
{
    m_samplers[id] = MakeShared<SamplerObject>();
}

void GLContext::DeleteSampler(GLuint id)
{
    m_samplers.erase(id);
}

SharedPtr<SamplerObject> GLContext::GetSampler(GLuint id)
{
    auto it = m_samplers.find(id);
    return (it != m_samplers.end()) ? it->second : nullptr;
}

Bool GLContext::IsSampler(GLuint id) const
{
    return m_samplers.find(id) != m_samplers.end();
}

// =============================================================================
// Object Management: Renderbuffer
// =============================================================================

void GLContext::CreateRenderbuffer(GLuint id)
{
    m_renderbuffers[id] = MakeShared<RenderbufferObject>();
}

void GLContext::DeleteRenderbuffer(GLuint id)
{
    m_renderbuffers.erase(id);
}

SharedPtr<RenderbufferObject> GLContext::GetRenderbuffer(GLuint id)
{
    auto it = m_renderbuffers.find(id);
    return (it != m_renderbuffers.end()) ? it->second : nullptr;
}

Bool GLContext::IsRenderbuffer(GLuint id) const
{
    return m_renderbuffers.find(id) != m_renderbuffers.end();
}

} // namespace MobileGL::MG_State::GLState