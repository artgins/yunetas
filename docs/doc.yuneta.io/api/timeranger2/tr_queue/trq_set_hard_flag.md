# `trq_set_hard_flag()`

**Description:**
Sets a hard flag for a message in the queue.

**Prototype:**
```c
PUBLIC int trq_set_hard_flag(tr_queue trq_, uint64_t rowid, uint32_t flag);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `rowid` (`uint64_t`): The row ID of the message.
- `flag` (`uint32_t`): The flag to set.

**Returns:**
- `int`: `0` on success, `-1` on error.

---
