// MobileGlues - gl/vertexattrib.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

// ============================================================================
// Vertex Attribute Architecture
//
// Principle: "ES 3.2 native → native, ES 3.2 not native → CPU simulation"
//
// ES 3.2 natively supports:
//   - glVertexAttribI4ui, glVertexAttribI4i (integer vertex attributes with 4 components)
//
// ES 3.2 does NOT natively support:
//   - glVertexAttribI1ui (1-component unsigned integer vertex attribute)
//   - glVertexAttribI2ui (2-component unsigned integer vertex attribute)
//   - glVertexAttribI3ui (3-component unsigned integer vertex attribute)
//     → CPU simulation: expand to I4ui variants by padding with zeros
// ============================================================================

#include "vertexattrib.h"

#define DEBUG 0

// ============================================================================
// Section: CPU Simulation — Integer Vertex Attribute Expansion
//
// These functions simulate 1/2/3-component unsigned integer vertex attributes
// by expanding them to the 4-component variant (I4ui) that ES 3.2 supports
// natively. Missing components are padded with zero.
// ============================================================================

// ---------------------------------------------------------------------------
// glVertexAttribI1ui — 1-component uint → expanded to I4ui (x, 0, 0, 0)
// ---------------------------------------------------------------------------
void glVertexAttribI1ui(GLuint index, GLuint x) {
    LOG()
    GLES.glVertexAttribI4ui(index, x, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// glVertexAttribI2ui — 2-component uint → expanded to I4ui (x, y, 0, 0)
// ---------------------------------------------------------------------------
void glVertexAttribI2ui(GLuint index, GLuint x, GLuint y) {
    LOG()
    GLES.glVertexAttribI4ui(index, x, y, 0, 0);
}

// ---------------------------------------------------------------------------
// glVertexAttribI3ui — 3-component uint → expanded to I4ui (x, y, z, 0)
// ---------------------------------------------------------------------------
void glVertexAttribI3ui(GLuint index, GLuint x, GLuint y, GLuint z) {
    LOG()
    GLES.glVertexAttribI4ui(index, x, y, z, 0);
}