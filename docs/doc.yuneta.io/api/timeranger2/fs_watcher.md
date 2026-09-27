# fs_watcher

inotify-based filesystem watcher used by timeranger2 and by the `C_FS` gclass to react to on-disk changes.

Source code:

- [`fs_watcher.h`](https://github.com/artgins/yunetas/blob/7.25.8/kernel/c/timeranger2/src/fs_watcher.h)
- [`fs_watcher.c`](https://github.com/artgins/yunetas/blob/7.25.8/kernel/c/timeranger2/src/fs_watcher.c)

(fs_create_watcher_event)=
## [`fs_create_watcher_event()`](https://github.com/artgins/yunetas/blob/7.25.8/kernel/c/timeranger2/src/fs_watcher.c#L89)

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
## [`fs_start_watcher_event()`](https://github.com/artgins/yunetas/blob/7.25.8/kernel/c/timeranger2/src/fs_watcher.c#L217)

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
## [`fs_stop_watcher_event()`](https://github.com/artgins/yunetas/blob/7.25.8/kernel/c/timeranger2/src/fs_watcher.c#L230)

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
in place (since 7.25.9):

1. a WARNING, *"inotify IN_Q_OVERFLOW: events lost, rescanning the watched
   tree"*, with the watched `path`;
2. in a recursive watch, a watch is set on every directory the tree has now
   and was not watched -- created while its `IN_CREATE` was dropped, so nothing
   inside it would ever be heard;
3. the owner's callback is called ONCE with **`FS_OVERFLOW_TYPE`**
   (`directory` = the watched path, `filename` = `""`): rebuild your view from
   the filesystem, the missed events will not come.

Every owner handles it -- in its callback, before anything that reads the type
as bits:

```C
PRIVATE int my_fs_callback(fs_event_t *fs_event)
{
    switch(fs_event->fs_type) {
        case FS_OVERFLOW_TYPE:
            // events were lost: list what is under fs_event->directory again
            rescan_my_tree(fs_event->gobj, (const char *)fs_event->directory);
            break;
        case FS_FILE_CREATED_TYPE:
            ...
    }
    return 0;
}
```

What the owners of the tree do:

- **timeranger2, a follower's rt_disk feed** (the heavy one: an event per new
  md2 of every key). The master hard-links each new md2 into
  `disks/<rt_id>/<key>/` and the follower consumes the link when it reads it,
  so a link still there IS a record not handed over yet: every key directory is
  scanned, as a newly created one is. A deleted key leaves no trace in
  `disks/<rt_id>/` (its signal is a directory created and removed), so the
  follower's cache is compared with the topic's `keys/`, and a key gone from
  there is heard as deleted (its `key_deleted` callback fires). Both reads are
  idempotent: nothing is handed over twice. An INFO closes it: *"rt_disk feed
  rescanned after lost inotify events"*, with `keys`, `keys_deleted` and `ms`.
- **timeranger2, the master's watch of `disks/`**: the feeds whose directory
  went are closed, and a feed is opened for every directory without one.
- **`C_FS`** publishes `EV_FS_CHANGED` for the watched root.
- **`utils/c/fs_watcher`** prints *"Events LOST"*.

Up to 7.25.8 an overflow aborted the yuno, to be relaunched and reload clean.
Under a sustained burst the reload met the next overflow: in yunovatios' stress
test of its central, a `db_history_ce` following a `db_tracks_ce` at ~3000
records/s over ~60000 keys aborted five times in two hours, each relaunch
spending 2-3 minutes catching up before falling again.

A follower also hears its OWN consumption: each link it removes is an
`IN_DELETE` in a watched key directory, which it ignores. Half of what fills
its queue is that echo, which is why a single burst can overflow it twice.

`tests/c/timeranger2` (`test_rt_disk_overflow`): with the loop stopped, the
master appends to `max_queued_events` + 4096 new keys, one `IN_CREATE` each,
and deletes a key while the queue is full. Every record reaches the feed exactly
once, the deleted key is heard once, and a key born during the overflow is
watched afterwards. With the code that aborted, the test aborts. It needs about
four open files per key and raises its soft limit to the hard one, as a yuno
does; below that it is skipped.
