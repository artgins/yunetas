# tr_treedb test

Core tests for **TreeDB** — the graph memory database built on `timeranger2`. Exercises schema validation, node CRUD, hook/fkey relationships, compound keys and the `__md_treedb__` metadata fields (`g_rowid`, `i_rowid`, …).

Several checkpoints compare the full treedb state against JSON "foto" files (`foto_final1`, `foto_final2`, `foto_final3`). If you change link/unlink semantics and a foto no longer matches, confirm the new `g_rowid` values match the rules in `CLAUDE.md` before updating the foto.

## Run

```bash
ctest -R test_tr_treedb --output-on-failure --test-dir build
```
