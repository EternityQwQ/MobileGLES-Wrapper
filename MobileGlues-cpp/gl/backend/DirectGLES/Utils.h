// MobileGlues - gl/backend/DirectGLES/Utils.h
// Backend utilities for GLES 3.2
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#pragma once

#include "../../../includes.h"
#include <MG_State/GLState/Core.h>

namespace MobileGL::backend::DirectGLES {

namespace DebugImpl {

class ErrorLopper {
public:
    static void Loop(const std::function<void(GLenum)>&);
    static void Clear();
    ErrorLopper();
    ~ErrorLopper();
};

class OpenGLScopeMarker {
public:
    explicit OpenGLScopeMarker(const String& scopeName);
    ~OpenGLScopeMarker();
};

} // namespace DebugImpl

namespace TextureImpl {

void GenerateTextureFormatInfo(TextureInternalFormat internalFormat, GLenum* outInternalFormat,
                               GLenum* outFormat, GLenum* outType,
                               TextureTarget target = TextureTarget::Unknown);
void GenerateRenderbufferFormatInfo(TextureInternalFormat internalFormat, GLenum* outInternalFormat,
                                    GLenum* outFormat, GLenum* outType);
Bool ShouldUseCaveatTextureFormat(TextureInternalFormat internalFormat, TextureTarget target);
Bool ShouldUseCaveatRenderbufferFormat(TextureInternalFormat internalFormat);

} // namespace TextureImpl

namespace ProgramImpl {

String ProcessOutColorLocations(const String& glslCode);
String ForceSupporterOutput(const String& glslCode);
String ClampNormFallbackOutputs(String glslCode, GLenum shaderType, Uint32 snormOutputMask,
                                Uint32 unormOutputMask);
String ForceFlatIntegerVaryings(const String& glslCode, GLenum shaderType);
String RemoveLayoutBinding(const String& glslCode);

} // namespace ProgramImpl

namespace Utils {

void CheckGLESError();
GLenum GetBindingQuery(GLenum target, bool isTexture);

} // namespace Utils

} // namespace MobileGL::backend::DirectGLES