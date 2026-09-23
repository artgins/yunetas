# c_treedb_literal_wins test

Tests what `C_TREEDB` does when a dynamic-schema treedb (`impose_c_schema`
off) opens with a schema from C (the literal) that is newer than the schema
file in use. The rule is the user's decision of 2026-09-23:

- A literal whose `schema_version` is **higher** than the file in use (or
  with no file) **wins whole**. It replaces the whole file, `__system__` is
  projected from it whole, and a topic it does not declare disappears from
  both.
- The operator work that this discards is reported: one WARNING *"Schema
  from C withdrew work on the schema at open"* that names the treedb and the
  topics, and `withdrawn_at_open` in `treedbs` and `saved-schema`, per topic
  `applied` (an apply that never ran), `in_use` (an apply that ran),
  `saved` or `unsaved`.
- A literal that is **not** higher is not installed: the file runs, and
  `__system__` keeps what it holds.

Each scenario opens its own treedb. At the end, it checks that the three
places of the schema agree: what the store RUNS (the open topics, their
`topic_version` and columns), the schema FILE in use, and `__system__`
(`saved-schema` answers no draft that differs from the file).

| Scenario | Treedb | What it checks |
|---|---|---|
| RM | `tw_rm` | The literal removes a parent topic and the fkey to it. The treedb opens, twice. The parent is gone from the file, from what runs and from `__system__`. |
| TIE+HOOK | `tw_tie` | An apply of `users` (topic_version 2) is not opened yet. The literal gives `users` 2 with a new fkey, and adds a parent `groups` that hooks it. The literal runs. The apply is reported as `applied`. |
| EQ | `tw_eq` | The same, with a new plain column: a record that uses it is accepted. A second open with the same literal reports nothing. |
| SAVED | `tw_saved` | A saved draft of `users` and an unsaved draft of `departments` are both withdrawn and reported, with the withdrawn saved schema. |
| OLD | `tw_old` | A literal equal to the file (other content) and one older than it are not installed. The unopened apply runs at that open. |
| REN | `tw_ren` | A topic is renamed, with its hook and fkey. The old topic is gone. |
| NR | `tw_nr` | A topic is changed without a higher `topic_version`. The file and `__system__` show the literal. The store keeps its own columns, and a warning says so. |
| IMP | `tw_imp` | With `impose_c_schema` forced by the code, a newer literal also re-makes `__system__` whole. |
| L-2 | `tw_l2` | A topic whose store directory is gone is not reported as `applied`. An apply is recorded when it is made (`saved_schemas/<treedb>.applied.json`), not guessed from a missing `topic_var.json`. |
| SNAP | `tw_snap` | A snapshot of `__system__` holds the topic the literal removes. The projection is unfinished and says so: warning, `c_schema_version` 0, `unfinished_projection`, `save-schema` refused, retried at every open. Once the snapshot is deleted, the next open completes it. |
| TWICE | `tw_twice` | A second `open-treedb` of a treedb that is open is refused as "already open" before anything is reconciled: `__system__`, the saved schema and the file stay as they were. |
| RAN | `tw_ran` | An apply that ran (an open read it), then a newer literal: the running dynamic `users` is reported as `in_use`; `departments`, changed only by the developer, is not reported. |
| SEED | `tw_seed` | After `delete-treedb`, a projection seeded from a dynamic file has `c_schema_version` 0, and the tie with a literal of the same number is said at every open. |
| LOCK | `tw_lock` | The store of the treedb is locked (as by another process): it opens as a replica and `__system__` is not reconciled. When the lock is free, the literal is installed and projected. |
| REC | `tw_rec` | The record of an apply cannot be written: the apply is refused and the file in use does not change. |
| M1 | `tw_m1` | A projection is unfinished (a snapshot holds `departments`). `saved-schema` does not show the leftover as a draft. The operator adds a column to `users`: it is the only draft. The retry of the projection replaces that column and reports `users` as `unsaved`. When the snapshot is gone, the projection completes and reports nothing. |
| M2 | `tw_m2i`, `tw_m2d` | The leftover of an unfinished projection is not reported as withdrawn work when the projection completes: on the imposed retry, and when a newer literal arrives. |
| L1 | `tw_l1` | A projection seeded from a dynamic file (`c_schema_version` 0) is not unfinished. The file itself, as the literal, projects nothing: the operator's column and header draft stay, nothing is reported. |
| L3 | `tw_l3` | The process died between the record of an apply and its rename (the record is written by hand). The next apply keeps the topics of the apply that ran, from the record's `previous`: a newer literal reports `users` as `in_use` and `departments` as `applied`. |
| L4 | `tw_l4` | A projection seeded from the file fails (a column flag that the meta-schema refuses). It is recorded, retried at every open, its partial projection is no draft, and `save-schema` refuses. When the file is fixed, the next open completes it. |
| L6 | `tw_l6` | An open that fails answers `-1` and names the yuno: a literal that C_TREEDB refuses, and a schema file that `treedb_open_db()` refuses (no topics). The second answer tells to `close-treedb` first. |

An unfinished projection is RECORDED in
`saved_schemas/<treedb>.unfinished.json` under the `__system__` tranger. The
record says which ids of `__system__` the projection left. Only these are
not drafts, on every path.

The expected log list in `src/main.c` is strict FIFO: every line from INFO
up, in order.

## Run

```bash
ctest -R test_c_treedb_literal_wins --output-on-failure --test-dir build
```
