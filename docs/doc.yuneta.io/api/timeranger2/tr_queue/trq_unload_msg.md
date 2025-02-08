# `trq_unload_msg()`

**Description:**
Removes a message from the queue.

**Prototype:**
```c
PUBLIC int trq_unload_msg(tr_queue trq_, uint64_t rowid);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID of the message to unload.

**Returns:**
- `int`: `0` on success, `-1` on error.

---
