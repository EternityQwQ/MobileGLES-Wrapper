// MobileGlues - gl/backend/Init.cpp
// Backend initialization entry point
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "BackendObject.h"
#include "DirectGLES/DirectGLES.h"
#include "DirectGLES/BackendObject_DirectGLES.h"

namespace MobileGL::MG_Backend {

GlobalBackendFunctionsTable gBackendFunctionsTable = {};
UniquePtr<BackendObject> pActiveBackendObject = nullptr;

} // namespace MobileGL::MG_Backend

namespace MobileGL::backend::DirectGLES {

EGLFunctionsTable g_EGLFuncs = {};
GLESFunctionsTable g_GLESFuncs = {};
GLESCapabilities g_GLESCapabilities = {};

// Backend object registry instances
namespace BufferImpl {
StateBackendObjectRegistry<MG_State::GLState::BufferObject, BackendBufferObject> g_backendBufferObjects;
}

namespace VertexArrayImpl {
StateBackendObjectRegistry<MG_State::GLState::VertexArrayObject, BackendVertexArrayObject>
    g_backendVertexArrayObjects;
}

namespace TextureImpl {
StateBackendObjectRegistry<MG_State::GLState::ITextureObject, BackendTextureObject>
    g_backendTextureObjects;
Array<Array<BackendTextureObject*, (SizeT)TextureTarget::TextureTargetCount>,
      MG_State::GLState::TextureState::MAX_TEXTURE_IMAGE_UNITS>
    g_boundTexturesCache = {};
Uint g_activeTextureUnit = 0;
}

namespace FramebufferImpl {
StateBackendObjectRegistry<MG_State::GLState::FramebufferObject, BackendFramebufferObject>
    g_backendFramebufferObjects;
Array<Uint16, SizeT(FramebufferTarget::FramebufferTargetCount)> g_fboBindVersions = {};
}

namespace ProgramImpl {
Uint32 g_snormFallbackClampOutputMask = 0;
Uint32 g_unormFallbackClampOutputMask = 0;
StateBackendObjectRegistry<MG_State::GLState::ProgramObject, BackendProgramObjectImpl>
    g_backendProgramObjects;
}

namespace SamplerImpl {
Array<BackendSamplerObject*, MG_State::GLState::TextureState::MAX_TEXTURE_IMAGE_UNITS>
    g_boundSamplersCache = {};
StateBackendObjectRegistry<MG_State::GLState::SamplerObject, BackendSamplerObject>
    g_backendSamplerObjects;
}

namespace RenderbufferImpl {
StateBackendObjectRegistry<MG_State::GLState::RenderbufferObject, BackendRenderbufferObject>
    g_backendRenderbufferObjects;
}

// Initialize backend
Bool InitWindowSurface(NativeWindowType window) {
    if (!MG_Backend::pActiveBackendObject) {
        MG_Backend::pActiveBackendObject = UniquePtr<BackendObject_DirectGLES>(new BackendObject_DirectGLES());
        MG_Backend::pActiveBackendObject->Initialize();
    }

    MG_Backend::pActiveBackendObject->SetWindowHandle(
        MG_Backend::WindowHandle{
            .Backend = MG_Backend::WindowBackend::Android,
            .Handle = window,
            .Width = 0,
            .Height = 0
        });
    return MG_Backend::pActiveBackendObject->InitWindowSurface();
}

Bool InitPbufferSurface(EGLint width, EGLint height) {
    if (!MG_Backend::pActiveBackendObject) {
        MG_Backend::pActiveBackendObject = UniquePtr<BackendObject_DirectGLES>(new BackendObject_DirectGLES());
        MG_Backend::pActiveBackendObject->Initialize();
    }
    return MG_Backend::pActiveBackendObject->InitPbufferSurface(width, height);
}

Bool MakeCurrent() {
    if (!MG_Backend::pActiveBackendObject) return false;
    return MG_Backend::pActiveBackendObject->MakeEGLCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);
}

Bool ReleaseCurrent() {
    if (!MG_Backend::pActiveBackendObject) return false;
    return MG_Backend::pActiveBackendObject->MakeEGLCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void Present() {
    if (MG_Backend::pActiveBackendObject) {
        MG_Backend::pActiveBackendObject->SwapEGLBuffers(m_eglDisplay, m_eglSurface);
    }
}

void SetEGLFuncsTable(const EGLFunctionsTable& eglFuncs) { g_EGLFuncs = eglFuncs; }
void SetGLESFuncsTable(const GLESFunctionsTable& glesFuncs) { g_GLESFuncs = glesFuncs; }
void SetGLESCapabilities(const GLESCapabilities& capabilities) { g_GLESCapabilities = capabilities; }

void DestroyEGLContext() {
    if (MG_Backend::pActiveBackendObject) {
        MG_Backend::pActiveBackendObject->ReleaseEGLResources();
        MG_Backend::pActiveBackendObject.reset();
    }
}

} // namespace MobileGL::backend::DirectGLES