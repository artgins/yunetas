# check_store_schemas.py

Says what the treedb and msg2db schemas persisted in a store will make the SDK
log when a yuno opens them. Read-only, and it needs nothing but `python3`.

**Run it on every store of a node before upgrading its SDK** (7.27.0+). A
topic already on disk that breaks a rule of a NEW topic is opened as it was,
and the treedb says so with an ERROR at **every** open; a few faults leave a
topic out, and one class refuses it with a CRITICAL. Fix what it prints first:
a clean store prints only the count of what it read.

```bash
T=/yuneta/development/yunetas/tools/treedb

python3 $T/check_store_schemas.py                             # every store under /yuneta/store
python3 $T/check_store_schemas.py /yuneta/store/wattyzerdb    # one store, or several dirs
python3 $T/check_store_schemas.py --list-ids-yuno db_history_ce^1620 /yuneta/store/yunovatioscedb   # the LIST estimate with that yuno
# checked 126 schema files: 0 faults, 0 unreadable
```

It ships with the packages under `tools/treedb/`. A node still on an older
package does not have it yet: stream it from a checkout instead.

```bash
ssh <node> python3 - /yuneta/store < tools/treedb/check_store_schemas.py
```

## What it reads

- `<treedb>.treedb_schema.json`, the schema `treedb_open_db()` persists (its
  `topics` a list or a dict);
- `<treedb>-<topic>.treedb_schema.json`, the schema of one topic;
- `<msg2db>.msg2db_schema.json`.

## What it prints

One line per fault: the file, the level of what the SDK does at the open, the
topic (and column), the fault, and the message the SDK logs.

```
/yuneta/store/x/.../treedb_x.treedb_schema.json: ERROR items.id: a pkey2 cannot be the pkey 'id' -- "Topic on disk opened as it was, with bad pkey2s"
/yuneta/store/x/.../treedb_x.treedb_schema.json: CRITICAL depts.users: a column both 'hook' and 'fkey' -- "A column cannot be both 'hook' and 'fkey'"
/yuneta/store/x/.../treedb_x.treedb_schema.json: LEFT-OUT "a/b": a name breaks the name rule -- "Topic of the schema left out: a name breaks the name rule"
```

| Level | What the SDK does at the open |
|---|---|
| `NO-OPEN` | The treedb is not opened (*"No topics found"*), when this file is the schema it opens with. The persisted schema wins unless the yuno is the master and its schema from C has another `schema_version` (a higher one when it is not imposed): then the code's schema replaces the file at that open. The line says the version to beat: *"a treedb schema with no 'topics' (unless a master opens it with a schema from C whose schema_version is not 0, a higher one when not imposed: that one replaces this file)"*. The `schema_version` of the file is read as the SDK reads it (`kw_get_int()` with `KW_WILD_NUMBER`): `3`, `"3"` and `3.0` are all 3, `"0x10"` is 16. |
| `LEFT-OUT` | The topic is not opened: its records stay on disk, unreachable until the schema is fixed. A name that breaks the [name rule](../api/timeranger2/treedb.md#treedb_open_db), a `pkey` that is not `id`, a `system_flag` that is not `sf_string_key`. |
| `CRITICAL` | *"Cannot create topic of the schema"*: a `file` column without `fkey` or not a string, a column both `hook` and `fkey`, no `id` column. A yuno whose `on_critical_error` exits, exits. |
| `ERROR` | The topic opens as it was, and the ERROR is logged at every open: a pkey2 that is `id`, the tkey or named twice, a pkey2 of no column, a `^` in a topic name or a hook id, a second `main_topic`, two `file` columns deriving one `__assets__` hook. |
| `PROJECTION` | The projection of the schema into `__system__` (`C_TREEDB`, on the master) refuses the node: a `<treedb>.<topic>.<column>` longer than a record key, a hook column that is not a dict or a list. |
| `CUT` | The `__assets__` hook of a `file` column is cut to `NAME_MAX` at the open, with no log. |
| `LIST` | The id of a list of an index is cut to `NAME_MAX`. **Estimated**: the id holds two things that no file says. The yuno that opens the store, `<role>^<name>` (after 7.27.10), is taken as 32 bytes, longer than the yunos of our nodes, or as the one given with `--list-ids-yuno db_history_ce^1620` (`--list-ids-yuno ""` for an SDK up to 7.27.10). The name of the gobj that owns the tranger is taken as `tranger_<treedb>` (the name `C_TREEDB` gives it), or as the name given with `--list-ids-gobj`. |

A fault a NEW topic would be refused for is a `CRITICAL` when the topic is not
on disk yet (no `<topic>/topic_desc.json` beside the schema), and an `ERROR`
when it is.

Exit status: `0` nothing found, `1` a fault found, `2` a file could not be read.

## What it does not check

The fields of a column against the column meta-schema (`parse_schema_cols()`:
its types and its flags), which the SDK says as *"a column is not valid (see
the log)"*. That meta-schema is derived from the `__system__` schema at run
time, and a copy in the script would drift from it.
