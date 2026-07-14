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
tranger2_open_iterator(...) / tranger2_iterator_get_page(...) / tranger2_close_iterator(...)
tranger2_topic_key_size(...) / tranger2_topic_key_range(...)
```

Every call to `tranger2_append_record()` writes one record to disk,
increments the per-key `g_rowid` (monotonic, never resets) and
`i_rowid` (row in the md2 file). TreeDB relies on this for its
link/unlink semantics — see `CLAUDE.md` for the full rules.

## Two time axes: `t` and `tm`

Every record carries **two** timestamps, and they are independent:

| Axis | Meaning | Source |
|------|---------|--------|
| `t`  | **Persistence** time — when the record was appended | the `__t__` argument of `tranger2_append_record()` (now, if 0) |
| `tm` | **Message** time — when the event it carries happened | the record's **tkey** field (usually `tm`), set by the producer |

They diverge whenever data is backfilled or a device uploads a buffered batch
late. Both are in the **topic's** unit: seconds, or **milliseconds** when the
topic sets `sf_t_ms` / `sf_tm_ms`.

An iterator's `match_cond` takes a range on each axis (`from_t`/`to_t`,
`from_tm`/`to_tm`) plus `from_rowid`/`to_rowid` and the `user_flag` conditions,
and **ANDs** them. Every condition is honored **per record**: a filtered paging
iterator builds its row index when it opens, so `tranger2_iterator_size()`,
`pages` and the pages themselves count only matching records (and `get_page`'s
`from_rowid` is then a position among the matching rows, not a global rowid). An
unfiltered iterator builds no index — its open stays cheap regardless of key
size. The index is a **snapshot** taken at open: records appended after a
filtered iterator opens never enter its count or its pages, while an unfiltered
one recounts the key on every call. `tranger2_topic_key_range()` reports a
key's span on both axes without reading a record.

> **In the md2 record, the times carry flags.** On disk the 16 high bits of
> `__t__` hold the `user_flag` and those of `__tm__` the `system_flag`; only the
> low 44 bits are the time. Always read them through `get_time_t()` /
> `get_time_tm()` — the raw field yields a timestamp with the flags baked in.

> **A topic OWNS the handles opened on it** (iterators, `rt_mem`, `rt_disk`):
> `tranger2_close_topic()` closes them all and frees the topic. Anyone caching a
> handle must check `tranger2_topic_is_open()` first — it is freed memory once
> its topic is gone.

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

### Path-traversal hardening (since 7.6.0)

A string primary key or treedb node `id` becomes a filesystem path component
(`keys/<key>/…`). Since 7.6.0 the key/id is validated before any filesystem use
— a value containing `/` or beginning with `.` (so `..`, absolute paths, and
hidden traversal can't escape the topic directory) is rejected, on the create,
lookup, **and** `tranger2_delete_instance` paths, with the mirror predicate
aligned so followers reject the same inputs. Regression coverage in
`tests/c/timeranger2/test_pkey_path_traversal.c`.

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

The deletion also drops the key from the **watermark** of every `rt_disk` feed
(below): a feed that outlives many keys must not carry a mark for each one, and
a key re-created afterwards must not inherit the dead one's.

### What a realtime feed hands its callback

The rowid a feed delivers is the key's **global `g_rowid`** — the one that
never resets — not the row's position in the `.md2` file it happens to live in
(that one is `i_rowid`, and it restarts at 1 with every new file). A consumer
may therefore dedupe or page by it across a file rotation.

That distinction is also the feed's own bookkeeping: each `rt_disk` feed keeps
a watermark of the last row it was given **per key AND per file**, because the
shared key cache advances with the first wake-up of a batch and a feed served
by a later one would otherwise find "nothing new". A watermark that forgot
which file it counted in became a ceiling over the next one, and the first
record of every new file reached no feed at all (fixed 2026-07-14;
`tests/c/timeranger2/test_rt_disk_multi_feed.c` covers the rotation, the
several-feeds-per-key fan-out and the re-created key).

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
