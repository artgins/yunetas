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

Step 2 is the one that matters for schema editing: it is the same path an
edited schema takes to reach a running treedb.

## Run

```bash
ctest -R test_c_treedb_system_schema --output-on-failure --test-dir build
```
