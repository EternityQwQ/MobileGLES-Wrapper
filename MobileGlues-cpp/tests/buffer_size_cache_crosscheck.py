#!/usr/bin/env python3
"""Keeps tests/buffer_size_cache_test.cpp honest about gl/multidraw.cpp.

The test carries a copy of md_caps_cache_t's buffer-size table, because the real
one lives inside an anonymous struct in a translation unit that drags in the
whole GL entry-point table. A copy is only useful if it is checked, so this
script asserts that the operations the test reproduces are still the operations
in the source.

It deliberately checks behaviour-defining facts rather than formatting: the
storage layout (two parallel arrays plus size_count plus size_clock), that
size_find scans [0, size_count) and is const, that forget_buffer_size swaps with
the last entry and shrinks, and that insert_size only ever writes an occupied
slot on the full-table path. If any of those change, the test's assumptions are
stale and this fails rather than letting the test pass against a table that no
longer exists.
"""

import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "gl" / "multidraw.cpp"

problems = []

def extract_body(text, signature_regex):
    """Return the brace-balanced body of the first function matching
    `signature_regex` (which must end at the opening '{'), or None.

    A plain regex cannot do this: the bodies contain nested braces, so a
    non-greedy match stops at the first inner block and silently truncates the
    function -- which is how an earlier version of this script reported a bogus
    failure for a function that was in fact correct.
    """
    m = re.search(signature_regex, text)
    if not m:
        return None
    start = text.find("{", m.end() - 1)
    if start == -1:
        return None
    depth = 0
    for i in range(start, len(text)):
        c = text[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:i]
    return None



def main():
    if not SRC.exists():
        print(f"FAIL: cannot find {SRC}")
        return 1
    text = SRC.read_text(encoding="utf-8", errors="replace")

    # --- the table's members ---
    if not re.search(r"GLuint\s+size_key\s*\[\s*kBufferSizeCacheCapacity\s*\]", text):
        problems.append("size_key is no longer a kBufferSizeCacheCapacity array")
    if not re.search(r"BufferSize\s+size_entry\s*\[\s*kBufferSizeCacheCapacity\s*\]", text):
        problems.append("size_entry is no longer a kBufferSizeCacheCapacity array")
    if "size_t size_count = 0;" not in text:
        problems.append("size_count is gone or no longer initialised to 0")
    if "kBufferSizeCacheCapacity = 128" not in text:
        problems.append("capacity is no longer 128 (the test's live-key budget assumes it)")

    # --- size_find: a scan over [0, size_count), const ---
    body = extract_body(text, r"size_t\s+size_find\s*\(\s*GLuint\s+\w+\s*\)\s*const\s*")
    if body is None:
        problems.append("size_find(GLuint) const not found -- the test mirrors its body")
    else:
        if "size_count" not in body:
            problems.append("size_find no longer bounds its scan by size_count")
        if "kBufferSizeCacheMissing" not in body:
            problems.append("size_find no longer returns kBufferSizeCacheMissing on a miss")

    # --- forget_buffer_size: swap with last, then shrink ---
    body = extract_body(text, r"void\s+forget_buffer_size\s*\(\s*GLuint\s+\w+\s*\)\s*")
    if body is None:
        problems.append("forget_buffer_size not found")
    else:
        for needle, why in (
            ("size_find(", "forget no longer looks the key up first"),
            ("size_count - 1", "forget no longer takes the last entry"),
            ("size_count = last", "forget no longer shrinks size_count"),
        ):
            if needle not in body:
                problems.append(why)
        if re.search(r"\.valid\b|\bvalid\s*=", body):
            problems.append(
                "forget_buffer_size uses a `valid` flag again -- that is the old "
                "design whose dead slots the packed array exists to avoid"
            )

    # --- insert_size: appends while there is room, only overwrites when full ---
    body = extract_body(text, r"void\s+insert_size\s*\(\s*GLuint\s+\w+\s*,\s*int\s+\w+\s*\)\s*")
    if body is None:
        problems.append("insert_size not found")
    else:
        if "size_count < kBufferSizeCacheCapacity" not in body:
            problems.append("insert_size no longer checks for room before appending")
        if "size_key[size_count]" not in body:
            problems.append("insert_size no longer appends at size_count (append point moved)")
        if "++size_count" not in body:
            problems.append("insert_size no longer increments size_count on append")

    # --- the old dead-slot machinery must be gone from the cache ---
    # Scope the check to the cache so an unrelated `valid` elsewhere is not a hit.
    cache_start = text.find("struct md_caps_cache_t")
    cache_end = text.find("struct md_scratch_state_t", cache_start)
    if cache_start != -1 and cache_end != -1:
        cache = text[cache_start:cache_end]
        if re.search(r"\bbool\s+valid\b", cache):
            problems.append("the cache has a `valid` member again (dead-slot design is back)")
        if "size_clock" not in cache:
            problems.append("size_clock (the eviction cursor) is gone")
    else:
        problems.append("could not locate the md_caps_cache_t block")

    # --- report ---
    print(f"buffer-size cache crosscheck against {SRC.name}")
    if problems:
        for p in problems:
            print(f"  FAIL: {p}")
        print(f"  {len(problems)} problem(s)")
        return 1
    print("  storage: parallel size_key/size_entry arrays + size_count + size_clock")
    print("  lookup:  size_find scans [0, size_count), const, misses as sentinel")
    print("  removal: forget_buffer_size swaps in the last entry and shrinks")
    print("  insert:  appends at size_count, overwrites only when full")
    print("  no `valid` member: entries are reclaimed, not tombstoned")
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
