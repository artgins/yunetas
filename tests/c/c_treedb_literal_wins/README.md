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
  `applied` (an apply that never ran), `saved` or `unsaved`.
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

The expected log list in `src/main.c` is strict FIFO: every line from INFO
up, in order.

## Run

```bash
ctest -R test_c_treedb_literal_wins --output-on-failure --test-dir build
```
