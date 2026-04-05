#!/usr/bin/env python3
"""
strip_toctrees.py — remove `{toctree}` blocks from MyST Markdown landing pages.

The legacy Sphinx site embedded `{toctree}` directives inside each module
landing page to list its sub-pages. mystmd picks up nav from myst.yml
instead (with `pattern:` globs), so the in-page toctree becomes redundant
and would render as an unwanted code-block in mystmd.

This script walks a directory and removes every

    ```{toctree}
    ...options and file list...
    ```

block it finds. The surrounding prose is preserved untouched.

Only files passed on the command line (or discovered via --dir) are
modified in place. Nothing else is touched.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


TOCTREE_BLOCK = re.compile(
    r"""
    (?:^|\n)            # start of a line (or start of file)
    (`{3,}|:{3,})       # opening backticks or colons (captured for matching close)
    \{toctree\}         # the directive name
    [^\n]*\n            # rest of the opening line
    .*?                 # body (non-greedy)
    \1                  # matching closing fence (same char and length)
    [^\n]*              # trailing chars on the close line
    """,
    re.VERBOSE | re.DOTALL,
)


def strip(text: str) -> tuple[str, int]:
    new_text, n = TOCTREE_BLOCK.subn("", text)
    # Collapse any run of 3+ blank lines that the removal may have left.
    new_text = re.sub(r"\n{3,}", "\n\n", new_text)
    return new_text.rstrip() + "\n", n


def process(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    new_text, n = strip(text)
    if n and new_text != text:
        path.write_text(new_text, encoding="utf-8")
        print(f"  {path}  (stripped {n} toctree block{'s' if n != 1 else ''})")
    return n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", type=Path, help="walk a directory and process every *.md under it")
    ap.add_argument("files", nargs="*", type=Path, help="explicit files to process")
    args = ap.parse_args()

    targets: list[Path] = list(args.files)
    if args.dir:
        targets.extend(sorted(args.dir.rglob("*.md")))

    if not targets:
        ap.error("nothing to do: pass files or --dir")

    total_blocks = 0
    total_files = 0
    for p in targets:
        if not p.is_file():
            continue
        n = process(p)
        if n:
            total_blocks += n
            total_files += 1

    print(f"\nDone: {total_blocks} toctree block(s) stripped across {total_files} file(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
