# `yev_get_yuno()`

## Description
Retrieves the Yuno instance associated with an event loop.

## Prototype
```c
PUBLIC hgobj yev_get_yuno(yev_loop_h yev_loop);
```

## Parameters
- `yev_loop` (`yev_loop_h`): The event loop handle.

## Return Value
- Returns the `hgobj` instance managing the event loop.
