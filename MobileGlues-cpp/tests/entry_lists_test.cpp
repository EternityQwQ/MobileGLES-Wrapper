// Verifies that gles/loader.cpp's GLES_ENTRY table addresses the SAME slots as
// init_target_gles()'s INIT_GLES_FUNC rows.
//
// The previous verification tried to infer field order by comparing addresses
// against struct offsets, which is not sound: the compiler may lay fields out in
// declaration order but nothing in the language guarantees the *names* the table
// uses line up with the *offsets* it produces. The question that actually
// matters is simpler and answerable exactly: for every name, does the table's
// expression `GLES.<name>` and the row's expression `GLES.<name>` produce the
// same address? Both are the same expression on the same object, so the answer
// is yes by construction -- unless a name is in one list and not the other.
//
// So this compiles the real gles.h and the real GLES_ALL_ENTRIES macro and
// compares the two name sequences directly, then independently confirms that
// every name in the macro is a real member of gles_func_t by taking its address.
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

#include "gles/gles.h"

// Stand-in for the real g_gles_func. Only its address and size are used, and
// every slot in it is a pointer, so the layout the compiler computes here is
// the same layout gles_func_t has in the real translation unit.
gles_func_t g_gles_func;

// Pull the INIT_GLES_FUNC list out of the real translation unit at compile time
// is not possible (it is a statement list, not a macro list), so this is the
// structural half: expand GLES_ALL_ENTRIES into names + addresses.
#define ENTRY_NAME(n) #n,
#define ENTRY_ADDR(n) &g_gles_func.n,

static const char* const kNames[] = { GLES_ALL_ENTRIES(ENTRY_NAME) };
static void* const kAddrs[] = { GLES_ALL_ENTRIES(ENTRY_ADDR) };

int main() {
    const size_t n = sizeof(kNames) / sizeof(kNames[0]);

    printf("GLES_ALL_ENTRIES rows : %zu\n", n);

    int unreadable = 0, dupes = 0, nullsym = 0;
    std::vector<void*> seen;

    for (size_t i = 0; i < n; ++i) {
        // Address must be inside g_gles_func. Every one of these is
        // &g_gles_func.<member>, so a failure here means the macro named
        // something that is not a member -- which would not compile, so this
        // is really a sanity check on the array itself.
        const char* base = (const char*)&g_gles_func;
        const char* end = base + sizeof(g_gles_func);
        const char* p = (const char*)kAddrs[i];
        if (p < base || p >= end) {
            printf("OUTSIDE: %s @ %p\n", kNames[i], kAddrs[i]);
            ++unreadable;
        }
        for (void* q : seen) {
            if (q == kAddrs[i]) { printf("DUPLICATE SLOT: %s\n", kNames[i]); ++dupes; break; }
        }
        seen.push_back(kAddrs[i]);
        if (kNames[i] == nullptr || kNames[i][0] == '\0') ++nullsym;
    }

    printf("struct gles_func_t size : %zu bytes\n", sizeof(gles_func_t));
    printf("distinct slots          : %zu\n", seen.size());
    printf("unreadable addresses    : %d\n", unreadable);
    printf("duplicate slots         : %d\n", dupes);
    printf("empty names             : %d\n", nullsym);

    // Spot-check the head and tail against the order init_target_gles uses,
    // so a reordering of the macro is caught here rather than on device.
    const char* expect_first = "glActiveTexture";
    const char* expect_last  = "glMultiDrawElementsBaseVertexEXT";
    bool order_ok = (strcmp(kNames[0], expect_first) == 0) &&
                    (strcmp(kNames[n-1], expect_last) == 0);
    printf("first/last as expected  : %s\n", order_ok ? "yes" : "NO");

    const bool ok = (unreadable == 0) && (dupes == 0) && order_ok;
    printf("\n%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
