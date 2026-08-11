# c_treedb_system_schema test

Tests the `__system__` meta-treedb that `C_TREEDB` keeps next to the treedbs it
manages: the place where a treedb schema is stored **as data** (topics
`treedbs` → `topics` → `cols`), which is what lets a schema be listed and
edited at runtime instead of only compiled into C.

The test drives `C_TREEDB` through its own commands and checks:

1. **Projection** — opening a treedb writes its schema into `__system__`: one
   `treedbs` node with its `schema_version`, one `topics` node per topic (with
   `system_topic` preserved), one `cols` node per column. Every column node
   must carry its name in `value`: `cols` is keyed by rowid because a column
   name is unique only inside its topic, so a column stored without `value` is
   a column nobody can name.
2. **Reconstruction** — the projection alone rebuilds the schema. The treedb is
   closed, its `<treedb_name>.treedb_schema.json` deleted so nothing on disk
   can supply the schema, and re-opened: the topics and columns that come back
   must be the ones that went in.

3. **Reconciliation** — a schema that moved forward (`schema_version` 1 → 2,
   one column added, another re-headered) updates the projection, and the
   columns that were already there **keep their rowid**. Re-creating columns
   instead of updating them would renumber every one of them, because a
   column's `id` is a rowid handed out from the topic size. The new column
   reaches the running treedb too, which is the whole chain: literal →
   `__system__` → schema file → topic.

Steps 2 and 3 are the ones that matter for schema editing: they are the path an
edited schema takes to reach a running treedb.

## Run

```bash
ctest -R test_c_treedb_system_schema --output-on-failure --test-dir build
```
