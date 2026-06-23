// MobileGlues - init.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

// ============================================================================
// Initialization Order (ES 3.2 target)
//
//  1. proc_init()     — main.cpp: config, logging, load libs, EGL, GLES
//  2. init_emulation_state() — mg.cpp: immediate mode + matrix stack emulation
//
//  init_emulation_state() must run AFTER proc_init() because:
//    - proc_init() calls init_target_gles(), which allocates gl_state via
//      init_gl_state() and loads all ES 3.2 function pointers.
//    - init_emulation_state() writes into gl_state->immediate and
//      gl_state->matrix, so gl_state must already exist.
// ============================================================================

#include "includes.h"
#include "gl/mg.h"

struct static_block_t {
    static_block_t() {
        proc_init();
        // After GL state is initialized by proc_init() → init_target_gles() →
        // init_gl_state(), set up the emulation state for legacy GL features
        // that are not natively supported by ES 3.2 (immediate mode, matrix stack).
        init_emulation_state();
    }
};

static static_block_t static_block;