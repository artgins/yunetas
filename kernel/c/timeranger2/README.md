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

Every call to `tranger2_append_record()` writes one record to disk, increments the per-key `g_rowid` (monotonic, never resets) and `i_rowid` (row in the md2 file). TreeDB relies on this for its link/unlink semantics — see `CLAUDE.md` for the full rules.

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
