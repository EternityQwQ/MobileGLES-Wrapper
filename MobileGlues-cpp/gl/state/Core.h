// MobileGlues - gl/state/Core.h
// GL State Core - Central state management for GL Core Profile → GLES 3.2
// Architecture inspired by MobileGL-DirectGLES
//
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#pragma once

#include "../../includes.h"
#include "../../state.h"

namespace MobileGL::MG_State::GLState {

// Forward declarations
class GLContext;
class BufferObject;
class VertexArrayObject;
class ITextureObject;
class FramebufferObject;
class ProgramObject;
class SamplerObject;
class RenderbufferObject;

// Version tracking for dirty-checking
using VersionType = Uint16;

// =============================================================================
// Core GL Context
// =============================================================================

class GLContext {
public:
    GLContext();
    ~GLContext();

    static GLContext& Get();

    // Object management
    void CreateBuffer(GLuint id);
    void CreateVertexArray(GLuint id);
    void CreateTexture(GLuint id);
    void CreateFramebuffer(GLuint id);
    void CreateProgram(GLuint id);
    void CreateShader(GLuint id);
    void CreateSampler(GLuint id);
    void CreateRenderbuffer(GLuint id);

    void DeleteBuffer(GLuint id);
    void DeleteVertexArray(GLuint id);
    void DeleteTexture(GLuint id);
    void DeleteFramebuffer(GLuint id);
    void DeleteProgram(GLuint id);
    void DeleteShader(GLuint id);
    void DeleteSampler(GLuint id);
    void DeleteRenderbuffer(GLuint id);

    // Object lookup
    SharedPtr<BufferObject> GetBuffer(GLuint id);
    SharedPtr<VertexArrayObject> GetVertexArray(GLuint id);
    SharedPtr<ITextureObject> GetTexture(GLuint id);
    SharedPtr<FramebufferObject> GetFramebuffer(GLuint id);
    SharedPtr<ProgramObject> GetProgram(GLuint id);
    SharedPtr<SamplerObject> GetSampler(GLuint id);
    SharedPtr<RenderbufferObject> GetRenderbuffer(GLuint id);

    Bool IsBuffer(GLuint id) const;
    Bool IsVertexArray(GLuint id) const;
    Bool IsTexture(GLuint id) const;
    Bool IsFramebuffer(GLuint id) const;
    Bool IsProgram(GLuint id) const;
    Bool IsShader(GLuint id) const;
    Bool IsSampler(GLuint id) const;
    Bool IsRenderbuffer(GLuint id) const;

    // State management
    GLStateManager& GetStateManager() { return m_stateManager; }
    const GLStateManager& GetStateManager() const { return m_stateManager; }

    // Version tracking for dirty-checking
    VersionType IncrementVersion();
    VersionType GetVersion() const;

private:
    GLStateManager m_stateManager;
    VersionType m_version = 0;

    // Object registries
    UnorderedMap<GLuint, SharedPtr<BufferObject>> m_buffers;
    UnorderedMap<GLuint, SharedPtr<VertexArrayObject>> m_vertexArrays;
    UnorderedMap<GLuint, SharedPtr<ITextureObject>> m_textures;
    UnorderedMap<GLuint, SharedPtr<FramebufferObject>> m_framebuffers;
    UnorderedMap<GLuint, SharedPtr<ProgramObject>> m_programs;
    UnorderedMap<GLuint, SharedPtr<SamplerObject>> m_samplers;
    UnorderedMap<GLuint, SharedPtr<RenderbufferObject>> m_renderbuffers;
    UnorderedMap<GLuint, GLenum> m_shaders;
};

// =============================================================================
// Buffer Object
// =============================================================================

class BufferObject {
public:
    BufferObject(GLuint id = 0);
    ~BufferObject();

    GLuint GetId() const { return m_id; }

    void SetData(GLsizeiptr size, const void* data, GLenum usage);
    void SetSubData(GLintptr offset, GLsizeiptr size, const void* data);
    void SetStorage(GLsizeiptr size, const void* data, GLbitfield flags);

    void* Map(GLenum access);
    void* MapRange(GLintptr offset, GLsizeiptr length, GLbitfield access);
    Bool Unmap();
    void FlushMappedRange(GLintptr offset, GLsizeiptr length);

    GLsizeiptr GetSize() const { return m_size; }
    GLenum GetUsage() const { return m_usage; }
    GLbitfield GetStorageFlags() const { return m_storageFlags; }
    Bool IsMapped() const { return m_mapped; }
    void* GetMappedPointer() const { return m_mappedPtr; }
    Bool IsImmutable() const { return m_immutable; }

    VersionType GetVersion() const { return m_version; }
    void IncrementVersion() { m_version++; }

private:
    GLuint m_id = 0;
    GLsizeiptr m_size = 0;
    GLenum m_usage = GL_STATIC_DRAW;
    GLbitfield m_storageFlags = 0;
    Bool m_immutable = false;
    Bool m_mapped = false;
    void* m_mappedPtr = nullptr;
    GLsizeiptr m_mappedOffset = 0;
    GLsizeiptr m_mappedLength = 0;
    GLbitfield m_mappedAccess = 0;
    VersionType m_version = 0;
};

// =============================================================================
// Vertex Array Object
// =============================================================================

class VertexArrayObject {
public:
    static constexpr SizeT MAX_VERTEX_ATTRIBS = 16;
    static constexpr SizeT MAX_VERTEX_BINDINGS = 16;

    struct VertexAttribute {
        Bool enabled = false;
        GLint size = 4;
        GLenum type = GL_FLOAT;
        Bool normalized = false;
        Bool isInteger = false;
        Bool isLong = false;
        GLuint relativeOffset = 0;
        GLuint bindingIndex = 0;
        GLuint divisor = 0;
        VersionType version = 0;
    };

    struct VertexBinding {
        GLuint buffer = 0;
        GLintptr offset = 0;
        GLsizeiptr stride = 0;
        VersionType version = 0;
    };

    VertexArrayObject(GLuint id = 0);
    ~VertexArrayObject();

    GLuint GetId() const { return m_id; }

    void SetElementBuffer(GLuint buffer);
    GLuint GetElementBuffer() const { return m_elementBuffer; }

    VertexAttribute& GetAttribute(GLuint index);
    VertexBinding& GetBinding(GLuint index);
    const VertexAttribute& GetAttribute(GLuint index) const;
    const VertexBinding& GetBinding(GLuint index) const;

    VersionType GetVersion() const { return m_version; }
    void IncrementVersion() { m_version++; }

private:
    GLuint m_id = 0;
    GLuint m_elementBuffer = 0;
    Array<VertexAttribute, MAX_VERTEX_ATTRIBS> m_attributes;
    Array<VertexBinding, MAX_VERTEX_BINDINGS> m_bindings;
    VersionType m_version = 0;
};

using VertexAttributeVersion = VersionType;

} // namespace MobileGL::MG_State::GLState