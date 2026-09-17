#!/usr/bin/env python3
"""Cross-checks the three entry-point lists that gles/ must keep in step.

  1. GLES_ALL_ENTRIES(X)      - gles/gles.h   (drives the report + the macro)
  2. INIT_GLES_FUNC(...) rows - gles/loader.cpp (drives dlsym resolution)
  3. GL_FUNC_DECL(...) fields - gles/gles.h, struct gles_func_t (the storage)

(1) and (2) must be identical in content AND order -- they are two expansions of
the one list, and a name in (2) but not (1) is an entry point the report would
silently not check. (3) is the storage and must be a superset of (1): a field in
(3) with no row in (2) is a slot nobody ever assigns.

Exits non-zero and prints the offending names on any violation.
"""
import re
import sys
import pathlib

root = pathlib.Path(__file__).resolve().parents[3] / "workspace/MobileGLES-Wrapper/MobileGlues-cpp"
if not root.exists():
    root = pathlib.Path("/workspace/MobileGLES-Wrapper/MobileGlues-cpp")

def strip_comments(s):
    return re.sub(r"//[^\n]*", "", s)

def brace_block(src, opener_idx):
    depth = 0
    for j in range(opener_idx, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[opener_idx:j]
    raise ValueError("unbalanced braces")

gles_h = (root / "gles/gles.h").read_text(encoding="utf-8", errors="replace")

# (3) struct fields
si = gles_h.index("struct gles_func_t {")
struct_names = re.findall(
    r"GL_FUNC_DECL\((gl\w+)\)",
    strip_comments(brace_block(gles_h, si + len("struct gles_func_t ") - 1)),
)

# (1) the macro body: every line up to the first that is not a continuation
mi = gles_h.index("#define GLES_ALL_ENTRIES(X)")
chunk = []
for ln in gles_h[mi:].split("\n"):
    chunk.append(ln)
    if not ln.rstrip().endswith("\\"):
        break
macro_names = re.findall(r"\bX\((gl\w+)\)", strip_comments("\n".join(chunk)))

# (2) the resolution rows
init_src = (root / "gles/loader.cpp").read_text(encoding="utf-8", errors="replace")
fi = init_src.index("void init_target_gles() {")
init_names = re.findall(
    r"INIT_GLES_FUNC\((gl\w+)\)",
    strip_comments(brace_block(init_src, fi + len("void init_target_gles() ") - 1)),
)

print(f"(1) GLES_ALL_ENTRIES rows        : {len(macro_names)}")
print(f"(2) INIT_GLES_FUNC rows          : {len(init_names)}")
print(f"(3) gles_func_t fields           : {len(struct_names)}")
print()

problems = []

if macro_names != init_names:
    if set(macro_names) == set(init_names):
        problems.append("(1) and (2) hold the same names in a DIFFERENT ORDER")
        for i, (a, b) in enumerate(zip(macro_names, init_names)):
            if a != b:
                problems.append(f"    first difference at row {i}: macro={a} init={b}")
                break
    else:
        only_m = sorted(set(macro_names) - set(init_names))
        only_i = sorted(set(init_names) - set(macro_names))
        problems.append("(1) and (2) hold DIFFERENT names")
        if only_m:
            problems.append(f"    only in the macro, never reported or resolved: {only_m}")
        if only_i:
            problems.append(f"    only in the init rows, never reported: {only_i}")
else:
    print("(1) == (2): same names, same order  ->  the report covers every entry "
          "point init_target_gles resolves")

missing_storage = sorted(set(macro_names) - set(struct_names))
if missing_storage:
    problems.append(f"(1) names with no slot in gles_func_t: {missing_storage}")
else:
    print("(1) subset of (3): every reported name is a real gles_func_t slot")

unresolved_storage = sorted(set(struct_names) - set(macro_names))
if unresolved_storage:
    print(f"(3) fields with no init row (dead slots, never assigned): {unresolved_storage}")

print()
if problems:
    for p in problems:
        print("FAIL:", p)
    sys.exit(1)
print("PASS")
