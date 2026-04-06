# fs_watcher

inotify-based filesystem watcher used by timeranger2 and by the `C_FS` gclass to react to on-disk changes.

Source code:

- [`fs_watcher.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/fs_watcher.h)
- [`fs_watcher.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/timeranger2/src/fs_watcher.c)

(fs_create_watcher_event)=
## `fs_create_watcher_event()`

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

The created watcher event must be started using [`fs_start_watcher_event()`](<#fs_start_watcher_event>) to begin monitoring. When no longer needed, it should be stopped using [`fs_stop_watcher_event()`](<#fs_stop_watcher_event>), which will also free the associated resources.

---

(fs_start_watcher_event)=
## `fs_start_watcher_event()`

`fs_start_watcher_event()` starts monitoring the specified file system event, enabling notifications for file and directory changes.

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
## `fs_stop_watcher_event()`

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

Once [`fs_stop_watcher_event()`](<#fs_stop_watcher_event>) is called, the `fs_event_t` instance is destroyed and should not be used again.

---
