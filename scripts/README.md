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
| `check_doc_line_refs.py`| Audits the `file.c:NNNN` source-line references in the docs against the current tree. Default mode reports which are verifiably linkable (a guard; fails on out-of-bounds lines). `--linkify=<TAG>` turns verifiable refs into GitHub source links pinned to the release tag and pointing at the function's current def; re-run when tagging a release. |

## Adding new scripts

Drop them in this directory and they will appear on `PATH` after the
next `source yunetas-env.sh`. Keep them self-contained — no
intra-script imports — and document them with a one-line `--help` so
new operators can discover what they do.

Operational note: nothing in this directory is part of the runtime.
These are developer / operator tools, not yunos.
