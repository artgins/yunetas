#!/usr/bin/env python3
"""
myst_to_quarto.py — pilot migration helper from MyST Markdown (Sphinx) to Quarto.

Scope: this is NOT a full MyST→Quarto converter. It handles the specific
patterns used by the yunetas API reference pages:

  - `(label())=` MyST anchors           →  Quarto header attribute `{#sec-label}`
  - `{tab-set}` / `{tab-item}` blocks   →  `::: {.panel-tabset}` + `## TabName`
  - `{list-table}` blocks               →  plain Markdown pipe tables
  - `{dropdown} Title` blocks           →  `::: {.callout-note collapse=true ...}`
  - `:::{toctree}` blocks               →  stripped (Quarto uses _quarto.yml sidebar)
  - MyST admonitions `:::{note}` etc    →  `::: {.callout-note}`
  - Custom roles `:danger:` / `:warning:` → span with `.danger` / `.warning` class
  - Cross-refs `[text](#label)`         →  `[text](#sec-label)`

Usage:
    python myst_to_quarto.py <input.md> <output.qmd>
    python myst_to_quarto.py --dir <src_dir> <dst_dir>

The script is deliberately line-oriented and regex-driven: easy to audit,
easy to extend. Complex edge cases are left for manual review.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Regex helpers
# ---------------------------------------------------------------------------

RE_ANCHOR       = re.compile(r"^\((?P<label>.+)\)=\s*$")
RE_HEADER1      = re.compile(r"^#\s+`?(?P<title>[^`]+?)`?\s*$")
RE_FENCE_OPEN   = re.compile(r"^(?P<ticks>`{3,})(?P<info>.*)$")
RE_COLON_OPEN   = re.compile(r"^(?P<colons>:{3,})\s*\{?(?P<directive>[^}]*)\}?\s*$")
RE_TAB_SET      = re.compile(r"^`{3,}\{tab-set\}\s*$")
RE_TAB_ITEM     = re.compile(r"^`{3,}\{tab-item\}\s+(?P<name>.+?)\s*$")
RE_DROPDOWN     = re.compile(r"^`{3,}\{dropdown\}\s+(?P<title>.+?)\s*$")
RE_LIST_TABLE   = re.compile(r"^:{3,}\s*\{list-table\}\s*$")
RE_TOCTREE      = re.compile(r"^:{3,}\s*\{toctree\}\s*$")
RE_ROLE         = re.compile(r":(?P<role>danger|warning):`(?P<text>[^`]+)`")
RE_CROSS_REF    = re.compile(r"\]\(#(?P<label>[A-Za-z_][A-Za-z0-9_]*)\)")

MYST_ADMONITIONS = {
    "note", "tip", "important", "warning", "caution",
    "attention", "danger", "error", "hint", "seealso",
}


# ---------------------------------------------------------------------------
# Core transformer (line-oriented state machine)
# ---------------------------------------------------------------------------

class Converter:
    def __init__(self, text: str):
        self.lines = text.splitlines()
        self.pos = 0
        self.out: list[str] = []

    # --- utility ------------------------------------------------------------

    def peek(self, offset: int = 0) -> str | None:
        i = self.pos + offset
        return self.lines[i] if 0 <= i < len(self.lines) else None

    def eat(self) -> str:
        line = self.lines[self.pos]
        self.pos += 1
        return line

    def _is_comment_line(self, line: str) -> bool:
        s = line.strip()
        return s.startswith("<!--") and s.endswith("-->")

    def emit(self, line: str = "") -> None:
        self.out.append(line)

    # --- block handlers -----------------------------------------------------

    def handle_tab_set(self, fence_len: int) -> None:
        """Convert a ```{tab-set} ... ``` block to a .panel-tabset div."""
        self.emit("::: {.panel-tabset}")
        closer = "`" * fence_len
        while self.pos < len(self.lines):
            line = self.peek()
            if line is None:
                break
            if line.rstrip() == closer:
                self.eat()
                break
            m = RE_TAB_ITEM.match(line)
            if m:
                # Find matching closing fence for this tab-item
                item_ticks = re.match(r"^(`{3,})", line).group(1)
                self.eat()
                self.emit(f"## {m.group('name').strip()}")
                while self.pos < len(self.lines):
                    inner = self.peek()
                    if inner is None:
                        break
                    if inner.rstrip() == item_ticks:
                        self.eat()
                        break
                    if self._is_comment_line(inner):
                        self.eat()
                        continue
                    # Recurse for nested directives (nested tab-set inside
                    # a dropdown, etc.)
                    if self._try_nested_directive(inner):
                        continue
                    self.emit(self._inline(inner))
                    self.pos += 1
                continue
            # Skip HTML comments / blank lines between items
            self.eat()
        self.emit(":::")

    def handle_dropdown(self, fence_len: int, title: str) -> None:
        self.emit(f'::: {{.callout-note collapse="true" title="{title}"}}')
        closer = "`" * fence_len
        while self.pos < len(self.lines):
            line = self.peek()
            if line is None:
                break
            if line.rstrip() == closer:
                self.eat()
                break
            if self._is_comment_line(line):
                self.eat()
                continue
            if self._try_nested_directive(line):
                continue
            self.emit(self._inline(line))
            self.pos += 1
        self.emit(":::")

    def handle_list_table(self, colon_len: int) -> None:
        """Convert a ::: {list-table} :widths:... block to a pipe table."""
        closer = ":" * colon_len
        # Parse options until first blank line
        while self.pos < len(self.lines):
            line = self.peek()
            if line is None or line.strip() == "":
                break
            if line.lstrip().startswith(":"):
                self.eat()
                continue
            break
        # Collect cells: rows start with "* - "
        rows: list[list[str]] = []
        current: list[str] = []
        while self.pos < len(self.lines):
            line = self.peek()
            if line is None:
                break
            if line.rstrip() == closer:
                self.eat()
                break
            stripped = line.lstrip()
            if stripped.startswith("* - "):
                if current:
                    rows.append(current)
                current = [stripped[4:].rstrip()]
            elif stripped.startswith("- "):
                current.append(stripped[2:].rstrip())
            elif stripped == "":
                pass
            else:
                # Continuation of previous cell
                if current:
                    current[-1] += " " + stripped.rstrip()
            self.eat()
        if current:
            rows.append(current)
        if not rows:
            return
        # Render as pipe table (first row is header)
        header, *body = rows
        ncols = len(header)
        self.emit("| " + " | ".join(header) + " |")
        self.emit("|" + "|".join(["---"] * ncols) + "|")
        for r in body:
            # Pad/truncate to header width
            r = r + [""] * (ncols - len(r))
            self.emit("| " + " | ".join(c.replace("|", "\\|") for c in r[:ncols]) + " |")

    def handle_toctree(self, colon_len: int) -> None:
        """Drop toctree blocks entirely — Quarto uses _quarto.yml sidebar."""
        closer = ":" * colon_len
        while self.pos < len(self.lines):
            line = self.peek()
            if line is None:
                break
            self.eat()
            if line.rstrip() == closer:
                break

    def handle_admonition(self, colon_len: int, kind: str, rest: str) -> None:
        closer = ":" * colon_len
        title = rest.strip()
        header = f"::: {{.callout-{kind}"
        if title:
            header += f' title="{title}"'
        header += "}"
        self.emit(header)
        while self.pos < len(self.lines):
            line = self.peek()
            if line is None:
                break
            if line.rstrip() == closer:
                self.eat()
                break
            self.emit(self._inline(line))
            self.pos += 1
        self.emit(":::")

    # --- dispatcher ---------------------------------------------------------

    def _try_nested_directive(self, line: str) -> bool:
        """Attempt to consume a nested directive at the current position.
        Returns True if one was consumed."""
        if RE_TAB_SET.match(line):
            ticks = re.match(r"^(`{3,})", line).group(1)
            self.eat()
            self.handle_tab_set(len(ticks))
            return True
        m = RE_DROPDOWN.match(line)
        if m:
            ticks = re.match(r"^(`{3,})", line).group(1)
            self.eat()
            self.handle_dropdown(len(ticks), m.group("title").strip())
            return True
        if RE_LIST_TABLE.match(line):
            colons = re.match(r"^(:{3,})", line).group(1)
            self.eat()
            self.handle_list_table(len(colons))
            return True
        if RE_TOCTREE.match(line):
            colons = re.match(r"^(:{3,})", line).group(1)
            self.eat()
            self.handle_toctree(len(colons))
            return True
        # Generic admonition: :::{note} foo
        m = re.match(r"^(:{3,})\s*\{(?P<kind>[a-z]+)\}(?P<rest>.*)$", line)
        if m and m.group("kind") in MYST_ADMONITIONS:
            self.eat()
            self.handle_admonition(
                len(m.group(1)), m.group("kind"), m.group("rest")
            )
            return True
        return False

    # --- inline transforms --------------------------------------------------

    def _inline(self, line: str) -> str:
        # Custom roles :danger:`foo` / :warning:`foo`
        line = RE_ROLE.sub(
            lambda m: f'[{m.group("text")}]{{.{m.group("role")}}}', line
        )
        # Cross-refs [text](#label) → [text](#sec-label)
        line = RE_CROSS_REF.sub(lambda m: f"](#sec-{m.group('label')})", line)
        return line

    # --- main loop ----------------------------------------------------------

    def run(self) -> str:
        while self.pos < len(self.lines):
            line = self.peek()
            if line is None:
                break

            # Strip HTML comment lines used as visual separators
            if line.strip().startswith("<!--") and line.strip().endswith("-->"):
                self.eat()
                continue

            # MyST anchor `(label)=` immediately before an H1 header
            m = RE_ANCHOR.match(line)
            if m:
                label = re.sub(r"\(\)$", "", m.group("label"))
                self.eat()
                nxt = self.peek()
                if nxt and RE_HEADER1.match(nxt):
                    self.eat()
                    title = RE_HEADER1.match(nxt).group("title").strip()
                    self.emit(f"# `{title}` {{#sec-{label}}}")
                    continue
                # Otherwise emit a bare div with the id
                self.emit(f"[]{{#sec-{label}}}")
                continue

            if self._try_nested_directive(line):
                continue

            self.emit(self._inline(line))
            self.pos += 1

        # Collapse 3+ consecutive blank lines into 2
        collapsed: list[str] = []
        blank_run = 0
        for ln in self.out:
            if ln.strip() == "":
                blank_run += 1
                if blank_run <= 2:
                    collapsed.append(ln)
            else:
                blank_run = 0
                collapsed.append(ln)
        return "\n".join(collapsed).rstrip() + "\n"


# ---------------------------------------------------------------------------
# Entry points
# ---------------------------------------------------------------------------

def convert_file(src: Path, dst: Path) -> None:
    text = src.read_text(encoding="utf-8")
    result = Converter(text).run()
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(result, encoding="utf-8")
    print(f"  {src}  →  {dst}")


def convert_dir(src_dir: Path, dst_dir: Path) -> None:
    for md in sorted(src_dir.rglob("*.md")):
        rel = md.relative_to(src_dir)
        qmd = dst_dir / rel.with_suffix(".qmd")
        convert_file(md, qmd)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", action="store_true", help="treat src and dst as directories")
    ap.add_argument("src", type=Path)
    ap.add_argument("dst", type=Path)
    args = ap.parse_args()
    if args.dir:
        convert_dir(args.src, args.dst)
    else:
        convert_file(args.src, args.dst)
    return 0


if __name__ == "__main__":
    sys.exit(main())
