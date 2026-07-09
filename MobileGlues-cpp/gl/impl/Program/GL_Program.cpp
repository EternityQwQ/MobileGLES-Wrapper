// MobileGlues - gl/impl/Program/GL_Program.cpp
// GL Program/Shader implementation - Core Profile → GLES 3.2
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "GL_Program.h"
#include "../../backend/DirectGLES/DirectGLES.h"
#include "../../backend/DirectGLES/Managers.h"
#include "../../state/Core.h"
#include "../../state.h"

using namespace MobileGL::backend::DirectGLES;
using namespace MobileGL::MG_State::GLState;

namespace MobileGL::impl::GLImpl {

// =============================================================================
// Shader
// =============================================================================

GLuint CreateShader(GLenum type) {
    auto& glCtx = GLContext::Get();
    GLuint shader = g_GLESFuncs.glCreateShader(type);
    glCtx.CreateShader(shader);
    return shader;
}

void DeleteShader(GLuint shader) {
    auto& glCtx = GLContext::Get();
    glCtx.DeleteShader(shader);
    g_GLESFuncs.glDeleteShader(shader);
}

GLboolean IsShader(GLuint shader) {
    return GLContext::Get().IsShader(shader) ? GL_TRUE : GL_FALSE;
}

void ShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {
    // GLSL shader transpilation could be applied here
    CallAndCheckGLES(g_GLESFuncs.glShaderSource(shader, count, string, length));
}

void CompileShader(GLuint shader) {
    CallAndCheckGLES(g_GLESFuncs.glCompileShader(shader));
}

void GetShaderiv(GLuint shader, GLenum pname, GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetShaderiv(shader, pname, params));
}

void GetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
    CallAndCheckGLES(g_GLESFuncs.glGetShaderInfoLog(shader, bufSize, length, infoLog));
}

// =============================================================================
// Program
// =============================================================================

GLuint CreateProgram() {
    auto& glCtx = GLContext::Get();
    GLuint program = g_GLESFuncs.glCreateProgram();
    glCtx.CreateProgram(program);
    return program;
}

void DeleteProgram(GLuint program) {
    auto& glCtx = GLContext::Get();
    glCtx.DeleteProgram(program);
    g_GLESFuncs.glDeleteProgram(program);
}

GLboolean IsProgram(GLuint program) {
    return GLContext::Get().IsProgram(program) ? GL_TRUE : GL_FALSE;
}

void AttachShader(GLuint program, GLuint shader) {
    CallAndCheckGLES(g_GLESFuncs.glAttachShader(program, shader));
}

void DetachShader(GLuint program, GLuint shader) {
    CallAndCheckGLES(g_GLESFuncs.glDetachShader(program, shader));
}

void LinkProgram(GLuint program) {
    CallAndCheckGLES(g_GLESFuncs.glLinkProgram(program));
}

void UseProgram(GLuint program) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    auto prog = glCtx.GetProgram(program);
    stateMgr.SetCurrentProgram(program);

    CallAndCheckGLES(g_GLESFuncs.glUseProgram(program));
}

void GetProgramiv(GLuint program, GLenum pname, GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetProgramiv(program, pname, params));
}

void GetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) {
    CallAndCheckGLES(g_GLESFuncs.glGetProgramInfoLog(program, bufSize, length, infoLog));
}

void ValidateProgram(GLuint program) {
    CallAndCheckGLES(g_GLESFuncs.glValidateProgram(program));
}

void BindAttribLocation(GLuint program, GLuint index, const GLchar* name) {
    CallAndCheckGLES(g_GLESFuncs.glBindAttribLocation(program, index, name));
}

void BindFragDataLocation(GLuint program, GLuint color, const GLchar* name) {
    // GLES uses glBindFragDataLocationEXT or glBindFragDataLocationIndexedEXT
    CallAndCheckGLES(g_GLESFuncs.glBindFragDataLocationEXT(program, color, name));
}

GLint GetAttribLocation(GLuint program, const GLchar* name) {
    return g_GLESFuncs.glGetAttribLocation(program, name);
}

GLint GetFragDataLocation(GLuint program, const GLchar* name) {
    return g_GLESFuncs.glGetFragDataLocationEXT(program, name);
}

GLint GetUniformLocation(GLuint program, const GLchar* name) {
    return g_GLESFuncs.glGetUniformLocation(program, name);
}

void GetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size,
                      GLenum* type, GLchar* name) {
    CallAndCheckGLES(g_GLESFuncs.glGetActiveUniform(program, index, bufSize, length, size, type, name));
}

void GetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size,
                     GLenum* type, GLchar* name) {
    CallAndCheckGLES(g_GLESFuncs.glGetActiveAttrib(program, index, bufSize, length, size, type, name));
}

// =============================================================================
// Uniform setters
// =============================================================================

void Uniform1f(GLint location, GLfloat v0) { g_GLESFuncs.glUniform1f(location, v0); }
void Uniform2f(GLint location, GLfloat v0, GLfloat v1) { g_GLESFuncs.glUniform2f(location, v0, v1); }
void Uniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) { g_GLESFuncs.glUniform3f(location, v0, v1, v2); }
void Uniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) { g_GLESFuncs.glUniform4f(location, v0, v1, v2, v3); }
void Uniform1i(GLint location, GLint v0) { g_GLESFuncs.glUniform1i(location, v0); }
void Uniform2i(GLint location, GLint v0, GLint v1) { g_GLESFuncs.glUniform2i(location, v0, v1); }
void Uniform3i(GLint location, GLint v0, GLint v1, GLint v2) { g_GLESFuncs.glUniform3i(location, v0, v1, v2); }
void Uniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) { g_GLESFuncs.glUniform4i(location, v0, v1, v2, v3); }
void Uniform1ui(GLint location, GLuint v0) { g_GLESFuncs.glUniform1ui(location, v0); }
void Uniform2ui(GLint location, GLuint v0, GLuint v1) { g_GLESFuncs.glUniform2ui(location, v0, v1); }
void Uniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) { g_GLESFuncs.glUniform3ui(location, v0, v1, v2); }
void Uniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) { g_GLESFuncs.glUniform4ui(location, v0, v1, v2, v3); }
void Uniform1fv(GLint location, GLsizei count, const GLfloat* value) { g_GLESFuncs.glUniform1fv(location, count, value); }
void Uniform2fv(GLint location, GLsizei count, const GLfloat* value) { g_GLESFuncs.glUniform2fv(location, count, value); }
void Uniform3fv(GLint location, GLsizei count, const GLfloat* value) { g_GLESFuncs.glUniform3fv(location, count, value); }
void Uniform4fv(GLint location, GLsizei count, const GLfloat* value) { g_GLESFuncs.glUniform4fv(location, count, value); }
void Uniform1iv(GLint location, GLsizei count, const GLint* value) { g_GLESFuncs.glUniform1iv(location, count, value); }
void Uniform2iv(GLint location, GLsizei count, const GLint* value) { g_GLESFuncs.glUniform2iv(location, count, value); }
void Uniform3iv(GLint location, GLsizei count, const GLint* value) { g_GLESFuncs.glUniform3iv(location, count, value); }
void Uniform4iv(GLint location, GLsizei count, const GLint* value) { g_GLESFuncs.glUniform4iv(location, count, value); }
void Uniform1uiv(GLint location, GLsizei count, const GLuint* value) { g_GLESFuncs.glUniform1uiv(location, count, value); }
void Uniform2uiv(GLint location, GLsizei count, const GLuint* value) { g_GLESFuncs.glUniform2uiv(location, count, value); }
void Uniform3uiv(GLint location, GLsizei count, const GLuint* value) { g_GLESFuncs.glUniform3uiv(location, count, value); }
void Uniform4uiv(GLint location, GLsizei count, const GLuint* value) { g_GLESFuncs.glUniform4uiv(location, count, value); }
void UniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { g_GLESFuncs.glUniformMatrix2fv(location, count, transpose, value); }
void UniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { g_GLESFuncs.glUniformMatrix3fv(location, count, transpose, value); }
void UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { g_GLESFuncs.glUniformMatrix4fv(location, count, transpose, value); }
void UniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { g_GLESFuncs.glUniformMatrix2x3fv(location, count, transpose, value); }
void UniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { g_GLESFuncs.glUniformMatrix3x2fv(location, count, transpose, value); }
void UniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { g_GLESFuncs.glUniformMatrix2x4fv(location, count, transpose, value); }
void UniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { g_GLESFuncs.glUniformMatrix4x2fv(location, count, transpose, value); }
void UniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { g_GLESFuncs.glUniformMatrix3x4fv(location, count, transpose, value); }
void UniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) { g_GLESFuncs.glUniformMatrix4x3fv(location, count, transpose, value); }

// Uniform block
GLuint GetUniformBlockIndex(GLuint program, const GLchar* uniformBlockName) {
    return g_GLESFuncs.glGetUniformBlockIndex(program, uniformBlockName);
}
void UniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) {
    CallAndCheckGLES(g_GLESFuncs.glUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding));
}
void GetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params));
}
void GetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length,
                               GLchar* uniformBlockName) {
    CallAndCheckGLES(g_GLESFuncs.glGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName));
}

// Program pipeline
void BindProgramPipeline(GLuint pipeline) {
    CallAndCheckGLES(g_GLESFuncs.glBindProgramPipeline(pipeline));
}
void GenProgramPipelines(GLsizei n, GLuint* pipelines) {
    CallAndCheckGLES(g_GLESFuncs.glGenProgramPipelines(n, pipelines));
}
void DeleteProgramPipelines(GLsizei n, const GLuint* pipelines) {
    CallAndCheckGLES(g_GLESFuncs.glDeleteProgramPipelines(n, pipelines));
}
void UseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program) {
    CallAndCheckGLES(g_GLESFuncs.glUseProgramStages(pipeline, stages, program));
}

} // namespace MobileGL::impl::GLImpl