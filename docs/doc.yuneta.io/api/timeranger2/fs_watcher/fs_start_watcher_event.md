# `fs_start_watcher_event()`

## Description
Starts monitoring the specified file system watcher event.

## Prototype
```c
int fs_start_watcher_event(
    fs_event_t *fs_event
);
```

## Parameters
- **fs_event** (`fs_event_t *`) - The watcher event to be started.

## Return Value
Returns `0` on success, `-1` if the event is `NULL`.

## Notes
- Internally calls `yev_start_event()` to begin event processing.
