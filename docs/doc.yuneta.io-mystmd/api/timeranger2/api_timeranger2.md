# Timeranger2 API

**Append-only time-series storage** for Yuneta. Records are grouped into
**topics**, keyed by a primary id, and indexed for fast access by time
range or row id.

## Higher-level stores built on top

- **`tr_msg2db`** — dict-style message store (one value per key).
- **`tr_queue`** — message queue with at-least-once semantics (used by
  the MQTT broker).
- **TreeDB** — graph memory database with hook/fkey relationships. See
  `kernel/c/root-linux/src/c_treedb.c` and the `CLAUDE.md` TreeDB section
  for the full rules.

## Core API (selected)

```c
tranger2_startup(...) / tranger2_shutdown(...)
tranger2_open_topic(...) / tranger2_close_topic(...)
tranger2_append_record(...)
tranger2_iterator_open(...) / tranger2_iterator_next(...) / _close(...)
```

## Persistence semantics

Every call to `tranger2_append_record()` writes one record to disk,
increments the per-key `g_rowid` (monotonic, never resets) and `i_rowid`
(row position in the `.md2` index file).

:::{important}
TreeDB relies on these semantics for its `link` / `unlink` behaviour:
**link/unlink saves only the child node** (the one carrying the fkey
field), never the parent. See `CLAUDE.md` for the full rules and
`g_rowid` tracing examples.
:::

## Filesystem watcher

`src/fs_watcher.c` implements an inotify-based watcher (`fs_event_t`)
used by `root-linux/C_FS` and by the stores themselves to react to
on-disk changes. See the **fs_watcher** section in the sidebar.

## CLI companions

| Tool | Purpose |
|---|---|
| `utils/c/tr2list` | List records in a topic |
| `utils/c/tr2keys` | List keys in a topic |
| `utils/c/tr2search` | Search by filter |
| `utils/c/tr2migrate` | Migrate from legacy timeranger v1 |
| `utils/c/list_queue_msgs2` | List a `tr_queue` queue |
| `utils/c/msg2db_list` | List a `tr_msg2db` topic |
| `utils/c/treedb_list` | List TreeDB nodes |

## Tests

`tests/c/timeranger2`, `tests/c/tr_msg`, `tests/c/tr_queue`,
`tests/c/tr_treedb`, `tests/c/tr_treedb_link_events`.

## Source code

- [`timeranger2.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/timeranger2.c)
- [`timeranger2.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/timeranger2.h)

## Function reference

The individual function reference pages are listed in the left-hand
sidebar under **Timeranger2 API**.
