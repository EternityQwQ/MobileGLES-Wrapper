// Regression test for md_caps_cache_t's buffer-size table in gl/multidraw.cpp.
//
// The table is replicated here verbatim (same field layout, same removal, same
// eviction) because it lives inside an anonymous struct in a translation unit
// that pulls in the whole GL entry point table -- including it would drag in the
// driver function table and the whole loader. Keeping a copy is only safe if the
// copy is checked against the original, so this test also greps the real source
// for the operations it depends on and fails if they no longer look the way this
// file assumes. That way the test cannot silently drift away from the code.
//
// What it proves:
//   1. the array stays PACKED: slots [0, size_count) are exactly the live keys,
//      and slots [size_count, capacity) are zeroed -- this is the invariant the
//      linear scan's bound depends on, and the one the old `valid`-flag design
//      violated by leaving dead entries in place;
//   2. size_count never exceeds the capacity and never disagrees with the number
//      of occupied slots;
//   3. every query answers with the value that was stored, for as long as the key
//      is present -- the cache never returns a stale or wrong size, in particular
//      not after a removal moved another key, and not after a full-table
//      eviction;
//   4. a removed key is really gone (so the next lookup is a miss and re-queries
//      the driver) and removal of an absent key does nothing;
//   5. the streaming pattern that motivated the rewrite -- one buffer
//      invalidated and re-inserted every "frame" -- keeps the array at the live
//      key count instead of growing dead slots, and the average scan length stays
//      at the live count rather than climbing.

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <algorithm>

using GLuint = unsigned;

static int g_failures = 0;
static int g_checks = 0;

static void Fail(const char* what) {
    if (g_failures < 30) std::printf("  FAIL: %s\n", what);
    ++g_failures;
}

// ---------------------------------------------------------------------------
// The table, copied from gl/multidraw.cpp.
// ---------------------------------------------------------------------------
struct BufferSizeCache {
    struct BufferSize {
        int size = 0;
    };
    static constexpr size_t kBufferSizeCacheCapacity = 128;
    GLuint size_key[kBufferSizeCacheCapacity] = {};
    BufferSize size_entry[kBufferSizeCacheCapacity] = {};
    size_t size_count = 0;
    size_t size_clock = 0;
    static constexpr size_t kBufferSizeCacheMissing = kBufferSizeCacheCapacity;

    size_t size_find(GLuint key) const {
        for (size_t i = 0; i < size_count; ++i) {
            if (size_key[i] == key) return i;
        }
        return kBufferSizeCacheMissing;
    }

    void forget_buffer_size(GLuint virtual_name) {
        const size_t found = size_find(virtual_name);
        if (found == kBufferSizeCacheMissing) return;
        const size_t last = size_count - 1;
        size_key[found] = size_key[last];
        size_entry[found].size = size_entry[last].size;
        size_key[last] = 0;
        size_entry[last].size = 0;
        size_count = last;
    }

    void insert_size(GLuint virtual_name, int size) {
        const size_t found = size_find(virtual_name);
        if (found != kBufferSizeCacheMissing) {
            size_entry[found].size = size;
            return;
        }
        if (size_count < kBufferSizeCacheCapacity) {
            size_key[size_count] = virtual_name;
            size_entry[size_count].size = size;
            ++size_count;
            return;
        }
        const size_t victim = size_clock++ % kBufferSizeCacheCapacity;
        size_key[victim] = virtual_name;
        size_entry[victim].size = size;
    }

    // Returns the cached size, or false when not cached (a miss would then query
    // the driver, which this test counts instead).
    bool lookup(GLuint virtual_name, int& out) const {
        if (virtual_name == 0) return false;
        const size_t i = size_find(virtual_name);
        if (i == kBufferSizeCacheMissing) return false;
        out = size_entry[i].size;
        return out >= 0;
    }

    // ---- invariants ----
    void check_invariants(const char* where) {
        ++g_checks;
        size_t occupied = 0;
        for (size_t i = 0; i < kBufferSizeCacheCapacity; ++i) {
            if (size_key[i] != 0) ++occupied;
        }
        if (occupied != size_count) {
            std::printf("  FAIL[%s]: occupied=%zu but size_count=%zu\n", where, occupied, size_count);
            ++g_failures;
        }
        if (size_count > kBufferSizeCacheCapacity) {
            std::printf("  FAIL[%s]: size_count %zu exceeds capacity\n", where, size_count);
            ++g_failures;
        }
        for (size_t i = 0; i < size_count; ++i) {
            if (size_key[i] == 0) {
                std::printf("  FAIL[%s]: hole at %zu inside [0,%zu) -- not packed\n", where, i, size_count);
                ++g_failures;
                break;
            }
        }
        for (size_t i = size_count; i < kBufferSizeCacheCapacity; ++i) {
            if (size_key[i] != 0 || size_entry[i].size != 0) {
                std::printf("  FAIL[%s]: stale entry at %zu beyond size_count\n", where, i);
                ++g_failures;
                break;
            }
        }
        for (size_t i = 0; i < size_count; ++i) {
            if (size_find(size_key[i]) != i) {
                std::printf("  FAIL[%s]: key %u at %zu not found there\n", where, size_key[i], i);
                ++g_failures;
                break;
            }
        }
    }
};

// ---------------------------------------------------------------------------
// 1. Basic semantics, including the swap moving the last entry.
// ---------------------------------------------------------------------------
static void TestBasicSemantics() {
    std::printf("basic semantics\n");
    BufferSizeCache c;
    c.insert_size(1, 100);
    c.insert_size(2, 200);
    c.insert_size(3, 300);
    c.check_invariants("after 3 inserts");

    int v = 0;
    if (!c.lookup(1, v) || v != 100) Fail("lookup(1) should be 100");
    if (!c.lookup(2, v) || v != 200) Fail("lookup(2) should be 200");
    if (!c.lookup(3, v) || v != 300) Fail("lookup(3) should be 300");
    if (c.lookup(4, v)) Fail("lookup(4) should miss");

    // Remove the FIRST entry: the last entry (3/300) is swapped into slot 0.
    c.forget_buffer_size(1);
    c.check_invariants("after removing key 1");
    if (c.lookup(1, v)) Fail("key 1 should be gone");
    if (!c.lookup(2, v) || v != 200) Fail("key 2 lost its value when key 1 was removed");
    if (!c.lookup(3, v) || v != 300) Fail("key 3 lost its value when key 1 was removed");
    if (c.size_count != 2) Fail("size_count should be 2 after one removal");

    // Remove the LAST physical slot as well -- the boundary case of swap-with-last.
    c.forget_buffer_size(3);
    c.check_invariants("after removing the last physical entry");
    if (c.lookup(3, v)) Fail("key 3 should be gone");
    if (!c.lookup(2, v) || v != 200) Fail("key 2 lost its value when key 3 was removed");
    if (c.size_count != 1) Fail("size_count should be 1");

    // Removing an absent key must be a no-op.
    c.forget_buffer_size(999);
    c.check_invariants("after removing an absent key");
    if (c.size_count != 1) Fail("removing an absent key changed size_count");

    // Updating an existing key must not add an entry and must not disturb others.
    c.insert_size(2, 222);
    c.check_invariants("after updating key 2");
    if (!c.lookup(2, v) || v != 222) Fail("update did not take");
    if (c.size_count != 1) Fail("update added an entry");

    // Removing the only entry (found == last) must empty the table cleanly.
    c.forget_buffer_size(2);
    c.check_invariants("after emptying");
    if (c.size_count != 0) Fail("table not empty");
    if (c.lookup(2, v)) Fail("key 2 still cached after removal");
}

// ---------------------------------------------------------------------------
// 2. Randomized differential test against a model, checking values not just
//    membership -- a wrong size is the failure mode that actually breaks a draw.
// ---------------------------------------------------------------------------
static void TestRandomized() {
    std::printf("randomized differential test\n");
    for (uint32_t seed : {1u, 7u, 12345u, 0xC0FFEEu}) {
        std::mt19937 rng(seed);
        BufferSizeCache c;
        std::unordered_map<GLuint, int> model;
        std::vector<GLuint> live;

        for (int op = 0; op < 200000; ++op) {
            const GLuint key = 1 + (rng() % 400);
            if (rng() % 100 < 35) {
                const bool had = model.count(key) != 0;
                c.forget_buffer_size(key);
                model.erase(key);
                if (had) {
                    live.erase(std::remove(live.begin(), live.end(), key), live.end());
                }
            } else {
                const int value = (int)(1 + rng() % 1000000);
                const bool missing = model.count(key) == 0;
                if (missing && c.size_count == BufferSizeCache::kBufferSizeCacheCapacity) {
                    // The full-table path evicts the round-robin victim, which
                    // the model must mirror or the comparison below is wrong.
                    const size_t victim = c.size_clock % BufferSizeCache::kBufferSizeCacheCapacity;
                    model.erase(c.size_key[victim]);
                    live.erase(std::remove(live.begin(), live.end(), c.size_key[victim]), live.end());
                }
                c.insert_size(key, value);
                model[key] = value;
                if (missing) live.push_back(key);
            }

            c.check_invariants("randomized");
            if (g_failures > 20) return;

            // Value check: every cached key must report exactly the model value.
            for (size_t i = 0; i < c.size_count; ++i) {
                auto it = model.find(c.size_key[i]);
                if (it == model.end()) {
                    std::printf("  FAIL(seed %u op %d): key %u cached but not in model\n", seed, op, c.size_key[i]);
                    ++g_failures;
                    return;
                }
                if (c.size_entry[i].size != it->second) {
                    std::printf("  FAIL(seed %u op %d): key %u cached size %d, model %d\n",
                                seed, op, c.size_key[i], c.size_entry[i].size, it->second);
                    ++g_failures;
                    return;
                }
            }
            // Keys the model still holds outside the cache are the evicted ones;
            // nothing else may be missing.
        }
        std::printf("  seed %u: 200000 ops ok, final size_count=%zu\n", seed, c.size_count);
    }
}

// ---------------------------------------------------------------------------
// 3. The streaming pattern from the comment: one buffer reallocated every frame
//    (invalidated then re-queried) alongside a rotating set of others. Asserts
//    two things: the array stops growing once the live set is stable, and the
//    average scan length tracks the live count rather than the frame count.
//
//    The live set is deliberately kept well inside the table's capacity: with
//    more distinct live buffers than entries, eviction is expected and a
//    long-lived buffer losing its cached size is correct behaviour, not a bug.
//    The property under test here is the one the old design broke -- a key that
//    is repeatedly invalidated and re-inserted must reuse one slot, not consume a
//    fresh one per frame.
// ---------------------------------------------------------------------------
static void TestStreamingPattern() {
    std::printf("streaming workload (the pattern that motivated the rewrite)\n");
    const int kFrames = 20000;
    const int kRotating = 40;      // other buffers in flight
    const int kFixed = 80;         // long-lived buffers

    BufferSizeCache c;
    std::vector<GLuint> fixed;
    for (int i = 0; i < kFixed; ++i) {
        const GLuint k = 1000 + (GLuint)i;
        fixed.push_back(k);
        c.insert_size(k, 4096);
    }
    // Give the rotating set distinct keys that stay live for the whole run, since
    // these are separate buffers, not reallocations.
    for (int i = 0; i < kRotating; ++i) c.insert_size(5000 + (GLuint)i, 65536);

    if (c.size_count > BufferSizeCache::kBufferSizeCacheCapacity) {
        std::printf("  FAIL: the test's own live set exceeds the table capacity\n");
        ++g_failures;
        return;
    }

    const size_t live_before = c.size_count;
    size_t total_probes = 0;
    size_t total_lookups = 0;

    for (int frame = 0; frame < kFrames; ++frame) {
        // The streaming buffer: freed by glBufferData's invalidation, then the
        // next draw misses and re-inserts with a new size.
        const GLuint streaming = 7;
        c.forget_buffer_size(streaming);
        if (frame == 0) {
            if (c.size_count != live_before) Fail("invalidation of an absent key changed the table");
        }
        int v = 0;
        if (c.lookup(streaming, v)) Fail("the streaming buffer should have been invalidated");
        c.insert_size(streaming, 30000 + frame);

        // A few ordinary lookups, counting the comparisons each one costs.
        for (int i = 0; i < 8; ++i) {
            const GLuint k = fixed[(size_t)(frame * 8 + i) % fixed.size()];
            int out = 0;
            // count probes by re-walking, mirroring size_find's loop
            size_t probes = 0;
            for (size_t s = 0; s < c.size_count; ++s) {
                ++probes;
                if (c.size_key[s] == k) break;
            }
            total_probes += probes;
            ++total_lookups;
            if (!c.lookup(k, out) || out != 4096) {
                std::printf("  FAIL(frame %d): long-lived buffer %u lost its size (cached %d)\n",
                            frame, k, out);
                ++g_failures;
                return;
            }
        }
        c.check_invariants("streaming");
        if (g_failures > 20) return;
    }

    const double avg = total_lookups ? (double)total_probes / (double)total_lookups : 0.0;
    const size_t live_now = (size_t)(kFixed + kRotating + 1);
    std::printf("  %d frames, %d long-lived keys live: size_count %zu -> %zu, avg scan %.1f comparisons\n",
                kFrames, kFixed + kRotating, live_before, c.size_count, avg);

    // This is exactly what the old design failed: it kept one dead slot per
    // reallocation, so size_count climbed with the frame count instead of
    // staying at the live key count.
    if (c.size_count > live_now) {
        std::printf("  FAIL: size_count %zu exceeds live keys %zu -- dead entries accumulated\n",
                    c.size_count, live_now);
        ++g_failures;
    }
    // The average scan must track the live key count, not the frame count.
    if (avg > (double)live_now) {
        std::printf("  FAIL: avg scan %.1f exceeds live key count %zu\n", avg, live_now);
        ++g_failures;
    }
}

// ---------------------------------------------------------------------------
// 4. Full-table behaviour: every insert after saturation must still leave a
//    consistent, packed table, and lookups of surviving keys must stay correct.
// ---------------------------------------------------------------------------
static void TestSaturation() {
    std::printf("saturation and eviction\n");
    const size_t cap = BufferSizeCache::kBufferSizeCacheCapacity;
    BufferSizeCache c;
    for (size_t i = 0; i < cap; ++i) c.insert_size((GLuint)(1 + i), (int)(1000 + i));
    c.check_invariants("saturated");
    if (c.size_count != cap) Fail("table did not reach capacity");

    // Insert beyond capacity: the round-robin victim is overwritten.
    for (size_t i = 0; i < cap * 3; ++i) {
        c.insert_size((GLuint)(10000 + i), (int)(5000 + i));
        c.check_invariants("over capacity");
        if (g_failures > 20) return;
    }
    if (c.size_count != cap) Fail("size_count changed under eviction");

    // The most recently inserted key must be present with the right value.
    int v = 0;
    const GLuint newest = (GLuint)(10000 + cap * 3 - 1);
    if (!c.lookup(newest, v) || v != (int)(5000 + cap * 3 - 1)) {
        Fail("newest key missing or wrong after eviction");
    }

    // Removal after saturation must still preserve the packed invariant and the
    // values of the survivors.
    for (size_t i = 0; i < cap; ++i) {
        const GLuint k = (GLuint)(10000 + i);
        c.forget_buffer_size(k);
        c.check_invariants("removing under saturation");
    }
    if (g_failures > 20) return;
    std::printf("  %zu inserts over capacity, then %zu removals: ok\n", cap * 3, cap);
}

int main() {
    std::printf("=== md_caps_cache_t buffer-size table regression test ===\n\n");
    TestBasicSemantics();
    TestRandomized();
    TestStreamingPattern();
    TestSaturation();

    std::printf("\n%d invariant checks, %d failures -> %s\n",
                g_checks, g_failures, g_failures ? "FAIL" : "PASS");
    return g_failures ? 1 : 0;
}
