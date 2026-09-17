#!/bin/sh
# Host-side checks for the parts of the pixel and framebuffer code that are pure
# enough to run without a GPU. The pixel and framebuffer tests link the real
# translation units, not copies.
#
#   sh MobileGlues-cpp/tests/run.sh
#
# Exit status is non-zero if any check fails, and every check runs even after one
# fails -- an earlier version used `set -e` and stopped at the first failure, so a
# broken test at the top hid the rest.
cd "$(dirname "$0")/.." || exit 1
INC="-I. -I./includes -I./include -I./3rdparty/xxhash"
CXX="${CXX:-g++}"
CXXFLAGS="-std=gnu++20 -w $INC"

failures=0
failed_names=""

run_check() {
    name="$1"
    shift
    printf '\n=== %s ===\n' "$name"
    if "$@"; then
        :
    else
        status=$?
        echo "  ^^ FAILED (exit $status)"
        failures=$((failures + 1))
        failed_names="$failed_names $name"
    fi
}

# Compile-then-run helper, so a build failure is reported the same way as a
# runtime failure instead of aborting the script.
run_built() {
    name="$1"; src="$2"; shift 2
    printf '\n=== %s ===\n' "$name"
    if ! $CXX $CXXFLAGS -o "/tmp/mg_$name" "$src" "$@"; then
        echo "  ^^ BUILD FAILED"
        failures=$((failures + 1))
        failed_names="$failed_names $name(build)"
        return
    fi
    if ! "/tmp/mg_$name"; then
        status=$?
        echo "  ^^ FAILED (exit $status)"
        failures=$((failures + 1))
        failed_names="$failed_names $name"
    fi
}

# The pixel and framebuffer tests are known to fail to build on a host compiler:
# their translation units clash with the gl_state macro. They are kept here so a
# fix shows up, but they must not take the rest of the suite down with them.
run_built pixel_size_test tests/pixel_size_test.cpp gl/pixel.cpp
run_built framebuffer_shuffle_test tests/framebuffer_shuffle_test.cpp gl/framebuffer.cpp
run_built md_order_test tests/md_order_test.cpp

# The entry-point lists must stay in step across gles/gles.h and gles/loader.cpp.
# This is the check that keeps GLES_ALL_ENTRIES (which drives the startup
# unresolved-slot report) equal to the INIT_GLES_FUNC rows init_target_gles()
# actually resolves. A name added to one and not the other is an entry point the
# report silently stops covering, which is exactly the kind of gap that only
# shows up as a null-pointer dereference on someone else's device.
run_built entry_lists_test tests/entry_lists_test.cpp
run_check entry_lists_crosscheck python3 tests/entry_lists_crosscheck.py

# The multidraw buffer-size cache must stay packed. Its lookup is a linear scan
# bounded by size_count, so the moment a removal stops reclaiming a slot the
# scan starts walking dead entries -- which is the defect this table was
# rewritten to remove. The test carries a copy of the table because the original
# sits inside an anonymous struct in a translation unit that pulls in the whole
# GL entry table; the crosscheck asserts the copy still matches the source, so it
# cannot drift silently.
run_check buffer_size_cache_crosscheck python3 tests/buffer_size_cache_crosscheck.py
run_built buffer_size_cache_test tests/buffer_size_cache_test.cpp

printf '\n========================================\n'
if [ "$failures" -eq 0 ]; then
    echo "all checks passed"
    exit 0
fi
echo "$failures check(s) failed:$failed_names"
exit 1
