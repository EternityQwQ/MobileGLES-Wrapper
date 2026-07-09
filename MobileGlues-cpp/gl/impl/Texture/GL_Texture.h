// MobileGlues - gl/impl/Texture/GL_Texture.h
// GL Texture implementation - Core Profile → GLES 3.2
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#pragma once

#include "../../../includes.h"

namespace MobileGL::impl::GLImpl {

void ActiveTexture(GLenum texture);
void BindTexture(GLenum target, GLuint texture);
void GenTextures(GLsizei n, GLuint* textures);
void DeleteTextures(GLsizei n, const GLuint* textures);
GLboolean IsTexture(GLuint texture);
void TexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                GLint border, GLenum format, GLenum type, const void* pixels);
void TexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels);
void TexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                   GLsizei height, GLenum format, GLenum type, const void* pixels);
void TexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                   GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);
void CompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                          GLsizei height, GLint border, GLsizei imageSize, const void* data);
void CompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                             GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data);
void CompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                          GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data);
void CompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                             GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize,
                             const void* data);
void GetCompressedTexImage(GLenum target, GLint level, void* img);
void TexParameterf(GLenum target, GLenum pname, GLfloat param);
void TexParameteri(GLenum target, GLenum pname, GLint param);
void TexParameterfv(GLenum target, GLenum pname, const GLfloat* params);
void TexParameteriv(GLenum target, GLenum pname, const GLint* params);
void GetTexParameterfv(GLenum target, GLenum pname, GLfloat* params);
void GetTexParameteriv(GLenum target, GLenum pname, GLint* params);
void GetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat* params);
void GetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params);
void GetTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels);
void GenerateMipmap(GLenum target);
void PixelStorei(GLenum pname, GLint param);
void PixelStoref(GLenum pname, GLfloat param);

// DSA (Direct State Access) texture functions
void TextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
void TextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height,
                      GLsizei depth);
void TextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                       GLsizei height, GLenum format, GLenum type, const void* pixels);
void TextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                       GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);
void CopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                           GLint x, GLint y, GLsizei width, GLsizei height);

} // namespace MobileGL::impl::GLImpl