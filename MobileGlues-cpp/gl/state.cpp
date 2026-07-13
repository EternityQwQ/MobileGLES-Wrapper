// MobileGlues - gl/state.cpp
// Unified OpenGL State Manager implementation
//
// Design (inspired by MobileGL-DirectGLES):
//   - RenderState: version-tracked dirty flag for efficient backend sync
//   - ErrorState: structured error recording (GL + non-GL errors)
//   - Nested subsystem state structs for backward compatibility
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "state.h"
#include "../gles/loader.h"
#include <cstring>

// Forward-declared from buffer.cpp (C-linkage, declared in buffer.h);
// updates the cached fast-path flag g_atomicCountersActive. Called from
// BufferState::Reset() so that after a context reset the draw hot path
// doesn't try to sync phantom atomic-counter state.
extern "C" void mg_update_atomic_counters_active_flag();

// ============================================================================
// ErrorState Implementation
// ============================================================================

void ErrorState::RecordError(ErrorCode code, std::unique_ptr<ErrorInfo> info) {
    if (code == ErrorCode::NoError) {
        MG_LOG_ERROR("Recording Non-OpenGL error: %s", info->ToString().c_str());
        m_nonGLErrors.push_back(std::make_unique<Error>(code, std::move(info)));
    } else {
        MG_LOG_ERROR("Recording OpenGL error (%u): %s",
                     static_cast<uint32_t>(code), info->ToString().c_str());
        m_errors.push_back(std::make_unique<Error>(code, std::move(info)));
    }
}

bool ErrorState::HasGLError() const {
    return !m_errors.empty();
}

const Error* ErrorState::PeekGLError() const {
    if (m_errors.empty()) return nullptr;
    return m_errors.front().get();
}

std::unique_ptr<Error> ErrorState::PopGLError() {
    if (m_errors.empty()) return nullptr;
    // deque::pop_front is O(1); vector::erase(begin()) was O(n).
    auto error = std::move(m_errors.front());
    m_errors.pop_front();
    return error;
}

bool ErrorState::HasNonGLError() const {
    return !m_nonGLErrors.empty();
}

const Error* ErrorState::PeekNonGLError() const {
    if (m_nonGLErrors.empty()) return nullptr;
    return m_nonGLErrors.front().get();
}

std::unique_ptr<Error> ErrorState::PopNonGLError() {
    if (m_nonGLErrors.empty()) return nullptr;
    auto error = std::move(m_nonGLErrors.front());
    m_nonGLErrors.pop_front();
    return error;
}

void ErrorState::Clear() {
    m_errors.clear();
    m_nonGLErrors.clear();
}

// ============================================================================
// RenderState Implementation (version-tracked dirty flag)
// ============================================================================

// --- Rasterization ---

void RenderState::SetViewport(int32_t x, int32_t y, int32_t width, int32_t height) {
    m_parameters.Viewport[0] = x;
    m_parameters.Viewport[1] = y;
    m_parameters.Viewport[2] = width;
    m_parameters.Viewport[3] = height;
    ++m_version;
}

void RenderState::GetViewport(int32_t* out) const {
    out[0] = m_parameters.Viewport[0];
    out[1] = m_parameters.Viewport[1];
    out[2] = m_parameters.Viewport[2];
    out[3] = m_parameters.Viewport[3];
}

void RenderState::SetLineWidth(float width) {
    m_parameters.LineWidth = width;
    ++m_version;
}

float RenderState::GetLineWidth() const {
    return m_parameters.LineWidth;
}

void RenderState::SetPointSize(float size) {
    m_parameters.PointSize = size;
    ++m_version;
}

float RenderState::GetPointSize() const {
    return m_parameters.PointSize;
}

void RenderState::SetPolygonOffset(float factor, float units) {
    m_parameters.PolygonOffsetFactor = factor;
    m_parameters.PolygonOffsetUnits = units;
    ++m_version;
}

float RenderState::GetPolygonOffsetFactor() const {
    return m_parameters.PolygonOffsetFactor;
}

float RenderState::GetPolygonOffsetUnits() const {
    return m_parameters.PolygonOffsetUnits;
}

// --- Capabilities ---

void RenderState::SetCapability(Capability cap, bool enabled) {
#define SET_CAP(capName, field) \
    case Capability::capName: \
        m_parameters.field = enabled; \
        ++m_version; \
        break;

    switch (cap) {
        SET_CAP(ColorLogicOp, ColorLogicOpEnabled)
        SET_CAP(DebugOutput, DebugOutputEnabled)
        SET_CAP(DebugOutputSynchronous, DebugOutputSynchronousEnabled)
        SET_CAP(DepthTest, DepthTestEnabled)
        SET_CAP(CullFace, CullFaceEnabled)
        SET_CAP(Dither, DitherEnabled)
        SET_CAP(LineSmooth, LineSmoothEnabled)
        SET_CAP(Multisample, MultisampleEnabled)
        SET_CAP(PolygonOffsetFill, PolygonOffsetFillEnabled)
        SET_CAP(PolygonOffsetLine, PolygonOffsetLineEnabled)
        SET_CAP(PolygonOffsetPoint, PolygonOffsetPointEnabled)
        SET_CAP(PolygonSmooth, PolygonSmoothEnabled)
        SET_CAP(PrimitiveRestart, PrimitiveRestartEnabled)
        SET_CAP(PrimitiveRestartFixedIndex, PrimitiveRestartFixedIndexEnabled)
        SET_CAP(RasterizerDiscard, RasterizerDiscardEnabled)
        SET_CAP(SampleAlphaToCoverage, SampleAlphaToCoverageEnabled)
        SET_CAP(SampleAlphaToOne, SampleAlphaToOneEnabled)
        SET_CAP(SampleCoverage, SampleCoverageEnabled)
        SET_CAP(SampleMask, SampleMaskEnabled)
        SET_CAP(ScissorTest, ScissorTestEnabled)
        SET_CAP(StencilTest, StencilTestEnabled)
        SET_CAP(ProgramPointSize, ProgramPointSizeEnabled)
    case Capability::Blend: {
        for (auto& blendState : m_parameters.BlendStates) {
            blendState.Enabled = enabled;
        }
        ++m_version;
        break;
    }
    default:
        break;
    }
#undef SET_CAP
}

bool RenderState::IsCapabilityEnabled(Capability cap) const {
#define RETURN_CAP(capName, field) \
    case Capability::capName: return m_parameters.field;
    switch (cap) {
        RETURN_CAP(ColorLogicOp, ColorLogicOpEnabled)
        RETURN_CAP(DebugOutput, DebugOutputEnabled)
        RETURN_CAP(DebugOutputSynchronous, DebugOutputSynchronousEnabled)
        RETURN_CAP(DepthTest, DepthTestEnabled)
        RETURN_CAP(CullFace, CullFaceEnabled)
        RETURN_CAP(Dither, DitherEnabled)
        RETURN_CAP(LineSmooth, LineSmoothEnabled)
        RETURN_CAP(Multisample, MultisampleEnabled)
        RETURN_CAP(PolygonOffsetFill, PolygonOffsetFillEnabled)
        RETURN_CAP(PolygonOffsetLine, PolygonOffsetLineEnabled)
        RETURN_CAP(PolygonOffsetPoint, PolygonOffsetPointEnabled)
        RETURN_CAP(PolygonSmooth, PolygonSmoothEnabled)
        RETURN_CAP(PrimitiveRestart, PrimitiveRestartEnabled)
        RETURN_CAP(PrimitiveRestartFixedIndex, PrimitiveRestartFixedIndexEnabled)
        RETURN_CAP(RasterizerDiscard, RasterizerDiscardEnabled)
        RETURN_CAP(SampleAlphaToCoverage, SampleAlphaToCoverageEnabled)
        RETURN_CAP(SampleAlphaToOne, SampleAlphaToOneEnabled)
        RETURN_CAP(SampleCoverage, SampleCoverageEnabled)
        RETURN_CAP(SampleMask, SampleMaskEnabled)
        RETURN_CAP(ScissorTest, ScissorTestEnabled)
        RETURN_CAP(StencilTest, StencilTestEnabled)
        RETURN_CAP(ProgramPointSize, ProgramPointSizeEnabled)
    case Capability::Blend:
        return m_parameters.BlendStates[0].Enabled;
    default:
        return false;
    }
#undef RETURN_CAP
}

void RenderState::SetCapabilityIndexed(Capability cap, uint32_t index, bool enabled) {
    if (cap != Capability::Blend) return;
    if (index >= static_cast<uint32_t>(MAX_DRAW_BUFFERS)) return;
    m_parameters.BlendStates[index].Enabled = enabled;
    ++m_version;
}

bool RenderState::IsCapabilityEnabledIndexed(Capability cap, uint32_t index) const {
    if (cap != Capability::Blend) return false;
    if (index >= static_cast<uint32_t>(MAX_DRAW_BUFFERS)) return false;
    return m_parameters.BlendStates[index].Enabled;
}

// --- Blending ---

void RenderState::SetBlendFunc(BlendFactor srcRGB, BlendFactor dstRGB,
                               BlendFactor srcAlpha, BlendFactor dstAlpha) {
    for (auto& blendState : m_parameters.BlendStates) {
        blendState.SrcFactorRGB = srcRGB;
        blendState.DstFactorRGB = dstRGB;
        blendState.SrcFactorAlpha = srcAlpha;
        blendState.DstFactorAlpha = dstAlpha;
    }
    ++m_version;
}

void RenderState::GetBlendFunc(BlendFactor& srcRGB, BlendFactor& dstRGB,
                               BlendFactor& srcAlpha, BlendFactor& dstAlpha) const {
    srcRGB = m_parameters.BlendStates[0].SrcFactorRGB;
    dstRGB = m_parameters.BlendStates[0].DstFactorRGB;
    srcAlpha = m_parameters.BlendStates[0].SrcFactorAlpha;
    dstAlpha = m_parameters.BlendStates[0].DstFactorAlpha;
}

void RenderState::SetBlendFuncIndexed(uint32_t index, BlendFactor srcRGB, BlendFactor dstRGB,
                                      BlendFactor srcAlpha, BlendFactor dstAlpha) {
    if (index >= static_cast<uint32_t>(MAX_DRAW_BUFFERS)) return;
    PerBufferBlendState& blendState = m_parameters.BlendStates[index];
    blendState.SrcFactorRGB = srcRGB;
    blendState.DstFactorRGB = dstRGB;
    blendState.SrcFactorAlpha = srcAlpha;
    blendState.DstFactorAlpha = dstAlpha;
    ++m_version;
}

void RenderState::GetBlendFuncIndexed(uint32_t index, BlendFactor& srcRGB, BlendFactor& dstRGB,
                                      BlendFactor& srcAlpha, BlendFactor& dstAlpha) const {
    if (index >= static_cast<uint32_t>(MAX_DRAW_BUFFERS)) return;
    srcRGB = m_parameters.BlendStates[index].SrcFactorRGB;
    dstRGB = m_parameters.BlendStates[index].DstFactorRGB;
    srcAlpha = m_parameters.BlendStates[index].SrcFactorAlpha;
    dstAlpha = m_parameters.BlendStates[index].DstFactorAlpha;
}

void RenderState::SetBlendEquation(BlendEquation color, BlendEquation alpha) {
    for (auto& blendState : m_parameters.BlendStates) {
        blendState.ColorEquation = color;
        blendState.AlphaEquation = alpha;
    }
    ++m_version;
}

void RenderState::GetBlendEquation(BlendEquation& color, BlendEquation& alpha) const {
    color = m_parameters.BlendStates[0].ColorEquation;
    alpha = m_parameters.BlendStates[0].AlphaEquation;
}

void RenderState::SetBlendEquationIndexed(uint32_t index, BlendEquation color, BlendEquation alpha) {
    if (index >= static_cast<uint32_t>(MAX_DRAW_BUFFERS)) return;
    PerBufferBlendState& blendState = m_parameters.BlendStates[index];
    blendState.ColorEquation = color;
    blendState.AlphaEquation = alpha;
    ++m_version;
}

void RenderState::GetBlendEquationIndexed(uint32_t index, BlendEquation& color, BlendEquation& alpha) const {
    if (index >= static_cast<uint32_t>(MAX_DRAW_BUFFERS)) return;
    color = m_parameters.BlendStates[index].ColorEquation;
    alpha = m_parameters.BlendStates[index].AlphaEquation;
}

void RenderState::SetLogicOp(LogicOperation logicOp) {
    m_parameters.LogicOp = logicOp;
    ++m_version;
}

LogicOperation RenderState::GetLogicOp() const {
    return m_parameters.LogicOp;
}

// --- Depth ---

void RenderState::SetDepthFunc(DepthTestFunc func) {
    m_parameters.DepthFunc = func;
    ++m_version;
}

DepthTestFunc RenderState::GetDepthFunc() const {
    return m_parameters.DepthFunc;
}

void RenderState::SetDepthMask(bool flag) {
    m_parameters.DepthMask = flag;
    ++m_version;
}

bool RenderState::GetDepthMask() const {
    return m_parameters.DepthMask;
}

void RenderState::SetStencilFunc(uint32_t faceIndex, DepthTestFunc func, int32_t ref, uint32_t mask) {
    if (faceIndex >= 2) return;
    StencilFaceState& state = m_parameters.StencilStates[faceIndex];
    state.Func = func;
    state.Ref = ref;
    state.ValueMask = mask;
    ++m_version;
}

void RenderState::SetStencilMask(uint32_t faceIndex, uint32_t mask) {
    if (faceIndex >= 2) return;
    StencilFaceState& state = m_parameters.StencilStates[faceIndex];
    state.WriteMask = mask;
    ++m_version;
}

void RenderState::SetStencilOp(uint32_t faceIndex, StencilOperation fail,
                               StencilOperation depthFail, StencilOperation depthPass) {
    if (faceIndex >= 2) return;
    StencilFaceState& state = m_parameters.StencilStates[faceIndex];
    state.FailOp = fail;
    state.PassDepthFailOp = depthFail;
    state.PassDepthPassOp = depthPass;
    ++m_version;
}

const StencilFaceState& RenderState::GetStencilState(uint32_t faceIndex) const {
    return m_parameters.StencilStates[faceIndex < 2 ? faceIndex : 0];
}

// --- Color Mask ---

void RenderState::SetColorMask(bool r, bool g, bool b, bool a) {
    m_parameters.ColorMaskR = r;
    m_parameters.ColorMaskG = g;
    m_parameters.ColorMaskB = b;
    m_parameters.ColorMaskA = a;
    ++m_version;
}

void RenderState::GetColorMask(bool* out) const {
    out[0] = m_parameters.ColorMaskR;
    out[1] = m_parameters.ColorMaskG;
    out[2] = m_parameters.ColorMaskB;
    out[3] = m_parameters.ColorMaskA;
}

// --- Clear State ---

void RenderState::SetClearColor(float r, float g, float b, float a) {
    m_parameters.ClearColor[0] = r;
    m_parameters.ClearColor[1] = g;
    m_parameters.ClearColor[2] = b;
    m_parameters.ClearColor[3] = a;
    ++m_version;
}

void RenderState::GetClearColor(float* out) const {
    out[0] = m_parameters.ClearColor[0];
    out[1] = m_parameters.ClearColor[1];
    out[2] = m_parameters.ClearColor[2];
    out[3] = m_parameters.ClearColor[3];
}

void RenderState::SetClearDepth(float depth) {
    m_parameters.ClearDepth = depth;
    ++m_version;
}

float RenderState::GetClearDepth() const {
    return m_parameters.ClearDepth;
}

void RenderState::SetClearStencil(int32_t stencil) {
    m_parameters.ClearStencil = static_cast<uint32_t>(stencil);
    ++m_version;
}

uint32_t RenderState::GetClearStencil() const {
    return m_parameters.ClearStencil;
}

void RenderState::SetBlendColor(float r, float g, float b, float a) {
    m_parameters.BlendColor[0] = r;
    m_parameters.BlendColor[1] = g;
    m_parameters.BlendColor[2] = b;
    m_parameters.BlendColor[3] = a;
    ++m_version;
}

void RenderState::GetBlendColor(float* out) const {
    out[0] = m_parameters.BlendColor[0];
    out[1] = m_parameters.BlendColor[1];
    out[2] = m_parameters.BlendColor[2];
    out[3] = m_parameters.BlendColor[3];
}

void RenderState::SetDepthRange(float nearVal, float farVal) {
    m_parameters.DepthRangeNear = nearVal;
    m_parameters.DepthRangeFar = farVal;
    ++m_version;
}

void RenderState::GetDepthRange(float* out) const {
    out[0] = m_parameters.DepthRangeNear;
    out[1] = m_parameters.DepthRangeFar;
}

void RenderState::SetSampleCoverage(float value, bool invert) {
    m_parameters.SampleCoverageValue = value;
    m_parameters.SampleCoverageInvert = invert;
    ++m_version;
}

float RenderState::GetSampleCoverageValue() const {
    return m_parameters.SampleCoverageValue;
}

bool RenderState::GetSampleCoverageInvert() const {
    return m_parameters.SampleCoverageInvert;
}

void RenderState::SetSampleMaskValue(uint32_t mask) {
    m_parameters.SampleMaskValue = mask;
    ++m_version;
}

uint32_t RenderState::GetSampleMaskValue() const {
    return m_parameters.SampleMaskValue;
}

// --- Pixel Store ---

void RenderState::SetPixelStoreParam(PixelStoreParam param, int32_t value) {
    switch (param) {
#define SET_PS(paramHead, paramTail) \
    case PixelStoreParam::paramHead##paramTail: \
        m_pixelStore##paramHead##Parameters.paramTail = value; \
        break;
    SET_PS(Pack, Alignment)
    SET_PS(Pack, RowLength)
    SET_PS(Pack, ImageHeight)
    SET_PS(Pack, SkipPixels)
    SET_PS(Pack, SkipRows)
    SET_PS(Pack, SkipImages)
    case PixelStoreParam::PackSwapBytes:
        m_pixelStorePackParameters.SwapBytes = (value != 0);
        break;
    case PixelStoreParam::PackLSBFirst:
        m_pixelStorePackParameters.LSBFirst = (value != 0);
        break;
    SET_PS(Unpack, Alignment)
    SET_PS(Unpack, RowLength)
    SET_PS(Unpack, ImageHeight)
    SET_PS(Unpack, SkipPixels)
    SET_PS(Unpack, SkipRows)
    SET_PS(Unpack, SkipImages)
    case PixelStoreParam::UnpackSwapBytes:
        m_pixelStoreUnpackParameters.SwapBytes = (value != 0);
        break;
    case PixelStoreParam::UnpackLSBFirst:
        m_pixelStoreUnpackParameters.LSBFirst = (value != 0);
        break;
    default:
        break;
    }
#undef SET_PS
}

int32_t RenderState::GetPixelStoreParam(PixelStoreParam param) const {
    switch (param) {
#define RETURN_PS(paramHead, paramTail) \
    case PixelStoreParam::paramHead##paramTail: return m_pixelStore##paramHead##Parameters.paramTail;
    RETURN_PS(Pack, Alignment)
    RETURN_PS(Pack, RowLength)
    RETURN_PS(Pack, ImageHeight)
    RETURN_PS(Pack, SkipPixels)
    RETURN_PS(Pack, SkipRows)
    RETURN_PS(Pack, SkipImages)
    case PixelStoreParam::PackSwapBytes: return m_pixelStorePackParameters.SwapBytes ? 1 : 0;
    case PixelStoreParam::PackLSBFirst: return m_pixelStorePackParameters.LSBFirst ? 1 : 0;
    RETURN_PS(Unpack, Alignment)
    RETURN_PS(Unpack, RowLength)
    RETURN_PS(Unpack, ImageHeight)
    RETURN_PS(Unpack, SkipPixels)
    RETURN_PS(Unpack, SkipRows)
    RETURN_PS(Unpack, SkipImages)
    case PixelStoreParam::UnpackSwapBytes: return m_pixelStoreUnpackParameters.SwapBytes ? 1 : 0;
    case PixelStoreParam::UnpackLSBFirst: return m_pixelStoreUnpackParameters.LSBFirst ? 1 : 0;
    default: return 0;
    }
#undef RETURN_PS
}

PixelStoreParameters RenderState::GetPixelStoreParameters(bool isUnpack) const {
    return isUnpack ? m_pixelStoreUnpackParameters : m_pixelStorePackParameters;
}

// --- Cull Face ---

void RenderState::SetCullFaceMode(CullFaceMode mode) {
    m_parameters.CullFaceModeSetting = mode;
    ++m_version;
}

CullFaceMode RenderState::GetCullFaceMode() const {
    return m_parameters.CullFaceModeSetting;
}

void RenderState::SetFrontFaceMode(FrontFaceMode mode) {
    m_parameters.FrontFaceModeSetting = mode;
    ++m_version;
}

FrontFaceMode RenderState::GetFrontFaceMode() const {
    return m_parameters.FrontFaceModeSetting;
}

void RenderState::SetProvokingVertexMode(ProvokingVertexMode mode) {
    m_parameters.ProvokingVertexModeSetting = mode;
    ++m_version;
}

ProvokingVertexMode RenderState::GetProvokingVertexMode() const {
    return m_parameters.ProvokingVertexModeSetting;
}

// --- Scissor ---

void RenderState::SetScissorBox(int32_t x, int32_t y, int32_t width, int32_t height) {
    m_parameters.ScissorBox[0] = x;
    m_parameters.ScissorBox[1] = y;
    m_parameters.ScissorBox[2] = width;
    m_parameters.ScissorBox[3] = height;
    ++m_version;
}

void RenderState::GetScissorBox(int32_t* out) const {
    out[0] = m_parameters.ScissorBox[0];
    out[1] = m_parameters.ScissorBox[1];
    out[2] = m_parameters.ScissorBox[2];
    out[3] = m_parameters.ScissorBox[3];
}

// ============================================================================
// GLStateManager::Initialize
// ============================================================================

void GLStateManager::Initialize()
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    STATE_LOG("GLStateManager::Initialize()");

    // Query ES version
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    esVersion = major * 10 + minor;

    // Reset all subsystems
    buffer.Reset();
    texture.Reset();
    framebuffer.Reset();
    shader.Reset();
    sampler.Reset();
    image.Reset();
    legacy.Reset();
    vertexArray.Reset();
    query.Reset();
    transformFeedback.Reset();
    renderbuffer.Reset();
    errorState.Clear();
}

void GLStateManager::Shutdown()
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    STATE_LOG("GLStateManager::Shutdown()");

    // Clean up temp FBOs
    for (auto& tmp : framebuffer.tempFBOs) {
        if (tmp.fbo) glDeleteFramebuffers(1, &tmp.fbo);
        if (tmp.texture) glDeleteTextures(1, &tmp.texture);
    }
    framebuffer.tempFBOs.clear();

    buffer.Reset();
    texture.Reset();
    framebuffer.Reset();
    shader.Reset();
    sampler.Reset();
    image.Reset();
    legacy.Reset();
    vertexArray.Reset();
    query.Reset();
    transformFeedback.Reset();
    renderbuffer.Reset();
    errorState.Clear();
}

// ============================================================================
// Subsystem Reset Methods
// ============================================================================

void GLStateManager::BufferState::Reset()
{
    bufferMap.clear();
    bufferMapReverse.clear();
    bufferInfo.clear();
    vaoMap.clear();
    vaoMapReverse.clear();
    boundBuffer.clear();
    memset(uniformBufferBases, 0, sizeof(uniformBufferBases));
    memset(uniformBufferOffsets, 0, sizeof(uniformBufferOffsets));
    memset(uniformBufferSizes, 0, sizeof(uniformBufferSizes));
    memset(shaderStorageBases, 0, sizeof(shaderStorageBases));
    memset(atomicCounterBases, 0, sizeof(atomicCounterBases));
    memset(transformFeedbackBuffers, 0, sizeof(transformFeedbackBuffers));
    bufferMaps.clear();
    atomicCounterData.clear();
    atomicCounterBufferBinding = 0;
    atomicCounterBufferSize = 0;
    atomicCounterBufferOffset = 0;
    // Refresh the cached draw-path flag after zeroing the atomic-counter
    // state; otherwise stale "active" would persist across context resets.
    mg_update_atomic_counters_active_flag();
}

void GLStateManager::TextureState::Reset()
{
    textureMap.clear();
    textureMapReverse.clear();
    for (int i = 0; i < MAX_TEXTURE_UNITS; i++) {
        texUnits[i].binding.clear();
    }
    activeUnit = 0;
    unpackAlignment = 4;
    unpackRowLength = 0;
    unpackImageHeight = 0;
    unpackSkipPixels = 0;
    unpackSkipRows = 0;
    unpackSkipImages = 0;
    packAlignment = 4;
    packRowLength = 0;
    packImageHeight = 0;
    packSkipPixels = 0;
    packSkipRows = 0;
    packSkipImages = 0;
}

void GLStateManager::FramebufferState::Reset()
{
    fboMap.clear();
    fboMapReverse.clear();
    drawFBO = 0;
    readFBO = 0;
    attachments.clear();
    memset(drawBuffers, 0, sizeof(drawBuffers));
    drawBuffers[0] = GL_BACK;
    readBuffer = GL_BACK;
    drawBufferCount = 1;
    tempFBOs.clear();
    defaultFboInvalidated = false;
}

void GLStateManager::ShaderState::Reset()
{
    shaderMap.clear();
    shaderMapReverse.clear();
    programMap.clear();
    programMapReverse.clear();
    shaderInfo.clear();
    programInfo.clear();
    currentProgram = 0;
}

// ============================================================================
// Format Conversion Utilities
// ============================================================================

// Map desktop GL texture targets to GLES-compatible targets.
// 1D textures → 2D, 1D array → 2D array, rectangle → 2D, etc.
GLenum GLStateManager::ConvertTextureTarget(GLenum target)
{
    switch (target) {
        case GL_TEXTURE_1D:
        case GL_PROXY_TEXTURE_1D:
            return GL_TEXTURE_2D;
        case GL_TEXTURE_1D_ARRAY:
            return GL_TEXTURE_2D_ARRAY;
        case GL_TEXTURE_RECTANGLE:
            return GL_TEXTURE_2D;
        case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
        case GL_TEXTURE_2D_MULTISAMPLE:
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        case GL_TEXTURE_BUFFER:
            return target;
        default:
            return GL_TEXTURE_2D;
    }
}

// Convert desktop GL internal formats to GLES-compatible equivalents.
GLenum GLStateManager::ConvertInternalFormat(GLenum format)
{
    switch (format) {
        // Legacy luminance formats → R
        case GL_LUMINANCE:           return GL_R8;
        case GL_LUMINANCE4:          return GL_R8;
        case GL_LUMINANCE8:          return GL_R8;
        case GL_LUMINANCE12:         return GL_R8;
        case GL_LUMINANCE16:         return GL_R16;
        case GL_LUMINANCE_ALPHA:     return GL_RG8;
        case GL_LUMINANCE4_ALPHA4:   return GL_RG8;
        case GL_LUMINANCE6_ALPHA2:   return GL_RG8;
        case GL_LUMINANCE8_ALPHA8:   return GL_RG8;
        case GL_LUMINANCE12_ALPHA4:  return GL_RG8;
        case GL_LUMINANCE12_ALPHA12: return GL_RG8;
        case GL_LUMINANCE16_ALPHA16: return GL_RG16;
        // Legacy intensity formats → R
        case GL_INTENSITY:           return GL_R8;
        case GL_INTENSITY4:          return GL_R8;
        case GL_INTENSITY8:          return GL_R8;
        case GL_INTENSITY12:         return GL_R8;
        case GL_INTENSITY16:         return GL_R16;
        // Legacy alpha formats → R
        case GL_ALPHA4:              return GL_R8;
        case GL_ALPHA8:              return GL_R8;
        case GL_ALPHA12:             return GL_R8;
        case GL_ALPHA16:             return GL_R16;
        // Legacy RGB formats
        case GL_R3_G3_B2:            return GL_RGB8;
        case GL_RGB4:                return GL_RGB8;
        case GL_RGB5:                return GL_RGB8;
        case GL_RGB8:                return GL_RGB8;
        case GL_RGB10:               return GL_RGB10;
        case GL_RGB12:               return GL_RGB8;
        case GL_RGB16:               return GL_RGB16;
        // Legacy RGBA formats
        case GL_RGBA2:               return GL_RGBA8;
        case GL_RGBA4:               return GL_RGBA8;
        case GL_RGB5_A1:             return GL_RGB5_A1;
        case GL_RGBA8:               return GL_RGBA8;
        case GL_RGB10_A2:            return GL_RGB10_A2;
        case GL_RGBA12:              return GL_RGBA8;
        case GL_RGBA16:              return GL_RGBA16;
        // Depth/stencil
        case GL_DEPTH24_STENCIL8:    return GL_DEPTH24_STENCIL8;
        case GL_DEPTH32F_STENCIL8:   return GL_DEPTH32F_STENCIL8;
        case GL_DEPTH_COMPONENT16:   return GL_DEPTH_COMPONENT16;
        case GL_DEPTH_COMPONENT24:   return GL_DEPTH_COMPONENT24;
        case GL_DEPTH_COMPONENT32:   return GL_DEPTH_COMPONENT32F;
        case GL_DEPTH_COMPONENT32F:  return GL_DEPTH_COMPONENT32F;
        // Compressed formats pass through
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_RED_RGTC1:
        case GL_COMPRESSED_RG_RGTC2:
        case GL_COMPRESSED_SIGNED_RED_RGTC1:
        case GL_COMPRESSED_SIGNED_RG_RGTC2:
        case GL_COMPRESSED_RGBA_BPTC_UNORM:
        case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
        case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:
        case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:
        case GL_COMPRESSED_RGB8_ETC2:
        case GL_COMPRESSED_SRGB8_ETC2:
        case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        case GL_COMPRESSED_RGBA8_ETC2_EAC:
        case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
        case GL_COMPRESSED_R11_EAC:
        case GL_COMPRESSED_SIGNED_R11_EAC:
        case GL_COMPRESSED_RG11_EAC:
        case GL_COMPRESSED_SIGNED_RG11_EAC:
        case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
        case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
            return format;
        // sRGB
        case GL_SRGB8:               return GL_SRGB8;
        case GL_SRGB8_ALPHA8:        return GL_SRGB8_ALPHA8;
        // Integer formats
        case GL_R8I:                 return GL_R8I;
        case GL_R8UI:                return GL_R8UI;
        case GL_R16I:                return GL_R16I;
        case GL_R16UI:               return GL_R16UI;
        case GL_R32I:                return GL_R32I;
        case GL_R32UI:               return GL_R32UI;
        case GL_RG8I:                return GL_RG8I;
        case GL_RG8UI:               return GL_RG8UI;
        case GL_RG16I:               return GL_RG16I;
        case GL_RG16UI:              return GL_RG16UI;
        case GL_RG32I:               return GL_RG32I;
        case GL_RG32UI:              return GL_RG32UI;
        case GL_RGB8I:               return GL_RGB8I;
        case GL_RGB8UI:              return GL_RGB8UI;
        case GL_RGB16I:              return GL_RGB16I;
        case GL_RGB16UI:             return GL_RGB16UI;
        case GL_RGB32I:              return GL_RGB32I;
        case GL_RGB32UI:             return GL_RGB32UI;
        case GL_RGBA8I:              return GL_RGBA8I;
        case GL_RGBA8UI:             return GL_RGBA8UI;
        case GL_RGBA16I:             return GL_RGBA16I;
        case GL_RGBA16UI:            return GL_RGBA16UI;
        case GL_RGBA32I:             return GL_RGBA32I;
        case GL_RGBA32UI:            return GL_RGBA32UI;
        // Float formats
        case GL_R16F:                return GL_R16F;
        case GL_RG16F:               return GL_RG16F;
        case GL_RGB16F:              return GL_RGB16F;
        case GL_RGBA16F:             return GL_RGBA16F;
        case GL_R32F:                return GL_R32F;
        case GL_RG32F:               return GL_RG32F;
        case GL_RGB32F:              return GL_RGB32F;
        case GL_RGBA32F:             return GL_RGBA32F;
        case GL_R11F_G11F_B10F:      return GL_R11F_G11F_B10F;
        case GL_RGB9_E5:             return GL_RGB9_E5;
        // Default: pass through
        default:
            return format;
    }
}

// Convert desktop GL pixel data formats to GLES-compatible equivalents.
GLenum GLStateManager::ConvertFormat(GLenum format)
{
    switch (format) {
        case GL_LUMINANCE:           return GL_RED;
        case GL_LUMINANCE_ALPHA:     return GL_RG;
        case GL_INTENSITY:           return GL_RED;
        case GL_ALPHA:               return GL_RED;
        case GL_BGR:                 return GL_RGB;
        case GL_BGRA:                return GL_RGBA;
        case GL_RED:                 return GL_RED;
        case GL_RG:                  return GL_RG;
        case GL_RGB:                 return GL_RGB;
        case GL_RGBA:                return GL_RGBA;
        case GL_DEPTH_COMPONENT:     return GL_DEPTH_COMPONENT;
        case GL_DEPTH_STENCIL:       return GL_DEPTH_STENCIL;
        case GL_STENCIL_INDEX:       return GL_STENCIL_INDEX;
        default:                     return format;
    }
}

// Convert desktop GL types to GLES-compatible equivalents.
GLenum GLStateManager::ConvertType(GLenum type)
{
    switch (type) {
        case GL_UNSIGNED_BYTE_3_3_2:        return GL_UNSIGNED_BYTE;
        case GL_UNSIGNED_BYTE_2_3_3_REV:    return GL_UNSIGNED_BYTE;
        case GL_UNSIGNED_SHORT_5_6_5:       return GL_UNSIGNED_SHORT_5_6_5;
        case GL_UNSIGNED_SHORT_5_6_5_REV:   return GL_UNSIGNED_SHORT_5_6_5;
        case GL_UNSIGNED_SHORT_4_4_4_4:     return GL_UNSIGNED_SHORT_4_4_4_4;
        case GL_UNSIGNED_SHORT_4_4_4_4_REV: return GL_UNSIGNED_SHORT_4_4_4_4;
        case GL_UNSIGNED_SHORT_5_5_5_1:     return GL_UNSIGNED_SHORT_5_5_5_1;
        case GL_UNSIGNED_SHORT_1_5_5_5_REV: return GL_UNSIGNED_SHORT_5_5_5_1;
        case GL_UNSIGNED_INT_8_8_8_8:       return GL_UNSIGNED_BYTE;
        case GL_UNSIGNED_INT_8_8_8_8_REV:   return GL_UNSIGNED_BYTE;
        case GL_UNSIGNED_INT_10_10_10_2:    return GL_UNSIGNED_INT_10_10_10_2;
        case GL_UNSIGNED_INT_2_10_10_10_REV:return GL_UNSIGNED_INT_2_10_10_10_REV;
        case GL_UNSIGNED_BYTE:              return GL_UNSIGNED_BYTE;
        case GL_BYTE:                       return GL_BYTE;
        case GL_UNSIGNED_SHORT:             return GL_UNSIGNED_SHORT;
        case GL_SHORT:                      return GL_SHORT;
        case GL_UNSIGNED_INT:               return GL_UNSIGNED_INT;
        case GL_INT:                        return GL_INT;
        case GL_FLOAT:                      return GL_FLOAT;
        case GL_HALF_FLOAT:                 return GL_HALF_FLOAT;
        case GL_UNSIGNED_INT_24_8:          return GL_UNSIGNED_INT_24_8;
        default:                            return type;
    }
}

bool GLStateManager::IsDepthStencilFormat(GLenum format)
{
    switch (format) {
        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT24:
        case GL_DEPTH_COMPONENT32:
        case GL_DEPTH_COMPONENT32F:
        case GL_DEPTH24_STENCIL8:
        case GL_DEPTH32F_STENCIL8:
        case GL_DEPTH_STENCIL:
        case GL_DEPTH_COMPONENT:
            return true;
        default:
            return false;
    }
}

bool GLStateManager::IsCompressedFormat(GLenum format)
{
    switch (format) {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_RED_RGTC1:
        case GL_COMPRESSED_RG_RGTC2:
        case GL_COMPRESSED_SIGNED_RED_RGTC1:
        case GL_COMPRESSED_SIGNED_RG_RGTC2:
        case GL_COMPRESSED_RGBA_BPTC_UNORM:
        case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
        case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:
        case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:
        case GL_COMPRESSED_RGB8_ETC2:
        case GL_COMPRESSED_SRGB8_ETC2:
        case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        case GL_COMPRESSED_RGBA8_ETC2_EAC:
        case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
        case GL_COMPRESSED_R11_EAC:
        case GL_COMPRESSED_SIGNED_R11_EAC:
        case GL_COMPRESSED_RG11_EAC:
        case GL_COMPRESSED_SIGNED_RG11_EAC:
            return true;
        default:
            return false;
    }
}

// Map a texture target to its corresponding binding query enum.
GLenum GLStateManager::GetTextureBindingTarget(GLenum target)
{
    switch (target) {
        case GL_TEXTURE_1D:       return GL_TEXTURE_BINDING_1D;
        case GL_TEXTURE_2D:       return GL_TEXTURE_BINDING_2D;
        case GL_TEXTURE_3D:       return GL_TEXTURE_BINDING_3D;
        case GL_TEXTURE_CUBE_MAP: return GL_TEXTURE_BINDING_CUBE_MAP;
        case GL_TEXTURE_1D_ARRAY: return GL_TEXTURE_BINDING_1D_ARRAY;
        case GL_TEXTURE_2D_ARRAY: return GL_TEXTURE_BINDING_2D_ARRAY;
        default:                  return GL_TEXTURE_BINDING_2D;
    }
}

// Map a texture target to its binding target enum (for tracking).
GLenum GLStateManager::TargetToBindingTarget(GLenum target)
{
    switch (target) {
        case GL_TEXTURE_1D:                 return GL_TEXTURE_1D;
        case GL_TEXTURE_2D:                 return GL_TEXTURE_2D;
        case GL_TEXTURE_3D:                 return GL_TEXTURE_3D;
        case GL_TEXTURE_CUBE_MAP:           return GL_TEXTURE_CUBE_MAP;
        case GL_TEXTURE_1D_ARRAY:           return GL_TEXTURE_1D_ARRAY;
        case GL_TEXTURE_2D_ARRAY:           return GL_TEXTURE_2D_ARRAY;
        case GL_TEXTURE_RECTANGLE:          return GL_TEXTURE_2D;
        case GL_TEXTURE_CUBE_MAP_ARRAY:     return GL_TEXTURE_CUBE_MAP_ARRAY;
        case GL_TEXTURE_2D_MULTISAMPLE:     return GL_TEXTURE_2D_MULTISAMPLE;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:return GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
        case GL_TEXTURE_BUFFER:             return GL_TEXTURE_BUFFER;
        default:                            return GL_TEXTURE_2D;
    }
}