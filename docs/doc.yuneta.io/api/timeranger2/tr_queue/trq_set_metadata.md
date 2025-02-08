# `trq_set_metadata()`

**Description:**
Sets metadata for the queue.

**Prototype:**
```c
PUBLIC int trq_set_metadata(tr_queue trq_, json_t *metadata);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `metadata` (`json_t *`): The metadata to set.

**Returns:**
- `int`: `0` on success, `-1` on failure.

---
