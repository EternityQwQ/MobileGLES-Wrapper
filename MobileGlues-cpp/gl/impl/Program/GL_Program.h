// MobileGlues - gl/impl/Program/GL_Program.h
// GL Program/Shader implementation - Core Profile → GLES 3.2
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

GLuint CreateShader(GLenum type);
void DeleteShader(GLuint shader);
GLboolean IsShader(GLuint shader);
void ShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
void CompileShader(GLuint shader);
void GetShaderiv(GLuint shader, GLenum pname, GLint* params);
void GetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);

GLuint CreateProgram();
void DeleteProgram(GLuint program);
GLboolean IsProgram(GLuint program);
void AttachShader(GLuint program, GLuint shader);
void DetachShader(GLuint program, GLuint shader);
void LinkProgram(GLuint program);
void UseProgram(GLuint program);
void GetProgramiv(GLuint program, GLenum pname, GLint* params);
void GetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
void ValidateProgram(GLuint program);

void BindAttribLocation(GLuint program, GLuint index, const GLchar* name);
void BindFragDataLocation(GLuint program, GLuint color, const GLchar* name);
GLint GetAttribLocation(GLuint program, const GLchar* name);
GLint GetFragDataLocation(GLuint program, const GLchar* name);
GLint GetUniformLocation(GLuint program, const GLchar* name);
void GetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size,
                      GLenum* type, GLchar* name);
void GetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size,
                     GLenum* type, GLchar* name);

// Uniform setters
void Uniform1f(GLint location, GLfloat v0);
void Uniform2f(GLint location, GLfloat v0, GLfloat v1);
void Uniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void Uniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void Uniform1i(GLint location, GLint v0);
void Uniform2i(GLint location, GLint v0, GLint v1);
void Uniform3i(GLint location, GLint v0, GLint v1, GLint v2);
void Uniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
void Uniform1ui(GLint location, GLuint v0);
void Uniform2ui(GLint location, GLuint v0, GLuint v1);
void Uniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
void Uniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
void Uniform1fv(GLint location, GLsizei count, const GLfloat* value);
void Uniform2fv(GLint location, GLsizei count, const GLfloat* value);
void Uniform3fv(GLint location, GLsizei count, const GLfloat* value);
void Uniform4fv(GLint location, GLsizei count, const GLfloat* value);
void Uniform1iv(GLint location, GLsizei count, const GLint* value);
void Uniform2iv(GLint location, GLsizei count, const GLint* value);
void Uniform3iv(GLint location, GLsizei count, const GLint* value);
void Uniform4iv(GLint location, GLsizei count, const GLint* value);
void Uniform1uiv(GLint location, GLsizei count, const GLuint* value);
void Uniform2uiv(GLint location, GLsizei count, const GLuint* value);
void Uniform3uiv(GLint location, GLsizei count, const GLuint* value);
void Uniform4uiv(GLint location, GLsizei count, const GLuint* value);
void UniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void UniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void UniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void UniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void UniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void UniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void UniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
void UniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);

// Uniform block
GLuint GetUniformBlockIndex(GLuint program, const GLchar* uniformBlockName);
void UniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
void GetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params);
void GetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length,
                               GLchar* uniformBlockName);

// Program pipeline
void BindProgramPipeline(GLuint pipeline);
void GenProgramPipelines(GLsizei n, GLuint* pipelines);
void DeleteProgramPipelines(GLsizei n, const GLuint* pipelines);
void UseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program);

} // namespace MobileGL::impl::GLImpl