# scripts

Auxiliary scripts shipped with the Yuneta source tree. This directory
is added to `PATH` by [`yunetas-env.sh`](../yunetas-env.sh), so the
scripts below are runnable by name from any cwd inside a sourced
yuneta shell.

## What's here

| Script                  | Purpose                                                                  |
|-------------------------|--------------------------------------------------------------------------|
| `migratev6tov7.py`      | One-shot migration tool: moves a v6 timeranger2 store to the v7 layout. Used during the v6→v7 upgrade window; not needed for greenfield. |
| `verify_api_coverage.py`| Compares every `PUBLIC` kernel C function against the documented API anchors in `docs/doc.yuneta.io`; reports MISSING / EXTRA symbols per header. |
| `check_doc_line_refs.py`| Audits/links the source references in the docs against the current tree. Default mode reports which `file.c:NNNN` refs are verifiably linkable, and flags GitHub blob links pinned to a moving ref (`main`, a branch) instead of a release tag (a guard; fails on out-of-bounds lines or any unpinned link). `--linkify=<TAG>` turns verifiable colon-form refs into GitHub source links pinned to the release tag and pointing at the function's current def. `--strip-prose` drops the same unverifiable line numbers written in prose (`(lines 316-335)`) or comma form (`gobj.c, 3622`). `--link-symbols=<TAG>` makes the backticked symbols in narrative prose clickable — functions to GitHub source, gclasses/CLI tools to their doc page. `--link-files=<TAG>` makes backticked bare file paths (`kernel/.../c_yuno.c`, `glogger.c`, and shell scripts like `yunetas-env.sh`) clickable — a file-level GitHub link with no line anchor, so it never drifts; abbreviated/relative paths resolve by unique suffix against the canonical (non-test, non-ESP32) tree, wrong-directory paths stay bare to surface the error, and absolute runtime paths (`/yuneta/store/...`) are left bare (they are not repo source). This is the only mode that indexes `.sh`; the audit/symbol modes stay C/H/JS/PY. `--repin=<NEWTAG>` is the release-time pass: it rewrites every `/blob/<anytag>/` to the new tag and recomputes each symbol link's `#L` anchor from the symbol's current definition (keeping `[`fn`](…) at [file:line](…)` row twins in sync; file-level links just get the tag; vanished symbols are reported for a human). The link modes are one-shot and idempotent — they skip anything already linked — so they don't move links to a new tag; `--repin` is what you run when tagging a release. |

## Adding new scripts

Drop them in this directory and they will appear on `PATH` after the
next `source yunetas-env.sh`. Keep them self-contained — no
intra-script imports — and document them with a one-line `--help` so
new operators can discover what they do.

Operational note: nothing in this directory is part of the runtime.
These are developer / operator tools, not yunos.
