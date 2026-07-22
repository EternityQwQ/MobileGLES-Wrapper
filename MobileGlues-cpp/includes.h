// MobileGLES - includes.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#ifndef MOBILEGLES_INCLUDES_H
#define MOBILEGLES_INCLUDES_H

// Compile-time enforcement of the iOS-only target. MobileGLES refuses to
// build for any non-iOS Apple platform (macOS desktop, tvOS, watchOS, ...) in
// addition to the CMake-level platform check. This keeps the iOS-specific
// ANGLE/Metal assumptions valid everywhere in the codebase.
#include <TargetConditionals.h>
#if !defined(TARGET_OS_IPHONE) || !TARGET_OS_IPHONE
#error "MobileGLES is iOS-only; this build targets a non-iOS platform."
#endif

#define RENDERERNAME "MobileGLES"

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <MG/extensions.h>

#include "egl/egl.h"
#include "egl/loader.h"

#ifdef __cplusplus
extern "C"
{
#endif

    static int g_initialized = 0;

    void proc_init();

#ifdef __cplusplus
}
#endif

#include <FastSTL/UnorderedMap.h>

template <typename Key, typename T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>>
using UnorderedMap = FastSTL::unordered_map<Key, T, Hash, KeyEqual, Allocator>;

#endif // MOBILEGLES_INCLUDES_H
