#!/usr/bin/env python3
"""
verify_js_api_coverage.py — Keep the JS API documentation honest.

The C side has scripts/verify_api_coverage.py, which compares every PUBLIC
function of a header against the anchors in docs/doc.yuneta.io. The JS side
had nothing, and it showed: `gobj_post_event()` lived in the JS runtime for
releases while a search of the site found no entry for it, because no JS
symbol carried an anchor and the appendix index was C-only.

This script does two jobs for the two JS packages, which are git submodules
with their own release tags:

    kernel/js/gobj-js   -> github.com/artgins/gobj-js
    kernel/js/gobj-ui   -> github.com/artgins/gobj-ui.js

  1. AUDIT (default). Reads every export of both packages, reads every
     `(js_<name>)=` anchor under docs/doc.yuneta.io/api/js and .../api/gobj-ui,
     and reports:
        MISSING  — exported, and no anchor documents it.
        STALE    — anchored, and the package exports it no longer.
     Exits 1 when anything is reported, so it fits a pre-commit hook.

  2. GENERATE (--write). Rewrites the appendix index
     docs/doc.yuneta.io/api/appendix_js_api_index.md with every export of both
     packages: name, signature, defining module and a source link pinned to
     the tag of that submodule. A symbol that carries an anchor also gets a
     link to its reference entry.

     Without --write the script checks that the committed index matches what
     it would generate, and reports DRIFT when it does not.

Why the tag comes from the submodule and not from YUNETA_VERSION: the two
packages version on their own line (gobj-js tracks the SDK loosely, gobj-ui
does not track it at all), so pinning their source links to the SDK tag would
point at a tag that does not exist in those repos. The tag is read from the
package.json of the submodule, and the script verifies that the checked-out
commit really is that tag before it writes a single link.

Usage:
    scripts/verify_js_api_coverage.py            # audit + index drift check
    scripts/verify_js_api_coverage.py --write    # regenerate the appendix index
    scripts/verify_js_api_coverage.py --summary  # one line per package
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DOCS = REPO / "docs" / "doc.yuneta.io"
INDEX = DOCS / "api" / "appendix_js_api_index.md"


#
#   The two JS packages, their repository and the doc tree that covers them
#
@dataclass
class Package:
    name: str               # npm package name
    path: Path              # submodule path in this repo
    repo: str               # github repo that really holds the source
    docdir: str             # doc tree that carries its anchors
    title: str              # section title in the appendix
    #   Filled in by load()
    tag: str = ""
    symbols: dict = field(default_factory=dict)


PACKAGES = [
    Package(
        name="@yuneta/gobj-js",
        path=REPO / "kernel" / "js" / "gobj-js",
        repo="https://github.com/artgins/gobj-js",
        docdir="api/js",
        title="gobj-js (Core Framework)",
    ),
    Package(
        name="@yuneta/gobj-ui",
        path=REPO / "kernel" / "js" / "gobj-ui",
        repo="https://github.com/artgins/gobj-ui.js",
        docdir="api/gobj-ui",
        title="gobj-ui (UI Library)",
    ),
]


#
#   Source scanning
#
EXPORT_BLOCK = re.compile(r"^export\s*\{([^}]*)\}", re.M | re.S)
EXPORT_INLINE = re.compile(
    r"^export\s+(?:async\s+)?(?:function|const|let|var|class)\s+([A-Za-z_$][\w$]*)",
    re.M,
)


def _exported_names(text: str) -> set[str]:
    """Every name that a module exports, whatever the export form."""
    names = set()
    for m in EXPORT_BLOCK.finditer(text):
        for raw in m.group(1).split(","):
            raw = raw.strip()
            if not raw or raw.startswith("//"):
                continue
            #   `foo as bar` exports bar
            names.add(raw.split(" as ")[-1].strip())
    for m in EXPORT_INLINE.finditer(text):
        names.add(m.group(1))
    names.discard("default")
    return names


def _definition(lines: list[str], name: str) -> tuple[int, str] | None:
    """Line number (1-based) and signature of the definition of `name`."""
    escaped = re.escape(name)
    patterns = [
        (re.compile(r"^(?:export\s+)?(?:async\s+)?function\s+" + escaped + r"\s*\("), True),
        (re.compile(r"^(?:export\s+)?class\s+" + escaped + r"\b"), False),
        (re.compile(r"^(?:export\s+)?(?:const|let|var)\s+" + escaped + r"\s*="), False),
    ]
    for pattern, is_function in patterns:
        for i, line in enumerate(lines):
            if not pattern.match(line):
                continue
            sig = line.rstrip()
            #   A parameter list that opens on one line and closes on another
            if is_function and not sig.rstrip().rstrip("{").rstrip().endswith(")"):
                parts, j = [], i
                while j < len(lines) and len(parts) < 12:
                    parts.append(lines[j].strip())
                    if ")" in lines[j]:
                        break
                    j += 1
                sig = " ".join(parts)
            sig = re.sub(r"^export\s+", "", sig)
            #   Drop a trailing body brace and any trailing comment
            sig = re.sub(r"\s*\{\s*$", "", sig.strip())
            sig = re.sub(r"\s*//.*$", "", sig).strip()
            return i + 1, sig
    return None


def scan_package(pkg: Package) -> None:
    """Read the tag and every export of a package."""
    pkg_json = pkg.path / "package.json"
    if not pkg_json.exists():
        sys.exit(
            f"ERROR: {pkg.path} has no package.json. "
            f"Run `git submodule update --init` first."
        )
    pkg.tag = json.loads(pkg_json.read_text())["version"]

    #   The source links carry line numbers, so the checkout must BE the tag.
    #   A detached commit past the tag makes every #L anchor silently wrong.
    try:
        head = subprocess.run(
            ["git", "-C", str(pkg.path), "rev-parse", "HEAD"],
            capture_output=True, text=True, check=True,
        ).stdout.strip()
        tagged = subprocess.run(
            ["git", "-C", str(pkg.path), "rev-list", "-n", "1", pkg.tag],
            capture_output=True, text=True, check=True,
        ).stdout.strip()
    except subprocess.CalledProcessError:
        print(
            f"WARNING: {pkg.name}: no tag {pkg.tag} in the submodule. "
            f"The source links can point at the wrong lines.",
            file=sys.stderr,
        )
    else:
        if head != tagged:
            print(
                f"WARNING: {pkg.name}: HEAD ({head[:9]}) is not tag {pkg.tag} "
                f"({tagged[:9]}). The #L anchors can point at the wrong lines.",
                file=sys.stderr,
            )

    src = pkg.path / "src"
    files = sorted(p for p in src.glob("*.js") if not p.name.endswith(".test.js"))
    for path in files:
        text = path.read_text()
        lines = text.split("\n")
        rel = f"src/{path.name}"
        for name in _exported_names(text):
            found = _definition(lines, name)
            if found is None:
                #   Re-exported from another module of the same package
                continue
            line, sig = found
            #   First definition wins, and src/ order is deterministic
            pkg.symbols.setdefault(name, {"file": rel, "line": line, "sig": sig})

    #   Which symbols are the PUBLIC surface, and which are only reachable
    #   through a deep import of the module.
    #
    #   gobj-js re-exports whole modules (`export * from "./gobj.js"`), so
    #   everything a module exports is public. gobj-ui names its re-exports
    #   one by one, so index.js is a real filter: what it does not name is
    #   reachable only as `@yuneta/gobj-ui/src/<module>.js`. The gate below
    #   demands an entry for the public surface, and lets the rest live in
    #   the appendix index alone.
    barrel = pkg.path / "index.js"
    star_only = True
    named: set[str] = set()
    if barrel.exists():
        text = barrel.read_text()
        named = _exported_names(text)
        star_only = not named and bool(re.search(r"^export\s+\*", text, re.M))
    for name in pkg.symbols:
        pkg.symbols[name]["public"] = True if (star_only or not barrel.exists()) \
            else name in named


#
#   Doc scanning
#
ANCHOR = re.compile(r"^\((js_[A-Za-z_$][\w$]*)\)=\s*$", re.M)


def doc_anchors(docdir: str) -> dict[str, str]:
    """Every `(js_name)=` anchor of a doc tree -> the page that holds it."""
    anchors: dict[str, str] = {}
    root = DOCS / docdir
    if not root.exists():
        return anchors
    for path in sorted(root.rglob("*.md")):
        rel = path.relative_to(DOCS).as_posix()
        for m in ANCHOR.finditer(path.read_text()):
            anchors[m.group(1)[3:]] = rel      # strip the `js_` prefix
    return anchors


#
#   The appendix index
#
def render_index(packages: list[Package], anchors: dict[str, dict[str, str]]) -> str:
    total = sum(len(p.symbols) for p in packages)
    out: list[str] = []
    out.append("---")
    out.append("title: 'Appendix: Kernel JS API — Complete Symbol Index'")
    out.append("description: >-")
    out.append("  Quick-reference listing of every symbol that the Yuneta JavaScript")
    out.append("  packages export, with the signature, the module and a source link.")
    out.append("---")
    out.append("")
    out.append("# Appendix: Kernel JS API — Complete Symbol Index")
    out.append("")
    out.append(
        "This appendix lists **every symbol** that the two JavaScript packages\n"
        "export. Use the find of your browser (**Ctrl+F**) to search by name."
    )
    out.append("")
    out.append(
        "A name in **bold** has a reference entry, and the link goes to it. A name\n"
        "in plain text has no entry yet, and the link goes to the source. Both are\n"
        "in this list, so a search always finds the symbol."
    )
    out.append("")
    out.append(
        "The C functions are in the\n"
        "[Kernel C API index](appendix_api_index.md). A name that exists on both\n"
        "sides is documented on both sides, and the two entries can differ: the\n"
        "signatures are close, and the semantics are not always identical."
    )
    out.append("")
    out.append(
        ":::{note}\n"
        "This page is generated by `scripts/verify_js_api_coverage.py --write`.\n"
        "Do not edit it by hand. Run the script after a submodule bump.\n"
        ":::"
    )
    out.append("")
    out.append(f"**{total} symbols** in {len(packages)} packages.")
    out.append("")

    for pkg in packages:
        pkg_anchors = anchors.get(pkg.name, {})
        documented = sum(1 for n in pkg.symbols if n in pkg_anchors)
        out.append(f"## {pkg.title}")
        out.append("")
        out.append(
            f"**Package:** `{pkg.name}` — **version:** `{pkg.tag}` — "
            f"**source:** [{pkg.repo.split('//')[1]}]({pkg.repo}/tree/{pkg.tag})"
        )
        out.append("")
        out.append(
            f"{len(pkg.symbols)} symbols, {documented} with a reference entry."
        )
        out.append("")

        by_file: dict[str, list[str]] = {}
        for name, info in pkg.symbols.items():
            by_file.setdefault(info["file"], []).append(name)

        for rel in sorted(by_file):
            names = sorted(by_file[rel])
            plural = "symbol" if len(names) == 1 else "symbols"
            out.append(f"### `{rel}` — {len(names)} {plural}")
            out.append("")
            out.append("| Symbol | Signature | Source |")
            out.append("|---|---|---|")
            for name in names:
                info = pkg.symbols[name]
                src = f"{pkg.repo}/blob/{pkg.tag}/{rel}#L{info['line']}"
                page = pkg_anchors.get(name)
                if page:
                    target = os.path.relpath(page, "api")
                    label = f"[**`{name}`**]({target}#js_{name})"
                else:
                    label = f"[`{name}`]({src})"
                sig = info["sig"].replace("|", "\\|")
                #   Keep the table readable: a long initializer is not a signature
                if len(sig) > 110:
                    sig = sig[:107] + "..."
                out.append(f"| {label} | `{sig}` | [L{info['line']}]({src}) |")
            out.append("")

    out.append("## Alphabetical index")
    out.append("")
    out.append("| Symbol | Module | Package |")
    out.append("|---|---|---|")
    rows = []
    for pkg in packages:
        pkg_anchors = anchors.get(pkg.name, {})
        for name, info in pkg.symbols.items():
            page = pkg_anchors.get(name)
            if page:
                target = os.path.relpath(page, "api")
                label = f"[**`{name}`**]({target}#js_{name})"
            else:
                src = f"{pkg.repo}/blob/{pkg.tag}/{info['file']}#L{info['line']}"
                label = f"[`{name}`]({src})"
            rows.append((name.lower(), f"| {label} | `{info['file']}` | {pkg.title} |"))
    for _, row in sorted(rows):
        out.append(row)
    out.append("")
    return "\n".join(out)


#
#   Main
#
def main(argv: list[str]) -> int:
    write = "--write" in argv
    summary = "--summary" in argv

    for pkg in PACKAGES:
        scan_package(pkg)

    anchors = {p.name: doc_anchors(p.docdir) for p in PACKAGES}

    problems = 0
    for pkg in PACKAGES:
        found = anchors[pkg.name]
        public = {n for n, i in pkg.symbols.items() if i["public"]}
        missing = sorted(n for n in public if n not in found)
        deep = sorted(
            n for n in pkg.symbols if n not in public and n not in found
        )
        stale = sorted(n for n in found if n not in pkg.symbols)
        documented = len(pkg.symbols) - len(missing) - len(deep)
        pct = 100.0 * len(public - set(missing)) / len(public) if public else 100.0

        print(
            f"{pkg.name}: public surface {len(public) - len(missing)}/{len(public)} "
            f"documented ({pct:.0f}%), {len(deep)} index-only, {len(stale)} stale"
        )
        if summary:
            continue
        if missing:
            problems += len(missing)
            print(f"  --- MISSING ({len(missing)}) — public, no anchor ---")
            for name in missing:
                info = pkg.symbols[name]
                print(f"    {name:38} {info['file']}:{info['line']}")
        if deep:
            print(
                f"  --- INDEX-ONLY ({len(deep)}) — reachable by deep import, "
                f"listed in the appendix ---"
            )
        if stale:
            problems += len(stale)
            print(f"  --- STALE ({len(stale)}) — anchored, not exported ---")
            for name in stale:
                print(f"    {name:38} {found[name]}")

    rendered = render_index(PACKAGES, anchors)
    if write:
        INDEX.parent.mkdir(parents=True, exist_ok=True)
        INDEX.write_text(rendered)
        print(f"\nwrote {INDEX.relative_to(REPO)}")
        return 0

    if not INDEX.exists():
        print(f"\nDRIFT: {INDEX.relative_to(REPO)} does not exist. Run --write.")
        problems += 1
    elif INDEX.read_text() != rendered:
        print(
            f"\nDRIFT: {INDEX.relative_to(REPO)} is stale. Run --write."
        )
        problems += 1

    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
