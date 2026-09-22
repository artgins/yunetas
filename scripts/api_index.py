#!/usr/bin/env python3
#
#   api_index.py -- the C API appendix, written from the headers
#
#   docs/doc.yuneta.io/api/appendix_api_index.md lists every public C
#   function of the kernel, header by header in declaration order, and then
#   A-Z. It was kept by hand, and by hand it drifted: whole headers missing
#   and 26 functions of the listed ones not there. This script writes it from
#   the headers, so it cannot drift; `--check` exits 1 when the file is not
#   what the headers say (run it before a release, as the other doc guards).
#
#   A function links to the page that carries its anchor `(name)=` under
#   docs/doc.yuneta.io/api/. One without an anchor is listed without a link:
#   verify_api_coverage.py is the guard that says it must be documented.
#
#   Usage:
#       scripts/api_index.py            rewrite the appendix
#       scripts/api_index.py --check    exit 1 if it is stale
#
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
API = ROOT / "docs" / "doc.yuneta.io" / "api"
OUT = API / "appendix_api_index.md"

#
#   The modules, in the order the appendix shows them
#
MODULES = [
    ("gobj-c (Core Framework)",          "kernel/c/gobj-c/src"),
    ("libjwt (JWT Authentication)",      "kernel/c/libjwt/src"),
    ("ytls (TLS Abstraction)",           "kernel/c/ytls/src"),
    ("yev_loop (Event Loop)",            "kernel/c/yev_loop/src"),
    ("timeranger2 (Time-Series DB)",     "kernel/c/timeranger2/src"),
    ("root-linux (Runtime GClasses)",    "kernel/c/root-linux/src"),
]

#
#   A declaration: an export macro, a return type, a name and its '('
#
#   (libjwt writes JWT_EXPORT on a line of its own, and the appendix shows its
#   prototypes without the macro, as the library's own docs do)
DECL_START = re.compile(
    r"^[ \t]*(PUBLIC[ \t]+|JWT_EXPORT\s+)(?:[\w*\s]+?)\b([A-Za-z_][A-Za-z0-9_]*)[ \t]*\(",
    re.MULTILINE,
)
#   Declared without its export macro, so the pattern above misses them.
EXTRA_DECLS = {
    "jwt.h": ["jwt_init"],
}
ANCHOR = re.compile(r"^\(([A-Za-z_][A-Za-z0-9_]*)\)=\s*$", re.MULTILINE)

HEADER = """---
title: 'Appendix: Kernel C API — Complete Function Index'
description: >-
  Quick-reference listing of every public C function in the Yuneta kernel,
  grouped by header file with full prototypes.
---

# Appendix: Kernel C API — Complete Function Index

This appendix lists **every public C function** declared in the kernel
header files (excluding `linux-ext-libs` and `root-esp32`).
Each section shows functions in their natural declaration order.
Use your browser's find (**Ctrl+F**) to search by name.
The [Alphabetical Index](#alphabetical-index) at the end lists every function sorted A–Z
with links to the API documentation.

This file is written by `scripts/api_index.py` from the headers: do not edit
it by hand, run the script.
"""


#
#   The prototype of each declaration, on one line
#
def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def declarations(path):
    source = path.read_text(encoding="utf-8", errors="replace")
    out = []
    for m in DECL_START.finditer(source):
        start = m.start()
        if m.group(1).startswith("JWT_EXPORT"):
            start = m.start() + len(m.group(0)) - len(m.group(0).lstrip()) + len(m.group(1))
        depth = 0
        i = m.end() - 1
        while i < len(source):
            if source[i] == "(":
                depth += 1
            elif source[i] == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        end = source.find(";", i)
        if end < 0:
            continue
        proto = strip_comments(source[start:end]).strip()
        proto = re.sub(r"\s*\n\s*", " ", proto)
        proto = re.sub(r"[ \t]+", " ", proto)
        out.append((m.group(2), proto))
    for name in EXTRA_DECLS.get(path.name, []):
        m = re.search(r"^[ \t]*([\w*][\w\s*]*\b" + name + r"[ \t]*\([^;]*\));", source, re.MULTILINE)
        if m:
            proto = re.sub(r"\s+", " ", strip_comments(m.group(1))).strip()
            out.append((name, proto))
    return out


#
#   Where each function is documented
#
def anchors():
    found = {}
    for md in sorted(API.rglob("*.md")):
        if md == OUT or "_build" in md.parts:
            continue
        rel = md.relative_to(API).as_posix()
        for name in ANCHOR.findall(md.read_text(encoding="utf-8", errors="replace")):
            found.setdefault(name, rel)
    return found


def entry_name(name, where):
    page = where.get(name)
    if page:
        return f"[**`{name}`**]({page}#{name})"
    return f"**`{name}`**"


#
#   The appendix
#
def build():
    where = anchors()
    lines = [HEADER]
    alpha = []
    grand = 0
    for module, directory in MODULES:
        sections = []
        for h in sorted((ROOT / directory).rglob("*.h"), key=lambda p: p.name):
            decls = declarations(h)
            if decls:
                sections.append((h, decls))
        if not sections:
            continue
        lines.append(f"## {module}\n")
        total = 0
        for h, decls in sections:
            rel = h.relative_to(ROOT).as_posix()
            lines.append(f"### `{h.name}` — {len(decls)} functions\n")
            lines.append(f"**Source:** `{rel}`\n")
            for n, (name, proto) in enumerate(decls, 1):
                lines.append(f"{n}. {entry_name(name, where)} — `{proto}`\n")
                alpha.append((name, h.name, module))
            total += len(decls)
        lines.append(f"**Total: {total} functions**\n")
        grand += total

    lines.append("(alphabetical-index)=\n## Alphabetical Index\n")
    lines.append(f"All **{grand} functions** sorted alphabetically with their source header.\n")
    rows = ["| Function | Header | Module |", "|----------|--------|--------|"]
    for name, header, module in sorted(alpha, key=lambda r: (r[0].lower(), r[0], r[1])):
        rows.append(f"| {entry_name(name, where)} | `{header}` | {module} |")
    lines.append("\n".join(rows) + "\n")
    return "\n".join(lines)


def main():
    check = "--check" in sys.argv[1:]
    text = build()
    current = OUT.read_text(encoding="utf-8") if OUT.exists() else ""
    if text == current:
        return 0
    if check:
        print(f"{OUT.relative_to(ROOT)} is stale: run scripts/api_index.py")
        return 1
    OUT.write_text(text, encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
