# `fs_stop_watcher_event()`

## Description
Stops monitoring the specified file system watcher event.

## Prototype
```c
int fs_stop_watcher_event(
    fs_event_t *fs_event
);
```

## Parameters
- **fs_event** (`fs_event_t *`) - The watcher event to be stopped.

## Return Value
Returns `0` on success, `-1` if the event is `NULL`.

## Notes
- Calls `yev_stop_event()` if the event is running.
- If the event is not running, `fs_destroy_watcher_event()` is called to clean up resources.
