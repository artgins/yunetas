# `yev_loop_create()`

## Description
Creates a new event loop instance.

## Prototype
```c
PUBLIC int yev_loop_create(
    hgobj yuno,
    unsigned entries,
    int keep_alive,
    yev_callback_t callback // if return -1 the loop in yev_loop_run will break;
);
```

## Parameters
- `yuno` (`hgobj`): The Yuno instance managing the loop.
- `entries` (`unsigned`): Number of event entries.
- `keep_alive` (`int`): Whether to keep the event loop alive.
- `callback` (`yev_callback_t`): Callback function triggered by events.

## Return Value
- Returns 0 on success, or an error code on failure.
