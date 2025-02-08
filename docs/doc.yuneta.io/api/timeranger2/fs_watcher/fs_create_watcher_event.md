# `fs_create_watcher_event()`

## Description
Creates a file system watcher event to monitor a directory.

## Prototype
```c
fs_event_t *fs_create_watcher_event(
    yev_loop_h yev_loop,
    const char *path,
    fs_flag_t fs_flag,
    fs_callback_t callback,
    hgobj gobj,
    void *user_data,
    void *user_data2
);
```

## Parameters
- **yev_loop** (`yev_loop_h`) - The event loop handler.
- **path** (`const char *`) - Path to the directory being watched.
- **fs_flag** (`fs_flag_t`) - Flags defining the type of event monitoring.
- **callback** (`fs_callback_t`) - Function to call when an event occurs.
- **gobj** (`hgobj`) - The GObj associated with the event.
- **user_data** (`void *`) - Custom user data.
- **user_data2** (`void *`) - Additional custom user data.

## Return Value
Returns a pointer to the newly created `fs_event_t` on success, or `NULL` on failure.

## Notes
- Uses `inotify` to set up the watcher.
- If the provided path is not a directory, an error is logged.
- Recursively tracks paths if `FS_FLAG_RECURSIVE_PATHS` is set.
