# timeranger2

**Append-only time-series storage** for Yuneta. Records are grouped into **topics**, keyed by a primary id, and indexed for fast access by time range or rowid. Several higher-level stores are built on top of it:

- **msg2db** — dict-style message store (one value per key)
- **msg2db queue** — message queue with at-least-once semantics (used by the MQTT broker)
- **TreeDB** — graph memory database with hook/fkey relationships (see `kernel/c/root-linux/src/c_treedb.c` and `CLAUDE.md`)

## Core API (selected)

```c
tranger2_startup(...) / tranger2_shutdown(...)
tranger2_open_topic(...) / tranger2_close_topic(...)
tranger2_append_record(...)
tranger2_iterator_open(...) / tranger2_iterator_next(...) / _close(...)
```

Every call to `tranger2_append_record()` writes one record to disk,
increments the per-key `g_rowid` (monotonic, never resets) and
`i_rowid` (row in the md2 file). TreeDB relies on this for its
link/unlink semantics — see `CLAUDE.md` for the full rules.

See [`yunos/c/yuno_agent/TREEDB.md`](../../../yunos/c/yuno_agent/TREEDB.md)
for the full timeranger2 + treedb walkthrough (mental model, on-disk
layout, master/non-master locking, snapshots, the cross-yuno
`rt_by_disk` pattern, sharp edges and recipes).

## Two delete granularities (record vs instance)

In timeranger2 the data model is **two-level**:

- A **record** = a primary `key`. Lives in its own directory
  `keys/<key>/` with its own `.md2` index.
- An **instance** = one entry in that key's time series. A row in
  the `.md2` file, addressed by `(key, __t__, rowid)`.

`tranger2_append_record(key, ..., __t__)` adds **one instance** of
the record `key`. The naming is historical; the contract operates on
instances.

| Granularity | API today | What it does |
|---|---|---|
| Whole record (key + all instances) | **`tranger2_delete_key`** (was `tranger2_delete_record` before 2026-05-25; legacy alias kept in `timeranger2.h`) | `rmrdir` of `keys/<key>/` + drop from `topic_cache`. Irrecoverable. |
| One instance | *not implemented in v7* — see TODO.md | Would mark the `.md2` row with `sf_deleted_instance` (bit `0x0400`, currently free) + optionally zero the payload. v6 had this under `sf0_deleted_record`. |

### v6 → v7: the missing per-instance delete

v6 had a per-instance soft delete (`sf0_deleted_record` bit honored
on read). The matching v7 flag — `sf_deleted_record` at bit `0x0400`
— was **removed** in commit `eb2c454a7` (2026-05-11) because nothing
in v7 was using it. The slot is empty in `system_flag2_t` and in
`sf_names[]`.

The plan in `TODO.md` ("tranger2: add `tranger2_delete_instance`")
reintroduces the bit as **`sf_deleted_instance`** and adds
`tranger2_delete_instance(tranger, topic_name, key, __t__, rowid)`
when a real consumer needs it.

If you need "logical delete" today, append a tombstone instance at the
treedb / application layer; don't try to mutate timeranger2 storage.

## Filesystem watcher

`src/fs_watcher.c` implements an inotify-based watcher (`fs_event_t`) used by `root-linux/C_FS` and by the stores themselves to react to on-disk changes.

## CLI companions

- `utils/c/tr2list` — list records in a topic
- `utils/c/tr2keys` — list keys in a topic
- `utils/c/tr2search` — search by filter
- `utils/c/tr2migrate` — migrate from legacy timeranger v1
- `utils/c/list_queue_msgs2` — list a msg2db queue
- `utils/c/msg2db_list` — list a msg2db topic
- `utils/c/treedb_list` — list TreeDB nodes

## Tests

`tests/c/timeranger2`, `tests/c/tr_msg`, `tests/c/tr_queue`, `tests/c/tr_treedb`, `tests/c/tr_treedb_link_events`.
