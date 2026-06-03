#!/usr/bin/env python3
"""
Audit `file.c:NNNN` references in the documentation against the current source.

Many docs (especially the onboarding chapters under yunos/c/yuno_agent/) point
at exact source lines like `gobj.c:11256` or
`kernel/c/gobj-c/src/gobj.c:11256`. Those line numbers drift silently whenever
the source file is edited above the referenced line. This tool re-checks every
reference against the current tree.

Four modes:

  (default) AUDIT — report how many `file.c:NNN` refs are verifiably *linkable*
  (the cited line is provably at / inside a known function in the current
  source) vs not (globals, macros, internal lines whose function moved, plain
  prose). Exit 1 only on out-of-bounds refs (a line past EOF).

  --linkify[=TAG]  TRANSFORM the docs (hybrid policy): a `file.c:NNN` ref that
  is verifiably a function citation becomes a GitHub source link pinned to TAG
  (default 7.5.1) and pointing at the function's CURRENT definition line; an
  unverifiable ref keeps just its filename (the line number — useless without a
  link and prone to drift — is dropped). Re-run at each release to re-pin.

  --strip-prose  Drop the SAME unverifiable line numbers in the two forms the
  colon-form `--linkify` never sees: prose ("(lines 316-335)", ", line 42",
  "at lines 58-113", "(deb lines 324-343)") and comma-form (`gobj.c, 3622`,
  `c_ievent_srv.c, 1070, 1092`, `gobj.c`, `3516-3517`). Keeps the filename.

  --link-symbols[=TAG]  Make the coloured backticked symbols in narrative prose
  clickable (the generated api/ tree is already cross-linked and is skipped),
  first occurrence per page only: a function with a UNIQUE definition -> GitHub
  source (pinned); a gclass `C_XXX` with a `(gclass-c-xxx)=` doc label -> that
  label; a CLI tool with a `(util-xxx)=`/`(tool-xxx)=` label -> that label.
  Events, states, types, kw keys and ambiguous names have no stable target and
  stay as code. Test helpers and the ESP32 port are excluded as link targets.

A `file.c:NNN` ref earns a link two ways: it sits right after a backticked
function name (`fn()` (file.c:N)) — a definition citation, linked to the current
def even if N drifted; or the cited line is provably inside some nearby
function's body span — an internal citation, linked to that function's def.

Run from anywhere inside the repo.

Usage:
    scripts/check_doc_line_refs.py                 # audit summary
    scripts/check_doc_line_refs.py -v              # also list unresolvable refs
    scripts/check_doc_line_refs.py --linkify       # link/strip file.c:NNN refs
    scripts/check_doc_line_refs.py --strip-prose   # drop prose/comma line nums
    scripts/check_doc_line_refs.py --link-symbols  # link symbols to source/docs
"""
import os
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]

# Where the docs live.
DOC_ROOTS = [
    REPO / "docs" / "doc.yuneta.io",
    REPO / "yunos" / "c" / "yuno_agent",
]
SRC_EXT = (".c", ".h", ".js", ".py")
SKIP_DIRS = {
    "build", "_build", "outputs", "outputs_ext", "node_modules",
    "linux-ext-libs", ".git", "cmake-build-debug", "cmake-build-release",
}

# A code-location reference: optional path prefix, then basename.ext:line[-line]
REF_RE = re.compile(
    r"(?P<path>(?:[\w./+-]+/)?(?P<base>[\w+.-]+\.(?:c|h|js|py))):(?P<l1>\d+)(?:-(?P<l2>\d+))?"
)
# Code symbols come from backticked spans in the prose (avoids grabbing plain
# English words like "installed" that happen to be followed by a paren). The
# fallback is a `name(` with NO space before the paren (a real call, not a
# parenthetical aside).
BT_SPAN = re.compile(r"`([^`]+)`")
IDENT_HEAD = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)")
CALL_NOSPACE = re.compile(r"([A-Za-z_][A-Za-z0-9_]{2,})\($")


def build_basename_index():
    idx = {}
    for root, dirs, files in os.walk(REPO):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for f in files:
            if f.endswith(SRC_EXT):
                idx.setdefault(f, []).append(Path(root) / f)
    return idx


def def_line_of(symbol, path):
    """1-based line of `symbol`'s DEFINITION at column ~0, or None.

    Skips C prototypes / forward declarations (signatures terminated by `;`),
    which otherwise sit in a block near the top of the file and masquerade as
    the definition.
    """
    try:
        lines = path.read_text(errors="replace").splitlines()
    except OSError:
        return None
    n = len(lines)
    if path.suffix in (".c", ".h"):
        sig = re.compile(r"^[A-Za-z_][\w \*]*\b" + re.escape(symbol) + r"\s*\(")
        for i, ln in enumerate(lines):
            if not sig.match(ln):
                continue
            # join forward until the signature's closing ')' to see what follows
            buf, j = ln, i
            while ")" not in buf and j < n - 1 and j < i + 8:
                j += 1
                buf += " " + lines[j]
            after = buf[buf.rfind(")") + 1:].lstrip()
            if after.startswith(";"):
                continue  # prototype / declaration, not the definition
            return i + 1
        return None
    # js / py
    sig = re.compile(
        r"^\s*(?:export\s+)?(?:function\s+|const\s+|def\s+|let\s+|var\s+)?"
        + re.escape(symbol)
        + r"\b\s*[\(=:]"
    )
    for i, ln in enumerate(lines):
        if sig.match(ln):
            return i + 1
    return None


def func_end_line(path, def_line):
    """1-based line where the function starting at `def_line` closes (brace
    balance back to 0), or None. C / JS only."""
    try:
        lines = path.read_text(errors="replace").splitlines()
    except OSError:
        return None
    depth, started = 0, False
    for i in range(def_line - 1, len(lines)):
        for ch in lines[i]:
            if ch == "{":
                depth += 1
                started = True
            elif ch == "}":
                depth -= 1
        if started and depth <= 0:
            return i + 1
    return None


def candidate_symbols(pre):
    """Yield candidate function identifiers from the doc context before a ref,
    nearest first. Skips wildcard families (`gobj_write_*_attr`)."""
    seen = []
    for span in reversed(BT_SPAN.findall(pre)):
        span = span.strip()
        if "*" in span:
            continue  # wildcard family — not a single resolvable symbol
        mm = IDENT_HEAD.match(span)
        if mm:
            seen.append(mm.group(1))
    mm = CALL_NOSPACE.search(pre.rstrip())
    if mm:
        seen.append(mm.group(1))
    out, dedup = [], set()
    for s in seen:
        if s not in dedup:
            dedup.add(s)
            out.append(s)
    return out


ADJACENT_FUNC = re.compile(r"`([A-Za-z_][A-Za-z0-9_]*)\s*\(?\)?`\s*\(?\s*$")


def func_def_for_ref(pre, path, l1, l2):
    """Return the definition line to link this ref to, or None.

    Two ways a ref earns a link to the CURRENT source:
      (b) the ref sits right after a backticked function name
          (`fn()` (file.c:NNNN)) — a definition-line citation. Link to the
          function's current def even if the cited number drifted.
      (a) the cited line is provably inside some nearby function's body span —
          an internal citation. Link to that function's def.
    Everything else (globals, macros, structs, moved functions, plain prose)
    returns None and the caller drops the number.
    """
    # (b) function name immediately adjacent to the ref
    preb = pre.rstrip()
    if preb.endswith("`"):
        preb = preb[:-1]  # the ref's own opening backtick, if any
    mb = ADJACENT_FUNC.search(preb)
    if mb and "*" not in mb.group(0):
        dl = def_line_of(mb.group(1), path)
        if dl is not None:
            return dl
    # (a) cited line inside a nearby function's span
    for sym in candidate_symbols(pre):
        dl = def_line_of(sym, path)
        if dl is None:
            continue
        fe = func_end_line(path, dl)
        if fe is not None and dl <= l1 and l2 <= fe:
            return dl
    return None


def file_len(path):
    try:
        with path.open("rb") as fh:
            return sum(1 for _ in fh)
    except OSError:
        return None


def resolve_path(m, basename_index):
    raw = m.group("path")
    base = m.group("base")
    if "/" in raw:
        p = REPO / raw
        if p.exists():
            return p
    cands = basename_index.get(base, [])
    if len(cands) == 1:
        return cands[0]
    return None  # zero or ambiguous


def doc_files():
    for root in DOC_ROOTS:
        for p in root.rglob("*.md"):
            if "_build" in p.parts:
                continue
            yield p


GH_BLOB = "https://github.com/artgins/yunetas/blob"


def gh_url(path, l1, l2, tag):
    rel = path.relative_to(REPO).as_posix()
    frag = f"#L{l1}" + (f"-L{l2}" if l2 else "")
    return f"{GH_BLOB}/{tag}/{rel}{frag}"


def linkify_doc(doc, basename_index, tag):
    """Hybrid transform of one doc:
      - symbol-anchored + verified ref  -> link `file:line` to GitHub (pinned).
      - ref with no nearby symbol        -> strip the :line (keep the file).
      - symbol present but unverifiable  -> leave untouched (conservative).
    Returns (n_linked, n_stripped, n_left).
    """
    text = doc.read_text()
    n_link = n_strip = n_left = 0
    out_lines = []
    for line in text.splitlines(keepends=True):
        eol = ""
        if line.endswith("\n"):
            line, eol = line[:-1], "\n"
        out, pos = [], 0
        for m in REF_RE.finditer(line):
            s, e = m.start(), m.end()
            bt = s > 0 and line[s - 1] == "`" and e < len(line) and line[e] == "`"
            ref = m.group(0)
            ft = m.group("path")
            l1 = int(m.group("l1"))
            l2 = int(m.group("l2")) if m.group("l2") else l1
            path = resolve_path(m, basename_index)
            pre = line[max(0, s - 80):s]
            dl = func_def_for_ref(pre, path, l1, l2) if path is not None else None

            span_s, span_e = (s - 1, e + 1) if bt else (s, e)
            out.append(line[pos:span_s])
            if dl is not None:
                # link the file:def_line of the function this ref lives in
                url = gh_url(path, dl, None, tag)
                text = f"{ft}:{dl}"
                out.append(f"[`{text}`]({url})" if bt else f"[{text}]({url})")
                n_link += 1
            elif path is not None:
                # resolvable file but the line is not provably inside a function
                # (internal/global/macro/moved) -> drop the unverifiable number
                out.append(f"`{ft}`" if bt else ft)
                n_strip += 1
            else:
                out.append(f"`{ref}`" if bt else ref)  # unresolvable file: leave
                n_left += 1
            pos = span_e
        out.append(line[pos:])
        out_lines.append("".join(out) + eol)
    doc.write_text("".join(out_lines))
    return n_link, n_strip, n_left


# ----------------------------------------------------------------------------
# Prose line-references (problem 1) — the `file.c:NNN` linkify above only sees
# colon-form refs. Two sibling forms it never touched, both unverifiable line
# numbers that drift and can't be clicked, both dropped per the same policy:
#   - prose:  "(lines 316-335)", "(line 42)", "at lines 58-113", ", line 2893",
#             "(deb lines 324-343)", "(deb postinst lines 1232-1235)".
#   - comma:  `file.c, 3622`, `c_ievent_srv.c, 1070, 1092` (file + bare numbers
#             inside one backticked span, no colon).
# Multi-line prose refs and malformed links are left for a human (they need
# context the regex can't see).
# ----------------------------------------------------------------------------
PROSE_PAREN = re.compile(
    r"\s*\((?:deb\s+|deb\s+postinst\s+)?lines?\s+\d+(?:-\d+)?"
    r"(?:,\s*(?:options\s+at\s+)?(?:lines?\s+)?\d+(?:-\d+)?)*\)"
)
PROSE_COMMA_LINE = re.compile(r",\s*lines?\s+\d+(?:-\d+)?")
PROSE_AT_LINE = re.compile(r"\s+at\s+lines?\s+\d+(?:-\d+)?")
# `file.c, 196-199` / `file.c, 774, 794, 3284-4365` — file + bare numbers
# (and ranges) inside ONE backticked span.
COMMA_FILE_REF = re.compile(
    r"`(?P<f>(?:[\w./+-]+/)?[\w+.-]+\.(?:c|h|js|py)),[\s\d,-]+`"
)
# `file.c`, `3516-3517` — the numbers sit in a SEPARATE backticked span right
# after the file's span and a comma. Collapse to just the file's span.
TWO_SPAN_FILE_REF = re.compile(
    r"(?P<f>`(?:[\w./+-]+/)?[\w+.-]+\.(?:c|h|js|py)`),\s*`\d+(?:-\d+)?(?:,\s*\d+(?:-\d+)?)*`"
)


def strip_prose_doc(doc):
    """Drop unverifiable prose / comma line numbers from one doc. Returns the
    number of refs stripped."""
    text = doc.read_text()
    n = 0

    def count_sub(rx, repl, s):
        nonlocal n
        s2, c = rx.subn(repl, s)
        n += c
        return s2

    text = count_sub(PROSE_PAREN, "", text)
    text = count_sub(PROSE_COMMA_LINE, "", text)
    text = count_sub(PROSE_AT_LINE, "", text)
    text = count_sub(COMMA_FILE_REF, lambda m: f"`{m.group('f')}`", text)
    text = count_sub(TWO_SPAN_FILE_REF, lambda m: m.group("f"), text)
    if n:
        doc.write_text(text)
    return n


# ----------------------------------------------------------------------------
# Symbol links (problem 2) — a backticked symbol in narrative prose is rendered
# coloured by the theme, indistinguishable from a link, yet leads nowhere. Link
# the ones with a real, stable target, first occurrence per page only:
#   - function with a UNIQUE definition in the tree  -> GitHub source (pinned).
#   - gclass `C_XXX` with a `(gclass-c-xxx)=` doc label -> that label.
#   - CLI tool with a `(util-xxx)=` / `(tool-xxx)=` doc label -> that label.
# Events (`EV_*`), states (`ST_*`), types, kw keys, ambiguous names: no stable
# target, left as code. The generated `api/` tree is already cross-linked and
# is skipped. A symbol on its own doc page is not self-linked.
# ----------------------------------------------------------------------------
C_FUNC_DEF = re.compile(r"^[A-Za-z_][\w \*]*?\b([A-Za-z_][A-Za-z0-9_]{2,})\s*\(")
JS_FUNC_DEF = re.compile(
    r"^\s*(?:export\s+)?(?:async\s+)?function\s+([A-Za-z_][A-Za-z0-9_]{2,})\s*\("
)
LABEL_DEF = re.compile(r"^\(([a-z0-9_-]+)\)=", re.M)
# A backticked single symbol not already inside a [..](..) link.
SYMBOL_SPAN = re.compile(r"(?<!\[)`([A-Za-z_][A-Za-z0-9_]{2,}(?:\(\))?)`(?!\])")
FENCE = re.compile(r"^\s*(```|~~~)")


# A doc symbol points at the canonical Linux kernel / yuno source, never at
# test helpers or the ESP32 port's vendored copies (those create misleading
# unique matches, e.g. `error_code` -> a vendored jansson test).
FUNC_INDEX_SKIP = SKIP_DIRS | {"test", "tests", "performance", "stress", "root-esp32"}


def build_func_symbol_index():
    """symbol -> set(paths) for C/JS function DEFINITIONS across the tree."""
    idx = {}
    for root, dirs, files in os.walk(REPO):
        dirs[:] = [d for d in dirs if d not in FUNC_INDEX_SKIP]
        for f in files:
            if not f.endswith(SRC_EXT):
                continue
            p = Path(root) / f
            try:
                lines = p.read_text(errors="replace").splitlines()
            except OSError:
                continue
            for ln in lines:
                if f.endswith((".c", ".h")):
                    m = C_FUNC_DEF.match(ln)
                    if m and ")" in ln:
                        after = ln[ln.rfind(")") + 1:].lstrip()
                        if after.startswith("{") or after == "":
                            idx.setdefault(m.group(1), set()).add(p)
                elif f.endswith(".js"):
                    m = JS_FUNC_DEF.match(ln)
                    if m:
                        idx.setdefault(m.group(1), set()).add(p)
    return idx


def build_doc_labels():
    """Scan the docs for mystmd target labels. Returns:
      gclass_labels: set of 'gclass-...' labels that exist,
      tool_labels:   dict tool-name -> 'util-x'/'tool-x' label,
      doc_own:       dict doc-path -> set(labels it defines) (for self-links)."""
    gclass_labels, tool_labels, doc_own = set(), {}, {}
    for doc in doc_files():
        labels = set(LABEL_DEF.findall(doc.read_text(errors="replace")))
        doc_own[doc] = labels
        for lbl in labels:
            if lbl.startswith("gclass-"):
                gclass_labels.add(lbl)
            elif lbl.startswith("util-") or lbl.startswith("tool-"):
                tool_labels[lbl.split("-", 1)[1]] = lbl
    return gclass_labels, tool_labels, doc_own


def link_symbols_doc(doc, func_index, gclass_labels, tool_labels, own_labels, tag):
    """Link the first occurrence per page of each resolvable symbol. Returns
    (n_func, n_gclass, n_tool)."""
    text = doc.read_text()
    seen = set()
    nf = ng = nt = 0
    out_lines, in_fence = [], False
    for line in text.splitlines(keepends=True):
        if FENCE.match(line):
            in_fence = not in_fence
            out_lines.append(line)
            continue
        if in_fence:
            out_lines.append(line)
            continue

        def repl(m):
            nonlocal nf, ng, nt
            inner = m.group(1)
            name = inner[:-2] if inner.endswith("()") else inner
            if name in seen:
                return m.group(0)
            # gclass label
            if name.startswith("C_"):
                lbl = "gclass-" + name.lower().replace("_", "-")
                if lbl in gclass_labels and lbl not in own_labels.get(doc, ()):
                    seen.add(name)
                    ng += 1
                    return f"[`{inner}`](#{lbl})"
            # CLI tool label
            if name in tool_labels and tool_labels[name] not in own_labels.get(doc, ()):
                seen.add(name)
                nt += 1
                return f"[`{inner}`](#{tool_labels[name]})"
            # unique function definition -> GitHub source
            paths = func_index.get(name)
            if paths and len(paths) == 1:
                path = next(iter(paths))
                dl = def_line_of(name, path)
                if dl is not None:
                    seen.add(name)
                    nf += 1
                    return f"[`{inner}`]({gh_url(path, dl, None, tag)})"
            return m.group(0)

        out_lines.append(SYMBOL_SPAN.sub(repl, line))
    new = "".join(out_lines)
    if nf or ng or nt:
        doc.write_text(new)
    return nf, ng, nt


def narrative_docs():
    """Doc files that are hand-written prose (skip the generated api/ tree)."""
    for doc in doc_files():
        if "api" in doc.relative_to(REPO).parts:
            continue
        yield doc


def main():
    verbose = "-v" in sys.argv
    do_linkify = any(a == "--linkify" or a.startswith("--linkify=") for a in sys.argv)
    do_strip = "--strip-prose" in sys.argv
    do_symbols = any(a == "--link-symbols" or a.startswith("--link-symbols=") for a in sys.argv)
    tag = "7.5.1"
    for a in sys.argv:
        if a.startswith("--linkify="):
            tag = a.split("=", 1)[1]
        if a.startswith("--link-symbols="):
            tag = a.split("=", 1)[1]
    basename_index = build_basename_index()

    if do_strip:
        total = 0
        for doc in doc_files():
            total += strip_prose_doc(doc)
        print(f"strip-prose: dropped {total} unverifiable prose / comma line numbers.")
        return 0

    if do_symbols:
        func_index = build_func_symbol_index()
        gclass_labels, tool_labels, own_labels = build_doc_labels()
        tf = tg = tt = 0
        for doc in narrative_docs():
            a, b, c = link_symbols_doc(
                doc, func_index, gclass_labels, tool_labels, own_labels, tag
            )
            tf += a
            tg += b
            tt += c
        print(
            f"link-symbols (pinned to {tag}): linked {tf} functions to source, "
            f"{tg} gclasses and {tt} CLI tools to their doc page."
        )
        return 0

    if do_linkify:
        tl = ts = tlf = 0
        for doc in doc_files():
            a, b, c = linkify_doc(doc, basename_index, tag)
            tl += a
            ts += b
            tlf += c
        print(
            f"linkify (pinned to {tag}): linked {tl} refs to their function "
            f"definition, dropped the line from {ts} unverifiable refs, "
            f"left {tlf} (unresolvable file)."
        )
        return 0

    # Default: audit only. Report what is verifiably linkable vs not.
    n_total = n_link = n_oob = n_strip = n_unres = 0
    oobs, unres = [], []
    for doc in doc_files():
        rel = doc.relative_to(REPO)
        for lineno, line in enumerate(doc.read_text(errors="replace").splitlines(), 1):
            for m in REF_RE.finditer(line):
                n_total += 1
                l1 = int(m.group("l1"))
                l2 = int(m.group("l2")) if m.group("l2") else l1
                path = resolve_path(m, basename_index)
                if path is None:
                    n_unres += 1
                    unres.append((rel, lineno, m.group(0)))
                    continue
                pre = line[max(0, m.start() - 80):m.start()]
                if func_def_for_ref(pre, path, l1, l2) is not None:
                    n_link += 1
                else:
                    flen = file_len(path)
                    if flen is not None and l1 > flen:
                        n_oob += 1
                        oobs.append((rel, lineno, m.group(0), flen))
                    else:
                        n_strip += 1

    print(f"Scanned {n_total} code-location references.\n")
    print(f"  linkable (line is inside a known function): {n_link}")
    print(f"  not verifiable (would drop the number)    : {n_strip}")
    print(f"  out of bounds (past EOF)                  : {n_oob}")
    print(f"  unresolvable file (missing/ambiguous)     : {n_unres}")
    if oobs:
        print("\n--- OUT OF BOUNDS ---")
        for rel, ln, ref, flen in oobs:
            print(f"  {rel}:{ln}  {ref}  (file has {flen} lines)")
    if verbose and unres:
        print("\n--- UNRESOLVABLE FILE (basename missing or ambiguous) ---")
        for rel, ln, ref in unres[:40]:
            print(f"  {rel}:{ln}  {ref}")
    return 1 if oobs else 0


if __name__ == "__main__":
    sys.exit(main())
