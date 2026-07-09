// MobileGlues - gl/impl/Texture/GL_Texture.cpp
// GL Texture implementation - Core Profile → GLES 3.2
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "GL_Texture.h"
#include "../../backend/DirectGLES/DirectGLES.h"
#include "../../backend/DirectGLES/Managers.h"
#include "../../state/Core.h"
#include "../../state.h"

using namespace MobileGL::backend::DirectGLES;
using namespace MobileGL::MG_State::GLState;

namespace MobileGL::impl::GLImpl {

void ActiveTexture(GLenum texture) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();
    stateMgr.SetActiveTextureUnit(texture - GL_TEXTURE0);
    CallAndCheckGLES(g_GLESFuncs.glActiveTexture(texture));
}

void BindTexture(GLenum target, GLuint texture) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    auto tex = glCtx.GetTexture(texture);
    TextureTarget texTarget = static_cast<TextureTarget>(target);

    Uint activeUnit = stateMgr.GetActiveTextureUnit();
    stateMgr.GetTextureState().BindTexture(activeUnit, texTarget, tex);
}

void GenTextures(GLsizei n, GLuint* textures) {
    auto& glCtx = GLContext::Get();
    for (GLsizei i = 0; i < n; ++i) {
        glCtx.CreateTexture(textures[i]);
    }
}

void DeleteTextures(GLsizei n, const GLuint* textures) {
    auto& glCtx = GLContext::Get();
    for (GLsizei i = 0; i < n; ++i) {
        glCtx.DeleteTexture(textures[i]);
    }
}

GLboolean IsTexture(GLuint texture) {
    return GLContext::Get().IsTexture(texture) ? GL_TRUE : GL_FALSE;
}

void TexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                GLint border, GLenum format, GLenum type, const void* pixels) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    TextureTarget texTarget = static_cast<TextureTarget>(target);
    Uint activeUnit = stateMgr.GetActiveTextureUnit();
    auto tex = stateMgr.GetTextureState().GetBoundTexture(activeUnit, texTarget);

    if (tex) {
        // Update state texture info
        auto& backendTex = TextureImpl::SyncTextureObjectToBackend(tex);
        backendTex.Bind(target);
    }

    CallAndCheckGLES(g_GLESFuncs.glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels));
}

void TexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels) {
    CallAndCheckGLES(g_GLESFuncs.glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels));
}

void TexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                   GLsizei height, GLenum format, GLenum type, const void* pixels) {
    CallAndCheckGLES(g_GLESFuncs.glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels));
}

void TexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                   GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    CallAndCheckGLES(g_GLESFuncs.glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels));
}

void CompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                          GLsizei height, GLint border, GLsizei imageSize, const void* data) {
    CallAndCheckGLES(g_GLESFuncs.glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data));
}

void CompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                             GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
    CallAndCheckGLES(g_GLESFuncs.glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data));
}

void CompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                          GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data) {
    CallAndCheckGLES(g_GLESFuncs.glCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data));
}

void CompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                             GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize,
                             const void* data) {
    CallAndCheckGLES(g_GLESFuncs.glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data));
}

void GetCompressedTexImage(GLenum target, GLint level, void* img) {
    CallAndCheckGLES(g_GLESFuncs.glGetCompressedTexImage(target, level, img));
}

void TexParameterf(GLenum target, GLenum pname, GLfloat param) {
    CallAndCheckGLES(g_GLESFuncs.glTexParameterf(target, pname, param));
}

void TexParameteri(GLenum target, GLenum pname, GLint param) {
    CallAndCheckGLES(g_GLESFuncs.glTexParameteri(target, pname, param));
}

void TexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
    CallAndCheckGLES(g_GLESFuncs.glTexParameterfv(target, pname, params));
}

void TexParameteriv(GLenum target, GLenum pname, const GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glTexParameteriv(target, pname, params));
}

void GetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetTexParameterfv(target, pname, params));
}

void GetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetTexParameteriv(target, pname, params));
}

void GetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetTexLevelParameterfv(target, level, pname, params));
}

void GetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetTexLevelParameteriv(target, level, pname, params));
}

void GetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
    CallAndCheckGLES(g_GLESFuncs.glGetTexImage(target, level, format, type, pixels));
}

void GenerateMipmap(GLenum target) {
    CallAndCheckGLES(g_GLESFuncs.glGenerateMipmap(target));
}

void PixelStorei(GLenum pname, GLint param) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();
    stateMgr.GetTextureState().SetPixelStore(pname, param);
    CallAndCheckGLES(g_GLESFuncs.glPixelStorei(pname, param));
}

void PixelStoref(GLenum pname, GLfloat param) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();
    stateMgr.GetTextureState().SetPixelStore(pname, static_cast<GLint>(param));
    CallAndCheckGLES(g_GLESFuncs.glPixelStorei(pname, static_cast<GLint>(param)));
}

// DSA texture functions
void TextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    auto& glCtx = GLContext::Get();
    auto tex = glCtx.GetTexture(texture);
    if (tex) {
        auto& backendTex = TextureImpl::SyncTextureObjectToBackend(tex);
        backendTex.Bind(GL_TEXTURE_2D);
    }
    CallAndCheckGLES(g_GLESFuncs.glTexStorage2D(GL_TEXTURE_2D, levels, internalformat, width, height));
}

void TextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height,
                      GLsizei depth) {
    auto& glCtx = GLContext::Get();
    auto tex = glCtx.GetTexture(texture);
    if (tex) {
        auto& backendTex = TextureImpl::SyncTextureObjectToBackend(tex);
        backendTex.Bind(GL_TEXTURE_3D);
    }
    CallAndCheckGLES(g_GLESFuncs.glTexStorage3D(GL_TEXTURE_3D, levels, internalformat, width, height, depth));
}

void TextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                       GLsizei height, GLenum format, GLenum type, const void* pixels) {
    CallAndCheckGLES(g_GLESFuncs.glTextureSubImage2D(texture, level, xoffset, yoffset, width, height, format, type, pixels));
}

void TextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                       GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    CallAndCheckGLES(g_GLESFuncs.glTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels));
}

void CopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                           GLint x, GLint y, GLsizei width, GLsizei height) {
    CallAndCheckGLES(g_GLESFuncs.glCopyTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, x, y, width, height));
}

} // namespace MobileGL::impl::GLImpl