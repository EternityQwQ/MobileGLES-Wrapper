# MobileGLES

> **Unofficial fork.** MobileGLES is an **unofficial** community fork of the
> [MobileGlues](https://github.com/MobileGL-Dev/MobileGlues) project
> ("(on) Mobile, GL uses ES"). It is not affiliated with, endorsed by, or
> maintained by the upstream MobileGlues team. All MobileGLES-specific changes
> are the responsibility of this fork's contributors.

MobileGLES is a desktop OpenGL compatibility layer that runs on top of a host
**OpenGL ES 3.0** backend, with running Minecraft: Java Edition in mind. It
translates desktop GL / GLSL calls into OpenGL ES 3.0 / GLSL ES 3.00 using
glslang + SPIRV-Cross, so that Java Edition's OpenGL renderer can run on
mobile-class GPUs.

## OpenGL ES 3.0 only

MobileGLES targets **OpenGL ES 3.0 exclusively**.

- **GLES 3.1 and GLES 3.2 support have been removed.** The host backend version
  reported to the layer is clamped to 3.0, and every version-branching code path
  takes the GLES 3.0 route.
- Features that are not part of core OpenGL ES 3.0 (compute shaders, SSBO-only
  paths, texture buffers, core `glDrawElementsBaseVertex`, etc.) are either
  **emulated in software** or used through **EXT/OES extensions** when the host
  driver exposes them — never through a 3.1/3.2 core dependency.
- The shading language version exposed to the application always reflects the
  GLES 3.0 baseline (`#version 300 es` / GLSL `4.00`).
- A host that only advertises OpenGL ES 3.0 is fully supported; a host that
  advertises 3.1/3.2 is treated as 3.0 by the layer.

If the host reports a version lower than 3.0, MobileGLES logs that the version
is unsupported and refuses to make use of unavailable features.

## Platform scope: iOS exclusively

MobileGLES targets **iOS exclusively**. It refuses to build or run on any other
platform (Android, Linux, macOS desktop, Windows):

- The CMake configuration emits a fatal error on non-Apple platforms.
- A compile-time guard (`TARGET_OS_IPHONE`) rejects non-iOS Apple targets such
  as macOS desktop.
- It relies on the prebuilt **ANGLE** frameworks (`libEGL.framework`,
  `libGLESv2.framework`) built atop **Metal**, which are statically linked into
  the host iOS application. There is no `dlopen` at runtime — EGL/GLES symbols
  are resolved through the main executable image.

### iOS build optimizations

The iOS build applies a number of platform-specific optimizations:

- **ThinLTO** (link-time optimization) when the toolchain supports it, falling
  back to full IPO.
- `-O2` as the baseline, plus `-ffunction-sections -fdata-sections` together
  with `-Wl,-dead_strip` to strip unreachable code/data and shrink the embedded
  dylib.
- `-fvisibility-inlines-hidden` and `-fomit-frame-pointer` for smaller, faster
  arm64 code.
- An explicit **GLES 3.0 EGL context** (`EGL_OPENGL_ES3_BIT`,
  `EGL_CONTEXT_CLIENT_VERSION = 3`) instead of an ES2 context.

## For shader developers

1. MobileGLES automatically:
   - Converts desktop GLSL → GLSL ES 3.00
   - Removes `layout(binding)` syntax
   - Handles version directives
   - Always declare precision explicitly:
     ```glsl
     precision highp float;
     precision highp int;
     ```

2. MobileGLES injects these macros into your shaders:
   ```glsl
   #define MG_MOBILEGLES                   // Indicates MobileGLES environment
   #define MG_MOBILEGLES_VERSION 1000      // Version number (e.g. 1000 = V1.0.0)
   ```

3. If encountering issues:
   - Enable `Ignore shader/program error`, and check the logs (located at
     `$HOME/Documents/MG/latest.log` inside the iOS app container).

## License

MobileGLES is licensed under the **GNU LGPL-2.1 License**.

Please see [LICENSE](LICENSE).

## Third-party components

**SPIRV-Cross** by **KhronosGroup** - [Apache License 2.0](https://github.com/KhronosGroup/SPIRV-Cross/blob/master/LICENSE): [github](https://github.com/KhronosGroup/SPIRV-Cross)

**glslang** by **KhronosGroup** - [Various Licenses](https://github.com/KhronosGroup/glslang/blob/main/LICENSE.txt): [github](https://github.com/KhronosGroup/glslang)

**cJSON** by **DaveGamble** - [MIT License](https://github.com/DaveGamble/cJSON/blob/master/LICENSE): [github](https://github.com/DaveGamble/cJSON)

**OpenGL Mathematics (*GLM*)** by **G-Truc Creation** - [The Happy Bunny License](https://github.com/g-truc/glm/blob/master/copying.txt): [github](https://github.com/g-truc/glm)

**FidelityFX-FSR** by **AMD** - [MIT License](https://github.com/GPUOpen-Effects/FidelityFX-FSR/blob/master/license.txt): [github](https://github.com/GPUOpen-Effects/FidelityFX-FSR)

**xxHash** by **Yann Collet** - [BSD 2-Clause License](https://github.com/Cyan4973/xxHash/blob/dev/LICENSE): [github](https://github.com/Cyan4973/xxHash)

**ANGLE** by **Google** - [BSD 3-Clause License](https://chromium.googlesource.com/angle/angle/+/refs/heads/main/LICENSE): [chromium](https://chromium.googlesource.com/angle/angle/)

---

*MobileGLES is an unofficial fork and is not affiliated with Mojang, Microsoft,
or the upstream MobileGlues project. Minecraft: Java Edition is a trademark of
Mojang Synergies AB.*
