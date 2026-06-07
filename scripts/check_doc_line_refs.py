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
  prose). Also flags GitHub blob links pinned to a MOVING ref (`main`, a branch)
  instead of an immutable release tag — those drift and should be re-pinned.
  Exit 1 on out-of-bounds refs (a line past EOF) or any unpinned blob link.

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

  --link-jansson  Make backticked jansson symbols (`json_pack`, `json_t`,
  `json_object_set_new`, …) clickable, first occurrence per page only, pointing
  at the jansson API reference (`apiref.html#c.<name>`). The target set is the
  authoritative list of `#c.json_*` / `#c.jansson_*` anchors jansson documents,
  so yuneta's own `json_*` helpers (e.g. `json_config`) are NOT matched and keep
  flowing to `--link-symbols`. External, untagged URL -> `--repin` never touches
  it. The api/ tree is skipped like the other symbol modes.

  --link-files[=TAG]  Make backticked bare file paths in narrative prose
  clickable. A span that is EXACTLY a source path (`kernel/.../c_yuno.c`,
  `glogger.c`) or a shell script (`yunetas-env.sh`) and resolves to a real file
  -> GitHub FILE link (no line anchor, so it never drifts; re-pinned to TAG per
  release; `.sh` is covered here only, not by the audit/symbol modes). The two locator forms
  `--linkify` / `--link-symbols` never reach: bare filenames in prose and the
  §"Code pointers" reference tables. Every occurrence, all pages (api/ skipped).
  Ambiguous basenames and non-existent paths stay as code.

  --repin=NEWTAG  RELEASE-TIME re-pin. The --linkify / --link-symbols /
  --link-files modes are one-shot (they link bare refs and now skip anything
  already linked), so they do NOT move existing links to a new release tag.
  --repin does: it rewrites every `/blob/<anytag>/` -> `/blob/NEWTAG/` and
  recomputes the `#L` anchor of every SYMBOL link from that symbol's CURRENT
  definition line, keeping `[`fn`](...) at [file:line](...)` row twins in sync.
  File-level links just get the new tag. Symbols whose definition vanished are
  reported for a human. Run this once when tagging a release; idempotent.

  The one-shot link modes are now idempotent too: --linkify leaves refs already
  inside a `[..](..)` link untouched; --link-symbols seeds itself from symbols
  already linked on the page so it never links a second occurrence. Re-running
  any mode over already-processed docs is a safe no-op.

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
    scripts/check_doc_line_refs.py --link-files    # link bare file paths to source
    scripts/check_doc_line_refs.py --repin=7.6.0   # release-time: re-pin to a new tag
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
# The ref in a blob URL (`/blob/<ref>/...`). A PINNED ref is an immutable
# release tag (`7.5.1`, `7.6`); anything else (`main`, `master`, a branch) moves
# under the link and is flagged by the audit. Re-pin with `--repin=<tag>`.
BLOB_REF = re.compile(re.escape(GH_BLOB) + r"/(?P<ref>[^/]+)/")
PINNED_REF = re.compile(r"^\d+\.\d+(?:\.\d+)?$")


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
            # Already the text of a `[file:line](url)` link (a previous pass, or
            # a hand-written link): leave it verbatim. Without this guard a
            # re-run would either strip the visible line (the anchoring function
            # name gets pushed out of the 80-char window by the inserted URL) or
            # double-wrap into a broken `[[..](..)](..)`. This is what makes
            # `--linkify` idempotent.
            if span_s > 0 and line[span_s - 1] == "[" and line[span_e:span_e + 2] == "](":
                out.append(line[pos:span_e])
                pos = span_e
                continue
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
# A symbol that is ALREADY linked: `[`name`](...)` / `[`name()`](...)`. Used to
# seed `seen` so a re-run never links a second occurrence of an already-linked
# symbol (the first linked hit is skipped by SYMBOL_SPAN, so without seeding the
# next bare occurrence would wrongly become the new "first").
LINKED_SYMBOL = re.compile(r"\[`([A-Za-z_][A-Za-z0-9_]{2,})(?:\(\))?`\]\(")
FENCE = re.compile(r"^\s*(```|~~~)")


# A doc symbol points at the canonical Linux kernel / yuno source, never at
# test helpers or the ESP32 port's vendored copies (those create misleading
# unique matches, e.g. `error_code` -> a vendored jansson test).
FUNC_INDEX_SKIP = SKIP_DIRS | {"test", "tests", "performance", "stress", "root-esp32"}


def c_head_after(lines, i):
    """First non-space char AFTER the balanced closing ')' of a C function head
    that starts on line `lines[i]`, or '' if the head does not close cleanly in
    a sane window. Balances parens so a function-pointer parameter (`(void)`)
    mid-signature is not mistaken for the closing one — '{' marks a definition,
    ';' a prototype. Multi-line signatures (the common kernel style) close fine.
    """
    depth, started = 0, False
    joined = " ".join(lines[i:i + 60])
    for k, ch in enumerate(joined):
        if ch == "(":
            depth += 1
            started = True
        elif ch == ")":
            depth -= 1
            if started and depth == 0:
                rest = joined[k + 1:].lstrip()
                return rest[:1]
    return ""


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
            for i, ln in enumerate(lines):
                if f.endswith((".c", ".h")):
                    m = C_FUNC_DEF.match(ln)
                    if not m:
                        continue
                    # Balance parens to the signature's true close so multi-line
                    # signatures are indexed and func-pointer params don't fool
                    # us. A prototype ends ';' (skip); a definition opens '{'.
                    after = c_head_after(lines, i)
                    if after in ("", ";"):
                        continue
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
    # Seed with symbols that already have a linked occurrence anywhere on the
    # page -> they stay first-only and a re-run is a no-op (idempotent).
    seen = set(LINKED_SYMBOL.findall(text))
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


# ----------------------------------------------------------------------------
# jansson links — jansson's `json_*` API is load-bearing across yuneta yet its
# symbols lead nowhere in the prose. Link them to jansson's own API reference
# (Sphinx C-domain anchors `#c.<name>`). The set below is the authoritative list
# of anchors jansson publishes; keeping it explicit means yuneta's own `json_*`
# helpers (notably `json_config`) are NOT swept here and stay with --link-symbols.
# The URL is external and untagged, so --repin leaves it alone.
# ----------------------------------------------------------------------------
JANSSON_APIREF = "https://jansson.readthedocs.io/en/latest/apiref.html"
JANSSON_API = {
    "jansson_version_cmp", "jansson_version_str", "json_array",
    "json_array_append", "json_array_append_new", "json_array_clear",
    "json_array_extend", "json_array_foreach", "json_array_get",
    "json_array_insert", "json_array_insert_new", "json_array_remove",
    "json_array_set", "json_array_set_new", "json_array_size", "json_boolean",
    "json_boolean_value", "json_copy", "json_decref", "json_deep_copy",
    "json_dump_callback", "json_dump_callback_t", "json_dump_file",
    "json_dumpb", "json_dumpf", "json_dumpfd", "json_dumps", "json_equal",
    "json_error_code", "json_error_t", "json_false", "json_free_t",
    "json_get_alloc_funcs", "json_get_alloc_funcs2", "json_incref",
    "json_int_t", "json_integer", "json_integer_set", "json_integer_value",
    "json_is_array", "json_is_boolean", "json_is_false", "json_is_integer",
    "json_is_null", "json_is_number", "json_is_object", "json_is_real",
    "json_is_string", "json_is_true", "json_load_callback",
    "json_load_callback_t", "json_load_file", "json_loadb", "json_loadf",
    "json_loadfd", "json_loads", "json_malloc_t", "json_null",
    "json_number_value", "json_object", "json_object_clear",
    "json_object_del", "json_object_deln", "json_object_foreach",
    "json_object_foreach_safe", "json_object_get", "json_object_getn",
    "json_object_iter", "json_object_iter_at", "json_object_iter_key",
    "json_object_iter_key_len", "json_object_iter_next",
    "json_object_iter_set", "json_object_iter_set_new",
    "json_object_iter_value", "json_object_key_to_iter",
    "json_object_keylen_foreach", "json_object_keylen_foreach_safe",
    "json_object_seed", "json_object_set", "json_object_set_new",
    "json_object_set_new_nocheck", "json_object_set_nocheck",
    "json_object_setn", "json_object_setn_new",
    "json_object_setn_new_nocheck", "json_object_setn_nocheck",
    "json_object_size", "json_object_update", "json_object_update_existing",
    "json_object_update_existing_new", "json_object_update_missing",
    "json_object_update_missing_new", "json_object_update_new",
    "json_object_update_recursive", "json_pack", "json_pack_ex", "json_real",
    "json_real_set", "json_real_value", "json_realloc_t",
    "json_set_alloc_funcs", "json_set_alloc_funcs2", "json_sprintf",
    "json_string", "json_string_length", "json_string_nocheck",
    "json_string_set", "json_string_set_nocheck", "json_string_setn",
    "json_string_setn_nocheck", "json_string_value", "json_stringn",
    "json_stringn_nocheck", "json_t", "json_true", "json_type", "json_typeof",
    "json_unpack", "json_unpack_ex", "json_vpack_ex", "json_vsprintf",
    "json_vunpack_ex",
}


def link_jansson_doc(doc):
    """Link the first occurrence per page of each jansson symbol to the jansson
    API reference. Returns the count linked."""
    text = doc.read_text()
    seen = set(LINKED_SYMBOL.findall(text))
    n = 0
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
            nonlocal n
            inner = m.group(1)
            name = inner[:-2] if inner.endswith("()") else inner
            if name in seen or name not in JANSSON_API:
                return m.group(0)
            seen.add(name)
            n += 1
            return f"[`{inner}`]({JANSSON_APIREF}#c.{name})"

        out_lines.append(SYMBOL_SPAN.sub(repl, line))
    new = "".join(out_lines)
    if n:
        doc.write_text(new)
    return n


# ----------------------------------------------------------------------------
# File links (problem 3) — a backticked bare file path in narrative prose
# (`kernel/c/root-linux/src/c_yuno.c`, `glogger.c`) is rendered coloured by the
# theme yet leads nowhere: `--linkify` only sees the colon-form `file:NNN` and
# `--link-symbols` only sees function / gclass / tool symbols. Link every
# occurrence of a backticked span that is EXACTLY a path resolving to a real
# source file to its GitHub FILE url — no line anchor, so it is drift-proof and
# just re-pinned to TAG per release. This is what reaches the bare filenames in
# prose and the "Code pointers" reference tables. Skips code fences and spans
# already inside a [..](..) link.
#
# Shell scripts (`.sh`) are in scope here even though they are not "source" for
# the audit / symbol modes (which only know C/H/JS/PY): a prose `yunetas-env.sh`
# / `set_compiler.sh` is just as much a "click to see the file" pointer. This is
# the ONLY mode that indexes `.sh`, via FILE_LINK_EXT below — SRC_EXT (audit,
# symbol linking) is left untouched.
# ----------------------------------------------------------------------------
FILE_SPAN = re.compile(
    r"(?<!\[)`((?:[\w./+-]+/)?[\w+.-]+\.(?:c|h|js|py|sh))`(?!\])"
)

# Extensions the file linker resolves. A superset of SRC_EXT (adds shell
# scripts) used ONLY by build_file_index / link_files_doc.
FILE_LINK_EXT = SRC_EXT + (".sh",)


def build_file_index():
    """basename -> [paths] for the CANONICAL source only: the same exclusion as
    the symbol linker (FUNC_INDEX_SKIP drops tests / perf / stress / the ESP32
    port). A prose `c_timer.c` then resolves to `root-linux/src/c_timer.c`
    instead of going ambiguous against the ESP32 port's copy. Explicit full
    paths (`tests/c/.../main.c`) still link — those hit the verbatim-path branch
    of resolve_file and never consult this index. Indexes FILE_LINK_EXT, so the
    `.sh` scripts the file linker also covers are resolvable (a non-unique
    basename like `build.sh` stays ambiguous -> bare, as for any other file)."""
    idx = {}
    for root, dirs, files in os.walk(REPO):
        dirs[:] = [d for d in dirs if d not in FUNC_INDEX_SKIP]
        for f in files:
            if f.endswith(FILE_LINK_EXT):
                idx.setdefault(f, []).append(Path(root) / f)
    return idx


def gh_file_url(path, tag):
    rel = path.relative_to(REPO).as_posix()
    return f"{GH_BLOB}/{tag}/{rel}"


def resolve_file(raw, basename_index):
    """Resolve a backticked path token to a real source file, or None.

    Three steps, each only firing when UNIQUE:
      - full repo path that exists verbatim;
      - abbreviated / doc-relative path (`src/c_agent.c`, `tls/openssl.c`,
        `yuno_agent/src/main.c`) -> the one real file whose repo-path ends with
        it. This is intentionally a SUFFIX match: a path written under the wrong
        directory (`kernel/c/gobj-c/src/msg_ievent.c` for a file that really
        lives under `root-linux/src/`) is NOT a suffix of the real path, so it
        stays bare and surfaces as a doc bug rather than being silently relinked;
      - a bare basename with a single definition in the tree.
    Ambiguous matches (e.g. `src/main.c`) return None and stay as code.

    An ABSOLUTE token (`/yuneta/store/certs/copy-certs.sh`) is a runtime path,
    not a repo source file: it is rejected outright. (Without this guard
    `REPO / raw` collapses to the absolute path, which may exist on a deployed
    box, and gh_file_url's relative_to(REPO) then blows up.)"""
    if raw.startswith("/"):
        return None
    if "/" in raw:
        p = REPO / raw
        if p.exists() and REPO in p.resolve().parents:
            return p
        base = raw.rsplit("/", 1)[1]
        suf = "/" + raw
        cands = [c for c in basename_index.get(base, []) if c.as_posix().endswith(suf)]
        return cands[0] if len(cands) == 1 else None
    cands = basename_index.get(raw, [])
    return cands[0] if len(cands) == 1 else None


def link_files_doc(doc, basename_index, tag):
    """Link every backticked bare file path that resolves to a real source file
    to its GitHub file url. Returns the number of paths linked."""
    text = doc.read_text()
    n = 0
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
            nonlocal n
            raw = m.group(1)
            path = resolve_file(raw, basename_index)
            if path is None:
                return m.group(0)
            n += 1
            return f"[`{raw}`]({gh_file_url(path, tag)})"

        out_lines.append(FILE_SPAN.sub(repl, line))
    new = "".join(out_lines)
    if n:
        doc.write_text(new)
    return n


# ----------------------------------------------------------------------------
# Re-pin (release-time) — the `--linkify` / `--link-symbols` / `--link-files`
# modes are ONE-SHOT transforms that find bare refs and link them; they are not
# re-pin operations and (now) skip anything already linked. When you cut a NEW
# release the existing links still point at the OLD tag with anchors computed
# against the OLD source. `--repin=<newtag>` is the release-time pass:
#
#   - rewrite every `/blob/<anytag>/` -> `/blob/<newtag>/` (mechanical, always);
#   - recompute the `#L` anchor of every SYMBOL link (`[`fn`](...#Ln)` /
#     `[`fn()`](...#Ln)`) from that symbol's CURRENT definition line in the
#     linked file — so the anchor is accurate for the new release even if the
#     function moved;
#   - keep the two columns of a `[`fn`](...) at [file:line](...)` row in sync:
#     a locator-form `[file:line]` link whose old anchor matches a symbol link
#     recomputed earlier on the SAME line is moved to the same new line (text
#     `:N` and `#LN` both updated);
#   - file-level links (`[`path.c`](...)`, no anchor) just get the tag.
#
# A symbol whose definition can no longer be found is reported (its anchor is
# left as-is) so a human can fix the stale reference. Idempotent: re-running
# with the same tag rewrites nothing and recomputes anchors to the same lines.
# ----------------------------------------------------------------------------
GH_LINK = re.compile(
    r"\[(?P<txt>`[^`]+`|[^\]]+)\]\("
    r"(?P<url>" + re.escape(GH_BLOB) + r"/(?P<tag>[^/]+)/(?P<rel>[^)#]+)"
    r"(?:#L(?P<l1>\d+)(?:-L(?P<l2>\d+))?)?)\)"
)
SYMBOL_TEXT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
FILELINE_TEXT = re.compile(r":(\d+)`?$")


def symbol_of_text(txt):
    """The bare symbol named by a link's text, or None. `` `fn()` `` -> 'fn'."""
    t = txt.strip()
    if t.startswith("`") and t.endswith("`"):
        t = t[1:-1]
    if t.endswith("()"):
        t = t[:-2]
    return t if SYMBOL_TEXT.match(t) else None


def repin_line(line, newtag, missing):
    """Re-pin every GitHub blob link on one line. Returns (newline, n_tag,
    n_anchor, n_drift)."""
    links = list(GH_LINK.finditer(line))
    if not links:
        return line, 0, 0, 0

    # Sub-pass 1: recompute single-line symbol-link anchors; map old line -> new
    # so locator twins on the same line can follow.
    anchor_map = {}
    for m in links:
        l1, l2 = m.group("l1"), m.group("l2")
        if not l1 or l2:
            continue
        sym = symbol_of_text(m.group("txt"))
        if not sym:
            continue
        path = REPO / m.group("rel")
        nl = def_line_of(sym, path) if path.exists() else None
        if nl is not None:
            anchor_map[int(l1)] = nl

    # Sub-pass 2: rebuild the line.
    out, pos = [], 0
    n_tag = n_anchor = n_drift = 0
    for m in links:
        out.append(line[pos:m.start()])
        txt, tag, rel = m.group("txt"), m.group("tag"), m.group("rel")
        l1, l2 = m.group("l1"), m.group("l2")
        new_txt, new_l1 = txt, None
        if l1 and not l2:
            sym = symbol_of_text(txt)
            if sym:
                path = REPO / rel
                nl = def_line_of(sym, path) if path.exists() else None
                if nl is not None:
                    new_l1 = nl
                else:
                    missing.append((rel, sym))
            elif int(l1) in anchor_map:
                # locator twin (`file:line`) of a recomputed symbol link
                new_l1 = anchor_map[int(l1)]
                new_txt = FILELINE_TEXT.sub(
                    lambda mm: f":{new_l1}" + ("`" if txt.endswith("`") else ""), txt
                )
        if l1:
            if l2:
                anchor = f"#L{l1}-L{l2}"
            else:
                final = new_l1 if new_l1 is not None else int(l1)
                anchor = f"#L{final}"
                if new_l1 is not None:
                    n_anchor += 1
                    if new_l1 != int(l1):
                        n_drift += 1
        else:
            anchor = ""
        if tag != newtag:
            n_tag += 1
        out.append(f"[{new_txt}]({GH_BLOB}/{newtag}/{rel}{anchor})")
        pos = m.end()
    out.append(line[pos:])
    return "".join(out), n_tag, n_anchor, n_drift


def repin_doc(doc, newtag, missing):
    """Re-pin all GitHub blob links in one doc. Returns (n_tag, n_anchor,
    n_drift, changed)."""
    text = doc.read_text()
    out, n_tag, n_anchor, n_drift = [], 0, 0, 0
    for line in text.splitlines(keepends=True):
        nl, a, b, c = repin_line(line, newtag, missing)
        out.append(nl)
        n_tag += a
        n_anchor += b
        n_drift += c
    new = "".join(out)
    changed = new != text
    if changed:
        doc.write_text(new)
    return n_tag, n_anchor, n_drift, 1 if changed else 0


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
    do_files = any(a == "--link-files" or a.startswith("--link-files=") for a in sys.argv)
    do_jansson = "--link-jansson" in sys.argv
    repin_tag = None
    for a in sys.argv:
        if a.startswith("--repin="):
            repin_tag = a.split("=", 1)[1]
    tag = "7.5.1"
    for a in sys.argv:
        if a.startswith("--linkify="):
            tag = a.split("=", 1)[1]
        if a.startswith("--link-symbols="):
            tag = a.split("=", 1)[1]
        if a.startswith("--link-files="):
            tag = a.split("=", 1)[1]
    basename_index = build_basename_index()

    if repin_tag:
        missing = []
        t_tag = t_anchor = t_drift = t_changed = 0
        for doc in doc_files():
            a, b, c, ch = repin_doc(doc, repin_tag, missing)
            t_tag += a
            t_anchor += b
            t_drift += c
            t_changed += ch
        print(
            f"repin -> {repin_tag}: {t_tag} tags rewritten, {t_anchor} symbol "
            f"anchors recomputed ({t_drift} drifted), {t_changed} files changed."
        )
        if missing:
            print(
                f"\n--- {len(missing)} symbol links whose target no longer "
                f"resolves (anchor left as-is; fix by hand) ---"
            )
            for rel, sym in missing[:40]:
                print(f"  {sym}  ->  {rel}")
        return 0

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

    if do_files:
        file_index = build_file_index()
        total = 0
        for doc in narrative_docs():
            total += link_files_doc(doc, file_index, tag)
        print(f"link-files (pinned to {tag}): linked {total} bare file paths to GitHub source.")
        return 0

    if do_jansson:
        total = 0
        for doc in narrative_docs():
            total += link_jansson_doc(doc)
        print(f"link-jansson: linked {total} jansson symbols to the jansson API reference.")
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
    oobs, unres, unpinned = [], [], []
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
            # A GitHub blob link on a moving ref (main / a branch) instead of a
            # release tag — it will drift. Same guard class as out-of-bounds.
            for m in BLOB_REF.finditer(line):
                ref = m.group("ref")
                if not PINNED_REF.match(ref):
                    unpinned.append((rel, lineno, ref))

    print(f"Scanned {n_total} code-location references.\n")
    print(f"  linkable (line is inside a known function): {n_link}")
    print(f"  not verifiable (would drop the number)    : {n_strip}")
    print(f"  out of bounds (past EOF)                  : {n_oob}")
    print(f"  unresolvable file (missing/ambiguous)     : {n_unres}")
    print(f"  unpinned blob links (moving ref, not tag) : {len(unpinned)}")
    if oobs:
        print("\n--- OUT OF BOUNDS ---")
        for rel, ln, ref, flen in oobs:
            print(f"  {rel}:{ln}  {ref}  (file has {flen} lines)")
    if unpinned:
        refs = ", ".join(sorted({r for _, _, r in unpinned}))
        print(f"\n--- UNPINNED BLOB LINKS ({refs}) — re-pin with --repin=<tag> ---")
        for rel, ln, ref in unpinned[:40]:
            print(f"  {rel}:{ln}  /blob/{ref}/")
        if len(unpinned) > 40:
            print(f"  … and {len(unpinned) - 40} more")
    if verbose and unres:
        print("\n--- UNRESOLVABLE FILE (basename missing or ambiguous) ---")
        for rel, ln, ref in unres[:40]:
            print(f"  {rel}:{ln}  {ref}")
    return 1 if (oobs or unpinned) else 0


if __name__ == "__main__":
    sys.exit(main())
