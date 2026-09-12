// MobileGlues - gl/multidraw.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
//
// ===========================================================================
// Multi-draw on GLES 3.2
// ===========================================================================
//
// GLES 3.2 has no multi-draw command of any kind. Not even the singular
// glMultiDrawArrays: 3.2 added glDrawElementsBaseVertex and nothing batched.
// Every batched form below therefore comes from an extension, and this file
// exists to serve the GL 4.6 core multi-draw family on top of whichever subset
// of those extensions the current driver actually implements -- degrading to a
// plain loop when it implements none of them.
//
// The principle is unchanged from the original implementation, and is worth
// stating up front because the file is long:
//
//   1. Each GL 4.6 entry point is *defined once* and picks a backend from
//      global_settings.multidraw_order, the user's per-entry-point preference
//      list resolved against device capability by config/settings.cpp.
//
//   2. A backend that discovers mid-call that it cannot serve the call does not
//      silently drop geometry and does not jump to a hard-coded neighbour: it
//      routes through md_fall_*(), which walks strictly forward through the
//      user's order. The walk terminates because the terminal rung is always
//      the unrolled loop.
//
//   3. Backends whose extension entry point may be a driver stub are
//      *probe-and-latch*: the first call is bracketed by an error probe and a
//      failure latches that backend off for the rest of the context, so a bad
//      driver costs one draw rather than every draw.
//
//   4. Cached GL object names are invalidated wholesale when the current
//      context changes (multidraw_check_context), because a name created in one
//      context is meaningless in another.
//
// The implementation has been restructured for clarity -- the order-driven
// dispatch, the probe-and-latch state, the scratch-object lifecycle and the
// shared "save the caller's bindings, do the work, put them back" pattern are
// each now expressed once instead of repeated per backend -- and a number of
// latent bugs have been fixed. Those fixes are marked with FIX: comments at
// the point they apply.
//
// ---------------------------------------------------------------------------
// ES 3.2 native vs. CPU simulation
// ---------------------------------------------------------------------------
// Following the rule stated in gl/drawing.h ("ES 3.2 native -> native, ES 3.2
// not native -> CPU simulation"), each backend below is either:
//
//   * a *native* path that forwards to a driver entry point -- the batched
//     glMultiDraw*EXT calls, or a loop of the singular glDraw* / glDraw*Indirect
//     calls that ES 3.2 does have; or
//   * a *simulated* path that does the work on the CPU -- the index rebasing
//     loop, and the two compute-shader paths (which are simulated on the GPU
//     instead, precisely to avoid the CPU stall a readback would cost).
//
// Nothing here emulates a feature ES 3.2 already has, and nothing assumes a
// desktop-only capability is present without checking the extension string.

#include "multidraw.h"
#include "../egl/loader.h"
#include "../config/settings.h"
#include "buffer.h"
#include "enable.h"
#include "restart.h"
#include "../egl/context.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#define DEBUG 0

void prepareForDraw();

// ---------------------------------------------------------------------------
// Diagnostics
//
// LOG_D/LOG_W/LOG_E and the whole CHECK_GL_ERROR family expand to nothing when
// GLOBAL_DEBUG is 0 (gl/log.h), which is the shipping configuration. Every
// fallback and every error path in this file used to be completely invisible.
// MD_WARN_ONCE goes through LOG_W_FORCE, which is unconditional, and keeps a
// per-site latch so a per-draw-call condition cannot flood the log.
// ---------------------------------------------------------------------------
#define MD_WARN_ONCE(...)                                                                                              \
    do {                                                                                                               \
        static bool mg_md_warned = false;                                                                              \
        if (!mg_md_warned) {                                                                                           \
            mg_md_warned = true;                                                                                       \
            LOG_W_FORCE(__VA_ARGS__)                                                                                   \
        }                                                                                                              \
    } while (0)

// ---------------------------------------------------------------------------
// Error probe used to drive fallback decisions
//
// CHECK_GL_ERROR is a no-op in release builds, so it cannot be used for that.
// This only reports the error; the caller logs it through MD_WARN_ONCE, because
// logging here would be per-call and unlatched and write_log() flushes
// synchronously -- a persistently failing driver would add a disk flush to
// every draw.
//
// Always pair this with mg_md_drain() immediately before the call being
// checked: otherwise an error left pending by the application would be misread
// as our own failure and push every later draw onto a slower path.
// ---------------------------------------------------------------------------
static GLenum mg_md_check() {
    return GLES.glGetError();
}

// Empty the error queue so a following mg_md_check() reports only what the
// bracketed call produced. Nothing observable is lost: this layer already
// answers glGetError from elsewhere, so these entries could never reach the
// application anyway.
static void mg_md_drain() {
    for (int i = 0; i < 16 && GLES.glGetError() != GL_NO_ERROR; ++i) {
    }
}

// ---------------------------------------------------------------------------
// Index helpers
// ---------------------------------------------------------------------------

static inline GLsizei mg_index_size(GLenum type) {
    switch (type) {
    case GL_UNSIGNED_BYTE:
        return 1;
    case GL_UNSIGNED_SHORT:
        return 2;
    case GL_UNSIGNED_INT:
        return 4;
    default:
        return 0;
    }
}

// Rebase indices of any width into a 32-bit output stream.
//
// Widening is mandatory, not an optimisation: GL 4.6 sec. 10.5 evaluates
// (index + basevertex) in the full vertex index space. Writing the sum back
// into the source width truncates it to (index + basevertex) mod 65536 (or
// mod 256), which silently renders wrong geometry for the standard
// "one large vertex buffer + 16-bit indices + large basevertex" layout.
//
// When GL_PRIMITIVE_RESTART_FIXED_INDEX is enabled the sentinel must not be
// offset by basevertex (GL 4.6 sec. 10.3.6 compares the value as read from the
// buffer, before basevertex is added), and it has to be re-emitted as the 32-bit
// sentinel because the output stream is drawn as GL_UNSIGNED_INT.
//
// When restart is disabled there is no sentinel at all: 0xFFFF in a 16-bit index
// buffer is simply vertex 65535, so it must be rebased like any other index.
// Translating it unconditionally would turn a legitimate vertex reference into an
// out-of-range one.
static void mg_rebase_indices_to_u32(GLuint* dst, const void* src, GLsizei count, GLenum type, GLint basevertex,
                                     bool restart_enabled, GLuint sentinel) {
    const GLuint bv = static_cast<GLuint>(basevertex);

#define MG_REBASE_LOOP(SRCTYPE)                                                                                        \
    do {                                                                                                               \
        const SRCTYPE* s = static_cast<const SRCTYPE*>(src);                                                           \
        if (restart_enabled) {                                                                                         \
            for (GLsizei j = 0; j < count; ++j)                                                                        \
                dst[j] = (static_cast<GLuint>(s[j]) == sentinel) ? 0xFFFFFFFFu : (static_cast<GLuint>(s[j]) + bv);     \
        } else {                                                                                                       \
            for (GLsizei j = 0; j < count; ++j)                                                                        \
                dst[j] = static_cast<GLuint>(s[j]) + bv;                                                               \
        }                                                                                                              \
    } while (0)

    switch (type) {
    case GL_UNSIGNED_INT:
        MG_REBASE_LOOP(GLuint);
        break;
    case GL_UNSIGNED_SHORT:
        MG_REBASE_LOOP(GLushort);
        break;
    case GL_UNSIGNED_BYTE:
        MG_REBASE_LOOP(GLubyte);
        break;
    default:
        break;
    }

#undef MG_REBASE_LOOP
}

// Vertices per primitive for the separable modes. 0 means "not separable, do not
// fuse"; is_strip_like_mode covers those already.
static GLsizei mg_verts_per_primitive(GLenum mode) {
    switch (mode) {
    case GL_POINTS:
        return 1;
    case GL_LINES:
        return 2;
    case GL_TRIANGLES:
        return 3;
    case GL_LINES_ADJACENCY:
        return 4;
    case GL_TRIANGLES_ADJACENCY:
        return 6;
    default:
        // GL_PATCHES depends on GL_PATCH_VERTICES, which is not tracked here.
        return 0;
    }
}

static bool is_strip_like_mode(GLenum mode) {
    switch (mode) {
    case GL_LINE_STRIP:
    case GL_LINE_LOOP:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_LINE_STRIP_ADJACENCY:
    case GL_TRIANGLE_STRIP_ADJACENCY:
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// GL_EXT_multi_draw_arrays
//
// glMultiDrawArraysEXT / glMultiDrawElementsEXT are the exact GLES equivalents of
// the GL 1.4 core commands: one driver call, no command buffer to build, no
// synthesised base vertex array. The GLES loader does not carry them and
// gles/loader.* is not ours to change, so they are resolved here, the same way
// this file already calls eglGetCurrentContext() directly.
//
// A resolved symbol is not proof of support on Android, so the callers probe the
// first call and latch the backend off if the driver rejects it.
//
// Resolution goes through the GLES library handle, NOT eglGetProcAddress: this
// layer's own eglGetProcAddress forwards to glXGetProcAddress, which does
// dlsym(RTLD_DEFAULT, ...) and would hand back MobileGlues' own exported
// glMultiDrawArraysEXT -- an alias of glMultiDrawArrays -- so calling it from
// inside glMultiDrawArrays would recurse until the stack ran out. `gles` is the
// same handle gles/loader.cpp resolves every other entry point from.
// ---------------------------------------------------------------------------

extern "C" void* gles; // defined in gles/loader.cpp

// ---------------------------------------------------------------------------
// Extension capability queries
//
// One place decides what the driver can really do. A resolved symbol alone is
// never sufficient evidence on Android -- the platform EGL wrapper resolves
// names whether or not anything implements them -- so each query pairs the
// pointer with the extension string.
// ---------------------------------------------------------------------------

static bool mg_gles_has_extension(const char* name) {
    if (!GLES.glGetStringi || !GLES.glGetIntegerv) return false;
    GLint count = 0;
    GLES.glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; ++i) {
        const GLubyte* s = GLES.glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i));
        if (s && strcmp(reinterpret_cast<const char*>(s), name) == 0) return true;
    }
    return false;
}

typedef void(GLAPIENTRY* mg_pfn_multi_draw_arrays_ext)(GLenum, const GLint*, const GLsizei*, GLsizei);
typedef void(GLAPIENTRY* mg_pfn_multi_draw_elements_ext)(GLenum, const GLsizei*, GLenum, const void* const*, GLsizei);

static mg_pfn_multi_draw_arrays_ext g_mda_ext = nullptr;
static mg_pfn_multi_draw_elements_ext g_mde_ext = nullptr;

bool mg_multi_draw_arrays_ext_available() {
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        // Require the extension string as well as the symbols: a bare dlsym can
        // pick up a platform wrapper stub that reports nothing.
        const bool ext = mg_gles_has_extension("GL_EXT_multi_draw_arrays");
        const bool angle = mg_gles_has_extension("GL_ANGLE_multi_draw");
        // On Apple `gles` is a dlsym pseudo-handle, not a library, and resolving
        // through it would find MobileGlues' own alias and recurse. The only thing
        // keeping that unreachable today is that neither extension is ever
        // advertised (gles/loader.cpp), which is not a guarantee worth relying on.
        const bool real_handle = gles != nullptr && gles != reinterpret_cast<void*>(~(uintptr_t)0);
        if (real_handle && (ext || angle)) {
            const char* arrays_name = ext ? "glMultiDrawArraysEXT" : "glMultiDrawArraysANGLE";
            const char* elements_name = ext ? "glMultiDrawElementsEXT" : "glMultiDrawElementsANGLE";
            g_mda_ext = reinterpret_cast<mg_pfn_multi_draw_arrays_ext>(dlsym(gles, arrays_name));
            g_mde_ext = reinterpret_cast<mg_pfn_multi_draw_elements_ext>(dlsym(gles, elements_name));
        }
        LOG_D("multidraw: multi_draw_arrays ext=%d angle=%d arrays=%p elements=%p", (int)ext, (int)angle,
              (void*)g_mda_ext, (void*)g_mde_ext)
    }
    return g_mda_ext != nullptr || g_mde_ext != nullptr;
}

// glMultiDrawElementsBaseVertexEXT is not part of EXT/OES_draw_elements_base_vertex
// on its own. Both specs define the multi-draw form only when
// GL_EXT_multi_draw_arrays is *also* supported, and a driver that has the base
// vertex extension without it is an ordinary configuration -- Mali r32p1 is one.
//
// Neither of the two things this code used to rely on can see the difference.
// Android's EGL wrapper resolves the symbol whether or not the driver behind it
// implements anything, so a non-null pointer proves nothing; and such a driver
// accepts the call, draws nothing, and raises no error, so the probe-and-latch
// in mg_multi_draw_basevertex latches Working and every sub-draw is silently
// dropped for the rest of the process. Only the extension string can tell.
bool mg_multi_draw_elements_basevertex_ext_available() {
    static bool resolved = false;
    static bool available = false;
    if (!resolved) {
        resolved = true;
        available = GLES.glMultiDrawElementsBaseVertexEXT != nullptr &&
                    (g_gles_caps.GL_EXT_draw_elements_base_vertex || g_gles_caps.GL_OES_draw_elements_base_vertex) &&
                    mg_gles_has_extension("GL_EXT_multi_draw_arrays");
        LOG_D("multidraw: multibasevertex available=%d (ptr=%p bv_ext=%d/%d)", (int)available,
              (void*)GLES.glMultiDrawElementsBaseVertexEXT, g_gles_caps.GL_EXT_draw_elements_base_vertex,
              g_gles_caps.GL_OES_draw_elements_base_vertex)
    }
    return available;
}

// GL_EXT_multi_draw_indirect supplies the batched indirect forms. Both the
// pointer and the string are required, and the CALLER of the indirect entry
// points re-checks at the call site rather than caching: those entry points are
// reachable by a direct dlsym through glXGetProcAddress, so resolution is not
// the only path in.
static bool mg_multi_draw_indirect_available() {
    return g_gles_caps.GL_EXT_multi_draw_indirect != 0 && GLES.glMultiDrawArraysIndirectEXT != nullptr &&
           GLES.glMultiDrawElementsIndirectEXT != nullptr;
}

// ---------------------------------------------------------------------------
// Scratch GL objects
//
// Each name below belongs to whichever context was current when it was created.
// They used to live for the whole process, so once the application destroyed and
// recreated its EGL context the stale names were reused against the new one:
// glBindBuffer would quietly create a fresh zero-byte buffer while the cached
// capacity still claimed it was large enough, the following map returned NULL,
// and the command fill wrote through a null pointer.
//
// They are therefore owned by a single md_scratch_state_t, created on first use,
// invalidated wholesale on a context change, and released together.
// ---------------------------------------------------------------------------

enum class md_probe_state_t { Unprobed, Working, Failed };

// Identified by the monotonic MGContext id, not by the EGLContext pointer.
// EGLContext is a driver heap allocation, so destroying one and creating another
// very often returns the same address; comparing addresses reported "same
// context" for a context that never owned any of these objects, and the stale
// names were then used against it. 0 means "no tracked context", which is also
// what the bootstrap probe context looks like.
static unsigned long long g_owner_ctx_id = 0;

// A grow-only scratch buffer together with the size this context has actually
// *verified* it to have.
//
// `cap` is the point of the struct. It is only ever advanced after a
// query-verified allocation succeeded, so it can never claim more room than the
// store has -- which is what used to happen when the capacity was assigned
// unconditionally: an out-of-memory then became permanent, the resize branch was
// never taken again, and every later map or upload ran past the end.
struct md_scratch_buffer_t {
    GLuint name = 0;
    GLsizei cap = 0; // in units of `unit_bytes` below, not bytes

    bool ensure(GLenum target, GLsizei units, size_t unit_bytes, const char* what) {
        if (units <= 0) return false;
        if (name == 0) {
            GLES.glGenBuffers(1, &name);
            cap = 0;
        }
        GLES.glBindBuffer(target, name);
        if (cap >= units) return true;

        size_t sz = cap > 0 ? static_cast<size_t>(cap) : 1;
        while (sz < static_cast<size_t>(units))
            sz *= 2;

        const size_t wanted = sz * unit_bytes;
        GLES.glBufferData(target, static_cast<GLsizeiptr>(wanted), nullptr, GL_DYNAMIC_DRAW);

        GLint real_size = 0;
        GLES.glGetBufferParameteriv(target, GL_BUFFER_SIZE, &real_size);
        if (real_size < 0 || static_cast<size_t>(real_size) < wanted) {
            MD_WARN_ONCE("multidraw %s: scratch allocation failed (wanted %zu bytes, got %d)", what, wanted, real_size);
            // The failed glBufferData leaves the store in an undefined state, so
            // the previously recorded capacity no longer describes it. Clearing it
            // forces a fresh allocation attempt next time; keeping the stale value
            // would skip the resize branch forever and make every later use fail.
            cap = 0;
            return false;
        }
        cap = static_cast<GLsizei>(sz);
        return true;
    }
};

struct md_scratch_state_t {
    // Elements commands are 20 bytes, arrays commands 16, and these two are
    // tracked separately because a shared buffer would let an Arrays call skip
    // its resize because an Elements call had already "grown" it -- and then
    // map or draw past the end of the store.
    md_scratch_buffer_t elements_indirect; // GL_DRAW_INDIRECT_BUFFER, elements commands
    md_scratch_buffer_t arrays_indirect;   // GL_DRAW_INDIRECT_BUFFER, arrays commands
    GLuint scratch_ibo = 0;                // GL_ELEMENT_ARRAY_BUFFER for the CPU rebase path

    // Index-fusion compute path. The three grow-only SSBOs keep their own
    // verified byte capacities because that path re-specifies them every draw
    // (see the WAW note in the compute backend) and must skip the verification
    // round-trip on the steady-state call.
    GLuint prefix_sum_buffer = 0;
    GLuint draw_cmd_buffer = 0;
    GLuint output_ibo = 0;
    size_t draw_cmd_cap = 0;
    size_t prefix_sum_cap = 0;
    size_t output_cap = 0;
    bool compute_inited = false;
    bool compute_failed = false;
    GLuint compute_program = 0;
    GLint element_size_loc = -1;
    GLint max_compute_groups_x = 0;

    // glMultiDraw*IndirectCount compaction.
    GLuint count_program = 0;
    md_scratch_buffer_t count_scratch;
    bool count_inited = false;
    bool count_failed = false;
    GLint count_loc_max = -1;
    GLint count_loc_srcwords = -1;
    GLint count_loc_srcoff = -1;
    GLint count_loc_cntoff = -1;
    GLint count_loc_dstwords = -1;

    // Probe latches for the extension-provided batched backends.
    //
    // One tri-state rather than a separate "probed" flag and "failed" flag: those
    // were kept in two places, only one of which the context check reset, so after
    // a context change the backend was re-enabled but never re-probed -- and an
    // unprobed call reports success unconditionally, which loses the whole batch
    // on a driver whose entry point is a stub.
    md_probe_state_t elements_multiarrays_state = md_probe_state_t::Unprobed;
    md_probe_state_t elements_multibasevertex_state = md_probe_state_t::Unprobed;
    md_probe_state_t arrays_multiarrays_state = md_probe_state_t::Unprobed;
    md_probe_state_t arrays_multiindirect_state = md_probe_state_t::Unprobed;
};

static md_scratch_state_t g_scratch;

// ---------------------------------------------------------------------------
// Invalidate every cached GL object name when the current context changes.
//
// Object names are actually shared across a share group rather than tied to one
// context, but EGL exposes no way to query the share group, so this compares
// context identity.
//
// Destroy-and-recreate is handled correctly. Two live contexts used alternately
// are not: each switch drops the other context's still-valid objects, so they
// are rebuilt every time and the abandoned ones are never freed (the output
// index buffer can be several MB). Deleting them here is not possible either,
// because a name from the context being left is not addressable from the one
// being entered. Making that case tidy needs a per-context map; it is not
// implemented.
// ---------------------------------------------------------------------------
static void multidraw_check_context() {
    const unsigned long long cur = g_current_ctx ? g_current_ctx->id : 0;
    if (cur == g_owner_ctx_id) return;

    // Deliberately no glDelete* here: if the owning context is gone its objects
    // went with it, and if it is merely not current then these names refer to
    // objects belonging to whichever context *is* current.
    g_scratch = md_scratch_state_t{};

    // gl/restart.cpp caches a scratch index buffer of its own, created with a
    // real driver name rather than a virtual one, so it has exactly the same
    // cross-context reuse hazard.
    mg_restart_invalidate();

    g_owner_ctx_id = cur;
    LOG_D("multidraw: context changed, scratch objects invalidated")
}

// ---------------------------------------------------------------------------
// Entry validation
//
// GL 4.6 sec. 10.5 requires GL_INVALID_VALUE for a negative count and
// GL_INVALID_ENUM for an unrecognised type. This layer has no way to raise a GL
// error the application can observe, so the achievable goal is consistency:
// every mode must react to the same bad input the same way. Before this, a
// negative count was silently skipped by the unrolled paths, clamped to zero by
// the compute path, and turned into a ~4-billion-index draw by the indirect
// paths.
// ---------------------------------------------------------------------------
static bool mg_validate_multidraw(const GLsizei* counts, GLenum type, GLsizei primcount) {
    if (primcount < 0) {
        MD_WARN_ONCE("multidraw: negative primcount %d", primcount);
        return false;
    }
    if (primcount == 0) return false; // legal no-op
    if (!counts) {
        MD_WARN_ONCE("multidraw: counts == NULL");
        return false;
    }
    if (mg_index_size(type) == 0) {
        MD_WARN_ONCE("multidraw: invalid index type 0x%04x", type);
        return false;
    }
    for (GLsizei i = 0; i < primcount; ++i) {
        if (counts[i] < 0) {
            MD_WARN_ONCE("multidraw: negative count at sub-draw %d", i);
            return false;
        }
    }
    return true;
}

// Shared entry gate for every indexed mode implementation.
//
// This deliberately lives in the mode functions rather than only in the two
// indexed dispatchers: glXGetProcAddress hands the mode-specific mg_* symbol
// straight to the application (glx/lookup.cpp handle_multidraw_func_name), so
// the dispatcher is not always on the call path, and the cached GL objects
// would then never be checked against the current context.
static bool mg_multidraw_enter(const GLsizei* counts, GLenum type, GLsizei primcount, const void* const* indices) {
    if (!mg_validate_multidraw(counts, type, primcount)) return false;
    if (!indices) {
        MD_WARN_ONCE("multidraw: indices == NULL");
        return false;
    }
    multidraw_check_context();
    return true;
}

// A negative `first` matters more than it looks: DrawArraysIndirectCommand::first
// is a GLuint, so it would become a ~4.29e9 vertex offset instead of an error.
// The arrays entry points have no `indices` and so no mg_multidraw_enter.
static bool mg_validate_multidraw_arrays(const GLint* first, const GLsizei* count, GLsizei drawcount) {
    if (drawcount <= 0) return false;
    if (!first || !count) {
        MD_WARN_ONCE("glMultiDrawArrays: first/count is NULL");
        return false;
    }
    for (GLsizei i = 0; i < drawcount; ++i) {
        if (count[i] < 0) {
            MD_WARN_ONCE("glMultiDrawArrays: negative count at sub-draw %d", i);
            return false;
        }
        if (first[i] < 0) {
            MD_WARN_ONCE("glMultiDrawArrays: negative first at sub-draw %d", i);
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// GL_PRIMITIVE_RESTART across a multi-draw
//
// Restart is per-index state, so it applies to every sub-draw of an indexed
// multi-draw exactly as it would to the equivalent loop of single draws. GLES
// only implements the fixed-index form, and nothing here used to account for
// either half of that, so a batch drawn with restart enabled came out with its
// strips joined end to end.
//
// Two cases, matching the two predicates gl/restart.cpp exposes:
//
//   the chosen value is the fixed one -- no rewrite is needed, but GLES still
//     has to be told, because GL_PRIMITIVE_RESTART itself is never forwarded.
//     One enable around the whole batch covers every sub-draw.
//
//   the chosen value is something else -- the index stream has to be rewritten,
//     and the drawelements backend is the only one that rewrites. Every other
//     backend hands the application's indices to the driver untouched, so for
//     the duration of such a draw they defer to it.
//
// Both live in the backends rather than in the dispatchers because
// glXGetProcAddress hands out the mg_* symbols directly.
// ---------------------------------------------------------------------------

namespace {
struct md_restart_scope_t {
    bool forced;
    explicit md_restart_scope_t(GLenum type) : forced(mg_restart_needs_driver_fixed(type)) {
        if (forced) GLES.glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    }
    ~md_restart_scope_t() {
        if (forced) GLES.glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    }
    md_restart_scope_t(const md_restart_scope_t&) = delete;
    md_restart_scope_t& operator=(const md_restart_scope_t&) = delete;
};
} // namespace

// True when this call was handed to the rewriting backend and the caller is done.
static bool mg_multidraw_restart_takeover(GLenum mode, const GLsizei* counts, GLenum type, const void* const* indices,
                                          GLsizei primcount, const GLint* basevertex) {
    if (!mg_restart_needs_rewrite(type)) return false;
    mg_glMultiDrawElementsBaseVertex_drawelements(mode, const_cast<GLsizei*>(counts), type, indices, primcount,
                                                  basevertex);
    return true;
}

// ---------------------------------------------------------------------------
// Order-driven degradation
//
// Every "this backend cannot serve this call" branch below routes through one
// of the md_fall_* helpers: the next rung is whatever the user ranked after the
// failing backend for that entry point (config/settings.cpp,
// global_settings.multidraw_order), not a hard-coded neighbour. The walk is
// strictly forward through that order, so it terminates; the terminal rung for
// the three list-taking entry points is the unrolled loop, which never falls.
//
// g_md_fallback_tick lets the in-process benchmark (multidraw_bench.cpp) detect
// that a measured call was not actually served by the backend it was aimed at.
// ---------------------------------------------------------------------------

std::atomic<uint32_t> g_md_fallback_tick{0};

// The one place the order walk happens. Every md_fall_* below is this.
static md_backend_t md_fall_target(md_entry_t entry, md_backend_t cur) {
    g_md_fallback_tick.fetch_add(1, std::memory_order_relaxed);
    md_backend_t next = md_next_backend(entry, cur);
    // The last rung failed. The unrolled loop is always legal, so it is the
    // terminal rung rather than a cycle back into `cur`.
    if (next == cur) next = md_backend_t::Unroll;
    return next;
}

// ---------------------------------------------------------------------------
// Shared binding save/restore
//
// Every backend that binds a scratch buffer has to put the caller's binding
// back. Rather than each backend open-coding the save (and risking the read
// happening after the save site has already bound over it -- a bug that reached
// production once, on GL_SHADER_STORAGE_BUFFER), the pattern is expressed once
// per target here as a small RAII type.
//
// The value is captured at construction from the CPU-side tracker, which is
// what makes it safe to construct at the top of a function before anything is
// bound. It is a driver-side name (mg_driver_bound_buffer) because it is handed
// straight back to GLES.
// ---------------------------------------------------------------------------
namespace {

struct md_indirect_binding_scope_t {
    GLuint prev;
    explicit md_indirect_binding_scope_t()
        : prev(mg_driver_bound_buffer(GL_DRAW_INDIRECT_BUFFER)) {}
    ~md_indirect_binding_scope_t() { GLES.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, prev); }
    md_indirect_binding_scope_t(const md_indirect_binding_scope_t&) = delete;
    md_indirect_binding_scope_t& operator=(const md_indirect_binding_scope_t&) = delete;
};

struct md_element_binding_scope_t {
    GLuint prev;
    explicit md_element_binding_scope_t()
        : prev(mg_driver_bound_buffer(GL_ELEMENT_ARRAY_BUFFER)) {}
    ~md_element_binding_scope_t() { GLES.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prev); }
    md_element_binding_scope_t(const md_element_binding_scope_t&) = delete;
    md_element_binding_scope_t& operator=(const md_element_binding_scope_t&) = delete;
};

// Scoped GL_PRIMITIVE_RESTART_FIXED_INDEX for the CPU rebase path only. The
// batched and indirect paths use md_restart_scope_t, which only ever enables.
struct md_fixed_restart_enable_t {
    bool forced;
    explicit md_fixed_restart_enable_t(bool enabled)
        : forced(enabled && mg_enable_get(GL_PRIMITIVE_RESTART_FIXED_INDEX, 0) != GL_TRUE) {
        if (forced) GLES.glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    }
    ~md_fixed_restart_enable_t() {
        if (forced) GLES.glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    }
    md_fixed_restart_enable_t(const md_fixed_restart_enable_t&) = delete;
    md_fixed_restart_enable_t& operator=(const md_fixed_restart_enable_t&) = delete;
};

} // namespace

// ---------------------------------------------------------------------------
// Backend call tables
//
// One switch per entry-point shape, used by both the dispatcher and the
// fallback walk, so the two can never disagree about which symbol implements a
// backend.
// ---------------------------------------------------------------------------

static void md_call_elements(md_backend_t b, GLenum mode, const GLsizei* count, GLenum type,
                             const void* const* indices, GLsizei primcount) {
    switch (b) {
    case md_backend_t::Indirect:
        mg_glMultiDrawElements_indirect(mode, count, type, indices, primcount);
        break;
    case md_backend_t::MultiIndirect:
        mg_glMultiDrawElements_multiindirect(mode, count, type, indices, primcount);
        break;
    case md_backend_t::MultiBaseVertex:
        mg_glMultiDrawElements_multibasevertex(mode, count, type, indices, primcount);
        break;
    case md_backend_t::MultiArrays:
        mg_glMultiDrawElements_multiarrays(mode, count, type, indices, primcount);
        break;
    default:
        mg_glMultiDrawElements_drawelements(mode, count, type, indices, primcount);
        break;
    }
}

static void md_fall_elements(md_backend_t cur, GLenum mode, const GLsizei* count, GLenum type,
                             const void* const* indices, GLsizei primcount) {
    md_call_elements(md_fall_target(md_entry_t::Elements, cur), mode, count, type, indices, primcount);
}

static void md_call_elements_bv(md_backend_t b, GLenum mode, GLsizei* counts, GLenum type,
                                const void* const* indices, GLsizei primcount, const GLint* basevertex) {
    switch (b) {
    case md_backend_t::Indirect:
        mg_glMultiDrawElementsBaseVertex_indirect(mode, counts, type, indices, primcount, basevertex);
        break;
    case md_backend_t::BaseVertex:
        mg_glMultiDrawElementsBaseVertex_basevertex(mode, counts, type, indices, primcount, basevertex);
        break;
    case md_backend_t::MultiIndirect:
        mg_glMultiDrawElementsBaseVertex_multiindirect(mode, counts, type, indices, primcount, basevertex);
        break;
    case md_backend_t::MultiBaseVertex:
        mg_glMultiDrawElementsBaseVertex_multibasevertex(mode, counts, type, indices, primcount, basevertex);
        break;
    case md_backend_t::Compute:
        mg_glMultiDrawElementsBaseVertex_compute(mode, counts, type, indices, primcount, basevertex);
        break;
    default:
        mg_glMultiDrawElementsBaseVertex_drawelements(mode, counts, type, indices, primcount, basevertex);
        break;
    }
}

static void md_fall_elements_bv(md_backend_t cur, GLenum mode, GLsizei* counts, GLenum type,
                                const void* const* indices, GLsizei primcount, const GLint* basevertex) {
    md_call_elements_bv(md_fall_target(md_entry_t::ElementsBaseVertex, cur), mode, counts, type, indices, primcount,
                        basevertex);
}

static void md_call_arrays(md_backend_t b, GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) {
    switch (b) {
    case md_backend_t::MultiArrays:
        mg_glMultiDrawArrays_multiarrays(mode, first, count, drawcount);
        break;
    case md_backend_t::MultiIndirect:
        mg_glMultiDrawArrays_multiindirect(mode, first, count, drawcount);
        break;
    default:
        mg_glMultiDrawArrays_unroll(mode, first, count, drawcount);
        break;
    }
}

static void md_fall_arrays(md_backend_t cur, GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) {
    md_call_arrays(md_fall_target(md_entry_t::Arrays, cur), mode, first, count, drawcount);
}

// ---------------------------------------------------------------------------
// Dispatchers
//
// Each of the two entry points that have more than one implementation resolves
// its backend once and caches the function pointer. Validation and the context
// check happen inside the backend implementation, so they also cover the case
// where glXGetProcAddress handed that symbol to the application directly.
// ---------------------------------------------------------------------------

typedef void (*glMultiDrawElements_t)(GLenum, const GLsizei*, GLenum, const void* const*, GLsizei);

void glMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                         GLsizei primcount) {
    static glMultiDrawElements_t func_ptr = nullptr;

    if (func_ptr == nullptr) {
        switch (multidraw_backend_of(md_entry_t::Elements)) {
        case md_backend_t::Indirect:
            func_ptr = mg_glMultiDrawElements_indirect;
            break;
        case md_backend_t::MultiIndirect:
            func_ptr = mg_glMultiDrawElements_multiindirect;
            break;
        case md_backend_t::MultiBaseVertex:
            func_ptr = mg_glMultiDrawElements_multibasevertex;
            break;
        case md_backend_t::MultiArrays:
            func_ptr = mg_glMultiDrawElements_multiarrays;
            break;
        default:
            // Unroll, and anything the mask should already have rejected.
            func_ptr = mg_glMultiDrawElements_drawelements;
            break;
        }
    }

    func_ptr(mode, count, type, indices, primcount);
}

typedef void (*glMultiDrawElementsBaseVertex_t)(GLenum, GLsizei*, GLenum, const void* const*, GLsizei, const GLint*);

void glMultiDrawElementsBaseVertex(GLenum mode, GLsizei* counts, GLenum type, const void* const* indices,
                                   GLsizei primcount, const GLint* basevertex) {
    static glMultiDrawElementsBaseVertex_t func_ptr = nullptr;

    if (func_ptr == nullptr) {
        switch (multidraw_backend_of(md_entry_t::ElementsBaseVertex)) {
        case md_backend_t::Indirect:
            func_ptr = mg_glMultiDrawElementsBaseVertex_indirect;
            break;
        case md_backend_t::BaseVertex:
            func_ptr = mg_glMultiDrawElementsBaseVertex_basevertex;
            break;
        case md_backend_t::MultiIndirect:
            func_ptr = mg_glMultiDrawElementsBaseVertex_multiindirect;
            break;
        case md_backend_t::Compute:
            func_ptr = mg_glMultiDrawElementsBaseVertex_compute;
            break;
        case md_backend_t::MultiBaseVertex:
            func_ptr = mg_glMultiDrawElementsBaseVertex_multibasevertex;
            break;
        default:
            func_ptr = mg_glMultiDrawElementsBaseVertex_drawelements;
            break;
        }
    }

    func_ptr(mode, counts, type, indices, primcount, basevertex);
}

typedef void (*glMultiDrawArrays_t)(GLenum, const GLint*, const GLsizei*, GLsizei);

void glMultiDrawArrays(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) {
    static glMultiDrawArrays_t func_ptr = nullptr;

    if (func_ptr == nullptr) {
        switch (multidraw_backend_of(md_entry_t::Arrays)) {
        case md_backend_t::MultiArrays:
            func_ptr = mg_glMultiDrawArrays_multiarrays;
            break;
        case md_backend_t::MultiIndirect:
            func_ptr = mg_glMultiDrawArrays_multiindirect;
            break;
        default:
            func_ptr = mg_glMultiDrawArrays_unroll;
            break;
        }
    }

    func_ptr(mode, first, count, drawcount);
}

// ---------------------------------------------------------------------------
// Indirect command buffer
//
// Both indirect backends need the application's per-sub-draw lists turned into a
// command buffer. This is that, once, shared by four backends.
//
// Returns false when the command buffer could not be prepared; the caller must
// then fall back rather than issue a draw from an unwritten buffer.
// ---------------------------------------------------------------------------
static bool prepare_elements_indirect_buffer(const GLsizei* counts, GLenum type, const void* const* indices,
                                             GLsizei primcount, const GLint* basevertex) {
    if (primcount <= 0) return false;

    const GLsizei elementSize = mg_index_size(type);
    if (elementSize == 0) return false;

    // firstIndex below reads indices[i] as a byte offset into the element array
    // buffer, which is only meaningful when one is bound. GLES also requires a
    // bound element array buffer for indirect draws, so without this check a
    // client-side pointer would be turned into a nonsense firstIndex and the
    // whole batch would disappear without a word. The drawelements and compute
    // paths already made this check.
    //
    // The binding comes from the tracked state (mg_driver_bound_buffer,
    // gl/buffer.h) rather than from the driver, because nothing has been bound
    // over it yet at this point.
    const GLuint bound_ibo = mg_driver_bound_buffer(GL_ELEMENT_ARRAY_BUFFER);
    if (bound_ibo == 0) {
        MD_WARN_ONCE("multidraw: indirect path needs a bound element array buffer, falling back");
        return false;
    }

    if (!g_scratch.elements_indirect.ensure(GL_DRAW_INDIRECT_BUFFER, primcount,
                                            sizeof(draw_elements_indirect_command_t), "elements indirect")) {
        return false;
    }

    // Built on the CPU and uploaded whole, rather than written through a
    // GL_MAP_INVALIDATE_BUFFER_BIT mapping. The mapping says "all of this is
    // about to be overwritten" more precisely, and on a driver that honours it
    // there is nothing to choose between the two. What it costs is two ways to
    // fail on every single draw call: a map that returns null, and an unmap that
    // reports the contents lost. Both used to abandon the backend for the rest of
    // the process, and falling back to unrolled draws forever is far more
    // expensive than the upload that was being avoided -- a driver refusing to
    // hand out a pointer for a buffer it is still reading is a perfectly ordinary
    // thing, not a reason to give up on indirect drawing.
    //
    // thread_local for the same reason as mg_zero_basevertex: nothing in this
    // file takes a lock, and two threads can each have a current context.
    static thread_local std::vector<draw_elements_indirect_command_t> staged;
    staged.resize(static_cast<size_t>(primcount));
    draw_elements_indirect_command_t* pcmds = staged.data();

    for (GLsizei i = 0; i < primcount; ++i) {
        const uintptr_t byteOffset = reinterpret_cast<uintptr_t>(indices[i]);
        // A negative count assigned straight into the unsigned command field used
        // to become ~4.29e9 indices, i.e. a draw far past the end of every bound
        // buffer.
        const GLsizei c = counts[i] > 0 ? counts[i] : 0;

        pcmds[i].firstIndex = static_cast<GLuint>(byteOffset / static_cast<uintptr_t>(elementSize));
        pcmds[i].count = static_cast<GLuint>(c);
        pcmds[i].instanceCount = 1;
        pcmds[i].baseVertex = basevertex ? basevertex[i] : 0;
        pcmds[i].reservedMustBeZero = 0;
    }

    GLES.glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
                         static_cast<GLsizeiptr>(primcount * sizeof(draw_elements_indirect_command_t)), pcmds);
    return true;
}

// Folds the batch into one glMultiDrawArraysIndirectEXT by building a command
// buffer. Returns false when the buffer could not be prepared.
static bool prepare_arrays_indirect_buffer(const GLint* first, const GLsizei* count, GLsizei drawcount) {
    if (drawcount <= 0) return false;

    if (!g_scratch.arrays_indirect.ensure(GL_DRAW_INDIRECT_BUFFER, drawcount,
                                          sizeof(draw_arrays_indirect_command_t), "arrays indirect")) {
        return false;
    }

    auto* cmds = static_cast<draw_arrays_indirect_command_t*>(GLES.glMapBufferRange(
        GL_DRAW_INDIRECT_BUFFER, 0, static_cast<GLsizeiptr>(drawcount * sizeof(draw_arrays_indirect_command_t)),
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
    if (!cmds) {
        MD_WARN_ONCE("multidraw arrays: failed to map the indirect command buffer");
        return false;
    }

    for (GLsizei i = 0; i < drawcount; ++i) {
        cmds[i].count = static_cast<GLuint>(count[i] > 0 ? count[i] : 0);
        cmds[i].instanceCount = 1;
        cmds[i].first = static_cast<GLuint>(first[i]); // validated non-negative
        cmds[i].baseInstanceOrReserved = 0;
    }

    if (GLES.glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER) == GL_FALSE) {
        MD_WARN_ONCE("multidraw arrays: indirect command buffer contents lost on unmap");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Backend: DrawElements (CPU rebase, no extension required)
//
// This is the simulated terminal rung for the indexed entry points. It is the
// only backend that can rewrite the index stream, which is also why it is the
// one gl/restart.cpp's takeover routes to.
// ---------------------------------------------------------------------------

void mg_glMultiDrawElementsBaseVertex_drawelements(GLenum mode, GLsizei* counts, GLenum type,
                                                   const void* const* indices, GLsizei primcount,
                                                   const GLint* basevertex) {
    LOG()
    if (!mg_multidraw_enter(counts, type, primcount, indices)) return;

    // Resolved once for the whole call. It used to be a switch inside the
    // per-sub-draw loop whose default branch `return`ed, abandoning the rest of
    // the multi-draw without a trace.
    const GLsizei indexSize = mg_index_size(type);

    prepareForDraw();

    // Queried once per multi-draw rather than per sub-draw: it decides whether the
    // sentinel value is special or an ordinary vertex index.
    // Read from the virtual table, not the driver: GL_PRIMITIVE_RESTART with a
    // custom index is invisible to GLES, and the driver's fixed-index flag is
    // also toggled behind the application's back by the restart emulation.
    const bool restart_enabled = mg_primitive_restart_enabled();
    const GLuint restart_value = mg_primitive_restart_index_for(type);
    // The rewritten stream carries 0xFFFFFFFF wherever a restart was, and is
    // drawn as GL_UNSIGNED_INT, so the driver's fixed-index restart has to be on
    // for these draws. Without it 0xFFFFFFFF is fetched as vertex 4294967295 and
    // every enabled attribute array is read out of bounds.
    md_fixed_restart_enable_t fixed_restart(restart_enabled);

    {
        // Tracked rather than queried, and read before the loop below starts
        // swapping the scratch buffer in. The scope restores it on every exit
        // path, including the fallbacks inside the loop.
        md_element_binding_scope_t element_restore;

        // One persistent scratch buffer instead of glGenBuffers/glDeleteBuffers
        // per sub-draw.
        if (g_scratch.scratch_ibo == 0) GLES.glGenBuffers(1, &g_scratch.scratch_ibo);

        // Grown but never shrunk, and thread_local for the same reason `staged`
        // above is. Only the first `count` elements of any one sub-draw are
        // written and uploaded, so what a wider sub-draw left behind is never
        // read; sizing it to each count in turn would zero-fill a range
        // mg_rebase_indices_to_u32 overwrites in full immediately after.
        static thread_local std::vector<GLuint> rebased;

        for (GLsizei i = 0; i < primcount; ++i) {
            const GLsizei count = counts[i];
            if (count <= 0) continue;

            const GLint bv = basevertex ? basevertex[i] : 0;

            if (rebased.size() < static_cast<size_t>(count)) rebased.resize(static_cast<size_t>(count));

            if (element_restore.prev != 0) {
                GLES.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_restore.prev);
                void* srcData = GLES.glMapBufferRange(
                    GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>(reinterpret_cast<uintptr_t>(indices[i])),
                    static_cast<GLsizeiptr>(count) * indexSize, GL_MAP_READ_BIT);
                if (!srcData) {
                    // An index buffer created with glBufferStorage is not readable
                    // via glMapBufferRange, and this used to drop the sub-draw
                    // silently. Let the driver apply the base vertex instead of
                    // dropping it.
                    MD_WARN_ONCE("multidraw drawelements: element buffer is not mappable for reading, "
                                 "using driver base vertex");
                    GLES.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_restore.prev);
                    if (GLES.glDrawElementsBaseVertex) {
                        GLES.glDrawElementsBaseVertex(mode, count, type, indices[i], bv);
                    } else if (bv == 0) {
                        // No base vertex to apply, so the unmodified stream is
                        // correct.
                        GLES.glDrawElements(mode, count, type, indices[i]);
                    } else {
                        // The offset cannot be applied without either a readback
                        // or driver support. Skipping the sub-draw loses geometry,
                        // but drawing it would place it at the wrong vertices, and
                        // wrong geometry is worse than missing geometry.
                        MD_WARN_ONCE("multidraw drawelements: cannot apply base vertex %d, sub-draw skipped", bv);
                    }
                    continue;
                }
                mg_rebase_indices_to_u32(rebased.data(), srcData, count, type, bv, restart_enabled, restart_value);
                GLES.glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
            } else if (indices[i] != nullptr) {
                mg_rebase_indices_to_u32(rebased.data(), indices[i], count, type, bv, restart_enabled, restart_value);
            } else {
                // No element buffer bound and a null client pointer: there is
                // nothing to read. GL leaves this undefined, and reading it is a
                // segfault at address zero rather than a wrong picture -- which is
                // what it was, reachable from the in-process benchmark the moment
                // borrowing ANGLE started working, because a sub-draw's `indices`
                // there is a buffer offset and offset zero is a null pointer.
                //
                // The binding is what decides which of the two `indices` means, so
                // a zero binding with offset-shaped indices is a caller-side
                // mistake this cannot repair. Say so once and skip: missing
                // geometry beats a crash, and beats reading whatever happens to be
                // at address zero.
                MD_WARN_ONCE("multidraw drawelements: no element buffer bound and indices[%d] is null; "
                             "sub-draw skipped",
                             i);
                continue;
            }

            GLES.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_scratch.scratch_ibo);
            GLES.glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(count) * sizeof(GLuint), rebased.data(),
                              GL_STREAM_DRAW);
            // The rebased stream is 32-bit regardless of the source width.
            GLES.glDrawElements(mode, count, GL_UNSIGNED_INT, nullptr);
        }
    }

    CHECK_GL_ERROR
}

void mg_glMultiDrawElements_drawelements(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                                         GLsizei primcount) {
    LOG()
    if (!mg_multidraw_enter(count, type, primcount, indices)) return;

    if (mg_multidraw_restart_takeover(mode, count, type, indices, primcount, nullptr)) return;
    md_restart_scope_t restart_scope(type);

    prepareForDraw();

    // GL 4.6 sec. 10.5 defines glMultiDrawElements as exactly this loop; there is
    // no base vertex component on this entry point.
    for (GLsizei i = 0; i < primcount; ++i) {
        const GLsizei c = count[i];
        if (c > 0) {
            GLES.glDrawElements(mode, c, type, indices[i]);
        }
    }

    CHECK_GL_ERROR
}

// ---------------------------------------------------------------------------
// Backend: Indirect (one glDrawElementsIndirect per sub-draw)
//
// The commands are built on the CPU either way, so this backend exists for
// drivers that have the singular ES 3.2 indirect draw but not the batched
// extension. It is cheap to be here and it is strictly better than a loop of
// glDrawElements, because the index fetch is fully driven by the driver.
// ---------------------------------------------------------------------------

void mg_glMultiDrawElements_indirect(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                                     GLsizei primcount) {
    LOG()
    if (!mg_multidraw_enter(count, type, primcount, indices)) return;

    if (mg_multidraw_restart_takeover(mode, count, type, indices, primcount, nullptr)) return;
    md_restart_scope_t restart_scope(type);

    if (!GLES.glDrawElementsIndirect) {
        MD_WARN_ONCE("multidraw indirect: unavailable, falling back");
        md_fall_elements(md_backend_t::Indirect, mode, count, type, indices, primcount);
        return;
    }

    prepareForDraw();

    {
        md_indirect_binding_scope_t indirect_restore;
        if (!prepare_elements_indirect_buffer(count, type, indices, primcount, nullptr)) {
            md_fall_elements(md_backend_t::Indirect, mode, count, type, indices, primcount);
            return;
        }

        for (GLsizei i = 0; i < primcount; ++i) {
            if (count[i] <= 0) continue;
            const GLvoid* offset = reinterpret_cast<GLvoid*>(i * sizeof(draw_elements_indirect_command_t));
            GLES.glDrawElementsIndirect(mode, type, offset);
        }
    }

    CHECK_GL_ERROR
}

void mg_glMultiDrawElementsBaseVertex_indirect(GLenum mode, GLsizei* counts, GLenum type, const void* const* indices,
                                               GLsizei primcount, const GLint* basevertex) {
    LOG()
    if (!mg_multidraw_enter(counts, type, primcount, indices)) return;

    if (mg_multidraw_restart_takeover(mode, counts, type, indices, primcount, basevertex)) return;
    md_restart_scope_t restart_scope(type);

    if (!GLES.glDrawElementsIndirect) {
        MD_WARN_ONCE("multidraw indirect: unavailable, falling back");
        md_fall_elements_bv(md_backend_t::Indirect, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    prepareForDraw();

    {
        md_indirect_binding_scope_t indirect_restore;
        if (!prepare_elements_indirect_buffer(counts, type, indices, primcount, basevertex)) {
            md_fall_elements_bv(md_backend_t::Indirect, mode, counts, type, indices, primcount, basevertex);
            return;
        }

        for (GLsizei i = 0; i < primcount; ++i) {
            if (counts[i] <= 0) continue;
            const GLvoid* offset = reinterpret_cast<GLvoid*>(i * sizeof(draw_elements_indirect_command_t));
            GLES.glDrawElementsIndirect(mode, type, offset);
        }
    }

    CHECK_GL_ERROR
}

// ---------------------------------------------------------------------------
// Backend: MultiIndirect (one GLES multi-draw-indirect call)
//
// The fastest of the extension-provided backends for the indexed entry points:
// the whole batch is one driver call. It is only reachable when
// GL_EXT_multi_draw_indirect is present, and it is also reached as a fallback
// from the batched backends, so it re-checks its own precondition rather than
// assuming resolution already vetted it.
// ---------------------------------------------------------------------------

void mg_glMultiDrawElements_multiindirect(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                                          GLsizei primcount) {
    LOG()
    if (!mg_multidraw_enter(count, type, primcount, indices)) return;

    if (mg_multidraw_restart_takeover(mode, count, type, indices, primcount, nullptr)) return;
    md_restart_scope_t restart_scope(type);

    if (!mg_multi_draw_indirect_available()) {
        MD_WARN_ONCE("multidraw multiindirect: unavailable, falling back");
        md_fall_elements(md_backend_t::MultiIndirect, mode, count, type, indices, primcount);
        return;
    }

    prepareForDraw();

    {
        md_indirect_binding_scope_t indirect_restore;
        if (!prepare_elements_indirect_buffer(count, type, indices, primcount, nullptr)) {
            md_fall_elements(md_backend_t::MultiIndirect, mode, count, type, indices, primcount);
            return;
        }

        GLES.glMultiDrawElementsIndirectEXT(mode, type, 0, primcount, 0);
    }

    CHECK_GL_ERROR
}

void mg_glMultiDrawElementsBaseVertex_multiindirect(GLenum mode, GLsizei* counts, GLenum type,
                                                    const void* const* indices, GLsizei primcount,
                                                    const GLint* basevertex) {
    LOG()
    if (!mg_multidraw_enter(counts, type, primcount, indices)) return;

    if (mg_multidraw_restart_takeover(mode, counts, type, indices, primcount, basevertex)) return;
    md_restart_scope_t restart_scope(type);

    if (!mg_multi_draw_indirect_available()) {
        MD_WARN_ONCE("multidraw multiindirect: unavailable, falling back");
        md_fall_elements_bv(md_backend_t::MultiIndirect, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    prepareForDraw();

    {
        md_indirect_binding_scope_t indirect_restore;
        if (!prepare_elements_indirect_buffer(counts, type, indices, primcount, basevertex)) {
            md_fall_elements_bv(md_backend_t::MultiIndirect, mode, counts, type, indices, primcount, basevertex);
            return;
        }

        GLES.glMultiDrawElementsIndirectEXT(mode, type, 0, primcount, 0);
    }

    CHECK_GL_ERROR
}

// ---------------------------------------------------------------------------
// Backend: BaseVertex (one glDrawElementsBaseVertex per sub-draw)
//
// ES 3.2 core added glDrawElementsBaseVertex, so this needs no extension at all
// and is the terminal rung for the base-vertex entry point. It is also the only
// backend besides the CPU rebase that can honour a non-zero base vertex without
// a compute shader.
// ---------------------------------------------------------------------------

void mg_glMultiDrawElementsBaseVertex_basevertex(GLenum mode, GLsizei* counts, GLenum type, const void* const* indices,
                                                 GLsizei primcount, const GLint* basevertex) {
    LOG()
    if (!mg_multidraw_enter(counts, type, primcount, indices)) return;

    if (mg_multidraw_restart_takeover(mode, counts, type, indices, primcount, basevertex)) return;
    md_restart_scope_t restart_scope(type);

    if (!GLES.glDrawElementsBaseVertex) {
        MD_WARN_ONCE("multidraw basevertex: unavailable, falling back");
        md_fall_elements_bv(md_backend_t::BaseVertex, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    prepareForDraw();

    for (GLsizei i = 0; i < primcount; ++i) {
        const GLsizei count = counts[i];
        if (count > 0) {
            LOG_D("GLES.glDrawElementsBaseVertex, mode = %s, count = %d, type = %s, indices[i] = %p, basevertex[i] = "
                  "%d",
                  glEnumToString(mode), count, glEnumToString(type), indices[i], basevertex ? basevertex[i] : 0)
            GLES.glDrawElementsBaseVertex(mode, count, type, indices[i], basevertex ? basevertex[i] : 0);
        }
    }
    CHECK_GL_ERROR
}

// glMultiDrawElements has no base vertex component, so on the `basevertex`
// backend -- which resolution only ever picks for an entry point that has one --
// this is the plain spec-defined loop. It exists because
// glx/lookup.cpp's suffix table is per-entry-point and not per-signature: the
// `basevertex` suffix is handed out for glMultiDrawElements too, so the symbol
// has to resolve.
void mg_glMultiDrawElements_basevertex(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                                      GLsizei primcount) {
    LOG()
    if (!mg_multidraw_enter(count, type, primcount, indices)) return;

    if (mg_multidraw_restart_takeover(mode, count, type, indices, primcount, nullptr)) return;
    md_restart_scope_t restart_scope(type);

    prepareForDraw();

    for (GLsizei i = 0; i < primcount; ++i) {
        const GLsizei c = count[i];
        if (c > 0) {
            GLES.glDrawElements(mode, c, type, indices[i]);
        }
    }

    CHECK_GL_ERROR
}

// ---------------------------------------------------------------------------
// Backend: MultiBaseVertex (GL_EXT/OES_draw_elements_base_vertex)
//
// GL_EXT_draw_elements_base_vertex provides glMultiDrawElementsBaseVertexEXT
// with exactly the GL 3.2 core signature -- the one batched backend that can
// carry a base vertex without a compute shader.
//
// That extension only defines the multi-draw form when EXT_multi_draw_arrays is
// also present, and g_gles_caps does not track that string. A resolved,
// non-null entry point is therefore not proof that the driver implements it --
// on Android dlsym can hand back a wrapper stub. The first call is probed for a
// GL error and the backend is latched off if it fails, so a bad driver costs
// one draw rather than every draw.
// ---------------------------------------------------------------------------

// glMultiDrawElements has no base vertex, but the extension takes a real array
// with no "all zero" shorthand. The contents never change, so the buffer only
// has to grow. It is thread_local because nothing in this file takes a lock and
// two threads can each hold a current context; it holds no GL object name, so
// unlike the scratch buffers it deliberately survives a context change.
static const GLint* mg_zero_basevertex(GLsizei primcount) {
    static thread_local std::vector<GLint> zeros;
    if (zeros.size() < static_cast<size_t>(primcount)) zeros.resize(static_cast<size_t>(primcount), 0);
    return zeros.data();
}

// Issues the batched base-vertex call and maintains the probe latch. Returns
// false when the call was not made, or was made and failed its probe; the
// caller falls back in both cases.
static bool mg_multi_draw_basevertex(GLenum mode, const GLsizei* counts, GLenum type, const void* const* indices,
                                     GLsizei primcount, const GLint* basevertex) {
    if (g_scratch.elements_multibasevertex_state == md_probe_state_t::Failed ||
        !mg_multi_draw_elements_basevertex_ext_available())
        return false;

    const bool probing = (g_scratch.elements_multibasevertex_state == md_probe_state_t::Unprobed);
    if (probing) mg_md_drain();

    GLES.glMultiDrawElementsBaseVertexEXT(mode, counts, type, indices, primcount,
                                          basevertex ? basevertex : mg_zero_basevertex(primcount));

    if (probing) {
        const GLenum err = mg_md_check();
        if (err != GL_NO_ERROR) {
            MD_WARN_ONCE("multidraw multibasevertex: glMultiDrawElementsBaseVertexEXT failed with 0x%04x, "
                         "disabling it",
                         err);
            g_scratch.elements_multibasevertex_state = md_probe_state_t::Failed;
            return false;
        }
        g_scratch.elements_multibasevertex_state = md_probe_state_t::Working;
    }
    return true;
}

// Shared body of the two MultiBaseVertex entry points. The only difference
// between them is the base vertex array, so there is no reason for two copies.
static void mg_multidraw_elements_via_multibasevertex(GLenum mode, GLsizei* counts, GLenum type,
                                                      const void* const* indices, GLsizei primcount,
                                                      const GLint* basevertex, md_backend_t self) {
    // The latch read by mg_multi_draw_basevertex is per-context, so re-arm it
    // first. Hand over before doing any work once this backend is known to be
    // unusable, otherwise every later call would validate and prepare the batch
    // twice. The next rung comes from the user's order, so the degradation lands
    // on whatever they ranked directly below this backend.
    multidraw_check_context();
    if (g_scratch.elements_multibasevertex_state == md_probe_state_t::Failed ||
        !mg_multi_draw_elements_basevertex_ext_available()) {
        md_fall_elements_bv(self, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    if (!mg_multidraw_enter(counts, type, primcount, indices)) return;

    if (mg_multidraw_restart_takeover(mode, counts, type, indices, primcount, basevertex)) return;
    md_restart_scope_t restart_scope(type);

    prepareForDraw();

    if (!mg_multi_draw_basevertex(mode, counts, type, indices, primcount, basevertex)) {
        // Only reachable on the single call whose probe failed.
        md_fall_elements_bv(self, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    CHECK_GL_ERROR
}

void mg_glMultiDrawElementsBaseVertex_multibasevertex(GLenum mode, GLsizei* counts, GLenum type,
                                                      const void* const* indices, GLsizei primcount,
                                                      const GLint* basevertex) {
    LOG()
    mg_multidraw_elements_via_multibasevertex(mode, counts, type, indices, primcount, basevertex,
                                              md_backend_t::MultiBaseVertex);
}

// glMultiDrawElements is glMultiDrawElementsBaseVertex with an all-zero base
// vertex array, so this entry point gets the same single driver call. It routes
// through the base-vertex fallback chain, not the plain one, because that is the
// symbol being used.
void mg_glMultiDrawElements_multibasevertex(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                                            GLsizei primcount) {
    LOG()
    // The latch read just below is per-context, so re-arm it first.
    multidraw_check_context();

    if (g_scratch.elements_multibasevertex_state == md_probe_state_t::Failed ||
        !mg_multi_draw_elements_basevertex_ext_available()) {
        md_fall_elements(md_backend_t::MultiBaseVertex, mode, count, type, indices, primcount);
        return;
    }

    if (!mg_multidraw_enter(count, type, primcount, indices)) return;

    if (mg_multidraw_restart_takeover(mode, count, type, indices, primcount, nullptr)) return;
    md_restart_scope_t restart_scope(type);

    prepareForDraw();

    if (mg_multi_draw_basevertex(mode, count, type, indices, primcount, nullptr)) {
        CHECK_GL_ERROR
        return;
    }

    md_fall_elements(md_backend_t::MultiBaseVertex, mode, count, type, indices, primcount);
}

// ---------------------------------------------------------------------------
// Backend: MultiArrays (GL_EXT_multi_draw_arrays / GL_ANGLE_multi_draw)
//
// glMultiDrawElementsEXT has exactly the signature and semantics of the GL 1.4
// core command, so this is one driver call with no command buffer to build and
// no synthesised base vertex array. Same probe-and-latch as MultiBaseVertex,
// for the same reason: a resolved symbol is not proof the driver implements it.
// ---------------------------------------------------------------------------

void mg_glMultiDrawElements_multiarrays(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                                        GLsizei primcount) {
    LOG()
    multidraw_check_context();

    if (g_scratch.elements_multiarrays_state == md_probe_state_t::Failed || !g_mde_ext) {
        md_fall_elements(md_backend_t::MultiArrays, mode, count, type, indices, primcount);
        return;
    }

    if (!mg_multidraw_enter(count, type, primcount, indices)) return;

    if (mg_multidraw_restart_takeover(mode, count, type, indices, primcount, nullptr)) return;
    md_restart_scope_t restart_scope(type);

    prepareForDraw();

    const bool probing = (g_scratch.elements_multiarrays_state == md_probe_state_t::Unprobed);
    if (probing) mg_md_drain();

    g_mde_ext(mode, count, type, indices, primcount);

    if (probing) {
        const GLenum err = mg_md_check();
        if (err != GL_NO_ERROR) {
            MD_WARN_ONCE("multidraw multiarrays: glMultiDrawElementsEXT failed with 0x%04x, disabling it", err);
            g_scratch.elements_multiarrays_state = md_probe_state_t::Failed;
            md_fall_elements(md_backend_t::MultiArrays, mode, count, type, indices, primcount);
            return;
        }
        g_scratch.elements_multiarrays_state = md_probe_state_t::Working;
    }

    CHECK_GL_ERROR
}

// ---------------------------------------------------------------------------
// glMultiDrawArrays family
//
// lookup.cpp does not mangle these names: there is one exported definition per
// entry point that picks its own backend, so the mg_glMultiDrawArrays_* symbols
// are internal and their suffixes do not have to match md_backend_suffix().
// ---------------------------------------------------------------------------

void mg_glMultiDrawArrays_unroll(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) {
    LOG()
    if (!mg_validate_multidraw_arrays(first, count, drawcount)) return;

    prepareForDraw();
    for (GLsizei i = 0; i < drawcount; ++i) {
        if (count[i] > 0) GLES.glDrawArrays(mode, first[i], count[i]);
    }
    CHECK_GL_ERROR
}

void mg_glMultiDrawArrays_multiarrays(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) {
    LOG()
    // The latch below is per-context, so it has to be re-armed before it is read.
    multidraw_check_context();

    if (g_scratch.arrays_multiarrays_state == md_probe_state_t::Failed || !g_mda_ext) {
        md_fall_arrays(md_backend_t::MultiArrays, mode, first, count, drawcount);
        return;
    }
    if (!mg_validate_multidraw_arrays(first, count, drawcount)) return;

    prepareForDraw();

    const bool probing = (g_scratch.arrays_multiarrays_state == md_probe_state_t::Unprobed);
    if (probing) mg_md_drain();

    g_mda_ext(mode, first, count, drawcount);

    if (probing) {
        const GLenum err = mg_md_check();
        if (err != GL_NO_ERROR) {
            MD_WARN_ONCE("multidraw multiarrays: glMultiDrawArraysEXT failed with 0x%04x, disabling it", err);
            g_scratch.arrays_multiarrays_state = md_probe_state_t::Failed;
            md_fall_arrays(md_backend_t::MultiArrays, mode, first, count, drawcount);
            return;
        }
        g_scratch.arrays_multiarrays_state = md_probe_state_t::Working;
    }
    CHECK_GL_ERROR
}

void mg_glMultiDrawArrays_multiindirect(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount) {
    LOG()
    multidraw_check_context();

    if (g_scratch.arrays_multiindirect_state == md_probe_state_t::Failed || !GLES.glMultiDrawArraysIndirectEXT) {
        md_fall_arrays(md_backend_t::MultiIndirect, mode, first, count, drawcount);
        return;
    }
    if (!mg_validate_multidraw_arrays(first, count, drawcount)) return;

    // GLES requires a vertex array object for an indirect draw, and requires
    // every enabled array to be buffer-backed. The next rung in the user's order
    // (multiarrays or the unrolled loop) is legal with VAO 0, so this check
    // decides between them rather than reporting an error.
    //
    // Asked of the driver on purpose: the question is whether the *driver* is in a
    // state that permits an indirect draw, and gl/gl.cpp's depth-clear triangle
    // leaves it on vertex array 0 while find_bound_array() still names the
    // application's. Trusting the tracked name there would issue a draw GLES
    // rejects and take the fallback away.
    GLint vao = 0;
    GLES.glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    if (vao == 0) {
        LOG_D("multidraw arrays: no vertex array object bound, falling back")
        md_fall_arrays(md_backend_t::MultiIndirect, mode, first, count, drawcount);
        return;
    }

    // Indirect draws are not allowed while transform feedback is active and not
    // paused; the other backends are.
    {
        GLint tf_active = 0, tf_paused = 0;
        GLES.glGetIntegerv(GL_TRANSFORM_FEEDBACK_ACTIVE, &tf_active);
        GLES.glGetIntegerv(GL_TRANSFORM_FEEDBACK_PAUSED, &tf_paused);
        if (tf_active && !tf_paused) {
            LOG_D("multidraw arrays: transform feedback active, falling back")
            md_fall_arrays(md_backend_t::MultiIndirect, mode, first, count, drawcount);
            return;
        }
    }

    prepareForDraw();

    {
        md_indirect_binding_scope_t indirect_restore;
        if (!prepare_arrays_indirect_buffer(first, count, drawcount)) {
            md_fall_arrays(md_backend_t::MultiIndirect, mode, first, count, drawcount);
            return;
        }

        const bool probing = (g_scratch.arrays_multiindirect_state == md_probe_state_t::Unprobed);
        if (probing) mg_md_drain();

        GLES.glMultiDrawArraysIndirectEXT(mode, 0, drawcount, 0);

        if (probing) {
            const GLenum err = mg_md_check();
            if (err != GL_NO_ERROR) {
                MD_WARN_ONCE("multidraw arrays: glMultiDrawArraysIndirectEXT failed with 0x%04x, disabling", err);
                g_scratch.arrays_multiindirect_state = md_probe_state_t::Failed;
                md_fall_arrays(md_backend_t::MultiIndirect, mode, first, count, drawcount);
                return;
            }
            g_scratch.arrays_multiindirect_state = md_probe_state_t::Working;
        }
    }

    CHECK_GL_ERROR
}

// ---------------------------------------------------------------------------
// glMultiDrawArraysIndirect / glMultiDrawElementsIndirect
//
// The application already supplies the commands in a GPU buffer, so the only
// choice is whether to hand the whole batch to the driver or walk it one
// command at a time. The runtime checks stay as a safety net: resolution only
// picks multiindirect when it is available, but the entry point is also
// reachable by direct dlsym.
// ---------------------------------------------------------------------------

void glMultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride) {
    ScopedHostContext __hostCtx;
    LOG()
    if (drawcount <= 0) return;
    if (stride < 0) {
        MD_WARN_ONCE("glMultiDrawArraysIndirect: negative stride %d", stride);
        return;
    }

    prepareForDraw();

    const bool want_batch =
        multidraw_backend_of(md_entry_t::ArraysIndirect) == md_backend_t::MultiIndirect && mg_multi_draw_indirect_available();

    if (want_batch) {
        GLES.glMultiDrawArraysIndirectEXT(mode, indirect, drawcount, stride);
    } else if (GLES.glDrawArraysIndirect) {
        // GL 4.6 sec. 10.5: stride 0 means the commands are tightly packed.
        const GLsizei s = stride ? stride : static_cast<GLsizei>(sizeof(draw_arrays_indirect_command_t));
        const uintptr_t base = reinterpret_cast<uintptr_t>(indirect);
        for (GLsizei i = 0; i < drawcount; ++i) {
            GLES.glDrawArraysIndirect(
                mode, reinterpret_cast<const void*>(base + static_cast<uintptr_t>(i) * static_cast<uintptr_t>(s)));
        }
    } else {
        MD_WARN_ONCE("glMultiDrawArraysIndirect: no indirect draw support on this context");
    }
    CHECK_GL_ERROR
}

void glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount,
                                 GLsizei stride) {
    ScopedHostContext __hostCtx;
    LOG()
    if (drawcount <= 0) return;
    if (stride < 0) {
        MD_WARN_ONCE("glMultiDrawElementsIndirect: negative stride %d", stride);
        return;
    }

    prepareForDraw();

    // Indexed, so restart applies here too. The commands live in a GPU buffer and
    // may have been written by the GPU, so the index stream cannot be rewritten
    // on the way past; the fixed-index form is all that can be honoured.
    md_restart_scope_t restart_scope(type);
    if (mg_restart_needs_rewrite(type)) {
        MD_WARN_ONCE("glMultiDrawElementsIndirect: GL_PRIMITIVE_RESTART with a custom index cannot be emulated "
                     "on an indirect draw; restarts will be ignored");
    }

    const bool want_batch = multidraw_backend_of(md_entry_t::ElementsIndirect) == md_backend_t::MultiIndirect &&
                            mg_multi_draw_indirect_available();

    if (want_batch) {
        GLES.glMultiDrawElementsIndirectEXT(mode, type, indirect, drawcount, stride);
    } else if (GLES.glDrawElementsIndirect) {
        const GLsizei s = stride ? stride : static_cast<GLsizei>(sizeof(draw_elements_indirect_command_t));
        const uintptr_t base = reinterpret_cast<uintptr_t>(indirect);
        for (GLsizei i = 0; i < drawcount; ++i) {
            GLES.glDrawElementsIndirect(
                mode, type, reinterpret_cast<const void*>(base + static_cast<uintptr_t>(i) * static_cast<uintptr_t>(s)));
        }
    } else {
        MD_WARN_ONCE("glMultiDrawElementsIndirect: no indirect draw support on this context");
    }
    CHECK_GL_ERROR
}

// ---------------------------------------------------------------------------
// Backend: Compute (fuse every sub-draw into one rebased 32-bit index stream)
//
// The only backend that can serve a base-vertex multi-draw at full speed on a
// driver with no base-vertex extension at all: instead of N draws, or N CPU
// rebases, the indices of every sub-draw are concatenated into one buffer by a
// compute shader and the whole batch becomes a single glDrawElements.
//
// This is a *simulated* backend in the gl/drawing.h sense, done on the GPU: the
// obvious CPU implementation of the same idea is exactly what the drawelements
// backend does, and it costs a map/upload/draw per sub-draw. Pushing the rebase
// through a compute shader keeps the whole batch to one draw call and one
// dispatch.
//
// Four shader storage blocks, which is the GLES 3.1 guaranteed minimum. The
// draw-lookup table is folded into the prefix-sum buffer for exactly that
// reason: a fifth block would drop support on minimal drivers.
// ---------------------------------------------------------------------------

const std::string multidraw_comp_shader =
    R"(#version 310 es

layout(local_size_x = 64) in;

layout(location = 0) uniform uint uElementSize;

layout(std430, binding = 0) readonly buffer Input { uint in_indices[]; };
// .x = firstIndex (element offset of the sub-draw), .y = baseVertex.
// Kept in one block so the shader needs only four shader storage blocks, the
// minimum GLES 3.1 guarantees.
layout(std430, binding = 1) readonly buffer DrawCmd { ivec2 drawCmd[]; };
layout(std430, binding = 2) readonly buffer Prefix { uint prefixSums[]; };
layout(std430, binding = 3) writeonly buffer Output { uint out_indices[]; };

uint read_index(uint elementIndex) {
    if (uElementSize == 4u) {
        return in_indices[elementIndex];
    }
    if (uElementSize == 2u) {
        uint word = in_indices[elementIndex >> 1u];
        uint shift = (elementIndex & 1u) * 16u;
        return (word >> shift) & 0xFFFFu;
    }
    uint word = in_indices[elementIndex >> 2u];
    uint shift = (elementIndex & 3u) * 8u;
    return (word >> shift) & 0xFFu;
}

void main() {
    uint outIdx = gl_GlobalInvocationID.x;
    // drawCmd holds one entry per sub-draw, so its length is the draw count.
    // The coarse level-1 table is packed into the tail of the Prefix buffer,
    // right after the `drawCount` fine prefix sums (the host packs it there to
    // keep the SSBO count at the GLES 3.1 guaranteed minimum of four).
    uint drawCount = uint(drawCmd.length());
    if (drawCount == 0u) {
        return;
    }
    uint total = prefixSums[drawCount - 1u];
    if (outIdx >= total) {
        return;
    }

    // Level-1 (coarse): one cumulative count per 64-draw bucket, stored at
    // Prefix[drawCount + b]. Searching this tiny table first costs a handful of
    // reads off a cache-hot span no matter how many sub-draws there are, leaving
    // one contiguous 64-entry fine span for the exact draw -- instead of a
    // full-draw-count binary search with scattered random accesses per output
    // index (which is what dominates the kernel on scenes with many chunks).
    uint coarseBase = drawCount;
    uint level1Count = (drawCount + 63u) / 64u;
    int blo = 0;
    int bhi = int(level1Count) - 1;
    while (blo < bhi) {
        int bmid = blo + (bhi - blo) / 2;
        if (prefixSums[coarseBase + uint(bmid)] > outIdx) {
            bhi = bmid; // next [blo, bmid)
        } else {
            blo = bmid + 1; // next [bmid + 1, bhi]
        }
    }

    // Fine search restricted to bucket `blo` = draws [blo*64, min((blo+1)*64, drawCount)).
    int low = blo * 64;
    int high = int(min(drawCount, uint(blo + 1) * 64u)) - 1;
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (prefixSums[mid] > outIdx) {
            high = mid; // next [low, mid)
        } else {
            low = mid + 1; // next [mid + 1, high)
        }
    }

    uint localIdx = outIdx - ((low == 0) ? 0u : (prefixSums[low - 1]));
    uint inIndex = localIdx + uint(drawCmd[low].x);

    int idx = int(read_index(inIndex));
    out_indices[outIdx] = uint(idx + drawCmd[low].y);
}

)";

// Compiles and links one compute program from source. Generic because this file
// builds two of them; the caller looks up whatever uniforms its own shader
// declares.
static GLuint compile_compute_program(const std::string& src, const char* what) {
    char compile_info[1024] = {};

    auto program = GLES.glCreateProgram();
    GLuint shader = GLES.glCreateShader(GL_COMPUTE_SHADER);
    if (program == 0 || shader == 0) {
        LOG_W_FORCE("multidraw %s: compute shaders are unavailable on this context", what)
        if (shader) GLES.glDeleteShader(shader);
        if (program) GLES.glDeleteProgram(program);
        return 0;
    }

    const char* s[] = {src.c_str()};
    const GLint length[] = {static_cast<GLint>(src.length())};
    GLES.glShaderSource(shader, 1, s, length);
    GLES.glCompileShader(shader);

    int success = 0;
    GLES.glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLES.glGetShaderInfoLog(shader, sizeof(compile_info), NULL, compile_info);
        LOG_W_FORCE("multidraw %s: shader compile error: %s", what, compile_info)
#if DEBUG || GLOBAL_DEBUG
        abort();
#endif
        GLES.glDeleteShader(shader);
        GLES.glDeleteProgram(program);
        return 0;
    }

    GLES.glAttachShader(program, shader);
    GLES.glLinkProgram(program);

    GLES.glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLES.glGetProgramInfoLog(program, sizeof(compile_info), NULL, compile_info);
        LOG_W_FORCE("multidraw %s: program link error: %s", what, compile_info)
#if DEBUG || GLOBAL_DEBUG
        abort();
#endif
        GLES.glDeleteShader(shader);
        GLES.glDeleteProgram(program);
        return 0;
    }

    GLES.glDeleteShader(shader);
    return program;
}

// ---------------------------------------------------------------------------
// Shader storage binding save/restore
//
// This is the one place in the file that cannot use the CPU-side trackers, and
// the reason is documented where it matters: gl/gl.cpp's depth-clear triangle
// leaves the driver on program 0 and (the first time it runs in a context) on
// GL_ARRAY_BUFFER 0 without restoring the application's, and
// glBindBufferBase/Range set the generic GL_SHADER_STORAGE_BUFFER binding
// without gl/buffer.cpp recording it. Both of the values saved here are written
// back to the driver, so a stale tracker value would clobber the caller's state
// rather than restore it.
//
// `count` slots are saved. start/size are only read when the slot is non-empty:
// an unbound slot is restored as glBindBufferBase regardless, and skipping the
// two 64-bit queries there removes up to six driver round-trips per compute
// draw on applications that never use indexed storage bindings.
// ---------------------------------------------------------------------------
namespace {

template <int N>
struct md_ssbo_binding_scope_t {
    GLint generic = 0;
    GLint program = 0;
    GLint base[N] = {};
    GLint64 start[N] = {};
    GLint64 size[N] = {};

    md_ssbo_binding_scope_t() {
        GLES.glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &generic);
        GLES.glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        for (int i = 0; i < N; ++i) {
            GLES.glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, i, &base[i]);
            if (base[i] != 0 && GLES.glGetInteger64i_v) {
                GLES.glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START, i, &start[i]);
                GLES.glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE, i, &size[i]);
            }
        }
    }

    void restore() const {
        for (int i = 0; i < N; ++i) {
            // glBindBufferBase is equivalent to binding the whole buffer, so a
            // sub-range binding has to be restored as a range or it silently
            // widens to the entire buffer.
            if (base[i] != 0 && size[i] > 0) {
                GLES.glBindBufferRange(GL_SHADER_STORAGE_BUFFER, i, static_cast<GLuint>(base[i]),
                                       static_cast<GLintptr>(start[i]), static_cast<GLsizeiptr>(size[i]));
            } else {
                GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, static_cast<GLuint>(base[i]));
            }
        }
        GLES.glBindBuffer(GL_SHADER_STORAGE_BUFFER, static_cast<GLuint>(generic));
        GLES.glUseProgram(static_cast<GLuint>(program));
    }

    md_ssbo_binding_scope_t(const md_ssbo_binding_scope_t&) = delete;
    md_ssbo_binding_scope_t& operator=(const md_ssbo_binding_scope_t&) = delete;
};

} // namespace

// Re-specify a grow-only scratch SSBO and verify the allocation, skipping the
// verification round-trip when this context has already proven the buffer can
// hold this many bytes.
//
// Re-specifying rather than updating is what keeps the previous frame's draw --
// which may still be reading this buffer -- from racing this dispatch's writes
// (a write-after-read hazard). Do NOT turn this into glBufferSubData without
// adding a fence.
static bool md_respecify_ssbo(GLuint buf, size_t bytes, const void* data, const char* what, size_t* cap) {
    GLES.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    GLES.glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(bytes), data, GL_DYNAMIC_DRAW);
    if (bytes <= *cap) return true;

    GLint got = 0;
    GLES.glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &got);
    if (got < 0 || static_cast<size_t>(got) < bytes) {
        LOG_W_FORCE("multidraw %s: SSBO allocation failed (wanted %zu bytes, got %d)", what, bytes, got)
        return false;
    }
    *cap = static_cast<size_t>(got);
    return true;
}

GLAPI GLAPIENTRY void mg_glMultiDrawElementsBaseVertex_compute(GLenum mode, GLsizei* counts, GLenum type,
                                                               const void* const* indices, GLsizei primcount,
                                                               const GLint* basevertex) {
    LOG()
    if (!mg_multidraw_enter(counts, type, primcount, indices)) return;

    // Latched: without this a context that cannot compile the program used to
    // re-run glCreateShader/glCompileShader/glLinkProgram on every single call.
    if (g_scratch.compute_failed) {
        md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    const GLuint elementSize = static_cast<GLuint>(mg_index_size(type));
    if (elementSize == 0) {
        md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    if (is_strip_like_mode(mode)) {
        LOG_D("multidraw compute: strip/loop mode, fallback")
        md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    // Every sub-draw is concatenated into one glDrawElements, which loses the rule
    // that each draw command discards the leftover vertices at its end (GL 4.6
    // sec. 10.1). That only stays invisible while every count is a whole number of
    // primitives; otherwise the tail of one sub-draw is joined with the head of the
    // next -- across two different base vertices.
    const GLsizei verts_per_prim = mg_verts_per_primitive(mode);
    if (verts_per_prim == 0) {
        // Unknown or non-separable mode (GL_PATCHES, anything new): fusing is not
        // provably safe, so do not.
        MD_WARN_ONCE("multidraw compute: mode 0x%04x cannot be fused safely, falling back", mode);
        md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
        return;
    }
    for (GLsizei i = 0; i < primcount; ++i) {
        if (counts[i] % verts_per_prim != 0) {
            MD_WARN_ONCE("multidraw compute: sub-draw count is not a whole number of primitives, falling back");
            md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
            return;
        }
    }

    // Every sub-draw ends up in a single fused stream, so an 8- or 16-bit restart
    // sentinel would be widened into an ordinary 32-bit value and offset by
    // baseVertex, silently disabling restart. The CPU path handles sentinels.
    if (mg_primitive_restart_enabled()) {
        LOG_D("multidraw compute: primitive restart enabled, fallback")
        md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    if (!g_scratch.compute_inited) {
        LOG_D("Initializing multidraw compute pipeline...")
        GLES.glGenBuffers(1, &g_scratch.prefix_sum_buffer);
        GLES.glGenBuffers(1, &g_scratch.draw_cmd_buffer);
        GLES.glGenBuffers(1, &g_scratch.output_ibo);

        g_scratch.compute_program = compile_compute_program(multidraw_comp_shader, "index fusion");
        if (g_scratch.compute_program != 0) {
            g_scratch.element_size_loc = GLES.glGetUniformLocation(g_scratch.compute_program, "uElementSize");
            if (g_scratch.element_size_loc < 0) {
                // uElementSize drives read_index()'s 8/16/32-bit unpacking. Without
                // it the uniform stays 0, every index is decoded as if it were
                // 8-bit, and the draw silently renders garbage.
                MD_WARN_ONCE("multidraw compute: uElementSize uniform not found, disabling compute mode");
                GLES.glDeleteProgram(g_scratch.compute_program);
                g_scratch.compute_program = 0;
            }
        }
        if (g_scratch.compute_program == 0) {
            MD_WARN_ONCE("multidraw compute: pipeline init failed, falling back for the rest of this context");
            GLES.glDeleteBuffers(1, &g_scratch.prefix_sum_buffer);
            GLES.glDeleteBuffers(1, &g_scratch.draw_cmd_buffer);
            GLES.glDeleteBuffers(1, &g_scratch.output_ibo);
            g_scratch.prefix_sum_buffer = 0;
            g_scratch.draw_cmd_buffer = 0;
            g_scratch.output_ibo = 0;
            g_scratch.compute_failed = true;
            md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
            return;
        }

        g_scratch.max_compute_groups_x = 0;
        GLES.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &g_scratch.max_compute_groups_x);
        if (g_scratch.max_compute_groups_x <= 0) g_scratch.max_compute_groups_x = 65535; // GLES 3.1 guaranteed minimum
        LOG_D("multidraw compute: max work group count x = %d", g_scratch.max_compute_groups_x)

        g_scratch.compute_inited = true;
    }

    // Tracked rather than queried. This is read at the point the pipeline objects
    // have only just been created and nothing has been bound yet, and the value is
    // both a shader storage source and the binding restored at the very end, so it
    // has to be the driver-side name -- which is what mg_driver_bound_buffer gives.
    const GLuint ibo = mg_driver_bound_buffer(GL_ELEMENT_ARRAY_BUFFER);
    if (ibo == 0) {
        LOG_D("multidraw compute: no element array buffer bound, fallback")
        md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
        return;
    }
    GLint ibo_size = 0;
    GLES.glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &ibo_size);
    if (ibo_size <= 0) {
        MD_WARN_ONCE("multidraw compute: invalid index buffer size, falling back");
        md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
        return;
    }
    if (elementSize < 4 && (ibo_size % 4) != 0) {
        MD_WARN_ONCE("multidraw compute: index buffer size is not 4-byte aligned, falling back");
        md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    // The fused stream is drawn as 32-bit indices with a GLsizei count, and one
    // dispatch has to cover it, so bound the total by both limits up front.
    const uint64_t max_total =
        std::min<uint64_t>(static_cast<uint64_t>(std::numeric_limits<GLint>::max()) / sizeof(GLuint),
                           static_cast<uint64_t>(g_scratch.max_compute_groups_x) * 64ull);

    // Two-level prefix sum for the fused-index kernel's draw lookup.
    //
    // The fine level is one entry per sub-draw, as before. The coarse level
    // stores, per 64-draw bucket, the cumulative index count at the end of that
    // bucket, packed into the tail of the SAME buffer right after the fine
    // entries. The kernel first binary-searches the tiny coarse table (a few
    // cache-hot entries regardless of sub-draw count), then narrows to the
    // contiguous fine entries of a single bucket -- turning most per-output-index
    // random accesses into reads of a small hot table plus a contiguous 64-entry
    // span, instead of one search over the whole fine array. Reusing the prefix
    // buffer keeps the SSBO count at the GLES 3.1 guaranteed minimum of four
    // compute storage blocks (a fifth block would drop support on minimal
    // drivers).
    constexpr GLuint kLevel1Stride = 64u;
    const GLuint level1_count =
        static_cast<GLuint>((static_cast<GLuint>(primcount) + kLevel1Stride - 1u) / kLevel1Stride);
    // Reused grow-only scratch (never shrunk), the same pattern as the indirect
    // path's `staged` and the CPU path's `rebased`. The vectors were built fresh
    // on every call before, paying a malloc/free per multi-draw compute call for
    // no observable benefit. Every failure-fallback above returns before this
    // point, so only the first `primcount` fine entries and the level-1 tail are
    // written; the value left beyond that is stale but never read.
    static thread_local std::vector<GLuint> prefix_data;
    static thread_local std::vector<drawcmd_compute_t> drawcmds;
    prefix_data.resize(static_cast<size_t>(primcount) + level1_count);
    drawcmds.resize(static_cast<size_t>(primcount));

    GLuint* const prefix_sum = prefix_data.data();         // [0, primcount)
    GLuint* const level1 = prefix_data.data() + primcount; // [primcount, ...)

    bool ok = true;
    uint64_t running = 0;
    for (GLsizei i = 0; i < primcount && ok; ++i) {
        const GLsizei c = counts[i] > 0 ? counts[i] : 0;
        running += static_cast<uint64_t>(c);
        if (running > max_total) {
            MD_WARN_ONCE("multidraw compute: fused index count exceeds the dispatch limit, falling back");
            ok = false;
            break;
        }
        prefix_sum[i] = static_cast<GLuint>(running);

        drawcmds[i].firstIndex = 0;
        drawcmds[i].baseVertex = basevertex ? basevertex[i] : 0;
        if (c == 0) continue;

        // indices[i] == 0 is a perfectly legal byte offset here: an element
        // array buffer is bound (checked above), so this is never a client
        // pointer. Rejecting it used to push every arena-style batch whose
        // first sub-draw starts at offset 0 onto the slowest path.
        const uint64_t byteOffset = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(indices[i]));
        if ((byteOffset % elementSize) != 0) {
            // Truncating the division here would shift the whole sub-draw onto
            // the wrong indices, which looks plausible but is wrong.
            MD_WARN_ONCE("multidraw compute: misaligned index offset, falling back");
            ok = false;
            break;
        }
        const uint64_t byteEnd = byteOffset + static_cast<uint64_t>(c) * elementSize;
        if (byteEnd > static_cast<uint64_t>(ibo_size)) {
            MD_WARN_ONCE("multidraw compute: index range out of bounds, falling back");
            ok = false;
            break;
        }
        const uint64_t elementOffset = byteOffset / elementSize;
        if (elementOffset > static_cast<uint64_t>(std::numeric_limits<GLint>::max())) {
            MD_WARN_ONCE("multidraw compute: index offset overflow, falling back");
            ok = false;
            break;
        }
        drawcmds[i].firstIndex = static_cast<GLuint>(elementOffset);
    }

    if (!ok) {
        md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    // Bucket-end cumulative totals. level1[b] == prefix_sum at draw
    // min((b+1)*64, primcount)-1, i.e. the count of output indices strictly
    // before the start of bucket b+1. The final entry equals total_indices.
    for (GLuint b = 0; b < level1_count; ++b) {
        const GLuint last = std::min((b + 1u) * kLevel1Stride, static_cast<GLuint>(primcount)) - 1u;
        level1[b] = prefix_sum[last];
    }

    // The fine entries are prefix_data[0, primcount); the fused total is the
    // last fine entry (== level1[level1_count-1]).
    const GLuint total_indices = prefix_data[static_cast<size_t>(primcount) - 1u];
    if (total_indices == 0) return;

    prepareForDraw();

    // Saved before anything is bound. The generic shader-storage binding and the
    // current program are both read back and written out by restore(), and both
    // are deliberately driver queries (see md_ssbo_binding_scope_t).
    md_ssbo_binding_scope_t<4> ssbo_restore;

    // Both stores are verified by query, like the output buffer below. The prefix
    // store especially: the shader reads drawCount from drawCmd.length() and the
    // coarse level-1 table from Prefix[drawCount ..], so a short allocation would
    // make invocations read out of bounds, and the draw below would consume an
    // uninitialised index buffer.
    if (!md_respecify_ssbo(g_scratch.draw_cmd_buffer, sizeof(drawcmd_compute_t) * static_cast<size_t>(primcount),
                           drawcmds.data(), "draw command buffer", &g_scratch.draw_cmd_cap) ||
        !md_respecify_ssbo(g_scratch.prefix_sum_buffer, sizeof(GLuint) * prefix_data.size(), prefix_data.data(),
                           "prefix sum buffer", &g_scratch.prefix_sum_cap)) {
        ssbo_restore.restore();
        md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    // Reallocating rather than updating is also what keeps the previous frame's
    // draw from racing this dispatch (write-after-read); do not turn this into
    // glBufferSubData without adding a fence.
    const size_t output_bytes = sizeof(GLuint) * static_cast<size_t>(total_indices);
    GLES.glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_scratch.output_ibo);
    GLES.glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(output_bytes), nullptr, GL_DYNAMIC_DRAW);

    // Verify by query rather than by glGetError: the output buffer is about to be
    // both the compute target and the index source, so drawing from a store that
    // was never allocated would render garbage. Like the other two scratch
    // stores, the grow-only output buffer is only re-checked when it has to grow;
    // a steady-state re-spec to an already-proven size skips the round-trip.
    if (output_bytes > g_scratch.output_cap) {
        GLint output_size = 0;
        GLES.glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &output_size);
        if (output_size < 0 || static_cast<size_t>(output_size) < output_bytes) {
            MD_WARN_ONCE("multidraw compute: output buffer allocation failed (wanted %zu bytes, got %d), falling back",
                         output_bytes, output_size);
            ssbo_restore.restore();
            md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
            return;
        }
        g_scratch.output_cap = static_cast<size_t>(output_size);
    }

    GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ibo);
    GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, g_scratch.draw_cmd_buffer);
    GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, g_scratch.prefix_sum_buffer);
    GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, g_scratch.output_ibo);

    LOG_D("Using compute program = %d", g_scratch.compute_program)
    GLES.glUseProgram(g_scratch.compute_program);
    if (g_scratch.element_size_loc >= 0) {
        GLES.glUniform1ui(g_scratch.element_size_loc, elementSize);
    }

    const uint64_t groups = (static_cast<uint64_t>(total_indices) + 63ull) / 64ull;
    if (groups > static_cast<uint64_t>(g_scratch.max_compute_groups_x)) {
        MD_WARN_ONCE("multidraw compute: work group count exceeds the limit, falling back");
        ssbo_restore.restore();
        md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    LOG_D("Dispatch compute")
    mg_md_drain();
    GLES.glDispatchCompute(static_cast<GLuint>(groups), 1, 1);
    const GLenum dispatch_err = mg_md_check();
    if (dispatch_err != GL_NO_ERROR) {
        MD_WARN_ONCE("multidraw compute: glDispatchCompute failed with 0x%04x, disabling compute mode", dispatch_err);
        // A driver that rejects this dispatch will reject the next one too, and
        // reaching this point costs three buffer re-specifications plus the whole
        // binding dance. Latch it so the rest of this context uses the CPU path.
        g_scratch.compute_failed = true;
        // The output buffer was just reallocated, so its contents are undefined:
        // drawing from it would render garbage rather than nothing.
        ssbo_restore.restore();
        md_fall_elements_bv(md_backend_t::Compute, mode, counts, type, indices, primcount, basevertex);
        return;
    }

    GLES.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT);

    // Restore the shader storage bindings *before* the application's draw. They
    // are context state, so leaving the scratch buffers on bindings 0..3 hands
    // them to the application's own shader, and a writable block declared there
    // would corrupt the very index buffer this draw is consuming.
    ssbo_restore.restore();

    LOG_D("draw")
    GLES.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_scratch.output_ibo);
    GLES.glDrawElements(mode, static_cast<GLsizei>(total_indices), GL_UNSIGNED_INT, nullptr);
    GLES.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
}

// glMultiDrawElements has no base vertex component, and the compute pipeline's
// entire job is applying one. There is nothing for it to do on this entry point,
// so it degrades to the spec-defined loop.
void mg_glMultiDrawElements_compute(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                                    GLsizei primcount) {
    LOG()
    if (!mg_multidraw_enter(count, type, primcount, indices)) return;

    prepareForDraw();

    for (GLsizei i = 0; i < primcount; ++i) {
        const GLsizei c = count[i];
        if (c > 0) {
            GLES.glDrawElements(mode, c, type, indices[i]);
        }
    }

    CHECK_GL_ERROR
}

// ---------------------------------------------------------------------------
// glMultiDraw{Arrays,Elements}IndirectCount
//
// The draw count lives in the buffer bound to GL_PARAMETER_BUFFER, which is the
// whole point of the command: it is normally written by the GPU, so reading it
// back on the CPU would mean a full pipeline stall every frame -- and reading it
// without a stall would mean reading a stale value and drawing the wrong number
// of commands.
//
// Instead the commands are compacted on the GPU. A compute shader copies
// maxdrawcount commands into a scratch buffer and sets instanceCount to 0 on
// every command at or past the real count; a command with instanceCount 0 draws
// nothing. One glMultiDraw*IndirectEXT over the scratch buffer then produces
// exactly the requested draws with no readback and no stall.
//
// Drawing maxdrawcount commands unchanged would NOT be a valid shortcut: the
// slots past the count hold stale or zeroed commands, and rendering them puts
// back the geometry the application just culled.
// ---------------------------------------------------------------------------

static const std::string multidraw_count_shader =
    R"(#version 310 es

layout(local_size_x = 64) in;

layout(location = 0) uniform uint uMaxDrawCount;
layout(location = 1) uniform uint uSrcWords;   // stride in words between source commands
layout(location = 2) uniform uint uSrcOffset;  // byte offset of the first command, in words
layout(location = 3) uniform uint uCountWord;  // byte offset of the count, in words
layout(location = 4) uniform uint uDstWords;   // words per command (4 arrays, 5 elements)

layout(std430, binding = 0) readonly buffer Src { uint src[]; };
layout(std430, binding = 1) readonly buffer Param { uint param[]; };
layout(std430, binding = 2) writeonly buffer Dst { uint dst[]; };

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= uMaxDrawCount) {
        return;
    }

    // Only decides whether this command is kept. The read bounds come from the
    // early return above and from the host-side check that maxdrawcount commands
    // fit inside the source buffer, so a corrupt count cannot widen them.
    uint realCount = param[uCountWord];

    uint sbase = uSrcOffset + i * uSrcWords;
    uint dbase = i * uDstWords;
    for (uint w = 0u; w < uDstWords; ++w) {
        dst[dbase + w] = src[sbase + w];
    }
    if (i >= realCount) {
        dst[dbase + 1u] = 0u; // instanceCount, word 1 of both command layouts
    }
}

)";

static bool mg_count_init() {
    if (g_scratch.count_failed) return false;
    if (g_scratch.count_inited) return true;

    if (!GLES.glDispatchCompute) {
        MD_WARN_ONCE("multidraw count: compute shaders are unavailable, cannot honour *IndirectCount");
        g_scratch.count_failed = true;
        return false;
    }

    g_scratch.count_program = compile_compute_program(multidraw_count_shader, "count compaction");
    if (g_scratch.count_program == 0) {
        MD_WARN_ONCE("multidraw count: compaction shader failed to build");
        g_scratch.count_failed = true;
        return false;
    }
    g_scratch.count_loc_max = GLES.glGetUniformLocation(g_scratch.count_program, "uMaxDrawCount");
    g_scratch.count_loc_srcwords = GLES.glGetUniformLocation(g_scratch.count_program, "uSrcWords");
    g_scratch.count_loc_srcoff = GLES.glGetUniformLocation(g_scratch.count_program, "uSrcOffset");
    g_scratch.count_loc_cntoff = GLES.glGetUniformLocation(g_scratch.count_program, "uCountWord");
    g_scratch.count_loc_dstwords = GLES.glGetUniformLocation(g_scratch.count_program, "uDstWords");
    if (g_scratch.count_loc_max < 0 || g_scratch.count_loc_srcwords < 0 || g_scratch.count_loc_srcoff < 0 ||
        g_scratch.count_loc_cntoff < 0 || g_scratch.count_loc_dstwords < 0) {
        MD_WARN_ONCE("multidraw count: compaction shader is missing uniforms");
        GLES.glDeleteProgram(g_scratch.count_program);
        g_scratch.count_program = 0;
        g_scratch.count_failed = true;
        return false;
    }

    g_scratch.count_scratch.name = 0;
    g_scratch.count_scratch.cap = 0;
    g_scratch.count_inited = true;
    return true;
}

// Returns true when the compacted draw was issued.
static bool mg_indirect_count(GLenum mode, GLenum type, bool is_elements, const void* indirect, GLintptr drawcount,
                              GLsizei maxdrawcount, GLsizei stride) {
    if (maxdrawcount <= 0) return true; // nothing to draw, and that is not an error

    const GLsizei cmd_bytes = is_elements ? static_cast<GLsizei>(sizeof(draw_elements_indirect_command_t))
                                          : static_cast<GLsizei>(sizeof(draw_arrays_indirect_command_t));
    const GLsizei src_stride = stride ? stride : cmd_bytes;
    const uintptr_t src_off = reinterpret_cast<uintptr_t>(indirect);

    if (stride < 0 || (src_stride % 4) != 0 || (src_off % 4) != 0 || (drawcount % 4) != 0 || drawcount < 0) {
        MD_WARN_ONCE("multidraw count: stride/offsets must be non-negative multiples of 4");
        return false;
    }
    if (src_stride < cmd_bytes) {
        MD_WARN_ONCE("multidraw count: stride %d is smaller than one command", src_stride);
        return false;
    }

    multidraw_check_context();
    if (!mg_count_init()) return false;

    const GLuint param_real = find_real_buffer(find_bound_buffer(GL_PARAMETER_BUFFER_BINDING));
    if (param_real == 0) {
        MD_WARN_ONCE("multidraw count: no GL_PARAMETER_BUFFER bound, nothing drawn");
        return false;
    }
    // Tracked, like the parameter buffer above: this ends up as a shader storage
    // source and as a bind target, so it has to be the driver-side name, and
    // nothing has been bound yet at this point.
    const GLuint src_bound = mg_driver_bound_buffer(GL_DRAW_INDIRECT_BUFFER);
    if (src_bound == 0) {
        MD_WARN_ONCE("multidraw count: no GL_DRAW_INDIRECT_BUFFER bound, nothing drawn");
        return false;
    }

    // Save everything the dispatch is about to take over BEFORE touching any of
    // it. Reading GL_SHADER_STORAGE_BUFFER_BINDING after the scratch sizing block
    // had already bound and unbound the scratch buffer would have recorded 0 and
    // then "restored" that, silently clearing the application's binding.
    md_ssbo_binding_scope_t<3> ssbo_restore;
    md_indirect_binding_scope_t indirect_restore;

    // Every source command must be inside the bound indirect buffer, and the
    // count must be inside the parameter buffer. The shader indexes both as raw
    // uint arrays, and an out-of-range SSBO access is undefined in GLES, not an
    // error. maxdrawcount is allowed to exceed the number of commands actually
    // stored, so this cannot be assumed.
    const uint64_t src_span = static_cast<uint64_t>(src_off) +
                              static_cast<uint64_t>(maxdrawcount - 1) * static_cast<uint64_t>(src_stride) +
                              static_cast<uint64_t>(cmd_bytes);
    GLint src_size = 0;
    GLES.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, src_bound);
    GLES.glGetBufferParameteriv(GL_DRAW_INDIRECT_BUFFER, GL_BUFFER_SIZE, &src_size);
    if (src_size < 0 || src_span > static_cast<uint64_t>(src_size)) {
        MD_WARN_ONCE("multidraw count: commands run past the end of the indirect buffer (%llu > %d), nothing drawn",
                     static_cast<unsigned long long>(src_span), src_size);
        return false;
    }

    // Accounted in size_t: truncating to GLsizei could wrap negative and make the
    // comparison skip the allocation entirely.
    const size_t dst_bytes = static_cast<size_t>(maxdrawcount) * static_cast<size_t>(cmd_bytes);
    if (dst_bytes > static_cast<size_t>(std::numeric_limits<GLint>::max())) {
        MD_WARN_ONCE("multidraw count: %zu bytes of commands is too large, nothing drawn", dst_bytes);
        return false;
    }

    GLint param_size = 0;
    GLES.glBindBuffer(GL_SHADER_STORAGE_BUFFER, param_real);
    GLES.glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &param_size);
    if (param_size < 0 || static_cast<uint64_t>(drawcount) + 4ull > static_cast<uint64_t>(param_size)) {
        MD_WARN_ONCE("multidraw count: the draw count lies past the end of the parameter buffer, nothing drawn");
        return false;
    }

    // Re-specified unconditionally rather than reused. That is what keeps the
    // previous frame's draw, which may still be fetching commands from this
    // buffer, from racing this dispatch's writes -- the same invariant the index
    // fusion path relies on.
    {
        // count_scratch.ensure caches a capacity, which is wrong here: this store
        // must be re-specified every call, so the capacity is deliberately not
        // trusted as a skip condition.
        if (g_scratch.count_scratch.name == 0) GLES.glGenBuffers(1, &g_scratch.count_scratch.name);
        GLES.glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_scratch.count_scratch.name);
        GLES.glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(dst_bytes), nullptr, GL_DYNAMIC_DRAW);
        GLint got = 0;
        GLES.glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &got);
        if (got < 0 || static_cast<size_t>(got) < dst_bytes) {
            MD_WARN_ONCE("multidraw count: scratch allocation failed (wanted %zu bytes, got %d)", dst_bytes, got);
            return false;
        }
    }

    GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, src_bound);
    GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, param_real);
    GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, g_scratch.count_scratch.name);

    GLES.glUseProgram(g_scratch.count_program);
    GLES.glUniform1ui(g_scratch.count_loc_max, static_cast<GLuint>(maxdrawcount));
    GLES.glUniform1ui(g_scratch.count_loc_srcwords, static_cast<GLuint>(src_stride / 4));
    GLES.glUniform1ui(g_scratch.count_loc_srcoff, static_cast<GLuint>(src_off / 4));
    GLES.glUniform1ui(g_scratch.count_loc_cntoff, static_cast<GLuint>(drawcount / 4));
    GLES.glUniform1ui(g_scratch.count_loc_dstwords, static_cast<GLuint>(cmd_bytes / 4));

    mg_md_drain();
    GLES.glDispatchCompute(static_cast<GLuint>((maxdrawcount + 63) / 64), 1, 1);
    const GLenum err = mg_md_check();
    if (err != GL_NO_ERROR) {
        MD_WARN_ONCE("multidraw count: compaction dispatch failed with 0x%04x, nothing drawn", err);
        ssbo_restore.restore();
        return false;
    }

    GLES.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

    ssbo_restore.restore();

    prepareForDraw();
    GLES.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_scratch.count_scratch.name);

    if (mg_multi_draw_indirect_available()) {
        if (is_elements)
            GLES.glMultiDrawElementsIndirectEXT(mode, type, 0, maxdrawcount, 0);
        else
            GLES.glMultiDrawArraysIndirectEXT(mode, 0, maxdrawcount, 0);
    } else if (is_elements ? GLES.glDrawElementsIndirect != nullptr : GLES.glDrawArraysIndirect != nullptr) {
        // Commands past the count carry instanceCount 0, so walking all of them
        // draws exactly the same thing, one call at a time.
        for (GLsizei i = 0; i < maxdrawcount; ++i) {
            const void* off = reinterpret_cast<const void*>(static_cast<uintptr_t>(i) * cmd_bytes);
            if (is_elements)
                GLES.glDrawElementsIndirect(mode, type, off);
            else
                GLES.glDrawArraysIndirect(mode, off);
        }
    } else {
        MD_WARN_ONCE("multidraw count: no indirect draw entry point, nothing drawn");
        return false;
    }

    return true;
}

void glMultiDrawArraysIndirectCount(GLenum mode, const void* indirect, GLintptr drawcount, GLsizei maxdrawcount,
                                    GLsizei stride) {
    LOG()
    mg_indirect_count(mode, 0, false, indirect, drawcount, maxdrawcount, stride);
}

void glMultiDrawElementsIndirectCount(GLenum mode, GLenum type, const void* indirect, GLintptr drawcount,
                                      GLsizei maxdrawcount, GLsizei stride) {
    LOG()
    // Same as glMultiDrawElementsIndirect: indexed, so restart applies, but the
    // stream is not reachable for rewriting.
    md_restart_scope_t restart_scope(type);
    if (mg_restart_needs_rewrite(type)) {
        MD_WARN_ONCE("glMultiDrawElementsIndirectCount: GL_PRIMITIVE_RESTART with a custom index cannot be emulated "
                     "on an indirect draw; restarts will be ignored");
    }
    mg_indirect_count(mode, type, true, indirect, drawcount, maxdrawcount, stride);
}

// ---------------------------------------------------------------------------
// EXT / ARB aliases
//
// These must exist and must forward: the gl_stub.cpp entries they replace were
// removed unconditionally, and without them the library would export nothing at
// all for these names and glXGetProcAddress would start returning nullptr where
// it used to return a stub.
// ---------------------------------------------------------------------------
#ifndef __APPLE__
extern "C"
{
    GLAPI GLAPIENTRY void glMultiDrawArraysEXT(GLenum mode, const GLint* first, const GLsizei* count, GLsizei primcount)
        __attribute__((alias("glMultiDrawArrays")));
    GLAPI GLAPIENTRY void glMultiDrawElementsEXT(GLenum mode, const GLsizei* count, GLenum type,
                                                 const void* const* indices, GLsizei primcount)
        __attribute__((alias("glMultiDrawElements")));
    GLAPI GLAPIENTRY void glMultiDrawArraysIndirectARB(GLenum mode, const void* indirect, GLsizei drawcount,
                                                       GLsizei stride)
        __attribute__((alias("glMultiDrawArraysIndirect")));
    GLAPI GLAPIENTRY void glMultiDrawElementsIndirectARB(GLenum mode, GLenum type, const void* indirect,
                                                         GLsizei drawcount, GLsizei stride)
        __attribute__((alias("glMultiDrawElementsIndirect")));
    GLAPI GLAPIENTRY void glMultiDrawArraysIndirectCountARB(GLenum mode, const void* indirect, GLintptr drawcount,
                                                            GLsizei maxdrawcount, GLsizei stride)
        __attribute__((alias("glMultiDrawArraysIndirectCount")));
    GLAPI GLAPIENTRY void glMultiDrawElementsIndirectCountARB(GLenum mode, GLenum type, const void* indirect,
                                                              GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
        __attribute__((alias("glMultiDrawElementsIndirectCount")));
}
#else
// Mach-O does not support alias attributes across translation units the way ELF
// does, so the same six names are provided as forwarding definitions.
extern "C"
{
    GLAPI GLAPIENTRY void glMultiDrawArraysEXT(GLenum mode, const GLint* first, const GLsizei* count,
                                               GLsizei primcount) {
        glMultiDrawArrays(mode, first, count, primcount);
    }
    GLAPI GLAPIENTRY void glMultiDrawElementsEXT(GLenum mode, const GLsizei* count, GLenum type,
                                                 const void* const* indices, GLsizei primcount) {
        glMultiDrawElements(mode, count, type, indices, primcount);
    }
    GLAPI GLAPIENTRY void glMultiDrawArraysIndirectARB(GLenum mode, const void* indirect, GLsizei drawcount,
                                                       GLsizei stride) {
        glMultiDrawArraysIndirect(mode, indirect, drawcount, stride);
    }
    GLAPI GLAPIENTRY void glMultiDrawElementsIndirectARB(GLenum mode, GLenum type, const void* indirect,
                                                         GLsizei drawcount, GLsizei stride) {
        glMultiDrawElementsIndirect(mode, type, indirect, drawcount, stride);
    }
    GLAPI GLAPIENTRY void glMultiDrawArraysIndirectCountARB(GLenum mode, const void* indirect, GLintptr drawcount,
                                                            GLsizei maxdrawcount, GLsizei stride) {
        glMultiDrawArraysIndirectCount(mode, indirect, drawcount, maxdrawcount, stride);
    }
    GLAPI GLAPIENTRY void glMultiDrawElementsIndirectCountARB(GLenum mode, GLenum type, const void* indirect,
                                                              GLintptr drawcount, GLsizei maxdrawcount,
                                                              GLsizei stride) {
        glMultiDrawElementsIndirectCount(mode, type, indirect, drawcount, maxdrawcount, stride);
    }
}
#endif
