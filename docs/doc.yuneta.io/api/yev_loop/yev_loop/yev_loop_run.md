# `yev_loop_run()`

## Description
Runs the event loop continuously.

## Prototype
```c
PUBLIC int yev_loop_run(yev_loop_h yev_loop, int timeout_in_seconds);
```

## Parameters
- `yev_loop` (`yev_loop_h`): The event loop handle.
- `timeout_in_seconds` (`int`): Timeout value in seconds.

## Return Value
- Returns 0 on success, or an error code on failure.
