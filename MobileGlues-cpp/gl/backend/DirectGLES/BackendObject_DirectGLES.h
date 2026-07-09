// MobileGlues - gl/backend/DirectGLES/BackendObject_DirectGLES.h
// DirectGLES backend object - EGL context management
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#pragma once

#include "../../../includes.h"
#include "../../../MG_Backend/BackendObject.h"
#include <MG_Util/BackendLoaders/OpenGL/Loader.h>

namespace MobileGL::backend::DirectGLES {

class BackendObject_DirectGLES : public MG_Backend::BackendObject {
public:
    ~BackendObject_DirectGLES() override;

    void Initialize() override;
    Bool InitCapabilities() override;
    Bool InitWindowSurface() override;
    Bool InitializeEGLDisplay(EGLDisplay dpy, EGLint* major, EGLint* minor) override;
    Bool CreateEGLWindowSurface(EGLSurface surface, const MG_Backend::WindowHandle& handle) override;
    Bool CreateEGLPbufferSurface(EGLSurface surface, EGLint width, EGLint height) override;
    Bool MakeEGLCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) override;
    Bool SwapEGLBuffers(EGLDisplay dpy, EGLSurface draw) override;
    void ReleaseEGLSurface(EGLSurface surface) override;
    void ReleaseEGLResources() override;

    const MG_Backend::RendererInfo& GetRendererInfo() const override;
    String GetBackendAPIVersionString() const override;
    const MG_Backend::GlobalBackendFunctionsTable& GetBackendFunctions() const override;
    const MG_Backend::DynamicBackendParameters& GetDynamicParameters() const override;
    MG_Backend::BackendType GetBackendType() const override;

    const MG_External::GLESFunctionsTable& GetGLESFunctions() const;
    const MG_External::EGLFunctionsTable& GetEGLFunctions() const;

private:
    void UpdateDynamicBackendParameters();
    Bool InitPbufferSurface(EGLint width, EGLint height) override;
    void OnEGLSurfaceReleased(EGLSurface surface) override;

    Bool m_initialized = false;
    MG_External::EGLFunctionsTable m_EGLFunctions;
    MG_External::GLESFunctionsTable m_GLESFunctions;
    MG_External::GLESCapabilities m_GLESCapabilities;
    MG_Backend::DynamicBackendParameters m_dynamicParameters;
    MG_Backend::RendererInfo m_rendererInfo;
    MG_Backend::GlobalBackendFunctionsTable m_backendFunctions;
};

} // namespace MobileGL::backend::DirectGLES
