// MobileGlues - gl/texture.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

// ============================================================================
// Texture Module Architecture (OpenGL ES 3.2)
//
// Rule: "ES 3.2 native → native, ES 3.2 not native → CPU simulation"
//
// Native (ES 3.2 directly supports):
//   - glTexImage2D, glTexImage3D, glTexSubImage2D, glTexSubImage3D
//   - glTexStorage2D, glTexStorage3D
//   - glCompressedTexImage2D, glCompressedTexImage3D
//   - glCompressedTexSubImage2D, glCompressedTexSubImage3D
//   - glCopyTexImage2D, glCopyTexSubImage2D, glCopyTexSubImage3D
//   - glGenerateMipmap
//   - glTexParameterf, glTexParameteri, glTexParameteriv, glTexParameterfv
//   - glGetTexParameteriv, glGetTexParameterfv
//   - glGetTexLevelParameteriv, glGetTexLevelParameterfv
//   - glTexBuffer, glTexBufferRange
//   - glBindTexture (with 1D→2D mapping for legacy targets)
// ============================================================================

#include "texture.h"
#include "GLES3/gl32.h"

#include <cstdlib>
#include <cstring>
#include <vector>
#include <unordered_map>

#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <malloc.h>

#include "../gles/gles.h"
#include "../gles/loader.h"
#include "buffer.h"
#include "drawing.h"
#include "framebuffer.h"
#include "log.h"
#include "mg.h"
#include "pixel.h"
#include <GL/gl.h>

#define DEBUG 0

// ============================================================================
// Internal helpers: mip level size calculation
// ============================================================================

int nlevel(int size, int level) {
    if (size) {
        size >>= level;
        if (!size) size = 1;
    }
    return size;
}

// ============================================================================
// Texture target enum conversion
// ============================================================================

// Lookup table: TextureTarget → GLenum (dense enum 0..23)
static const GLenum kTextureTargetToGLEnum[] = {
    GL_TEXTURE_1D,                   // TEXTURE_1D = 0
    GL_PROXY_TEXTURE_1D,             // PROXY_TEXTURE_1D = 1
    GL_TEXTURE_1D_ARRAY,             // TEXTURE_1D_ARRAY = 2
    GL_PROXY_TEXTURE_1D_ARRAY,       // PROXY_TEXTURE_1D_ARRAY = 3
    GL_TEXTURE_2D,                   // TEXTURE_2D = 4
    GL_PROXY_TEXTURE_2D,             // PROXY_TEXTURE_2D = 5
    GL_TEXTURE_2D_ARRAY,             // TEXTURE_2D_ARRAY = 6
    GL_PROXY_TEXTURE_2D_ARRAY,       // PROXY_TEXTURE_2D_ARRAY = 7
    GL_TEXTURE_2D_MULTISAMPLE,       // TEXTURE_2D_MULTISAMPLE = 8
    GL_PROXY_TEXTURE_2D_MULTISAMPLE, // PROXY_TEXTURE_2D_MULTISAMPLE = 9
    GL_TEXTURE_2D_MULTISAMPLE_ARRAY,         // TEXTURE_2D_MULTISAMPLE_ARRAY = 10
    GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY,   // PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY = 11
    GL_TEXTURE_3D,                   // TEXTURE_3D = 12
    GL_PROXY_TEXTURE_3D,             // PROXY_TEXTURE_3D = 13
    GL_TEXTURE_RECTANGLE,            // TEXTURE_RECTANGLE = 14
    GL_PROXY_TEXTURE_RECTANGLE,      // PROXY_TEXTURE_RECTANGLE = 15
    GL_TEXTURE_CUBE_MAP,             // TEXTURE_CUBE_MAP = 16
    GL_PROXY_TEXTURE_CUBE_MAP,       // PROXY_TEXTURE_CUBE_MAP = 17
    GL_TEXTURE_CUBE_MAP_ARRAY,       // TEXTURE_CUBE_MAP_ARRAY = 18
    GL_PROXY_TEXTURE_CUBE_MAP_ARRAY, // PROXY_TEXTURE_CUBE_MAP_ARRAY = 19
    GL_TEXTURE_BUFFER,               // TEXTURE_BUFFER = 20
};

GLenum ConvertTextureTargetToGLEnum(TextureTarget target) {
    auto idx = static_cast<unsigned>(target);
    if (idx < sizeof(kTextureTargetToGLEnum) / sizeof(kTextureTargetToGLEnum[0])) [[likely]] {
        return kTextureTargetToGLEnum[idx];
    }
    return GL_TEXTURE_2D;
}

// Sorted key-value pair for binary search lookup (GLenum → TextureTarget)
struct TexTargetEntry {
    GLenum key;
    TextureTarget value;
};

static const TexTargetEntry kGLEnumToTextureTarget[] = {
    {GL_TEXTURE_1D, TextureTarget::TEXTURE_1D},
    {GL_TEXTURE_2D, TextureTarget::TEXTURE_2D},
    {GL_PROXY_TEXTURE_1D, TextureTarget::PROXY_TEXTURE_1D},
    {GL_PROXY_TEXTURE_2D, TextureTarget::PROXY_TEXTURE_2D},
    {GL_TEXTURE_3D, TextureTarget::TEXTURE_3D},
    {GL_PROXY_TEXTURE_3D, TextureTarget::PROXY_TEXTURE_3D},
    {GL_TEXTURE_RECTANGLE, TextureTarget::TEXTURE_RECTANGLE},
    {GL_PROXY_TEXTURE_RECTANGLE, TextureTarget::PROXY_TEXTURE_RECTANGLE},
    {GL_TEXTURE_CUBE_MAP, TextureTarget::TEXTURE_CUBE_MAP},
    {GL_TEXTURE_CUBE_MAP_POSITIVE_X, TextureTarget::TEXTURE_CUBE_MAP},
    {GL_TEXTURE_CUBE_MAP_NEGATIVE_X, TextureTarget::TEXTURE_CUBE_MAP},
    {GL_TEXTURE_CUBE_MAP_POSITIVE_Y, TextureTarget::TEXTURE_CUBE_MAP},
    {GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, TextureTarget::TEXTURE_CUBE_MAP},
    {GL_TEXTURE_CUBE_MAP_POSITIVE_Z, TextureTarget::TEXTURE_CUBE_MAP},
    {GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, TextureTarget::TEXTURE_CUBE_MAP},
    {GL_PROXY_TEXTURE_CUBE_MAP, TextureTarget::PROXY_TEXTURE_CUBE_MAP},
    {GL_TEXTURE_1D_ARRAY, TextureTarget::TEXTURE_1D_ARRAY},
    {GL_PROXY_TEXTURE_1D_ARRAY, TextureTarget::PROXY_TEXTURE_1D_ARRAY},
    {GL_TEXTURE_2D_ARRAY, TextureTarget::TEXTURE_2D_ARRAY},
    {GL_PROXY_TEXTURE_2D_ARRAY, TextureTarget::PROXY_TEXTURE_2D_ARRAY},
    {GL_TEXTURE_BUFFER, TextureTarget::TEXTURE_BUFFER},
    {GL_TEXTURE_CUBE_MAP_ARRAY, TextureTarget::TEXTURE_CUBE_MAP_ARRAY},
    {GL_PROXY_TEXTURE_CUBE_MAP_ARRAY, TextureTarget::PROXY_TEXTURE_CUBE_MAP_ARRAY},
    {GL_TEXTURE_2D_MULTISAMPLE, TextureTarget::TEXTURE_2D_MULTISAMPLE},
    {GL_PROXY_TEXTURE_2D_MULTISAMPLE, TextureTarget::PROXY_TEXTURE_2D_MULTISAMPLE},
    {GL_TEXTURE_2D_MULTISAMPLE_ARRAY, TextureTarget::TEXTURE_2D_MULTISAMPLE_ARRAY},
    {GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY, TextureTarget::PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY},
};

static constexpr size_t kTexTargetEntryCount = sizeof(kGLEnumToTextureTarget) / sizeof(kGLEnumToTextureTarget[0]);

TextureTarget ConvertGLEnumToTextureTarget(GLenum target) {
    // Switch-based direct lookup — faster than binary search for this hot path
    switch (target) {
    case GL_TEXTURE_1D:                   return TextureTarget::TEXTURE_1D;
    case GL_PROXY_TEXTURE_1D:             return TextureTarget::PROXY_TEXTURE_1D;
    case GL_TEXTURE_2D:                   return TextureTarget::TEXTURE_2D;
    case GL_PROXY_TEXTURE_2D:             return TextureTarget::PROXY_TEXTURE_2D;
    case GL_TEXTURE_3D:                   return TextureTarget::TEXTURE_3D;
    case GL_PROXY_TEXTURE_3D:             return TextureTarget::PROXY_TEXTURE_3D;
    case GL_TEXTURE_RECTANGLE:            return TextureTarget::TEXTURE_RECTANGLE;
    case GL_PROXY_TEXTURE_RECTANGLE:      return TextureTarget::PROXY_TEXTURE_RECTANGLE;
    case GL_TEXTURE_CUBE_MAP:             return TextureTarget::TEXTURE_CUBE_MAP;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:  return TextureTarget::TEXTURE_CUBE_MAP;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:  return TextureTarget::TEXTURE_CUBE_MAP;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:  return TextureTarget::TEXTURE_CUBE_MAP;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:  return TextureTarget::TEXTURE_CUBE_MAP;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:  return TextureTarget::TEXTURE_CUBE_MAP;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:  return TextureTarget::TEXTURE_CUBE_MAP;
    case GL_PROXY_TEXTURE_CUBE_MAP:       return TextureTarget::PROXY_TEXTURE_CUBE_MAP;
    case GL_TEXTURE_1D_ARRAY:             return TextureTarget::TEXTURE_1D_ARRAY;
    case GL_PROXY_TEXTURE_1D_ARRAY:       return TextureTarget::PROXY_TEXTURE_1D_ARRAY;
    case GL_TEXTURE_2D_ARRAY:             return TextureTarget::TEXTURE_2D_ARRAY;
    case GL_PROXY_TEXTURE_2D_ARRAY:       return TextureTarget::PROXY_TEXTURE_2D_ARRAY;
    case GL_TEXTURE_BUFFER:               return TextureTarget::TEXTURE_BUFFER;
    case GL_TEXTURE_CUBE_MAP_ARRAY:       return TextureTarget::TEXTURE_CUBE_MAP_ARRAY;
    case GL_PROXY_TEXTURE_CUBE_MAP_ARRAY: return TextureTarget::PROXY_TEXTURE_CUBE_MAP_ARRAY;
    case GL_TEXTURE_2D_MULTISAMPLE:       return TextureTarget::TEXTURE_2D_MULTISAMPLE;
    case GL_PROXY_TEXTURE_2D_MULTISAMPLE: return TextureTarget::PROXY_TEXTURE_2D_MULTISAMPLE;
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:         return TextureTarget::TEXTURE_2D_MULTISAMPLE_ARRAY;
    case GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY:   return TextureTarget::PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY;
    default:                              return TextureTarget::UNKNWON;
    }
}

// ============================================================================
// Texture map management (tracking bound texture objects)
// ============================================================================

const int MAX_TEXTURE_IMAGE_UNITS = 32;

class TextureBindingSlot {
public:
    using TargetEnum = TextureTarget;

    TextureBindingSlot() : m_target((TargetEnum)0), m_boundObject(nullptr) {}

    explicit TextureBindingSlot(TargetEnum target) : m_target(target), m_boundObject(nullptr) {}

    void Bind(TextureObject* object) {
        // No-op: already bound to the same object (avoids redundant vector work).
        if (m_boundObject == object) return;
        // Remove this slot from the old texture's reverse mapping.
        if (m_boundObject) {
            auto& slots = m_boundObject->binding_slots;
            uintptr_t self = reinterpret_cast<uintptr_t>(this);
            for (size_t i = 0; i < slots.size(); ++i) {
                if (slots[i] == self) {
                    slots[i] = slots.back(); // swap-with-last (order doesn't matter)
                    slots.pop_back();
                    break;
                }
            }
        }
        m_boundObject = object;
        if (object) {
            object->binding_slots.push_back(reinterpret_cast<uintptr_t>(this));
        }
    }

    TextureObject* GetBoundObject() const { return m_boundObject; }

    TargetEnum GetTarget() const { return m_target; }

private:
    TargetEnum m_target;
    TextureObject* m_boundObject;
};

class TextureUnit {
public:
    TextureBindingSlot& GetBindingSlot(TextureBindingSlot::TargetEnum target) { return m_slots[(int)target]; }

private:
    std::array<TextureBindingSlot, (int)TextureTarget::TEXTURES_COUNT> m_slots;
};

static std::vector<TextureObject*> BufferObjectsVec;
static std::array<TextureUnit, MAX_TEXTURE_IMAGE_UNITS> TextureUnits;
static int CurrentTextureUnitIndex = 0;

void InitTextureMap(size_t expectedSize) {
    BufferObjectsVec.reserve(expectedSize);
}

TextureObject* GetOrCreateTextureObject(GLuint index) {
    if (index >= BufferObjectsVec.size()) {
        BufferObjectsVec.resize(index + 1, nullptr);
    }

    auto& obj = BufferObjectsVec[index];
    if (!obj) {
        obj = new TextureObject();
        obj->texture = index;
    }
    return obj;
}

void ActivateTextureUnit(int unit) {
    if (unit < 0 || unit >= MAX_TEXTURE_IMAGE_UNITS) {
        LOG_E("Invalid texture unit: %d", unit);
        return;
    }
    CurrentTextureUnitIndex = unit;
}

int GetCurrentTextureUnitIndex() {
    return CurrentTextureUnitIndex;
}

TextureUnit& GetTextureUnit(int unit) {
    if (unit < 0 || unit >= MAX_TEXTURE_IMAGE_UNITS) {
        LOG_E("Invalid texture unit: %d", unit);
        return TextureUnits[0];
    }
    return TextureUnits[unit];
}

void MarkTextureObjectForDeletion(unsigned texture) {
    if (texture >= BufferObjectsVec.size() || !BufferObjectsVec[texture]) {
        LOG_D("Texture %u not found in BufferObjectsVec!", texture);
        return;
    }

    auto textureObject = BufferObjectsVec[texture];

    // Copy the binding_slots before iteration: Bind(nullptr) calls
    // m_boundObject->binding_slots.erase() which would invalidate
    // the iterator if we iterated the original set directly.
    auto slots = textureObject->binding_slots;
    for (auto slotPtr : slots) {
        auto* slot = reinterpret_cast<TextureBindingSlot*>(slotPtr);
        slot->Bind(nullptr);
    }
    textureObject->binding_slots.clear();

    BufferObjectsVec[texture] = nullptr;
    delete textureObject;
}

TextureObject* mgGetTexObjectByTarget(GLenum target) {
    return GetTextureUnit(GetCurrentTextureUnitIndex())
        .GetBindingSlot(ConvertGLEnumToTextureTarget(target))
        .GetBoundObject();
}

TextureObject* mgGetTexObjectByID(unsigned texture) {
    if (texture >= BufferObjectsVec.size() || !BufferObjectsVec[texture]) {
        LOG_E("Texture %u not found in BufferObjectsVec!", texture);
        return nullptr;
    }
    return BufferObjectsVec[texture];
}

// ============================================================================
// Internal format conversion helper
// ============================================================================

// ============================================================================
// Lookup table for GL_RED type→internalformat mapping.
// Sorted by GLenum type for fast binary search.
// ============================================================================
struct RedTypeMapping {
    GLenum type;
    GLenum internalformat;
    GLenum format;
};
static const RedTypeMapping kRedTypeMappings[] = {
    {GL_UNSIGNED_BYTE, GL_R8,        GL_RED},
    {GL_BYTE,          GL_R8_SNORM,  GL_RED},
    {GL_HALF_FLOAT,    GL_R16F,      GL_RED},
    {GL_FLOAT,         GL_R32F,      GL_RED},
};
static constexpr size_t kRedTypeMappingCount = sizeof(kRedTypeMappings) / sizeof(kRedTypeMappings[0]);

// ============================================================================
// Inline mapping for various internal formats to format and type
// Most common formats (RGBA8, RGBA, RGBA16F, R8, RGBA32F) are handled first
// with early return for maximum performance on the hot path.
// ============================================================================
void internal_convert(GLenum* internal_format, GLenum* type, GLenum* format) {
    // NOTE: GL_BGRA is intentionally NOT rewritten to GL_RGBA here.
    // GLES 3.2 rejects GL_BGRA, so callers that hand pixel data to GLES
    // (glTexImage2D / glTexSubImage2D / glTexSubImage3D / glReadPixels /
    // glGetTexImage) must run the CPU-side swizzle from
    // pixel.h (get_rgba8_unpack_swizzle / get_rgba_pack_swizzle) and only
    // then flip the format enum to GL_RGBA before invoking GLES.

    // Fast path: most common formats first
    switch (*internal_format) {
    // --- Most common formats (hot path) ---
    case GL_RGBA8:
    case GL_RGBA:
        if (type) *type = GL_UNSIGNED_BYTE;
        // Preserve GL_BGRA if the caller explicitly requested it; the caller
        // is responsible for running the CPU swizzle and flipping the enum
        // to GL_RGBA before invoking GLES.
        if (format && *format != GL_BGRA) *format = GL_RGBA;
        return;
    case GL_RGBA16F:
        if (type) *type = GL_HALF_FLOAT;
        return;
    case GL_R8:
        if (format) *format = GL_RED;
        if (type) *type = GL_UNSIGNED_BYTE;
        return;
    case GL_RGBA32F:
    case GL_RGB32F:
        if (type) *type = GL_FLOAT;
        return;

    // --- GL_RED: lookup-table dispatch (replaces nested switch) ---
    case GL_RED:
        if (type) {
            GLenum t = *type;
            // Linear search is faster than binary search for 4 entries
            for (size_t i = 0; i < kRedTypeMappingCount; ++i) {
                if (kRedTypeMappings[i].type == t) {
                    *internal_format = kRedTypeMappings[i].internalformat;
                    if (format) *format = kRedTypeMappings[i].format;
                    return;
                }
            }
            LOG_E("Unsupported type for GL_RED: %s", glEnumToString(*type));
            if (type) *type = GL_UNSIGNED_BYTE;
            *internal_format = GL_R8;
            if (format) *format = GL_RED;
        }
        return;

    // --- Less common formats ---
    case GL_DEPTH_COMPONENT16:
        if (type) *type = GL_UNSIGNED_SHORT;
        break;
    case GL_DEPTH_COMPONENT24:
        if (type) *type = GL_UNSIGNED_INT;
        break;
    case GL_DEPTH_COMPONENT32:
        *internal_format = GL_DEPTH_COMPONENT;
        if (type) *type = GL_UNSIGNED_INT;
        break;
    case GL_DEPTH_COMPONENT32F:
        if (type) *type = GL_FLOAT;
        break;
    case GL_DEPTH_COMPONENT:
        LOG_D("Find GL_DEPTH_COMPONENT: internalFormat: %s, format: %s, type: %s", glEnumToString(*internal_format),
              glEnumToString(*format), glEnumToString(*type));
        if (type) {
            *internal_format = GL_DEPTH_COMPONENT;
            *type = GL_UNSIGNED_INT;
        }
        break;
    case GL_DEPTH_STENCIL:
        *internal_format = GL_DEPTH32F_STENCIL8;
        if (type) *type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
        break;
    case GL_RGB10_A2:
        if (type) *type = GL_UNSIGNED_INT_2_10_10_10_REV;
        break;
    case GL_RGB5_A1:
        if (type) *type = GL_UNSIGNED_SHORT_5_5_5_1;
        break;
    case GL_COMPRESSED_RED_RGTC1:
    case GL_COMPRESSED_RG_RGTC2:
        LOG_E("GL_COMPRESSED_RED_RGTC1 or GL_COMPRESSED_RG_RGTC2 is not supported!");
        break;
    case GL_SRGB8:
        if (type) *type = GL_UNSIGNED_BYTE;
        break;
    case GL_RGB9_E5:
        if (type) *type = GL_UNSIGNED_INT_5_9_9_9_REV;
        break;
    case GL_R11F_G11F_B10F:
        if (type) *type = GL_UNSIGNED_INT_10F_11F_11F_REV;
        if (format) *format = GL_RGB;
        break;
    case GL_RGBA32UI:
    case GL_RGB32UI:
        if (type) *type = GL_UNSIGNED_INT;
        break;
    case GL_RGBA32I:
    case GL_RGB32I:
        if (type) *type = GL_INT;
        break;
    case GL_RGBA16: {
        if (g_gles_caps.GL_EXT_texture_norm16) {
            if (type) *type = GL_UNSIGNED_SHORT;
        } else {
            *internal_format = GL_RGBA16F;
            if (type) *type = GL_FLOAT;
        }
        break;
    }
    case GL_R16:
        *internal_format = GL_R16F;
        if (type) *type = GL_FLOAT;
        break;
    case GL_RGB16:
        *internal_format = GL_RGB16F;
        if (type) *type = GL_HALF_FLOAT;
        if (format) *format = GL_RGB;
        break;
    case GL_RGB16F:
        if (type) *type = GL_HALF_FLOAT;
        if (format) *format = GL_RGB;
        break;
    case GL_RG16:
        *internal_format = GL_RG16F;
        if (type) *type = GL_HALF_FLOAT;
        if (format) *format = GL_RG;
        break;
    case GL_R8_SNORM:
        if (format) *format = GL_RED;
        if (type) *type = GL_BYTE;
        break;
    case GL_R16F:
        if (format) *format = GL_RED;
        if (type) *type = GL_HALF_FLOAT;
        break;
    case GL_R8UI:
        if (format) *format = GL_RED_INTEGER;
        if (type) *type = GL_UNSIGNED_BYTE;
        break;
    case GL_R8I:
        if (format) *format = GL_RED_INTEGER;
        if (type) *type = GL_BYTE;
        break;
    case GL_R16UI:
        if (format) *format = GL_RED_INTEGER;
        if (type) *type = GL_UNSIGNED_SHORT;
        break;
    case GL_R16I:
        if (format) *format = GL_RED_INTEGER;
        if (type) *type = GL_SHORT;
        break;
    case GL_R32UI:
        if (format) *format = GL_RED_INTEGER;
        if (type) *type = GL_UNSIGNED_INT;
        break;
    case GL_R32I:
        if (format) *format = GL_RED_INTEGER;
        if (type) *type = GL_INT;
        break;
    case GL_RG8:
        if (format) *format = GL_RG;
        if (type) *type = GL_UNSIGNED_BYTE;
        break;
    case GL_RG8_SNORM:
        if (format) *format = GL_RG;
        if (type) *type = GL_BYTE;
        break;
    case GL_RG16F:
        if (format) *format = GL_RG;
        if (type) *type = GL_HALF_FLOAT;
        break;
    case GL_RG32F:
        if (format) *format = GL_RG;
        if (type) *type = GL_FLOAT;
        break;
    case GL_RG8UI:
        if (format) *format = GL_RG_INTEGER;
        if (type) *type = GL_UNSIGNED_BYTE;
        break;
    case GL_RG8I:
        if (format) *format = GL_RG_INTEGER;
        if (type) *type = GL_BYTE;
        break;
    case GL_RG16UI:
        if (format) *format = GL_RG_INTEGER;
        if (type) *type = GL_UNSIGNED_SHORT;
        break;
    case GL_RG16I:
        if (format) *format = GL_RG_INTEGER;
        if (type) *type = GL_SHORT;
        break;
    case GL_RG32UI:
        if (format) *format = GL_RG_INTEGER;
        if (type) *type = GL_UNSIGNED_INT;
        break;
    case GL_RG32I:
        if (format) *format = GL_RG_INTEGER;
        if (type) *type = GL_INT;
        break;
    case GL_R32F:
        if (format) *format = GL_RED;
        if (type) *type = GL_FLOAT;
        break;
    case GL_RGBA8_SNORM:
        if (format) *format = GL_RGBA;
        if (type) *type = GL_BYTE;
        break;
    default:
        // fallback handling for GL_RGB8, GL_RGBA16_SNORM etc.
        if (*internal_format == GL_RGB8) {
            if (type && *type != GL_UNSIGNED_BYTE) *type = GL_UNSIGNED_BYTE;
            if (format) *format = GL_RGB;
        } else if (*internal_format == GL_RGBA16_SNORM) {
            if (type && *type != GL_SHORT) *type = GL_SHORT;
        }
        break;
    }
}

// ============================================================================
// Internal helpers: depth format detection, binding query
// ============================================================================

static int is_depth_format(GLenum format) {
    switch (format) {
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
        return 1;
    default:
        return 0;
    }
}

static GLenum get_binding_for_target(GLenum target) {
    switch (target) {
    case GL_TEXTURE_2D:
        return GL_TEXTURE_BINDING_2D;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return GL_TEXTURE_BINDING_CUBE_MAP;
    default:
        return 0;
    }
}

// ============================================================================
// Convenience macro: get texture object from bound target
// ============================================================================

#define GET_TEXTURE_OBJECT(target)                                                                                     \
    unsigned __currentUnitIndex = GetCurrentTextureUnitIndex();                                                        \
    auto& __currentUnit = GetTextureUnit(__currentUnitIndex);                                                          \
    auto targetR = ConvertGLEnumToTextureTarget(target);                                                               \
    if (targetR == TextureTarget::UNKNWON) {                                                                           \
        LOG_E("%s: Unknown texture target: %s", __func__, glEnumToString(target))                                      \
        return;                                                                                                        \
    }                                                                                                                  \
    auto& __bindingSlot = __currentUnit.GetBindingSlot(targetR);                                                       \
    auto tex = __bindingSlot.GetBoundObject()

// ============================================================================
// BGRA / packed-type CPU swizzle helpers for the upload (unpack) path.
//
// GLES 3.2 only accepts GL_RGBA + GL_UNSIGNED_BYTE for the RGBA8 upload
// case, but desktop GL callers frequently pass GL_BGRA and / or packed types
// such as GL_UNSIGNED_INT_8_8_8_8(_REV). swizzle_pixels_for_unpack() takes
// the user-supplied (format, type, pixels) triple and returns a pointer that
// is safe to feed straight to GLES as (GL_RGBA, GL_UNSIGNED_BYTE).
//
// When no swizzle is needed the original `pixels` pointer is returned and no
// allocation is performed. When a swizzle is needed a heap buffer is
// allocated with malloc() and the caller must free() it.
//
// Pixel-store awareness (GL_UNPACK_ROW_LENGTH / SKIP_PIXELS / SKIP_ROWS /
// ALIGNMENT / IMAGE_HEIGHT / SKIP_IMAGES): when the caller has set non-default
// unpack layout parameters, the source bytes are *not* tightly packed. We
// read GLState.texture.unpack* to compute the real source row stride and skip
// offset, copy the actual source rows into a tightly-packed output buffer,
// swizzle the tight buffer, and then upload with a temporarily-reset GLES
// unpack state (see ScopedUnpackTight below) so GLES reads the tight buffer
// correctly. This is what Xaero's World Map and similar sub-region updaters
// rely on.
// ============================================================================

// RAII helper: temporarily reset GLES's GL_UNPACK_* state to the tightly
// packed default (row length 0, all skips 0, alignment 1, image height 0),
// used right after swizzle_pixels_for_unpack() returns a freshly-allocated
// tight buffer. The previous GLES state is restored on destruction so the
// caller's GL_UNPACK_* settings are preserved across the call.
// GLState.texture.* caches are kept in sync too.
struct ScopedUnpackTight {
    GLint prevRowLength;
    GLint prevSkipPixels;
    GLint prevSkipRows;
    GLint prevAlignment;
    GLint prevImageHeight;
    GLint prevSkipImages;

    ScopedUnpackTight() {
        // Snapshot GLES state and CPU cache together.
        GLES.glGetIntegerv(GL_UNPACK_ROW_LENGTH, &prevRowLength);
        GLES.glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &prevSkipPixels);
        GLES.glGetIntegerv(GL_UNPACK_SKIP_ROWS, &prevSkipRows);
        GLES.glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlignment);
        GLES.glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &prevImageHeight);
        GLES.glGetIntegerv(GL_UNPACK_SKIP_IMAGES, &prevSkipImages);

        GLES.glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        GLES.glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        GLES.glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        GLES.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        GLES.glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
        GLES.glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
    }
    ~ScopedUnpackTight() {
        GLES.glPixelStorei(GL_UNPACK_ROW_LENGTH, prevRowLength);
        GLES.glPixelStorei(GL_UNPACK_SKIP_PIXELS, prevSkipPixels);
        GLES.glPixelStorei(GL_UNPACK_SKIP_ROWS, prevSkipRows);
        GLES.glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlignment);
        GLES.glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, prevImageHeight);
        GLES.glPixelStorei(GL_UNPACK_SKIP_IMAGES, prevSkipImages);
        // CPU cache is already current - the GLES.glPixelStorei() calls above
        // were forwarded through the GLES function pointer table which doesn't
        // touch our cache; that's intentional because the cache is the
        // caller-visible state and we want it preserved too.
    }
};

// RAII helper: temporarily reset GLES's GL_PACK_* state to the tightly
// packed default, mirror of ScopedUnpackTight but for the pack (readback)
// path. Used by glReadPixels when a CPU swizzle is required so that GLES
// writes a tightly packed RGBA buffer into a temp allocation; the buffer is
// then CPU-swizzled and re-laid-out into the caller's destination according
// to the caller's GL_PACK_* settings.
struct ScopedPackTight {
    GLint prevRowLength;
    GLint prevSkipPixels;
    GLint prevSkipRows;
    GLint prevAlignment;

    ScopedPackTight() {
        GLES.glGetIntegerv(GL_PACK_ROW_LENGTH, &prevRowLength);
        GLES.glGetIntegerv(GL_PACK_SKIP_PIXELS, &prevSkipPixels);
        GLES.glGetIntegerv(GL_PACK_SKIP_ROWS, &prevSkipRows);
        GLES.glGetIntegerv(GL_PACK_ALIGNMENT, &prevAlignment);

        GLES.glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        GLES.glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        GLES.glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        GLES.glPixelStorei(GL_PACK_ALIGNMENT, 1);
    }
    ~ScopedPackTight() {
        GLES.glPixelStorei(GL_PACK_ROW_LENGTH, prevRowLength);
        GLES.glPixelStorei(GL_PACK_SKIP_PIXELS, prevSkipPixels);
        GLES.glPixelStorei(GL_PACK_SKIP_ROWS, prevSkipRows);
        GLES.glPixelStorei(GL_PACK_ALIGNMENT, prevAlignment);
    }
};

static const void* swizzle_pixels_for_unpack(GLenum internalFormat, GLenum& format, GLenum& type,
                                              const void* pixels, GLsizei width, GLsizei height, GLsizei depth) {
    if (!pixels) return nullptr;

    // Only handle the RGBA8 family for now; other internal formats are passed
    // through to GLES unchanged (the format enum may still be GL_BGRA, in
    // which case GLES will report an error - matching previous behaviour).
    if (internalFormat != GL_RGBA8 && internalFormat != GL_RGBA) return pixels;

    unsigned char swizzle[4];
    if (!get_rgba8_unpack_swizzle(format, type, swizzle)) {
        // No swizzle needed. Make sure the format/type pair is GLES-friendly:
        // GL_UNSIGNED_INT_8_8_8_8(_REV) are also unsupported by GLES even
        // when paired with GL_RGBA, so normalize them to GL_UNSIGNED_BYTE
        // (the byte layout is already RGBA on little-endian for the _REV
        // variant; the non-REV variant would have been swizzled above).
        if (type == GL_UNSIGNED_INT_8_8_8_8 || type == GL_UNSIGNED_INT_8_8_8_8_REV) {
            type = GL_UNSIGNED_BYTE;
        }
        if (format == GL_BGRA) format = GL_RGBA;
        return pixels;
    }

    // Read the caller's GL_UNPACK_* layout parameters so we can locate the
    // actual source rows correctly. The cache mirrors what glPixelStorei()
    // forwarded to GLES.
    const GLint rowLength    = GLState.texture.unpackRowLength;
    const GLint alignment    = GLState.texture.unpackAlignment;
    const GLint skipPixels   = GLState.texture.unpackSkipPixels;
    const GLint skipRows     = GLState.texture.unpackSkipRows;
    const GLint imageHeight  = GLState.texture.unpackImageHeight;
    const GLint skipImages   = GLState.texture.unpackSkipImages;

    constexpr GLuint bytesPerPixel = 4;
    const GLuint effectiveRow = (rowLength > 0) ? static_cast<GLuint>(rowLength) : static_cast<GLuint>(width);
    const GLuint effectiveImageHeight = (imageHeight > 0) ? static_cast<GLuint>(imageHeight) : static_cast<GLuint>(height);
    // Row stride in bytes, honoring GL_UNPACK_ALIGNMENT.
    const GLuint srcRowStride   = static_cast<GLuint>(widthalign(effectiveRow * bytesPerPixel, alignment));
    const GLuint srcImageStride = srcRowStride * effectiveImageHeight;

    // Offset in bytes from `pixels` to the first byte of the first source row.
    const size_t srcSkipOffset =
        static_cast<size_t>(skipImages) * srcImageStride +
        static_cast<size_t>(skipRows) * srcRowStride +
        static_cast<size_t>(skipPixels) * bytesPerPixel;

    const GLuint pixelCount = static_cast<GLuint>(width) * static_cast<GLuint>(height) * static_cast<GLuint>(depth);
    const size_t byteCount = static_cast<size_t>(pixelCount) * bytesPerPixel;
    void* out = malloc(byteCount);
    if (!out) {
        LOG_E("swizzle_pixels_for_unpack: allocation of %zu bytes failed", byteCount)
        return pixels;
    }

    // Copy from source (with row stride + skip) into the tightly-packed `out`.
    const unsigned char* src = static_cast<const unsigned char*>(pixels) + srcSkipOffset;
    unsigned char* dst = static_cast<unsigned char*>(out);
    const size_t srcRowBytes = static_cast<size_t>(width) * bytesPerPixel;
    if (srcRowStride == static_cast<GLuint>(width) * bytesPerPixel && depth == 1) {
        // Source is already tightly packed - single memcpy is enough.
        memcpy(dst, src, byteCount);
    } else {
        // Per-row copy (handles non-default row length / alignment).
        for (GLsizei z = 0; z < depth; ++z) {
            const unsigned char* srcRow = src + static_cast<size_t>(z) * srcImageStride;
            for (GLsizei y = 0; y < height; ++y) {
                memcpy(dst, srcRow, srcRowBytes);
                srcRow += srcRowStride;
                dst += srcRowBytes;
            }
        }
    }

    ProcessColorSwizzle(out, pixelCount, swizzle, 4);

    // GLES now sees a clean GL_RGBA + GL_UNSIGNED_BYTE upload. The caller is
    // responsible for wrapping the GLES call in ScopedUnpackTight() so that
    // GLES also interprets `out` as tightly packed.
    format = GL_RGBA;
    type = GL_UNSIGNED_BYTE;
    return out;
}

void glBindTexture(GLenum target, GLuint texture) {
    LOG()
    LOG_D("glBindTexture(%s, %d)", glEnumToString(target), texture)
    INIT_CHECK_GL_ERROR

    const int currentUnitIndex = GetCurrentTextureUnitIndex();

    GLES.glBindTexture(target, texture);
    CHECK_GL_ERROR_NO_INIT

    // Track GL_TEXTURE_2D binding per-unit to avoid glGetIntegerv GPU queries
    if (target == GL_TEXTURE_2D) {
        g_tracked_tex2d_binding[currentUnitIndex] = texture;
    }

    auto& currentUnit = GetTextureUnit(currentUnitIndex);
    auto targetR = ConvertGLEnumToTextureTarget(target);
    if (targetR == TextureTarget::UNKNWON) {
        LOG_E("glBindTexture: Unknown texture target: %s", glEnumToString(target));
        return;
    }
    auto& bindingSlot = currentUnit.GetBindingSlot(targetR);

    // Unbind: skip TextureObject creation for texture 0
    if (texture == 0) [[unlikely]] {
        bindingSlot.Bind(nullptr);
        return;
    }

    auto textureObject = GetOrCreateTextureObject(texture);
    if (!textureObject) {
        LOG_W("glBindTexture: Failed to get or create texture object for ID %d, it may be not tracked", texture);
        return;
    }
    bindingSlot.Bind(textureObject);
    textureObject->target = targetR;
}

void glDeleteTextures(GLsizei n, const GLuint* textures) {
    LOG()
    INIT_CHECK_GL_ERROR
    GLES.glDeleteTextures(n, textures);
    CHECK_GL_ERROR_NO_INIT

    for (GLsizei i = 0; i < n; ++i) {
        MarkTextureObjectForDeletion(textures[i]);
        // Invalidate CPU-side texture binding tracking
        for (int unit = 0; unit < MAX_TEXTURE_IMAGE_UNITS; ++unit) {
            if (g_tracked_tex2d_binding[unit] == textures[i]) {
                g_tracked_tex2d_binding[unit] = 0;
            }
        }
    }
}

void glActiveTexture(GLenum texture) {
    LOG()
    LOG_D("glActiveTexture, texture = %s", glEnumToString(texture))
    if (texture < GL_TEXTURE0 || texture >= GL_TEXTURE0 + MAX_TEXTURE_IMAGE_UNITS) {
        LOG_E("Invalid texture enum: %s", glEnumToString(texture))
        return;
    }

    set_gl_state_current_tex_unit(texture - GL_TEXTURE0);
    GLES.glActiveTexture(texture);
    ActivateTextureUnit(texture - GL_TEXTURE0);
    CHECK_GL_ERROR
}

// ============================================================================
// Native texture image specification (ES 3.2)
// ============================================================================

// --- glTexImage2D (native, with format conversion) ---
void glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border,
                  GLenum format, GLenum type, const GLvoid* pixels) {
    LOG()

    LOG_D("mg_glTexImage2D,target: %s,level: %d,internalFormat: %s->%s,width: "
          "%d,height: %d,border: %d,format: %s,type: %s, pixels: 0x%x",
          glEnumToString(target), level, glEnumToString(internalFormat), glEnumToString(internalFormat), width, height,
          border, glEnumToString(format), glEnumToString(type), pixels)
    GLenum internalFormat_mut = (GLenum)internalFormat;
    internal_convert(&internalFormat_mut, &type, &format);
    internalFormat = (GLint)internalFormat_mut;

    LOG_D("GLES.glTexImage2D,target: %s,level: %d,internalFormat: %s->%s,width: "
          "%d,height: %d,border: %d,format: %s,type: %s, pixels: 0x%x",
          glEnumToString(target), level, glEnumToString(internalFormat), glEnumToString(internalFormat), width, height,
          border, glEnumToString(format), glEnumToString(type), pixels)
    GLenum rtarget = map_tex_target(target);
    if (rtarget == GL_PROXY_TEXTURE_2D) {
        static int maxTexSize = -1;
        if (maxTexSize < 0) GLES.glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
        set_gl_state_proxy_width(((width << level) > maxTexSize) ? 0 : width);
        set_gl_state_proxy_height(((height << level) > maxTexSize) ? 0 : height);
        set_gl_state_proxy_intformat(internalFormat);
        return;
    }

    GET_TEXTURE_OBJECT(target);
    tex->target = ConvertGLEnumToTextureTarget(target);
    tex->internal_format = internalFormat;
    tex->width = width;
    tex->height = height;
    tex->depth = 1;
    tex->swizzle_param[0] = GL_RED;
    tex->swizzle_param[1] = GL_GREEN;
    tex->swizzle_param[2] = GL_BLUE;
    tex->swizzle_param[3] = GL_ALPHA;

    // CPU-side BGRA/packed-type swizzle so GLES sees GL_RGBA + GL_UNSIGNED_BYTE.
    // This replaces the previous ≤128x128 GLES-texture-swizzle hack that
    // corrupted the texture's swizzle state and only worked for tiny textures.
    const void* uploadPixels = swizzle_pixels_for_unpack((GLenum)internalFormat, format, type, pixels, width, height, 1);

    tex->format = format;

    // If swizzle_pixels_for_unpack() allocated a tight buffer, we must also
    // tell GLES to read it as tight (the caller's GL_UNPACK_* settings still
    // apply and would otherwise misread the freshly-swizzled buffer).
    if (uploadPixels != pixels && uploadPixels != nullptr) {
        ScopedUnpackTight tightGuard;
        GLES.glTexImage2D(target, level, internalFormat, width, height, border, format, type, uploadPixels);
    } else {
        GLES.glTexImage2D(target, level, internalFormat, width, height, border, format, type, uploadPixels);
    }

    if (uploadPixels != pixels && uploadPixels != nullptr) free(const_cast<void*>(uploadPixels));

    CHECK_GL_ERROR
}

// --- glTexImage3D (native, ES 3.2 supports it) ---
void glTexImage3D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth,
                  GLint border, GLenum format, GLenum type, const GLvoid* pixels) {
    LOG()
    LOG_D("glTexImage3D, target: 0x%x, level: %d, internalFormat: 0x%x, width: "
          "0x%x, height: %d, depth: %d, border: %d, format: 0x%x, type: %d",
          target, level, internalFormat, width, height, depth, border, format, type)

    GLenum internalFormat_mut = (GLenum)internalFormat;
    internal_convert(&internalFormat_mut, &type, &format);
    internalFormat = (GLint)internalFormat_mut;
    GLenum rtarget = map_tex_target(target);
    if (rtarget == GL_PROXY_TEXTURE_3D) {
        static int maxTexSize3D = -1;
        if (maxTexSize3D < 0) GLES.glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize3D);
        set_gl_state_proxy_width(((width << level) > maxTexSize3D) ? 0 : width);
        set_gl_state_proxy_height(((height << level) > maxTexSize3D) ? 0 : height);
        set_gl_state_proxy_intformat(internalFormat);
        return;
    }

    // CPU-side BGRA/packed-type swizzle so GLES sees GL_RGBA + GL_UNSIGNED_BYTE.
    const void* uploadPixels = swizzle_pixels_for_unpack((GLenum)internalFormat, format, type, pixels, width, height, depth);

    if (uploadPixels != pixels && uploadPixels != nullptr) {
        ScopedUnpackTight tightGuard;
        GLES.glTexImage3D(target, level, internalFormat, width, height, depth, border, format, type, uploadPixels);
    } else {
        GLES.glTexImage3D(target, level, internalFormat, width, height, depth, border, format, type, uploadPixels);
    }

    if (uploadPixels != pixels && uploadPixels != nullptr) free(const_cast<void*>(uploadPixels));

    GET_TEXTURE_OBJECT(target);
    tex->target = ConvertGLEnumToTextureTarget(target);
    tex->internal_format = internalFormat;
    tex->width = width;
    tex->height = height;
    tex->depth = depth;
    tex->swizzle_param[0] = GL_RED;
    tex->swizzle_param[1] = GL_GREEN;
    tex->swizzle_param[2] = GL_BLUE;
    tex->swizzle_param[3] = GL_ALPHA;

    CHECK_GL_ERROR
}

// --- glTexSubImage2D (native, with format conversion) ---
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                     GLenum format, GLenum type, const void* pixels) {
    LOG()

    LOG_D("glTexSubImage2D, target = %s, level = %d, xoffset = %d, yoffset = %d, "
          "width = %d, height = %d, format = %s, type = %s, pixels = 0x%x",
          glEnumToString(target), level, xoffset, yoffset, width, height, glEnumToString(format), glEnumToString(type),
          pixels)

    // Look up the bound texture's internal format so the BGRA/packed-type
    // swizzle is only applied to RGBA8-class textures.
    GLenum texInternalFormat = GL_RGBA8;
    {
        GET_TEXTURE_OBJECT(target);
        if (tex) texInternalFormat = tex->internal_format;
    }

    // CPU-side BGRA/packed-type swizzle. Replaces the previous code that
    // permanently corrupted the texture's GL_TEXTURE_SWIZZLE_R/B state and
    // only handled the BGRA + GL_UNSIGNED_INT_8_8_8_8(_REV) case.
    const void* uploadPixels = swizzle_pixels_for_unpack(texInternalFormat, format, type, pixels, width, height, 1);

    if (uploadPixels != pixels && uploadPixels != nullptr) {
        ScopedUnpackTight tightGuard;
        GLES.glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, uploadPixels);
    } else {
        GLES.glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, uploadPixels);
    }

    if (uploadPixels != pixels && uploadPixels != nullptr) free(const_cast<void*>(uploadPixels));

    CHECK_GL_ERROR
}

// --- glTexSubImage3D (native) ---
void glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
                     GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    LOG()
    LOG_D("glTexSubImage3D, target: %s, level: %d, xoffset: %d, yoffset: %d, zoffset: %d, "
          "width: %d, height: %d, depth: %d, format: %s, type: %s",
          glEnumToString(target), level, xoffset, yoffset, zoffset, width, height, depth, glEnumToString(format),
          glEnumToString(type))

    GLenum texInternalFormat = GL_RGBA8;
    {
        GET_TEXTURE_OBJECT(target);
        if (tex) texInternalFormat = tex->internal_format;
    }

    const void* uploadPixels = swizzle_pixels_for_unpack(texInternalFormat, format, type, pixels, width, height, depth);

    if (uploadPixels != pixels && uploadPixels != nullptr) {
        ScopedUnpackTight tightGuard;
        GLES.glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, uploadPixels);
    } else {
        GLES.glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, uploadPixels);
    }

    if (uploadPixels != pixels && uploadPixels != nullptr) free(const_cast<void*>(uploadPixels));

    CHECK_GL_ERROR
}

// --- glTexStorage2D (native) ---
void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height) {
    LOG()
    LOG_D("glTexStorage2D, target: %d, levels: %d, internalFormat: %d, width: "
          "%d, height: %d",
          target, levels, internalFormat, width, height)

    internal_convert(&internalFormat, nullptr, nullptr);
    GLES.glTexStorage2D(target, levels, internalFormat, width, height);

    GET_TEXTURE_OBJECT(target);
    tex->target = ConvertGLEnumToTextureTarget(target);
    tex->internal_format = internalFormat;
    tex->width = width;
    tex->height = height;
    tex->depth = 1;
    tex->swizzle_param[0] = GL_RED;
    tex->swizzle_param[1] = GL_GREEN;
    tex->swizzle_param[2] = GL_BLUE;
    tex->swizzle_param[3] = GL_ALPHA;

    GLenum ERR = GLES.glGetError();
    if (ERR != GL_NO_ERROR) LOG_E("glTexStorage2D ERROR: %d", ERR)
}

// --- glTexStorage3D (native) ---
void glTexStorage3D(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height,
                    GLsizei depth) {
    LOG()
    LOG_D("glTexStorage3D, target: %d, levels: %d, internalFormat: %d, width: "
          "%d, height: %d, depth: %d",
          target, levels, internalFormat, width, height, depth)

    internal_convert(&internalFormat, nullptr, nullptr);

    GLES.glTexStorage3D(target, levels, internalFormat, width, height, depth);

    GET_TEXTURE_OBJECT(target);
    tex->target = ConvertGLEnumToTextureTarget(target);
    tex->internal_format = internalFormat;
    tex->width = width;
    tex->height = height;
    tex->depth = depth;
    tex->swizzle_param[0] = GL_RED;
    tex->swizzle_param[1] = GL_GREEN;
    tex->swizzle_param[2] = GL_BLUE;
    tex->swizzle_param[3] = GL_ALPHA;

    CHECK_GL_ERROR
}

// ============================================================================
// Native compressed texture functions (ES 3.2)
// ============================================================================

// --- glCompressedTexImage2D (native) ---
void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height,
                            GLint border, GLsizei imageSize, const void* data) {
    LOG()
    LOG_D("glCompressedTexImage2D, target: %s, level: %d, internalformat: %s, width: %d, height: %d, "
          "border: %d, imageSize: %d",
          glEnumToString(target), level, glEnumToString(internalformat), width, height, border, imageSize)

    GLES.glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);

    GET_TEXTURE_OBJECT(target);
    tex->target = ConvertGLEnumToTextureTarget(target);
    tex->internal_format = internalformat;
    tex->width = width;
    tex->height = height;
    tex->depth = 1;
    tex->swizzle_param[0] = GL_RED;
    tex->swizzle_param[1] = GL_GREEN;
    tex->swizzle_param[2] = GL_BLUE;
    tex->swizzle_param[3] = GL_ALPHA;

    CHECK_GL_ERROR
}

// --- glCompressedTexImage3D (native) ---
void glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height,
                            GLsizei depth, GLint border, GLsizei imageSize, const void* data) {
    LOG()
    LOG_D("glCompressedTexImage3D, target: %s, level: %d, internalformat: %s, width: %d, height: %d, "
          "depth: %d, border: %d, imageSize: %d",
          glEnumToString(target), level, glEnumToString(internalformat), width, height, depth, border, imageSize)

    GLES.glCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data);

    GET_TEXTURE_OBJECT(target);
    tex->target = ConvertGLEnumToTextureTarget(target);
    tex->internal_format = internalformat;
    tex->width = width;
    tex->height = height;
    tex->depth = depth;
    tex->swizzle_param[0] = GL_RED;
    tex->swizzle_param[1] = GL_GREEN;
    tex->swizzle_param[2] = GL_BLUE;
    tex->swizzle_param[3] = GL_ALPHA;

    CHECK_GL_ERROR
}

// --- glCompressedTexSubImage2D (native) ---
void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                               GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
    LOG()
    LOG_D("glCompressedTexSubImage2D, target: %s, level: %d, xoffset: %d, yoffset: %d, "
          "width: %d, height: %d, format: %s, imageSize: %d",
          glEnumToString(target), level, xoffset, yoffset, width, height, glEnumToString(format), imageSize)

    GLES.glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    CHECK_GL_ERROR
}

// --- glCompressedTexSubImage3D (native) ---
void glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                               GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize,
                               const void* data) {
    LOG()
    LOG_D("glCompressedTexSubImage3D, target: %s, level: %d, xoffset: %d, yoffset: %d, zoffset: %d, "
          "width: %d, height: %d, depth: %d, format: %s, imageSize: %d",
          glEnumToString(target), level, xoffset, yoffset, zoffset, width, height, depth, glEnumToString(format),
          imageSize)

    GLES.glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize,
                                   data);
    CHECK_GL_ERROR
}

// ============================================================================
// Native copy texture functions (ES 3.2)
// ============================================================================

// --- glCopyTexImage2D (native) ---
void glCopyTexImage2D(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width,
                      GLsizei height, GLint border) {
    LOG()

    INIT_CHECK_GL_ERROR

    GLint realInternalFormat;
    GLES.glGetTexLevelParameteriv(target, level, GL_TEXTURE_INTERNAL_FORMAT, &realInternalFormat);
    internalFormat = (GLenum)realInternalFormat;

    LOG_D("glCopyTexImage2D, target: %d, level: %d, internalFormat: %d, x: %d, "
          "y: %d, width: %d, height: %d, border: %d",
          target, level, internalFormat, x, y, width, height, border)

    if (is_depth_format(internalFormat)) {
        GLenum format = GL_DEPTH_COMPONENT;
        GLenum type = GL_UNSIGNED_INT;
        internal_convert(&internalFormat, &type, &format);
        GLES.glTexImage2D(target, level, (GLint)internalFormat, width, height, border, format, type, nullptr);
        CHECK_GL_ERROR_NO_INIT
        // Use CPU-side cached FBO bindings instead of glGetIntegerv GPU round-trips
        GLint prevDrawFBO = GLState.framebuffer.drawFBO;
        CHECK_GL_ERROR_NO_INIT
        GLuint tempDrawFBO = acquireTempFBO();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tempDrawFBO);
        CHECK_GL_ERROR_NO_INIT
        // Use CPU-side tracked texture binding instead of glGetIntegerv
        GLint currentTex = g_tracked_tex2d_binding[GetCurrentTextureUnitIndex()];
        CHECK_GL_ERROR_NO_INIT
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, currentTex, level);
        CHECK_GL_ERROR_NO_INIT

        if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) [[unlikely]] {
            CHECK_GL_ERROR_NO_INIT
            releaseTempFBO(tempDrawFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
            CHECK_GL_ERROR_NO_INIT
            return;
        }
        CHECK_GL_ERROR_NO_INIT

        GLES.glBlitFramebuffer(x, y, x + width, y + height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        CHECK_GL_ERROR_NO_INIT

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
        releaseTempFBO(tempDrawFBO);
        CHECK_GL_ERROR_NO_INIT
    } else {
        GLES.glCopyTexImage2D(target, level, internalFormat, x, y, width, height, border);
        CHECK_GL_ERROR_NO_INIT
    }

    GET_TEXTURE_OBJECT(target);
    tex->target = ConvertGLEnumToTextureTarget(target);
    tex->internal_format = internalFormat;
    tex->width = width;
    tex->height = height;
    tex->depth = 1;
    tex->swizzle_param[0] = GL_RED;
    tex->swizzle_param[1] = GL_GREEN;
    tex->swizzle_param[2] = GL_BLUE;
    tex->swizzle_param[3] = GL_ALPHA;

    CHECK_GL_ERROR_NO_INIT
}

// --- glCopyTexSubImage2D (native) ---
void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width,
                         GLsizei height) {
    LOG()
    GLint internalFormat;
    GLES.glGetTexLevelParameteriv(target, level, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);

    LOG_D("glCopyTexSubImage2D, target: %s, level: %d, xoffset: %d, yoffset: %d, "
          "x: %d, y: %d, width: %d, height: %d",
          glEnumToString(target), level, xoffset, yoffset, x, y, width, height)

    if (is_depth_format((GLenum)internalFormat)) {
        // Use CPU-side cached FBO bindings instead of glGetIntegerv GPU round-trips
        GLint prevReadFBO = GLState.framebuffer.readFBO;
        GLint prevDrawFBO = GLState.framebuffer.drawFBO;

        GLuint tempDrawFBO = acquireTempFBO();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tempDrawFBO);

        GLint currentTex;
        // Use CPU-side tracked texture binding for 2D (common case),
        // fall back to glGetIntegerv for cubemap targets
        if (target == GL_TEXTURE_2D) {
            currentTex = g_tracked_tex2d_binding[GetCurrentTextureUnitIndex()];
        } else {
            glGetIntegerv(get_binding_for_target(target), &currentTex);
        }
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, currentTex, level);

        if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) [[unlikely]] {
            releaseTempFBO(tempDrawFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
            return;
        }

        GLES.glBlitFramebuffer(x, y, x + width, y + height, xoffset, yoffset, xoffset + width, yoffset + height,
                               GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
        releaseTempFBO(tempDrawFBO);
    } else {
        GLES.glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    }

    CHECK_GL_ERROR
}

// --- glCopyTexSubImage3D (native) ---
void glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y,
                         GLsizei width, GLsizei height) {
    LOG()
    LOG_D("glCopyTexSubImage3D, target: %s, level: %d, xoffset: %d, yoffset: %d, zoffset: %d, "
          "x: %d, y: %d, width: %d, height: %d",
          glEnumToString(target), level, xoffset, yoffset, zoffset, x, y, width, height)

    GLES.glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
    CHECK_GL_ERROR
}

// ============================================================================
// Native mipmap generation (ES 3.2)
// ============================================================================

// --- glGenerateMipmap (native) ---
void glGenerateMipmap(GLenum target) {
    LOG()
    LOG_D("glGenerateMipmap, target: %s", glEnumToString(target))
    GLES.glGenerateMipmap(target);
    CHECK_GL_ERROR
}

// ============================================================================
// Native texture parameter functions (ES 3.2)
// ============================================================================

// --- glTexParameterf (native) ---
void glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    LOG()
    pname = pname_convert(pname);
    LOG_D("glTexParameterf, target: %d, pname: %d, param: %f", target, pname, param)

    if (pname == GL_TEXTURE_LOD_BIAS_QCOM && !g_gles_caps.GL_QCOM_texture_lod_bias) {
        LOG_D("Does not support GL_QCOM_texture_lod_bias, skipped!")
        return;
    }

    GLES.glTexParameterf(target, pname, param);
    CHECK_GL_ERROR
}

// --- glTexParameteri (native) ---
void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    LOG()
    pname = pname_convert(pname);
    LOG_D("glTexParameteri, pname: 0x%x", pname)

    if (pname == GL_TEXTURE_LOD_BIAS_QCOM && !g_gles_caps.GL_QCOM_texture_lod_bias) {
        LOG_D("Does not support GL_QCOM_texture_lod_bias, skipped!")
        return;
    }

    GLES.glTexParameteri(target, pname, param);
    CHECK_GL_ERROR
}

// --- glTexParameteriv (native) ---
void glTexParameteriv(GLenum target, GLenum pname, const GLint* params) {
    LOG()
    LOG_D("glTexParameteriv, target: %s, pname: %s", glEnumToString(target), glEnumToString(pname))

    if (pname == GL_TEXTURE_SWIZZLE_RGBA) {
        LOG_D("find GL_TEXTURE_SWIZZLE_RGBA, now use glTexParameteri")
        if (params) {
            // deferred those call to draw call?
            GLES.glTexParameteri(target, GL_TEXTURE_SWIZZLE_R, params[0]);
            GLES.glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, params[1]);
            GLES.glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, params[2]);
            GLES.glTexParameteri(target, GL_TEXTURE_SWIZZLE_A, params[3]);

            // save states for now
            GET_TEXTURE_OBJECT(target);
            tex->swizzle_param[0] = params[0];
            tex->swizzle_param[1] = params[1];
            tex->swizzle_param[2] = params[2];
            tex->swizzle_param[3] = params[3];
        } else {
            LOG_E("glTexParameteriv: params is nullptr for GL_TEXTURE_SWIZZLE_RGBA")
        }
    } else {
        GLES.glTexParameteriv(target, pname, params);
    }

    CHECK_GL_ERROR
}

// --- glTexParameterfv (native) ---
void glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
    LOG()
    LOG_D("glTexParameterfv, target: %s, pname: %s", glEnumToString(target), glEnumToString(pname))
    GLES.glTexParameterfv(target, pname, params);
    CHECK_GL_ERROR
}

// ============================================================================
// Native texture query functions (ES 3.2)
// ============================================================================

// --- glGetTexParameteriv (native) ---
void glGetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
    LOG()
    LOG_D("glGetTexParameteriv, target: %s, pname: %s", glEnumToString(target), glEnumToString(pname))
    GLES.glGetTexParameteriv(target, pname, params);
    CHECK_GL_ERROR
}

// --- glGetTexParameterfv (native) ---
void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
    LOG()
    LOG_D("glGetTexParameterfv, target: %s, pname: %s", glEnumToString(target), glEnumToString(pname))
    GLES.glGetTexParameterfv(target, pname, params);
    CHECK_GL_ERROR
}

// --- glGetTexLevelParameterfv (native) ---
void glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat* params) {
    LOG()
    LOG_D("glGetTexLevelParameterfv,target: %d, level: %d, pname: %d", target, level, pname)
    if (gl_state) {
        GLenum rtarget = map_tex_target(target);
        if (rtarget == GL_PROXY_TEXTURE_2D) {
            switch (pname) {
            case GL_TEXTURE_WIDTH:
                (*params) = (float)nlevel(gl_state->proxy_width, level);
                return;
            case GL_TEXTURE_HEIGHT:
                (*params) = (float)nlevel(gl_state->proxy_height, level);
                return;
            case GL_TEXTURE_INTERNAL_FORMAT:
                (*params) = (float)gl_state->proxy_intformat;
                return;
            default:
                return;
            }
        }
    }
    GLES.glGetTexLevelParameterfv(target, level, pname, params);
    CHECK_GL_ERROR
}

// --- glGetTexLevelParameteriv (native) ---
void glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params) {
    LOG()
    LOG_D("glGetTexLevelParameteriv,target: %s, level: %d, pname: %s", glEnumToString(target), level,
          glEnumToString(pname))
    if (gl_state) {
        GLenum rtarget = map_tex_target(target);
        if (rtarget == GL_PROXY_TEXTURE_2D) {
            switch (pname) {
            case GL_TEXTURE_WIDTH:
                (*params) = nlevel(gl_state->proxy_width, level);
                return;
            case GL_TEXTURE_HEIGHT:
                (*params) = nlevel(gl_state->proxy_height, level);
                return;
            case GL_TEXTURE_INTERNAL_FORMAT:
                (*params) = (GLint)gl_state->proxy_intformat;
                return;
            default:
                return;
            }
        }
    }
    LOG_D("es.glGetTexLevelParameteriv,target: %s, level: %d, pname: %s", glEnumToString(target), level,
          glEnumToString(pname))
    GLES.glGetTexLevelParameteriv(target, level, pname, params);
    CHECK_GL_ERROR
}

// ============================================================================
// Renderbuffer functions (native)
// ============================================================================

void glRenderbufferStorage(GLenum target, GLenum internalFormat, GLsizei width, GLsizei height) {
    LOG()

    INIT_CHECK_GL_ERROR_FORCE

    LOG_D("glRenderbufferStorage, target: %s, internalFormat: %s, width: %d, "
          "height: %d",
          glEnumToString(target), glEnumToString(internalFormat), width, height)

    GLES.glRenderbufferStorage(target, internalFormat, width, height);

    CHECK_GL_ERROR_NO_INIT
}

void glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width,
                                      GLsizei height) {
    LOG()

    INIT_CHECK_GL_ERROR_FORCE

    LOG_D("glRenderbufferStorageMultisample, target: %d, samples: %d, "
          "internalFormat: %d, width: %d, height: %d",
          target, samples, internalFormat, width, height)

    GLES.glRenderbufferStorageMultisample(target, samples, internalFormat, width, height);

    CHECK_GL_ERROR_NO_INIT
}

// ============================================================================
// Other texture functions (keep existing logic)
// ============================================================================

void glGetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
    LOG()
    LOG_D("glGetTexImage, target: 0x%x, level: %d, format: 0x%x, type: 0x%x, pixels: 0x%x", target, level, format, type,
          pixels)
    // Use CPU-side cached FBO bindings instead of glGetIntegerv GPU round-trips
    GLint prevDrawFBO = GLState.framebuffer.drawFBO;
    GLint prevReadFBO = GLState.framebuffer.readFBO;

    GLuint tempFBO = acquireTempFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, tempFBO);

    GLint textureId = 0;
    GLenum textureBindingTarget;
    if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z) {
        textureBindingTarget = GL_TEXTURE_BINDING_CUBE_MAP;
    } else if (target == GL_TEXTURE_2D) {
        textureBindingTarget = GL_TEXTURE_BINDING_2D;
    } else {
        LOG_E("glGetTexImage: Unsupported or complex target: 0x%x", target)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
        releaseTempFBO(tempFBO);
        return;
    }
    // Use CPU-side tracked texture binding for 2D (common case),
    // fall back to glGetIntegerv for cubemap targets
    if (textureBindingTarget == GL_TEXTURE_BINDING_2D) {
        textureId = g_tracked_tex2d_binding[GetCurrentTextureUnitIndex()];
    } else {
        glGetIntegerv(textureBindingTarget, &textureId);
    }

    if (textureId == 0) {
        LOG_E("glGetTexImage: No texture bound to the specified target.")
        glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
        releaseTempFBO(tempFBO);
        return;
    }

    GLint width = 0, height = 0;
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);

    if (width == 0 || height == 0) {
        LOG_E("glGetTexImage: Texture level %d has zero width or height.", level)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
        releaseTempFBO(tempFBO);
        return;
    }

    if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, textureId, level);
    } else if (target == GL_TEXTURE_2D) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, level);
    }

    GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE) [[unlikely]] {
        LOG_E("glGetTexImage: Failed to create complete framebuffer. Status: 0x%x", fboStatus)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
        releaseTempFBO(tempFBO);
        return;
    }

    glReadBuffer(GL_COLOR_ATTACHMENT0);

    glReadPixels(0, 0, width, height, format, type, pixels);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
    releaseTempFBO(tempFBO);
}

#if GLOBAL_DEBUG || DEBUG
#include "../config/config.h"
#include <fstream>
#endif

void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) {
    LOG()
    LOG_D("glReadPixels, x=%d, y=%d, width=%d, height=%d, format=0x%x, "
          "type=0x%x, pixels=0x%x",
          x, y, width, height, format, type, pixels)

    // GLES 3.2 cannot emit GL_BGRA / GL_UNSIGNED_INT_8_8_8_8 / GL_UNSIGNED_INT_8_8_8_8_REV
    // directly. Always read back as GL_RGBA + GL_UNSIGNED_BYTE and apply a
    // CPU-side swizzle afterwards so the in-memory bytes match what the caller
    // asked for. This mirrors the pack path of MobileGL-DirectGLES's
    // PixelStoreProcessor.
    unsigned char swizzle[4];
    const bool needSwizzle = get_rgba_pack_swizzle(format, type, swizzle);

    GLenum glesFormat = format;
    GLenum glesType = type;
    if (needSwizzle) {
        glesFormat = GL_RGBA;
        glesType = GL_UNSIGNED_BYTE;
    }

    LOG_D("glReadPixels converted, x=%d, y=%d, width=%d, height=%d, format=0x%x, "
          "type=0x%x, pixels=0x%x, needSwizzle=%d",
          x, y, width, height, glesFormat, glesType, pixels, (int)needSwizzle)

    // PBO path: `pixels` is a byte offset into the bound GL_PIXEL_PACK_BUFFER
    // and an in-place CPU swizzle is not possible. Fall back to the legacy
    // behaviour (write RGBA bytes - layout differs from what the caller asked
    // for, but at least the read itself succeeds).
    const GLuint boundPBO = find_bound_buffer(GL_PIXEL_PACK_BUFFER_BINDING);
    if (needSwizzle && boundPBO != 0) {
        LOG_W("glReadPixels: BGRA swizzle requested with a PBO bound; "
              "data will be RGBA instead of %s/%s",
              glEnumToString(format), glEnumToString(type))
        GLES.glReadPixels(x, y, width, height, glesFormat, glesType, pixels);
        CHECK_GL_ERROR
        return;
    }

    if (!needSwizzle || !pixels) {
        // No swizzle needed - forward to GLES with the caller's pack settings.
        GLES.glReadPixels(x, y, width, height, glesFormat, glesType, pixels);
        CHECK_GL_ERROR
        return;
    }

    // BGRA / packed-type readback path:
    // 1. Allocate a tightly-packed temp buffer.
    // 2. Reset GLES's GL_PACK_* to tight so GLES writes tightly-packed RGBA
    //    into the temp buffer (independent of the caller's pack settings,
    //    which may have non-default row length / alignment / skips).
    // 3. CPU-swizzle the tight RGBA buffer into the requested BGRA / packed
    //    layout.
    // 4. Copy the tight result back into the caller's `pixels` buffer,
    //    honoring the caller's GL_PACK_ROW_LENGTH / ALIGNMENT / SKIP_*.
    const GLuint pixelCount = static_cast<GLuint>(width) * static_cast<GLuint>(height);
    const size_t byteCount = static_cast<size_t>(pixelCount) * 4;
    void* tight = malloc(byteCount);
    if (!tight) {
        LOG_E("glReadPixels: allocation of %zu bytes failed; falling back to RGBA write", byteCount)
        GLES.glReadPixels(x, y, width, height, glesFormat, glesType, pixels);
        CHECK_GL_ERROR
        return;
    }

    {
        ScopedPackTight packGuard;
        GLES.glReadPixels(x, y, width, height, glesFormat, glesType, tight);
    }
    // Errors from the glReadPixels call above are still reported via the
    // post-call glGetError() in CHECK_GL_ERROR below.

    // Swizzle the tightly-packed RGBA into the user-requested layout (BGRA
    // and/or reversed byte order for GL_UNSIGNED_INT_8_8_8_8 etc.).
    ProcessColorSwizzle(tight, pixelCount, swizzle, 4);

    // Re-layout from tight buffer to caller's `pixels` honoring GL_PACK_*.
    const GLint rowLength   = GLState.texture.packRowLength;
    const GLint alignment   = GLState.texture.packAlignment;
    const GLint skipPixels  = GLState.texture.packSkipPixels;
    const GLint skipRows    = GLState.texture.packSkipRows;
    constexpr GLuint bytesPerPixel = 4;
    const GLuint effectiveRow = (rowLength > 0) ? static_cast<GLuint>(rowLength) : static_cast<GLuint>(width);
    const GLuint dstRowStride = static_cast<GLuint>(widthalign(effectiveRow * bytesPerPixel, alignment));
    const size_t dstSkipOffset =
        static_cast<size_t>(skipRows) * dstRowStride +
        static_cast<size_t>(skipPixels) * bytesPerPixel;

    if (dstRowStride == static_cast<GLuint>(width) * bytesPerPixel && skipPixels == 0 && skipRows == 0) {
        // Caller wants tightly packed output - single memcpy.
        memcpy(pixels, tight, byteCount);
    } else {
        // Per-row copy with the caller's row stride; padding bytes between
        // rows are left untouched (matching OpenGL spec behaviour).
        const unsigned char* src = static_cast<const unsigned char*>(tight);
        unsigned char* dst = static_cast<unsigned char*>(pixels) + dstSkipOffset;
        const size_t rowBytes = static_cast<size_t>(width) * bytesPerPixel;
        for (GLsizei y = 0; y < height; ++y) {
            memcpy(dst, src, rowBytes);
            src += rowBytes;
            dst += dstRowStride;
        }
    }

    free(tight);

    CHECK_GL_ERROR
}

void glClearTexImage(GLuint texture, GLint level, GLenum format, GLenum type, const void* data) {
    LOG()
    LOG_D("glClearTexImage, texture: %d, level: %d, format: %d, type: %d", texture, level, format, type)
    INIT_CHECK_GL_ERROR_FORCE
    // Use CPU-side cached FBO bindings instead of glGetIntegerv GPU round-trips
    GLuint fbo;
    GLint prevDrawFBO = GLState.framebuffer.drawFBO;
    GLint prevReadFBO = GLState.framebuffer.readFBO;
    fbo = acquireTempFBO();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    CHECK_GL_ERROR_NO_INIT

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, level);

    CHECK_GL_ERROR_NO_INIT
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) [[unlikely]] {
        LOG_D("  -> exit")
        glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
        releaseTempFBO(fbo);
        CHECK_GL_ERROR_NO_INIT
        return;
    }

    GLES.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    CHECK_GL_ERROR_NO_INIT

    if (data != nullptr) {
        if (format == GL_RGBA && type == GL_UNSIGNED_BYTE) {
            auto* byteData = static_cast<const GLubyte*>(data);
            GLES.glClearColor((float)byteData[0] / 255.0f, (float)byteData[1] / 255.0f, (float)byteData[2] / 255.0f,
                              (float)byteData[3] / 255.0f);
        } else if (format == GL_RGB && type == GL_UNSIGNED_BYTE) {
            auto* byteData = static_cast<const GLubyte*>(data);
            GLES.glClearColor((float)byteData[0] / 255.0f, (float)byteData[1] / 255.0f, (float)byteData[2] / 255.0f,
                              1.0f);
        } else if (format == GL_RGBA && type == GL_FLOAT) {
            auto* floatData = static_cast<const GLfloat*>(data);
            GLES.glClearColor(floatData[0], floatData[1], floatData[2], floatData[3]);
        } else if (format == GL_RGB && type == GL_FLOAT) {
            auto* floatData = static_cast<const GLfloat*>(data);
            GLES.glClearColor(floatData[0], floatData[1], floatData[2], 1.0f);
        } else if (format == GL_DEPTH_COMPONENT && type == GL_FLOAT) {
            auto* depthData = static_cast<const GLfloat*>(data);
            GLES.glClearDepthf(depthData[0]);
            GLES.glClear(GL_DEPTH_BUFFER_BIT);
        } else if (format == GL_STENCIL_INDEX && type == GL_UNSIGNED_BYTE) {
            auto* stencilData = static_cast<const GLubyte*>(data);
            GLES.glClearStencil(stencilData[0]);
            GLES.glClear(GL_STENCIL_BUFFER_BIT);
        }
    }
    CHECK_GL_ERROR_NO_INIT

    if (format == GL_DEPTH_COMPONENT || format == GL_STENCIL_INDEX) {
        GLES.glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        CHECK_GL_ERROR_NO_INIT
    } else {
        GLES.glClear(GL_COLOR_BUFFER_BIT);
        CHECK_GL_ERROR_NO_INIT
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
    releaseTempFBO(fbo);
    CHECK_GL_ERROR_NO_INIT
}

void glPixelStorei(GLenum pname, GLint param) {
    LOG_D("glPixelStorei, pname = %s, param = %d", glEnumToString(pname), param)
    GLES.glPixelStorei(pname, param);
    // Keep CPU-side cache in sync to avoid glGetIntegerv GPU round-trips
    switch (pname) {
        case GL_UNPACK_ALIGNMENT:  GLState.texture.unpackAlignment = param;  break;
        case GL_UNPACK_ROW_LENGTH: GLState.texture.unpackRowLength = param;  break;
        case GL_UNPACK_IMAGE_HEIGHT: GLState.texture.unpackImageHeight = param; break;
        case GL_UNPACK_SKIP_PIXELS: GLState.texture.unpackSkipPixels = param; break;
        case GL_UNPACK_SKIP_ROWS:  GLState.texture.unpackSkipRows = param;  break;
        case GL_UNPACK_SKIP_IMAGES: GLState.texture.unpackSkipImages = param; break;
        case GL_PACK_ALIGNMENT:    GLState.texture.packAlignment = param;    break;
        case GL_PACK_ROW_LENGTH:   GLState.texture.packRowLength = param;    break;
        case GL_PACK_IMAGE_HEIGHT: GLState.texture.packImageHeight = param;  break;
        case GL_PACK_SKIP_PIXELS:  GLState.texture.packSkipPixels = param;   break;
        case GL_PACK_SKIP_ROWS:    GLState.texture.packSkipRows = param;     break;
        case GL_PACK_SKIP_IMAGES:  GLState.texture.packSkipImages = param;   break;
        default: break;
    }
    CHECK_GL_ERROR
}

// glGenerateTextureMipmap is handled in ExtWrappers/DSAWrapper.cpp (DSA emulation)