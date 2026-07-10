// MobileGlues - gl/impl/Drawing/GL_Drawing.cpp
// GL Drawing implementation - Core Profile → GLES 3.2
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "GL_Drawing.h"
#include "../../backend/DirectGLES/DirectGLES.h"
#include "../../backend/DirectGLES/Managers.h"
#include "../../state/Core.h"
#include "../../state.h"

using namespace MobileGL::backend::DirectGLES;
using namespace MobileGL::MG_State::GLState;

namespace MobileGL::impl::GLImpl {

// =============================================================================
// Internal helpers
// =============================================================================

// GL call overhead optimizations applied (MobileGL-DirectGLES reference):
//   - CallAndCheckGLES: periodic error check every 64 calls (DirectGLES.h)
//   - glGetUniformLocation / glGetUniformBlockIndex: cached per-program (Managers.h)
//   - StateBackendObjectRegistry::GetOrCreate/find: inlined erase, single lookup (Managers.h)
//   - SyncToBackend: clears cached queries on re-link (Managers.h ClearCachedQueries)
//
// TODO: When backend state sync methods are implemented, apply these optimizations:
//   - Texture dirty version tracking (XOR binding slot versions per unit)
//   - Double-lookup elimination (use GetOrCreate directly instead of find+GetOrCreate)
//   - currentVAO caching (move GetBoundVertexArray outside inner scope)

static void SyncAllStateForDraw() {
    // Sync VAO → backend
    // Sync program → backend (use GetCachedUniformLocation/GetCachedUniformBlockIndex)
    // Sync textures → backend (with dirty version tracking)
    // Sync samplers → backend
    // Sync uniforms → backend
    // Sync render state → backend
    // Sync framebuffer → backend
}

static void SyncVAOForDraw(const SharedPtr<VertexArrayObject>& vao) {
    if (!vao) return;
    auto& backendVAO = VertexArrayImpl::g_backendVertexArrayObjects.GetOrCreate(vao);
    backendVAO.SyncToBackend(vao);
}

static void SyncProgramForDraw(const SharedPtr<ProgramObject>& program) {
    if (!program) return;
    auto& backendProg = ProgramImpl::g_backendProgramObjects.GetOrCreate(program);
    backendProg.ClearCachedQueries();
    backendProg.SyncToBackend(program);
    backendProg.Use();
}

static void SyncTexturesForDraw() {
    // Sync all active texture units to backend
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();
    for (Uint i = 0; i < stateMgr.GetTextureState().GetMaxTextureImageUnits(); ++i) {
        auto tex = stateMgr.GetTextureState().GetBoundTexture(i, TextureTarget::Texture2D);
        if (tex) {
            TextureImpl::SyncTextureObjectToBackend(tex);
        }
    }
}

// =============================================================================
// Draw Commands
// =============================================================================

void DrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (count <= 0) return;

    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    SyncAllStateForDraw();

    CallAndCheckGLES(g_GLESFuncs.glDrawArrays(mode, first, count));
}

void DrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    if (count <= 0) return;

    auto& glCtx = GLContext::Get();
    SyncAllStateForDraw();

    CallAndCheckGLES(g_GLESFuncs.glDrawElements(mode, count, type, indices));
}

void DrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices, GLint basevertex) {
    if (count <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDrawElementsBaseVertex(mode, count, type, indices, basevertex));
}

void MultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const GLvoid* const* indices,
                       GLsizei drawcount) {
    if (drawcount <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glMultiDrawElementsEXT(mode, count, type, indices, drawcount));
}

void MultiDrawElementsBaseVertex(GLenum mode, const GLsizei* count, GLenum type, const GLvoid* const* indices,
                                 GLsizei drawcount, const GLint* basevertex) {
    if (drawcount <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glMultiDrawElementsBaseVertexEXT(mode, count, type, indices, drawcount, basevertex));
}

void MultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride) {
    if (drawcount <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glMultiDrawElementsIndirectEXT(mode, type, indirect, drawcount, stride));
}

void MultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride) {
    if (drawcount <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glMultiDrawArraysIndirectEXT(mode, indirect, drawcount, stride));
}

void MultiDrawElementsIndirectCount(GLenum mode, GLenum type, const void* indirect, GLintptr drawcount,
                                    GLsizei maxdrawcount, GLsizei stride) {
    if (drawcount <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glMultiDrawElementsIndirectCountEXT(mode, type, indirect, drawcount, maxdrawcount, stride));
}

void DrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type,
                                 const void* indices, GLint basevertex) {
    if (count <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDrawRangeElementsBaseVertexEXT(mode, start, end, count, type, indices, basevertex));
}

void DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices) {
    if (count <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDrawRangeElements(mode, start, end, count, type, indices));
}

void DrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices,
                                                 GLsizei instancecount, GLint basevertex, GLuint baseinstance) {
    if (count <= 0 || instancecount <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDrawElementsInstancedBaseVertexBaseInstanceEXT(mode, count, type, indices,
                                                                                   instancecount, basevertex, baseinstance));
}

void DrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void* indices,
                                     GLsizei instancecount, GLint basevertex) {
    if (count <= 0 || instancecount <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDrawElementsInstancedBaseVertexEXT(mode, count, type, indices, instancecount, basevertex));
}

void DrawElementsInstancedBaseInstance(GLenum mode, GLsizei count, GLenum type, const void* indices,
                                       GLsizei instancecount, GLuint baseinstance) {
    if (count <= 0 || instancecount <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDrawElementsInstancedBaseInstanceEXT(mode, count, type, indices, instancecount, baseinstance));
}

void DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount) {
    if (count <= 0 || instancecount <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDrawElementsInstanced(mode, count, type, indices, instancecount));
}

void DrawElementsIndirect(GLenum mode, GLenum type, const void* indirect) {
    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDrawElementsIndirect(mode, type, indirect));
}

void DrawArraysInstancedBaseInstance(GLenum mode, GLint first, GLsizei count, GLsizei instancecount,
                                     GLuint baseinstance) {
    if (count <= 0 || instancecount <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDrawArraysInstancedBaseInstanceEXT(mode, first, count, instancecount, baseinstance));
}

void DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) {
    if (count <= 0 || instancecount <= 0) return;

    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDrawArraysInstanced(mode, first, count, instancecount));
}

void DrawArraysIndirect(GLenum mode, const void* indirect) {
    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDrawArraysIndirect(mode, indirect));
}

void Clear(GLbitfield mask) {
    auto& glCtx = GLContext::Get();
    // Sync framebuffer state before clear
    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glClear(mask));
}

void DispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ) {
    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDispatchCompute(numGroupsX, numGroupsY, numGroupsZ));
}

void DispatchComputeIndirect(GLintptr indirect) {
    SyncAllStateForDraw();
    CallAndCheckGLES(g_GLESFuncs.glDispatchComputeIndirect(indirect));
}

void MemoryBarrier(GLbitfield barriers) {
    CallAndCheckGLES(g_GLESFuncs.glMemoryBarrier(barriers));
}

void MemoryBarrierByRegion(GLbitfield barriers) {
    CallAndCheckGLES(g_GLESFuncs.glMemoryBarrierByRegion(barriers));
}

} // namespace MobileGL::impl::GLImpl