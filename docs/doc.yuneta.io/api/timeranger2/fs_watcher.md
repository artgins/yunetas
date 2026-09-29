# fs_watcher

inotify-based filesystem watcher used by timeranger2 and by the `C_FS` gclass to react to on-disk changes.

Source code:

- [`fs_watcher.h`](https://github.com/artgins/yunetas/blob/7.25.14/kernel/c/timeranger2/src/fs_watcher.h)
- [`fs_watcher.c`](https://github.com/artgins/yunetas/blob/7.25.14/kernel/c/timeranger2/src/fs_watcher.c)

(fs_create_watcher_event)=
## [`fs_create_watcher_event()`](https://github.com/artgins/yunetas/blob/7.25.14/kernel/c/timeranger2/src/fs_watcher.c#L96)

`fs_create_watcher_event()` initializes a new file system watcher event, monitoring the specified `path` for changes based on the given `fs_flag`. The event is associated with the provided `yev_loop` and invokes the specified `callback` when triggered.

```C
fs_event_t *fs_create_watcher_event(
    yev_loop_h     yev_loop,
    const char     *path,
    fs_flag_t      fs_flag,
    fs_callback_t  callback,
    hgobj          gobj,
    void           *user_data,
    void           *user_data2
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the watcher event will be registered. |
| `path` | `const char *` | The directory or file path to monitor for changes. |
| `fs_flag` | `fs_flag_t` | Flags specifying monitoring options, such as recursive watching or file modification tracking. |
| `callback` | `fs_callback_t` | The function to be called when a file system event occurs. |
| `gobj` | `hgobj` | A generic object handle associated with the watcher event. |
| `user_data` | `void *` | User-defined data passed to the callback function. |
| `user_data2` | `void *` | Additional user-defined data passed to the callback function. |

**Returns**

Returns a pointer to a newly allocated [`fs_event_t`](#fs_event_t) structure representing the watcher event, or `NULL` on failure.

**Notes**

The created watcher event must be started using [`fs_start_watcher_event()`](<#fs_start_watcher_event>) to begin monitoring. When no longer needed, it must be stopped using [`fs_stop_watcher_event()`](<#fs_stop_watcher_event>), which will also free the associated resources.

---

(fs_start_watcher_event)=
## [`fs_start_watcher_event()`](https://github.com/artgins/yunetas/blob/7.25.14/kernel/c/timeranger2/src/fs_watcher.c#L224)

`fs_start_watcher_event()` starts monitoring the specified file system event. This enables notifications for file and directory changes.

```C
int fs_start_watcher_event(
    fs_event_t *fs_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fs_event` | `fs_event_t *` | Pointer to the file system event structure to be started. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

Once started, the event will trigger the associated callback when file system changes occur. Use [`fs_stop_watcher_event()`](<#fs_stop_watcher_event>) to stop monitoring and release resources.

---

(fs_stop_watcher_event)=
## [`fs_stop_watcher_event()`](https://github.com/artgins/yunetas/blob/7.25.14/kernel/c/timeranger2/src/fs_watcher.c#L237)

`fs_stop_watcher_event()` stops the given file system watcher event and destroys the associated `fs_event_t` instance.

```C
int fs_stop_watcher_event(
    fs_event_t *fs_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fs_event` | `fs_event_t *` | Pointer to the `fs_event_t` instance representing the file system watcher event to be stopped and destroyed. |

**Returns**

Returns `0` on success, or a negative error code on failure.

**Notes**

Once [`fs_stop_watcher_event()`](<#fs_stop_watcher_event>) is called, the `fs_event_t` instance is destroyed and must not be used again.

---

## Queue overflow (`IN_Q_OVERFLOW`)

Each watcher owns one inotify instance with a bounded kernel event queue
(`fs.inotify.max_queued_events`; the deb/rpm packagers set 65536). Under a
burst the kernel drops the events that do not fit and signals a single
`IN_Q_OVERFLOW`: from there the watcher cannot know every change it missed.

What the lost events said is still on the filesystem, so the watcher recovers
in place (since 7.25.9; in slices since 7.25.10):

1. a WARNING, *"inotify IN_Q_OVERFLOW: events lost, rescanning the watched
   tree"*, with the watched `path`;
2. the owner's callback is called ONCE with **`FS_OVERFLOW_TYPE`**
   (`directory` = the watched path): do there what is global and cheap;
3. a **pass** over the tree follows: every directory, the root included, is
   handed to the owner as **`FS_RESCAN_DIR_TYPE`** (`directory` = that
   directory) -- read it again, what it holds may never have been told. In a
   recursive watch a directory born while its `IN_CREATE` was dropped is
   watched before it is handed over;
4. the pass runs **a slice of 20 ms per loop turn**, and an INFO closes it:
   *"watched tree rescanned after lost inotify events"*, with `directories`,
   `ms`, and where that time went: `slices`, `ms_owner` (in the owner's
   callbacks), `ms_watcher` (the walk itself), `ms_loop` (the loop's own work
   between slices) and `max_loop_ms` (its longest turn). Another overflow during a pass schedules one more whole pass after it
   (starting again would starve the end of the tree under overflows that keep
   coming).

In 7.25.9 the owner rescanned the whole tree inside the one `FS_OVERFLOW_TYPE`
call. A timeranger2 follower of 50000 keys on the busy disk of a central took
56 s, then 243 s -- the yuno deaf to its agent, its commands and its timers all
that time. The slices keep it answering; the pass costs the same.

What the pass costs is mostly the owner's: the watcher's own part is a
`readdir()` per directory and a lookup in an index of the watched paths, built
once per pass. 7.25.10 rebuilt that index in every slice -- 50000 paths every
20 ms -- so its own cost grew with the tree (254 us per directory at 69632,
73 us since 7.25.11), and on the central a pass over 50501 directories took
7 minutes. `tests/c/timeranger2/test_fs_watcher_overflow` measures it (the
pass less the owner's time, per directory) and fails above 200 us.

A pass closes with its own account (since 7.25.12), so a long one says why:

```
"msg": "watched tree rescanned after lost inotify events",
"directories": 69633, "ms": 14374, "slices": 612,
"ms_owner": 11169, "ms_watcher": 887, "ms_loop": 2318, "max_loop_ms": 17
```

Here the owner took most of it (a test's owner that sleeps 100 us per
directory); a large `ms_loop` would say the slices were waiting for a busy
loop instead.

What it said on yunovatios' central, a timeranger2 follower of 50501 keys
while its master wrote a backlog of ~9 GB (2026-09-27): passes of 142-335 s,
**65-78 % in the owner**, 22-35 % in the loop, and **1.1-1.7 s in the
watcher** -- whatever the length of the pass. The owner's time is the records
it hands over: the same tree with nothing pending took 1 s, 0.35 s of it in
the owner. So a long pass is a follower working through a backlog at its own
pace (there, ~80 % of a core), not a slow walk; it ends about a minute after
the master stops writing, and the loop never waited more than 239 ms for a
slice.

Every owner handles both -- in its callback, before anything that reads the
type as bits:

```C
PRIVATE int my_fs_callback(fs_event_t *fs_event)
{
    switch(fs_event->fs_type) {
        case FS_OVERFLOW_TYPE:
            // events were lost: what is global and cheap (a pass follows)
            break;
        case FS_RESCAN_DIR_TYPE:
            // one directory of the tree: list it again
            rescan_my_dir(fs_event->gobj, (const char *)fs_event->directory);
            break;
        case FS_FILE_CREATED_TYPE:
            ...
    }
    return 0;
}
```

What the owners of the tree do:

- **timeranger2, a follower's rt_disk feed** (the heavy one: an event per new
  md2 of every key). At `FS_OVERFLOW_TYPE`: a deleted key leaves no trace in
  `disks/<rt_id>/` (its signal is a directory created and removed), so the
  follower's cache is compared with the topic's `keys/`, read once, and a key
  gone from there is heard as deleted (its `key_deleted` callback fires; INFO
  *"keys deleted while the inotify events were lost"*). At each
  `FS_RESCAN_DIR_TYPE`: the master hard-links each new md2 into
  `disks/<rt_id>/<key>/` and the follower consumes the link when it reads it,
  so a link still there IS a record not handed over yet, and the key directory
  is read. Both are idempotent: nothing is handed over twice.
- **timeranger2, the master's watch of `disks/`**: at `FS_OVERFLOW_TYPE`, the
  feeds whose directory went are closed, and a feed is opened for every
  directory without one (a handful of directories: no pass needed).
- **`C_FS`** publishes `EV_FS_CHANGED` for the watched root, once.
- **`utils/c/fs_watcher`** prints *"Events LOST"* and each *"Rescan dir"*.

Up to 7.25.8 an overflow aborted the yuno, to be relaunched and reload clean.
Under a sustained burst the reload met the next overflow: in yunovatios' stress
test of its central, a `db_history_ce` following a `db_tracks_ce` at ~3000
records/s over ~60000 keys aborted five times in two hours, each relaunch
spending 2-3 minutes catching up before falling again.

A follower also hears its OWN consumption: each link it removes is an
`IN_DELETE` in a watched key directory, which it ignores. Half of what fills
its queue is that echo, which is why a single burst can overflow it twice.

`tests/c/timeranger2`:

- `test_rt_disk_overflow`: with the loop stopped, the master appends to
  `max_queued_events` + 4096 new keys, one `IN_CREATE` each, and deletes a key
  while the queue is full. Every record reaches the feed exactly once, the
  deleted key is heard once, and a key born during the overflow is watched
  afterwards. With the code that aborted, the test aborts. It needs about four
  open files per key and raises its soft limit to the hard one, as a yuno does;
  below that it is skipped.
- `test_fs_watcher_overflow`: the watcher alone, with an owner slow on purpose
  (100 us per directory) and a periodic timer probing the loop. `max_queued_events`
  + 4096 directories are created with the loop stopped: every one is told to the
  owner, the pass takes ~30 s and the loop is never deaf for more than 1 s (it is
  50 ms, the probe's period). On a local disk the directories just created are
  in the kernel's cache and a pass in one piece takes milliseconds -- which is
  why only a slow owner shows what a busy disk does.
