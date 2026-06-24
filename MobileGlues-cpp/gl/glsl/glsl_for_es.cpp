// MobileGlues - gl/glsl/glsl_for_es.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

// ============================================================================
// GLSL → SPIR-V → GLSL ES Converter (EShClientOpenGL + spirv-cross)
// ============================================================================
//
// Architecture:
//   Stage 1: Preprocess   – strip #line, upgrade legacy syntax, inject Iris macros
//   Stage 2: glslang      – parse desktop GLSL as EShClientOpenGL, emit SPIR-V 1.5
//   Stage 3: spirv-cross  – consume SPIR-V, emit GLSL ES 3.2 (ESSL 320)
//   Stage 4: Post-process – remove layout(binding), fix precision, add extensions
//
// Key design decisions:
//   - EShClientOpenGL is used for BOTH setEnvInput and setEnvClient
//     This tells glslang to parse OpenGL GLSL semantics (not Vulkan)
//   - EShMsgSpvRules ensures SPIR-V is generated with OpenGL-style I/O mapping
//   - Auto-map locations/bindings enabled for OpenGL compatibility
//   - Target: OpenGL ES 3.2 (ESSL 320) with required extensions
//   - SPIR-V 1.5 with relaxed Vulkan rules for maximum compatibility
//
// Target: OpenGL ES 3.2
// ============================================================================

#include "glsl_for_es.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/Include/Types.h>
#include <spirv_cross/spirv_cross_c.h>
#include <iostream>
#include <fstream>
#include "../log.h"
#include "glslang/SPIRV/GlslangToSpv.h"
#include <string>
#include <regex>
#include <strstream>
#include <algorithm>
#include <sstream>
#include <set>
#include <map>
#include <vector>
#include <mutex>
#include "cache.h"
#include "../../version.h"

#define DEBUG 0

// ============================================================================
// Section 1: Constants
// ============================================================================

// Watermark inserted when atomic counters are converted to SSBOs
const char* atomicCounterEmulatedWatermark = "// Non-opaque atomic uniform converted to SSBO";

// Supported EShTargetOpenGL versions for fallback
// glslang currently only supports EShTargetOpenGL_450 for OpenGL client
static const int k_opengl_version_fallbacks[] = {450, 440, 430, 420, 410, 400, 330};

// Target ESSL version (always ES 3.2)
static const uint k_target_essl_version = 320;

// Target SPIR-V version
static const glslang::EShTargetSpv k_target_spv = glslang::EShTargetSpv_1_5;

// ============================================================================
// Section 2: Version Mapping
// ============================================================================

// Map desktop GLSL version → glslang EShTargetOpenGL version
// glslang only supports these discrete versions for OpenGL client
static int map_glsl_to_opengl_version(int glsl_version) {
    if (glsl_version >= 460) return 460;
    if (glsl_version >= 450) return 450;
    if (glsl_version >= 440) return 440;
    if (glsl_version >= 430) return 430;
    if (glsl_version >= 420) return 420;
    if (glsl_version >= 410) return 410;
    if (glsl_version >= 400) return 400;
    if (glsl_version >= 330) return 330;
    if (glsl_version >= 150) return 150;
    if (glsl_version >= 140) return 140;
    return 330; // minimum for core profile
}

// Map GLSL version to EShTargetOpenGL_* enum
static EShTargetClientVersion glsl_version_to_target(int glsl_version) {
    if (glsl_version >= 450) return EShTargetOpenGL_450;
    if (glsl_version >= 440) return EShTargetOpenGL_440;
    if (glsl_version >= 430) return EShTargetOpenGL_430;
    if (glsl_version >= 420) return EShTargetOpenGL_420;
    if (glsl_version >= 410) return EShTargetOpenGL_410;
    if (glsl_version >= 400) return EShTargetOpenGL_400;
    if (glsl_version >= 330) return EShTargetOpenGL_330;
    if (glsl_version >= 150) return EShTargetOpenGL_150;
    return EShTargetOpenGL_450; // default: use the highest supported
}

// ============================================================================
// Section 3: Resource Limits
// ============================================================================

static TBuiltInResource InitResources() {
    TBuiltInResource Resources{};

    Resources.maxLights = 32;
    Resources.maxClipPlanes = 6;
    Resources.maxTextureUnits = 32;
    Resources.maxTextureCoords = 32;
    Resources.maxVertexAttribs = 64;
    Resources.maxVertexUniformComponents = 4096;
    Resources.maxVaryingFloats = 64;
    Resources.maxVertexTextureImageUnits = 32;
    Resources.maxCombinedTextureImageUnits = 80;
    Resources.maxTextureImageUnits = 32;
    Resources.maxFragmentUniformComponents = 4096;
    Resources.maxDrawBuffers = 32;
    Resources.maxVertexUniformVectors = 128;
    Resources.maxVaryingVectors = 8;
    Resources.maxFragmentUniformVectors = 16;
    Resources.maxVertexOutputVectors = 16;
    Resources.maxFragmentInputVectors = 15;
    Resources.minProgramTexelOffset = -8;
    Resources.maxProgramTexelOffset = 7;
    Resources.maxClipDistances = 8;
    Resources.maxComputeWorkGroupCountX = 65535;
    Resources.maxComputeWorkGroupCountY = 65535;
    Resources.maxComputeWorkGroupCountZ = 65535;
    Resources.maxComputeWorkGroupSizeX = 1024;
    Resources.maxComputeWorkGroupSizeY = 1024;
    Resources.maxComputeWorkGroupSizeZ = 64;
    Resources.maxComputeUniformComponents = 1024;
    Resources.maxComputeTextureImageUnits = 16;
    Resources.maxComputeImageUniforms = 8;
    Resources.maxComputeAtomicCounters = 8;
    Resources.maxComputeAtomicCounterBuffers = 1;
    Resources.maxVaryingComponents = 60;
    Resources.maxVertexOutputComponents = 64;
    Resources.maxGeometryInputComponents = 64;
    Resources.maxGeometryOutputComponents = 128;
    Resources.maxFragmentInputComponents = 128;
    Resources.maxImageUnits = 8;
    Resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
    Resources.maxCombinedShaderOutputResources = 8;
    Resources.maxImageSamples = 0;
    Resources.maxVertexImageUniforms = 0;
    Resources.maxTessControlImageUniforms = 0;
    Resources.maxTessEvaluationImageUniforms = 0;
    Resources.maxGeometryImageUniforms = 0;
    Resources.maxFragmentImageUniforms = 8;
    Resources.maxCombinedImageUniforms = 8;
    Resources.maxGeometryTextureImageUnits = 16;
    Resources.maxGeometryOutputVertices = 256;
    Resources.maxGeometryTotalOutputComponents = 1024;
    Resources.maxGeometryUniformComponents = 1024;
    Resources.maxGeometryVaryingComponents = 64;
    Resources.maxTessControlInputComponents = 128;
    Resources.maxTessControlOutputComponents = 128;
    Resources.maxTessControlTextureImageUnits = 16;
    Resources.maxTessControlUniformComponents = 1024;
    Resources.maxTessControlTotalOutputComponents = 4096;
    Resources.maxTessEvaluationInputComponents = 128;
    Resources.maxTessEvaluationOutputComponents = 128;
    Resources.maxTessEvaluationTextureImageUnits = 16;
    Resources.maxTessEvaluationUniformComponents = 1024;
    Resources.maxTessPatchComponents = 120;
    Resources.maxPatchVertices = 32;
    Resources.maxTessGenLevel = 64;
    Resources.maxViewports = 16;
    Resources.maxVertexAtomicCounters = 0;
    Resources.maxTessControlAtomicCounters = 0;
    Resources.maxTessEvaluationAtomicCounters = 0;
    Resources.maxGeometryAtomicCounters = 0;
    Resources.maxFragmentAtomicCounters = 8;
    Resources.maxCombinedAtomicCounters = 8;
    Resources.maxAtomicCounterBindings = 1;
    Resources.maxVertexAtomicCounterBuffers = 0;
    Resources.maxTessControlAtomicCounterBuffers = 0;
    Resources.maxTessEvaluationAtomicCounterBuffers = 0;
    Resources.maxGeometryAtomicCounterBuffers = 0;
    Resources.maxFragmentAtomicCounterBuffers = 1;
    Resources.maxCombinedAtomicCounterBuffers = 1;
    Resources.maxAtomicCounterBufferSize = 16384;
    Resources.maxTransformFeedbackBuffers = 4;
    Resources.maxTransformFeedbackInterleavedComponents = 64;
    Resources.maxCullDistances = 8;
    Resources.maxCombinedClipAndCullDistances = 8;
    Resources.maxSamples = 4;
    Resources.maxMeshOutputVerticesNV = 256;
    Resources.maxMeshOutputPrimitivesNV = 512;
    Resources.maxMeshWorkGroupSizeX_NV = 32;
    Resources.maxMeshWorkGroupSizeY_NV = 1;
    Resources.maxMeshWorkGroupSizeZ_NV = 1;
    Resources.maxTaskWorkGroupSizeX_NV = 32;
    Resources.maxTaskWorkGroupSizeY_NV = 1;
    Resources.maxTaskWorkGroupSizeZ_NV = 1;
    Resources.maxMeshViewCountNV = 4;

    Resources.limits.nonInductiveForLoops = true;
    Resources.limits.whileLoops = true;
    Resources.limits.doWhileLoops = true;
    Resources.limits.generalUniformIndexing = true;
    Resources.limits.generalAttributeMatrixVectorIndexing = true;
    Resources.limits.generalVaryingIndexing = true;
    Resources.limits.generalSamplerIndexing = true;
    Resources.limits.generalVariableIndexing = true;
    Resources.limits.generalConstantMatrixVectorIndexing = true;

    return Resources;
}

// ============================================================================
// Section 4: Utility Functions
// ============================================================================

// Extract #version from GLSL source
int getGLSLVersion(const char* glsl_code) {
    std::string code(glsl_code);
    static std::regex version_pattern(R"(#version\s+(\d{3}))");
    std::smatch match;
    if (std::regex_search(code, match, version_pattern)) {
        return std::stoi(match[1].str());
    }
    return -1;
}

// Replace all occurrences of a substring in-place
static inline void replace_all(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

// Replace all occurrences matching a regex pattern in-place
static inline void replace_all_regex(std::string& str, const std::regex& pattern, const std::string& replacement) {
    str = std::regex_replace(str, pattern, replacement);
}

// Find insertion point after #version, #extension, and whitespace
static size_t find_insertion_point(const std::string& glsl) {
    size_t pos = 0;
    size_t insertion_point = 0;

    size_t version_pos = glsl.find("#version");
    if (version_pos != std::string::npos) {
        size_t version_end = glsl.find('\n', version_pos);
        if (version_end == std::string::npos) {
            version_end = glsl.length();
        } else {
            version_end++;
        }
        insertion_point = version_end;
        pos = version_end;
    }

    while (pos < glsl.length()) {
        while (pos < glsl.length() && std::isspace(glsl[pos])) {
            pos++;
        }
        if (pos >= glsl.length()) break;

        if (glsl[pos] == '#') {
            pos++;
            while (pos < glsl.length() && std::isspace(glsl[pos])) {
                pos++;
            }
            if (glsl.compare(pos, 9, "extension") == 0) {
                size_t ext_end = glsl.find('\n', pos);
                if (ext_end == std::string::npos) {
                    ext_end = glsl.length();
                } else {
                    ext_end++;
                }
                insertion_point = ext_end;
                pos = ext_end;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    return insertion_point;
}

// Remove all lines that start with a given prefix
std::string replace_line_starting_with(const std::string& glslCode, const std::string& starting,
                                       const std::string& substitution = "") {
    std::string result;
    size_t length = glslCode.size();
    size_t start = 0;
    size_t current = 0;

    auto append_chunk = [&](size_t end) {
        if (end > start) {
            result.append(glslCode, start, end - start);
        }
    };

    while (current < length) {
        size_t lineStart = current;
        while (current < length && (glslCode[current] == ' ' || glslCode[current] == '\t')) {
            current++;
        }

        bool isMatch = false;
        if (current + starting.length() <= length && glslCode.compare(current, starting.length(), starting) == 0) {
            isMatch = true;
        }

        while (current < length && glslCode[current] != '\r' && glslCode[current] != '\n') {
            current++;
        }

        size_t newlineLength = 0;
        if (current < length) {
            if (glslCode[current] == '\r') {
                newlineLength = (current + 1 < length && glslCode[current + 1] == '\n') ? 2 : 1;
            } else {
                newlineLength = 1;
            }
        }

        if (isMatch) {
            append_chunk(lineStart);
            current += newlineLength;
            start = current;
            result += substitution;
        } else {
            current += newlineLength;
        }
    }

    append_chunk(current);
    return result;
}

// ============================================================================
// Section 5: Legacy GLSL → Modern GLSL Transformations
// ============================================================================

static bool is_compatibility_profile(const std::string& glsl) {
    return glsl.find("compatibility") != std::string::npos;
}

static std::string strip_compatibility_profile(const std::string& glsl) {
    static std::regex compat_regex(R"(#version\s+(\d+)\s+compatibility)");
    return std::regex_replace(glsl, compat_regex, "#version $1 core");
}

// Replace legacy texture functions with modern equivalents
static void upgrade_texture_functions(std::string& result) {
    replace_all(result, "texture2D(", "texture(");
    replace_all(result, "texture2DArray(", "texture(");
    replace_all(result, "texture2DLod(", "textureLod(");
    replace_all(result, "texture2DProj(", "textureProj(");
    replace_all(result, "texture2DProjLod(", "textureProjLod(");
    replace_all(result, "texture3D(", "texture(");
    replace_all(result, "texture3DLod(", "textureLod(");
    replace_all(result, "textureCube(", "texture(");
    replace_all(result, "textureCubeLod(", "textureLod(");
    replace_all(result, "shadow2D(", "texture(");
    replace_all(result, "shadow2DProj(", "textureProj(");
}

// Handle gl_TextureMatrix references (legacy OpenGL built-in) - replace with identity
static void handle_glTextureMatrix(std::string& result) {
    if (result.find("gl_TextureMatrix") != std::string::npos) {
        replace_all_regex(result, std::regex(R"(gl_TextureMatrix\s*\[\s*(\d+)\s*\])"), "mat4(1.0)");
    }
}

// Clean unsupported #extension directives
static void clean_extensions(std::string& result) {
    static const std::vector<std::string> unsupported_extensions = {
        "GL_ARB_compatibility",
        "GL_ARB_shader_texture_lod",
        "GL_EXT_gpu_shader4",
    };

    for (const auto& ext : unsupported_extensions) {
        std::regex ext_pattern("#extension\\s+" + ext + "\\s*:\\s*\\w+");
        result = std::regex_replace(result, ext_pattern, "// $&");
    }
}

// ============================================================================
// Section 6: ESSL Post-processing
// ============================================================================

// Ensure precision qualifiers are present in the output
std::string forceSupporterOutput(const std::string& glslCode) {
    bool hasPrecisionFloat =
        glslCode.find("precision ") != std::string::npos && glslCode.find("float;") != std::string::npos;
    bool hasPrecisionInt =
        glslCode.find("precision ") != std::string::npos && glslCode.find("int;") != std::string::npos;

    std::string result = glslCode;
    std::string precisionFloat;
    std::string precisionInt;

    if (hasPrecisionFloat && hasPrecisionInt) {
        // Remove existing precision, re-add as highp at the top
        std::istringstream iss(result);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(iss, line)) {
            bool isPrecisionLine = (line.find("precision ") != std::string::npos) &&
                                   (line.find("float;") != std::string::npos || line.find("int;") != std::string::npos);
            if (!isPrecisionLine) {
                lines.push_back(line);
            }
        }
        result.clear();
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i != 0) result += '\n';
            result += lines[i];
        }
        precisionFloat = "precision highp float;\n";
        precisionInt = "precision highp int;\n";
    } else {
        precisionFloat = hasPrecisionFloat ? "" : "precision highp float;\n";
        precisionInt = hasPrecisionInt ? "" : "precision highp int;\n";
    }

    size_t lastExtensionPos = result.rfind("#extension");
    size_t insertionPos = 0;

    if (lastExtensionPos != std::string::npos) {
        size_t nextNewline = result.find('\n', lastExtensionPos);
        if (nextNewline != std::string::npos) {
            insertionPos = nextNewline + 1;
        } else {
            insertionPos = result.length();
        }
    } else {
        size_t firstNewline = result.find('\n');
        if (firstNewline != std::string::npos) {
            insertionPos = firstNewline + 1;
        } else {
            result = precisionFloat + precisionInt + result;
            return result;
        }
    }

    result.insert(insertionPos, precisionFloat + precisionInt);
    return result;
}

// Remove layout(binding=N) from spirv-cross output (non-compute only)
// Handles: layout(binding=N) → (remove), layout(binding=N, X) → layout(X), etc.
std::string removeLayoutBinding(const std::string& glslCode) {
    std::string result;
    result.reserve(glslCode.size());
    size_t i = 0;
    const size_t n = glslCode.size();

    while (i < n) {
        if (glslCode.compare(i, 7, "layout(") == 0) {
            size_t start = i;
            size_t depth = 1;
            size_t j = i + 7;
            while (j < n && depth > 0) {
                if (glslCode[j] == '(') depth++;
                else if (glslCode[j] == ')') depth--;
                j++;
            }
            std::string inner = glslCode.substr(start + 7, j - start - 8);

            static std::regex binding_rx(R"(\s*binding\s*=\s*\d+\s*,?\s*)");
            inner = std::regex_replace(inner, binding_rx, "");

            while (!inner.empty() && (inner.front() == ',' || inner.front() == ' '))
                inner.erase(0, 1);
            while (!inner.empty() && (inner.back() == ',' || inner.back() == ' '))
                inner.pop_back();

            if (inner.empty()) {
                while (j < n && (glslCode[j] == ' ' || glslCode[j] == '\t' || glslCode[j] == '\n'))
                    j++;
            } else {
                result += "layout(" + inner + ")";
            }
            i = j;
        } else {
            result += glslCode[i];
            i++;
        }
    }
    return result;
}

// Add layout(location=N) to outColorN declarations
std::string processOutColorLocations(const std::string& glslCode) {
    const static std::regex pattern(R"(\n(out\s+(?:highp\s+|mediump\s+|lowp\s+)?vec4\s+outColor)(\d+);)");
    const std::string replacement = "\nlayout(location=$2) $1$2;";
    return std::regex_replace(glslCode, pattern, replacement);
}

// Process uniform declarations: strip initializers (ES doesn't allow them)
std::string process_uniform_declarations(const std::string& glslCode) {
    std::string result;
    size_t scan_pos = 0;
    const size_t length = glslCode.length();

    result.reserve(glslCode.length() + 256);

    while (scan_pos < length) {
        if (glslCode.compare(scan_pos, 7, "uniform") == 0) {
            if (scan_pos + 7 < length && std::isalnum(glslCode[scan_pos + 7])) {
                result += glslCode[scan_pos];
                ++scan_pos;
                continue;
            }
            if (scan_pos > 0 && std::isalnum(glslCode[scan_pos - 1])) {
                result += glslCode[scan_pos];
                ++scan_pos;
                continue;
            }

            size_t decl_end = glslCode.find(';', scan_pos);
            if (decl_end == std::string::npos) {
                result.append(glslCode, scan_pos, length - scan_pos);
                break;
            }
            ++decl_end;

            size_t eq_pos = glslCode.find('=', scan_pos);
            bool has_initializer = (eq_pos != std::string::npos && eq_pos < decl_end);

            size_t brace_pos = glslCode.find('{', scan_pos);
            bool is_block = (brace_pos != std::string::npos && brace_pos < decl_end);

            if (has_initializer && !is_block) {
                std::string decl = glslCode.substr(scan_pos, eq_pos - scan_pos);
                while (!decl.empty() && std::isspace(decl.back()))
                    decl.pop_back();
                result += decl + ";\n";
            } else {
                result.append(glslCode, scan_pos, decl_end - scan_pos);
            }

            scan_pos = decl_end;
        } else {
            result += glslCode[scan_pos];
            ++scan_pos;
        }
    }

    return result;
}

// ============================================================================
// Section 7: Atomic Counter → SSBO Emulation
// ============================================================================

bool process_non_opaque_atomic_to_ssbo(std::string& source) {
    if (source.find("atomicCounter") == std::string::npos) return false;

    std::set<std::string> atomic_vars;
    std::map<std::string, std::string> binding_map;
    std::regex decl_rx(
        R"(layout\s*\(\s*binding\s*=\s*(\d+)\s*(?:,\s*offset\s*=\s*(\d+)\s*)?\)\s*uniform\s+atomic_uint\s+(\w+)\s*;)",
        std::regex::icase);

    std::smatch m;
    auto it = source.cbegin();
    while (std::regex_search(it, source.cend(), m, decl_rx)) {
        size_t prefix = std::distance(source.cbegin(), it);
        size_t match_pos = prefix + m.position(0);
        size_t match_len = m.length(0);

        std::string binding = m[1].str();
        std::string var = m[3].str();
        atomic_vars.insert(var);
        binding_map[var] = binding;

        std::string repl = "layout(std430, binding=" + binding + ") coherent buffer AtomicCounterSSBO_" + binding +
                           " {\n"
                           "    coherent uint " + var + ";\n"
                           "};\n";
        source.replace(match_pos, match_len, repl);

        it = source.cbegin() + match_pos + repl.size();
    }

    if (atomic_vars.empty()) return true;

    for (auto& var : atomic_vars) {
        source = std::regex_replace(
            source, std::regex(R"(\batomicCounterIncrement\s*\(\s*)" + var + R"(\s*\))", std::regex::icase),
            "atomicAdd(" + var + ", 1u)");
        source = std::regex_replace(
            source, std::regex(R"(\batomicCounterDecrement\s*\(\s*)" + var + R"(\s*\))", std::regex::icase),
            "atomicAdd(" + var + ", uint(-1))");
        source = std::regex_replace(
            source, std::regex(R"(\batomicCounterAdd\s*\(\s*)" + var + R"(\s*,\s*([^)]+)\s*\))", std::regex::icase),
            "atomicAdd(" + var + ", $1)");
        source = std::regex_replace(
            source, std::regex(R"(\batomicCounter\s*\(\s*)" + var + R"(\s*\))", std::regex::icase), var);
    }

    // Insert memoryBarrierBuffer after atomicAdd calls
    {
        std::regex rx_barrier(R"(([ \t]*\batomicAdd\b[^;]*;))", std::regex::icase);

        std::set<size_t> processed_positions;
        std::string res;
        size_t last_pos = 0;

        for (auto it = std::sregex_iterator(source.begin(), source.end(), rx_barrier); it != std::sregex_iterator();
             ++it) {
            size_t start_pos = it->position();
            size_t end_pos = start_pos + it->length();

            if (processed_positions.find(start_pos) != processed_positions.end()) {
                continue;
            }

            res += source.substr(last_pos, start_pos - last_pos);
            res += it->str();
            res += "\n    memoryBarrierBuffer();";

            processed_positions.insert(start_pos);
            last_pos = end_pos;
        }

        res += source.substr(last_pos);
        source = res;
    }

    source += "\n" + std::string(atomicCounterEmulatedWatermark);
    return true;
}

// ============================================================================
// Section 8: Sampler Buffer Emulation
// ============================================================================

void process_sampler_buffer(std::string& source) {
    if (source.find("isamplerBuffer") == std::string::npos &&
        source.find("samplerBuffer") == std::string::npos) {
        return;
    }

    std::set<std::string> buffer_samplers;

    // Replace samplerBuffer types with sampler2D equivalents
    size_t pos = 0;
    while ((pos = source.find("isamplerBuffer", pos)) != std::string::npos) {
        source.replace(pos, 14, "isampler2D");
        pos += 11;
    }
    pos = 0;
    while ((pos = source.find("samplerBuffer", pos)) != std::string::npos) {
        if (pos > 0 && (source[pos - 1] == 'i' || source[pos - 1] == 'u')) {
            pos += 13;
            continue;
        }
        source.replace(pos, 13, "sampler2D");
        pos += 10;
    }

    // Extract sampler variable names from declarations
    {
        std::regex sampler_decl(R"(uniform\s+(?:i)?sampler2D\s+(\w+)\s*;)");
        auto it = std::sregex_iterator(source.begin(), source.end(), sampler_decl);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            buffer_samplers.insert((*it)[1].str());
        }
    }

    // Replace texelFetch with bufferCoords helper using manual parenthesis-matching parser.
    // This handles nested parentheses in index expressions (fix for Sodium 0.9.x).
    {
        std::string result;
        result.reserve(source.size() * 2);
        size_t scan_pos = 0;
        const size_t src_len = source.size();

        while (scan_pos < src_len) {
            size_t tf_pos = source.find("texelFetch(", scan_pos);
            if (tf_pos == std::string::npos) {
                result.append(source, scan_pos, src_len - scan_pos);
                break;
            }

            result.append(source, scan_pos, tf_pos - scan_pos);

            size_t paren_start = tf_pos + 11;
            size_t arg_start = paren_start;

            while (arg_start < src_len && std::isspace(source[arg_start]))
                arg_start++;

            size_t arg1_start = arg_start;
            while (arg_start < src_len && (std::isalnum(source[arg_start]) || source[arg_start] == '_'))
                arg_start++;
            std::string sampler_name = source.substr(arg1_start, arg_start - arg1_start);

            // Only transform if this sampler was declared as a buffer type
            if (buffer_samplers.find(sampler_name) == buffer_samplers.end()) {
                result.append("texelFetch(");
                scan_pos = paren_start;
                continue;
            }

            while (arg_start < src_len && std::isspace(source[arg_start]))
                arg_start++;

            if (arg_start >= src_len || source[arg_start] != ',') {
                result.append("texelFetch(");
                scan_pos = paren_start;
                continue;
            }
            arg_start++;

            while (arg_start < src_len && std::isspace(source[arg_start]))
                arg_start++;

            size_t arg2_start = arg_start;
            int paren_depth = 0;
            size_t arg2_end = arg_start;
            while (arg2_end < src_len) {
                char c = source[arg2_end];
                if (c == '(') {
                    paren_depth++;
                } else if (c == ')') {
                    if (paren_depth == 0) break;
                    paren_depth--;
                } else if (c == ',' && paren_depth == 0) {
                    break;
                }
                arg2_end++;
            }

            std::string index_expr = source.substr(arg2_start, arg2_end - arg2_start);
            while (!index_expr.empty() && std::isspace(index_expr.back()))
                index_expr.pop_back();

            while (arg2_end < src_len && source[arg2_end] != ')')
                arg2_end++;

            result.append("texelFetch(" + sampler_name + ", bufferCoords(" + index_expr + "), 0)");
            scan_pos = arg2_end + 1;
        }
        source = result;
    }

    // Inject helper function and uniforms
    const char* boundaryProtection = R"(
ivec2 bufferCoords(int index) {
    return ivec2(index % u_BufferTexWidth, index / u_BufferTexWidth);
}
)";

    size_t insertion_point = find_insertion_point(source);
    if (insertion_point != std::string::npos) {
        source.insert(insertion_point, boundaryProtection);
    }

    const char* uniformDecl = R"(
uniform int u_BufferTexWidth;
uniform int u_BufferTexHeight;
)";

    insertion_point = find_insertion_point(source);
    if (insertion_point != std::string::npos) {
        insertion_point = source.find('\n', insertion_point);
        if (insertion_point != std::string::npos) {
            source.insert(insertion_point + 1, uniformDecl);
        }
    }
}

// ============================================================================
// Section 9: Feature Injections
// ============================================================================

static void inject_textureQueryLod(std::string& glsl) {
    const std::regex defRegex(R"(vec2\s+mg_textureQueryLod\s*\()", std::regex::ECMAScript);

    if (glsl.find("textureQueryLod") == std::string::npos) {
        return;
    }
    if (std::regex_search(glsl, defRegex)) {
        return;
    }

    const std::string textureQueryLodImpl = R"(
#define textureQueryLod mg_textureQueryLod

vec2 mg_textureQueryLod(sampler2D tex, vec2 uv) {
    vec2 texSizeF = vec2(textureSize(tex, 0));
    vec2 dFdx_uv = dFdx(uv * texSizeF);
    vec2 dFdy_uv = dFdy(uv * texSizeF);
    float maxDerivative = max(length(dFdx_uv), length(dFdy_uv));
    float lod = log2(maxDerivative);
    return vec2(lod);
}
)";

    size_t insertPos = find_insertion_point(glsl);
    glsl.insert(insertPos, "\n" + textureQueryLodImpl + "\n");
}

static inline void inject_temporal_filter(std::string& glsl) {
    const std::regex defRegex(R"(vec4\s+GI_TemporalFilter\s*\()", std::regex::ECMAScript);

    if (glsl.find("GI_TemporalFilter") == std::string::npos) {
        return;
    }
    if (std::regex_search(glsl, defRegex)) {
        return;
    }

    const std::regex uniformRegex(
        R"(^\s*(?:layout\s*\([^)]*\)\s*)?uniform\s+\w+(?:\s*\[\s*\d+\s*\])?\s+\w+(?:\s*\[\s*\d+\s*\])?\s*;.*$)",
        std::regex::ECMAScript | std::regex::multiline);
    std::sregex_iterator it(glsl.begin(), glsl.end(), uniformRegex);
    std::sregex_iterator end;
    size_t insertPos = 0;
    for (; it != end; ++it) {
        insertPos = it->position() + it->length();
    }

    const std::string GI_TemporalFilterImpl = R"(
vec4 GI_TemporalFilter() {
    vec2 uv = gl_FragCoord.xy / screenSize;
    uv += taaJitter * pixelSize;
    vec4 currentGI = texture(colortex0, uv);
    float depth = texture(depthtex0, uv).r;
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = gbufferProjectionInverse * clipPos;
    viewPos /= viewPos.w;
    vec4 worldPos = gbufferModelViewInverse * viewPos;
    vec4 prevClipPos = gbufferPreviousProjection * (gbufferPreviousModelView * worldPos);
    prevClipPos /= prevClipPos.w;
    vec2 prevUV = prevClipPos.xy * 0.5 + 0.5;
    vec4 historyGI = texture(colortex1, prevUV);
    float difference = length(currentGI.rgb - historyGI.rgb);
    float thresholdValue = 0.1;
    float adaptiveBlend = mix(0.9, 0.0, smoothstep(thresholdValue, thresholdValue * 2.0, difference));
    vec4 filteredGI = mix(currentGI, historyGI, adaptiveBlend);
    if (difference > thresholdValue * 2.0) {
        filteredGI = currentGI;
    }
    return filteredGI;
}
)";
    glsl.insert(insertPos, "\n" + GI_TemporalFilterImpl + "\n");
}

#define xstr(s) str(s)
#define str(s) #s

// Inject MobileGlues and Iris macros for platform detection
void inject_mg_macro_definition(std::string& glslCode) {
    std::string macro_definitions =
        "\n#define MG_MOBILEGLUES\n"
        "#define MC_MOBILEGLUES\n"
        "#define IS_IRIS\n"
        "#define MC_GL_RENDERER_MOBILEGLUES\n"
        "#define RENDERER_MOBILEGLUES\n"
        "#define MG_MOBILEGLUES_VERSION " xstr(MAJOR) xstr(MINOR) xstr(REVISION) xstr(PATCH) "\n";

    size_t versionPos = glslCode.rfind("#version");
    size_t insertionPos = 0;

    if (versionPos != std::string::npos) {
        size_t nextNewline = glslCode.find('\n', versionPos);
        insertionPos = (nextNewline != std::string::npos) ? nextNewline + 1 : glslCode.length();
    } else {
        size_t firstNewline = glslCode.find('\n');
        insertionPos = (firstNewline != std::string::npos) ? firstNewline + 1 : 0;
    }

    glslCode.insert(insertionPos, macro_definitions);
}

// ============================================================================
// Section 10: Preprocessing Pipeline
// ============================================================================

// Stage 1: Transform desktop GLSL into a form that glslang can parse as EShClientOpenGL
std::string preprocess_glsl(const std::string& glsl, GLenum shaderType, bool* atomicCounterEmulated) {
    std::string ret = glsl;

    // Step 1: Remove #line directives (confuse glslang version detection)
    ret = replace_line_starting_with(ret, "#line");

    // Step 2: Strip compatibility profile → core profile
    if (is_compatibility_profile(ret)) {
        ret = strip_compatibility_profile(ret);
    }

    // Step 3: Upgrade legacy texture functions, handle gl_TextureMatrix, clean extensions
    upgrade_texture_functions(ret);
    handle_glTextureMatrix(ret);
    clean_extensions(ret);

    // Step 4: Disable GL_ARB_derivative_control blocks (not in ESSL)
    replace_all(ret, "#ifdef GL_ARB_derivative_control", "#if 0");
    replace_all(ret, "#ifndef GL_ARB_derivative_control", "#if 1");

    // Step 5: Polyfill transpose() for mat3
    replace_all(ret, "const mat3 rotInverse = transpose(rot);",
                "const mat3 rotInverse = mat3(rot[0][0], rot[1][0], rot[2][0], rot[0][1], rot[1][1], rot[2][1], "
                "rot[0][2], rot[1][2], rot[2][2]);");

    // Step 6: Feature injections
    inject_temporal_filter(ret);

    if (!g_gles_caps.GL_EXT_texture_query_lod) {
        inject_textureQueryLod(ret);
    }

    // Step 7: MobileGlues/Iris macro injection
    inject_mg_macro_definition(ret);

    // Step 8: Sampler buffer processing
    if (hardware->emulate_texture_buffer) {
        process_sampler_buffer(ret);
    }

    // Step 9: Atomic counter emulation
    *atomicCounterEmulated = process_non_opaque_atomic_to_ssbo(ret);

    return ret;
}

// ============================================================================
// Section 11: Version Handling
// ============================================================================

// Normalize GLSL version: ensure #version is present and at least 330 core
int get_or_add_glsl_version(std::string& glsl) {
    int glsl_version = getGLSLVersion(glsl.c_str());
    if (glsl_version == -1) {
        glsl_version = 330;
        glsl.insert(0, "#version 330 core\n");
    } else if (glsl_version < 140) {
        glsl = replace_line_starting_with(glsl, "#version", "#version 330 core\n");
        glsl_version = 330;
    } else if (glsl_version < 330) {
        static std::regex version_regex(R"(#version\s+\d+(\s+\w+)?)");
        glsl = std::regex_replace(glsl, version_regex, "#version 330 core");
        glsl_version = 330;
    }

    // Ensure "core" profile for versions >= 150
    if (glsl_version >= 150 && glsl.find("compatibility") == std::string::npos &&
        glsl.find("core") == std::string::npos && glsl.find("es") == std::string::npos) {
        static std::regex version_regex(R"(#version\s+\d+)");
        glsl = std::regex_replace(glsl, version_regex, "$& core");
    }

    LOG_D("GLSL version: %d", glsl_version)
    return glsl_version;
}

// ============================================================================
// Section 12: Stage 2 — GLSL → SPIR-V (glslang with EShClientOpenGL)
// ============================================================================

// Map GLenum shader type to glslang EShLanguage
static EShLanguage glenum_to_esh_language(GLenum shader_type) {
    switch (shader_type) {
    case GL_VERTEX_SHADER:          return EShLanguage::EShLangVertex;
    case GL_FRAGMENT_SHADER:        return EShLanguage::EShLangFragment;
    case GL_COMPUTE_SHADER:         return EShLanguage::EShLangCompute;
    case GL_TESS_CONTROL_SHADER:    return EShLanguage::EShLangTessControl;
    case GL_TESS_EVALUATION_SHADER: return EShLanguage::EShLangTessEvaluation;
    case GL_GEOMETRY_SHADER:        return EShLanguage::EShLangGeometry;
    default:                        return EShLanguage::EShLangCount; // invalid
    }
}

// Configure glslang TShader for EShClientOpenGL parsing
static void configure_shader_for_opengl(glslang::TShader& shader, EShLanguage shader_language,
                                        int glsl_version, int target_gl_version) {
    using namespace glslang;

    // KEY: Set input to EShClientOpenGL so glslang parses OpenGL GLSL semantics
    shader.setEnvInput(EShSourceGlsl, shader_language, EShClientOpenGL, target_gl_version);

    // Set client to OpenGL (not Vulkan)
    EShTargetClientVersion target_gl = glsl_version_to_target(target_gl_version);
    shader.setEnvClient(EShClientOpenGL, target_gl);

    // Output SPIR-V 1.5
    shader.setEnvTarget(EShTargetSpv, k_target_spv);

    // Auto-map locations and bindings for OpenGL compatibility
    shader.setAutoMapLocations(true);
    shader.setAutoMapBindings(true);

    // Relaxed rules for maximum compatibility with Iris shaders
    shader.setEnvInputVulkanRulesRelaxed();

    // Binding shift offsets (start from 0 for OpenGL)
    shader.setShiftSamplerBinding(0);
    shader.setShiftTextureBinding(0);
    shader.setShiftImageBinding(0);
    shader.setShiftUboBinding(0);
    shader.setShiftSsboBinding(0);
    shader.setShiftUavBinding(0);

    shader.setNoStorageFormat(false);
    shader.setNanMinMaxClamp(false);
}

// Compile GLSL to SPIR-V using glslang with EShClientOpenGL
// Tries multiple version strategies if the initial attempt fails
std::vector<unsigned int> glsl_to_spirv(GLenum shader_type, int glsl_version, const char* const* shader_src,
                                        int& errc) {
    EShLanguage shader_language = glenum_to_esh_language(shader_type);
    if (shader_language == EShLanguage::EShLangCount) {
        LOG_D("GLSL type not supported! type=%d", shader_type)
        errc = -1;
        return {};
    }

    using namespace glslang;
    TBuiltInResource resources = InitResources();

    // Use SpvRules for proper SPIR-V generation with OpenGL semantics
    EShMessages messages = (EShMessages)(EShMsgDefault | EShMsgSpvRules);

    int target_gl_version = map_glsl_to_opengl_version(glsl_version);

    // Strategy 1: Try with the mapped version
    {
        glslang::TShader shader(shader_language);
        shader.setStrings(shader_src, 1);
        configure_shader_for_opengl(shader, shader_language, glsl_version, target_gl_version);

        if (shader.parse(&resources, target_gl_version, true, messages)) {
            LOG_D("GLSL Compiled (v%d).", target_gl_version)

            glslang::TProgram program;
            program.addShader(&shader);

            if (program.link(messages)) {
                LOG_D("Shader Linked.")
                std::vector<unsigned int> spirv_code;
                glslang::SpvOptions spvOptions;
                spvOptions.disableOptimizer = false;
                spvOptions.validate = true;
                glslang::GlslangToSpv(*program.getIntermediate(shader_language), spirv_code, &spvOptions);
                errc = 0;
                return spirv_code;
            }
            LOG_D("Shader Linking ERROR: %s", program.getInfoLog())
        }
        LOG_D("GLSL Compiling ERROR (v%d): \n%s", target_gl_version, shader.getInfoLog())
    }

    // Strategy 2: Retry with version 450 (the highest glslang supports for OpenGL client)
    if (target_gl_version < 450) {
        glslang::TShader shader(shader_language);
        shader.setStrings(shader_src, 1);
        configure_shader_for_opengl(shader, shader_language, glsl_version, 450);

        if (shader.parse(&resources, 450, true, messages)) {
            LOG_D("GLSL Compiled (retry v450).")

            glslang::TProgram program;
            program.addShader(&shader);

            if (program.link(messages)) {
                LOG_D("Shader Linked (retry).")
                std::vector<unsigned int> spirv_code;
                glslang::SpvOptions spvOptions;
                spvOptions.disableOptimizer = false;
                spvOptions.validate = true;
                glslang::GlslangToSpv(*program.getIntermediate(shader_language), spirv_code, &spvOptions);
                errc = 0;
                return spirv_code;
            }
            LOG_D("Shader Linking ERROR (retry): %s", program.getInfoLog())
        }
        LOG_D("GLSL Compiling ERROR (retry v450): \n%s", shader.getInfoLog())
    }

    // Strategy 3: Retry without EShMsgSpvRules (looser rules)
    {
        glslang::TShader shader(shader_language);
        shader.setStrings(shader_src, 1);
        configure_shader_for_opengl(shader, shader_language, glsl_version, 450);

        EShMessages loose_messages = EShMsgDefault;
        if (shader.parse(&resources, 450, true, loose_messages)) {
            LOG_D("GLSL Compiled (loose rules).")

            glslang::TProgram program;
            program.addShader(&shader);

            if (program.link(loose_messages)) {
                LOG_D("Shader Linked (loose rules).")
                std::vector<unsigned int> spirv_code;
                glslang::SpvOptions spvOptions;
                spvOptions.disableOptimizer = false;
                spvOptions.validate = false;
                glslang::GlslangToSpv(*program.getIntermediate(shader_language), spirv_code, &spvOptions);
                errc = 0;
                return spirv_code;
            }
            LOG_D("Shader Linking ERROR (loose): %s", program.getInfoLog())
        }
        LOG_D("GLSL Compiling ERROR (loose): \n%s", shader.getInfoLog())
    }

    errc = -1;
    return {};
}

// ============================================================================
// Section 13: Stage 3 — SPIR-V → GLSL ES (spirv-cross)
// ============================================================================

std::string spirv_to_essl(std::vector<unsigned int> spirv, uint essl_version, GLenum shader_type, int& errc) {
    spvc_context context = nullptr;
    spvc_parsed_ir ir = nullptr;
    spvc_compiler compiler_glsl = nullptr;
    spvc_compiler_options options = nullptr;
    const char* result = nullptr;

    const SpvId* p_spirv = spirv.data();
    size_t word_count = spirv.size();

    LOG_D("spirv_code.size(): %zu", spirv.size())

    if (spvc_context_create(&context) != SPVC_SUCCESS) {
        LOG_E("spirv-cross: Failed to create context")
        errc = -1;
        return "";
    }

    if (spvc_context_parse_spirv(context, p_spirv, word_count, &ir) != SPVC_SUCCESS) {
        LOG_E("spirv-cross: Failed to parse SPIR-V: %s", spvc_context_get_last_error_string(context))
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }

    if (spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP,
                                     &compiler_glsl) != SPVC_SUCCESS) {
        LOG_E("spirv-cross: Failed to create compiler: %s", spvc_context_get_last_error_string(context))
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }

    spvc_compiler_create_compiler_options(compiler_glsl, &options);

    // Configure GLSL ES 3.2 output
    uint target_essl = 320;
    spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, target_essl);
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_TRUE);

    // Default precision for ESSL
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_FLOAT_PRECISION_HIGHP, SPVC_TRUE);
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_INT_PRECISION_HIGHP, SPVC_TRUE);

    // Enable 420pack extension for binding layout support
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ENABLE_420PACK_EXTENSION, SPVC_TRUE);

    // Emit UBOs as uniform blocks (not plain uniforms)
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_EMIT_UNIFORM_BUFFER_AS_PLAIN_UNIFORMS, SPVC_FALSE);

    // Emit push constants as plain uniforms (ES doesn't have push constants)
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_EMIT_PUSH_CONSTANT_AS_UNIFORM_BUFFER, SPVC_FALSE);

    // Support for non-zero base instance (ES 3.2 via EXT)
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_SUPPORT_NONZERO_BASE_INSTANCE, SPVC_TRUE);

    // Use separate shader objects layout (ES 3.1+)
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_SEPARATE_SHADER_OBJECTS, SPVC_TRUE);

    spvc_compiler_install_compiler_options(compiler_glsl, options);

    if (spvc_compiler_compile(compiler_glsl, &result) != SPVC_SUCCESS) {
        LOG_E("spirv-cross: Compilation failed: %s", spvc_context_get_last_error_string(context));
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }

    if (!result) {
        LOG_E("Error: unexpected error in spirv-cross.")
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }

    std::string essl = result;

    spvc_context_destroy(context);

    errc = 0;
    return essl;
}

// ============================================================================
// Section 14: Stage 4 — ESSL Post-processing
// ============================================================================

// Add required ESSL extensions based on shader type and content
static void add_required_extensions(std::string& essl, GLenum shader_type, uint essl_version) {
    std::string extensions;

    // Geometry shader requires GL_EXT_geometry_shader on ES 3.2
    if (shader_type == GL_GEOMETRY_SHADER) {
        extensions += "#extension GL_EXT_geometry_shader : require\n";
    }

    // Tessellation shaders require GL_EXT_tessellation_shader on ES 3.2
    if (shader_type == GL_TESS_CONTROL_SHADER || shader_type == GL_TESS_EVALUATION_SHADER) {
        extensions += "#extension GL_EXT_tessellation_shader : require\n";
    }

    if (!extensions.empty()) {
        size_t version_end = essl.find('\n');
        if (version_end != std::string::npos) {
            essl.insert(version_end + 1, extensions);
        }
    }
}

// Apply all post-processing stages to spirv-cross output
static std::string postprocess_essl(std::string essl, GLenum shader_type, uint essl_version) {
    // Remove layout(binding) for non-compute shaders (ES may not support on all targets)
    if (shader_type != GL_COMPUTE_SHADER) {
        essl = removeLayoutBinding(essl);
    }

    // Add layout(location) to outColor declarations
    essl = processOutColorLocations(essl);

    // Ensure precision qualifiers
    essl = forceSupporterOutput(essl);

    // Add required extensions
    add_required_extensions(essl, shader_type, essl_version);

    return essl;
}

// ============================================================================
// Section 15: Main Conversion Entry Points
// ============================================================================

static std::once_flag glslang_init_flag;

// Direct conversion (no caching) — used by GLSLtoGLSLES_2
std::string GLSLtoGLSLES_2(const char* glsl_code, GLenum glsl_type, uint essl_version, int& return_code) {
    bool atomicCounterEmulated = false;

    // Stage 1: Preprocess
    std::string correct_glsl_str = preprocess_glsl(glsl_code, glsl_type, &atomicCounterEmulated);
    LOG_D("Preprocessed GLSL:\n%s", correct_glsl_str.c_str())

    // Normalize version
    int glsl_version = get_or_add_glsl_version(correct_glsl_str);

    // Initialize glslang (once per process)
    std::call_once(glslang_init_flag, [] { glslang::InitializeProcess(); });

    // Stage 2: GLSL → SPIR-V
    const char* s[] = {correct_glsl_str.c_str()};
    int errc = 0;
    std::vector<unsigned int> spirv_code = glsl_to_spirv(glsl_type, glsl_version, s, errc);
    if (errc != 0) {
        return_code = -1;
        return "";
    }

    // Stage 3: SPIR-V → GLSL ES
    errc = 0;
    std::string essl = spirv_to_essl(spirv_code, essl_version, glsl_type, errc);
    if (errc != 0) {
        return_code = -2;
        return "";
    }

    // Stage 4: Post-process
    essl = postprocess_essl(essl, glsl_type, essl_version);

    LOG_D("GLSL to GLSL ES Complete: \n%s", essl.c_str())

    return_code = errc;
    if (return_code == 0) {
        return_code = atomicCounterEmulated ? 1 : 0;
    }
    return essl;
}

// Deprecated path — returns empty string
std::string GLSLtoGLSLES_1(const char* glsl_code, GLenum glsl_type, uint esversion, int& return_code) {
    // Deprecated: use GLSLtoGLSLES_2 instead
    (void)glsl_code;
    (void)glsl_type;
    (void)esversion;
    return_code = -1;
    return "";
}

// Main entry point with caching
std::string GLSLtoGLSLES(const char* glsl_code, GLenum glsl_type, uint essl_version, uint glsl_version,
                         int& return_code) {
    // Build cache key: source hash + shader type + MG version + target ESSL version
    std::string sha256_string(glsl_code);
    sha256_string += "\n//" + std::to_string(glsl_type) + "|" + std::to_string(MAJOR) + "." + std::to_string(MINOR) +
                     "." + std::to_string(REVISION) + "|" + std::to_string(essl_version);

    // Check cache
    int cached_rc = -1;
    const char* cachedESSL = Cache::get_instance().get(sha256_string.c_str(), &cached_rc);
    if (cachedESSL) {
        LOG_D("GLSL Hit Cache:\n%s\n-->\n%s", glsl_code, cachedESSL)
        return_code = cached_rc;
        return (char*)cachedESSL;
    }

    // Perform conversion
    return_code = -1;
    std::string converted = GLSLtoGLSLES_2(glsl_code, glsl_type, essl_version, return_code);
    if (return_code >= 0 && !converted.empty()) {
        converted = process_uniform_declarations(converted);
        Cache::get_instance().put(sha256_string.c_str(), converted.c_str(), return_code);
    }

    return (return_code >= 0) ? converted : glsl_code;
}