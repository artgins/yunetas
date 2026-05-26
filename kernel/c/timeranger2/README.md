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

| Granularity | API | What it does |
|---|---|---|
| Whole record (key + all instances) | **`tranger2_delete_key`** (was `tranger2_delete_record` before 2026-05-25; legacy alias kept in `timeranger2.h`) | `rmrdir` of `keys/<key>/` + drop from `topic_cache` + propagate to in-process subscribers + mirror to `disks/<rt_id>/<key>/` for `rt_by_disk` followers. Irrecoverable. |
| One instance | **`tranger2_delete_instance`** (2026-05-26) | Mutates the `.md2` row in place with `sf_deleted_instance = 0x0400` (inherited side, followers see the same tombstone). Optional `zero_payload` overwrites the matching bytes in the data `.json` for sensitive-data wipes. Read paths skip dead rows; rowids do NOT renumber. Master-only, idempotent. |

### Subscriber propagation on `tranger2_delete_key`

In-process subscribers (rt_mem, rt_disk in the same yuno, open_iterator)
register interest with:

```c
tranger2_set_rt_key_deleted_callback(handle, cb, user_data);
```

When the master calls `tranger2_delete_key()`:

1. Every `topic/disks/<rt_id>/<key>/` subdirectory is removed before
   the live `keys/<key>/` directory. `rt_by_disk` followers
   recursive-watching `disks/<rt_id>/` pick this up as
   `FS_SUBDIR_DELETED_TYPE` and run the same callback fan-out on
   their side.
2. In-process subscribers whose `key` filter matches receive the
   `key_deleted_callback`. Handles that own an inotify watcher
   (`fs_event_client` set) are skipped here — the inotify branch
   delivers the same event.

Pre-2026-05-26 followers that polled their cache on a timer can drop
the timer. See `tests/c/timeranger2/test_delete_key_propagation.c`
for the regression coverage.

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
