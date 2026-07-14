// MobileGlues - gl/state.h
// Unified OpenGL State Manager: encapsulates all CPU-side state tracking
// to avoid GPU round-trips (glGetIntegerv etc.) and provides ID mapping
// for the desktop GL → OpenGL ES 3.2 translation layer.
//
// Architecture principle: "ES 3.2 native → native, ES 3.2 not native → CPU simulation"
//
// Design (inspired by MobileGL-DirectGLES):
//   - GLStateManager is the central context holding modular sub-state components
//   - RenderState uses version-based dirty tracking for efficient backend sync
//   - ErrorState provides structured error recording (GL + non-GL errors)
//   - Strongly-typed enums for all render state parameters
//   - ID mapping (virtual↔real) for buffer/texture/FBO/shader/program objects
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#pragma once

#include "../includes.h"
#include <GL/gl.h>
#include "glcorearb.h"
#include <GLES3/gl32.h>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <sstream>
#include <memory>
#include <optional>
#include <array>
#include <cstdint>
#include <deque>

// ============================================================================
// FastGLIdMap: flat-array ID mapping for O(1) lookup without hashing
//
// Replaces UnorderedMap<GLuint, GLuint> for forward ID mappings where keys
// are small sequential GL object IDs. Gives direct array access instead of
// hash computation + bucket traversal. Falls back to unordered_map for
// large IDs.
// ============================================================================

namespace {

static constexpr size_t kFastIdMapFlatSize = 65536;

template<typename FallbackMap = UnorderedMap<GLuint, GLuint>>
class FastGLIdMap {
public:
    GLuint lookup(GLuint key) const {
        if (__builtin_expect(key < kFastIdMapFlatSize, 1)) {
            return (__builtin_expect(key < m_flat.size(), 1)) ? m_flat[key] : 0;
        }
        auto it = m_overflow.find(key);
        return (it != m_overflow.end()) ? it->second : 0;
    }

    void insert(GLuint key, GLuint value) {
        if (__builtin_expect(key < kFastIdMapFlatSize, 1)) {
            if (__builtin_expect(key >= m_flat.size(), 0)) {
                m_flat.resize(key + 1, 0);
            }
            m_flat[key] = value;
        } else {
            m_overflow[key] = value;
        }
    }

    void erase(GLuint key) {
        if (__builtin_expect(key < kFastIdMapFlatSize, 1)) {
            if (__builtin_expect(key < m_flat.size(), 1)) {
                m_flat[key] = 0;
            }
        } else {
            m_overflow.erase(key);
        }
    }

    GLuint& operator[](GLuint key) {
        if (__builtin_expect(key < kFastIdMapFlatSize, 1)) {
            if (__builtin_expect(key >= m_flat.size(), 0)) {
                m_flat.resize(key + 1, 0);
            }
            return m_flat[key];
        }
        return m_overflow[key];
    }

    void clear() {
        m_flat.clear();
        m_overflow.clear();
    }

    void reserve(size_t n) {
        if (n <= kFastIdMapFlatSize) {
            m_flat.reserve(n);
        }
        // overflow map doesn't need explicit reserve
    }

    bool contains(GLuint key) const {
        return lookup(key) != 0;
    }

    size_t size() const {
        size_t count = 0;
        for (auto v : m_flat) { if (v) ++count; }
        return count + m_overflow.size();
    }

private:
    std::vector<GLuint> m_flat;
    FallbackMap m_overflow;
};

} // anonymous namespace

// ============================================================================
// Constants
// ============================================================================

constexpr int MAX_TEXTURE_UNITS = 96;    // GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS minimum
constexpr int MAX_DRAW_BUFFERS = 16;     // GL_MAX_DRAW_BUFFERS minimum
constexpr int MAX_COLOR_ATTACHMENTS = 8;
constexpr int MAX_VERTEX_ATTRIBS = 16;
constexpr int MAX_UNIFORM_BUFFER_BINDINGS = 84;
constexpr int MAX_SHADER_STORAGE_BUFFER_BINDINGS = 64;
constexpr int MAX_ATOMIC_COUNTER_BUFFER_BINDINGS = 8;
constexpr int MAX_IMAGE_UNITS = 16;
constexpr int MAX_TRANSFORM_FEEDBACK_BUFFERS = 4;

// ============================================================================
// Logging
// ============================================================================
// Debug/info logging is gated on MG_DEBUG (off by default) to avoid the
// per-call printf/__android_log_print overhead on the hot path: every
// CallAndCheckGLES() invocation emits an MG_LOG_DEBUG trace. Warnings and
// errors remain unconditional since they are rare and indicate real issues.
#ifndef MG_DEBUG
#define MG_DEBUG 0
#endif

#ifdef __ANDROID__
#define MG_LOG(level, fmt, ...) __android_log_print(level, "MobileGlues", fmt, ##__VA_ARGS__)
#define MG_LOG_WARN(fmt, ...) MG_LOG(ANDROID_LOG_WARN, fmt, ##__VA_ARGS__)
#define MG_LOG_ERROR(fmt, ...) MG_LOG(ANDROID_LOG_ERROR, fmt, ##__VA_ARGS__)
#if MG_DEBUG
#define MG_LOG_DEBUG(fmt, ...) MG_LOG(ANDROID_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define MG_LOG_INFO(fmt, ...) MG_LOG(ANDROID_LOG_INFO, fmt, ##__VA_ARGS__)
#else
#define MG_LOG_DEBUG(fmt, ...) ((void)0)
#define MG_LOG_INFO(fmt, ...) ((void)0)
#endif
#else
#define MG_LOG_WARN(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define MG_LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#if MG_DEBUG
#define MG_LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define MG_LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#else
#define MG_LOG_DEBUG(fmt, ...) ((void)0)
#define MG_LOG_INFO(fmt, ...) ((void)0)
#endif
#endif

// ============================================================================
// Debug macros
// ============================================================================

#define DEBUG_STATE 0
#if DEBUG_STATE
#define STATE_LOG(fmt, ...) MG_LOG_DEBUG(fmt, ##__VA_ARGS__)
#else
#define STATE_LOG(fmt, ...) ((void)0)
#endif

// ============================================================================
// Forward declarations
// ============================================================================

// ============================================================================
// Strongly-typed Render State Enums (inspired by MobileGL-DirectGLES)
// ============================================================================

enum class BlendFactor : uint32_t {
    Zero = 0,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor,
    ConstantAlpha,
    OneMinusConstantAlpha,
    Count,
    Unknown = 0xFFFFFFFF
};

enum class BlendEquation : uint32_t {
    Add = 0,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
    Count,
    Unknown = 0xFFFFFFFF
};

enum class LogicOperation : uint32_t {
    Clear = 0,
    And,
    AndReverse,
    Copy,
    AndInverted,
    Noop,
    Xor,
    Or,
    Nor,
    Equiv,
    Invert,
    OrReverse,
    CopyInverted,
    OrInverted,
    Nand,
    Set,
    Count,
    Unknown = 0xFFFFFFFF
};

enum class DepthTestFunc : uint32_t {
    Never = 0,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
    Count,
    Unknown = 0xFFFFFFFF
};

enum class StencilOperation : uint32_t {
    Keep = 0,
    Zero,
    Replace,
    IncrementClamp,
    DecrementClamp,
    Invert,
    IncrementWrap,
    DecrementWrap,
    Count,
    Unknown = 0xFFFFFFFF
};

enum class StencilFaceIndex : uint32_t {
    Front = 0,
    Back = 1,
    Count
};

enum class CullFaceMode : uint32_t {
    Front = 0,
    Back,
    FrontAndBack,
    Count,
    Unknown = 0xFFFFFFFF
};

enum class FrontFaceMode : uint32_t {
    CounterClockwise = 0,
    Clockwise,
    Count,
    Unknown = 0xFFFFFFFF
};

enum class ProvokingVertexMode : uint32_t {
    FirstVertex = 0,
    LastVertex,
    Count,
    Unknown = 0xFFFFFFFF
};

enum class Capability : uint32_t {
    Blend = 0,
    ColorLogicOp,
    CullFace,
    DebugOutput,
    DebugOutputSynchronous,
    DepthClamp,
    DepthTest,
    Dither,
    FramebufferSrgb,
    LineSmooth,
    Multisample,
    PolygonOffsetFill,
    PolygonOffsetLine,
    PolygonOffsetPoint,
    PolygonSmooth,
    PrimitiveRestart,
    PrimitiveRestartFixedIndex,
    RasterizerDiscard,
    SampleAlphaToCoverage,
    SampleAlphaToOne,
    SampleCoverage,
    SampleMask,
    ScissorTest,
    StencilTest,
    TextureCubeMapSeamless,
    ProgramPointSize,
    Count,
    Unknown = 0xFFFFFFFF
};

enum class PixelStoreParam : uint32_t {
    // Pack parameters
    PackAlignment = 0,
    PackRowLength,
    PackImageHeight,
    PackSkipRows,
    PackSkipPixels,
    PackSkipImages,
    PackSwapBytes,
    PackLSBFirst,
    // Unpack parameters
    UnpackAlignment,
    UnpackRowLength,
    UnpackImageHeight,
    UnpackSkipRows,
    UnpackSkipPixels,
    UnpackSkipImages,
    UnpackSwapBytes,
    UnpackLSBFirst,
    Count,
    Unknown = 0xFFFFFFFF
};

// ============================================================================
// Render State Data Structures
// ============================================================================

struct PixelStoreParameters {
    bool SwapBytes = false;
    bool LSBFirst = false;
    int32_t RowLength = 0;
    int32_t ImageHeight = 0;
    int32_t SkipPixels = 0;
    int32_t SkipRows = 0;
    int32_t SkipImages = 0;
    int32_t Alignment = 4;
};

struct PerBufferBlendState {
    bool Enabled = false;
    BlendFactor SrcFactorRGB = BlendFactor::One;
    BlendFactor DstFactorRGB = BlendFactor::Zero;
    BlendFactor SrcFactorAlpha = BlendFactor::One;
    BlendFactor DstFactorAlpha = BlendFactor::Zero;
    BlendEquation ColorEquation = BlendEquation::Add;
    BlendEquation AlphaEquation = BlendEquation::Add;
};

struct StencilFaceState {
    DepthTestFunc Func = DepthTestFunc::Always;
    int32_t Ref = 0;
    uint32_t ValueMask = 0xFFFFFFFFu;
    uint32_t WriteMask = 0xFFFFFFFFu;
    StencilOperation FailOp = StencilOperation::Keep;
    StencilOperation PassDepthFailOp = StencilOperation::Keep;
    StencilOperation PassDepthPassOp = StencilOperation::Keep;
};

struct RenderStateParameters {
    // Rasterization
    int32_t Viewport[4] = {0, 0, 0, 0};     // x, y, width, height
    float LineWidth = 1.0f;
    float PointSize = 1.0f;
    float PolygonOffsetFactor = 0.0f;
    float PolygonOffsetUnits = 0.0f;

    // Blending (per-draw-buffer)
    std::array<PerBufferBlendState, MAX_DRAW_BUFFERS> BlendStates;
    LogicOperation LogicOp = LogicOperation::Copy;

    // Depth
    bool DepthTestEnabled = false;
    DepthTestFunc DepthFunc = DepthTestFunc::Less;
    bool DepthMask = true;

    // Color Mask
    bool ColorMaskR = true;
    bool ColorMaskG = true;
    bool ColorMaskB = true;
    bool ColorMaskA = true;

    // Clear State
    float ClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float ClearDepth = 1.0f;
    uint32_t ClearStencil = 0;
    float BlendColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float DepthRangeNear = 0.0f;
    float DepthRangeFar = 1.0f;
    float SampleCoverageValue = 1.0f;
    bool SampleCoverageInvert = false;
    uint32_t SampleMaskValue = 0xFFFFFFFFu;
    std::array<StencilFaceState, 2> StencilStates{};

    // Cull Face
    bool CullFaceEnabled = false;
    CullFaceMode CullFaceModeSetting = CullFaceMode::Back;
    FrontFaceMode FrontFaceModeSetting = FrontFaceMode::CounterClockwise;
    ProvokingVertexMode ProvokingVertexModeSetting = ProvokingVertexMode::LastVertex;

    // Capability flags
    bool ColorLogicOpEnabled = false;
    bool DebugOutputEnabled = false;
    bool DebugOutputSynchronousEnabled = false;
    bool DitherEnabled = true;
    bool LineSmoothEnabled = false;
    bool MultisampleEnabled = true;
    bool PolygonOffsetFillEnabled = false;
    bool PolygonOffsetLineEnabled = false;
    bool PolygonOffsetPointEnabled = false;
    bool PolygonSmoothEnabled = false;
    bool PrimitiveRestartEnabled = false;
    bool PrimitiveRestartFixedIndexEnabled = false;
    bool RasterizerDiscardEnabled = false;
    bool SampleAlphaToCoverageEnabled = false;
    bool SampleAlphaToOneEnabled = false;
    bool SampleCoverageEnabled = false;
    bool SampleMaskEnabled = false;
    bool ScissorTestEnabled = false;
    bool StencilTestEnabled = false;
    bool ProgramPointSizeEnabled = false;

    // Scissor
    int32_t ScissorBox[4] = {0, 0, 0, 0};   // x, y, width, height
};

// ============================================================================
// RenderState: Version-tracked render state with dirty tracking
// (inspired by MobileGL-DirectGLES)
// ============================================================================

class RenderState {
public:
    RenderState() = default;

    uint32_t GetVersion() const { return m_version; }
    const RenderStateParameters& GetAllParameters() const { return m_parameters; }

    // Rasterization
    void SetViewport(int32_t x, int32_t y, int32_t width, int32_t height);
    void GetViewport(int32_t* out) const;
    void SetLineWidth(float width);
    float GetLineWidth() const;
    void SetPointSize(float size);
    float GetPointSize() const;
    void SetPolygonOffset(float factor, float units);
    float GetPolygonOffsetFactor() const;
    float GetPolygonOffsetUnits() const;

    // Capabilities
    void SetCapability(Capability cap, bool enabled);
    bool IsCapabilityEnabled(Capability cap) const;
    void SetCapabilityIndexed(Capability cap, uint32_t index, bool enabled);
    bool IsCapabilityEnabledIndexed(Capability cap, uint32_t index) const;

    // Blending
    void SetBlendFunc(BlendFactor srcRGB, BlendFactor dstRGB, BlendFactor srcAlpha, BlendFactor dstAlpha);
    void GetBlendFunc(BlendFactor& srcRGB, BlendFactor& dstRGB, BlendFactor& srcAlpha, BlendFactor& dstAlpha) const;
    void SetBlendFuncIndexed(uint32_t index, BlendFactor srcRGB, BlendFactor dstRGB, BlendFactor srcAlpha, BlendFactor dstAlpha);
    void GetBlendFuncIndexed(uint32_t index, BlendFactor& srcRGB, BlendFactor& dstRGB, BlendFactor& srcAlpha, BlendFactor& dstAlpha) const;
    void SetBlendEquation(BlendEquation color, BlendEquation alpha);
    void GetBlendEquation(BlendEquation& color, BlendEquation& alpha) const;
    void SetBlendEquationIndexed(uint32_t index, BlendEquation color, BlendEquation alpha);
    void GetBlendEquationIndexed(uint32_t index, BlendEquation& color, BlendEquation& alpha) const;
    void SetLogicOp(LogicOperation logicOp);
    LogicOperation GetLogicOp() const;

    // Depth
    void SetDepthFunc(DepthTestFunc func);
    DepthTestFunc GetDepthFunc() const;
    void SetDepthMask(bool flag);
    bool GetDepthMask() const;
    void SetStencilFunc(uint32_t faceIndex, DepthTestFunc func, int32_t ref, uint32_t mask);
    void SetStencilMask(uint32_t faceIndex, uint32_t mask);
    void SetStencilOp(uint32_t faceIndex, StencilOperation fail, StencilOperation depthFail, StencilOperation depthPass);
    const StencilFaceState& GetStencilState(uint32_t faceIndex) const;

    // Color Mask
    void SetColorMask(bool r, bool g, bool b, bool a);
    void GetColorMask(bool* out) const;

    // Clear State
    void SetClearColor(float r, float g, float b, float a);
    void GetClearColor(float* out) const;
    void SetClearDepth(float depth);
    float GetClearDepth() const;
    void SetClearStencil(int32_t stencil);
    uint32_t GetClearStencil() const;
    void SetBlendColor(float r, float g, float b, float a);
    void GetBlendColor(float* out) const;
    void SetDepthRange(float nearVal, float farVal);
    void GetDepthRange(float* out) const;
    void SetSampleCoverage(float value, bool invert);
    float GetSampleCoverageValue() const;
    bool GetSampleCoverageInvert() const;
    void SetSampleMaskValue(uint32_t mask);
    uint32_t GetSampleMaskValue() const;

    // Pixel Store
    void SetPixelStoreParam(PixelStoreParam param, int32_t value);
    int32_t GetPixelStoreParam(PixelStoreParam param) const;
    PixelStoreParameters GetPixelStoreParameters(bool isUnpack) const;

    // Cull Face
    void SetCullFaceMode(CullFaceMode mode);
    CullFaceMode GetCullFaceMode() const;
    void SetFrontFaceMode(FrontFaceMode mode);
    FrontFaceMode GetFrontFaceMode() const;
    void SetProvokingVertexMode(ProvokingVertexMode mode);
    ProvokingVertexMode GetProvokingVertexMode() const;

    // Scissor
    void SetScissorBox(int32_t x, int32_t y, int32_t width, int32_t height);
    void GetScissorBox(int32_t* out) const;

private:
    uint16_t m_version = 0;
    RenderStateParameters m_parameters;
    PixelStoreParameters m_pixelStorePackParameters;
    PixelStoreParameters m_pixelStoreUnpackParameters;
};

// ============================================================================
// ErrorState: Structured error recording (inspired by MobileGL-DirectGLES)
// ============================================================================

enum class ErrorCode : uint32_t {
    NoError = 0,
    InvalidEnum,
    InvalidValue,
    InvalidOperation,
    InvalidFramebufferOperation,
    OutOfMemory,
    StackUnderflow,
    StackOverflow
};

class ErrorInfo {
public:
    virtual ~ErrorInfo() = default;
    virtual std::string ToString() const = 0;
};

class GenericErrorInfo : public ErrorInfo {
public:
    explicit GenericErrorInfo(std::string message) : m_message(std::move(message)) {}
    std::string ToString() const override { return m_message; }
private:
    std::string m_message;
};

struct Error {
    ErrorCode code;
    std::unique_ptr<ErrorInfo> info;
};

class ErrorState {
public:
    void RecordError(ErrorCode code, std::unique_ptr<ErrorInfo> info);
    bool HasGLError() const;
    const Error* PeekGLError() const;
    std::unique_ptr<Error> PopGLError();
    bool HasNonGLError() const;
    const Error* PeekNonGLError() const;
    std::unique_ptr<Error> PopNonGLError();
    void Clear();

private:
    // std::deque gives O(1) front pop; std::vector::erase(begin()) was O(n)
    // and was called per glGetError() on the hot path.
    std::deque<std::unique_ptr<Error>> m_errors;
    std::deque<std::unique_ptr<Error>> m_nonGLErrors;
};

// ============================================================================
// BufferObject: per-buffer CPU-side metadata
// ============================================================================

struct MGBufferInfo {
    GLuint id = 0;           // real GLES buffer ID
    GLenum target = 0;
    GLsizeiptr size = 0;
    GLbitfield access = 0;
    void* mappedPtr = nullptr;
    bool isMapped = false;
};

// ============================================================================
// GLStateManager: unified state machine
// (Refactored with modular sub-state components, inspired by MobileGL-DirectGLES)
// ============================================================================

class GLStateManager {
public:
    static GLStateManager& Instance() {
        static GLStateManager instance;
        return instance;
    }

    // ------------------------------------------------------------------------
    // Initialization
    // ------------------------------------------------------------------------

    void Initialize();
    void Shutdown();

    // ------------------------------------------------------------------------
    // Hardware state
    // ------------------------------------------------------------------------

    int esVersion = 32;                    // OpenGL ES version (30 or 32)
    int &es_version = esVersion;           // backward-compatible alias

    // ------------------------------------------------------------------------
    // Proxy framebuffer state (for FSR1-style rendering)
    // ------------------------------------------------------------------------

    GLsizei proxyWidth = 0;
    GLsizei proxyHeight = 0;
    GLenum proxyInternalFormat = GL_RGBA8;
    GLuint currentProgram = 0;
    GLint currentTexUnit = 0;
    GLuint currentDrawFBO = 0;
    bool isDrawCall = false;               // guard flag during draw calls

    // ========================================================================
    // Error State (new: inspired by MobileGL-DirectGLES)
    // ========================================================================

    ErrorState errorState;

    // ========================================================================
    // Render State (new: version-tracked, inspired by MobileGL-DirectGLES)
    // ========================================================================

    RenderState renderState;

    // ========================================================================
    // Buffer State Management
    // ========================================================================

    struct BufferState {
        // ID mapping: virtual (desktop GL) → real (GLES)
        FastGLIdMap<> bufferMap;          // virtual → real (flat array for fast lookup)
        UnorderedMap<GLuint, GLuint> bufferMapReverse; // real → virtual
        UnorderedMap<GLuint, MGBufferInfo> bufferInfo; // virtual → metadata

        // VAO mapping
        FastGLIdMap<> vaoMap;             // virtual → real (flat array)
        UnorderedMap<GLuint, GLuint> vaoMapReverse;   // real → virtual

        // Current bindings per target
        UnorderedMap<GLenum, GLuint> boundBuffer;     // target → virtual ID

        // Uniform buffer bases
        GLuint uniformBufferBases[MAX_UNIFORM_BUFFER_BINDINGS] = {};
        GLintptr uniformBufferOffsets[MAX_UNIFORM_BUFFER_BINDINGS] = {};
        GLsizeiptr uniformBufferSizes[MAX_UNIFORM_BUFFER_BINDINGS] = {};

        // Shader storage buffer bases
        GLuint shaderStorageBases[MAX_SHADER_STORAGE_BUFFER_BINDINGS] = {};

        // Atomic counter buffer bases
        GLuint atomicCounterBases[MAX_ATOMIC_COUNTER_BUFFER_BINDINGS] = {};

        // Transform feedback buffer bindings
        GLuint transformFeedbackBuffers[MAX_TRANSFORM_FEEDBACK_BUFFERS] = {};

        // Buffer map for glMapBufferRange
        UnorderedMap<GLuint, void*> bufferMaps;         // virtual → mapped pointer

        // Atomic counter buffer data (CPU-side copy for emulation)
        std::vector<GLuint> atomicCounterData;
        GLuint atomicCounterBufferBinding = 0;
        GLsizeiptr atomicCounterBufferSize = 0;
        GLsizeiptr atomicCounterBufferOffset = 0;

        void Reset();
    } buffer;

    // ========================================================================
    // Texture State Management
    // ========================================================================

    struct TextureState {
        // ID mapping: virtual → real GLES texture ID
        FastGLIdMap<> textureMap;          // virtual → real (flat array)
        UnorderedMap<GLuint, GLuint> textureMapReverse; // real → virtual

        // Per-unit binding state: unit → target → virtual texture ID
        struct TexUnitState {
            UnorderedMap<GLenum, GLuint> binding;              // target → virtual ID
        };
        TexUnitState texUnits[MAX_TEXTURE_UNITS];

        // Current active texture unit
        GLint activeUnit = 0;

        // Pixel store state (kept for backward compatibility;
        // primary state is now in RenderState)
        GLint unpackAlignment = 4;
        GLint unpackRowLength = 0;
        GLint unpackImageHeight = 0;
        GLint unpackSkipPixels = 0;
        GLint unpackSkipRows = 0;
        GLint unpackSkipImages = 0;
        GLint packAlignment = 4;
        GLint packRowLength = 0;
        GLint packImageHeight = 0;
        GLint packSkipPixels = 0;
        GLint packSkipRows = 0;
        GLint packSkipImages = 0;

        void Reset();
    } texture;

    // ========================================================================
    // Framebuffer State Management
    // ========================================================================

    struct FramebufferState {
        // ID mapping: virtual → real
        FastGLIdMap<> fboMap;             // virtual → real (flat array)
        UnorderedMap<GLuint, GLuint> fboMapReverse;   // real → virtual

        // Current bindings
        GLuint drawFBO = 0;    // virtual
        GLuint readFBO = 0;    // virtual

        // Attachment tracking: virtual FBO → attachment → {real FBO, attachment, texture}
        // The `texture` field stores the virtual texture ID attached at this
        // attachment point (0 = no texture attached). It is consulted by
        // glCheckFramebufferStatus() to detect when
        // GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT is caused by a float
        // internalformat that GLES rejects without GL_EXT_color_buffer_float /
        // GL_EXT_color_buffer_half_float, so that status can be promoted to
        // COMPLETE to match desktop GL behaviour.
        struct AttachmentInfo {
            GLuint fbo = 0;          // real FBO
            GLenum attachment = 0;   // real attachment point
            GLuint texture = 0;      // virtual texture ID (0 = no texture attached)
        };
        UnorderedMap<GLuint, UnorderedMap<GLenum, AttachmentInfo>> attachments;

        // Draw buffer state
        GLenum drawBuffers[MAX_DRAW_BUFFERS] = {GL_BACK};
        GLenum readBuffer = GL_BACK;
        int drawBufferCount = 1;

        // Temp FBO pool for format conversion
        struct TempFBO {
            GLuint fbo = 0;
            GLuint texture = 0;
            GLsizei width = 0;
            GLsizei height = 0;
        };
        std::vector<TempFBO> tempFBOs;

        // FBO invalidation state
        bool defaultFboInvalidated = false;

        void Reset();
    } framebuffer;

    // ========================================================================
    // Shader/Program State Management
    // ========================================================================

    struct ShaderState {
        // ID mapping: virtual → real
        FastGLIdMap<> shaderMap;          // virtual → real (flat array)
        FastGLIdMap<> programMap;         // virtual → real (flat array)
        UnorderedMap<GLuint, GLuint> shaderMapReverse;  // real → virtual
        UnorderedMap<GLuint, GLuint> programMapReverse; // real → virtual

        // Shader metadata
        struct ShaderInfo {
            GLenum type = 0;
            std::string source;
            bool compiled = false;
        };
        UnorderedMap<GLuint, ShaderInfo> shaderInfo;   // virtual → info

        // Program metadata
        struct ProgramInfo {
            std::vector<GLuint> attachedShaders;  // virtual IDs
            bool linked = false;
            bool validated = false;
        };
        UnorderedMap<GLuint, ProgramInfo> programInfo;  // virtual → info

        // Current program
        GLuint currentProgram = 0;

        void Reset();
    } shader;

    // ========================================================================
    // Sampler State
    // ========================================================================

    struct SamplerState {
        GLuint boundSamplers[MAX_TEXTURE_UNITS] = {};
        void Reset() { memset(boundSamplers, 0, sizeof(boundSamplers)); }
    } sampler;

    // ========================================================================
    // Image Unit State
    // ========================================================================

    struct ImageState {
        struct ImageBinding {
            GLuint texture = 0;
            GLint level = 0;
            GLboolean layered = GL_FALSE;
            GLint layer = 0;
            GLenum access = GL_READ_ONLY;
            GLenum format = GL_RGBA8;
        };
        ImageBinding imageUnits[MAX_IMAGE_UNITS];
        void Reset() { memset(imageUnits, 0, sizeof(imageUnits)); }
    } image;

    // ========================================================================
    // Legacy GL State (for backward-compatible queries)
    // ========================================================================

    struct LegacyState {
        GLint viewport[4] = {0, 0, 0, 0};
        GLint scissor[4] = {0, 0, 0, 0};
        GLfloat clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        GLfloat clearDepth = 1.0f;
        GLint clearStencil = 0;
        GLboolean colorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
        GLboolean depthMask = GL_TRUE;
        GLenum depthFunc = GL_LESS;
        GLenum blendSrcRGB = GL_ONE;
        GLenum blendDstRGB = GL_ZERO;
        GLenum blendSrcAlpha = GL_ONE;
        GLenum blendDstAlpha = GL_ZERO;
        GLenum cullFace = GL_BACK;
        GLenum frontFace = GL_CCW;
        GLfloat lineWidth = 1.0f;
        GLenum polygonMode[2] = {GL_FILL, GL_FILL};
        GLfloat polygonOffsetFactor = 0.0f;
        GLfloat polygonOffsetUnits = 0.0f;
        GLint stencilRef = 0;
        GLuint stencilMask = 0xFFFFFFFF;

        void Reset() {
            memset(this, 0, sizeof(*this));
            viewport[2] = 0; viewport[3] = 0;
            scissor[2] = 0; scissor[3] = 0;
            clearColor[3] = 0.0f;
            clearDepth = 1.0f;
            colorMask[0] = colorMask[1] = colorMask[2] = colorMask[3] = GL_TRUE;
            depthMask = GL_TRUE;
            depthFunc = GL_LESS;
            blendSrcRGB = GL_ONE; blendDstRGB = GL_ZERO;
            blendSrcAlpha = GL_ONE; blendDstAlpha = GL_ZERO;
            cullFace = GL_BACK;
            frontFace = GL_CCW;
            lineWidth = 1.0f;
            polygonMode[0] = GL_FILL; polygonMode[1] = GL_FILL;
            stencilMask = 0xFFFFFFFF;
        }
    } legacy;

    // ========================================================================
    // Vertex Array State
    // ========================================================================

    struct VertexArrayState {
        GLuint currentVAO = 0;  // virtual
        void Reset() { currentVAO = 0; }
    } vertexArray;

    // ========================================================================
    // Query and Sync State
    // ========================================================================

    struct QueryState {
        UnorderedMap<GLuint, GLuint> queryMap;
        UnorderedMap<GLuint, GLuint> queryMapReverse;
        void Reset() { queryMap.clear(); queryMapReverse.clear(); }
    } query;

    // ========================================================================
    // Transform Feedback State
    // ========================================================================

    struct TransformFeedbackState {
        UnorderedMap<GLuint, GLuint> tfMap;
        UnorderedMap<GLuint, GLuint> tfMapReverse;
        GLuint currentTF = 0;
        void Reset() { tfMap.clear(); tfMapReverse.clear(); currentTF = 0; }
    } transformFeedback;

    // ========================================================================
    // Renderbuffer State
    // ========================================================================

    struct RenderbufferState {
        UnorderedMap<GLuint, GLuint> rbMap;
        UnorderedMap<GLuint, GLuint> rbMapReverse;
        void Reset() { rbMap.clear(); rbMapReverse.clear(); }
    } renderbuffer;

    // ========================================================================
    // Utility: ID mapping helpers
    // ========================================================================

    GLuint GetRealBuffer(GLuint virtualId) {
        return buffer.bufferMap.lookup(virtualId);
    }

    GLuint GetVirtualBuffer(GLuint realId) {
        auto it = buffer.bufferMapReverse.find(realId);
        return (it != buffer.bufferMapReverse.end()) ? it->second : 0;
    }

    GLuint GetRealTexture(GLuint virtualId) {
        return texture.textureMap.lookup(virtualId);
    }

    GLuint GetVirtualTexture(GLuint realId) {
        auto it = texture.textureMapReverse.find(realId);
        return (it != texture.textureMapReverse.end()) ? it->second : 0;
    }

    GLuint GetRealFBO(GLuint virtualId) {
        return framebuffer.fboMap.lookup(virtualId);
    }

    GLuint GetVirtualFBO(GLuint realId) {
        auto it = framebuffer.fboMapReverse.find(realId);
        return (it != framebuffer.fboMapReverse.end()) ? it->second : 0;
    }

    GLuint GetRealVAO(GLuint virtualId) {
        return buffer.vaoMap.lookup(virtualId);
    }

    GLuint GetVirtualVAO(GLuint realId) {
        auto it = buffer.vaoMapReverse.find(realId);
        return (it != buffer.vaoMapReverse.end()) ? it->second : 0;
    }

    GLuint GetRealShader(GLuint virtualId) {
        return shader.shaderMap.lookup(virtualId);
    }

    GLuint GetRealProgram(GLuint virtualId) {
        return shader.programMap.lookup(virtualId);
    }

    // ========================================================================
    // Utility: Format conversion helpers
    // ========================================================================

    static GLenum ConvertTextureTarget(GLenum target);
    static GLenum ConvertInternalFormat(GLenum format);
    static GLenum ConvertFormat(GLenum format);
    static GLenum ConvertType(GLenum type);
    static bool IsDepthStencilFormat(GLenum format);
    static bool IsCompressedFormat(GLenum format);
    static GLenum GetTextureBindingTarget(GLenum target);
    static GLenum TargetToBindingTarget(GLenum target);

    // ========================================================================
    // Thread safety
    // ========================================================================

    std::recursive_mutex mutex;

    void Lock() { mutex.lock(); }
    void Unlock() { mutex.unlock(); }

private:
    GLStateManager() = default;
    ~GLStateManager() = default;
    GLStateManager(const GLStateManager&) = delete;
    GLStateManager& operator=(const GLStateManager&) = delete;
};

// ============================================================================
// Convenience alias
// ============================================================================

#define GLState GLStateManager::Instance()

// ============================================================================
// Draw preparation macro
// ============================================================================

#define PREPARE_FOR_DRAW() ((void)0)