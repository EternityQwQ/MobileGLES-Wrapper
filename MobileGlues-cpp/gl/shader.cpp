// MobileGlues - gl/shader.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

// ============================================================================
// Shader Compilation Architecture
//
// Principle: "ES 3.2 native → native, ES 3.2 not native → CPU simulation"
//
// ES 3.2 natively supports:
//   - GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_COMPUTE_SHADER
//   - glCreateShader, glCompileShader, glShaderSource
//
// ES 3.2 does NOT natively support:
//   - Desktop GLSL (non-ESSL) shader sources
//     → CPU simulation: GLSL → GLSL ES conversion via GLSLtoGLSLES()
//   - GL_GEOMETRY_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER
//     → Detected and rejected (no ES 3.2 equivalent)
//
// Conversion Pipeline:
//   1. glCreateShader(type)           → native GLES 3.2 pass-through
//   2. glShaderSource(shader, source) → if ESSL 100/300/310/320: native pass-through
//                                       otherwise: GLSL→ESSL (version 320) conversion
//   3. glCompileShader(shader)        → native GLES 3.2 pass-through
// ============================================================================

#include "shader.h"
#include "../includes.h"
#include "glsl/glsl_for_es.h"
#include "log.h"
#include "../egl/loader.h"
#include "../gles/loader.h"
#include "mg.h"
#include "GL/gl.h"
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

#define DEBUG 0

// ============================================================================
// Shader type cache: avoids glGetShaderiv GPU round-trip on every glShaderSource
// Indexed by shader object ID. Stores the shader type and whether the shader
// source was previously verified as ESSL (to skip scanShaderSource on re-upload).
//
// This is indexed by the application's shader name, so it is shared state
// between every thread the application compiles on — and it compiles on more
// than one. Minecraft and every shader pack around it run shader compilation on
// worker threads, which is precisely why the host-context guard exists in the
// first place (see egl/loader.h). A bare std::vector here is therefore a race
// in two separate ways:
//
//   1. Two threads that both find `shader >= size` call reserve/resize on the
//      same vector. That is undefined behaviour, not a stale read — the
//      reallocation frees the old block while the other thread is indexing into
//      it. A crash here would present as a random one-off during shader-pack
//      loading, which is close to unreportable.
//   2. Even without a realloc, the entries themselves are written without
//      synchronisation, so a worker can read a half-updated ShaderCacheEntry.
//
// A mutex over the whole structure is the right size of fix: these are touched
// on glCreateShader, glShaderSource and glDeleteShader, i.e. on shader
// (re)loads, not per draw or per frame. The alternative — thread_local — would
// be wrong here, because the point of the cache is to share one answer about a
// shader between the thread that created it and the thread that compiles it;
// per-thread copies would each pay the glGetShaderiv round trip the cache
// exists to avoid, and would disagree after glShaderSource updates the type.
//
// Growth is by doubling rather than to `shader + 1`: names come from the
// application and are not dense, so exact-size growth reallocates once per new
// name and wastes a slot for every skipped id.
// ============================================================================
struct ShaderCacheEntry {
    GLenum type = 0;         // cached shader type (GL_VERTEX_SHADER, etc.)
    bool is_essl_verified = false; // true if source was confirmed ESSL
};
static std::vector<ShaderCacheEntry> g_shader_cache;
static std::mutex g_shader_cache_mutex;

// Returns a copy, not a reference: call sites hold it across a GL call, and a
// reference would be invalidated by a concurrent growth in between.
static ShaderCacheEntry get_shader_cache(GLuint shader) {
    std::lock_guard<std::mutex> lock(g_shader_cache_mutex);
    if (shader >= g_shader_cache.size()) {
        g_shader_cache.resize(std::max<size_t>(shader + 1, g_shader_cache.size() * 2));
    }
    return g_shader_cache[shader];
}

// Read-modify-write under one lock. Doing get/modify/put from the call site
// would let two threads interleave and lose an update, which is the same class
// of bug this lock was added to stop.
static void update_shader_cache(GLuint shader, GLenum type, int set_type, int set_essl_verified) {
    std::lock_guard<std::mutex> lock(g_shader_cache_mutex);
    if (shader >= g_shader_cache.size()) {
        g_shader_cache.resize(std::max<size_t>(shader + 1, g_shader_cache.size() * 2));
    }
    ShaderCacheEntry& e = g_shader_cache[shader];
    if (set_type) e.type = type;
    if (set_essl_verified) e.is_essl_verified = true;
}

void invalidate_shader_cache(GLuint shader) {
    std::lock_guard<std::mutex> lock(g_shader_cache_mutex);
    if (shader < g_shader_cache.size()) {
        g_shader_cache[shader] = ShaderCacheEntry{};
    }
}

// ============================================================================
// Section: Shader Type Detection Helpers
//
// Single-pass GLSL source analysis. Instead of multiple strstr() calls
// (up to 20+ per shader source), we scan once and check all patterns
// simultaneously. This avoids O(n * m) overhead where n = source length
// and m = number of patterns.
// ============================================================================

// ---------------------------------------------------------------------------
// Result of single-pass shader source analysis
// ---------------------------------------------------------------------------
struct ShaderSourceInfo {
    bool is_essl;          // true if #version 100/300/310/320
    bool is_geometry;      // geometry shader keywords found
    bool is_tessellation;  // tessellation shader keywords found
    bool is_compute;       // compute shader keywords found
};

// ---------------------------------------------------------------------------
// Single-pass scan of GLSL source for version + shader type detection.
// Scans the entire source string once, checking for all patterns.
// ---------------------------------------------------------------------------
static ShaderSourceInfo scanShaderSource(const char* code) {
    ShaderSourceInfo info = {};
    const char* p = code;

    while (*p) {
        // Skip whitespace and comments efficiently
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;

        // Check for #version directive (only at/near the beginning matters)
        if (!info.is_essl && *p == '#') {
            const char* v = p + 1;
            while (*v == ' ' || *v == '\t') v++;
            if (v[0] == 'v' && v[1] == 'e' && v[2] == 'r' && v[3] == 's' && v[4] == 'i' && v[5] == 'o' && v[6] == 'n') {
                v += 7;
                while (*v == ' ' || *v == '\t') v++;
                int ver = 0;
                while (*v >= '0' && *v <= '9') { ver = ver * 10 + (*v - '0'); v++; }
                info.is_essl = (ver == 100 || ver == 300 || ver == 310 || ver == 320);
            }
        }

        // Check for shader-type-defining keywords
        // We use multi-character prefix matching to reduce false positives
        switch (*p) {
        case 'E':
            if (!info.is_geometry) {
                if (p[1] == 'm' && p[2] == 'i' && p[3] == 't' && p[4] == 'V' && p[5] == 'e' && p[6] == 'r' && p[7] == 't' && p[8] == 'e' && p[9] == 'x')
                    info.is_geometry = true;
                else if (p[1] == 'n' && p[2] == 'd' && p[3] == 'P' && p[4] == 'r' && p[5] == 'i' && p[6] == 'm' && p[7] == 'i' && p[8] == 't' && p[9] == 'i' && p[10] == 'v' && p[11] == 'e')
                    info.is_geometry = true;
            }
            break;
        case 'g':
            if (!info.is_tessellation && p[1] == 'l' && p[2] == '_') {
                if (p[3] == 'T' && p[4] == 'e' && p[5] == 's' && p[6] == 's' && (p[7] == 'L' || p[7] == 'C'))
                    info.is_tessellation = true;
            }
            if (!info.is_compute && p[1] == 'l' && p[2] == '_') {
                if ((p[3] == 'W' && p[4] == 'o' && p[5] == 'r' && p[6] == 'k' && p[7] == 'G') ||
                    (p[3] == 'L' && p[4] == 'o' && p[5] == 'c' && p[6] == 'a' && p[7] == 'l') ||
                    (p[3] == 'G' && p[4] == 'l' && p[5] == 'o' && p[6] == 'b' && p[7] == 'a' && p[8] == 'l') ||
                    (p[3] == 'N' && p[4] == 'u' && p[5] == 'm' && p[6] == 'W' && p[7] == 'o' && p[8] == 'r' && p[9] == 'k'))
                    info.is_compute = true;
            }
            break;
        case 'l':
            if (!info.is_geometry && p[1] == 'a' && p[2] == 'y' && p[3] == 'o' && p[4] == 'u' && p[5] == 't' && p[6] == '(') {
                p += 7;
                while (*p == ' ' || *p == '\t') p++;
                if (p[0] == 't' && p[1] == 'r' && p[2] == 'i') info.is_geometry = true;
                else if (p[0] == 'p' && p[1] == 'o' && p[2] == 'i' && p[3] == 'n' && p[4] == 't' && p[5] == 's') info.is_geometry = true;
                else if (p[0] == 'l' && p[1] == 'i' && p[2] == 'n' && p[3] == 'e') info.is_geometry = true;
                else if (p[0] == 'q' && p[1] == 'u' && p[2] == 'a' && p[3] == 'd' && p[4] == 's') info.is_geometry = true;
                else if (p[0] == 'i' && p[1] == 's' && p[2] == 'o' && p[3] == 'l' && p[4] == 'i' && p[5] == 'n' && p[6] == 'e' && p[7] == 's') info.is_geometry = true;
                continue;
            }
            if (!info.is_compute && p[1] == 'a' && p[2] == 'y' && p[3] == 'o' && p[4] == 'u' && p[5] == 't' && p[6] == '(') {
                p += 7;
                while (*p == ' ' || *p == '\t') p++;
                if (p[0] == 'l' && p[1] == 'o' && p[2] == 'c' && p[3] == 'a' && p[4] == 'l' && p[5] == '_' && p[6] == 's' && p[7] == 'i' && p[8] == 'z' && p[9] == 'e' && p[10] == '_')
                    info.is_compute = true;
                continue;
            }
            if (!info.is_tessellation && p[1] == 'a' && p[2] == 'y' && p[3] == 'o' && p[4] == 'u' && p[5] == 't' && p[6] == '(') {
                p += 7;
                while (*p == ' ' || *p == '\t') p++;
                if (p[0] == 'v' && p[1] == 'e' && p[2] == 'r' && p[3] == 't' && p[4] == 'i' && p[5] == 'c' && p[6] == 'e' && p[7] == 's')
                    info.is_tessellation = true;
                continue;
            }
            break;
        case 'm':
            if (!info.is_geometry && p[1] == 'a' && p[2] == 'x' && p[3] == '_' && p[4] == 'v' && p[5] == 'e' && p[6] == 'r' && p[7] == 't' && p[8] == 'i' && p[9] == 'c' && p[10] == 'e' && p[11] == 's')
                info.is_geometry = true;
            break;
        case 'p':
            if (!info.is_tessellation && p[1] == 'a' && p[2] == 't' && p[3] == 'c' && p[4] == 'h') {
                // "patch" is 5 chars, check it's not part of a longer word
                char next = p[5];
                if (next == ' ' || next == '\t' || next == '\n' || next == '\r' || next == ';' || next == '(' || next == '\0')
                    info.is_tessellation = true;
            }
            break;
        }
        p++;
    }
    return info;
}

// ---------------------------------------------------------------------------
// Determine if shader is already ES-compatible (direct passthrough)
// ESSL versions recognized: 100 (ES 1.0), 300 (ES 3.0), 310 (ES 3.1), 320 (ES 3.2)
// ---------------------------------------------------------------------------
static inline bool is_direct_shader(const ShaderSourceInfo& info) {
    return info.is_essl;
}

// ---------------------------------------------------------------------------
// Detect the actual shader type from GLSL source content
// Used when the shader type was not explicitly set (e.g. Iris sets it later)
// ---------------------------------------------------------------------------
static GLenum detect_shader_type_from_source(const ShaderSourceInfo& info) {
    if (info.is_geometry) return GL_GEOMETRY_SHADER;
    if (info.is_tessellation) return GL_TESS_EVALUATION_SHADER;
    if (info.is_compute) return GL_COMPUTE_SHADER;
    return GL_FRAGMENT_SHADER; // default
}

// ============================================================================
// Section: Native ES 3.2 Pass-Through Functions
//
// These functions are natively supported by OpenGL ES 3.2 and are passed
// through directly to the underlying GLES implementation without any
// CPU-side conversion or emulation.
// ============================================================================

// ---------------------------------------------------------------------------
// glCreateShader - Creates a shader object of the given type (native ES 3.2)
// ES 3.2 natively supports: VERTEX_SHADER, FRAGMENT_SHADER, COMPUTE_SHADER
// Also caches the shader type to avoid glGetShaderiv GPU round-trip later.
// ---------------------------------------------------------------------------
GLuint glCreateShader(GLenum type) {
    ScopedHostContext __hostCtx;
    GLuint shader = GLES.glCreateShader(type);

    // The 26.3-pre-3 crash, fixed at the point of failure rather than avoided
    // globally.
    //
    // A host driver handed a call from a thread with no current EGL context
    // does nothing and reports nothing, so this returns 0 — and 0 is not a
    // value any caller can survive. That release's startup queries the device
    // before the application has bound a context, so it got 0 here, compiled
    // "shader 0", received no info log, and died with
    //   Failed to find or load pipeline minecraft:pipeline/gui
    //
    // The eager guard above is supposed to prevent this, but it only helps when
    // it is enabled, and enabling it means one eglGetCurrentContext() in front
    // of ~127 entry points — which is the CPU cost this branch set out to stop
    // paying. Repairing here instead costs nothing until the failure actually
    // happens: a shader creation that succeeds (the overwhelming majority, on
    // any thread that has a context) never reaches the repair at all.
    //
    // 0 is an unambiguous signal — there is no valid shader name 0 — so unlike
    // the limit queries in getter.cpp this needs no whitelist to tell a real 0
    // from a missing context.
    if (shader == 0 && RepairHostContextOnce()) {
        shader = GLES.glCreateShader(type);
        if (shader != 0) {
            LOG_W_FORCE("glCreateShader returned 0 until MobileGLES bound a fallback EGL context, then returned %u "
                        "(type=0x%x). The calling thread had no current EGL context.",
                        shader, type);
        }
    }

    if (shader != 0) {
        update_shader_cache(shader, type, /*set_type=*/1, /*set_essl_verified=*/0);

        // Track in unified state manager
        auto &ss = GLState.shader;
        ss.shaderMap[shader] = shader;
        ss.shaderMapReverse[shader] = shader;
        auto &info = ss.shaderInfo[shader];
        info.type = type;
    }
    return shader;
}

// ---------------------------------------------------------------------------
// glCompileShader - Compiles a shader object (native ES 3.2)
// The shader source must already be valid ESSL — conversion is handled by
// glShaderSource before this function is called.
// Logs compilation errors to help diagnose shader pack issues.
// ---------------------------------------------------------------------------
void glCompileShader(GLuint shader) {
    ScopedHostContext __hostCtx;
    GLES.glCompileShader(shader);
    GLint compiled = 0;
    GLES.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    // Track in unified state manager
    auto &ss = GLState.shader;
    auto it = ss.shaderInfo.find(shader);
    if (it != ss.shaderInfo.end()) {
        it->second.compiled = (compiled == GL_TRUE);
    }

    if (!compiled) {
        GLint infoLen = 0;
        GLES.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            // Include shader type from cache for better diagnostics
            GLenum shaderType = get_shader_cache(shader).type;
            std::vector<char> log(infoLen);
            GLES.glGetShaderInfoLog(shader, infoLen, nullptr, log.data());
            LOG_I("[MobileGLES] Shader %d (type=%d) compilation failed: %s", shader, shaderType, log.data())
        } else {
            LOG_I("[MobileGLES] Shader %d compilation failed (no info log)", shader)
        }
    }
}

// ============================================================================
// Section: GLSL-to-GLSL-ES Conversion Pipeline
//
// glShaderSource is the entry point for shader source code. It implements
// the CPU simulation layer for non-ES GLSL code by converting desktop GLSL
// to GLSL ES 3.2 (version 320) using the GLSLtoGLSLES() conversion engine.
//
// Pipeline:
//   1. If shader is already ESSL (100/300/310/320) → native pass-through
//   2. If shader type is unknown → detect from source content
//   3. Convert GLSL → GLSL ES 3.2 via GLSLtoGLSLES()
//   4. On conversion failure → fall back to original source
// ============================================================================

void glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {
    ScopedHostContext __hostCtx;
    LOG()
    LOG_D("glShaderSource hook, shader=%d, count=%d", shader, count)

    if (count <= 0 || !string || !string[0]) {
        // Invalidate cache when clearing source. A full entry reset (type 0,
        // verified false), not just the flag: the type recorded for this name
        // is about to stop matching whatever source is uploaded next, and
        // Step 1 below re-queries glGetShaderiv when it reads 0. Clearing the
        // flag alone left the stale type in place, which is what let a reused
        // name keep the previous shader's type.
        invalidate_shader_cache(shader);
        GLES.glShaderSource(shader, count, string, length);
        return;
    }

    const char* raw_code = string[0];
    ShaderCacheEntry cacheEntry = get_shader_cache(shader);

    // Step 1: Get shader type — use cache to avoid glGetShaderiv GPU round-trip
    GLenum shaderType = cacheEntry.type;
    if (shaderType == 0) [[unlikely]] {
        // First call: query type from GLES and cache it
        GLES.glGetShaderiv(shader, GL_SHADER_TYPE, (GLint*)&shaderType);
        update_shader_cache(shader, shaderType, /*set_type=*/1, /*set_essl_verified=*/0);
    }

    // Step 2: Fast path — if previously verified as ESSL, skip scan and pass through
    if (cacheEntry.is_essl_verified) [[likely]] {
        LOG_D("Cached ESSL shader %d, passing through directly", shader)
        GLES.glShaderSource(shader, count, string, length);
        return;
    }

    // Step 3: Single-pass scan of source for version + shader type detection
    ShaderSourceInfo sourceInfo = scanShaderSource(raw_code);

    // Step 4: If shader type is GL_FRAGMENT_SHADER, check if source actually
    // contains geometry/tessellation/compute shader keywords and correct the type.
    // (shaderType is always cached by glCreateShader, so shaderType==0 is unreachable)
    if (shaderType == GL_FRAGMENT_SHADER) {
        GLenum detected = detect_shader_type_from_source(sourceInfo);
        if (detected != GL_FRAGMENT_SHADER) {
            shaderType = detected;
            update_shader_cache(shader, shaderType, /*set_type=*/1, /*set_essl_verified=*/0);
            LOG_I("[MobileGLES] Detected non-fragment shader type from source: %d", shaderType)
        }
    }

    LOG_D("glShaderSource: shaderType=%d", shaderType)

    // Step 5: Check if already an ES-compatible shader → native pass-through
    if (is_direct_shader(sourceInfo)) {
        LOG_D("Direct ES shader (ESSL 100/300/310/320), passing through")
        // mark as verified ESSL for future calls
        update_shader_cache(shader, shaderType, /*set_type=*/0, /*set_essl_verified=*/1);
        GLES.glShaderSource(shader, count, string, length);
        return;
    }

    // Step 6: Convert desktop GLSL → GLSL ES 3.2 (version 320)
    uint essl_version = 320;
    int return_code = -1;
    std::string converted = GLSLtoGLSLES(raw_code, shaderType, essl_version, 0, return_code);

    if (return_code < 0) {
        // Conversion failed — fall back to original source
        LOG_I("[MobileGLES] GLSL conversion failed for shader %d (type=%d), passing through original desktop GLSL (will likely fail to compile on GLES)", shader, shaderType)
        GLES.glShaderSource(shader, count, string, length);
        return;
    }

    LOG_D("Converted GLSL ES:\n%s", converted.c_str())

    // Step 7: Set the converted GLSL ES source
    const char* converted_code = converted.c_str();
    GLES.glShaderSource(shader, 1, &converted_code, nullptr);

    // Track in unified state manager
    auto &ss = GLState.shader;
    auto infoIt = ss.shaderInfo.find(shader);
    if (infoIt != ss.shaderInfo.end()) {
        infoIt->second.source = std::move(converted);
    }
}