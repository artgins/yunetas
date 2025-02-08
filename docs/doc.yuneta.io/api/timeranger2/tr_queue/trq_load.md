# `trq_load()`

**Description:**
Loads pending messages from the queue based on stored conditions.

**Prototype:**
```c
PUBLIC int trq_load(tr_queue trq_);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.

**Returns:**
- `int`: `0` on success, `-1` on error.
