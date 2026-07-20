# MobileGLES

**MobileGLES** is the iOS-only fork of MobileGlues ("(on) Mobile, GL uses ES"). It is a GL implementation running on top of host OpenGL ES 3.x (best on 3.2, minimum 3.0), with running Minecraft: Java Edition in mind.

> **Platform scope**: MobileGLES targets **iOS exclusively**. It refuses to build or run on any other platform (Android, Linux, macOS desktop, Windows). It relies on the prebuilt ANGLE frameworks (`libEGL.framework`, `libGLESv2.framework`) built atop Metal, which are statically linked into the host application.

# For Shader Developers

1. MobileGLES automatically:
   - Converts desktop GLSL → GLSL ES
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
   - Enable `Ignore shader/program error`, and check the logs (located at `$HOME/Documents/MG/latest.log` inside the iOS app container).

# License

MobileGLES is licensed under **GNU LGPL-2.1 License**.

Please see [LICENSE](https://github.com/MobileGL-Dev/MobileGLES/blob/main/LICENSE).

# Third-party components

**SPIRV-Cross** by **KhronosGroup** - [Apache License 2.0](https://github.com/KhronosGroup/SPIRV-Cross/blob/master/LICENSE): [github](https://github.com/KhronosGroup/SPIRV-Cross)

**glslang** by **KhronosGroup** - [Various Licenses](https://github.com/KhronosGroup/glslang/blob/main/LICENSE.txt): [github](https://github.com/KhronosGroup/glslang)

**cJSON** by **DaveGamble** - [MIT License](https://github.com/DaveGamble/cJSON/blob/master/LICENSE): [github](https://github.com/DaveGamble/cJSON)

**OpenGL Mathematics (*GLM*)** by **G-Truc Creation** - [The Happy Bunny License](https://github.com/g-truc/glm/blob/master/copying.txt): [github](https://github.com/g-truc/glm)

**FidelityFX-FSR** by **AMD** - [MIT License](https://github.com/GPUOpen-Effects/FidelityFX-FSR/blob/master/license.txt): [github](https://github.com/GPUOpen-Effects/FidelityFX-FSR)

**xxHash** by **Yann Collet** - [BSD 2-Clause License](https://github.com/Cyan4973/xxHash/blob/dev/LICENSE): [github](https://github.com/Cyan4973/xxHash)

**ANGLE** by **Google** - [BSD 3-Clause License](https://chromium.googlesource.com/angle/angle/+/refs/heads/main/LICENSE): [chromium](https://chromium.googlesource.com/angle/angle/)
