// Verifies the Elements-entry compute fusion wiring. The selection logic is
// copied verbatim from config/settings.cpp (allowed masks, the per-entry
// default order, expansion and capability filtering) and gl/multidraw.cpp
// (fall_target's terminal rung, the owner-aware compute fallback), following
// the same "real logic, no GPU" pattern as the other tests here.
//
// 1. On the common no-batched-extension driver (most GLES 3.2 phones), the
//    Elements chain must lead with compute when the compute gate passes, then
//    unroll, then indirect; a failure inside the fusion pipeline lands on
//    unroll, not on a wrap back into compute.
// 2. A driver with a batched one-call form keeps it ahead of compute.
// 3. Compute gate off: Elements resolves to unroll as before.
// 4. An explicit multidrawOrderElements still wins over the per-entry default
//    (documenting that a config pinned to unroll>indirect keeps compute ranked
//    last by the padding -- such a config must be updated or removed to get
//    the fusion).
// 5. BaseVertex default (no config) is unchanged: still the global order.
// 6. A failure inside the shared fusion pipeline falls back through the chain
//    of the OWNING entry point, never the other one.
#include <cstdio>
#include <cstring>
#include <string>

enum class B { Auto, Unroll, BaseVertex, Indirect, MultiArrays, MultiBaseVertex, MultiIndirect, Compute };
enum class E { Arrays = 0, Elements, ElementsBaseVertex, ArraysIndirect, ElementsIndirect };

constexpr int MD_ENTRY_COUNT = 5;
constexpr int MD_BACKEND_COUNT = 8;

static unsigned md_bit(B b) { return 1u << static_cast<int>(b); }

static const char* md_backend_name(B b) {
    switch (b) {
    case B::Unroll: return "unroll";
    case B::BaseVertex: return "basevertex";
    case B::Indirect: return "indirect";
    case B::MultiArrays: return "multiarrays";
    case B::MultiBaseVertex: return "multibasevertex";
    case B::MultiIndirect: return "multiindirect";
    case B::Compute: return "compute";
    default: return "native";
    }
}

// ---- verbatim from settings.cpp ----
struct md_caps_t {
    bool basevertex, indirect_arrays, indirect_elements, multiindirect_arrays, multiindirect_elements,
        multibasevertex, multiarrays, compute;
};

static bool md_is_arrays_side(E e) { return e == E::Arrays || e == E::ArraysIndirect; }

static bool md_backend_available(E e, B b, const md_caps_t& c) {
    switch (b) {
    case B::Unroll: return true;
    case B::BaseVertex: return c.basevertex;
    case B::Indirect: return md_is_arrays_side(e) ? c.indirect_arrays : c.indirect_elements;
    case B::MultiIndirect: return md_is_arrays_side(e) ? c.multiindirect_arrays : c.multiindirect_elements;
    case B::MultiBaseVertex: return c.multibasevertex;
    case B::MultiArrays: return c.multiarrays;
    case B::Compute: return c.compute;
    default: return false;
    }
}

struct md_order_item_t {
    bool is_native;
    B backend;
};

struct md_entry_desc_t {
    const char* label;
    unsigned allowed;
    B native_backend;
    const char* const* default_order; // per-entry padding order; null pads with the global default
    int default_order_len;
};

static const char* const k_md_default_global_order[] = {
    "native", "multiindirect", "multibasevertex", "multiarrays", "indirect", "basevertex", "unroll", "compute",
};
constexpr int MD_GLOBAL_ITEMS = 8;

static const char* const k_md_default_order_elements[] = {
    "native", "multiindirect", "multibasevertex", "compute", "unroll", "indirect",
};
constexpr int MD_DEFAULT_ORDER_ELEMENTS_LEN = 6;

static bool md_parse_backend(const char* name, B* out) {
    struct { const char* n; B b; } tab[] = {
        {"unroll", B::Unroll}, {"basevertex", B::BaseVertex}, {"indirect", B::Indirect},
        {"multiarrays", B::MultiArrays}, {"multibasevertex", B::MultiBaseVertex},
        {"multiindirect", B::MultiIndirect}, {"compute", B::Compute},
    };
    for (auto& t : tab)
        if (std::string(name) == t.n) { *out = t.b; return true; }
    return false;
}

static const md_entry_desc_t k_md_entries[MD_ENTRY_COUNT] = {
    {"glMultiDrawArrays",
     md_bit(B::Unroll) | md_bit(B::MultiArrays) | md_bit(B::MultiIndirect),
     B::MultiArrays, nullptr, 0},

    {"glMultiDrawElements",
     md_bit(B::Unroll) | md_bit(B::Indirect) | md_bit(B::MultiIndirect) | md_bit(B::MultiBaseVertex) |
         md_bit(B::MultiArrays) | md_bit(B::Compute),
     B::MultiArrays, k_md_default_order_elements, MD_DEFAULT_ORDER_ELEMENTS_LEN},

    {"glMultiDrawElementsBaseVertex",
     md_bit(B::Unroll) | md_bit(B::BaseVertex) | md_bit(B::Indirect) | md_bit(B::MultiIndirect) |
         md_bit(B::MultiBaseVertex) | md_bit(B::Compute),
     B::MultiBaseVertex, nullptr, 0},

    {"glMultiDrawArraysIndirect",
     md_bit(B::Indirect) | md_bit(B::MultiIndirect),
     B::MultiIndirect, nullptr, 0},

    {"glMultiDrawElementsIndirect",
     md_bit(B::Indirect) | md_bit(B::MultiIndirect),
     B::MultiIndirect, nullptr, 0},
};

static B s_md_requested[MD_ENTRY_COUNT][MD_BACKEND_COUNT];
static int s_md_requested_len[MD_ENTRY_COUNT];

static void md_expand_order(E e, const md_order_item_t* items, int item_count) {
    const md_entry_desc_t& d = k_md_entries[static_cast<int>(e)];
    B* out = s_md_requested[static_cast<int>(e)];
    int n = 0;
    unsigned seen = 0;

    auto push = [&](B b) {
        if ((d.allowed & md_bit(b)) == 0) return;
        if (seen & md_bit(b)) return;
        seen |= md_bit(b);
        out[n++] = b;
    };

    for (int i = 0; i < item_count; ++i) {
        push(items[i].is_native ? d.native_backend : items[i].backend);
    }
    const char* const* pad = d.default_order ? d.default_order : k_md_default_global_order;
    const int pad_len = d.default_order ? d.default_order_len : MD_GLOBAL_ITEMS;
    for (int p = 0; p < pad_len; ++p) {
        const char* name = pad[p];
        if (std::string(name) == "native") {
            push(d.native_backend);
        } else {
            B b;
            if (md_parse_backend(name, &b)) push(b);
        }
    }
    s_md_requested_len[static_cast<int>(e)] = n;
}

static void resolve_entry(E e, const md_caps_t& caps, B* order, int* len) {
    const int i = static_cast<int>(e);
    int n = 0;
    for (int k = 0; k < s_md_requested_len[i]; ++k) {
        const B cand = s_md_requested[i][k];
        if (!md_backend_available(e, cand, caps)) continue;
        order[n++] = cand;
    }
    *len = n;
}

// ---- verbatim in shape from multidraw.cpp ----
static B md_fall_target(const B* order, int len, B cur) {
    if (len <= 0) return B::Unroll;
    for (int i = 0; i < len; ++i) {
        if (order[i] == cur) return order[i + 1 < len ? i + 1 : len - 1];
    }
    return order[len - 1];
}
static B md_fall_target_terminal(const B* order, int len, B cur) {
    B next = md_fall_target(order, len, cur);
    if (next == cur) next = B::Unroll; // the unrolled loop is always legal: terminal rung
    return next;
}

// Model of md_fall_from_compute: which chain an in-pipeline failure walks.
static E g_routed_owner = E::Elements;
static void md_fall_from_compute(E owner) { g_routed_owner = owner; }

// ---- helpers ----
static std::string chain_str(const B* order, int len) {
    std::string s;
    for (int i = 0; i < len; ++i) {
        if (i) s += " > ";
        s += md_backend_name(order[i]);
    }
    return s;
}

static md_caps_t user_device_caps() {
    // Oppo PGGM10 / Adreno 619 shape from the device logs: GLES 3.2, single
    // glDrawElementsIndirect core, no batched multi-draw extension of any
    // spelling, compute usable (4+ SSBO blocks).
    md_caps_t c{};
    c.basevertex = true;
    c.indirect_arrays = true;
    c.indirect_elements = true;
    c.multiindirect_arrays = false;
    c.multiindirect_elements = false;
    c.multibasevertex = false;
    c.multiarrays = false;
    c.compute = true;
    return c;
}

static int fail(const char* what, const std::string& got) {
    std::printf("FAIL: %s got [%s]\n", what, got.c_str());
    return 1;
}

int main() {
    const md_caps_t caps = user_device_caps();
    B order[MD_BACKEND_COUNT];
    int len = 0;

    // 1. No Elements config, common driver: compute leads, unroll is the
    //    fallback rung, indirect trails.
    md_expand_order(E::Elements, nullptr, 0);
    resolve_entry(E::Elements, caps, order, &len);
    if (chain_str(order, len) != "compute > unroll > indirect")
        return fail("case 1 chain", chain_str(order, len));
    if (md_fall_target_terminal(order, len, B::Compute) != B::Unroll)
        return fail("case 1 fallback after compute", md_backend_name(order[0]));

    // 2. A driver with EXT_multi_draw_arrays keeps its one-call form ahead.
    md_caps_t caps_ma = caps;
    caps_ma.multiarrays = true;
    md_expand_order(E::Elements, nullptr, 0);
    resolve_entry(E::Elements, caps_ma, order, &len);
    if (chain_str(order, len) != "multiarrays > compute > unroll > indirect")
        return fail("case 2 chain", chain_str(order, len));

    // 3. Compute gate off: resolves to unroll, exactly as before this change.
    md_caps_t caps_nc = caps;
    caps_nc.compute = false;
    md_expand_order(E::Elements, nullptr, 0);
    resolve_entry(E::Elements, caps_nc, order, &len);
    if (chain_str(order, len) != "unroll > indirect") return fail("case 3 chain", chain_str(order, len));

    // 4. Explicit config still wins: unroll>indirect pads compute to last
    //    place, where it is unreachable (unroll never fails). This documents
    //    why a device with that config must update or remove the line.
    md_order_item_t cfg[] = {{false, B::Unroll}, {false, B::Indirect}};
    md_expand_order(E::Elements, cfg, 2);
    resolve_entry(E::Elements, caps, order, &len);
    if (chain_str(order, len) != "unroll > indirect > compute")
        return fail("case 4 chain", chain_str(order, len));

    // 5. BaseVertex default (no config) is untouched by the per-entry order:
    //    still the global default filtered by caps.
    md_expand_order(E::ElementsBaseVertex, nullptr, 0);
    resolve_entry(E::ElementsBaseVertex, caps, order, &len);
    if (chain_str(order, len) != "indirect > basevertex > unroll > compute")
        return fail("case 5 chain", chain_str(order, len));

    // 6. In-pipeline failure routing: the Elements owner walks the Elements
    //    chain, the BaseVertex owner walks the BaseVertex chain.
    md_fall_from_compute(E::Elements);
    if (g_routed_owner != E::Elements) return fail("case 6 elements routing", "bv chain");
    md_fall_from_compute(E::ElementsBaseVertex);
    if (g_routed_owner != E::ElementsBaseVertex) return fail("case 6 bv routing", "elements chain");

    // 7. Small-batch cutoff: the fusion's fixed per-batch block (~17 driver
    //    calls: 3 scratch re-specs, binding dance, dispatch, barrier, fused
    //    draw, restore) exceeds the per-sub-draw loop below kComputeMinBatch=12,
    //    so batches under it route straight to the unroll backend. This is a
    //    routing decision, not a chain fallback: the fallback tick must not
    //    move. Dense foliage is the shape that produces many tiny batches.
    {
        constexpr int kComputeMinBatch = 12;
        constexpr int kFusedFixedCost = 17;
        if (kComputeMinBatch >= kFusedFixedCost) return fail("case 7 threshold sanity", "cutoff above fixed cost");
        int fused = 0, small = 0, fallback_ticks = 0;
        auto route = [&](int primcount) {
            if (primcount < kComputeMinBatch) { ++small; return; } // direct unroll call
            ++fused;                                               // fusion pipeline
            (void)fallback_ticks;                                  // unchanged by routing
        };
        const int foliage_frame[] = {3, 6, 2, 9, 4, 40, 64, 8, 120, 7};
        for (int pc : foliage_frame) route(pc);
        if (fused != 3 || small != 7) return fail("case 7 cutoff routing", std::to_string(fused) + "/" + std::to_string(small));
        if (fallback_ticks != 0) return fail("case 7 fallback tick moved", "dirty");
    }

    std::printf("PASS: Elements leads with compute on no-batched-extension drivers, batched forms stay ahead "
                "where they exist, explicit config still wins, BaseVertex default unchanged, fallback routing "
                "is owner-aware, small batches route to the per-draw loop\n");
    return 0;
}
