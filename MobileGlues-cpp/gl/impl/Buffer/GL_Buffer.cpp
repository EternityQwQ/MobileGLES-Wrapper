// MobileGlues - gl/impl/Buffer/GL_Buffer.cpp
// GL Buffer implementation - Core Profile → GLES 3.2
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "GL_Buffer.h"
#include "../../backend/DirectGLES/DirectGLES.h"
#include "../../backend/DirectGLES/Managers.h"
#include "../../state/Core.h"
#include "../../state.h"

using namespace MobileGL::backend::DirectGLES;
using namespace MobileGL::MG_State::GLState;

namespace MobileGL::impl::GLImpl {

void GenBuffers(GLsizei n, GLuint* buffers) {
    auto& glCtx = GLContext::Get();
    for (GLsizei i = 0; i < n; ++i) {
        glCtx.CreateBuffer(buffers[i]);
    }
}

void DeleteBuffers(GLsizei n, const GLuint* buffers) {
    auto& glCtx = GLContext::Get();
    for (GLsizei i = 0; i < n; ++i) {
        glCtx.DeleteBuffer(buffers[i]);
    }
}

GLboolean IsBuffer(GLuint buffer) {
    return GLContext::Get().IsBuffer(buffer) ? GL_TRUE : GL_FALSE;
}

void BindBuffer(GLenum target, GLuint buffer) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    auto buf = glCtx.GetBuffer(buffer);
    if (target == GL_ELEMENT_ARRAY_BUFFER) {
        stateMgr.SetElementArrayBuffer(buffer);
    } else if (target == GL_ARRAY_BUFFER) {
        stateMgr.SetArrayBuffer(buffer);
    } else if (target == GL_UNIFORM_BUFFER) {
        stateMgr.SetUniformBuffer(buffer);
    } else if (target == GL_COPY_READ_BUFFER) {
        stateMgr.SetCopyReadBuffer(buffer);
    } else if (target == GL_COPY_WRITE_BUFFER) {
        stateMgr.SetCopyWriteBuffer(buffer);
    } else if (target == GL_PIXEL_PACK_BUFFER) {
        stateMgr.SetPixelPackBuffer(buffer);
    } else if (target == GL_PIXEL_UNPACK_BUFFER) {
        stateMgr.SetPixelUnpackBuffer(buffer);
    } else if (target == GL_TRANSFORM_FEEDBACK_BUFFER) {
        stateMgr.SetTransformFeedbackBuffer(buffer);
    }

    if (buf) {
        auto& backendBuf = BufferImpl::g_backendBufferObjects.GetOrCreate(buf);
        backendBuf.Bind(target);
    }
}

void BufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    GLuint bufferId = stateMgr.GetCurrentBufferForTarget(target);
    auto buf = glCtx.GetBuffer(bufferId);
    if (!buf) return;

    buf->SetData(size, data, usage);

    auto& backendBuf = BufferImpl::g_backendBufferObjects.GetOrCreate(buf);
    backendBuf.SyncToBackend(buf);
}

void BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    GLuint bufferId = stateMgr.GetCurrentBufferForTarget(target);
    auto buf = glCtx.GetBuffer(bufferId);
    if (!buf) return;

    buf->SetSubData(offset, size, data);

    auto& backendBuf = BufferImpl::g_backendBufferObjects.GetOrCreate(buf);
    backendBuf.SyncToBackend(buf);
}

void CopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset,
                       GLsizeiptr size) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    GLuint readBufferId = stateMgr.GetCurrentBufferForTarget(readTarget);
    GLuint writeBufferId = stateMgr.GetCurrentBufferForTarget(writeTarget);

    auto readBuf = glCtx.GetBuffer(readBufferId);
    auto writeBuf = glCtx.GetBuffer(writeBufferId);
    if (!readBuf || !writeBuf) return;

    // Use backend glCopyBufferSubData
    CallAndCheckGLES(g_GLESFuncs.glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size));
}

void BufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    GLuint bufferId = stateMgr.GetCurrentBufferForTarget(target);
    auto buf = glCtx.GetBuffer(bufferId);
    if (!buf) return;

    buf->SetStorage(size, data, flags);

    auto& backendBuf = BufferImpl::g_backendBufferObjects.GetOrCreate(buf);
    backendBuf.SyncToBackend(buf);
}

void* MapBuffer(GLenum target, GLenum access) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    GLuint bufferId = stateMgr.GetCurrentBufferForTarget(target);
    auto buf = glCtx.GetBuffer(bufferId);
    if (!buf) return nullptr;

    return buf->Map(access);
}

void* MapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    GLuint bufferId = stateMgr.GetCurrentBufferForTarget(target);
    auto buf = glCtx.GetBuffer(bufferId);
    if (!buf) return nullptr;

    return buf->MapRange(offset, length, access);
}

GLboolean UnmapBuffer(GLenum target) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    GLuint bufferId = stateMgr.GetCurrentBufferForTarget(target);
    auto buf = glCtx.GetBuffer(bufferId);
    if (!buf) return GL_FALSE;

    return buf->Unmap() ? GL_TRUE : GL_FALSE;
}

void FlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    GLuint bufferId = stateMgr.GetCurrentBufferForTarget(target);
    auto buf = glCtx.GetBuffer(bufferId);
    if (!buf) return;

    buf->FlushMappedRange(offset, length);
}

void BindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    auto buf = glCtx.GetBuffer(buffer);
    stateMgr.SetIndexedBufferBinding(target, index, buffer);

    CallAndCheckGLES(g_GLESFuncs.glBindBufferBase(target, index, buffer));
}

void BindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    auto& glCtx = GLContext::Get();
    auto& stateMgr = glCtx.GetStateManager();

    auto buf = glCtx.GetBuffer(buffer);
    stateMgr.SetIndexedBufferBinding(target, index, buffer);

    CallAndCheckGLES(g_GLESFuncs.glBindBufferRange(target, index, buffer, offset, size));
}

void GetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetBufferParameteriv(target, pname, params));
}

void GetBufferParameteri64v(GLenum target, GLenum pname, GLint64* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetBufferParameteri64v(target, pname, params));
}

void GetBufferPointerv(GLenum target, GLenum pname, void** params) {
    CallAndCheckGLES(g_GLESFuncs.glGetBufferPointerv(target, pname, params));
}

// DSA buffer functions
void CreateBuffers(GLsizei n, GLuint* buffers) {
    GenBuffers(n, buffers);
}

void NamedBufferStorage(GLuint buffer, GLsizeiptr size, const void* data, GLbitfield flags) {
    auto& glCtx = GLContext::Get();
    auto buf = glCtx.GetBuffer(buffer);
    if (!buf) return;

    buf->SetStorage(size, data, flags);

    auto& backendBuf = BufferImpl::g_backendBufferObjects.GetOrCreate(buf);
    backendBuf.SyncToBackend(buf);
}

void NamedBufferData(GLuint buffer, GLsizeiptr size, const void* data, GLenum usage) {
    auto& glCtx = GLContext::Get();
    auto buf = glCtx.GetBuffer(buffer);
    if (!buf) return;

    buf->SetData(size, data, usage);

    auto& backendBuf = BufferImpl::g_backendBufferObjects.GetOrCreate(buf);
    backendBuf.SyncToBackend(buf);
}

void NamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data) {
    auto& glCtx = GLContext::Get();
    auto buf = glCtx.GetBuffer(buffer);
    if (!buf) return;

    buf->SetSubData(offset, size, data);

    auto& backendBuf = BufferImpl::g_backendBufferObjects.GetOrCreate(buf);
    backendBuf.SyncToBackend(buf);
}

void CopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset,
                            GLsizeiptr size) {
    auto& glCtx = GLContext::Get();
    auto readBuf = glCtx.GetBuffer(readBuffer);
    auto writeBuf = glCtx.GetBuffer(writeBuffer);
    if (!readBuf || !writeBuf) return;

    CallAndCheckGLES(g_GLESFuncs.glCopyNamedBufferSubData(readBuffer, writeBuffer, readOffset, writeOffset, size));
}

void ClearNamedBufferData(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void* data) {
    CallAndCheckGLES(g_GLESFuncs.glClearNamedBufferData(buffer, internalformat, format, type, data));
}

void ClearNamedBufferSubData(GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format,
                             GLenum type, const void* data) {
    CallAndCheckGLES(g_GLESFuncs.glClearNamedBufferSubData(buffer, internalformat, offset, size, format, type, data));
}

void* MapNamedBuffer(GLuint buffer, GLenum access) {
    auto& glCtx = GLContext::Get();
    auto buf = glCtx.GetBuffer(buffer);
    if (!buf) return nullptr;
    return buf->Map(access);
}

void* MapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    auto& glCtx = GLContext::Get();
    auto buf = glCtx.GetBuffer(buffer);
    if (!buf) return nullptr;
    return buf->MapRange(offset, length, access);
}

GLboolean UnmapNamedBuffer(GLuint buffer) {
    auto& glCtx = GLContext::Get();
    auto buf = glCtx.GetBuffer(buffer);
    if (!buf) return GL_FALSE;
    return buf->Unmap() ? GL_TRUE : GL_FALSE;
}

void FlushMappedNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length) {
    auto& glCtx = GLContext::Get();
    auto buf = glCtx.GetBuffer(buffer);
    if (!buf) return;
    buf->FlushMappedRange(offset, length);
}

void GetNamedBufferParameteriv(GLuint buffer, GLenum pname, GLint* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetNamedBufferParameteriv(buffer, pname, params));
}

void GetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64* params) {
    CallAndCheckGLES(g_GLESFuncs.glGetNamedBufferParameteri64v(buffer, pname, params));
}

void GetNamedBufferPointerv(GLuint buffer, GLenum pname, void** params) {
    CallAndCheckGLES(g_GLESFuncs.glGetNamedBufferPointerv(buffer, pname, params));
}

} // namespace MobileGL::impl::GLImpl